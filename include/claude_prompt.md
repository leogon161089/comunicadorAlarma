# Prompt y resumen para uso con Claude — Referencia del proyecto

Este archivo sirve para pegar el prompt que se le entregará a Claude cuando se le pida generar/modificar código del proyecto, y como referencia rápida del objetivo del proyecto.

=== Instrucciones de uso del prompt ===
- Pega el bloque "Prompt para Claude" tal cual en la interfaz de Claude.
- Adjunta o referencia archivos relevantes si la interfaz lo permite (por ejemplo `src/main.cpp`, `include/sia_map.h`).
- Indica la tarea concreta (ej.: "corrige duplicado de envío", "implementar failover primario/backup en sendToHost()").

---

## Prompt para Claude (plantilla)

# Proyecto SmartHome Cloud Alarm Communicator

## Objetivo General

Diseñar una plataforma IoT profesional para monitoreo de alarmas, gestión remota, automatización y notificaciones, basada inicialmente en ESP32-C3 con conectividad WiFi y preparada desde el inicio para futuras versiones WiFi + 4G utilizando módulos LTE-M / NB-IoT como SIM7000.

La arquitectura debe ser escalable desde unas pocas unidades hasta miles de dispositivos sin requerir rediseños importantes.

El objetivo de este documento es que comprendas la visión completa del producto, los requisitos funcionales, los requisitos técnicos y la arquitectura deseada antes de comenzar cualquier desarrollo.

---

# Contexto del Proyecto

Actualmente existe un firmware funcional sobre ESP32-C3 SuperMini.

El firmware ya es capaz de:

* Conectarse a Internet mediante WiFi.
* Reportar eventos hacia una central de monitoreo.
* Enviar señales compatibles con SIA DC-09 mediante HTTP POST.
* Utilizar una librería que permite representar un teclado virtual de alarma.

El proyecto se encuentra en una etapa temprana donde es prioritario definir correctamente la arquitectura antes de continuar agregando funcionalidades.

---

# Modalidades de Uso

El comunicador debe soportar tres escenarios:

## Modalidad 1: Monitoreo Profesional

El dispositivo reporta eventos hacia una central de monitoreo utilizando SIA DC-09.

Ejemplos:

* Apertura de zona.
* Restauración de zona.
* Armado.
* Desarmado.
* Pérdida de energía.
* Batería baja.
* Fallas de comunicación.

El servidor, puerto y número de abonado son configurables.

---

## Modalidad 2: Aplicación para Usuario Final

El usuario utiliza una aplicación web para:

* Ver el estado de la alarma.
* Operar un teclado virtual.
* Armar y desarmar.
* Ver eventos.
* Recibir notificaciones push.
* Consultar estados en tiempo real.

No requiere servicio de monitoreo.

---

## Modalidad 3: Mixta

El equipo utiliza simultáneamente:

* Monitoreo profesional.
* Aplicación para usuario final.

Las dos funciones deben ser independientes.

---

# Filosofía de Diseño

Separar completamente:

## Canal de Monitoreo

Dispositivo → Central de Monitoreo

## Canal Cloud

Dispositivo → Plataforma SmartHome Cloud

La caída o modificación de uno no debe afectar al otro.

---

# Hardware Actual

Microcontrolador:

ESP32-C3 SuperMini

Conectividad actual:

WiFi

Conectividad futura:

* WiFi
* LTE-M
* NB-IoT
* Módulos SIM7000 o equivalentes

La arquitectura debe ser compatible desde el inicio con redes de bajo ancho de banda.

---

# Plataforma Cloud

Se busca minimizar costos operativos.

Prioridades:

1. Simplicidad.
2. Escalabilidad.
3. Bajo consumo de datos.
4. Tiempo real.
5. Seguridad.
6. Facilidad de mantenimiento.

---

# Tecnologías Deseadas

## Firebase

Utilizar Firebase exclusivamente para:

### Authentication

Permitir:

* Login con Google.
* Login con email.
* Recuperación de contraseña.

### Firestore

Almacenar:

* Usuarios.
* Dispositivos.
* Permisos.
* Comparticiones.
* Historial.
* Configuraciones.

### Firebase Cloud Messaging

Enviar:

* Notificaciones push.
* Alertas.
* Eventos importantes.

Firebase NO debe ser el mecanismo principal de comunicación en tiempo real entre dispositivos y usuarios.

---

# Comunicación en Tiempo Real

Utilizar MQTT como mecanismo principal.

Razones:

* Menor consumo de datos.
* Menor latencia.
* Compatible con WiFi.
* Compatible con LTE-M.
* Compatible con NB-IoT.
* Escalable.

La comunicación en tiempo real debe ocurrir mediante MQTT.

---

# Broker MQTT

