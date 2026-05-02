/**
 * @file panel_led_esp32.ino
 * @brief Sistema completo de control para panel LED WS2812B con ESP32.
 *
 * @details
 * Controla un panel LED formado por tiras WS2812B organizadas en una
 * grilla con relación de aspecto 16:9. Soporta múltiples pines GPIO
 * en paralelo mediante los canales RMT nativos del ESP32 para maximizar
 * los FPS. Incluye bot de Telegram, servidor WebSocket, scroll de texto,
 * visualización de imágenes JPEG y efectos de transición animados.
 *
 * CONSIDERACIONES ELÉCTRICAS:
 *   - Cada tira de 1m a 60 LEDs/m consume hasta 60 × 0.06A = 3.6A @ 5V.
 *   - Inyectar 5V cada 2-3 tiras (máx ~2m sin reinyección).
 *   - Usar conversor de nivel lógico 3.3V→5V (SN74HCT245 o 74AHCT125)
 *     entre los pines GPIO del ESP32 y el DIN de cada tira.
 *   - GND COMÚN entre la fuente de alimentación y el ESP32.
 *   - NUNCA alimentar las tiras LED desde el conector USB del ESP32.
 *   - Amperaje total estimado: NUM_LEDS_TOTAL × 0.06A × (BRILLO_INICIAL/255)
 *
 * @author  Generado para uso educativo/profesional
 * @version 1.0
 */

// ============================================================
// SECCIÓN 1: INCLUSIONES DE LIBRERÍAS
// ============================================================
#include <Arduino.h>          // API base de Arduino
#include <FastLED.h>          // Control de LEDs WS2812B
#include <WiFi.h>             // Conexión WiFi del ESP32
#include <WiFiClientSecure.h> // Cliente HTTPS (Telegram usa TLS)
#include <WebSocketsServer.h> // Servidor WebSocket (puerto 81)
#include <ArduinoJson.h>      // Parseo de respuestas JSON de Telegram
#include <JPEGDecoder.h>      // Decodificación de imágenes JPEG

// ============================================================
// SECCIÓN 2: CONFIGURACIÓN Y CONSTANTES EDITABLES
// ============================================================

// ---- Credenciales WiFi ----
#define WIFI_SSID         "TU_RED_WIFI"       ///< Nombre de la red WiFi
#define WIFI_PASSWORD     "TU_CONTRASEÑA"     ///< Contraseña del WiFi

// ---- Configuración de Telegram ----
#define TELEGRAM_TOKEN    "TU_TOKEN_BOT"      ///< Token del bot de Telegram
#define TELEGRAM_CHAT_ID  123456789L          ///< chat_id autorizado (long)
#define TELEGRAM_HOST     "api.telegram.org"  ///< Servidor de Telegram
#define TELEGRAM_PORT     443                 ///< Puerto HTTPS

// ---- Configuración física del panel ----
#define LEDS_POR_TIRA     60    ///< LEDs por metro de tira: 30 o 60
#define NUM_PINES         4     ///< Pines GPIO activos (1-8)
#define MODO_ENTRAZADO    0     ///< 0=SECTOR, 1=INTERCALADO, 2=VERTICAL

/**
 * @def NUM_TIRAS_VERTICAL
 * @brief Número de tiras (columnas). Si es 0, se calcula automáticamente
 *        para mantener relación 16:9. Si es manual, debe ser múltiplo
 *        exacto de NUM_PINES para distribución sin fracciones.
 */
#define NUM_TIRAS_VERTICAL  0   ///< 0 = cálculo automático 16:9

// ---- Pines GPIO para datos de las tiras ----
// Deben ser pines con soporte RMT en el ESP32 (casi todos los GPIO)
#define PIN_0   2
#define PIN_1   4
#define PIN_2   5
#define PIN_3   18
#define PIN_4   19
#define PIN_5   21
#define PIN_6   22
#define PIN_7   23

// ---- Configuración de visualización ----
#define BRILLO_INICIAL    60    ///< Brillo inicial (0-255)
#define VELOCIDAD_SCROLL  40    ///< ms entre cada paso del scroll de texto
#define DURACION_TRANSICION 500 ///< ms que dura cada transición animada

// ---- WebSocket ----
#define WEBSOCKET_PUERTO  81    ///< Puerto del servidor WebSocket

// ---- Intervalo de consulta a Telegram ----
#define INTERVALO_TELEGRAM 1000 ///< ms entre consultas al servidor de Telegram

// ============================================================
// SECCIÓN 3: CÁLCULO AUTOMÁTICO DE DIMENSIONES (16:9)
// ============================================================

/**
 * @brief Calcula el número de tiras necesarias para relación 16:9.
 * @details Con tiras de 1m el eje largo es vertical (número de tiras
 *          = altura en cm / 1m = número de tiras). La relación 16:9
 *          implica: ancho/alto = 16/9. Ancho = LEDS_POR_TIRA px (columnas
 *          de LEDs), alto = NUM_TIRAS px (filas de tiras).
 *          NUM_TIRAS = LEDS_POR_TIRA * 9 / 16, redondeado al múltiplo
 *          de NUM_PINES más cercano por arriba.
 */
// Paso 1: calcular valor base 16:9
static const int _BASE_TIRAS = (LEDS_POR_TIRA * 9 + 15) / 16; // redondeo hacia arriba

// Paso 2: ajustar al múltiplo exacto de NUM_PINES
// Si el usuario fijó NUM_TIRAS_VERTICAL > 0, se usa ese valor directamente
static const int ANCHO_PANEL = LEDS_POR_TIRA;  ///< Píxeles horizontales (LEDs/tira)
static const int NUM_TIRAS   =                 ///< Número total de tiras (filas)
    (NUM_TIRAS_VERTICAL > 0)
        ? NUM_TIRAS_VERTICAL
        : ((_BASE_TIRAS + NUM_PINES - 1) / NUM_PINES) * NUM_PINES;

static const int ALTO_PANEL  = NUM_TIRAS;       ///< Píxeles verticales (= tiras)
static const int TIRAS_POR_PIN = NUM_TIRAS / NUM_PINES; ///< Tiras asignadas a cada pin

static const int NUM_LEDS_TOTAL = ANCHO_PANEL * ALTO_PANEL; ///< LEDs totales del panel

// Verificación en tiempo de compilación
static_assert(NUM_TIRAS % NUM_PINES == 0,
    "NUM_TIRAS debe ser múltiplo exacto de NUM_PINES. "
    "Ajusta NUM_TIRAS_VERTICAL manualmente.");

// ============================================================
// SECCIÓN 4: VARIABLES GLOBALES
// ============================================================

// ---- Buffer de LEDs para FastLED (un array por pin) ----
// FastLED requiere un array CRGB por pin para operar en paralelo.
CRGB ledsPin0[LEDS_POR_TIRA * ((NUM_PINES > 0) ? TIRAS_POR_PIN : 1)];
CRGB ledsPin1[LEDS_POR_TIRA * ((NUM_PINES > 1) ? TIRAS_POR_PIN : 1)];
CRGB ledsPin2[LEDS_POR_TIRA * ((NUM_PINES > 2) ? TIRAS_POR_PIN : 1)];
CRGB ledsPin3[LEDS_POR_TIRA * ((NUM_PINES > 3) ? TIRAS_POR_PIN : 1)];
CRGB ledsPin4[LEDS_POR_TIRA * ((NUM_PINES > 4) ? TIRAS_POR_PIN : 1)];
CRGB ledsPin5[LEDS_POR_TIRA * ((NUM_PINES > 5) ? TIRAS_POR_PIN : 1)];
CRGB ledsPin6[LEDS_POR_TIRA * ((NUM_PINES > 6) ? TIRAS_POR_PIN : 1)];
CRGB ledsPin7[LEDS_POR_TIRA * ((NUM_PINES > 7) ? TIRAS_POR_PIN : 1)];

/** @brief Punteros a los buffers de cada pin para acceso indexado. */
CRGB* bufferPines[8] = {
    ledsPin0, ledsPin1, ledsPin2, ledsPin3,
    ledsPin4, ledsPin5, ledsPin6, ledsPin7
};

/**
 * @brief Buffer global del frame actual en formato RGB888.
 * @details Almacena la imagen completa: ANCHO_PANEL × ALTO_PANEL × 3 bytes.
 *          Índice: (y * ANCHO_PANEL + x) * 3 + canal (0=R, 1=G, 2=B).
 */
uint8_t bufferImagen[ANCHO_PANEL * ALTO_PANEL * 3];

