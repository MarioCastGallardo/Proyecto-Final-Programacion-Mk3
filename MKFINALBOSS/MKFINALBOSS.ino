#include <Wire.h>
#include <U8g2lib.h>
#include <LiquidCrystal_I2C.h>
#include "DHT.h"

// ============================================================
// OLED — modo página (ahorra RAM)
// ============================================================
U8G2_SSD1306_128X64_NONAME_1_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
bool oledConectada = false;

// ============================================================
// LCD
// ============================================================
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ============================================================
// DHT11
// ============================================================
#define DHTPIN  6
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// ============================================================
// LDR
// ============================================================
const int pinLDR = A0;

// ============================================================
// LEDs
// ============================================================
#define LED_ROJO     2
#define LED_AMARILLO 3
#define LED_AZUL     4
#define LED_VERDE    5

// ============================================================
// SENSOR LLUVIA, BUZZER Y LED AGUA
// CORRECCIÓN: digitalRead en lugar de analogRead
// ============================================================
#define PIN_AGUA     A2  // sensor lluvia — lectura digital
#define PIN_BUZZER   7   // buzzer activo
#define PIN_LED_AGUA 8   // led indicador de agua

bool hayAgua      = false;
bool buzzerActivo = true;

// ============================================================
// TEMPORIZACIÓN
// ============================================================
#define INTERVALO_DATOS  2000
#define INTERVALO_OJOS    800

unsigned long ultimoEnvio = 0;
unsigned long ultimoOjo   = 0;

// ============================================================
// ESTADO CARAS
// ============================================================
byte contadorOjos    = 0;
#define TOTAL_CARAS  11

byte expresionActual = 0;

#define CARA_CENTER    0
#define CARA_ANGRY     1
#define CARA_BLINK     2
#define CARA_RIGHT     3
#define CARA_LEFT      4
#define CARA_RANDOM    5
#define CARA_HAPPY     6
#define CARA_SURPRISED 7
#define CARA_TIRED     8
#define CARA_SLEEP     9
#define CARA_LOVE      10

float ultimaTemp    = 0.0;
float ultimaHumedad = 0.0;

// ============================================================
// PROTOTIPOS
// ============================================================
void leerComandos();
void dibujarOjos(byte estado);
void apgarTodosLeds();
void leerSensorAgua();

// ============================================================
// SETUP
// ============================================================
void setup() {
  Serial.begin(9600);
  Wire.begin();
  delay(500);

  dht.begin();

  // LEDs proyecto
  pinMode(LED_ROJO,     OUTPUT);
  pinMode(LED_AMARILLO, OUTPUT);
  pinMode(LED_AZUL,     OUTPUT);
  pinMode(LED_VERDE,    OUTPUT);
  apgarTodosLeds();

  // Sensor lluvia, buzzer y led agua
  pinMode(PIN_AGUA,     INPUT);   // digital input sin pullup
  pinMode(PIN_BUZZER,   OUTPUT);
  pinMode(PIN_LED_AGUA, OUTPUT);
  digitalWrite(PIN_BUZZER,   LOW);
  digitalWrite(PIN_LED_AGUA, LOW);

  // OLED primero
  delay(100);
  if (u8g2.begin()) {
    oledConectada = true;
    u8g2.firstPage();
    do {} while (u8g2.nextPage());
    Serial.println(F("OLED_OK"));
  } else {
    oledConectada = false;
    Serial.println(F("OLED_NO_DETECTADA"));
  }

  // LCD después
  delay(100);
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(F("Sistema listo"));
  lcd.setCursor(0, 1);
  lcd.print(F("Equipo Mk3"));
}

// ============================================================
// LOOP
// ============================================================
void loop() {
  leerComandos();
  leerSensorAgua();

  unsigned long ahora = millis();

  // ── Cambiar cara OLED ──────────────────────────────────
  if (ahora - ultimoOjo >= INTERVALO_OJOS) {
    ultimoOjo = ahora;
    contadorOjos = (contadorOjos + 1) % TOTAL_CARAS;
    expresionActual = contadorOjos;

    if (oledConectada) dibujarOjos(expresionActual);

    Serial.print(F("CARA,"));
    Serial.println(expresionActual);
  }

  // ── Enviar datos sensores cada 2 segundos ──────────────
  if (ahora - ultimoEnvio >= INTERVALO_DATOS) {
    ultimoEnvio = ahora;

    float h = dht.readHumidity();
    float t = dht.readTemperature();
    if (!isnan(h) && !isnan(t)) {
      ultimaHumedad = h;
      ultimaTemp    = t;
    }

    int luz      = analogRead(pinLDR);
    int lectAgua = digitalRead(PIN_AGUA); // ← digital 0 o 1

    // Formato: temp,hum,luz,indiceCara,valorAgua
    Serial.print(ultimaTemp, 1);
    Serial.print(F(","));
    Serial.print(ultimaHumedad, 1);
    Serial.print(F(","));
    Serial.print(luz);
    Serial.print(F(","));
    Serial.print(expresionActual);
    Serial.print(F(","));
    Serial.println(lectAgua);
  }
}

