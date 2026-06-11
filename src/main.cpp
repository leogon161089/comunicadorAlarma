#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <dscKeybusInterface.h>
#include "sia_map.h"

static const char* AP_SSID = "ESP32-C3-Config";
static const char* AP_PASS = "12345678";
static const char* PREF_NAMESPACE = "sia_dc09";

WebServer server(80);
Preferences prefs;
// PIN del LED integrado (ESP32-C3 Super Mini usa GPIO8)
const uint8_t LED_PIN = 8;

// Pines para dscKeybusInterface (evitar pines USB D+/D- en ESP32-C3)
// Nota: GPIO19/GPIO20 suelen usarse para USB en algunas placas; no los usemos.
const uint8_t DSC_CLOCK_PIN = 4;
const uint8_t DSC_READ_PIN = 5;
const uint8_t DSC_WRITE_PIN = 6;

dscKeybusInterface dsc(DSC_CLOCK_PIN, DSC_READ_PIN, DSC_WRITE_PIN);

struct SiaConfig {
  String wifiSsid;
  String wifiPassword;
  String primaryHost;
  uint16_t primaryPort = 0;
  String secondaryHost;
  uint16_t secondaryPort = 0;
  bool secondaryRedundant = false;
  String account = "2510";
  uint16_t seq = 1;
  uint32_t keepaliveInterval = 300; // segundos entre keepalive, 0 = deshabilitado
};

SiaConfig config;

bool hasWifiCredentials = false;
bool wifiConnectedPreviously = false;
bool keybusConnectedPreviously = false;
unsigned long lastReconnectAttempt = 0;
bool sendQueued = false;
bool sendingNow = false;
unsigned long lastKeepaliveSent = 0;

// Comprueba si un evento SIA está habilitado en preferencias (usa default del mapa)
bool isSiaEventEnabled(const char* key) {
  // buscar default en la tabla
  bool def = true;
  for (int i = 0; i < siaMapCount; ++i) {
    if (strcmp(siaMap[i].eventKey, key) == 0) {
      def = siaMap[i].defaultEnabled;
      break;
    }
  }
  // usar clave corta en Preferences para evitar límite de nombre (ev{index})
  for (int i = 0; i < siaMapCount; ++i) {
    if (strcmp(siaMap[i].eventKey, key) == 0) {
      String shortKey = String("ev") + String(i);
      return prefs.getBool(shortKey.c_str(), def);
    }
  }
  return def;
}
String pendingType;
String pendingAddress;
String pendingPartition;
String pendingDescription;
String pendingTarget;
String lastSendStatus = "Inactivo";
unsigned long lastStatusPrint = 0;
String lastSiaMessage = "";

String defaultSiaTarget() {
  return config.secondaryRedundant ? String("all") : String("primary");
}

bool queueSiaEvent(const String& eventType, const String& partition, const String& address, const String& description) {
  pendingType = eventType;
  pendingPartition = partition.length() > 0 ? partition : String("1");
  pendingAddress = address;
  pendingDescription = description;
  pendingTarget = defaultSiaTarget();
  sendQueued = true;
  return true;
}

bool queueSiaEventByKey(const char* eventKey, const String& description) {
  for (int i = 0; i < siaMapCount; ++i) {
    if (strcmp(siaMap[i].eventKey, eventKey) == 0) {
      return queueSiaEvent(String(siaMap[i].siaCode), "1", "", description);
    }
  }
  return false;
}

String normalizeAccount(const String& input) {
  String s = input;
  s.trim();
  if (s.length() == 0) return String("2510");
  // aceptar hexa de 1-4 dígitos (0-FFFF) sin conversión
  s.toUpperCase();
  if (s.length() > 4) return String("2510");
  for (size_t i = 0; i < s.length(); ++i) {
    char c = s.charAt(i);
    if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F'))) {
      return String("2510");
    }
  }
  // rellenar a 4 dígitos con ceros adelante
  while (s.length() < 4) {
    s = String('0') + s;
  }
  return s;
}

String formatDigits(int value, int width) {
  char buffer[16];
  snprintf(buffer, sizeof(buffer), "%0*d", width, value);
  return String(buffer);
}

String formatHex(uint16_t value, int width) {
  char buffer[16];
  snprintf(buffer, sizeof(buffer), "%0*X", width, value);
  return String(buffer);
}

uint8_t reflect8(uint8_t data) {
  uint8_t result = 0;
  for (uint8_t i = 0; i < 8; i++) {
    if (data & (1 << i)) {
      result |= 1 << (7 - i);
    }
  }
  return result;
}

uint16_t reflect16(uint16_t data) {
  uint16_t result = 0;
  for (uint8_t i = 0; i < 16; i++) {
    if (data & (1 << i)) {
      result |= 1 << (15 - i);
    }
  }
  return result;
}

uint16_t calculateCrc16(const String& payload) {
  // Variante de CRC que coincide con el ejemplo (entrada/salida reflejadas, polinomio 0x8005)
  uint16_t crc = 0x0000;
  for (size_t i = 0; i < payload.length(); ++i) {
    uint8_t b = reflect8((uint8_t)payload[i]);
    crc ^= ((uint16_t)b << 8);
    for (uint8_t bit = 0; bit < 8; ++bit) {
      if (crc & 0x8000) {
        crc = (crc << 1) ^ 0x8005;
      } else {
        crc <<= 1;
      }
    }
  }
  crc = reflect16(crc);
  return crc;
}

String makeAccountHex(const String& account) {
  String result = account;
  result.toUpperCase();
  return result;
}

String buildSiaMessage(const String& eventType, const String& address, const String& partition, const String& description) {
  String acct = makeAccountHex(config.account);
  String seq = formatDigits(config.seq, 4);
  String prefix = "L0";

  String dataBlock = "[#" + acct + "|Nri" + partition;
  String typePart = eventType;
  if (typePart.length() >= 2) {
    typePart.toUpperCase();
  }
  if (address.length() > 0) {
    String addr = address;
    addr.toUpperCase();
    while (addr.length() < 3) {
      addr = String('0') + addr;
    }
    dataBlock += "/" + typePart + addr;
  } else {
    dataBlock += typePart;
  }
  dataBlock += "]";

  String mac = WiFi.macAddress();
  mac.replace(":", "");
  String xdata = "[M" + mac + "]";
  String info = description;
  if (info.length() == 0) {
    info = "Evento manual desde web";
  }
  xdata += "[I " + info + "]";

  String payload = "\"SIA-DCS\"" + seq + prefix + "#" + acct + dataBlock + xdata;
  uint16_t crcValue = calculateCrc16(payload);
  String crc = formatHex(crcValue, 4);
  String lengthField = formatDigits(payload.length(), 4);

  String fullMessage = "\n" + crc + lengthField + payload + "\r";
  // store last built message for debugging/consulta
  lastSiaMessage = fullMessage;
  return fullMessage;
}