/** @brief Buffer temporal para la transición (frame anterior). */
uint8_t bufferTransicion[ANCHO_PANEL * ALTO_PANEL * 3];

// ---- Modo de visualización ----
/** @brief Modos de visualización disponibles para el panel. */
enum ModoVisualizacion {
    MODO_IMAGEN_ESTATICA = 0, ///< Muestra una imagen fija
    MODO_TEXTO_SCROLL,        ///< Desplaza texto de derecha a izquierda
    MODO_STREAMING_WS,        ///< Recibe frames por WebSocket
    MODO_APAGADO              ///< Panel completamente apagado
};

ModoVisualizacion modoActual    = MODO_APAGADO; ///< Modo en curso
ModoVisualizacion modoAnterior  = MODO_APAGADO; ///< Modo antes de la transición

// ---- Efecto de transición ----
/** @brief Tipos de transición animada entre modos. */
enum TipoTransicion {
    TRANS_FADE = 0,   ///< Crossfade lineal de opacidad
    TRANS_WIPE_IZQ,   ///< Barrido de derecha a izquierda
    TRANS_WIPE_DER,   ///< Barrido de izquierda a derecha
    TRANS_WIPE_ARRIBA,///< Barrido de abajo hacia arriba
    TRANS_WIPE_ABAJO, ///< Barrido de arriba hacia abajo
    TRANS_DISSOLVE    ///< Disolución aleatoria por hash de posición
};

TipoTransicion tipoTransActual = TRANS_FADE; ///< Transición seleccionada
bool           enTransicion    = false;      ///< ¿Hay transición en curso?
uint32_t       tiempoInicioTransicion = 0;   ///< millis() cuando empezó la transición

// ---- Configuración de texto scroll ----
char     textoScroll[256]   = "Hola desde ESP32!"; ///< Texto a desplazar
CRGB     colorTexto         = CRGB::White;          ///< Color del texto
int      posScrollX         = ANCHO_PANEL;          ///< Posición X actual del scroll
uint32_t ultimoScrollMs     = 0;                    ///< millis() del último paso

// ---- Color de fondo sólido (comando /color) ----
CRGB colorFondo = CRGB::Black; ///< Color de relleno para comandos de color

// ---- Brillo actual ----
uint8_t brilloActual = BRILLO_INICIAL; ///< Brillo global (0-255)

// ---- Telegram: control de offset para getUpdates ----
long telegramOffset = 0;   ///< Offset del último update procesado
uint32_t ultimoTelegramMs = 0; ///< millis() de la última consulta

// ---- Objetos de red ----
WiFiClientSecure clienteHTTPS; ///< Cliente seguro para Telegram (TLS)
WebSocketsServer servidorWS(WEBSOCKET_PUERTO); ///< Servidor WebSocket

// ============================================================
// SECCIÓN 5: FUENTE BITMAP 5×3 EN PROGMEM
// ============================================================

/**
 * @brief Fuente bitmap de 5 filas × 3 columnas para caracteres ASCII 32-126.
 * @details Cada byte codifica una fila de 3 bits (bits 2..0 = col 0..2).
 *          Se almacena en PROGMEM para no consumir RAM dinámica.
 *          Espacio entre caracteres: 1 columna vacía.
 */
const uint8_t FUENTE_5X3[][5] PROGMEM = {
    // ASCII 32 = espacio
    {0b000, 0b000, 0b000, 0b000, 0b000}, // ' '
    // ASCII 33 = !
    {0b010, 0b010, 0b010, 0b000, 0b010}, // '!'
    // ASCII 34 = "
    {0b101, 0b101, 0b000, 0b000, 0b000}, // '"'
    // ASCII 35 = #
    {0b101, 0b111, 0b101, 0b111, 0b101}, // '#'
    // ASCII 36 = $
    {0b011, 0b110, 0b011, 0b110, 0b010}, // '$'
    // ASCII 37 = %
    {0b100, 0b001, 0b010, 0b100, 0b001}, // '%'
    // ASCII 38 = &
    {0b010, 0b101, 0b110, 0b101, 0b110}, // '&'
    // ASCII 39 = '
    {0b010, 0b010, 0b000, 0b000, 0b000}, // '\''
    // ASCII 40 = (
    {0b001, 0b010, 0b010, 0b010, 0b001}, // '('
    // ASCII 41 = )
    {0b100, 0b010, 0b010, 0b010, 0b100}, // ')'
    // ASCII 42 = *
    {0b101, 0b010, 0b111, 0b010, 0b101}, // '*'
    // ASCII 43 = +
    {0b000, 0b010, 0b111, 0b010, 0b000}, // '+'
    // ASCII 44 = ,
    {0b000, 0b000, 0b000, 0b010, 0b100}, // ','
    // ASCII 45 = -
    {0b000, 0b000, 0b111, 0b000, 0b000}, // '-'
    // ASCII 46 = .
    {0b000, 0b000, 0b000, 0b000, 0b010}, // '.'
    // ASCII 47 = /
    {0b001, 0b001, 0b010, 0b100, 0b100}, // '/'
    // ASCII 48-57 = 0-9
    {0b111, 0b101, 0b101, 0b101, 0b111}, // '0'
    {0b010, 0b110, 0b010, 0b010, 0b111}, // '1'
    {0b111, 0b001, 0b111, 0b100, 0b111}, // '2'
    {0b111, 0b001, 0b111, 0b001, 0b111}, // '3'
    {0b101, 0b101, 0b111, 0b001, 0b001}, // '4'
    {0b111, 0b100, 0b111, 0b001, 0b111}, // '5'
    {0b111, 0b100, 0b111, 0b101, 0b111}, // '6'
    {0b111, 0b001, 0b001, 0b001, 0b001}, // '7'
    {0b111, 0b101, 0b111, 0b101, 0b111}, // '8'
    {0b111, 0b101, 0b111, 0b001, 0b111}, // '9'
    // ASCII 58 = :
    {0b000, 0b010, 0b000, 0b010, 0b000}, // ':'
    // ASCII 59 = ;
    {0b000, 0b010, 0b000, 0b010, 0b100}, // ';'
    // ASCII 60 = <
    {0b001, 0b010, 0b100, 0b010, 0b001}, // '<'
    // ASCII 61 = =
    {0b000, 0b111, 0b000, 0b111, 0b000}, // '='
    // ASCII 62 = >
    {0b100, 0b010, 0b001, 0b010, 0b100}, // '>'
    // ASCII 63 = ?
    {0b111, 0b001, 0b011, 0b000, 0b010}, // '?'
    // ASCII 64 = @
    {0b111, 0b101, 0b111, 0b100, 0b111}, // '@' (simplificado)
    // ASCII 65-90 = A-Z
    {0b111, 0b101, 0b111, 0b101, 0b101}, // 'A'
    {0b110, 0b101, 0b110, 0b101, 0b110}, // 'B'
    {0b111, 0b100, 0b100, 0b100, 0b111}, // 'C'
    {0b110, 0b101, 0b101, 0b101, 0b110}, // 'D'
    {0b111, 0b100, 0b111, 0b100, 0b111}, // 'E'
    {0b111, 0b100, 0b111, 0b100, 0b100}, // 'F'
    {0b111, 0b100, 0b101, 0b101, 0b111}, // 'G'
    {0b101, 0b101, 0b111, 0b101, 0b101}, // 'H'
    {0b111, 0b010, 0b010, 0b010, 0b111}, // 'I'
    {0b001, 0b001, 0b001, 0b101, 0b111}, // 'J'
    {0b101, 0b110, 0b100, 0b110, 0b101}, // 'K'
    {0b100, 0b100, 0b100, 0b100, 0b111}, // 'L'
    {0b101, 0b111, 0b101, 0b101, 0b101}, // 'M'
    {0b101, 0b111, 0b111, 0b101, 0b101}, // 'N'
    {0b111, 0b101, 0b101, 0b101, 0b111}, // 'O'
    {0b111, 0b101, 0b111, 0b100, 0b100}, // 'P'
    {0b111, 0b101, 0b101, 0b111, 0b011}, // 'Q'
    {0b111, 0b101, 0b110, 0b101, 0b101}, // 'R'
    {0b111, 0b100, 0b111, 0b001, 0b111}, // 'S'
    {0b111, 0b010, 0b010, 0b010, 0b010}, // 'T'
    {0b101, 0b101, 0b101, 0b101, 0b111}, // 'U'
    {0b101, 0b101, 0b101, 0b101, 0b010}, // 'V'
    {0b101, 0b101, 0b101, 0b111, 0b101}, // 'W'
    {0b101, 0b101, 0b010, 0b101, 0b101}, // 'X'
    {0b101, 0b101, 0b010, 0b010, 0b010}, // 'Y'
    {0b111, 0b001, 0b010, 0b100, 0b111}, // 'Z'
    // ASCII 91 = [
    {0b110, 0b100, 0b100, 0b100, 0b110}, // '['
    // ASCII 92 = backslash
    {0b100, 0b100, 0b010, 0b001, 0b001}, // '\'
    // ASCII 93 = ]
    {0b011, 0b001, 0b001, 0b001, 0b011}, // ']'
    // ASCII 94 = ^
    {0b010, 0b101, 0b000, 0b000, 0b000}, // '^'
    // ASCII 95 = _
    {0b000, 0b000, 0b000, 0b000, 0b111}, // '_'
    // ASCII 96 = `
    {0b010, 0b001, 0b000, 0b000, 0b000}, // '`'
    // ASCII 97-122 = a-z
    {0b000, 0b111, 0b101, 0b101, 0b111}, // 'a'
    {0b100, 0b110, 0b101, 0b101, 0b110}, // 'b'
    {0b000, 0b111, 0b100, 0b100, 0b111}, // 'c'
    {0b001, 0b011, 0b101, 0b101, 0b011}, // 'd'
    {0b000, 0b111, 0b111, 0b100, 0b111}, // 'e'
    {0b011, 0b010, 0b111, 0b010, 0b010}, // 'f'
    {0b000, 0b111, 0b101, 0b011, 0b111}, // 'g'
    {0b100, 0b110, 0b101, 0b101, 0b101}, // 'h'
    {0b010, 0b000, 0b010, 0b010, 0b010}, // 'i'
    {0b001, 0b000, 0b001, 0b101, 0b111}, // 'j'
    {0b100, 0b101, 0b110, 0b101, 0b101}, // 'k'
    {0b010, 0b010, 0b010, 0b010, 0b001}, // 'l'
    {0b000, 0b111, 0b111, 0b101, 0b101}, // 'm'
    {0b000, 0b110, 0b101, 0b101, 0b101}, // 'n'
    {0b000, 0b111, 0b101, 0b101, 0b111}, // 'o'
    {0b000, 0b111, 0b101, 0b111, 0b100}, // 'p'
    {0b000, 0b011, 0b101, 0b011, 0b001}, // 'q'
    {0b000, 0b110, 0b101, 0b100, 0b100}, // 'r'
    {0b000, 0b111, 0b110, 0b001, 0b111}, // 's'
    {0b010, 0b111, 0b010, 0b010, 0b001}, // 't'
    {0b000, 0b101, 0b101, 0b101, 0b111}, // 'u'
    {0b000, 0b101, 0b101, 0b101, 0b010}, // 'v'
    {0b000, 0b101, 0b101, 0b111, 0b111}, // 'w'
    {0b000, 0b101, 0b010, 0b101, 0b101}, // 'x'
    {0b000, 0b101, 0b101, 0b011, 0b111}, // 'y'
    {0b000, 0b111, 0b010, 0b100, 0b111}, // 'z'
    // ASCII 123-126
    {0b011, 0b010, 0b110, 0b010, 0b011}, // '{'
    {0b010, 0b010, 0b010, 0b010, 0b010}, // '|'
    {0b110, 0b010, 0b011, 0b010, 0b110}, // '}'
    {0b010, 0b101, 0b000, 0b000, 0b000}, // '~'
};

