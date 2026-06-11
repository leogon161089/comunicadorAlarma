# Roadmap Estratégico del Proyecto

ETAPA 1 - Firmware Base
- Definir hardware definitivo.
- Definir asignación final de GPIO.
- Crear arquitectura modular del firmware.
- Implementar NVS.
- Implementar sistema interno de eventos.
- Implementar sistema de logs.
- Implementar manejo de errores.
- Implementar watchdogs.
- Implementar gestión de reinicios.

ETAPA 2 - Seguridad Local
- Asegurar WiFi AP.
- Generar credenciales únicas de fábrica.
- Implementar autenticación web.
- Implementar cambio de contraseña.
- Implementar bloqueo por intentos fallidos.
- Definir comportamiento de factory reset.
- Proteger configuraciones sensibles.

ETAPA 3 - WebServer Local
- Dashboard.
- Configuración WiFi.
- Configuración SIA.
- Estado de entradas y salidas.
- Eventos y logs.
- Diagnóstico.
- Gestión de usuarios.
- Configuración general.

ETAPA 4 - OTA Local
- Actualización por archivo.
- Verificación de integridad.
- Gestión de versiones.
- Mecanismo de recuperación.

ETAPA 5 - Identidad del Dispositivo
- Serial único.
- UUID interno.
- Credenciales de fábrica.
- Modelo de dispositivo.
- Información de hardware.
- Información de firmware.

ETAPA 6 - MQTT Básico
- Seleccionar broker.
- Conexión MQTT.
- Publicación de eventos.
- Recepción de comandos.
- Reconexión automática.
- Heartbeat.

ETAPA 7 - Protocolo MQTT Definitivo
- Diseñar topics.
- Diseñar payloads.
- Diseñar comandos.
- Diseñar respuestas.
- Diseñar telemetría.
- Diseñar actualizaciones OTA.
- Congelar protocolo.

ETAPA 8 - Backend Cloud
- Crear proyecto Firebase.
- Authentication.
- Firestore.
- Estructura de usuarios.
- Estructura de dispositivos.
- Estructura de permisos.
- Estructura de eventos.
- Estructura de configuraciones.

ETAPA 9 - PWA Inicial
- Login.
- Gestión de sesión.
- Listado de dispositivos.
- Estado online/offline.
- Visualización de eventos.
- Envío de comandos.
- Visualización de estados.

ETAPA 10 - Vinculación y Compartición
- Alta de dispositivos.
- Asociación dispositivo-cuenta.
- Compartición entre usuarios.
- Gestión de permisos.
- Revocación de accesos.

ETAPA 11 - Notificaciones
- Firebase Cloud Messaging.
- Alertas.
- Eventos críticos.
- Configuración de preferencias.
- Gestión de tokens.

ETAPA 12 - OTA Remota
- Publicación de firmware.
- Actualización individual.
- Actualización grupal.
- Actualización masiva.
- Seguimiento de actualizaciones.

ETAPA 13 - Seguridad Cloud
- TLS.
- ACL MQTT.
- Firestore Rules.
- Gestión de permisos.
- Auditoría.
- Protección de APIs.
- Gestión de sesiones.

ETAPA 14 - Infraestructura Productiva
- app.smarthome.net.ar
- mqtt.smarthome.net.ar
- DNS.
- Certificados.
- Backups.
- Logs.
- Monitoreo de servicios.

ETAPA 15 - Soporte SIM7000
- LTE-M.
- NB-IoT.
- Conmutación automática WiFi/4G.
- Telemetría de red.
- Gestión de consumo de datos.

ETAPA 16 - Escalabilidad
- Portal de instaladores.
- Portal de administración.
- Telemetría avanzada.
- Estadísticas.
- Diagnóstico remoto.
- Gestión masiva de dispositivos.
- Herramientas de soporte.
- Herramientas de mantenimiento.

---

Este documento sirve como referencia estratégica y lista de tareas en alto nivel. Puede dividirse en issues y milestones en el control de versiones para seguimiento detallado.

Fecha: 2026-06-08