// ============================================================
// LEER SENSOR LLUVIA — digital con antirrebote
// ============================================================
void leerSensorAgua() {
  int lectura = digitalRead(PIN_AGUA); // HIGH=agua, LOW=seco
  bool aguaDetectada = (lectura == HIGH);

  // Antirrebote: 5 lecturas consecutivas iguales para confirmar
  static byte contadorConfirmacion = 0;
  static bool estadoPendiente      = false;
  static bool primeraCorrida       = true;

  if (primeraCorrida) {
    estadoPendiente = aguaDetectada;
    hayAgua         = aguaDetectada;
    primeraCorrida  = false;

    // Sincronizar LEDs y buzzer al inicio
    digitalWrite(PIN_LED_AGUA, hayAgua ? HIGH : LOW);
    digitalWrite(PIN_BUZZER,   (hayAgua && buzzerActivo) ? HIGH : LOW);
    return;
  }

  if (aguaDetectada == estadoPendiente) {
    if (contadorConfirmacion < 5) contadorConfirmacion++;
  } else {
    estadoPendiente      = aguaDetectada;
    contadorConfirmacion = 0;
  }

  // Solo actuar cuando se confirman 5 lecturas y el estado cambió
  if (contadorConfirmacion >= 5 && aguaDetectada != hayAgua) {
    hayAgua              = aguaDetectada;
    contadorConfirmacion = 0;

    if (hayAgua) {
      digitalWrite(PIN_LED_AGUA, HIGH);
      if (buzzerActivo) digitalWrite(PIN_BUZZER, HIGH);
      Serial.println(F("AGUA,ON"));
    } else {
      digitalWrite(PIN_LED_AGUA, LOW);
      digitalWrite(PIN_BUZZER,   LOW);
      Serial.println(F("AGUA,OFF"));
    }
  }
}

// ============================================================
// LEER COMANDOS
// ============================================================
void leerComandos() {
  if (!Serial.available()) return;

  String cmd = Serial.readStringUntil('\n');
  cmd.trim();

  if      (cmd == F("LED_ROJO_ON"))      { digitalWrite(LED_ROJO,     HIGH); Serial.println(F("ESTADO_LED,ROJO,ON"));      }
  else if (cmd == F("LED_ROJO_OFF"))     { digitalWrite(LED_ROJO,     LOW);  Serial.println(F("ESTADO_LED,ROJO,OFF"));     }
  else if (cmd == F("LED_AMARILLO_ON"))  { digitalWrite(LED_AMARILLO, HIGH); Serial.println(F("ESTADO_LED,AMARILLO,ON"));  }
  else if (cmd == F("LED_AMARILLO_OFF")) { digitalWrite(LED_AMARILLO, LOW);  Serial.println(F("ESTADO_LED,AMARILLO,OFF")); }
  else if (cmd == F("LED_AZUL_ON"))      { digitalWrite(LED_AZUL,     HIGH); Serial.println(F("ESTADO_LED,AZUL,ON"));      }
  else if (cmd == F("LED_AZUL_OFF"))     { digitalWrite(LED_AZUL,     LOW);  Serial.println(F("ESTADO_LED,AZUL,OFF"));     }
  else if (cmd == F("LED_VERDE_ON"))     { digitalWrite(LED_VERDE,    HIGH); Serial.println(F("ESTADO_LED,VERDE,ON"));     }
  else if (cmd == F("LED_VERDE_OFF"))    { digitalWrite(LED_VERDE,    LOW);  Serial.println(F("ESTADO_LED,VERDE,OFF"));    }
  else if (cmd == F("LEDS_OFF"))         { apgarTodosLeds();                 Serial.println(F("ESTADO_LED,TODOS,OFF"));    }

  else if (cmd == F("BUZZER_ON")) {
    buzzerActivo = true;
    if (hayAgua) digitalWrite(PIN_BUZZER, HIGH);
    Serial.println(F("ESTADO_BUZZER,ON"));
  }
  else if (cmd == F("BUZZER_OFF")) {
    buzzerActivo = false;
    digitalWrite(PIN_BUZZER, LOW);
    Serial.println(F("ESTADO_BUZZER,OFF"));
  }

  else if (cmd.startsWith(F("OLED_CARA,"))) {
    byte idx = cmd.substring(10).toInt();
    if (idx < TOTAL_CARAS) {
      expresionActual = idx;
      if (oledConectada) dibujarOjos(expresionActual);
      Serial.print(F("CARA,"));
      Serial.println(expresionActual);
    }
  }

  else if (cmd.startsWith(F("LCD_PRINT|"))) {
    cmd.remove(0, 10);
    int sep = cmd.indexOf('|');
    String l1 = "", l2 = "";
    if (sep >= 0) { l1 = cmd.substring(0, sep); l2 = cmd.substring(sep + 1); }
    else          { l1 = cmd; }
    l1.trim(); l2.trim();
    lcd.clear();
    lcd.setCursor(0, 0); lcd.print(l1.substring(0, 16));
    if (l2.length() > 0) { lcd.setCursor(0, 1); lcd.print(l2.substring(0, 16)); }
    Serial.println(F("ESTADO_LCD,PRINT,OK"));
  }

  else if (cmd.startsWith(F("LCD_L1|"))) {
    cmd.remove(0, 7); cmd.trim();
    lcd.setCursor(0, 0); lcd.print(F("                "));
    lcd.setCursor(0, 0); lcd.print(cmd.substring(0, 16));
    Serial.println(F("ESTADO_LCD,L1,OK"));
  }

  else if (cmd.startsWith(F("LCD_L2|"))) {
    cmd.remove(0, 7); cmd.trim();
    lcd.setCursor(0, 1); lcd.print(F("                "));
    lcd.setCursor(0, 1); lcd.print(cmd.substring(0, 16));
    Serial.println(F("ESTADO_LCD,L2,OK"));
  }

  else if (cmd == F("LCD_CLEAR")) {
    lcd.clear();
    Serial.println(F("ESTADO_LCD,CLEAR,OK"));
  }

  else {
    Serial.print(F("CMD_DESCONOCIDO,"));
    Serial.println(cmd);
  }
}