// Ancho y alto de cada glifo de la fuente
static const int FUENTE_ANCHO = 3; ///< Columnas por carácter (sin separador)
static const int FUENTE_ALTO  = 5; ///< Filas por carácter
static const int FUENTE_SEP   = 1; ///< Columnas de separación entre caracteres

// ============================================================
// SECCIÓN 5b: MAPA DE PINES (arreglo auxiliar)
// ============================================================

/** @brief Números de pin GPIO para cada índice (hasta 8). */
static const uint8_t PINES_GPIO[8] = {
    PIN_0, PIN_1, PIN_2, PIN_3,
    PIN_4, PIN_5, PIN_6, PIN_7
};

// ============================================================
// SECCIÓN 6: FUNCIONES DE MAPEO MULTI-PIN CON ENTRAZADO
// ============================================================

/**
 * @brief Determina a qué pin pertenece una tira dada según el modo de entrazado.
 * @param fila Índice de la tira (0 = tira superior).
 * @return Índice del pin (0 a NUM_PINES-1).
 * @details
 *   - SECTOR (0):     pin = fila / TIRAS_POR_PIN
 *   - INTERCALADO (1): pin = fila % NUM_PINES
 *   - VERTICAL (2):   pin basado en el bloque de columnas (usa col como parámetro
 *                     extra; aquí devuelve el pin de la fila para el caso sin columna)
 */
int obtenerPinDeFila(int fila) {
    switch (MODO_ENTRAZADO) {
        case 0: // SECTOR: bloques horizontales consecutivos
            return fila / TIRAS_POR_PIN;
        case 1: // INTERCALADO: round-robin
            return fila % NUM_PINES;
        default:
            return fila / TIRAS_POR_PIN; // fallback a SECTOR
    }
}

/**
 * @brief Determina a qué pin pertenece una columna dada (modo VERTICAL).
 * @param col Índice de columna (0 = izquierda).
 * @return Índice del pin (0 a NUM_PINES-1).
 * @details En VERTICAL, el panel se divide en bloques verticales de columnas.
 *          Columnas por pin = ANCHO_PANEL / NUM_PINES.
 */
int obtenerPinDeColumna(int col) {
    // Columnas por pin calculadas en tiempo de ejecución
    int colsPorPin = ANCHO_PANEL / NUM_PINES;
    return col / colsPorPin;
}

/**
 * @brief Convierte coordenadas (x,y) del panel a índice en el buffer del pin.
 * @param x      Columna del panel (0 = izquierda).
 * @param y      Fila del panel (0 = arriba).
 * @param pinOut [salida] Pin al que pertenece este píxel.
 * @return Índice dentro del array bufferPines[pinOut].
 * @details Implementa el mapeo serpenteante (zigzag) dentro de cada sub-panel:
 *          - Filas pares: de izquierda a derecha.
 *          - Filas impares: de derecha a izquierda.
 *          También maneja los tres modos de entrazado (SECTOR, INTERCALADO, VERTICAL).
 */
int mapearPixel(int x, int y, int &pinOut) {
    int indiceLed = 0; // índice resultante en el buffer del pin

    if (MODO_ENTRAZADO == 2) {
        // ---- MODO VERTICAL: bloques de columnas ----
        int colsPorPin  = ANCHO_PANEL / NUM_PINES; // columnas asignadas a cada pin
        pinOut          = obtenerPinDeColumna(x);  // pin según columna
        int colLocal    = x % colsPorPin;           // columna dentro del sub-panel
        // Serpenteante vertical: la tira 'y' puede ir de arriba a abajo o al revés
        // Para simplificar: dentro del bloque vertical, filas pares → col local izq→der
        // Filas impares → col local der→izq
        if (y % 2 == 0) {
            // Fila par: de izquierda a derecha
            indiceLed = y * colsPorPin + colLocal;
        } else {
            // Fila impar: de derecha a izquierda
            indiceLed = y * colsPorPin + (colsPorPin - 1 - colLocal);
        }
    } else {
        // ---- MODOS SECTOR e INTERCALADO ----
        // El sub-panel de este pin contiene TIRAS_POR_PIN tiras completas.
        // Cada tira tiene ANCHO_PANEL LEDs.

        pinOut = obtenerPinDeFila(y); // pin según la fila

        // Calcular la fila LOCAL dentro del sub-panel del pin
        int filaLocal;
        if (MODO_ENTRAZADO == 0) {
            // SECTOR: las tiras son consecutivas (0,1,2,... en el pin)
            filaLocal = y - (pinOut * TIRAS_POR_PIN);
        } else {
            // INTERCALADO: las tiras están intercaladas (0,NUM_PINES,2*NUM_PINES,...)
            filaLocal = y / NUM_PINES;
        }

        // Aplicar serpenteante: filas pares izq→der, impares der→izq
        if (filaLocal % 2 == 0) {
            // Fila local par: dirección normal (izquierda a derecha)
            indiceLed = filaLocal * ANCHO_PANEL + x;
        } else {
            // Fila local impar: dirección inversa (derecha a izquierda)
            indiceLed = filaLocal * ANCHO_PANEL + (ANCHO_PANEL - 1 - x);
        }
    }
    return indiceLed;
}

