# 🟥🟩🟦 Panel LED WS2812B con ESP32
### Proyecto de Electrónica y Programación — Escuela Técnica

> **¿Qué vas a construir?**
> Un panel de LEDs a color inteligente, controlado por WiFi, que puede mostrar texto en movimiento, imágenes y efectos de color. Lo vas a poder manejar desde tu celular usando Telegram. ¡Como una pantalla gigante casera!

---

## 📋 Tabla de Contenidos

1. [¿Qué es este proyecto?](#1--qué-es-este-proyecto)
2. [Materiales necesarios](#2--materiales-necesarios)
3. [¿Qué es el ESP32?](#3--qué-es-el-esp32)
4. [¿Qué son los LEDs WS2812B?](#4--qué-son-los-leds-ws2812b)
5. [Circuito electrónico completo](#5--circuito-electrónico-completo)
6. [¿Por qué el entorno eléctrico es agresivo? Protecciones](#6--por-qué-el-entorno-eléctrico-es-agresivo-protecciones)
7. [Cálculo del consumo eléctrico y fuente de alimentación](#7--cálculo-del-consumo-eléctrico-y-fuente-de-alimentación)
8. [Instalación del software](#8--instalación-del-software)
9. [Configuración del código](#9--configuración-del-código)
10. [¿Cómo funciona el código? (explicación sección por sección)](#10--cómo-funciona-el-código-sección-por-sección)
11. [Cómo construir el bot de Telegram](#11--cómo-construir-el-bot-de-telegram)
12. [Comandos disponibles y cómo usarlos](#12--comandos-disponibles-y-cómo-usarlos)
13. [Control por WebSocket (streaming de video)](#13--control-por-websocket-streaming-de-video)
14. [Resolución de problemas frecuentes](#14--resolución-de-problemas-frecuentes)
15. [Glosario de términos técnicos](#15--glosario-de-términos-técnicos)

---

## 1. 🎯 ¿Qué es este proyecto?

Este proyecto consiste en construir un **panel de LEDs a color** usando tiras de LEDs inteligentes (modelo **WS2812B**) y una placa de desarrollo llamada **ESP32**.

### ¿Qué puede hacer el panel?

| Función | Descripción |
|---|---|
| 📝 **Texto con scroll** | Muestra texto que se desplaza de derecha a izquierda, como un cartel luminoso |
| 🖼️ **Imágenes JPEG** | Recibe fotos por Telegram y las muestra pixeladas en el panel |
| 🌈 **Color sólido** | Pinta todo el panel de un color a elección |
| 🎞️ **Transiciones animadas** | Cambia entre modos con efectos: fade, barrido, disolución |
| 📡 **Streaming por WiFi** | Recibe frames de video en tiempo real desde una computadora |
| 📱 **Control por Telegram** | Envías comandos desde tu celular por Telegram |

### ¿Cómo se ve el panel?

El panel tiene una relación de aspecto **16:9** (como la pantalla de tu TV o celular). Está formado por tiras de LEDs apiladas una encima de la otra:

```
 ← 60 LEDs (= 1 metro de tira) →
 ┌──────────────────────────────────┐  ↑
 │ → → → → → → → → → → → → → → →  │  │
 │ ← ← ← ← ← ← ← ← ← ← ← ← ← ←  │  │
 │ → → → → → → → → → → → → → → →  │ 34
 │ ← ← ← ← ← ← ← ← ← ← ← ← ← ←  │ tiras
 │             ...                  │  │
 └──────────────────────────────────┘  ↓
```

Las flechas muestran la dirección en que están soldadas las tiras. Esto se llama **conexión serpenteante** (o "zigzag") y es importante para que el código sepa a qué LED corresponde cada punto en pantalla.

---

## 2. 🛒 Materiales necesarios

### Componentes principales

| Cantidad | Componente | Especificación | Para qué sirve |
|---|---|---|---|
| 1 | **ESP32 Dev Module** | 38 pines, dual core, 240 MHz | El "cerebro" del sistema |
| Varias | **Tiras LED WS2812B** | 60 LEDs/metro, IP30 o IP65 | Los píxeles del panel |
| 1 | **Fuente de alimentación** | 5V, ver sección 7 | Alimentar los LEDs |
| 4–8 | **CI conversor de nivel lógico** | SN74HCT245 o 74AHCT125 | Proteger el ESP32 del ruido |
| Varios | **Capacitores electrolíticos** | 1000 µF 10V | Filtrar la alimentación |
| Varios | **Capacitores cerámicos** | 100 nF (0.1 µF) | Desacoplo de alta frecuencia |
| Varios | **Resistencias** | 330 Ω – 470 Ω | Proteger la señal de datos |
| 1 | **Cable de alimentación** | Sección ≥ 2.5 mm² | Conducir corriente alta |
| 1 | **Riel DIN / caja** | — | Montar y proteger la electrónica |

### Herramientas

- Soldador de 30–60 W + estaño
- Multímetro digital
- Fuente de laboratorio (para pruebas, opcional pero muy útil)
- Computadora con Arduino IDE instalado
- Cable USB tipo A/B o USB-C (según el ESP32 que tengas)

---

## 3. 🧠 ¿Qué es el ESP32?

El ESP32 es un **microcontrolador** fabricado por la empresa Espressif. Un microcontrolador es una pequeña computadora dentro de un chip: tiene procesador, memoria y pines de entrada/salida, todo junto.

### ¿Por qué usamos el ESP32 y no un Arduino Uno clásico?

| Característica | Arduino Uno | ESP32 |
|---|---|---|
| WiFi | ❌ No tiene | ✅ Incluido |
| Velocidad | 16 MHz | 240 MHz |
| Memoria RAM | 2 KB | 520 KB |
| Pines para LEDs en paralelo | 1 | Hasta 8 (con RMT) |
| Tensión de señal | 5V | 3.3V |
| Precio aproximado | $3–5 USD | $4–8 USD |

### El periférico RMT del ESP32

El ESP32 tiene un módulo especial llamado **RMT** (Remote Control Transceiver). Originalmente fue diseñado para enviar señales de control remoto infrarrojo, pero es perfecto para controlar LEDs WS2812B porque:

- Puede generar pulsos de tiempo muy preciso (con resolución de nanosegundos)
- Tiene **8 canales independientes**, uno por pin
- No necesita que el programa principal esté pendiente: trabaja solo en segundo plano

> 💡 **Analogy**: Imaginate que vos sos el ESP32 y tenés 8 empleados (los canales RMT). En lugar de ir vos a atender a cada tira de LEDs una por una (lo cual sería lento), les das las instrucciones a los 8 empleados y ellos trabajan **al mismo tiempo**. Por eso podemos actualizar el panel mucho más rápido.

### Pines del ESP32 que usamos

```
                       ┌──────────────┐
                  3V3  │ 3V3      GND │  GND
                  EN   │              │
                  VP   │              │  GPIO23 → PIN_7 (tiras 7)
                  VN   │              │  GPIO22 → PIN_6 (tiras 6)
                       │              │  TX0
                       │              │  RX0
                       │              │  GPIO21 → PIN_5 (tiras 5)
                       │              │
                       │              │  GPIO19 → PIN_3 (tiras 3)
                       │              │  GPIO18 → PIN_2 (tiras 2)
                       │              │
                 GND   │              │
                 GPIO5 → PIN_2 ───────│
                 GPIO4 → PIN_1 ───────│
                 GPIO2 → PIN_0 ───────│
                       └──────────────┘
```

> ⚠️ **IMPORTANTE**: El ESP32 trabaja con señales de **3.3V**, pero los LEDs WS2812B necesitan señales de **5V** para funcionar de forma confiable. Esto lo resolvemos con un **conversor de nivel lógico** (ver Sección 6).

---

## 4. 💡 ¿Qué son los LEDs WS2812B?

### Un LED inteligente

Cada "píxel" de la tira WS2812B no es solo un LED: es un LED RGB (Rojo, Verde, Azul) con un **chip controlador integrado** dentro del mismo encapsulado. Esto significa que podés controlar el color de cada LED de forma independiente usando solo **un cable de datos**.

```
  Un LED WS2812B desde adentro:
  ┌────────────────────────────┐
  │  ┌──────┐  ┌──────────┐   │
  │  │CHIP  │  │ LED ROJO │   │
  │  │WS2811│  │ LED VERDE│   │
  │  │      │  │ LED AZUL │   │
  │  └──────┘  └──────────┘   │
  │  VCC  GND  DIN  DOUT       │
  └──┬────┬───┬───┬────────────┘
     5V  GND  ←datos  datos→
```

### ¿Cómo se comunican los LEDs?

Cada LED recibe datos de los 8 bits de Rojo, 8 de Verde y 8 de Azul = **24 bits por LED**. Cuando el primer LED recibe sus datos, pasa el resto de la cadena al siguiente LED (pin DOUT → DIN del siguiente). Así, enviando todos los datos de corrido, podés controlar toda la tira.

```
ESP32 → [LED 1] → [LED 2] → [LED 3] → ... → [LED 60]
         toma       toma       toma             toma
        primeros   siguientes  siguientes       últimos
        24 bits    24 bits     24 bits          24 bits
```

### Especificaciones eléctricas clave

| Parámetro | Valor |
|---|---|
| Tensión de alimentación (VCC) | 5V DC |
| Tensión de señal DIN (mínima) | 0.7 × VCC = **3.5V** |
| Corriente máxima por LED (blanco) | 60 mA |
| Corriente típica promedio (colores) | 20 mA |
| Longitud máxima sin reinyección de 5V | ~2 metros (~120 LEDs) |

> ⚠️ **¿Ves el problema?** El ESP32 genera señales de 3.3V, pero el WS2812B necesita mínimo 3.5V para reconocer un "1 lógico". Estamos 0.2V por debajo del límite. En condiciones ideales puede funcionar, pero en un entorno con ruido eléctrico (como un taller, un tablero, o cerca de variadores de velocidad), esos 0.2V de margen desaparecen y los LEDs se vuelven locos. **Por eso siempre usamos un conversor de nivel.**

---

## 5. 🔌 Circuito electrónico completo

### Diagrama general del sistema

```
                              ┌─────────────────────────────────────┐
                              │          FUENTE 5V / XX A            │
                              │  (ver sección 7 para el amperaje)    │
                              └──────┬────────────────┬─────────────┘
                                     │ +5V            │ GND
                                     │                │
          ┌──────────────────────────┤                │
          │  Condensador electrolítico│               │
          │  1000µF entre +5V y GND  │               │
          │  (uno por cada 2 tiras)  │               │
          └──────────────────────────┤                │
                                     │                │
                    ┌────────────────┘                │
                    │  +5V                            │
                    │             ┌───────────────────┘
                    │             │ GND COMÚN (muy importante!)
                    │             │
          ┌─────────┴─────────────┴──────────┐
          │         ESP32 DEV MODULE          │
          │  GPIO2  ──→ Conversor ──→ DIN T0  │
          │  GPIO4  ──→ Conversor ──→ DIN T1  │
          │  GPIO5  ──→ Conversor ──→ DIN T2  │
          │  GPIO18 ──→ Conversor ──→ DIN T3  │
          │  3V3 ←── (solo para el ESP32)     │
          │  GND ←── GND COMÚN                │
          └───────────────────────────────────┘
```

### Detalle del conversor de nivel lógico (74AHCT125)

El **74AHCT125** es un buffer/conversor que convierte la señal de 3.3V del ESP32 a 5V para los LEDs. Tiene 4 canales independientes, por lo que con 2 chips cubrimos los 8 pines.

```
    ESP32 (3.3V)         74AHCT125          Tira LED (5V)
    ─────────────        ─────────          ────────────
                         ┌───────┐
     GPIO2 ─────────────→│OE (GND│
                         │       │
     GPIO2 ─────3.3V────→│A      │──5V──→ DIN Tira 0
     GPIO4 ─────3.3V────→│A      │──5V──→ DIN Tira 1
     GPIO5 ─────3.3V────→│A      │──5V──→ DIN Tira 2
     GPIO18────3.3V────→│A      │──5V──→ DIN Tira 3
                         │       │
     +5V ───────────────→│VCC    │
     GND ───────────────→│GND    │
                         └───────┘
```

**Conexión del 74AHCT125 pin a pin:**

| Pin del 74AHCT125 | Conexión |
|---|---|
| Pin 1 (nOE) | GND (habilitar siempre) |
| Pin 2 (A) | GPIO del ESP32 |
| Pin 3 (Y) | DIN de la tira LED |
| Pin 14 (VCC) | +5V de la fuente |
| Pin 7 (GND) | GND común |

> 📌 **Consejo**: Colocá un capacitor cerámico de **100 nF** entre el pin VCC y GND del CI, lo más cerca posible del CI. Esto se llama "capacitor de desacoplo" y elimina los pulsos de ruido de alta frecuencia que puede generar el CI al cambiar de estado.

### Conexión de las tiras LED

```
  Fuente 5V
  ┌────────────────────────────────────────────────────────┐
  │ +5V          ┌── +5V ─┬── +5V ─┬── +5V ─┐            │
  │              │        │        │        │             │
  │     ┌────────┴──┐  ┌──┴────┐  ┌┴─────┐  ┌─────┐      │
  │     │  Tira 0   │  │Tira 1 │  │Tira 2│  │Tira3│      │
  │     │ DIN←DOUT→ │→ │DIN    │  │ DIN  │  │ DIN │      │
  │     └────────┬──┘  └──┬────┘  └──┬───┘  └──┬──┘      │
  │ GND          └─── GND ┴──── GND ─┴─── GND ─┘         │
  └────────────────────────────────────────────────────────┘

  NOTA: Con 4 pines en paralelo, cada pin controla TIRAS_POR_PIN tiras
        conectadas en serie. El DOUT de una tira va al DIN de la siguiente.
```

### Inyección de corriente

Cuando la corriente recorre los LEDs, la resistencia del cobre de la pista hace que la tensión caiga. Si tenés muchos LEDs en serie, los últimos reciben menos de 5V y se ven diferentes (más opacos o con colores incorrectos). La solución es **reinjectar +5V cada 2 metros (120 LEDs)**:

```
  +5V ──────┬──────────────────────┬──────────────────────┐
            │                      │                      │
          [LED 1..60]          [LED 61..120]          [LED 121..180]
            │                      │                      │
  GND ──────┴──────────────────────┴──────────────────────┘
            ↑                      ↑
         Inyección 1           Inyección 2
```

---

## 6. ⚡ ¿Por qué el entorno eléctrico es agresivo? Protecciones

### ¿Qué es el ruido eléctrico?

En entornos como talleres, escuelas técnicas o fábricas, hay mucha maquinaria eléctrica: motores, variadores de velocidad, soldadoras, luces fluorescentes. Todos ellos generan **interferencia electromagnética (EMI)**: pequeñas variaciones de tensión y corriente que viajan por el aire y por los cables, como un "murmullo eléctrico" invisible.

### ¿Cómo afecta al proyecto?

| Fuente de ruido | Efecto en el proyecto |
|---|---|
| Motor o variador de frecuencia | Pulsos de corriente que "pisan" la señal de datos → LEDs parpadean o cambian de color solos |
| Fuente conmutada barata | Rizado en el +5V → LEDs titilan a 50/100 Hz |
| Cables de datos largos sin blindaje | Los cables actúan como antenas y captan interferencia |
| GND mal conectado | Diferencia de potencial entre partes del circuito → errores de comunicación |

### Protecciones implementadas en el diseño

#### 1. Conversor de nivel lógico (74AHCT125 o SN74HCT245)

Ya explicado en la sección anterior. El margen extra de 1.7V (de 3.3V a 5V) es tu "colchón" contra el ruido. Una señal que en 3.3V podría confundirse, en 5V tiene mucho más margen para ser interpretada correctamente.

#### 2. Resistencia de serie en la línea de datos (330 Ω – 470 Ω)

```
  GPIO ──[330Ω]──→ DIN del LED
```

Esta resistencia limita la corriente en caso de cortocircuito accidental, y también **amortigua la señal**: elimina las "resonancias" que ocurren cuando la señal viaja por un cable largo. Sin la resistencia, los flancos de la señal digital "rebotan" (se llama *ringing*) y el LED puede interpretar un "1" como "0" o viceversa.

#### 3. Capacitores de filtrado en la alimentación

- **1000 µF electrolítico** en los bornes de entrada de la fuente: es un reservorio de energía. Cuando muchos LEDs se encienden al mismo tiempo, consumen un pico de corriente. El capacitor lo entrega instantáneamente y evita que la tensión caiga.

- **100 nF cerámico** junto a cada CI y cada conector de alimentación de tira: filtra el ruido de alta frecuencia (por encima de los kHz) que el capacitor grande no puede filtrar por su inercia.

```
  +5V ─────┬──────────────────────────────────
           │                      │
        [1000µF]               [100nF]
           │                      │
  GND ─────┴──────────────────────┴──────────
           ↑                      ↑
      Baja frecuencia       Alta frecuencia
      (picos de carga)      (ruido digital)
```

#### 4. GND común — ¡El más importante!

Un error muy frecuente en principiantes es conectar la fuente de 5V a los LEDs, pero olvidarse de conectar el GND de la fuente al GND del ESP32. Sin GND común, las señales de datos **no tienen referencia** y todo funciona mal.

```
  ╔══════════════════════════════╗
  ║  REGLA DE ORO DE ELECTRÓNICA ║
  ║                              ║
  ║  Todo GND debe estar         ║
  ║  unido a un solo punto.      ║
  ║  Sin GND común = caos.       ║
  ╚══════════════════════════════╝
```

#### 5. Cable de datos corto y separado de cables de potencia

- Máximo **50 cm** entre el conversor de nivel y el primer LED de cada tira
- Nunca hacer correr el cable de datos paralelo y pegado al cable de +5V/GND de alta corriente

#### 6. Ferrite bead (opcional pero recomendado)

Un ferrite bead es un pequeño componente que se pone en serie en el cable de datos. Actúa como un filtro pasabajos: deja pasar la señal útil (frecuencia de datos ~800 kHz) pero bloquea el ruido de frecuencias más altas.

---

## 7. 🔋 Cálculo del consumo eléctrico y fuente de alimentación

### ¿Cuánta corriente consume el panel?

Cada LED WS2812B consume como máximo **60 mA** cuando está en blanco al 100% de brillo (Rojo + Verde + Azul al máximo). En la práctica, con colores mezclados y brillo al 25% (valor `BRILLO_INICIAL = 60`), el consumo es mucho menor.

**Fórmula:**
```
Corriente total (máx.) = Cantidad de LEDs × 0.06 A
Corriente real         = Corriente máx × (Brillo / 255)
```

**Ejemplo con el panel por defecto** (60 LEDs/tira, 34 tiras):
```
LEDs totales = 60 × 34 = 2040 LEDs

Corriente máxima = 2040 × 0.06 A = 122.4 A  ← nunca vas a necesitar esto
Corriente con brillo 60/255 ≈ 23.5% = 28.8 A

Con colores promedio (no todo blanco): ÷ 3 ≈ 9.6 A

Potencia = 9.6 A × 5 V = 48 W
```

> 💡 El programa ya calcula esto en el arranque y lo muestra en el monitor serie.

### Elección de la fuente de alimentación

| Tamaño de panel | LEDs totales | Fuente recomendada |
|---|---|---|
| Pequeño (30 LEDs/m, 2 tiras) | 60 LEDs | 5V / 5A |
| Mediano (60 LEDs/m, 4 tiras) | 240 LEDs | 5V / 15A |
| Grande (60 LEDs/m, 8 tiras) | 480 LEDs | 5V / 30A |
| Completo (60 LEDs/m, 34 tiras) | 2040 LEDs | 5V / 40A (o dos fuentes en paralelo) |

> ⚠️ **Nunca alimentes las tiras LED desde el puerto USB del ESP32.** El USB solo entrega 500 mA – 1 A. Con más de 8 LEDs encendidos en blanco ya lo superás. Podés quemar el ESP32, la PC, o el cable USB.

### ¿Puedo usar dos fuentes en paralelo?

Sí, siempre que sean **de la misma tensión** (5V exactos). Conectá los +5V juntos y los GND juntos. No uses dos fuentes de distintos fabricantes sin un diodo de aislamiento en cada +5V para evitar que una "pelee" contra la otra.

---

## 8. 💻 Instalación del software

### Paso 1: Instalar Arduino IDE 2

1. Entrá a [https://www.arduino.cc/en/software](https://www.arduino.cc/en/software)
2. Descargá Arduino IDE 2.x para tu sistema operativo
3. Instalalo como cualquier programa

### Paso 2: Agregar soporte para ESP32

El Arduino IDE no trae soporte para ESP32 por defecto. Hay que agregarlo:

1. Abrí Arduino IDE
2. Andá a **Archivo → Preferencias**
3. En el campo "URLs adicionales de gestor de placas", pegá esta URL:
   ```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
4. Hacé clic en **OK**
5. Andá a **Herramientas → Placa → Gestor de placas...**
6. Buscá **"esp32"** y hacé clic en **Instalar** en el paquete de Espressif Systems

### Paso 3: Seleccionar la placa correcta

1. Conectá el ESP32 a la PC con el cable USB
2. Andá a **Herramientas → Placa → ESP32 Arduino → ESP32 Dev Module**
3. En **Herramientas → Puerto**, seleccioná el COM que apareció (ej: COM3 en Windows, /dev/ttyUSB0 en Linux)
4. Configurá estos parámetros:

| Parámetro | Valor |
|---|---|
| Partition Scheme | **Huge APP (3MB No OTA/1MB SPIFFS)** |
| CPU Frequency | **240 MHz** |
| Upload Speed | **921600** |
| Flash Size | 4MB (default) |

> ❓ **¿Por qué "Huge APP"?** Este proyecto usa muchas librerías (WiFi, WebSocket, JPEG, JSON, FastLED). El esquema de particiones por defecto le da 1MB al programa, que no alcanza. Con "Huge APP" le damos 3MB y el programa entra con holgura.

### Paso 4: Instalar las librerías necesarias

Andá a **Herramientas → Administrar bibliotecas** y buscá e instalá estas 4 librerías:

| Librería | Autor | Versión mínima |
|---|---|---|
| **FastLED** | Daniel Garcia | 3.6+ |
| **WebSockets** | Markus Sattler | 2.3+ |
| **ArduinoJson** | Benoit Blanchon | 6.x |
| **JPEGDecoder** | Bodmer | 1.8+ |

> 💡 **¿Cómo instalo una librería?** Escribí el nombre exacto en el buscador, hacé clic en la librería correcta y luego en **Instalar**. Si te pregunta si instalar dependencias, decí que sí.

### Paso 5: Abrir y compilar el proyecto

1. Descargá el archivo `panel_led_esp32.ino`
2. Abrilo con Arduino IDE (**Archivo → Abrir**)
3. Antes de compilar, editá las constantes de configuración (ver Sección 9)
4. Hacé clic en el botón de **✓ (Verificar)** para compilar sin subir
5. Si no hay errores, hacé clic en **→ (Subir)** para programar el ESP32

---

## 9. ⚙️ Configuración del código

Todas las opciones que podés cambiar están en la **Sección 2** del código, al principio del archivo. Cada una tiene una explicación.

### 9.1 Credenciales WiFi

```cpp
#define WIFI_SSID         "TU_RED_WIFI"    // Nombre de tu WiFi
#define WIFI_PASSWORD     "TU_CONTRASEÑA"  // Contraseña de tu WiFi
```

Reemplazá los valores por los de tu red.

### 9.2 Configuración de Telegram

```cpp
#define TELEGRAM_TOKEN    "TU_TOKEN_BOT"   // Lo creás con @BotFather (ver Sección 11)
#define TELEGRAM_CHAT_ID  123456789L       // Tu chat_id (ver Sección 11)
```

### 9.3 Configuración física del panel

```cpp
#define LEDS_POR_TIRA     60   // 30 si usás tiras de 30 LEDs/metro, 60 si usás 60 LEDs/metro
#define NUM_PINES         4    // Cuántos pines GPIO usás (1 a 8)
#define MODO_ENTRAZADO    0    // Cómo están conectadas las tiras (ver tabla abajo)
```

#### ¿Qué modo de entrazado elegir?

| Valor | Nombre | Descripción | Cuándo usarlo |
|---|---|---|---|
| `0` | **SECTOR** | Cada pin controla un bloque de tiras contiguas (pin 0 → tiras 0-8, pin 1 → tiras 9-17, etc.) | Construcción modular, más fácil de cablear |
| `1` | **INTERCALADO** | Las tiras se reparten alternando (pin 0 → tiras 0, 4, 8... pin 1 → tiras 1, 5, 9...) | Distribuye mejor el calor, más uniforme |
| `2` | **VERTICAL** | Cada pin controla columnas verticales | Casos especiales |

Para principiantes, usar `0` (SECTOR).

### 9.4 Número de tiras (relación 16:9)

Si `NUM_TIRAS_VERTICAL = 0`, el código calcula automáticamente cuántas tiras necesitás para que el panel tenga proporción 16:9:

```
NUM_TIRAS = LEDS_POR_TIRA × 9 / 16 = 60 × 9 / 16 ≈ 34 tiras
```

Con 4 pines y 34 tiras → **8 o 9 tiras por pin** (se redondea al múltiplo de NUM_PINES).

Si querés un tamaño diferente, ponelo manual:
```cpp
#define NUM_TIRAS_VERTICAL  32  // 32 tiras, 8 por pin (con 4 pines)
```

### 9.5 Pines GPIO

```cpp
#define PIN_0   2     // Primer pin de datos
#define PIN_1   4
#define PIN_2   5
#define PIN_3   18
// ... hasta PIN_7
```

Podés cambiarlos, pero tené en cuenta:
- Los pines **GPIO34, 35, 36, 39** son solo de entrada y **NO funcionan** como salida
- El pin **GPIO0** está conectado al botón de BOOT y puede causar problemas
- Evitá los pines que controlan el Flash interno: GPIO6, 7, 8, 9, 10, 11

### 9.6 Brillo y velocidades

```cpp
#define BRILLO_INICIAL    60    // 0 = apagado, 255 = máximo brillo
#define VELOCIDAD_SCROLL  40    // ms entre pasos de scroll. Más alto = más lento
#define DURACION_TRANSICION 500 // ms que dura cada efecto de transición
```

---

## 10. 📖 ¿Cómo funciona el código? (sección por sección)

El código está dividido en **14 secciones** claramente marcadas con comentarios. Acá explicamos cada una.

### 10.1 Secciones 1 y 2: Librerías y Configuración

Las **librerías** son paquetes de código ya hecho que importamos para no tener que programar todo desde cero:

```cpp
#include <FastLED.h>          // Maneja los LEDs WS2812B
#include <WiFi.h>             // Conecta el ESP32 a WiFi
#include <WiFiClientSecure.h> // Conecta usando HTTPS (seguro, con TLS)
#include <WebSocketsServer.h> // Crea un servidor WebSocket
#include <ArduinoJson.h>      // Lee/escribe datos en formato JSON
#include <JPEGDecoder.h>      // Decodifica imágenes JPEG
```

### 10.2 Sección 3: Cálculo automático de dimensiones 16:9

Este bloque de código calcula automáticamente cuántas tiras necesitás basándose en la relación de aspecto 16:9. Es código que se ejecuta en **tiempo de compilación** (antes de que el programa corra):

```
// Con 60 LEDs/tira:
// ANCHO_PANEL = 60 (los 60 LEDs de cada tira forman el eje horizontal)
// NUM_TIRAS   = 60 × 9 / 16 = 33.75 → se redondea a 36 (múltiplo de 4 pines)
// ALTO_PANEL  = 36
// LEDs totales = 60 × 36 = 2160
```

El `static_assert` es una comprobación en tiempo de compilación: si el número de tiras no es divisible exactamente por el número de pines, el compilador da un error explicativo antes de que puedas cargar un programa que no funcionaría bien.

### 10.3 Sección 4: Variables globales

#### Buffer de LEDs multi-pin

FastLED necesita un array de tipo `CRGB` separado por cada pin. `CRGB` es una estructura de 3 bytes (Rojo, Verde, Azul) que representa el color de un LED:

```cpp
CRGB ledsPin0[LEDS_POR_TIRA * TIRAS_POR_PIN]; // Array del pin 0
CRGB ledsPin1[LEDS_POR_TIRA * TIRAS_POR_PIN]; // Array del pin 1
// ...
```

Si tenés 4 pines, 9 tiras por pin y 60 LEDs por tira:
```
Tamaño de cada array = 60 × 9 = 540 LEDs × 3 bytes = 1620 bytes por pin
Total para 4 pines = 6480 bytes ≈ 6.3 KB de RAM
```

#### Buffer de imagen

Además de los buffers de FastLED, hay un buffer intermedio `bufferImagen[]` que almacena la imagen completa. ¿Por qué dos buffers? Porque FastLED organiza los LEDs según el cableado físico (serpenteante, multi-pin), mientras que nosotros queremos trabajar con coordenadas (x, y) como en una pantalla. La función `mapearPixel()` hace la traducción.

#### Enum de modos

Un `enum` es una lista de constantes con nombre:

```cpp
enum ModoVisualizacion {
    MODO_IMAGEN_ESTATICA = 0, // Muestra una imagen fija
    MODO_TEXTO_SCROLL,        // Texto en movimiento
    MODO_STREAMING_WS,        // Video por WiFi
    MODO_APAGADO              // Panel apagado
};
```

Usar un enum hace el código mucho más legible: `modoActual = MODO_TEXTO_SCROLL` se entiende solo.

### 10.4 Sección 5: Fuente bitmap 5×3

Para poder mostrar texto, el programa incluye una **fuente tipográfica** guardada como datos binarios. Cada carácter ocupa 5 filas × 3 columnas (15 bits).

Cada fila del carácter es un número donde cada bit representa un LED encendido o apagado:

```
Letra 'A':
Fila 0: 0b111  →  ■ ■ ■
Fila 1: 0b101  →  ■ · ■
Fila 2: 0b111  →  ■ ■ ■
Fila 3: 0b101  →  ■ · ■
Fila 4: 0b101  →  ■ · ■
```

Los datos se guardan en **PROGMEM** (memoria Flash del microcontrolador en lugar de la RAM). Esto es importante porque los WS2812B ya consumen bastante RAM con sus buffers, y la fuente para los 95 caracteres ASCII ocuparía 475 bytes de RAM que no tenemos de sobra.

```cpp
const uint8_t FUENTE_5X3[][5] PROGMEM = { ... };
//                             ↑
//                   Esta palabra indica: guardar en Flash
```

### 10.5 Sección 6: Mapeo multi-pin con entrazado serpenteante

Esta es una de las partes más complejas del código. El problema es este:

- Vos pensás en una pantalla con coordenadas (x=0..59, y=0..33)
- Pero los LEDs están físicamente conectados en zigzag a través de múltiples pines
- La función `mapearPixel(x, y, pin)` hace la traducción

```
Cómo están conectados físicamente los LEDs (modo SECTOR, 2 pines):

PIN 0 → [LED0..LED59] → [LED60..LED119] → ... (tiras 0 a N/2)
PIN 1 → [LED0..LED59] → [LED60..LED119] → ... (tiras N/2 a N)

En la pantalla:
(x=5, y=3) → tira 3 → PIN 0 → posición en array: 3*60 + 5 = 185
(x=5, y=20) → tira 20 → PIN 1 → posición en array: (20-17)*60 + 5 = 185

¡El mismo índice 185 en arrays distintos!
```

El **zigzag serpenteante** invierte la dirección en filas pares/impares:
```
Fila par:   LED[0] ← LED[1] ← LED[2] ... LED[59]   (DIN al principio)
Fila impar: LED[0] → LED[1] → LED[2] ... LED[59]   (DOUT al principio)
```

Entonces para una fila impar, el píxel en columna x está en la posición `(columnas - 1 - x)` del array.

### 10.6 Sección 7: Inicialización de FastLED multi-pin

La función `inicializarFastLED()` usa un **switch con fallthrough intencional**. Normalmente en un switch cada caso termina con `break`, pero acá se omite para que los casos se "encadenen":

```cpp
switch (NUM_PINES) {
    case 4:
        FastLED.addLeds<WS2812B, PIN_3, GRB>(ledsPin3, tamaño); // registra pin 3
        __attribute__((fallthrough)); // ← sin break: cae al caso 3
    case 3:
        FastLED.addLeds<WS2812B, PIN_2, GRB>(ledsPin2, tamaño); // registra pin 2
        __attribute__((fallthrough)); // ← cae al caso 2
    case 2:
        FastLED.addLeds<WS2812B, PIN_1, GRB>(ledsPin1, tamaño);
        __attribute__((fallthrough));
    case 1:
        FastLED.addLeds<WS2812B, PIN_0, GRB>(ledsPin0, tamaño);
        break; // ← ahora sí termina
}
```

Esto evita repetir código: si `NUM_PINES = 4`, se registran los 4 pines sin necesidad de 4 if/else separados.

El parámetro `GRB` le dice a FastLED que este modelo de LED tiene el orden de colores Verde-Rojo-Azul (en lugar del más común RGB). Los WS2812B usan GRB.

### 10.7 Sección 8: WiFi, Telegram y WebSocket

#### Conexión WiFi

```cpp
void conectarWiFi() {
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500); // esperar medio segundo y volver a chequear
    }
}
```

Esta función es **bloqueante**: el programa no avanza hasta que haya WiFi. Esto está bien en el `setup()` porque si no hay WiFi no podemos hacer nada de todos modos.

#### Comunicación con Telegram (HTTPS)

Telegram usa **HTTPS**: HTTP con encriptación TLS/SSL. Esto significa que antes de enviar datos, el ESP32 y el servidor de Telegram hacen un "apretón de manos" criptográfico para verificar que nadie está espiando la conexión. Por eso usamos `WiFiClientSecure` en lugar del `WiFiClient` común.

```cpp
clienteHTTPS.setInsecure(); // acepta cualquier certificado (simplificado)
// En producción usar: clienteHTTPS.setCACert(certificado_telegram);
```

#### Polling de mensajes

El bot de Telegram funciona por **polling**: el ESP32 le pregunta a Telegram cada segundo "¿Llegó algún mensaje nuevo?". Esto se hace con la función `getUpdates` de la API de Telegram.

```
ESP32 cada 1000ms:
    GET https://api.telegram.org/botTOKEN/getUpdates?offset=ULTIMO_ID+1

Telegram responde con un JSON:
    { "ok": true, "result": [ { "message": { "text": "/color FF0000" } } ] }

El ESP32 parsea el JSON con ArduinoJson y ejecuta el comando.
```

El `offset` es importante: le dice a Telegram "ya procesé hasta el mensaje N, dame solo los más nuevos". Sin él, recibiríamos todos los mensajes desde el principio cada vez.

### 10.8 Sección 9: Texto scroll

El algoritmo de scroll es simple pero elegante:

1. Cada `VELOCIDAD_SCROLL` milisegundos, se borran solo las filas donde está el texto
2. Se re-dibuja el texto desplazado 1 píxel a la izquierda (`posScrollX--`)
3. Cuando el texto salió completamente por la izquierda, `posScrollX` vuelve a `ANCHO_PANEL` (empezando desde la derecha)
4. Se llama `FastLED.show()` para enviar el frame al hardware

La función usa `millis()` en lugar de `delay()`. **¿Por qué es esto importante?**

```
CON delay():                     CON millis():
loop() {                         loop() {
  moverTexto();    ← tarda 40ms    if (millis() - ultimo >= 40) {
  // El resto del                     moverTexto();
  // programa                         ultimo = millis();
  // está BLOQUEADO                 }
  // 40ms!                          // WebSocket y Telegram
}                                   // siguen funcionando!
                                 }
```

Con `millis()`, el programa puede atender el WebSocket y Telegram mientras el texto se desplaza.

### 10.9 Sección 10: Imágenes JPEG

#### ¿Por qué JPEG y no BMP o PNG?

JPEG permite comprimir fotos de cientos de KB a unos pocos KB. Dado que el ESP32 tiene solo 520 KB de RAM y Telegram limita las fotos enviadas por bots, JPEG es el único formato práctico.

#### Escalado por vecino más cercano

La imagen JPEG puede tener cualquier resolución (p.ej. 640×480), pero el panel tiene su propia resolución (60×34). El código escala la imagen usando el algoritmo del **vecino más cercano**:

```
Para cada píxel del panel (xPanel, yPanel):
    xOriginal = xPanel × anchuraJPEG / anchuraPanel
    yOriginal = yPanel × alturaJPEG  / alturaPanel
    color = imagen[yOriginal][xOriginal]  ← "vecino más cercano"
```

Es el algoritmo de escalado más simple: no hace interpolación, pero es muy rápido en un microcontrolador.

#### RGB565 → RGB888

El decodificador JPEG devuelve colores en formato **RGB565** (16 bits), que es el estándar de pantallas TFT baratas. Pero nuestros LEDs necesitan **RGB888** (24 bits, 8 por canal). La conversión expande los bits:

```cpp
uint8_t r = (rgb565 >> 11) << 3;          // 5 bits de rojo  → 8 bits
uint8_t g = ((rgb565 >> 5) & 0x3F) << 2;  // 6 bits de verde → 8 bits
uint8_t b = (rgb565 & 0x1F) << 3;          // 5 bits de azul  → 8 bits
```

### 10.10 Sección 11: Transiciones animadas

Las transiciones mezclan el frame anterior (`bufferTransicion`) con el nuevo (`bufferImagen`) usando un valor `progreso` que va de 0.0 a 1.0:

#### FADE (crossfade)
```
colorFinal = colorAnterior + (colorNuevo - colorAnterior) × progreso
           = lerp(colorAnterior, colorNuevo, progreso)
```
En el inicio (progreso=0) se ve el frame viejo; al final (progreso=1) se ve el nuevo.

#### WIPE (barrido)
```
colCorte = ANCHO_PANEL × progreso
Si columna < colCorte → mostrar frame nuevo
Si columna >= colCorte → mostrar frame viejo
```
Una "cortina" que avanza de un lado al otro.

#### DISSOLVE (disolución)
Cada píxel cambia al nuevo frame en un momento aleatorio pero **determinístico**: basado en un hash de su posición (x, y). Esto da un efecto de "disolución" sin necesidad de guardar un estado aleatorio por píxel.

### 10.11 Sección 12: Servidor WebSocket

**WebSocket** es un protocolo de comunicación que, a diferencia de HTTP normal (donde el cliente pregunta y el servidor responde), permite que ambos lados envíen datos en cualquier momento. Es ideal para streaming de video en tiempo real.

El ESP32 actúa como servidor WebSocket en el **puerto 81**. Una aplicación en la PC puede conectarse y enviar frames del panel como bytes crudos (ANCHO × ALTO × 3 bytes por frame).

```
PC (cliente WS):                    ESP32 (servidor WS):
  conectar("ws://IP_ESP32:81")  →   acepta conexión
  enviar(bytes del frame 1)     →   recibe, muestra en LEDs
  enviar(bytes del frame 2)     →   recibe, muestra en LEDs
  ...                               ...
```

### 10.12 Sección 13: Procesamiento de comandos Telegram

El código reconoce estos comandos:

| Comando | Ejemplo | Acción |
|---|---|---|
| `/texto` | `/texto Bienvenidos!` | Activa scroll con ese texto |
| `/color` | `/color FF0000` | Pinta todo de rojo |
| `/brillo` | `/brillo 128` | Ajusta el brillo a la mitad |
| `/transicion` | `/transicion fade` | Cambia el efecto de transición |
| `/imagen` | `/imagen` | Activa modo imagen |
| `/apagar` | `/apagar` | Apaga el panel |
| `/estado` | `/estado` | Muestra modo actual, brillo y resolución |
| `/ayuda` | `/ayuda` | Lista todos los comandos |
| (foto) | (foto JPEG) | Muestra la foto en el panel |

### 10.13 Sección 14: Setup y Loop principal

#### setup()

Se ejecuta **una sola vez** al encender el ESP32. En orden:

1. Inicia comunicación Serial (para ver mensajes de debug en la PC)
2. Imprime la configuración calculada
3. Inicializa FastLED (registra todos los pines)
4. Conecta al WiFi
5. Inicia el servidor WebSocket
6. Configura el cliente HTTPS para Telegram
7. Muestra el texto de bienvenida "Panel LED listo!" con scroll

#### loop()

Se ejecuta **continuamente** (miles de veces por segundo). Cada iteración hace:

```
1. servidorWS.loop()     → Procesa cualquier dato entrante de WebSocket
2. consultarTelegram()   → Si pasó 1 segundo, pregunta si hay mensajes nuevos
3. actualizarTransicion() → Si hay un efecto de transición, avanza un frame
4. switch(modoActual):
     TEXTO_SCROLL → avanza el texto si pasó VELOCIDAD_SCROLL ms
     IMAGEN       → no hace nada (la imagen ya está en los LEDs)
     STREAMING_WS → no hace nada (los frames llegan por WebSocket)
     APAGADO      → no hace nada
```

---

## 11. 📱 Cómo construir el bot de Telegram

### Paso 1: Crear el bot con @BotFather

1. Abrí Telegram y buscá `@BotFather`
2. Escribí `/newbot`
3. Cuando te pida el nombre, escribí algo como `Panel LED Escuela`
4. Cuando te pida el username, debe terminar en "bot", por ejemplo `PanelLedETxx_bot`
5. BotFather te va a dar un **token** que se ve así:
   ```
   1234567890:AAHdqTcvCH1vGWJxfSeofSoGy4YXNsV5vM
   ```
6. Copiá ese token y pegalo en el código donde dice `TELEGRAM_TOKEN`

### Paso 2: Obtener tu Chat ID

1. Buscá `@userinfobot` en Telegram
2. Escribile cualquier mensaje
3. Te va a responder con tu **ID de usuario** (un número como `987654321`)
4. Copiá ese número y pegalo en el código donde dice `TELEGRAM_CHAT_ID`

> 🔒 **Seguridad**: El `CHAT_ID` es tu escudo. El código verifica que solo mensajes de ese ID sean procesados. Sin esta verificación, cualquier persona que encuentre tu bot podría controlar el panel.

### Paso 3: Probar

1. Cargá el código en el ESP32
2. Abrí el **Monitor Serie** (Ctrl+Shift+M) y verificá que aparezca:
   ```
   Conectado! IP: 192.168.X.X
   WebSocket iniciado en puerto 81
   === Setup completo ===
   ```
3. En Telegram, mandá `/ayuda` a tu bot
4. El bot debería responder con la lista de comandos

---

## 12. 📡 Comandos disponibles y cómo usarlos

### Texto con scroll

```
/texto Hola Mundo!
/texto Buenos Aires, Argentina
```

Podés cambiar el color del texto editando `colorTexto` en el código, o en el futuro con un comando `/colorTexto`.

### Color sólido

```
/color FF0000    → Rojo
/color 00FF00    → Verde
/color 0000FF    → Azul
/color FFFFFF    → Blanco
/color FF6600    → Naranja
/color 000000    → Apagar (negro)
```

Los colores son en formato **hexadecimal RGB**: RRGGBB. Podés usar cualquier selector de colores online para obtener el valor hex.

### Brillo

```
/brillo 30     → Muy tenue (bueno para ahorro de energía)
/brillo 100    → Brillo medio
/brillo 200    → Brillante
/brillo 255    → Máximo (¡cuidado con el consumo!)
```

### Transiciones

```
/transicion fade         → Crossfade suave
/transicion wipe_izq     → Barrido de derecha a izquierda
/transicion wipe_der     → Barrido de izquierda a derecha
/transicion wipe_arriba  → Barrido de abajo hacia arriba
/transicion wipe_abajo   → Barrido de arriba hacia abajo
/transicion dissolve     → Disolución aleatoria
```

### Enviar una foto

Simplemente enviá una foto al bot desde Telegram. El sistema automáticamente:
1. Descarga la foto
2. La decodifica de JPEG
3. La escala al tamaño del panel
4. La muestra con la transición seleccionada

> 💡 Las fotos con alto contraste y colores saturados se ven mejor. Fotos muy oscuras o muy detalladas pierden mucho al escalar a la baja resolución del panel.

---

## 13. 💻 Control por WebSocket (streaming de video)

Podés enviar video en tiempo real al panel desde tu PC usando cualquier lenguaje que soporte WebSocket. El protocolo es simple:

### Formato del frame

Cada frame es un array de bytes con el siguiente formato:
```
[R₀G₀B₀] [R₁G₁B₁] ... [Rₙ₋₁Gₙ₋₁Bₙ₋₁]

Donde n = ANCHO_PANEL × ALTO_PANEL
Total bytes por frame = 60 × 34 × 3 = 6120 bytes (con panel por defecto)
```

### Ejemplo con Python

```python
import websocket
import numpy as np

PANEL_W = 60  # ANCHO_PANEL
PANEL_H = 34  # ALTO_PANEL
IP_ESP32 = "192.168.X.X"  # IP que mostró el monitor serie

ws = websocket.create_connection(f"ws://{IP_ESP32}:81")

# Crear un frame con un degradado de rojo
frame = np.zeros((PANEL_H, PANEL_W, 3), dtype=np.uint8)
for x in range(PANEL_W):
    frame[:, x, 0] = int(x / PANEL_W * 255)  # canal rojo varía con x

ws.send_binary(frame.tobytes())
ws.close()
```

---

## 14. 🔧 Resolución de problemas frecuentes

### "Los LEDs no encienden"

1. Verificá que el GND del ESP32 y el GND de la fuente estén conectados
2. Medí con el multímetro que el +5V llegue a los conectores de las tiras
3. Verificá en el Monitor Serie que FastLED se inicializó sin errores
4. Asegurate de que el conversor de nivel esté correctamente alimentado con 5V

### "Los LEDs encienden pero con colores incorrectos"

- El parámetro `GRB` en `FastLED.addLeds<WS2812B, PIN_0, GRB>` es correcto para WS2812B. Si usás APA102 u otro modelo, puede ser diferente.
- Verificá que todos los GND estén unidos (panel, ESP32, fuente)

### "Los LEDs parpadean o se vuelven locos aleatoriamente"

- **Síntoma clásico de ruido eléctrico o tensión incorrecta**
- Agregá la resistencia de 330 Ω en la línea de datos
- Asegurate de usar el conversor de nivel lógico 74AHCT125
- Acortá el cable entre el conversor y el primer LED
- Agregá capacitores de desacoplo (100 nF) junto a cada tira

### "El ESP32 no conecta al WiFi"

- Verificá que el SSID y la contraseña sean correctos (es case-sensitive)
- El ESP32 solo conecta a redes WiFi de **2.4 GHz** (no 5 GHz)
- Asegurate de que el router esté encendido y al alcance
- Si estás en una red de escuela con filtros MAC, puede que necesites registrar la MAC del ESP32

### "El bot de Telegram no responde"

- Verificá que el token y el chat_id estén correctamente copiados
- El ESP32 debe tener conexión a internet, no solo a la red local
- Revisá el Monitor Serie para ver si hay errores de conexión HTTPS
- Comprobá que `TELEGRAM_CHAT_ID` sea exactamente tu ID (incluyendo el signo positivo/negativo)

### "Error de compilación: 'NUM_TIRAS debe ser múltiplo exacto de NUM_PINES'"

Esto ocurre cuando el cálculo automático 16:9 genera un número de tiras que no es divisible exactamente por el número de pines. Solución: define manualmente el número de tiras:
```cpp
#define NUM_TIRAS_VERTICAL  32  // debe ser múltiplo de NUM_PINES
```

### "La imagen se ve distorsionada o girada"

Verificá el `MODO_ENTRAZADO` (0, 1 o 2) coincide con la forma en que soldaste las tiras. También confirmá que el primer LED de cada tira (el que tiene el DIN) apunte en la dirección correcta.

---

## 15. 📚 Glosario de términos técnicos

| Término | Definición |
|---|---|
| **Microcontrolador** | Pequeña computadora integrada en un chip. Tiene procesador, memoria y pines de I/O en un solo componente |
| **GPIO** | General Purpose Input/Output. Pines del microcontrolador que se pueden configurar como entrada o salida digital |
| **RMT** | Remote Control Transceiver. Módulo del ESP32 que genera señales de tiempo muy preciso, usado para controlar LEDs WS2812B |
| **WS2812B** | LED RGB inteligente con controlador integrado. Se comunica con un protocolo de 1 cable a 800 Kbps |
| **PWM** | Pulse Width Modulation. Técnica para simular un voltaje analógico usando pulsos digitales. FastLED usa PWM para controlar el brillo |
| **GND** | Ground / tierra. El punto de referencia de 0V en el circuito. Todos los GND deben estar conectados entre sí |
| **PROGMEM** | Directiva de Arduino/avr-gcc para guardar datos en la memoria Flash (programa) en lugar de la RAM |
| **FastLED** | Librería de Arduino para controlar LEDs inteligentes (WS2812B, APA102, etc.) |
| **millis()** | Función de Arduino que devuelve los milisegundos transcurridos desde que el programa arrancó |
| **WebSocket** | Protocolo de comunicación bidireccional en tiempo real sobre TCP, usado para streaming |
| **HTTPS** | HTTP con encriptación TLS. Telegram requiere HTTPS para todas sus comunicaciones |
| **JSON** | JavaScript Object Notation. Formato de texto para intercambiar datos estructurados |
| **JPEG** | Formato de imagen comprimido con pérdida. Ideal para fotografías |
| **RGB888** | Formato de color con 8 bits para Rojo, Verde y Azul (24 bits en total) |
| **RGB565** | Formato de color con 5-6-5 bits para R-G-B (16 bits en total). Usado por pantallas TFT |
| **Buffer** | Zona de memoria RAM usada para almacenar datos temporalmente |
| **enum** | Enumeración en C/C++. Lista de constantes con nombre para mejorar la legibilidad del código |
| **EMI** | Electromagnetic Interference. Interferencia electromagnética generada por equipos eléctricos |
| **Polling** | Técnica donde el cliente pregunta periódicamente al servidor si hay datos nuevos |
| **Conversor de nivel lógico** | CI que convierte señales entre distintas tensiones (ej: 3.3V ↔ 5V) |
| **Capacitor de desacoplo** | Capacitor colocado cerca de un CI para filtrar el ruido en su alimentación |
| **Resistencia de serie** | Resistencia colocada en un cable de señal para limitar corriente y amortiguar resonancias |
| **Serpenteante / zigzag** | Patrón de conexión de tiras LED donde filas pares e impares van en direcciones opuestas |
| **Inyección de corriente** | Técnica de conectar el +5V en múltiples puntos de la tira para compensar la caída de tensión |
| **Vecino más cercano** | Algoritmo simple de escalado de imágenes que usa el pixel más cercano sin interpolación |
| **static_assert** | Verificación en tiempo de compilación en C++. Si falla, el compilador da un error con el mensaje especificado |
| **Fallthrough** | En un switch de C/C++, la ejecución "cae" al siguiente caso si no hay `break` |
| **TLS/SSL** | Transport Layer Security. Protocolo de encriptación usado en HTTPS |
| **Chat ID** | Número único que identifica a un usuario o grupo en Telegram |
| **BotFather** | Bot oficial de Telegram para crear y gestionar otros bots |

---

## 🏁 ¡Listo para construir!

Ahora tenés todo lo que necesitás:

1. ✅ Entendés qué hace cada componente electrónico
2. ✅ Sabés por qué son necesarias las protecciones contra ruido
3. ✅ Podés calcular la fuente de alimentación necesaria
4. ✅ Instalaste el software y las librerías
5. ✅ Configuraste el bot de Telegram
6. ✅ Entendés la lógica del código sección por sección

> **Consejo final:** Empezá con una sola tira de LEDs y un solo pin antes de conectar el panel completo. Verificá que el texto scroll funcione, que los colores sean correctos, y que Telegram responda. Cuando todo eso funcione, agregá el resto de las tiras de a poco.

¡Mucha suerte en el proyecto! 🚀

---

*Documentación generada para uso educativo en Escuelas Técnicas de la Provincia de Buenos Aires.*
*Este proyecto forma parte del aprendizaje de Electrónica y Sistemas Embebidos.*