bool sendToHost(const String& host, uint16_t port, const String& message, String& response) {
  WiFiClient client;
  client.setTimeout(5000);
  Serial.printf("Conectando a %s:%u\n", host.c_str(), port);
  if (!client.connect(host.c_str(), port)) {
    Serial.printf("Fallo al conectar a %s:%u\n", host.c_str(), port);
    response = "conexion fallida";
    return false;
  }
  Serial.println("Conectado, enviando mensaje...");
  client.print(message);
  client.flush();
  Serial.println("Mensaje enviado. Esperando respuesta...");
  unsigned long start = millis();
  response = "";
  while (millis() - start < 3000) {
    while (client.available()) {
      char c = (char)client.read();
      response += c;
    }
    if (response.indexOf("ACK") >= 0 || response.indexOf("NAK") >= 0) break;
    delay(10);
  }
  if (response.length() == 0) {
    response = "sin respuesta";
  }
  Serial.printf("Respuesta recibida: %s\n", response.c_str());
  client.stop();
  return response.indexOf("ACK") >= 0;
}

bool sendSiaEventNow(const String& target) {
  String message = buildSiaMessage(pendingType, pendingAddress, pendingPartition, pendingDescription);
  Serial.println("Mensaje SIA construido:");
  Serial.println(message);
  bool primaryOk = false;
  bool secondaryOk = false;
  String response;

  if ((target == "primary" || target == "all") && config.primaryHost.length() > 0 && config.primaryPort > 0) {
    primaryOk = sendToHost(config.primaryHost, config.primaryPort, message, response);
    Serial.printf("Resultado servidor primario: %s - %s\n", primaryOk ? "OK" : "FAIL", response.c_str());
  }

  if ((target == "secondary" || target == "all" || (!config.secondaryRedundant && !primaryOk)) && config.secondaryHost.length() > 0 && config.secondaryPort > 0) {
    secondaryOk = sendToHost(config.secondaryHost, config.secondaryPort, message, response);
    Serial.printf("Resultado servidor secundario: %s - %s\n", secondaryOk ? "OK" : "FAIL", response.c_str());
  }

  if (config.secondaryRedundant) {
    return (primaryOk || secondaryOk);
  }
  return primaryOk || secondaryOk;
}

void saveConfig() {
  prefs.putString("ssid", config.wifiSsid);
  prefs.putString("password", config.wifiPassword);
  prefs.putString("primaryHost", config.primaryHost);
  prefs.putUInt("primaryPort", config.primaryPort);
  prefs.putString("secondaryHost", config.secondaryHost);
  prefs.putUInt("secondaryPort", config.secondaryPort);
  prefs.putBool("secondaryRedundant", config.secondaryRedundant);
  prefs.putString("account", config.account);
  prefs.putUInt("seq", config.seq);
  // guardar keepalive configurado (segundos)
  prefs.putUInt("keepalive", config.keepaliveInterval);
}

void loadConfig() {
  config.wifiSsid = prefs.getString("ssid", "");
  config.wifiPassword = prefs.getString("password", "");
  config.primaryHost = prefs.getString("primaryHost", "");
  config.primaryPort = prefs.getUInt("primaryPort", 0);
  config.secondaryHost = prefs.getString("secondaryHost", "");
  config.secondaryPort = prefs.getUInt("secondaryPort", 0);
  config.secondaryRedundant = prefs.getBool("secondaryRedundant", false);
  config.account = prefs.getString("account", "2510");
  config.seq = prefs.getUInt("seq", 1);
  config.keepaliveInterval = prefs.getUInt("keepalive", 300);
  if (config.account.length() == 0) {
    config.account = "2510";
  }
  hasWifiCredentials = config.wifiSsid.length() > 0 && config.wifiPassword.length() > 0;
}

void startAccessPoint() {
  Serial.println("\n--- Iniciando Punto de Acceso ---");
  WiFi.mode(WIFI_AP_STA);
  delay(100);
  // iniciar AP en modo simple (canal automático, máxima potencia)
  Serial.printf("Llamando a softAP('%s', '%s')\n", AP_SSID, AP_PASS);
  bool apOk = WiFi.softAP(AP_SSID, AP_PASS);
  delay(500);
  if (apOk) {
    Serial.printf("✓ AP iniciado correctamente\n");
    Serial.printf("  SSID: %s\n", WiFi.softAPSSID().c_str());
    Serial.printf("  IP: %s\n", WiFi.softAPIP().toString().c_str());
    Serial.printf("  MAC: %s\n", WiFi.softAPmacAddress().c_str());
    Serial.printf("  Clientes: %d\n", WiFi.softAPgetStationNum());
    // intentar máxima potencia
    WiFi.setTxPower(WIFI_POWER_15dBm);
    Serial.println("  Potencia TX: máxima (15dBm)");
  } else {
    Serial.println("✗ FALLO al iniciar AP - verificar hardware WiFi");
  }
  Serial.println("--- Fin Punto de Acceso ---\n");
}

void startStation() {
  if (!hasWifiCredentials) {
    Serial.println("No hay credenciales STA guardadas.");
    return;
  }
  Serial.printf("Conectando STA a '%s'...\n", config.wifiSsid.c_str());
  WiFi.begin(config.wifiSsid.c_str(), config.wifiPassword.c_str());
}