/**
 * @brief Establece el color de un píxel en las coordenadas (x,y) del panel.
 * @param x     Columna (0 = izquierda, ANCHO_PANEL-1 = derecha).
 * @param y     Fila (0 = arriba, ALTO_PANEL-1 = abajo).
 * @param color Color CRGB a asignar.
 * @details Función de alto nivel que oculta completamente el entrazado multi-pin.
 *          Usa mapearPixel() para determinar pin e índice, luego escribe en el
 *          buffer correspondiente. También actualiza bufferImagen[].
 */
void establecerPixel(int x, int y, CRGB color) {
    // Validar que las coordenadas estén dentro del panel
    if (x < 0 || x >= ANCHO_PANEL || y < 0 || y >= ALTO_PANEL) return;

    int pin, indice;
    indice = mapearPixel(x, y, pin); // obtener destino en el buffer del pin

    // Solo escribir si el pin es válido
    if (pin >= 0 && pin < NUM_PINES) {
        bufferPines[pin][indice] = color; // escribir en el buffer físico del pin
    }

    // Actualizar también el buffer de imagen RGB888 para consistencia
    int idxBuf = (y * ANCHO_PANEL + x) * 3;
    bufferImagen[idxBuf + 0] = color.r;
    bufferImagen[idxBuf + 1] = color.g;
    bufferImagen[idxBuf + 2] = color.b;
}

/**
 * @brief Obtiene el color actual del píxel en las coordenadas (x,y).
 * @param x Columna.
 * @param y Fila.
 * @return CRGB del LED en esa posición, leído del bufferImagen.
 */
CRGB obtenerPixel(int x, int y) {
    // Leer del buffer de imagen (fuente de verdad)
    if (x < 0 || x >= ANCHO_PANEL || y < 0 || y >= ALTO_PANEL) return CRGB::Black;
    int idxBuf = (y * ANCHO_PANEL + x) * 3;
    return CRGB(bufferImagen[idxBuf], bufferImagen[idxBuf + 1], bufferImagen[idxBuf + 2]);
}

/**
 * @brief Copia bufferImagen[] completo hacia los buffers de cada pin.
 * @details Se debe llamar después de modificar bufferImagen directamente
 *          (por ejemplo al recibir un frame por WebSocket) para que los
 *          cambios se reflejen en el hardware al próximo FastLED.show().
 */
void volcarBufferALeds() {
    for (int y = 0; y < ALTO_PANEL; y++) {
        for (int x = 0; x < ANCHO_PANEL; x++) {
            int idxBuf = (y * ANCHO_PANEL + x) * 3;
            CRGB color(bufferImagen[idxBuf], bufferImagen[idxBuf+1], bufferImagen[idxBuf+2]);
            int pin, indice;
            indice = mapearPixel(x, y, pin);
            if (pin >= 0 && pin < NUM_PINES) {
                bufferPines[pin][indice] = color;
            }
        }
    }
}

/**
 * @brief Apaga todos los LEDs (pone en negro todos los buffers).
 */
void apagarTodos() {
    for (int p = 0; p < NUM_PINES; p++) {
        // Limpiar cada buffer de pin completamente
        int cantLeds = TIRAS_POR_PIN * ANCHO_PANEL;
        for (int i = 0; i < cantLeds; i++) {
            bufferPines[p][i] = CRGB::Black;
        }
    }
    memset(bufferImagen, 0, sizeof(bufferImagen)); // limpiar también el buffer global
}

// ============================================================
// SECCIÓN 7: INICIALIZACIÓN DE FASTLED MULTI-PIN
// ============================================================

/**
 * @brief Inicializa FastLED para todos los pines activos usando canales RMT.
 * @details Usa switch/fallthrough para registrar dinámicamente de 1 a 8 pines
 *          sin duplicar código. Cada llamada addLeds<>() ocupa un canal RMT
 *          diferente del ESP32, permitiendo la salida paralela de datos.
 *          IMPORTANTE: los casos van desde NUM_PINES hacia abajo (fallthrough).
 */
void inicializarFastLED() {
    // Aplicar brillo global antes de la primera actualización
    FastLED.setBrightness(brilloActual);

    // switch con fallthrough: se ejecutan todos los casos desde NUM_PINES hasta 1
    switch (NUM_PINES) {
        case 8:
            // Registrar pin 7 con su buffer
            FastLED.addLeds<WS2812B, PIN_7, GRB>(ledsPin7, TIRAS_POR_PIN * ANCHO_PANEL);
            // [fallthrough intencional]
            __attribute__((fallthrough));
        case 7:
            FastLED.addLeds<WS2812B, PIN_6, GRB>(ledsPin6, TIRAS_POR_PIN * ANCHO_PANEL);
            __attribute__((fallthrough));
        case 6:
            FastLED.addLeds<WS2812B, PIN_5, GRB>(ledsPin5, TIRAS_POR_PIN * ANCHO_PANEL);
            __attribute__((fallthrough));
        case 5:
            FastLED.addLeds<WS2812B, PIN_4, GRB>(ledsPin4, TIRAS_POR_PIN * ANCHO_PANEL);
            __attribute__((fallthrough));
        case 4:
            FastLED.addLeds<WS2812B, PIN_3, GRB>(ledsPin3, TIRAS_POR_PIN * ANCHO_PANEL);
            __attribute__((fallthrough));
        case 3:
            FastLED.addLeds<WS2812B, PIN_2, GRB>(ledsPin2, TIRAS_POR_PIN * ANCHO_PANEL);
            __attribute__((fallthrough));
        case 2:
            FastLED.addLeds<WS2812B, PIN_1, GRB>(ledsPin1, TIRAS_POR_PIN * ANCHO_PANEL);
            __attribute__((fallthrough));
        case 1:
        default:
            // Mínimo: siempre registrar el pin 0
            FastLED.addLeds<WS2812B, PIN_0, GRB>(ledsPin0, TIRAS_POR_PIN * ANCHO_PANEL);
            break;
    }

    // Limpiar todos los LEDs en el arranque
    apagarTodos();
    FastLED.show(); // enviar estado inicial (todos apagados) al hardware
}

// ============================================================
// SECCIÓN 8: CONEXIÓN WIFI, TELEGRAM Y WEBSOCKET
// ============================================================

/**
 * @brief Conecta el ESP32 a la red WiFi configurada.
 * @details Intenta la conexión de forma bloqueante durante el setup().
 *          Muestra el progreso por Serial cada 500ms.
 */
void conectarWiFi() {
    Serial.print("Conectando a WiFi: ");
    Serial.println(WIFI_SSID);

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD); // iniciar conexión WiFi

    // Esperar hasta obtener IP (bloqueante solo en setup)
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println();
    Serial.print("Conectado! IP: ");
    Serial.println(WiFi.localIP());
}

/**
 * @brief Envía un mensaje de texto a través del bot de Telegram.
 * @param mensaje Texto a enviar al chat autorizado.
 * @details Construye la petición HTTPS GET a la API de Telegram y
 *          espera la respuesta. No parsea la respuesta de envío.
 */
void enviarMensajeTelegram(const char* mensaje) {
    // Usar cliente seguro para TLS (Telegram requiere HTTPS)
    WiFiClientSecure cliente;
    cliente.setInsecure(); // en producción usar setCACert() con certificado real

    // Construir la URL de la petición (usando char[] para evitar String en crítico)
    char url[512];
    snprintf(url, sizeof(url),
        "/bot%s/sendMessage?chat_id=%ld&text=%s",
        TELEGRAM_TOKEN, TELEGRAM_CHAT_ID, mensaje);

    if (!cliente.connect(TELEGRAM_HOST, TELEGRAM_PORT)) {
        Serial.println("Error: no se pudo conectar a Telegram");
        return;
    }

    // Enviar cabeceras HTTP mínimas
    cliente.print("GET ");
    cliente.print(url);
    cliente.println(" HTTP/1.1");
    cliente.print("Host: ");
    cliente.println(TELEGRAM_HOST);
    cliente.println("Connection: close");
    cliente.println(); // línea en blanco = fin de cabeceras

    // Esperar respuesta (timeout implícito)
    delay(300);
    cliente.stop(); // liberar la conexión TCP
}

/**
 * @brief Descarga el archivo de una foto de Telegram dado su file_id.
 * @param fileId    Identificador del archivo en Telegram.
 * @param buffer    Buffer de destino para los datos JPEG.
 * @param tamMax    Tamaño máximo del buffer en bytes.
 * @return Número de bytes descargados, o 0 si falló.
 * @details Primero llama a getFile para obtener la ruta del archivo,
 *          luego descarga el archivo como bytes crudos.
 */
