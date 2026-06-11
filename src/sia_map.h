// Archivo editable: mapeo entre eventos detectados en DSC Keybus y códigos SIA
// Edita esta tabla para ajustar los códigos SIA a tu proveedor/central de monitoreo.

#ifndef SIA_MAP_H
#define SIA_MAP_H

struct SiaMapEntry {
  const char* eventKey;    // clave interna del evento (usa en el detector)
  const char* siaCode;     // código SIA (2 letras) que se enviará en el campo tipo
  const char* description; // descripción legible (sólo para referencia)
  bool defaultEnabled;     // si está habilitado por defecto
};

// Entradas por defecto (revisa y ajusta los códigos SIA según tu central)
// Las claves usadas abajo son las que busca el detector en el firmware.
static const SiaMapEntry siaMap[] = {
  {"PARTITION_ALARM", "BA", "Alarma de intrusión (partición)", true},
  {"ZONE_ALARM", "BA", "Alarma de zona (sensor)", true},
  {"PARTITION_ARMED", "AA", "Partición armada", true},
  {"PARTITION_DISARMED", "AD", "Partición desarmada (restore)", true},
  {"ENTRY_DELAY", "EC", "Inicio de retardo de entrada", false},
  {"EXIT_DELAY", "EX", "Inicio de retardo de salida", false},
  {"FIRE_ALARM", "FA", "Alarma de incendio", true},
  {"PANIC", "PA", "Pánico/auxiliar", true},
  {"AC_FAIL", "AC", "Fallo de alimentación AC", true},
  {"BATTERY_LOW", "BT", "Batería baja", true},
  {"TROUBLE", "TR", "Fallo / Trouble", true},
  {"PGM_ON", "PG", "PGM activado", false},
  {"PGM_OFF", "PO", "PGM desactivado", false},
  {"KEYBUS_CONNECTED", "KD", "Keybus conectado", false},
  {"KEYBUS_DISCONNECTED", "KU", "Keybus desconectado", false},
  {"WIFI_CONNECTED", "WC", "WiFi STA conectado", false},
  {"WIFI_DISCONNECTED", "WD", "WiFi STA desconectado", false},
  {"KEEPALIVE", "KA", "Keepalive / Heartbeat", false},
  {"WIFI_RECONNECT", "WR", "Reintento de reconexión WiFi", false},
  {"STARTUP", "ST", "Arranque del dispositivo", false},
  // Añade nuevas entradas según necesites
};

static const int siaMapCount = sizeof(siaMap) / sizeof(siaMap[0]);

#endif // SIA_MAP_H
