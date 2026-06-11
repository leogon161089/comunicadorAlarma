# Referencia de distribución de pines — ESP32 (archivo de solo referencia)

Este archivo es únicamente informativo y no afecta al funcionamiento del firmware.

Descripción de pines proporcionada por el usuario:

- GPIO0: DSC Clock
- GPIO1: DSC Read
- GPIO3: DSC Write
- GPIO4: Entrada Analógica
- GPIO5: Entrada Digital 1
- GPIO6: SIM7600 TX
- GPIO7: SIM7600 RX
- GPIO9: Botón Reset de Fábrica (BOOT)
- GPIO10: Salida Digital (Reset SIM7600)
- GPIO20: Salida Digital (Relé)
- GPIO21: Entrada Digital 2

Notas:

- Verifique la numeración y disponibilidad física en su placa específica antes de conectar/permanecer operacional.
- Algunos pines pueden estar reservados por el USB, la flash o funciones del chipset en ciertas versiones de placa.
- Este documento debe usarse como referencia para futuras consultas y documentación del proyecto.

Fecha: 2026-06-08

Uso de Serial / UART:

- USB (depuración): `Serial`
- Módem SIM7600: `Serial1` (GPIO6 TX, GPIO7 RX)

Importante: el monitor serie y la depuración deben realizarse mediante `Serial` (USB nativo del ESP32-C3). La comunicación con el módem SIM7600 debe realizarse exclusivamente mediante `Serial1` para evitar conexiones accidentales del módem al puerto serie de depuración.

Nota: Asegúrate de no conectar físicamente el SIM7600 al puerto USB/Serial de depuración; usa siempre los pines asignados a `Serial1`.