size_t descargarFotoTelegram(const char* fileId, uint8_t* buffer, size_t tamMax) {
    WiFiClientSecure cliente;
    cliente.setInsecure();

    // Paso 1: obtener la ruta del archivo con getFile
    char urlGetFile[256];
    snprintf(urlGetFile, sizeof(urlGetFile),
        "/bot%s/getFile?file_id=%s", TELEGRAM_TOKEN, fileId);

    if (!cliente.connect(TELEGRAM_HOST, TELEGRAM_PORT)) return 0;

    cliente.print("GET ");
    cliente.print(urlGetFile);
    cliente.println(" HTTP/1.1");
    cliente.print("Host: "); cliente.println(TELEGRAM_HOST);
    cliente.println("Connection: close");
    cliente.println();

    // Leer cuerpo de la respuesta (JSON con file_path)
    char respuesta[1024] = {0};
    size_t leidos = 0;
    bool cabecera = true;
    uint32_t tInicio = millis();

    while (cliente.connected() && millis() - tInicio < 5000) {
        if (cliente.available()) {
            char c = cliente.read();
            if (cabecera) {
                // Detectar fin de cabeceras HTTP (línea vacía)
                static char ultimos4[4] = {0};
                memmove(ultimos4, ultimos4+1, 3);
                ultimos4[3] = c;
                if (memcmp(ultimos4, "\r\n\r\n", 4) == 0) cabecera = false;
            } else {
                if (leidos < sizeof(respuesta)-1) respuesta[leidos++] = c;
            }
        }
    }
    cliente.stop();

    // Parsear JSON para extraer file_path
    StaticJsonDocument<512> doc;
    if (deserializeJson(doc, respuesta) != DeserializationError::Ok) return 0;

    const char* filePath = doc["result"]["file_path"];
    if (!filePath) return 0;

    // Paso 2: descargar el archivo real desde api.telegram.org/file/
    char urlDescarga[256];
    snprintf(urlDescarga, sizeof(urlDescarga),
        "/file/bot%s/%s", TELEGRAM_TOKEN, filePath);

    WiFiClientSecure cliente2;
    cliente2.setInsecure();
    if (!cliente2.connect(TELEGRAM_HOST, TELEGRAM_PORT)) return 0;

    cliente2.print("GET ");
    cliente2.print(urlDescarga);
    cliente2.println(" HTTP/1.1");
    cliente2.print("Host: "); cliente2.println(TELEGRAM_HOST);
    cliente2.println("Connection: close");
    cliente2.println();

    // Saltar cabeceras HTTP
    cabecera = true;
    size_t bytesRecibidos = 0;
    tInicio = millis();
    char ultimos4[4] = {0};

    while (cliente2.connected() && millis() - tInicio < 15000) {
        if (cliente2.available()) {
            uint8_t b = cliente2.read();
            if (cabecera) {
                memmove(ultimos4, ultimos4+1, 3);
                ultimos4[3] = (char)b;
                if (memcmp(ultimos4, "\r\n\r\n", 4) == 0) cabecera = false;
            } else {
                if (bytesRecibidos < tamMax) buffer[bytesRecibidos++] = b;
            }
        }
    }
    cliente2.stop();
    return bytesRecibidos;
}

// ============================================================
// SECCIÓN 9: RENDERIZADO DE TEXTO (FUENTE BITMAP)
// ============================================================

/**
 * @brief Obtiene el ancho en píxeles de un carácter de la fuente 5×3.
 * @param c Carácter ASCII (32-126).
 * @return Ancho en columnas del carácter (siempre FUENTE_ANCHO).
 */
int anchoCaracter(char c) {
    // Todos los caracteres de esta fuente tienen el mismo ancho fijo
    (void)c; // suprimir advertencia de parámetro no usado
    return FUENTE_ANCHO;
}

/**
 * @brief Calcula el ancho total en píxeles de una cadena con la fuente 5×3.
 * @param texto Cadena de caracteres terminada en nulo.
 * @return Número de columnas que ocupa el texto completo.
 */
int anchoCadena(const char* texto) {
    int ancho = 0;
    for (const char* p = texto; *p; p++) {
        ancho += FUENTE_ANCHO + FUENTE_SEP; // ancho del glifo + separador
    }
    // Quitar el separador extra del último carácter
    if (ancho > 0) ancho -= FUENTE_SEP;
    return ancho;
}

/**
 * @brief Dibuja un único carácter en el panel en la posición dada.
 * @param c     Carácter ASCII a dibujar.
 * @param xBase Columna izquierda donde empieza el carácter.
 * @param yBase Fila superior donde empieza el carácter.
 * @param color Color del carácter.
 * @details Lee los datos de la fuente desde PROGMEM para ahorrar RAM.
 *          Los píxeles apagados (bit=0) no se tocan (fondo transparente).
 */
void dibujarCaracter(char c, int xBase, int yBase, CRGB color) {
    // Validar rango ASCII de la fuente (32-126)
    if (c < 32 || c > 126) return;
    int idxFuente = c - 32; // índice en el array FUENTE_5X3

    for (int fila = 0; fila < FUENTE_ALTO; fila++) {
        // Leer la fila del bitmap desde PROGMEM (Flash)
        uint8_t lineaBitmap = pgm_read_byte(&FUENTE_5X3[idxFuente][fila]);
        for (int col = 0; col < FUENTE_ANCHO; col++) {
            // Extraer el bit correspondiente a esta columna (bit 2 = col 0)
            bool pixelActivo = (lineaBitmap >> (FUENTE_ANCHO - 1 - col)) & 0x01;
            if (pixelActivo) {
                // Solo dibujar píxeles encendidos (fondo transparente)
                establecerPixel(xBase + col, yBase + fila, color);
            }
        }
    }
}

/**
 * @brief Actualiza el scroll de texto en el panel (modo TEXTO_SCROLL).
 * @details Se llama periódicamente en el loop. Cada vez que pasa VELOCIDAD_SCROLL ms,
 *          borra el panel, re-dibuja el texto desplazado un píxel a la izquierda,
 *          y reinicia cuando el texto sale completamente por la izquierda.
 *          El texto está centrado verticalmente en el panel.
 */
void actualizarTextoScroll() {
    // Verificar si pasó suficiente tiempo para el siguiente paso del scroll
    if (millis() - ultimoScrollMs < VELOCIDAD_SCROLL) return;
    ultimoScrollMs = millis(); // resetear temporizador

    // Calcular posición vertical centrada
    int yInicio = (ALTO_PANEL - FUENTE_ALTO) / 2; // centrar verticalmente

    // Limpiar solo las filas del texto (eficiencia)
    for (int y = yInicio; y < yInicio + FUENTE_ALTO && y < ALTO_PANEL; y++) {
        for (int x = 0; x < ANCHO_PANEL; x++) {
            establecerPixel(x, y, CRGB::Black); // borrar fila
        }
    }

    // Dibujar cada carácter del texto en su posición X desplazada
    int xCursor = posScrollX; // posición X del primer carácter
    for (const char* p = textoScroll; *p; p++) {
        // Solo dibujar si el carácter es visible en el panel
        if (xCursor + FUENTE_ANCHO >= 0 && xCursor < ANCHO_PANEL) {
            dibujarCaracter(*p, xCursor, yInicio, colorTexto);
        }
        xCursor += FUENTE_ANCHO + FUENTE_SEP; // avanzar al siguiente carácter
        if (xCursor > ANCHO_PANEL) break;     // optimización: dejar de procesar
    }

    // Avanzar posición del scroll un píxel a la izquierda
    posScrollX--;

    // Calcular ancho total del texto para detectar cuando reiniciar
    int anchoTotal = anchoCadena(textoScroll);
    if (posScrollX < -anchoTotal) {
        posScrollX = ANCHO_PANEL; // reiniciar desde la derecha
    }

    FastLED.show(); // enviar frame actualizado al hardware
}

// ============================================================
// SECCIÓN 10: PROCESAMIENTO DE IMÁGENES JPEG
// ============================================================

// Buffer estático para almacenar la imagen JPEG descargada
static uint8_t bufferJpeg[50000]; ///< 50KB max para imagen JPEG de Telegram

/**
 * @brief Callback del decodificador JPEG: recibe bloques MCU y los escala al panel.
 * @param pDraw Puntero a la estructura JpegDraw con los datos del bloque actual.
 * @return true para continuar la decodificación.
 * @details Se llama por JpegDec para cada bloque decodificado (típicamente 8x8 o 16x16).
 *          Implementa escalado por vecino más cercano desde la resolución original
 *          hasta ANCHO_PANEL × ALTO_PANEL.
 */