// ============================================================
// DIBUJAR OJOS — 11 expresiones
// ============================================================
void dibujarOjos(byte estado) {
  u8g2.firstPage();
  do {
    if (estado == CARA_CENTER) {
      u8g2.drawRBox(20, 18, 35, 28, 8);
      u8g2.drawRBox(73, 18, 35, 28, 8);
    }
    else if (estado == CARA_BLINK) {
      u8g2.drawRBox(20, 30, 35, 5, 2);
      u8g2.drawRBox(73, 30, 35, 5, 2);
    }
    else if (estado == CARA_RIGHT) {
      u8g2.drawRBox(30, 18, 35, 28, 8);
      u8g2.drawRBox(83, 18, 35, 28, 8);
    }
    else if (estado == CARA_LEFT) {
      u8g2.drawRBox(10, 18, 35, 28, 8);
      u8g2.drawRBox(63, 18, 35, 28, 8);
    }
    else if (estado == CARA_RANDOM) {
      int x = random(-8, 9);
      int y = random(-5, 6);
      u8g2.drawRBox(20 + x, 18 + y, 35, 28, 8);
      u8g2.drawRBox(73 + x, 18 + y, 35, 28, 8);
    }
    else if (estado == CARA_ANGRY) {
      u8g2.drawRBox(20, 22, 35, 24, 8);
      u8g2.drawRBox(73, 22, 35, 24, 8);
      u8g2.setDrawColor(0);
      u8g2.drawTriangle(20, 22, 40, 22, 20, 32);
      u8g2.drawTriangle(108, 22, 88, 22, 108, 32);
      u8g2.setDrawColor(1);
    }
    else if (estado == CARA_HAPPY) {
      u8g2.drawRBox(20, 18, 35, 28, 8);
      u8g2.drawRBox(73, 18, 35, 28, 8);
      u8g2.setDrawColor(0);
      u8g2.drawDisc(33, 29, 5);
      u8g2.drawDisc(86, 29, 5);
      u8g2.setDrawColor(1);
    }
    else if (estado == CARA_SURPRISED) {
      u8g2.drawDisc(37, 32, 15);
      u8g2.drawDisc(90, 32, 15);
      u8g2.setDrawColor(0);
      u8g2.drawDisc(40, 29, 7);
      u8g2.drawDisc(93, 29, 7);
      u8g2.setDrawColor(1);
    }
    else if (estado == CARA_TIRED) {
      u8g2.drawRBox(20, 18, 35, 28, 8);
      u8g2.drawRBox(73, 18, 35, 28, 8);
      u8g2.setDrawColor(0);
      u8g2.drawBox(20, 18, 35, 14);
      u8g2.drawBox(73, 18, 35, 14);
      u8g2.setDrawColor(1);
    }
    else if (estado == CARA_SLEEP) {
      u8g2.drawLine(20, 18, 55, 46);
      u8g2.drawLine(55, 18, 20, 46);
      u8g2.drawLine(21, 18, 56, 46);
      u8g2.drawLine(56, 18, 21, 46);
      u8g2.drawLine(73, 18, 108, 46);
      u8g2.drawLine(108, 18, 73, 46);
      u8g2.drawLine(74, 18, 109, 46);
      u8g2.drawLine(109, 18, 74, 46);
    }
    else if (estado == CARA_LOVE) {
      u8g2.drawDisc(29, 22, 9);
      u8g2.drawDisc(40, 22, 9);
      u8g2.drawTriangle(20, 27, 49, 27, 34, 44);
      u8g2.drawDisc(82, 22, 9);
      u8g2.drawDisc(93, 22, 9);
      u8g2.drawTriangle(73, 27, 102, 27, 87, 44);
    }
  } while (u8g2.nextPage());
}

// ============================================================
// UTILIDADES
// ============================================================
void apgarTodosLeds() {
  digitalWrite(LED_ROJO,     LOW);
  digitalWrite(LED_AMARILLO, LOW);
  digitalWrite(LED_AZUL,     LOW);
  digitalWrite(LED_VERDE,    LOW);
}