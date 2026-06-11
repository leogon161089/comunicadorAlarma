// Archivo editable: mapeo entre eventos detectados en DSC Keybus y códigos SIA
// Este archivo contiene un mapeo amplio entre los eventos que la librería
// dscKeybusInterface puede detectar y los códigos SIA-DC09 que se deberían
// enviar a la central. Ajusta los códigos según los requisitos de tu
// proveedor/central si es necesario.

#ifndef SIA_MAP_H
#define SIA_MAP_H

struct SiaMapEntry {
  const char* eventKey;    // clave interna del evento (usa en el detector)
  const char* siaCode;     // código SIA (2 letras) que se enviará en el campo tipo
  const char* description; // descripción legible (sólo para referencia)
  bool defaultEnabled;     // si está habilitado por defecto
};

// Mapeo ampliado — cubre particiones, zonas, power, bateria, tamper, PGM,
// eventos de teclado y otros mensajes de sistema que la librería procesa.
static const SiaMapEntry siaMap[] = {
  // Sistema / Inicio
  {"STARTUP", "ST", "Arranque del dispositivo", true},
  {"KEEPALIVE", "KA", "Keepalive / Heartbeat", true},
  {"KEYBUS_CONNECTED", "KD", "Keybus conectado", false},
  {"KEYBUS_DISCONNECTED", "KU", "Keybus desconectado", true},

  // Particiones
  {"PARTITION_ALARM", "BA", "Alarma de intrusión (partición)", true},
  {"PARTITION_ARMED", "AA", "Partición armada", true},
  {"PARTITION_DISARMED", "AD", "Partición desarmada", true},
  {"PARTITION_READY", "RD", "Partición lista", false},
  {"PARTITION_NOT_READY", "RN", "Partición no lista", false},

  // Retardos
  {"ENTRY_DELAY_START", "EC", "Inicio de retardo de entrada", false},
  {"ENTRY_DELAY_END", "ER", "Fin de retardo de entrada", false},
  {"EXIT_DELAY_START", "EX", "Inicio de retardo de salida", false},
  {"EXIT_DELAY_END", "XR", "Fin de retardo de salida", false},

  // Zonas
  {"ZONE_ALARM", "BA", "Zona en alarma (sensor)", true},
  {"ZONE_RESTORE", "AD", "Zona restaurada", true},
  {"ZONE_OPEN", "ZO", "Zona abierta (tamper/abierta)", false},
  {"ZONE_TAMPER", "ZT", "Tamper en zona", true},
  {"ZONE_FAULT", "ZF", "Fallo en zona", true},
  {"ZONE_BYPASS", "BY", "Zona en bypass", false},
  {"ZONE_UNBYPASS", "UB", "Zona fuera de bypass", false},

  // Incendio / Pánico / Aux
  {"FIRE_ALARM", "FA", "Alarma de incendio", true},
  {"PANIC", "PA", "Pánico (general)", true},
  {"KEYPAD_PANIC", "PP", "Pánico desde teclado", true},
  {"KEYPAD_FIRE", "PF", "Fuego desde teclado", true},
  {"KEYPAD_AUX", "PA", "Auxiliar desde teclado", false},

  // Alimentación y batería
  {"AC_FAIL", "AC", "Fallo de alimentación AC", true},
  {"AC_RESTORE", "AR", "Restauración de alimentación AC", true},
  {"BATTERY_LOW", "BT", "Batería baja", true},
  {"BATTERY_OK", "BR", "Batería OK/restaurada", true},

  // Trouble / Estado del sistema
  {"TROUBLE", "TR", "Fallo / Trouble", true},
  {"TROUBLE_RESTORE", "TR", "Fallo resuelto", false},
  {"MODULE_FAULT", "MF", "Fallo en módulo/expansión", true},

  // PGM / Salidas
  {"PGM_ON", "PG", "PGM activado", false},
  {"PGM_OFF", "PO", "PGM desactivado", false},

  // Eventos de teclado / códigos de acceso
  {"ACCESS_CODE", "AC", "Evento por código de acceso", false},
  {"INVALID_CODE", "IC", "Código de acceso inválido", false},

  // WiFi / conectividad
  {"WIFI_CONNECTED", "WC", "WiFi STA conectado", true},
  {"WIFI_DISCONNECTED", "WD", "WiFi STA desconectado", true},
  {"WIFI_RECONNECT", "WR", "Reintento de reconexión WiFi", false},

  // Misc / diagnóstico
  {"SYSTEM_TAMPER", "TM", "Tamper del sistema", true},
  {"EVENT_BUFFER", "EB", "Evento buffer / revisión", false},

  // Mantener compatibilidad con nombres previos/uso en firmware
  {"PARTITION_ARMED_STAY", "AA", "Partición armada (stay)", false},
  {"PARTITION_ARMED_AWAY", "AA", "Partición armada (away)", false},
  {"WIFI_UP", "WC", "Alias WiFi conectado", false},
  {"WIFI_DOWN", "WD", "Alias WiFi desconectado", false},

  // Depuración / pruebas
  {"TEST_EVENT", "TT", "Evento de prueba/manual", false}
};

static const int siaMapCount = sizeof(siaMap) / sizeof(siaMap[0]);

#endif // SIA_MAP_H