bool cbkJpeg(JPEGDRAW* pDraw) {
    // Dimensiones originales del JPEG (se obtienen antes de esta llamada)
    // Accedemos al tamaño completo mediante JpegDec global
    int jpegAncho = JpegDec.width;
    int jpegAlto  = JpegDec.height;

    for (int yBloque = 0; yBloque < pDraw->iHeight; yBloque++) {
        int ySrc = pDraw->y + yBloque; // fila en la imagen original

        // Mapear fila de la imagen al panel (vecino más cercano)
        int yPanel = (ySrc * ALTO_PANEL) / jpegAlto;
        if (yPanel >= ALTO_PANEL) continue; // fuera del panel

        for (int xBloque = 0; xBloque < pDraw->iWidth; xBloque++) {
            int xSrc = pDraw->x + xBloque; // columna en la imagen original

            // Mapear columna al panel
            int xPanel = (xSrc * ANCHO_PANEL) / jpegAncho;
            if (xPanel >= ANCHO_PANEL) continue;

            // Obtener color del píxel (formato RGB565 de JpegDec)
            uint16_t rgb565 = pDraw->pPixels[yBloque * pDraw->iWidth + xBloque];

            // Convertir RGB565 a RGB888
            uint8_t r = (rgb565 >> 11) << 3;          // 5 bits → 8 bits
            uint8_t g = ((rgb565 >> 5) & 0x3F) << 2;  // 6 bits → 8 bits
            uint8_t b = (rgb565 & 0x1F) << 3;          // 5 bits → 8 bits

            // Escribir en el panel
            establecerPixel(xPanel, yPanel, CRGB(r, g, b));
        }
    }
    return true; // continuar decodificación
}

/**
 * @brief Decodifica un buffer JPEG y lo muestra escalado en el panel.
 * @param datos    Puntero al buffer con los datos JPEG crudos.
 * @param longitud Tamaño del buffer en bytes.
 * @details Usa la librería JPEGDecoder para decodificar el JPEG en memoria.
 *          Cada bloque MCU decodificado se escala al panel mediante cbkJpeg().
 */
void mostrarImagenJpeg(const uint8_t* datos, size_t longitud) {
    // Limpiar el panel antes de mostrar la imagen
    apagarTodos();

    // Intentar abrir el JPEG desde el buffer en RAM
    if (JpegDec.decodeArray(datos, longitud)) {
        // Procesar todos los bloques MCU del JPEG
        while (JpegDec.read()) {
            cbkJpeg(&JpegDec.mcuInfo); // escalar y dibujar cada bloque
        }
        FastLED.show(); // mostrar imagen completa
    } else {
        Serial.println("Error: no se pudo decodificar el JPEG");
    }
}

// ============================================================
// SECCIÓN 11: EFECTOS DE TRANSICIÓN
// ============================================================

/**
 * @brief Inicia una transición animada hacia un nuevo modo de visualización.
 * @param nuevoModo Modo al que se quiere transicionar.
 * @param tipo      Tipo de efecto de transición a usar.
 * @details Guarda el frame actual en bufferTransicion[] y registra el instante
 *          de inicio. La transición se actualiza en cada iteración del loop
 *          mediante actualizarTransicion().
 */
void iniciarTransicion(ModoVisualizacion nuevoModo, TipoTransicion tipo) {
    // Guardar el frame actual como origen de la transición
    memcpy(bufferTransicion, bufferImagen, sizeof(bufferImagen));

    modoAnterior       = modoActual;    // recordar modo anterior
    modoActual         = nuevoModo;     // cambiar al nuevo modo
    tipoTransActual    = tipo;          // guardar tipo de transición
    enTransicion       = true;          // activar bandera de transición
    tiempoInicioTransicion = millis();  // marcar inicio del tiempo
}

/**
 * @brief Actualiza el frame actual de la transición animada (no bloqueante).
 * @details Se llama en cada iteración del loop(). Calcula el progreso (0.0-1.0)
 *          según el tiempo transcurrido y aplica el efecto correspondiente
 *          mezclando bufferTransicion (origen) con bufferImagen (destino).
 *          Cuando la transición termina, desactiva la bandera enTransicion.
 */
void actualizarTransicion() {
    if (!enTransicion) return; // no hay transición activa

    // Calcular progreso de la transición (0.0 = inicio, 1.0 = fin)
    uint32_t tiempoTranscurrido = millis() - tiempoInicioTransicion;
    float progreso = (float)tiempoTranscurrido / DURACION_TRANSICION;

    if (progreso >= 1.0f) {
        // Transición completa: mostrar frame destino limpio
        enTransicion = false;
        volcarBufferALeds(); // asegurar que bufferImagen esté en los LEDs
        FastLED.show();
        return;
    }

    // Aplicar efecto según tipo de transición
    for (int y = 0; y < ALTO_PANEL; y++) {
        for (int x = 0; x < ANCHO_PANEL; x++) {

            // Obtener color del frame origen (bufferTransicion)
            int idx = (y * ANCHO_PANEL + x) * 3;
            CRGB origen(bufferTransicion[idx], bufferTransicion[idx+1], bufferTransicion[idx+2]);

            // Obtener color del frame destino (bufferImagen)
            CRGB destino(bufferImagen[idx], bufferImagen[idx+1], bufferImagen[idx+2]);

            CRGB colorFinal;

            switch (tipoTransActual) {

                case TRANS_FADE:
                    // Crossfade lineal: mezcla ponderada de origen y destino
                    colorFinal.r = origen.r + (int8_t)((destino.r - origen.r) * progreso);
                    colorFinal.g = origen.g + (int8_t)((destino.g - origen.g) * progreso);
                    colorFinal.b = origen.b + (int8_t)((destino.b - origen.b) * progreso);
                    break;

                case TRANS_WIPE_IZQ: {
                    // Barrido de derecha a izquierda: columna de corte avanza
                    int colCorte = ANCHO_PANEL - (int)(progreso * ANCHO_PANEL);
                    colorFinal = (x >= colCorte) ? destino : origen;
                    break;
                }

                case TRANS_WIPE_DER: {
                    // Barrido de izquierda a derecha: columna de corte avanza
                    int colCorte = (int)(progreso * ANCHO_PANEL);
                    colorFinal = (x < colCorte) ? destino : origen;
                    break;
                }

                case TRANS_WIPE_ARRIBA: {
                    // Barrido de abajo hacia arriba: fila de corte sube
                    int filaCorte = ALTO_PANEL - (int)(progreso * ALTO_PANEL);
                    colorFinal = (y >= filaCorte) ? destino : origen;
                    break;
                }

                case TRANS_WIPE_ABAJO: {
                    // Barrido de arriba hacia abajo: fila de corte baja
                    int filaCorte = (int)(progreso * ALTO_PANEL);
                    colorFinal = (y < filaCorte) ? destino : origen;
                    break;
                }

                case TRANS_DISSOLVE: {
                    // Disolución aleatoria: cada píxel tiene su propio umbral
                    // basado en un hash determinista de su posición (sin rand())
                    uint32_t hash = (uint32_t)(x * 1234567 + y * 9876543);
                    hash ^= (hash >> 16);
                    float umbral = (hash & 0xFFFF) / 65535.0f; // umbral [0,1] por píxel
                    colorFinal = (progreso >= umbral) ? destino : origen;
                    break;
                }

                default:
                    colorFinal = destino; // fallback: mostrar destino directo
                    break;
            }

            // Escribir color calculado directamente en el buffer del pin
            int pin, indice;
            indice = mapearPixel(x, y, pin);
            if (pin >= 0 && pin < NUM_PINES) {
                bufferPines[pin][indice] = colorFinal;
            }
        }
    }

    FastLED.show(); // enviar frame de transición al hardware
}

// ============================================================
// SECCIÓN 12: MANEJO DE WEBSOCKET
// ============================================================

/**
 * @brief Callback del servidor WebSocket: procesa eventos entrantes.
 * @param num     Número de cliente WebSocket.
 * @param tipo    Tipo de evento (CONNECTED, DISCONNECTED, TEXT, BIN, ERROR).
 * @param payload Puntero a los datos recibidos.
 * @param longitud Longitud del payload en bytes.
 * @details En modo STREAMING_WS, acepta frames binarios RGB888 crudos de
 *          exactamente ANCHO_PANEL × ALTO_PANEL × 3 bytes y los renderiza
 *          directamente al panel sin transición.
 */