La arquitectura debe asumir la existencia de un broker MQTT central.

Ejemplo conceptual:

mqtt.smarthome.net.ar

Tanto los dispositivos como la aplicación web se conectarán al broker.

---

# Flujo de Estados

Ejemplo:

Zona 3 se abre.

Dispositivo publica evento MQTT.

La aplicación recibe inmediatamente el evento.

La interfaz se actualiza sin refrescar la página.

---

# Flujo de Comandos

Ejemplo:

Usuario presiona Armar.

La aplicación publica un comando MQTT.

El dispositivo recibe el comando.

El dispositivo ejecuta la acción.

El dispositivo publica confirmación.

La aplicación actualiza el estado.

---

# Aplicación de Usuario

La aplicación debe ser una PWA.

No se desea desarrollar aplicaciones nativas Android o iOS.

Objetivos:

* Funcionar desde navegador.
* Instalable.
* Compatible con Android.
* Compatible con iPhone.
* Compatible con Windows.
* Compatible con macOS.

Dominio previsto:

app.smarthome.net.ar

---

# Vinculación de Dispositivos

La experiencia debe ser similar a DVRs con sistema P2P.

Cada dispositivo tendrá:

* Número de serie único.
* Usuario administrador.
* Contraseña inicial.

Ejemplo:

Serial:
SH26-00000001

Usuario:
admin

Contraseña:
AB12CD34

El usuario ingresa estos datos para vincular el dispositivo a su cuenta.

---

# Modelo de Propiedad

Cada dispositivo posee:

## Owner

Usuario propietario.

Tiene control total.

---

## Shared Users

Usuarios invitados.

Pueden acceder al dispositivo.

Se agregan utilizando únicamente el correo electrónico registrado en la plataforma.

Ejemplo:

Propietario:
[juan@gmail.com](mailto:juan@gmail.com)

Comparte con:
[ana@gmail.com](mailto:ana@gmail.com)

Ana ve automáticamente el dispositivo en su cuenta.

---

# Roles del Sistema

## SuperAdmin

Fabricante.

Permisos absolutos.

---

## Installer

Instaladores o revendedores.

Pueden administrar dispositivos asignados.

---

## Client

Usuario final.

Opera dispositivos autorizados.

---

# Administración Remota

El fabricante debe poder:

* Ver dispositivos.
* Diagnosticar equipos.
* Consultar estado online/offline.
* Consultar versión de firmware.
* Enviar comandos administrativos.
* Realizar actualizaciones OTA.

---

# Configuración Remota

Debe existir un mecanismo genérico.

Evitar campos rígidos.

Utilizar estructuras JSON.

Ejemplo:

{
"heartbeat":120,
"wifi_check":60,
"sia_server":"1.2.3.4",
"sia_port":7700
}

Esto permitirá agregar nuevos parámetros sin rediseñar la plataforma.

---

# OTA

El sistema debe contemplar desde el inicio actualizaciones remotas.

Escenarios:

## OTA Local

Desde webserver del dispositivo.

---

## OTA Individual

Desde la nube.

---

## OTA Masiva

Para grupos o todos los dispositivos.

---

# Estados del Dispositivo

Como mínimo almacenar:

* Online.
* Offline.
* RSSI.
* Dirección IP.
* Versión de firmware.
* Uptime.
* Último contacto.
* Método de conexión.
* WiFi.
* LTE-M.
* NB-IoT.

---

# Eventos

Registrar:

* Armado.
* Desarmado.
* Apertura de zona.
* Restauración de zona.
* Pérdida de energía.
* Restablecimiento de energía.
* Fallas.
* Reinicios.
* Actualizaciones.
* Cambios de configuración.

---

# Seguridad

Todo el diseño debe contemplar:

* TLS.
* MQTT seguro.
* Control de permisos.
* Separación entre dispositivos.
* Separación entre usuarios.
* Prevención de acceso no autorizado.

---

# Restricciones Importantes

No asumir que existe backend propio inicialmente.

No asumir servidores dedicados.

Priorizar servicios administrados.

Mantener la arquitectura simple.

Evitar soluciones complejas que requieran DevOps avanzado.

---

# Tu Tarea

Antes de escribir código:

1. Analizar completamente esta arquitectura.
2. Detectar riesgos técnicos.
3. Detectar limitaciones futuras.
4. Proponer mejoras.
5. Diseñar el modelo de datos.
6. Diseñar los topics MQTT.
7. Diseñar la estructura Firebase.
8. Diseñar el flujo de vinculación.
9. Diseñar el flujo OTA.
10. Diseñar la seguridad del sistema.

No comiences implementando código inmediatamente.

Primero produce un documento de arquitectura técnica completo y justificado para revisión.


Fecha: 2026-06-10