String htmlPage() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html lang="es">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>ESP32-C3 SIA DC09 Config</title>
  <style>
    body { font-family: Arial, sans-serif; margin: 16px; background:#f4f6f8; color:#1a1a1a; }
    h1,h2 { color:#222; }
    .card { background:#fff; border:1px solid #d9dde2; border-radius:10px; padding:16px; margin-bottom:18px; box-shadow:0 3px 8px rgba(15,23,42,0.05); }
    label { display:block; margin:10px 0 4px; font-weight:600; }
    input, select, textarea { width:100%; padding:10px; font-size:1rem; border:1px solid #bfc6cf; border-radius:8px; }
    button { margin-top:12px; padding:12px 18px; border:none; border-radius:10px; cursor:pointer; font-size:1rem; color:#fff; background:#0073e6; }
    button.secondary { background:#5c6b77; }
    .small { font-size:0.9rem; color:#555; }
    .status { padding:12px; border-radius:10px; background:#eef6ff; margin-top:8px; }
    .control-btn { background:#28a745; margin-right:8px; }
  /* Estilos específicos para la lista de eventos reportables */
  #eventsList { border:1px solid #e6eaee; border-radius:8px; max-height:280px; overflow:auto; padding:6px; background:#fff; }
  .event-row { display:flex; align-items:center; gap:10px; padding:8px 6px; border-bottom:1px solid #f0f3f5; }
  .event-row:last-child { border-bottom: none; }
  .event-row input[type="checkbox"] { width:18px; height:18px; margin:0; }
  .event-label { display:inline-block; margin:0; font-weight:500; color:#22303a; }
  .event-meta { margin-left:auto; font-size:0.85rem; color:#6b7880; }
  </style>
</head>
<body>
  <h1>ESP32-C3 SIA DC09</h1>
  <div class="card">
    <h2>Estado</h2>
    <div id="statusArea" class="status">Cargando estado...</div>
  </div>
  <div class="card">
    <h2>Configuración WiFi y Servidores</h2>
    <label for="ssid">SSID WiFi</label>
    <input id="ssid" placeholder="Nombre de la red WiFi" />
    <label for="password">Contraseña WiFi</label>
    <input id="password" placeholder="Contraseña WiFi" type="password" />
    <label for="primaryHost">Servidor primario</label>
    <input id="primaryHost" placeholder="IP o DNS" />
    <label for="primaryPort">Puerto primario</label>
    <input id="primaryPort" placeholder="Puerto" type="number" />
    <label for="secondaryHost">Servidor secundario</label>
    <input id="secondaryHost" placeholder="IP o DNS" />
    <label for="secondaryPort">Puerto secundario</label>
    <input id="secondaryPort" placeholder="Puerto" type="number" />
    <label for="secondaryMode">Modo secundario</label>
    <select id="secondaryMode">
      <option value="backup">Backup</option>
      <option value="redundant">Redundante</option>
    </select>
    <label for="account">Número de cuenta (hexadecimal, ej: 2510, ABCD)</label>
    <input id="account" placeholder="2510" />
    <label for="keepalive">Keepalive (segundos, 0=deshabilitado)</label>
    <input id="keepalive" placeholder="300" type="number" />
    <div id="accountHint" class="small" style="margin-top:6px;color:#555"></div>
    <button onclick="saveConfig()">Guardar configuración</button>
    <div id="saveResult" class="small"></div>
  </div>
  <div class="card">
    <h2>Enviar señal SIA manual</h2>
    <label for="eventType">Tipo de evento</label>
    <select id="eventType"></select>
    <label for="partition">Partición</label>
    <input id="partition" placeholder="1" />
    <label for="address">Dirección / Zona (opcional)</label>
    <input id="address" placeholder="002" />
    <label for="description">Descripción</label>
    <textarea id="description" rows="3" placeholder="Texto adicional para el servidor"></textarea>
    <label for="target">Enviar a</label>
    <select id="target">
      <option value="primary">Primario</option>
      <option value="secondary">Secundario</option>
      <option value="all">Primario + Secundario</option>
    </select>
    <button onclick="sendEvent()">Enviar señal</button>
    <div id="sendResult" class="small"></div>
  </div>
  <div class="card">
    <h2>Control</h2>
    <button class="control-btn" onclick="control('sendTest')">Enviar evento de prueba</button>
    <button class="control-btn" onclick="control('reconnect')">Forzar reconexión WiFi</button>
    <button class="control-btn" style="background:#dc3545" onclick="control('restart')">Reiniciar MCU</button>
    <div id="controlResult" class="small"></div>
  </div>
  <div class="card">
    <h2>Último mensaje SIA</h2>
    <button onclick="fetchLastMsg()">Ver último mensaje</button>
    <pre id="lastMsg" style="white-space:pre-wrap; word-break:break-word; background:#fff; padding:10px; border-radius:8px; margin-top:8px; border:1px solid #dfe6eb;"></pre>
  </div>
  <div class="card">
    <h2>Eventos reportables</h2>
    <div id="eventsList" class="small">Cargando eventos...</div>
    <button type="button" id="saveEventsBtn" onclick="saveEvents()" style="margin-top:12px;">Guardar eventos</button>
    <div id="eventsResult" class="small"></div>
  </div>
  <script>
    // El select de tipos se rellena dinámicamente desde /events (código SIA + descripción)
    // Mapa local para almacenar el estado actual de los checkboxes y evitar condiciones de carrera
    let pendingEvents = {};

    function normalizeAccountJS(input) {
      let s = (input||"").trim().toUpperCase();
      if (s.length === 0) return '2510';
      // aceptar hex 1-4 dígitos, rellenar a 4
      if (s.length > 4) return '2510';
      if (!/^[0-9A-F]+$/.test(s)) return '2510';
      return s.padStart(4, '0');
    }

    function validateAccountInput() {
      const acct = document.getElementById('account').value.trim();
      const hint = document.getElementById('accountHint');
      if (acct.length === 0) { hint.textContent = ''; document.getElementById('account').style.borderColor = ''; return true; }
      const upper = acct.toUpperCase();
      if (upper.length > 4) { 
        hint.textContent = 'Máximo 4 dígitos hexadecimales'; 
        document.getElementById('account').style.borderColor = '#dc3545'; 
        return false; 
      }
      if (!/^[0-9A-F]+$/.test(upper)) {
        hint.textContent = 'Solo dígitos hexadecimales (0-9, A-F)';
        document.getElementById('account').style.borderColor = '#dc3545';
        return false;
      }
      const normalized = upper.padStart(4, '0');
      hint.textContent = `Se guardará como: ${normalized}`;
      document.getElementById('account').style.borderColor = '#28a745';
      return true;
    }

    async function fetchStatus() {
      const res = await fetch('/status');
      const json = await res.json();
      document.getElementById('statusArea').innerHTML = `
        AP: <strong>${json.apSsid}</strong><br>
        STA: <strong>${json.wifiStatus}</strong><br>
        IP: <strong>${json.ip}</strong><br>
        Primario: <strong>${json.primaryHost}:${json.primaryPort}</strong><br>
        Secundario: <strong>${json.secondaryHost}:${json.secondaryPort}</strong><br>
        Modo secundario: <strong>${json.secondaryMode}</strong><br>
        Último envío: <strong>${json.lastSendStatus}</strong><br>
      `;
      // solo actualizar campos si NO están siendo editados actualmente
      const activeElem = document.activeElement;
      if (activeElem.id !== 'ssid') document.getElementById('ssid').value = json.wifiSsid;
      if (activeElem.id !== 'password') document.getElementById('password').value = json.wifiPassword;
      if (activeElem.id !== 'primaryHost') document.getElementById('primaryHost').value = json.primaryHost;
      if (activeElem.id !== 'primaryPort') document.getElementById('primaryPort').value = json.primaryPort;
      if (activeElem.id !== 'secondaryHost') document.getElementById('secondaryHost').value = json.secondaryHost;
      if (activeElem.id !== 'secondaryPort') document.getElementById('secondaryPort').value = json.secondaryPort;
      if (activeElem.id !== 'secondaryMode') document.getElementById('secondaryMode').value = json.secondaryMode;
      if (activeElem.id !== 'account') document.getElementById('account').value = json.account;
      if (activeElem.id !== 'keepalive') document.getElementById('keepalive').value = json.keepalive;
    }

    async function saveConfig() {
      // validar cuenta en cliente antes de enviar
      if (!validateAccountInput()) {
        document.getElementById('saveResult').textContent = 'Cuenta inválida. Corrige antes de guardar.';
        return;
      }
      const acctInput = document.getElementById('account');
      const normalized = normalizeAccountJS(acctInput.value);
      acctInput.value = normalized;
      const body = new URLSearchParams();
      body.append('ssid', document.getElementById('ssid').value);
      body.append('password', document.getElementById('password').value);
      body.append('primaryHost', document.getElementById('primaryHost').value);
      body.append('primaryPort', document.getElementById('primaryPort').value);
      body.append('secondaryHost', document.getElementById('secondaryHost').value);
      body.append('secondaryPort', document.getElementById('secondaryPort').value);
      body.append('secondaryMode', document.getElementById('secondaryMode').value);
      body.append('account', normalized);
      body.append('keepalive', document.getElementById('keepalive').value || '300');
      const res = await fetch('/save', { method: 'POST', body });
      const json = await res.json();
      document.getElementById('saveResult').textContent = json.success ? 'Configuración guardada.' : 'Error guardando.';
      fetchStatus();
        fetchEvents();
    }

      async function fetchEvents() {
        const res = await fetch('/events');
        const list = await res.json();
        const container = document.getElementById('eventsList');
        container.innerHTML = '';
        // inicializar mapa local
        pendingEvents = {};
        // construir filas alineadas con checkbox, etiqueta y meta
        list.forEach(e => {
          const id = 'ev_' + e.key;
          const row = document.createElement('div');
          row.className = 'event-row';

          const cb = document.createElement('input');
          cb.type = 'checkbox';
          cb.id = id;
          cb.checked = e.enabled;
          // guardar estado inicial
          pendingEvents[e.key] = !!e.enabled;
          // actualizar mapa cuando el usuario cambie
          cb.addEventListener('change', () => { pendingEvents[e.key] = cb.checked; });

          const label = document.createElement('label');
          label.htmlFor = id;
          label.className = 'event-label';
          label.textContent = `${e.key} - ${e.desc}`;

          const meta = document.createElement('div');
          meta.className = 'event-meta';
          meta.textContent = e.code;

          row.appendChild(cb);
          row.appendChild(label);
          row.appendChild(meta);
          container.appendChild(row);
        });
        // también rellenar el select de tipos con código + descripción
        const typeSelect = document.getElementById('eventType');
        typeSelect.innerHTML = '';
        list.forEach(ev => {
          const opt = document.createElement('option');
          opt.value = ev.code;
          opt.textContent = `${ev.code} - ${ev.desc}`;
          typeSelect.appendChild(opt);
        });
      }

      async function saveEvents() {
        // Construir objeto JSON con el estado actual desde pendingEvents (evita condiciones de carrera con DOM)
        const payload = {};
        // si pendingEvents está vacío, obtener la lista primero
        if (!pendingEvents || Object.keys(pendingEvents).length === 0) {
          const resList = await fetch('/events');
          const list = await resList.json();
          list.forEach(e => pendingEvents[e.key] = !!e.enabled);
        }
        Object.keys(pendingEvents).forEach(k => { payload['enable_' + k] = pendingEvents[k] ? 1 : 0; });
        // UI helpers
        const btn = document.getElementById('saveEventsBtn');
        btn.disabled = true;
        document.getElementById('eventsResult').textContent = 'Guardando...';
        const res = await fetch('/saveEvents', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(payload) });
        const json = await res.json();
        if (json.savedKeys && json.savedKeys.length) {
          const lines = json.savedKeys.map(k => `${k.key} = ${k.value}`);
          document.getElementById('eventsResult').textContent = 'Guardado: ' + lines.join(', ');
          // aplicar el estado confirmado a los checkboxes (sin refrescar todo)
          json.savedKeys.forEach(k => {
            const id = 'ev_' + k.key.replace('enable_', '');
            const el = document.getElementById(id);
            if (el) {
              const checked = (k.value === true || k.value === 'true' || k.value === 1 || k.value === '1');
              el.checked = checked;
              pendingEvents[k.key.replace('enable_', '')] = checked;
            }
          });
        } else {
          document.getElementById('eventsResult').textContent = json.saved ? 'Eventos guardados.' : 'Error guardando eventos.';
        }
        btn.disabled = false;
      }

    async function sendEvent() {
      const body = new URLSearchParams();
      body.append('eventType', document.getElementById('eventType').value);
      body.append('partition', document.getElementById('partition').value || '1');
      body.append('address', document.getElementById('address').value);
      body.append('description', document.getElementById('description').value);
      body.append('target', document.getElementById('target').value);
      const res = await fetch('/send', { method: 'POST', body });
      const json = await res.json();
      document.getElementById('sendResult').textContent = json.queued ? 'Señal en cola para enviar.' : 'Error al poner en cola.';
      setTimeout(fetchStatus, 1000);
    }

    async function control(action) {
      const body = new URLSearchParams();
      body.append('action', action);
      const res = await fetch('/control', { method: 'POST', body });
      const json = await res.json();
      document.getElementById('controlResult').textContent = json.result;
      setTimeout(fetchStatus, 1000);
    }

    async function fetchLastMsg() {
      const res = await fetch('/lastmsg');
      const json = await res.json();
      document.getElementById('lastMsg').textContent = json.last || '(vacío)';
    }

    fetchStatus();
    fetchEvents();
    setInterval(fetchStatus, 5000);
    // vincular validación de cuenta al input
    document.getElementById('account').addEventListener('input', validateAccountInput);
  </script>
</body>
</html>
 )rawliteral";
  return html;
}

void handleRoot() {
  server.send(200, "text/html", htmlPage());
}

void handleStatus() {
  String payload = "{";
  payload += "\"apSsid\":\"" + String(AP_SSID) + "\",";
  payload += "\"wifiSsid\":\"" + config.wifiSsid + "\",";
  payload += "\"wifiPassword\":\"" + config.wifiPassword + "\",";
  payload += "\"primaryHost\":\"" + config.primaryHost + "\",";
  payload += "\"primaryPort\":" + String(config.primaryPort) + ",";
  payload += "\"secondaryHost\":\"" + config.secondaryHost + "\",";
  payload += "\"secondaryPort\":" + String(config.secondaryPort) + ",";
  payload += "\"secondaryMode\":\"" + String(config.secondaryRedundant ? "redundant" : "backup") + "\",";
  payload += "\"account\":\"" + config.account + "\",";
  payload += "\"wifiStatus\":\"" + String(WiFi.status() == WL_CONNECTED ? "Conectado" : "Desconectado") + "\",";
  payload += "\"ip\":\"" + String(WiFi.localIP().toString()) + "\",";
  payload += "\"keepalive\":" + String(config.keepaliveInterval) + ",";
  payload += "\"lastSendStatus\":\"" + lastSendStatus + "\"";
  payload += "}";
  server.send(200, "application/json", payload);
}

void handleSave() {
  config.wifiSsid = server.arg("ssid");
  config.wifiPassword = server.arg("password");
  config.primaryHost = server.arg("primaryHost");
  config.primaryPort = server.arg("primaryPort").toInt();
  config.secondaryHost = server.arg("secondaryHost");
  config.secondaryPort = server.arg("secondaryPort").toInt();
  config.secondaryRedundant = server.arg("secondaryMode") == "redundant";
  // normalizar la cuenta: hex 1-4 dígitos, rellenar a 4, sin conversión
  config.account = normalizeAccount(server.arg("account"));
  if (config.account.length() == 0) {
    config.account = "2510";
  }
  if (config.primaryPort == 0) {
    config.primaryPort = 3000;
  }
  // keepalive
  config.keepaliveInterval = server.arg("keepalive").toInt();
  saveConfig();
  hasWifiCredentials = config.wifiSsid.length() > 0 && config.wifiPassword.length() > 0;
  if (hasWifiCredentials) {
    startStation();
  }
  lastSendStatus = "Configuración guardada";
  server.send(200, "application/json", "{\"success\":true}");
  // Si keepalive está configurado y el evento KEEPALIVE está habilitado, encolarlo inmediatamente
  if (config.keepaliveInterval > 0 && isSiaEventEnabled("KEEPALIVE")) {
    for (int i = 0; i < siaMapCount; ++i) {
      if (strcmp(siaMap[i].eventKey, "KEEPALIVE") == 0) {
        pendingType = String(siaMap[i].siaCode);
        pendingPartition = "1";
        pendingAddress = "";
        pendingDescription = "Keepalive (guardado config)";
        pendingTarget = defaultSiaTarget();
        sendQueued = true;
        break;
      }
    }
  }
}

void handleSend() {
  pendingType = server.arg("eventType");
  pendingPartition = server.arg("partition");
  pendingAddress = server.arg("address");
  pendingDescription = server.arg("description");
  pendingTarget = server.arg("target");
  if (pendingPartition.length() == 0) {
    pendingPartition = "1";
  }
  if (pendingType.length() == 0) {
    pendingType = "BA";
  }
  sendQueued = true;
  lastSendStatus = "Evento en cola";
  server.send(200, "application/json", "{\"queued\":true}");
}

void handleControl() {
  String action = server.arg("action");
  String result = "";
  if (action == "reconnect") {
    startStation();
    result = "Reconexión forzada iniciada";
    Serial.println("Control: inicio forzado de conexión STA");
  } else if (action == "sendTest") {
    // configurar un evento de prueba y ponerlo en cola
    pendingType = "BA";
    pendingPartition = "1";
    pendingAddress = "002";
    pendingDescription = "Evento de prueba (control)";
    pendingTarget = defaultSiaTarget();
    sendQueued = true;
    result = "Evento de prueba en cola";
    Serial.println("Control: evento de prueba en cola");
  } else if (action == "restart") {
    result = "Reiniciando...";
    server.send(200, "application/json", String("{\"result\":\"") + result + "\"}");
    Serial.println("Control: reiniciando MCU");
    delay(200);
    ESP.restart();
    return;
  } else {
    result = "Acción desconocida";
  }
  server.send(200, "application/json", String("{\"result\":\"") + result + "\"}");
}

void handleNotFound() {
  server.send(404, "text/plain", "No encontrado");
}

void handleWifi() {
  if (!hasWifiCredentials) {
    wifiConnectedPreviously = false;
    return;
  }

  if (WiFi.status() != WL_CONNECTED && millis() - lastReconnectAttempt > 5000) {
    lastReconnectAttempt = millis();
    Serial.println("Intentando reconectar WiFi STA...");
    WiFi.disconnect();
    WiFi.begin(config.wifiSsid.c_str(), config.wifiPassword.c_str());
  }

  if (WiFi.status() == WL_CONNECTED) {
    if (!wifiConnectedPreviously) {
      wifiConnectedPreviously = true;
      Serial.printf("WiFi STA conectado: %s, IP: %s\n", config.wifiSsid.c_str(), WiFi.localIP().toString().c_str());
      // encender LED (activo-Bajo) indicando que la STA está conectada
      digitalWrite(LED_PIN, LOW);
      // encolar evento WiFi conectado si está mapeado y habilitado
      if (isSiaEventEnabled("WIFI_CONNECTED")) {
        for (int i = 0; i < siaMapCount; ++i) {
          if (strcmp(siaMap[i].eventKey, "WIFI_CONNECTED") == 0) {
            pendingType = String(siaMap[i].siaCode);
            pendingPartition = "1";
            pendingAddress = "";
            pendingDescription = "WiFi conectado";
            pendingTarget = defaultSiaTarget();
            sendQueued = true;
            break;
          }
        }
      }
    }
  } else {
    if (wifiConnectedPreviously) {
      wifiConnectedPreviously = false;
      Serial.println("WiFi STA desconectado.");
      // apagar LED (activo-Bajo) cuando la STA se desconecta
      digitalWrite(LED_PIN, HIGH);
      // encolar evento WiFi desconectado si está mapeado y habilitado
      if (isSiaEventEnabled("WIFI_DISCONNECTED")) {
        for (int i = 0; i < siaMapCount; ++i) {
          if (strcmp(siaMap[i].eventKey, "WIFI_DISCONNECTED") == 0) {
            pendingType = String(siaMap[i].siaCode);
            pendingPartition = "1";
            pendingAddress = "";
            pendingDescription = "WiFi desconectado";
            pendingTarget = defaultSiaTarget();
            sendQueued = true;
            break;
          }
        }
      }
    }
  }
}

void handleSendQueue() {
  if (!sendQueued || sendingNow) {
    return;
  }
  sendingNow = true;
  sendQueued = false;
  if (WiFi.status() != WL_CONNECTED) {
    lastSendStatus = "No conectado a WiFi STA";
    Serial.println("No se puede enviar: WiFi no conectado.");
    sendingNow = false;
    return;
  }

  bool result = false;
  if (pendingTarget == "primary") {
    result = sendSiaEventNow("primary");
  } else if (pendingTarget == "secondary") {
    result = sendSiaEventNow("secondary");
  } else {
    result = sendSiaEventNow("all");
  }
  lastSendStatus = result ? "Envío completado" : "Error en el envío";
  if (result) {
    config.seq = (config.seq % 9999) + 1;
    prefs.putUInt("seq", config.seq);
    Serial.printf("Secuencia actualizada a %u\n", config.seq);
  }
  sendingNow = false;
}

void setupWebServer() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/status", HTTP_GET, handleStatus);
  server.on("/save", HTTP_POST, handleSave);
  server.on("/send", HTTP_POST, handleSend);
  server.on("/lastmsg", HTTP_GET, []() {
    String payload = "{";
    payload += "\"last\":\"" + lastSiaMessage + "\"";
    payload += "}";
    server.send(200, "application/json", payload);
  });
  server.on("/control", HTTP_POST, handleControl);
  server.on("/test", HTTP_GET, []() {
    // endpoint simple de diagnóstico
    server.send(200, "text/plain", "OK - Servidor respondiendo");
  });
  server.on("/scan", HTTP_GET, []() {
    // escanear redes disponibles para diagnóstico
    Serial.println("Iniciando escaneo de redes WiFi...");
    int n = WiFi.scanNetworks();
    String result = "Redes encontradas: " + String(n) + "\n";
    for (int i = 0; i < n; ++i) {
      result += String(i+1) + ". " + WiFi.SSID(i) + " (potencia: " + String(WiFi.RSSI(i)) + ")\n";
    }
    server.send(200, "text/plain", result);
    Serial.print(result);
  });
  server.on("/events", HTTP_GET, []() {
    // devolver lista de eventos y estado (enabled)
    String out = "[";
    for (int i = 0; i < siaMapCount; ++i) {
      String key = String(siaMap[i].eventKey);
      String shortKey = String("ev") + String(i);
      bool en = prefs.getBool(shortKey.c_str(), siaMap[i].defaultEnabled);
      out += "{";
      out += "\"key\":\"" + key + "\",";
      out += "\"code\":\"" + String(siaMap[i].siaCode) + "\",";
      out += "\"desc\":\"" + String(siaMap[i].description) + "\",";
      out += "\"enabled\":" + String(en ? "true" : "false");
      out += "}";
      if (i < siaMapCount - 1) out += ",";
    }
    out += "]";
    server.send(200, "application/json", out);
  });
  server.on("/saveEvents", HTTP_POST, []() {
    // Handler robusto para guardar enable_<KEY>=1/0
    // Primero, comprobar si WebServer parseó argumentos normalmente
    int parsed = server.args();
    bool anySaved = false;
    String savedList = "";
    for (int i = 0; i < parsed; ++i) {
      String name = server.argName(i);
      String val = server.arg(i);
      if (name.startsWith("enable_")) {
        bool enabled = (val == "1" || val == "true" || val == "on");
        String evKey = name.substring(7); // after 'enable_'
        int found = -1;
        for (int j = 0; j < siaMapCount; ++j) if (String(siaMap[j].eventKey) == evKey) { found = j; break; }
        if (found >= 0) {
          String shortKey = String("ev") + String(found);
          prefs.putBool(shortKey.c_str(), enabled);
          anySaved = true;
          if (savedList.length() > 0) savedList += ",";
          savedList += "{\"key\":\"" + name + "\",\"value\":" + String(enabled ? "true" : "false") + "}";
        }
      }
    }
    // Si no se parsearon args, intentar leer el body crudo y parsear manualmente
    if (!anySaved) {
      String raw = server.arg("plain");
      // almacenar el raw recibido para diagnóstico vía HTTP
      if (raw.length() > 0) {
        // Si el body es JSON, parsear claves enable_<KEY>
        raw.trim();
        if (raw.startsWith("{")) {
          for (int i = 0; i < siaMapCount; ++i) {
            String pref = String("enable_") + String(siaMap[i].eventKey);
            String needle = String("\"") + pref + String("\"");
            int idx = raw.indexOf(needle);
                if (idx >= 0) {
              int col = raw.indexOf(':', idx + needle.length());
              if (col > 0) {
                int end = raw.indexOf(',', col + 1);
                if (end == -1) end = raw.indexOf('}', col + 1);
                if (end == -1) end = raw.length();
                String token = raw.substring(col + 1, end);
                token.trim();
                token.replace('"', ' ');
                token.trim();
                bool enabled = (token.indexOf('1') >= 0 || token.indexOf('t') >= 0 || token.indexOf('T') >= 0);
                // map pref (enable_<KEY>) to short key ev{index}
                int found = -1;
                for (int j = 0; j < siaMapCount; ++j) if (String(siaMap[j].eventKey) == String(siaMap[i].eventKey)) { found = j; break; }
                if (found >= 0) {
                  String shortKey = String("ev") + String(found);
                  prefs.putBool(shortKey.c_str(), enabled);
                  if (savedList.length() > 0) savedList += ",";
                  savedList += "{\"key\":\"" + pref + "\",\"value\":" + String(enabled ? "true" : "false") + "}";
                }
              }
            }
          }
        } else {
          int pos = 0;
          while (pos < raw.length()) {
            int amp = raw.indexOf('&', pos);
            String pair = (amp == -1) ? raw.substring(pos) : raw.substring(pos, amp);
            int eq = pair.indexOf('=');
            if (eq > 0) {
              String name = pair.substring(0, eq);
              String val = pair.substring(eq + 1);
              name.replace('+', ' ');
              val.replace('+', ' ');
              // decodificar %XX
              auto urlDecode = [&](String s) {
                String out = "";
                for (int p = 0; p < s.length(); ++p) {
                  char c = s.charAt(p);
                  if (c == '%' && p + 2 < s.length()) {
                    String hex = s.substring(p + 1, p + 3);
                    char ch = (char)strtol(hex.c_str(), NULL, 16);
                    out += ch;
                    p += 2;
                  } else {
                    out += c;
                  }
                }
                return out;
              };
              name = urlDecode(name);
              val = urlDecode(val);
              if (name.startsWith("enable_")) {
                bool enabled = (val == "1" || val == "true" || val == "on");
                String evKey = name.substring(7);
                int found = -1;
                for (int j = 0; j < siaMapCount; ++j) if (String(siaMap[j].eventKey) == evKey) { found = j; break; }
                if (found >= 0) {
                  String shortKey = String("ev") + String(found);
                  prefs.putBool(shortKey.c_str(), enabled);
                  if (savedList.length() > 0) savedList += ",";
                  savedList += "{\"key\":\"" + name + "\",\"value\":" + String(enabled ? "true" : "false") + "}";
                }
              }
            }
            if (amp == -1) break;
            pos = amp + 1;
          }
        }
      }
    }
    // Leer de vuelta todas las claves y devolver el estado actual confirmado
    String confirmList = "";
    for (int i = 0; i < siaMapCount; ++i) {
      String pref = String("enable_") + String(siaMap[i].eventKey);
      String shortKey = String("ev") + String(i);
      bool val = prefs.getBool(shortKey.c_str(), siaMap[i].defaultEnabled);
      if (confirmList.length() > 0) confirmList += ",";
      confirmList += "{\"key\":\"" + pref + "\",\"value\":" + String(val ? "true" : "false") + "}";
    }
    // Si KEEPALIVE quedó habilitado tras guardar y hay intervalo, encolarlo para prueba
    for (int i = 0; i < siaMapCount; ++i) {
      if (strcmp(siaMap[i].eventKey, "KEEPALIVE") == 0) {
        String shortKey = String("ev") + String(i);
        bool kaVal = prefs.getBool(shortKey.c_str(), siaMap[i].defaultEnabled);
        if (kaVal && config.keepaliveInterval > 0) {
          pendingType = String(siaMap[i].siaCode);
          pendingPartition = "1";
          pendingAddress = "";
          pendingDescription = "Keepalive (guardado eventos)";
          pendingTarget = defaultSiaTarget();
          sendQueued = true;
        }
        break;
      }
    }
    String resp = "{\"saved\":true,\"savedKeys\": [" + confirmList + "]}";
    server.send(200, "application/json", resp);
  });
  server.onNotFound(handleNotFound);
  server.begin();
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("\n======================================");
  Serial.println("=== ESP32-C3 SIA DC09 Web Config ===");
  Serial.println("======================================");
  Serial.printf("Chipset: %s\n", ESP.getChipModel());
  Serial.printf("Firmware: %s\n", ESP.getSdkVersion());
  
  prefs.begin(PREF_NAMESPACE, false);
  loadConfig();
  // configurar pin LED
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH); // por defecto apagado (no conectado)
  // iniciar DSC Keybus Interface para leer eventos de alarma
  dsc.begin(Serial);
  // enviar evento de arranque si está mapeado y habilitado
  if (isSiaEventEnabled("STARTUP")) {
    for (int i = 0; i < siaMapCount; ++i) {
      if (strcmp(siaMap[i].eventKey, "STARTUP") == 0) {
        pendingType = String(siaMap[i].siaCode);
        pendingPartition = "1";
        pendingAddress = "";
        pendingDescription = "Arranque del dispositivo";
        pendingTarget = defaultSiaTarget();
        sendQueued = true;
        break;
      }
    }
  }
  startAccessPoint();
  if (hasWifiCredentials) {
    startStation();
  }
  setupWebServer();
  
  Serial.println("\n--- INSTRUCCIONES DE USO ---");
  Serial.println("Red WiFi: ESP32-C3-Config");
  Serial.println("Contraseña: 12345678");
  Serial.println("URL web: http://192.168.4.1");
  Serial.println("O prueba escaneo: http://192.168.4.1/scan");
  Serial.println("--- FIN INSTRUCCIONES ---\n");
}

void loop() {
  dsc.loop();
  if (dsc.keybusConnected != keybusConnectedPreviously) {
    keybusConnectedPreviously = dsc.keybusConnected;
    Serial.printf("DSC Keybus %s\n", dsc.keybusConnected ? "conectado" : "desconectado");
  }
  // detectar eventos DSC y mapearlos a SIA
  // handleDscEvents() revisa flags 'Changed' de la librería y encola eventos SIA
  auto handleDscEvents = [&]() {
    // Particiones
    for (byte pi = 0; pi < dscPartitions; ++pi) {
      if (dsc.alarmChanged[pi] && dsc.alarm[pi]) {
        if (!isSiaEventEnabled("PARTITION_ALARM")) continue;
        pendingType = String("BA"); // default si no hay mapeo
        // buscar mapeo en tabla
        for (int i = 0; i < siaMapCount; ++i) {
          if (strcmp(siaMap[i].eventKey, "PARTITION_ALARM") == 0) {
            pendingType = String(siaMap[i].siaCode);
            break;
          }
        }
        pendingPartition = String(pi + 1);
        pendingAddress = ""; // usar partición como partición, no zona
        pendingDescription = String("Particion ") + String(pi + 1) + " en alarma";
        pendingTarget = defaultSiaTarget();
        sendQueued = true;
      }

      if (dsc.armedChanged[pi]) {
        if (dsc.armed[pi]) {
          if (!isSiaEventEnabled("PARTITION_ARMED")) continue;
          // armado
          for (int i = 0; i < siaMapCount; ++i) {
            if (strcmp(siaMap[i].eventKey, "PARTITION_ARMED") == 0) {
              pendingType = String(siaMap[i].siaCode);
              break;
            }
          }
          pendingPartition = String(pi + 1);
          pendingAddress = "";
          pendingDescription = String("Particion ") + String(pi + 1) + " armada";
          pendingTarget = defaultSiaTarget();
          sendQueued = true;
        } else {
          if (!isSiaEventEnabled("PARTITION_DISARMED")) continue;
          for (int i = 0; i < siaMapCount; ++i) {
            if (strcmp(siaMap[i].eventKey, "PARTITION_DISARMED") == 0) {
              pendingType = String(siaMap[i].siaCode);
              break;
            }
          }
          pendingPartition = String(pi + 1);
          pendingAddress = "";
          pendingDescription = String("Particion ") + String(pi + 1) + " desarmada";
          pendingTarget = defaultSiaTarget();
          sendQueued = true;
        }
      }
    }

    // Zonas (alarmZones) - dsc.alarmZones is array of bytes (bitmask)
    for (byte gi = 0; gi < dscZones; ++gi) {
      if (dsc.alarmZonesChanged[gi]) {
        if (!isSiaEventEnabled("ZONE_ALARM")) continue;
        byte mask = dsc.alarmZones[gi];
        for (byte bit = 0; bit < 8; ++bit) {
          if (mask & (1 << bit)) {
            int zoneNumber = gi * 8 + bit + 1;
            // mapear a SIA
            for (int i = 0; i < siaMapCount; ++i) {
              if (strcmp(siaMap[i].eventKey, "ZONE_ALARM") == 0) {
                pendingType = String(siaMap[i].siaCode);
                break;
              }
            }
            // dirección = número de zona (3 dígitos)
            char buf[8];
            snprintf(buf, sizeof(buf), "%03d", zoneNumber);
            pendingAddress = String(buf);
            pendingPartition = "1"; // por defecto partición 1 (ajustable)
            pendingDescription = String("Zona ") + String(zoneNumber) + " en alarma";
            pendingTarget = defaultSiaTarget();
            sendQueued = true;
          }
        }
      }
    }

    // Otros eventos: power/trouble/pgm
    if (dsc.powerChanged) {
      if (!isSiaEventEnabled("AC_FAIL")) {
        // limpiar flag y no reportar
      } else {
        for (int i = 0; i < siaMapCount; ++i) {
          if (strcmp(siaMap[i].eventKey, "AC_FAIL") == 0) {
            pendingType = String(siaMap[i].siaCode);
            break;
          }
        }
        pendingPartition = "1";
        pendingAddress = "";
        pendingDescription = dsc.powerTrouble ? "AC Fallo" : "AC OK";
        pendingTarget = defaultSiaTarget();
        sendQueued = true;
      }
    }
    if (dsc.batteryChanged) {
      if (isSiaEventEnabled("BATTERY_LOW")) {
        for (int i = 0; i < siaMapCount; ++i) {
          if (strcmp(siaMap[i].eventKey, "BATTERY_LOW") == 0) {
            pendingType = String(siaMap[i].siaCode);
            break;
          }
        }
        pendingPartition = "1";
        pendingAddress = "";
        pendingDescription = dsc.batteryTrouble ? "Bateria baja" : "Bateria OK";
        pendingTarget = defaultSiaTarget();
        sendQueued = true;
      }
    }
  };
  handleDscEvents();
  // enviar keepalive si está configurado
  if (config.keepaliveInterval > 0) {
    unsigned long now = millis();
    if (now - lastKeepaliveSent >= (unsigned long)config.keepaliveInterval * 1000UL) {
      lastKeepaliveSent = now;
      if (isSiaEventEnabled("KEEPALIVE")) {
        for (int i = 0; i < siaMapCount; ++i) {
          if (strcmp(siaMap[i].eventKey, "KEEPALIVE") == 0) {
            pendingType = String(siaMap[i].siaCode);
            pendingPartition = "1";
            pendingAddress = "";
            pendingDescription = "Keepalive";
            pendingTarget = defaultSiaTarget();
            sendQueued = true;
            break;
          }
        }
      }
    }
  }
  server.handleClient();
  handleWifi();
  handleSendQueue();
  if (millis() - lastStatusPrint > 15000) {
    lastStatusPrint = millis();
    uint8_t clients = WiFi.softAPgetStationNum();
    int8_t power = WiFi.getTxPower();
    Serial.printf("=== Status @%lums ===\n", millis());
    Serial.printf("AP SSID: %s\n", WiFi.softAPSSID().c_str());
    Serial.printf("AP IP: %s | Clientes: %d\n", WiFi.softAPIP().toString().c_str(), clients);
    Serial.printf("STA: %s\n", WiFi.status() == WL_CONNECTED ? "Conectado" : "Desconectado");
    Serial.printf("Potencia TX: %d dBm\n", power);
    Serial.println("===================\n");
  }
}