void cbkWebSocket(uint8_t num, WStype_t tipo,
                  uint8_t* payload, size_t longitud) {
    switch (tipo) {
        case WStype_CONNECTED:
            Serial.printf("WebSocket cliente %d conectado\n", num);
            break;

        case WStype_DISCONNECTED:
            Serial.printf("WebSocket cliente %d desconectado\n", num);
            // Si estábamos en streaming, volver a apagado
            if (modoActual == MODO_STREAMING_WS) {
                iniciarTransicion(MODO_APAGADO, TRANS_FADE);
            }
            break;

        case WStype_BIN: {
            // Verificar tamaño esperado: ancho × alto × 3 bytes (RGB888)
            size_t tamEsperado = (size_t)(ANCHO_PANEL * ALTO_PANEL * 3);
            if (longitud == tamEsperado) {
                // Cambiar a modo streaming si no estábamos en él
                if (modoActual != MODO_STREAMING_WS) {
                    iniciarTransicion(MODO_STREAMING_WS, TRANS_FADE);
                }
                // Copiar frame al bufferImagen
                memcpy(bufferImagen, payload, tamEsperado);
                // Volcar al hardware inmediatamente
                volcarBufferALeds();
                FastLED.show();
            } else {
                Serial.printf("WebSocket: tamaño incorrecto (%d vs %d)\n",
                    longitud, tamEsperado);
            }
            break;
        }

        case WStype_TEXT:
            // Aceptar también comandos de texto por WebSocket (extensible)
            Serial.printf("WebSocket texto: %s\n", payload);
            break;

        default:
            break;
    }
}

// ============================================================
// SECCIÓN 13: MANEJO DE MENSAJES DE TELEGRAM
// ============================================================

/**
 * @brief Consulta los últimos mensajes del bot de Telegram (getUpdates).
 * @details Realiza una petición HTTPS a getUpdates con el offset actual para
 *          recibir solo mensajes nuevos. Parsea el JSON de respuesta y procesa
 *          cada update llamando a procesarComandoTelegram().
 *          Se debe llamar periódicamente desde el loop, no bloquea.
 */
void consultarTelegram() {
    // Verificar intervalo de polling (no bloqueante)
    if (millis() - ultimoTelegramMs < INTERVALO_TELEGRAM) return;
    ultimoTelegramMs = millis();

    // Solo consultar si hay WiFi
    if (WiFi.status() != WL_CONNECTED) return;

    WiFiClientSecure cliente;
    cliente.setInsecure(); // en producción: cliente.setCACert(TELEGRAM_CERT)

    // URL de getUpdates con offset y timeout corto (long polling ligero)
    char url[256];
    snprintf(url, sizeof(url),
        "/bot%s/getUpdates?offset=%ld&limit=10&timeout=1",
        TELEGRAM_TOKEN, telegramOffset);

    if (!cliente.connect(TELEGRAM_HOST, TELEGRAM_PORT)) return;

    // Enviar petición GET
    cliente.print("GET "); cliente.print(url);
    cliente.println(" HTTP/1.1");
    cliente.print("Host: "); cliente.println(TELEGRAM_HOST);
    cliente.println("Connection: close");
    cliente.println();

    // Leer respuesta JSON (saltar cabeceras HTTP)
    static char jsonBuf[4096]; // buffer estático para el cuerpo JSON
    size_t longBuf = 0;
    bool cabecera = true;
    char ultimos4[4] = {0};
    uint32_t tInicio = millis();

    while (cliente.connected() && millis() - tInicio < 5000) {
        if (cliente.available()) {
            char c = cliente.read();
            if (cabecera) {
                memmove(ultimos4, ultimos4+1, 3);
                ultimos4[3] = c;
                if (memcmp(ultimos4, "\r\n\r\n", 4) == 0) cabecera = false;
            } else {
                if (longBuf < sizeof(jsonBuf)-1) jsonBuf[longBuf++] = c;
            }
        }
    }
    jsonBuf[longBuf] = '\0'; // terminar cadena
    cliente.stop();

    // Parsear JSON de la respuesta
    DynamicJsonDocument doc(4096); // usar heap para JSON de tamaño variable
    if (deserializeJson(doc, jsonBuf) != DeserializationError::Ok) return;

    // Verificar que la respuesta fue OK
    if (!doc["ok"]) return;

    JsonArray resultados = doc["result"].as<JsonArray>();
    for (JsonObject update : resultados) {
        long updateId = update["update_id"];
        telegramOffset = updateId + 1; // avanzar offset para no repetir

        JsonObject mensaje = update["message"];
        if (!mensaje) continue; // ignorar updates sin mensaje

        // Verificar que el mensaje viene del chat autorizado
        long chatId = mensaje["chat"]["id"];
        if (chatId != TELEGRAM_CHAT_ID) continue; // ignorar chats no autorizados

        // Verificar si tiene foto
        if (mensaje.containsKey("photo")) {
            // Tomar la foto de mayor resolución (último elemento del array)
            JsonArray fotos = mensaje["photo"].as<JsonArray>();
            const char* fileId = fotos[fotos.size()-1]["file_id"];
            if (fileId) {
                procesarFotoTelegram(fileId);
            }
            continue;
        }

        // Procesar comando de texto
        const char* texto = mensaje["text"];
        if (texto) {
            procesarComandoTelegram(texto);
        }
    }
}

/**
 * @brief Descarga y muestra una foto recibida por Telegram.
 * @param fileId Identificador del archivo de foto en Telegram.
 */
void procesarFotoTelegram(const char* fileId) {
    enviarMensajeTelegram("Descargando imagen...");

    // Descargar JPEG al buffer estático
    size_t bytes = descargarFotoTelegram(fileId, bufferJpeg, sizeof(bufferJpeg));
    if (bytes == 0) {
        enviarMensajeTelegram("Error al descargar la imagen.");
        return;
    }

    // Decodificar y mostrar la imagen en el panel con transición
    mostrarImagenJpeg(bufferJpeg, bytes); // rellena bufferImagen con la imagen escalada
    iniciarTransicion(MODO_IMAGEN_ESTATICA, tipoTransActual);
    enviarMensajeTelegram("Imagen mostrada!");
}

/**
 * @brief Procesa un comando de texto recibido por Telegram.
 * @param texto Cadena del mensaje (ej: "/texto Hola mundo").
 * @details Implementa los comandos:
 *   /texto <msg>    → activa scroll con ese texto
 *   /color <RRGGBB> → llena el panel con ese color
 *   /brillo <0-255> → ajusta brillo global
 *   /transicion <tipo> → cambia el tipo de transición activa
 *   /imagen         → activa modo imagen (requiere foto previa)
 *   /apagar         → apaga el panel
 *   /estado         → envía estado actual por Telegram
 *   /ayuda          → envía lista de comandos
 */
void procesarComandoTelegram(const char* texto) {
    // Comparar con cada comando conocido usando strncmp (sin String)
    if (strncmp(texto, "/texto ", 7) == 0) {
        // Comando /texto: copiar el mensaje al buffer de scroll
        const char* msg = texto + 7; // apuntar al texto tras "/texto "
        strncpy(textoScroll, msg, sizeof(textoScroll) - 1);
        textoScroll[sizeof(textoScroll)-1] = '\0';
        posScrollX = ANCHO_PANEL; // reiniciar posición del scroll
        iniciarTransicion(MODO_TEXTO_SCROLL, tipoTransActual);
        enviarMensajeTelegram("Texto actualizado!");

    } else if (strncmp(texto, "/color ", 7) == 0) {
        // Comando /color: parsear color hex RRGGBB
        const char* hexColor = texto + 7;
        uint32_t valorColor = strtoul(hexColor, nullptr, 16); // parsear hex a uint32
        uint8_t r = (valorColor >> 16) & 0xFF;
        uint8_t g = (valorColor >>  8) & 0xFF;
        uint8_t b = (valorColor >>  0) & 0xFF;
        colorFondo = CRGB(r, g, b);
        // Llenar todo el buffer de imagen con el color
        for (int i = 0; i < ANCHO_PANEL * ALTO_PANEL; i++) {
            bufferImagen[i*3+0] = r;
            bufferImagen[i*3+1] = g;
            bufferImagen[i*3+2] = b;
        }
        iniciarTransicion(MODO_IMAGEN_ESTATICA, tipoTransActual);
        enviarMensajeTelegram("Color actualizado!");

    } else if (strncmp(texto, "/brillo ", 8) == 0) {
        // Comando /brillo: ajustar brillo global (0-255)
        int valor = atoi(texto + 8); // convertir texto a entero
        brilloActual = constrain(valor, 0, 255); // limitar rango
        FastLED.setBrightness(brilloActual); // aplicar a FastLED
        enviarMensajeTelegram("Brillo actualizado!");

    } else if (strncmp(texto, "/transicion ", 12) == 0) {
        // Comando /transicion: cambiar tipo de transición
        const char* tipo = texto + 12;
        if      (strcmp(tipo, "fade")        == 0) tipoTransActual = TRANS_FADE;
        else if (strcmp(tipo, "wipe_izq")    == 0) tipoTransActual = TRANS_WIPE_IZQ;
        else if (strcmp(tipo, "wipe_der")    == 0) tipoTransActual = TRANS_WIPE_DER;
        else if (strcmp(tipo, "wipe_arriba") == 0) tipoTransActual = TRANS_WIPE_ARRIBA;
        else if (strcmp(tipo, "wipe_abajo")  == 0) tipoTransActual = TRANS_WIPE_ABAJO;
        else if (strcmp(tipo, "dissolve")    == 0) tipoTransActual = TRANS_DISSOLVE;
        enviarMensajeTelegram("Transicion actualizada!");

    } else if (strcmp(texto, "/imagen") == 0) {
        // Comando /imagen: activar modo imagen (usa el buffer actual)
        iniciarTransicion(MODO_IMAGEN_ESTATICA, tipoTransActual);
        enviarMensajeTelegram("Modo imagen activado. Envia una foto para mostrarla.");

    } else if (strcmp(texto, "/apagar") == 0) {
        // Comando /apagar: apagar el panel
        apagarTodos();
        iniciarTransicion(MODO_APAGADO, tipoTransActual);
        enviarMensajeTelegram("Panel apagado.");

    } else if (strcmp(texto, "/estado") == 0) {
        // Comando /estado: reportar estado actual del sistema
        char estado[256];
        const char* modoStr[] = {"imagen", "texto_scroll", "streaming_ws", "apagado"};
        snprintf(estado, sizeof(estado),
            "Estado:\nModo: %s\nBrillo: %d\nPanel: %dx%d\nPines: %d",
            modoStr[modoActual], brilloActual, ANCHO_PANEL, ALTO_PANEL, NUM_PINES);
        enviarMensajeTelegram(estado);

    } else if (strcmp(texto, "/ayuda") == 0) {
        // Comando /ayuda: listar todos los comandos disponibles
        enviarMensajeTelegram(
            "Comandos disponibles:\n"
            "/texto <msg> - Mostrar texto\n"
            "/color <RRGGBB> - Color solido\n"
            "/brillo <0-255> - Ajustar brillo\n"
            "/transicion <tipo> - Cambiar transicion\n"
            "  tipos: fade, wipe_izq, wipe_der, wipe_arriba, wipe_abajo, dissolve\n"
            "/imagen - Activar modo imagen\n"
            "/apagar - Apagar panel\n"
            "/estado - Ver estado\n"
            "/ayuda - Esta ayuda\n"
            "Tambien puedes enviar una foto para mostrarla!"
        );

    } else {
        // Comando desconocido
        enviarMensajeTelegram("Comando no reconocido. Usa /ayuda para ver los comandos.");
    }
}

// ============================================================
// SECCIÓN 14: SETUP Y LOOP PRINCIPAL
// ============================================================

/**
 * @brief Inicialización del sistema (se ejecuta una sola vez al arrancar).
 * @details Configura Serial para depuración, inicializa FastLED, conecta WiFi,
 *          inicia el servidor WebSocket y envía mensaje de bienvenida por Telegram.
 */
void setup() {
    // Iniciar comunicación serie para depuración
    Serial.begin(115200);
    delay(200); // pequeña espera para estabilizar el puerto serie
    Serial.println("\n=== Panel LED ESP32 iniciando ===");

    // --- Imprimir configuración calculada ---
    Serial.printf("Panel: %d x %d pixeles (%d LEDs totales)\n",
        ANCHO_PANEL, ALTO_PANEL, NUM_LEDS_TOTAL);
    Serial.printf("Tiras: %d total, %d por pin, %d pines\n",
        NUM_TIRAS, TIRAS_POR_PIN, NUM_PINES);
    // Estimación de consumo de corriente a brillo máximo
    float amperajeMax = NUM_LEDS_TOTAL * 0.06f;
    float amperajeActual = amperajeMax * (BRILLO_INICIAL / 255.0f);
    Serial.printf("Consumo estimado: %.1f A max, %.1f A @ brillo %d\n",
        amperajeMax, amperajeActual, BRILLO_INICIAL);

    // --- Inicializar FastLED ---
    inicializarFastLED();
    Serial.println("FastLED inicializado");

    // --- Conectar WiFi ---
    conectarWiFi();

    // --- Iniciar servidor WebSocket ---
    servidorWS.begin();
    servidorWS.onEvent(cbkWebSocket); // registrar callback de eventos
    Serial.printf("WebSocket iniciado en puerto %d\n", WEBSOCKET_PUERTO);

    // --- Configurar cliente HTTPS (Telegram) ---
    // Nota: setInsecure() acepta cualquier certificado TLS.
    // Para producción, usar setCACert() con el certificado root de Telegram.
    clienteHTTPS.setInsecure();

    // --- Mostrar animación de inicio y enviar bienvenida ---
    // Activar scroll con texto de bienvenida
    strncpy(textoScroll, "Panel LED listo!", sizeof(textoScroll)-1);
    modoActual = MODO_TEXTO_SCROLL;
    posScrollX = ANCHO_PANEL;

    // Enviar mensaje de bienvenida por Telegram
    enviarMensajeTelegram("Panel LED iniciado! Usa /ayuda para ver los comandos.");
    Serial.println("=== Setup completo ===");
}

/**
 * @brief Bucle principal del programa (ejecutado continuamente).
 * @details Maneja de forma no bloqueante:
 *   1. WebSocket: procesamiento de eventos de red entrantes.
 *   2. Telegram: consulta periódica de mensajes nuevos.
 *   3. Transiciones: actualización del frame de animación en curso.
 *   4. Modo activo: lógica específica del modo actual (scroll de texto, etc).
 *
 * @note El uso de millis() en lugar de delay() garantiza que todas las
 *       tareas se ejecuten sin bloquear el CPU, manteniendo buena respuesta.
 */
void loop() {
    // ---- 1. Procesar eventos WebSocket ----
    // Debe llamarse en cada iteración para no perder frames de streaming
    servidorWS.loop();

    // ---- 2. Consultar mensajes de Telegram ----
    // La función tiene su propio control de tiempo (INTERVALO_TELEGRAM)
    consultarTelegram();

    // ---- 3. Actualizar transición animada (si está activa) ----
    // Si hay una transición en curso, renderizar el siguiente frame
    if (enTransicion) {
        actualizarTransicion();
        return; // durante la transición no actualizar el modo activo
    }

    // ---- 4. Actualizar modo de visualización activo ----
    switch (modoActual) {

        case MODO_TEXTO_SCROLL:
            // Actualizar scroll de texto (tiene su propio control de tiempo)
            actualizarTextoScroll();
            break;

        case MODO_IMAGEN_ESTATICA:
            // La imagen estática no necesita actualización en cada frame
            // (ya está en los LEDs desde cuando se cargó)
            break;

        case MODO_STREAMING_WS:
            // Los frames llegan por WebSocket (manejado en cbkWebSocket)
            // No hay nada que hacer aquí excepto mantener el loop de WS arriba
            break;

        case MODO_APAGADO:
            // Panel apagado: no hacer nada
            break;

        default:
            break;
    }
}

// ============================================================
// FIN DEL ARCHIVO
// ============================================================
/**
 * @note LIBRERÍAS REQUERIDAS (instalar desde el Gestor de librerías de Arduino IDE):
 *   - FastLED        (por Daniel Garcia)     → Control de WS2812B
 *   - WebSockets     (por Markus Sattler)    → Servidor WebSocket
 *   - ArduinoJson    (por Benoit Blanchon)   → Parseo de JSON de Telegram
 *   - JPEGDecoder    (por Bodmer)            → Decodificación JPEG
 *
 * @note CONFIGURACIÓN DEL ESP32 EN ARDUINO IDE:
 *   - Placa: "ESP32 Dev Module" o equivalente
 *   - Partition scheme: "Huge APP (3MB no OTA/1MB SPIFFS)" para más espacio de código
 *   - Upload speed: 921600
 *   - CPU frequency: 240 MHz (máximo rendimiento para multi-pin RMT)
 */
