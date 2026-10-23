// ==============================================================================
// FIRMWARE MASTER: ESP32-S3 DEVKITC-1 (N16R8 - 16MB Flash & 8MB OPI PSRAM)
// Proyek: Sistem Monitoring Tekanan & Suhu dengan Kontrol Solenoid Otomatis
// Mata Kuliah: Workshop Mikrokontroler Lanjut dan Teknik Antarmuka (WMLTA)
// Layar: TFT ST7789 1.47 Inch SPI (172 x 320) - Mode Horizontal / Landscape (320 x 172)
// Fitur Layar: Hardware SPI 40MHz, Rotasi 180 Derajat (Rotation 3),
//              Zero-Flicker Differential Rendering (Hanya update karakter yang berubah)
// ==============================================================================

#include <WiFi.h>
#include <WebServer.h>
#include <PubSubClient.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>

// ------------------------------------------------------------------------------
// KONFIGURASI JARINGAN WIFI & MQTT
// ------------------------------------------------------------------------------
// Mode Access Point (ESP32 memancarkan WiFi Hotspot sendiri untuk Web SCADA)
const char* AP_SSID = "SCADA_BOILER_PLTU";
const char* AP_PASS = "admin1234";

// Mode Station (Opsional: Hubungkan ke Router jika ingin terhubung ke internet/MQTT luar)
// PENTING: Set 'ENABLE_STA_ROUTER = false' jika hanya ingin Web Dashboard via Hotspot ESP32.
// Jika di-set 'true' tapi router tidak aktif, ESP32 akan terus scanning channel yang
// menyebabkan WiFi Hotspot PUTUS-NYAMBUNG!
const bool  ENABLE_STA_ROUTER = false; 
const char* STA_SSID          = "Riezky Mochammad";
const char* STA_PASS          = "00000000";

// MQTT Broker (Opsional: Komputer lokal dengan Mosquitto / dashboard_mqtt_bridge.py)
const char* MQTT_BROKER = "192.168.4.2";
const int   MQTT_PORT   = 1883;

// ------------------------------------------------------------------------------
// DEFINISI PIN HARDWARE (SESUAI SKEMATIK KICAD MASTER)
// ------------------------------------------------------------------------------
// Layar TFT ST7789 SPI (1.47 Inch)
#define PIN_TFT_DC    9    // GPIO 09: TFT Data/Command
#define PIN_TFT_CS    10   // GPIO 10: TFT Chip Select
#define PIN_TFT_MOSI  11   // GPIO 11: SPI MOSI (SDA)
#define PIN_TFT_SCK   12   // GPIO 12: SPI SCK (SCL)
#define PIN_TFT_RST   14   // GPIO 14: TFT Reset

// Potensiometer Setpoint (ADC1)
#define PIN_VR_SETPOINT 4  // GPIO 04: RV5 Wiper (0 - 3.3V)

// Tombol Master (Active LOW - Internal Pullup)
#define PIN_PB1_MASTER  5  // GPIO 05: Ganti Halaman Layar TFT
#define PIN_PB2_MASTER  6  // GPIO 06: Global Alarm Silence / Mute
#define PIN_PB3_MASTER  7  // GPIO 07: Emergency All-Purge (Buka kedua katup)

// Dual UART Hardware ESP32-S3
// UART 1 -> Slave 1
#define PIN_UART1_RX   16  // GPIO 16 <- Slave1TX_ESP (via divider R5/R6)
#define PIN_UART1_TX   17  // GPIO 17 -> ESPTX_Slave1 (langsung ke D2 Nano 1)

// UART 2 -> Slave 2
#define PIN_UART2_RX   8   // GPIO 08 <- Slave2TX_ESP (via divider R7/R8)
#define PIN_UART2_TX   18  // GPIO 18 -> ESPTX_Slave2 (langsung ke D2 Nano 2)

// ------------------------------------------------------------------------------
// KONFIGURASI DISPLAY 1.47 INCH (172 x 320) HORIZONTAL
// ------------------------------------------------------------------------------
#define TFT_WIDTH       320
#define TFT_HEIGHT      172

// Rotasi Layar Horizontal (3 = Landscape diputar 180 derajat dari rotasi 1)
#define TFT_ROTATION    3

// ------------------------------------------------------------------------------
// OBJEK DRIVER & SERVER
// ------------------------------------------------------------------------------
// Gunakan Hardware SPI (&SPI) agar transfer data secepat 40 MHz (Bebas Kedip / Lag)
Adafruit_ST7789 tft = Adafruit_ST7789(&SPI, PIN_TFT_CS, PIN_TFT_DC, PIN_TFT_RST);
WebServer server(80);
WiFiClient espClient;
PubSubClient mqttClient(espClient);

// ------------------------------------------------------------------------------
// STRUKTUR DATA TELEMETRI BOILER
// ------------------------------------------------------------------------------
struct BoilerData {
  float pressure;       // Bar
  float temp;           // Celcius
  String srcPressure;   // "REAL" atau "SIM"
  String srcTemp;       // "REAL" atau "SIM"
  String valve;         // "OPEN" atau "CLOSED"
  bool buzzer;          // true / false
  String state;         // "NORMAL", "WARNING", "ALARM"
  unsigned long lastSeen;
  bool isConnected;
  unsigned long packetCount;
};

BoilerData boiler1 = {0.0, 0.0, "REAL", "REAL", "CLOSED", false, "NORMAL", 0, false, 0};
BoilerData boiler2 = {0.0, 0.0, "REAL", "REAL", "CLOSED", false, "NORMAL", 0, false, 0};

// Setpoint Ambang Batas dari Potensio Master RV5
float setpointPressure = 4.0;   // Rentang: 1.0 - 8.0 Bar
float setpointTemp     = 160.0; // Rentang: 100 - 200 C
float lastSentSetpoint = -1.0;

// Navigasi Layar TFT (0: Dual Overview, 1: Detail Gauge, 2: Info WiFi/MQTT)
uint8_t tftScreenPage    = 0;
uint8_t lastRenderedPage = 255; // Pelacak perubahan halaman untuk redrawing bebas kedip
bool    forceFullRedraw  = true;

// Timing Millis
unsigned long lastTftRefresh   = 0;
unsigned long lastAdcRead      = 0;
unsigned long lastMqttSend     = 0;
unsigned long lastDebouncePB1  = 0;
unsigned long lastDebouncePB2  = 0;
unsigned long lastDebouncePB3  = 0;
unsigned long lastBlinkMillis  = 0;
bool alarmBlinkState           = false;
const unsigned long DEBOUNCE_MS = 200;

// Buffer Penerimaan UART
String rxBuffer1 = "";
String rxBuffer2 = "";

// --------------------------------------------// Buffer Penerimaan UART (char[] tetap -> tanpa String, tanpa fragmentasi heap)
#define RX_MAX 80
char    rx1[RX_MAX];  uint8_t rx1Len = 0;  bool rx1Over = false;
char    rx2[RX_MAX];  uint8_t rx2Len = 0;  bool rx2Over = false;

// Status global: true bila perintah "buka semua katup" sedang aktif (toggle PB3 / web)
bool purgeAllActive = false;

// Tombol master dengan deteksi tepi (1 tekan = 1 aksi) + debounce
struct MasterButton {
  uint8_t pin;
  bool    stable;
  bool    lastRaw;
  unsigned long changedAt;
};
MasterButton btnPB1 = {PIN_PB1_MASTER, false, false, 0};
MasterButton btnPB2 = {PIN_PB2_MASTER, false, false, 0};
MasterButton btnPB3 = {PIN_PB3_MASTER, false, false, 0};

// ------------------------------------------------------------------------------
// PALET WARNA INDUSTRIAL MODERN & COLORFUL (RGB565)
// ------------------------------------------------------------------------------
#define COLOR_BG          0x0000  // Matte Pure Black (Kontras Maksimal)
#define COLOR_HEADER_BG   0x10A2  // Charcoal Header
#define COLOR_CARD_BG     0x10A2  // Dark Slate Card Box
#define COLOR_CARD_BORDER 0x2945  // Subtle Slate Border
#define COLOR_TEXT_MUTED  0x8410  // Soft Gray Text
#define COLOR_WHITE       0xFFFF  // Crisp White
#define COLOR_BLACK       0x0000  // Black
#define COLOR_CYAN        0x07FF  // Bright Electric Cyan (Aksen Tekanan & B1)
#define COLOR_AMBER       0xFD20  // Vivid Fire Amber (Aksen Suhu)
#define COLOR_GOLD        0xFFE0  // Bright Gold / Yellow (Aksen Setpoint)
#define COLOR_NORMAL      0x07E0  // Vivid Emerald Green (Status Normal)
#define COLOR_WARNING     0xFF80  // Warning Amber/Orange
#define COLOR_DANGER      0xF800  // Bright Crimson Red (Alarm/Emergency)
#define COLOR_VIOLET      0xB81F  // Electric Violet / Purple (Aksen B2)
#define COLOR_VALVE_SAFE  0x0A40  // Dark Forest Green (Katup Tutup)
#define COLOR_VALVE_ALERT 0xF800  // Bright Red (Katup Buka)
#define COLOR_PILL_DARK   0x2124  // Background Badge Header

// ------------------------------------------------------------------------------
// CACHE RENDERING DIFFERENTIAL (ZERO FLICKER CACHE)
// Menyimpan nilai terakhir yang digambar, sehingga hanya bagian layar
// yang berubah yang diperbarui!
// ------------------------------------------------------------------------------
struct CardCache {
  char     pStr[10];
  char     tStr[10];
  char     srcP;        // 0 = belum digambar, 'R' / 'S' / '-'
  char     srcT;
  int8_t   valve;       // 0 = belum, 1 = tutup, 2 = buka, 3 = offline
  int8_t   badge;       // 0 = belum, 1 = NORM, 2 = WARN, 3 = ALRM, 4 = DISC
  int8_t   link;        // 0 = belum, 1 = aktif, 2 = mati
  int8_t   buzz;        // 0 = belum, 1 = off, 2 = on
  int      barW;
  uint16_t barCol;
  bool     blink;
};

struct Page0Cache {
  CardCache c[2];
  char      setpointStr[26];
  int       apN;        // jumlah klien AP (atau 100 = STA tersambung), -1 = belum
  int8_t    purgeLbl;   // 0 = belum, 1 = "Purge", 2 = "Tutup"
};

Page0Cache cache0;

struct RowCache {
  char     s[28];
  in  int8_t mqttState;     // -1 = belum digambar, 0 = off, 1 = on
  unsigned long lastSec;
  char rv5Str[16];
  int  apN;
};

Page2Cache cache2;

void resetCaches() {
  memset(&cache0, 0, sizeof(cache0));
  cache0.c[0].barW = -1;
  cache0.c[1].barW = -1;
  cache0.apN = -1;
  memset(&cache1, 0, sizeof(cache1));
  for (int i = 0; i < 4; i++) cache1.row[i].w = -1;
  memset(&cache2, 0, sizeof(cache2));
  cache2.lastSec = 999999;
  cache2.apN = -1;
  cache2.mqttState = -1;
}

// ------------------------------------------------------------------------------
// PROTOTYPE FUNGSI
// ------------------------------------------------------------------------------
void initTft();
void renderTftDashboard();
void drawStaticPage0();
void drawDynamicPage0();
void drawStaticPage1();
void drawDynamicPage1();
void drawStaticPage2();
void drawDynamicPage2();
void readSetpointPot();
void handleDualUart();
void parseSlavePacket(int slaveId, char *pkt);
void sendSetpointToSlaves();
void sendCommandToSlave(int targetId, String cmd);
void emergencyPurgeAll();
void silenceAllAlarms();
void setupWebServer();
void sendMqttTelemetry();

// ------------------------------------------------------------------------------
// HTML HALAMAN DASHBOARD SCADA (TERPISAH DI dashboard_html.h AGAR BERSIH & CEPAT COMPILE)
// ------------------------------------------------------------------------------
#include "dashboard_html.h"

// ------------------------------------------------------------------------------
// INISIALISASI LAYAR TFT 1.47 INCH ST7789 HORIZONTAL (320 x 172)
// ------------------------------------------------------------------------------
void initTft() {
  // Inisialisasi Hardware SPI pin
  SPI.begin(PIN_TFT_SCK, -1, PIN_TFT_MOSI, PIN_TFT_CS);

  // Inisialisasi controller ST7789 untuk modul 1.47" (172 x 320)
  tft.init(172, 320);

  // Set frekuensi SPI ke 40 MHz Hardware SPI (Super Cepat, Tanpa Garis Scan!)
  tft.setSPISpeed(40000000);

  // Rotasi Layar Horizontal (3 = Diputar 180 derajat dari rotasi 1)
  tft.setRotation(TFT_ROTATION);
  tft.fillScreen(COLOR_BG);

  // Splash Screen Booting yang Elegan & Bersih
  tft.fillRoundRect(20, 25, 280, 122, 8, COLOR_CARD_BG);
  tft.drawRoundRect(20, 25, 280, 122, 8, COLOR_CARD_BORDER);

  tft.setTextWrap(false);
  tft.setTextColor(COLOR_WHITE, COLOR_CARD_BG);
  tft.setTextSize(2);
  tft.setCursor(45, 42);
  tft.print("PLTU BOILER SCADA");

  tft.setTextSize(1);
  tft.setTextColor(COLOR_CYAN, COLOR_CARD_BG);
  tft.setCursor(45, 68);
  tft.print("MONITORING & CONTROL MASTER");

  tft.setTextColor(COLOR_TEXT_MUTED, COLOR_CARD_BG);
  tft.setCursor(45, 84);
  tft.print("WMLTA - Workshop Mikrokontroler Lanjut");

  // Loading Progress Bar
  tft.drawRoundRect(45, 108, 230, 8, 3, COLOR_CARD_BORDER);
  for (int w = 0; w <= 226; w += 25) {
    tft.fillRoundRect(47, 110, w, 4, 2, COLOR_NORMAL);
    delay(30);
  }

  delay(200);
  tft.fillScreen(COLOR_BG);
  forceFullRedraw = true;
  resetCaches();
}

// ------------------------------------------------------------------------------
// FUNGSI UTAMA RENDER TFT DASHBOARD
// ------------------------------------------------------------------------------
void renderTftDashboard() {
  // Indikator kedip alarm setiap 300 ms
  if (millis() - lastBlinkMillis >= 300) {
    lastBlinkMillis = millis();
    alarmBlinkState = !alarmBlinkState;
  }

  // Refresh hanya setiap 100 ms agar efisien
  if (millis() - lastTftRefresh < 100) return;
  lastTftRefresh = millis();

  // Jika halaman berganti, gambar ulang bingkai statis ONCE (100% Bebas Kedip!)
  if (tftScreenPage != lastRenderedPage || forceFullRedraw) {
    tft.fillScreen(COLOR_BG);
    lastRenderedPage = tftScreenPage;
    forceFullRedraw  = false;
    resetCaches();

    if (tftScreenPage == 0) {
      drawStaticPage0();
    } else if (tftScreenPage == 1) {
      drawStaticPage1();
    } else if (tftScreenPage == 2) {
      drawStaticPage2();
    }
  }

  // Perbarui HANYA data dan karakter yang nilainya berubah!
  if (tftScreenPage == 0) {
    drawDynamicPage0();
  } else if (tftScreenPage == 1) {
    drawDynamicPage1();
  } else if (tftScreenPage == 2) {
    drawDynamicPage2();
  }
}

// ==============================================================================
// HALAMAN 0: DUAL BOILER OVERVIEW (TAMPILAN UTAMA 320 x 172)
// ==============================================================================
void drawStaticPage0() {
  // 1. TOP HEADER (Y: 0 - 20)
  tft.fillRect(0, 0, 320, 20, COLOR_HEADER_BG);
  tft.drawFastHLine(0, 20, 320, COLOR_CYAN); // Aksen garis Cyan pembatas header

  tft.setTextSize(1);
  tft.setTextColor(COLOR_WHITE, COLOR_HEADER_BG);
  tft.setCursor(6, 6);
  tft.print("PLTU BOILER");

  // Tag Badge [SCADA]
  tft.fillRoundRect(78, 4, 42, 12, 3, COLOR_PILL_DARK);
  tft.setTextColor(COLOR_CYAN, COLOR_PILL_DARK);
  tft.setCursor(82, 6);
  tft.print("SCADA");

  // 2. BINGKAI KARTU BOILER 1 (KIRI: X: 3, Y: 22, W: 154, H: 128)
  tft.fillRoundRect(3, 22, 154, 128, 5, COLOR_CARD_BG);
  tft.drawRoundRect(3, 22, 154, 128, 5, COLOR_CARD_BORDER);
  // Garis aksen Cyan di header kartu B1
  tft.drawFastHLine(5, 23, 150, COLOR_CYAN);

  tft.setTextColor(COLOR_WHITE, COLOR_CARD_BG);
  tft.setCursor(9, 28);
  tft.print("B1: CHAMBER 1");
  tft.drawFastHLine(7, 39, 146, COLOR_CARD_BORDER);

  // Label statis B1
  tft.setCursor(9, 48);
  tft.setTextColor(COLOR_CYAN, COLOR_CARD_BG);
  tft.print("P:");
  tft.setCursor(88, 52);
  tft.setTextColor(COLOR_TEXT_MUTED, COLOR_CARD_BG);
  tft.print("Bar");

  tft.setCursor(9, 70);
  tft.setTextColor(COLOR_AMBER, COLOR_CARD_BG);
  tft.print("T:");
  tft.setCursor(88, 74);
  tft.setTextColor(COLOR_TEXT_MUTED, COLOR_CARD_BG);
  tft.print("C");

  // Bingkai Progress Bar B1 (Y: 88, W: 142)
  tft.drawRoundRect(9, 88, 142, 6, 2, COLOR_CARD_BORDER);

  // 3. BINGKAI KARTU BOILER 2 (KANAN: X: 163, Y: 22, W: 154, H: 128)
  tft.fillRoundRect(163, 22, 154, 128, 5, COLOR_CARD_BG);
  tft.drawRoundRect(163, 22, 154, 128, 5, COLOR_CARD_BORDER);
  // Garis aksen Violet di header kartu B2
  tft.drawFastHLine(165, 23, 150, COLOR_VIOLET);

  tft.setTextColor(COLOR_WHITE, COLOR_CARD_BG);
  tft.setCursor(169, 28);
  tft.print("B2: CHAMBER 2");
  tft.drawFastHLine(167, 39, 146, COLOR_CARD_BORDER);

  // Label statis B2
  tft.setCursor(169, 48);
  tft.setTextColor(COLOR_CYAN, COLOR_CARD_BG);
  tft.print("P:");
  tft.setCursor(248, 52);
  tft.setTextColor(COLOR_TEXT_MUTED, COLOR_CARD_BG);
  tft.print("Bar");

  tft.setCursor(169, 70);
  tft.setTextColor(COLOR_AMBER, COLOR_CARD_BG);
  tft.print("T:");
  tft.setCursor(248, 74);
  tft.setTextColor(COLOR_TEXT_MUTED, COLOR_CARD_BG);
  tft.print("C");

  // Bingkai Progress Bar B2 (Y: 88, W: 142)
  tft.drawRoundRect(169, 88, 142, 6, 2, COLOR_CARD_BORDER);

  // 4. BOTTOM FOOTER BAR (Y: 153 - 171)
  tft.fillRect(0, 153, 320, 19, COLOR_HEADER_BG);
  tft.drawFastHLine(0, 152, 320, COLOR_CARD_BORDER);

  tft.setTextColor(COLOR_CYAN, COLOR_HEADER_BG);
  tft.setCursor(6, 157);
  tft.print("[PB1:Tab]");

  tft.setTextColor(COLOR_GOLD, COLOR_HEADER_BG);
  tft.setCursor(105, 157);
  tft.print("[PB2:Mute]");

  tft.setTextColor(COLOR_DANGER, COLOR_HEADER_BG);
  tft.setCursor(195, 157);
  tft.print("[PB3:Purge]");

  // Indikator halaman
  tft.setTextColor(COLOR_WHITE, COLOR_HEADER_BG);
  tft.setCursor(285, 157);
  tft.print("1/3");
}

// Menggambar satu kartu boiler (x0 = 3 untuk B1, 163 untuk B2).
// Hanya elemen yang nilainya berubah yang digambar ulang.
void drawBoilerCard(uint8_t idx, const BoilerData &b, int x0) {
  CardCache &k = cache0.c[idx];
  const bool conn    = b.isConnected;
  const bool isAlarm = conn && b.state == "ALARM";

  // ---- 1. Badge status + border kartu ----
  int8_t badge = !conn ? 4 : isAlarm ? 3 : (b.state == "WARNING") ? 2 : 1;
  if (badge != k.badge || (isAlarm && alarmBlinkState != k.blink)) {
    uint16_t bg, fg = COLOR_WHITE;
    const char *txt;
    switch (badge) {
      case 4:  bg = COLOR_TEXT_MUTED; txt = "DISC"; break;
      case 3:  bg = alarmBlinkState ? COLOR_DANGER : 0x6000; txt = "ALRM"; break;
      case 2:  bg = COLOR_WARNING; fg = COLOR_BLACK; txt = "WARN"; break;
      default: bg = COLOR_NORMAL;  fg = COLOR_BLACK; txt = "NORM"; break;
    }
    tft.fillRoundRect(x0 + 107, 25, 42, 13, 3, bg);
    tft.setTextSize(1);
    tft.setTextColor(fg, bg);
    tft.setCursor(x0 + 111, 28);
    tft.print(txt);

    uint16_t border = isAlarm ? (alarmBlinkState ? COLOR_DANGER : COLOR_CARD_BORDER)
                    : (badge == 2) ? COLOR_WARNING : COLOR_CARD_BORDER;
    tft.drawRoundRect(x0, 22, 154, 128, 5, border);
    k.badge = badge;
  }

  // ---- 2. Nilai tekanan (lebar tetap 5 karakter -> tidak ada sisa karakter) ----
  char buf[10];
  if (conn) snprintf(buf, sizeof(buf), "%5.2f", constrain(b.pressure, 0.0f, 99.99f));
  else      strcpy(buf, " --- ");
  if (strcmp(buf, k.pStr) != 0) {
    tft.setTextSize(2);
    tft.setTextColor(COLOR_CYAN, COLOR_CARD_BG);
    tft.setCursor(x0 + 22, 44);
    tft.print(buf);
    strcpy(k.pStr, buf);
  }

  // ---- 3. Nilai suhu (lebar tetap 3 karakter) ----
  if (!conn)                  strcpy(buf, "---");
  else if (b.temp >= 900.0f)  strcpy(buf, "ERR");
  else snprintf(buf, sizeof(buf), "%3d", (int)constrain(b.temp, -99.0f, 899.0f));
  if (strcmp(buf, k.tStr) != 0) {
    tft.setTextSize(2);
    tft.setTextColor(COLOR_AMBER, COLOR_CARD_BG);
    tft.setCursor(x0 + 22, 66);
    tft.print(buf);
    strcpy(k.tStr, buf);
  }

  // ---- 4. Tag sumber sensor [R]/[S] ----
  char sp = !conn ? '-' : (b.srcPressure == "REAL" ? 'R' : 'S');
  if (sp != k.srcP) {
    tft.setTextSize(1);
    tft.setTextColor(sp == 'R' ? COLOR_NORMAL : (sp == 'S' ? COLOR_AMBER : COLOR_TEXT_MUTED), COLOR_CARD_BG);
    tft.setCursor(x0 + 122, 52);
    tft.print('['); tft.print(sp); tft.print(']');
    k.srcP = sp;
  }
  char st = !conn ? '-' : (b.srcTemp == "REAL" ? 'R' : 'S');
  if (st != k.srcT) {
    tft.setTextSize(1);
    tft.setTextColor(st == 'R' ? COLOR_NORMAL : (st == 'S' ? COLOR_AMBER : COLOR_TEXT_MUTED), COLOR_CARD_BG);
    tft.setCursor(x0 + 122, 74);
    tft.print('['); tft.print(st); tft.print(']');
    k.srcT = st;
  }

  // ---- 5. Progress bar tekanan terhadap setpoint (diferensial) ----
  float sps = (setpointPressure < 0.1f) ? 0.1f : setpointPressure;
  int w = conn ? constrain((int)((b.pressure / sps) * 138), 0, 138) : 0;
  uint16_t col = !conn ? COLOR_TEXT_MUTED
               : (b.pressure >= setpointPressure) ? COLOR_DANGER
               : (b.pressure >= setpointPressure * 0.85f) ? COLOR_WARNING : COLOR_CYAN;
  if (w != k.barW || col != k.barCol) {
    if (k.barW < 0 || col != k.barCol) {            // Warna berubah: gambar ulang tanpa menghapus dulu
      if (w > 0)   tft.fillRect(x0 + 8, 90, w, 2, col);
      if (w < 138) tft.fillRect(x0 + 8 + w, 90, 138 - w, 2, COLOR_CARD_BG);
    } else if (w > k.barW) {
      tft.fillRect(x0 + 8 + k.barW, 90, w - k.barW, 2, col);
    } else {
      tft.fillRect(x0 + 8 + w, 90, k.barW - w, 2, COLOR_CARD_BG);
    }
    k.barW = w;
    k.barCol = col;
  }

  // ---- 6. Status katup ----
  int8_t v = !conn ? 3 : (b.valve == "OPEN") ? 2 : 1;
  if (v != k.valve || (v == 2 && alarmBlinkState != k.blink)) {
    uint16_t bg = (v == 2) ? (alarmBlinkState ? COLOR_VALVE_ALERT : 0x5800)
                : (v == 1) ? COLOR_VALVE_SAFE : COLOR_PILL_DARK;
    tft.fillRoundRect(x0 + 5, 98, 144, 18, 3, bg);
    tft.setTextSize(1);
    tft.setTextColor(v == 3 ? COLOR_TEXT_MUTED : COLOR_WHITE, bg);
    tft.setCursor(x0 + 9, 103);
    tft.print(v == 2 ? "KATUP: BUANG (90*)" : v == 1 ? "KATUP: TUTUP (0*)" : "KATUP: OFFLINE");
    k.valve = v;
  }

  // ---- 7. Status link UART (hanya saat berubah) ----
  int8_t l = conn ? 1 : 2;
  if (l != k.link) {
    tft.setTextSize(1);
    tft.setTextColor(conn ? COLOR_TEXT_MUTED : COLOR_DANGER, COLOR_CARD_BG);
    tft.setCursor(x0 + 6, 122);
    tft.print(conn ? "LINK UART: AKTIF " : "LINK UART: MATI! ");
    k.link = l;
    k.buzz = 0;                                     // Paksa gambar ulang penanda buzzer
  }

  // ---- 8. Penanda buzzer ----
  int8_t bz = (conn && b.buzzer) ? 2 : 1;
  if (bz != k.buzz) {
    tft.setTextSize(1);
    tft.setTextColor(bz == 2 ? COLOR_WHITE : COLOR_CARD_BG, bz == 2 ? COLOR_DANGER : COLOR_CARD_BG);
    tft.setCursor(x0 + 114, 122);
    tft.print(bz == 2 ? "BUZZ!" : "     ");
    k.buzz = bz;
  }

  k.blink = alarmBlinkState;
}

void drawDynamicPage0() {
  tft.setTextSize(1);

  // ---- Header: pill setpoint ----
  char currentSetPStr[26];
  snprintf(currentSetPStr, sizeof(currentSetPStr), "TH:%3.1fB/%3dC", setpointPressure, (int)setpointTemp);
  if (strcmp(currentSetPStr, cache0.setpointStr) != 0) {
    tft.fillRoundRect(126, 3, 108, 14, 3, COLOR_PILL_DARK);
    tft.setTextColor(COLOR_GOLD, COLOR_PILL_DARK);
    tft.setCursor(130, 6);
    tft.print(currentSetPStr);
    strcpy(cache0.setpointStr, currentSetPStr);
  }

  // ---- Header: pill WiFi (jumlah klien hotspot / status router) ----
  bool staOk = ENABLE_STA_ROUTER && (WiFi.status() == WL_CONNECTED);
  int  apN   = WiFi.softAPgetStationNum();
  int  key   = staOk ? 100 : apN;
  if (key != cache0.apN) {
    tft.fillRoundRect(240, 3, 74, 14, 3, COLOR_PILL_DARK);
    tft.setTextColor(staOk ? COLOR_NORMAL : (apN > 0 ? COLOR_CYAN : COLOR_TEXT_MUTED), COLOR_PILL_DARK);
    tft.setCursor(244, 6);
    if (staOk) tft.print("[WIFI:OK]");
    else       tft.printf("[AP:%d]", apN);
    cache0.apN = key;
  }

  // ---- Dua kartu boiler ----
  drawBoilerCard(0, boiler1, 3);
  drawBoilerCard(1, boiler2, 163);

  // ---- Footer: label PB3 berubah sesuai status purge ----
  int8_t lbl = purgeAllActive ? 2 : 1;
  if (lbl != cache0.purgeLbl) {
    tft.setTextColor(purgeAllActive ? COLOR_NORMAL : COLOR_DANGER, COLOR_HEADER_BG);
    tft.setCursor(195, 157);
    tft.print(purgeAllActive ? "[PB3:Tutup]" : "[PB3:Purge]");
    cache0.purgeLbl = lbl;
  }
}

// ==============================================================================
// HALAMAN 1: DETAIL GAUGE & ANALOG TELEMETRY (320 x 172)
// ==============================================================================
void drawStaticPage1() {
  tft.fillRect(0, 0, 320, 20, COLOR_HEADER_BG);
  tft.drawFastHLine(0, 20, 320, COLOR_GOLD);

  tft.setTextSize(1);
  tft.setTextColor(COLOR_WHITE, COLOR_HEADER_BG);
  tft.setCursor(6, 6);
  tft.print("DIAGNOSTIK & BAR GAUGE");

  tft.setTextColor(COLOR_GOLD, COLOR_HEADER_BG);
  tft.setCursor(295, 6);
  tft.print("2/3");

  // Bingkai Bar 1: Tekanan B1
  tft.drawRoundRect(10, 42, 300, 8, 2, COLOR_CARD_BORDER);
  // Bingkai Bar 2: Suhu B1
  tft.drawRoundRect(10, 72, 300, 8, 2, COLOR_CARD_BORDER);
  // Bingkai Bar 3: Tekanan B2
  tft.drawRoundRect(10, 102, 300, 8, 2, COLOR_CARD_BORDER);
  // Bingkai Bar 4: Suhu B2
  tft.drawRoundRect(10, 132, 300, 8, 2, COLOR_CARD_BORDER);

  // Footer
  tft.fillRect(0, 153, 320, 19, COLOR_HEADER_BG);
  tft.drawFastHLine(0, 152, 320, COLOR_CARD_BORDER);
  tft.setTextColor(COLOR_TEXT_MUTED, COLOR_HEADER_BG);
  tft.setCursor(10, 157);
  tft.print("[PB1]Hal  [PB2]Mute  [PB3]Purge/Tutup");
}

// Satu baris gauge: teks nilai + bar horizontal (diferensial, tanpa kedip)
void drawMetricRow(uint8_t r, int yText, const char *label, uint16_t labelCol,
                   bool conn, float v, bool isTemp, float maxV, float thr, uint16_t barBase) {
  RowCache &rc = cache1.row[r];
  const int yBar = yText + 16;

  // Teks: "B1 TEKANAN: 4.25 Bar  TH:4.0"
  char cur[28];
  if (!conn) {
    strcpy(cur, "--");
  } else if (isTemp) {
    if (v >= 900.0f) snprintf(cur, sizeof(cur), "ERR  TH:%d", (int)thr);
    else             snprintf(cur, sizeof(cur), "%d C  TH:%d", (int)constrain(v, -99.0f, 899.0f), (int)thr);
  } else {
    snprintf(cur, sizeof(cur), "%4.2f Bar  TH:%3.1f", constrain(v, 0.0f, 99.99f), thr);
  }
  if (strcmp(cur, rc.s) != 0) {
    tft.setCursor(10, yText);
    tft.setTextColor(labelCol, COLOR_BG);
    tft.print(label);
    tft.setTextColor(COLOR_WHITE, COLOR_BG);
    tft.print(cur);
    tft.print("        ");                 // Hapus sisa karakter jika teks memendek
    strncpy(rc.s, cur, sizeof(rc.s) - 1);
    rc.s[sizeof(rc.s) - 1] = 0;
  }

  // Bar
  int w = conn ? constrain((int)((v / maxV) * 296), 0, 296) : 0;
  uint16_t col = !conn ? COLOR_TEXT_MUTED
               : (v >= thr) ? COLOR_DANGER
               : (v >= thr * 0.85f) ? COLOR_WARNING : barBase;
  if (w != rc.w || col != rc.col) {
    if (rc.w < 0 || col != rc.col) {
      if (w > 0)   tft.fillRect(12, yBar, w, 4, col);
      if (w < 296) tft.fillRect(12 + w, yBar, 296 - w, 4, COLOR_BG);
    } else if (w > rc.w) {
      tft.fillRect(12 + rc.w, yBar, w - rc.w, 4, col);
    } else {
      tft.fillRect(12 + w, yBar, rc.w - w, 4, COLOR_BG);
    }
    rc.w = w;
    rc.col = col;
  }
}

void drawDynamicPage1() {
  tft.setTextSize(1);
  drawMetricRow(0, 28,  "B1 TEKANAN: ", COLOR_CYAN,  boiler1.isConnected, boiler1.pressure, false, 8.0f,   setpointPressure, COLOR_CYAN);
  drawMetricRow(1, 58,  "B1 SUHU   : ", COLOR_AMBER, boiler1.isConnected, boiler1.temp,     true,  200.0f, setpointTemp,     COLOR_AMBER);
  drawMetricRow(2, 88,  "B2 TEKANAN: ", COLOR_CYAN,  boiler2.isConnected, boiler2.pressure, false, 8.0f,   setpointPressure, COLOR_CYAN);
  drawMetricRow(3, 118, "B2 SUHU   : ", COLOR_AMBER, boiler2.isConnected, boiler2.temp,     true,  200.0f, setpointTemp,     COLOR_AMBER);
}

// ==============================================================================
// HALAMAN 2: NETWORK & SYSTEM INFO (320 x 172)
// ==============================================================================
void drawStaticPage2() {
  tft.fillRect(0, 0, 320, 20, COLOR_HEADER_BG);
  tft.drawFastHLine(0, 20, 320, COLOR_NORMAL);

  tft.setTextSize(1);
  tft.setTextColor(COLOR_WHITE, COLOR_HEADER_BG);
  tft.setCursor(6, 6);
  tft.print("KONEKSI IOT & SCADA WEB");

  tft.setTextColor(COLOR_NORMAL, COLOR_HEADER_BG);
  tft.setCursor(295, 6);
  tft.print("3/3");

  // Kolom Kiri: WiFi Hotspot (X: 8, Y: 26, W: 148, H: 122)
  tft.fillRoundRect(8, 26, 148, 122, 5, COLOR_CARD_BG);
  tft.drawRoundRect(8, 26, 148, 122, 5, COLOR_CARD_BORDER);
  tft.setCursor(14, 32);
  tft.setTextColor(COLOR_CYAN, COLOR_CARD_BG);
  tft.print("WIFI ACCESS POINT");
  tft.drawFastHLine(12, 42, 140, COLOR_CARD_BORDER);

  tft.setCursor(14, 48); tft.setTextColor(COLOR_TEXT_MUTED, COLOR_CARD_BG); tft.print("SSID:");
  tft.setCursor(14, 58); tft.setTextColor(COLOR_WHITE, COLOR_CARD_BG);      tft.print(AP_SSID);

  tft.setCursor(14, 72); tft.setTextColor(COLOR_TEXT_MUTED, COLOR_CARD_BG); tft.print("PASS:");
  tft.setCursor(14, 82); tft.setTextColor(COLOR_WHITE, COLOR_CARD_BG);      tft.print(AP_PASS);

  tft.setCursor(14, 96);  tft.setTextColor(COLOR_TEXT_MUTED, COLOR_CARD_BG); tft.print("IP WEB SCADA:");
  tft.setCursor(14, 108); tft.setTextColor(COLOR_NORMAL, COLOR_CARD_BG);     tft.print("http://192.168.4.1");
  tft.setCursor(14, 122); tft.setTextColor(COLOR_TEXT_MUTED, COLOR_CARD_BG); tft.print("Port: 80 (Aktif)");

  // Kolom Kanan: MQTT & Status (X: 164, Y: 26, W: 148, H: 122)
  tft.fillRoundRect(164, 26, 148, 122, 5, COLOR_CARD_BG);
  tft.drawRoundRect(164, 26, 148, 122, 5, COLOR_CARD_BORDER);
  tft.setCursor(170, 32);
  tft.setTextColor(COLOR_AMBER, COLOR_CARD_BG);
  tft.print("MQTT & TELEMETRI");
  tft.drawFastHLine(168, 42, 140, COLOR_CARD_BORDER);

  tft.setCursor(170, 48); tft.setTextColor(COLOR_TEXT_MUTED, COLOR_CARD_BG); tft.print("BROKER HOST:");
  tft.setCursor(170, 58); tft.setTextColor(COLOR_WHITE, COLOR_CARD_BG);      tft.print(MQTT_BROKER);

  tft.setCursor(170, 72); tft.setTextColor(COLOR_TEXT_MUTED, COLOR_CARD_BG); tft.print("MQTT STATUS:");
  tft.setCursor(170, 96); tft.setTextColor(COLOR_TEXT_MUTED, COLOR_CARD_BG); tft.print("UPTIME MASTER:");

  // Footer
  tft.fillRect(0, 153, 320, 19, COLOR_HEADER_BG);
  tft.drawFastHLine(0, 152, 320, COLOR_CARD_BORDER);
  tft.setTextColor(COLOR_TEXT_MUTED, COLOR_HEADER_BG);
  tft.setCursor(10, 157);
  tft.print("[PB1]Hal  [PB2]Mute  [PB3]Purge/Tutup");
}

void drawDynamicPage2() {
  tft.setTextSize(1);

  // Status MQTT (digambar saat pertama kali & saat status berubah)
  bool isMqttNow = mqttClient.connected();
  if ((int8_t)isMqttNow != cache2.mqttState) {
    tft.setCursor(170, 82);
    if (isMqttNow) {
      tft.setTextColor(COLOR_NORMAL, COLOR_CARD_BG);
      tft.print("SYNC (TERHUBUNG) ");
    } else {
      tft.setTextColor(COLOR_TEXT_MUTED, COLOR_CARD_BG);
      tft.print("STANDALONE (OFF) ");
    }
    cache2.mqttState = (int8_t)isMqttNow;
  }

  // Jumlah klien yang terhubung ke hotspot ESP32
  int apN = WiFi.softAPgetStationNum();
  if (apN != cache2.apN) {
    tft.setCursor(14, 134);
    tft.setTextColor(COLOR_TEXT_MUTED, COLOR_CARD_BG);
    tft.print("KLIEN WIFI: ");
    tft.setTextColor(apN > 0 ? COLOR_NORMAL : COLOR_TEXT_MUTED, COLOR_CARD_BG);
    tft.printf("%-3d", apN);
    cache2.apN = apN;
extColor(COLOR_TEXT_MUTED, COLOR_CARD_BG);
      tft.print("STANDALONE (OFF) ");
    }
    cache2.mqttConn = isMqttNow;
  }

  // Uptime (Hanya update sekali per detik)
  unsigned long sec = millis() / 1000;
  if (sec != cache2.lastSec) {
    char bufUp[20];
    snprintf(bufUp, sizeof(bufUp), "%02lu:%02lu:%02lu", sec / 3600, (sec % 3600) / 60, sec % 60);
    tft.setCursor(170, 108);
    tft.setTextColor(COLOR_WHITE, COLOR_CARD_BG);
    tft.print(bufUp);
    cache2.lastSec = sec;
  }

  // Info RV5 Potensio
  char curRv5[16];
  snprintf(curRv5, sizeof(curRv5), "%3.1f Bar", setpointPressure);
  if (strcmp(curRv5, cache2.rv5Str) != 0) {
    tft.setCursor(170, 126);
    tft.setTextColor(COLOR_CYAN, COLOR_CARD_BG);
    tft.print("SET: ");
    tft.setTextColor(COLOR_WHITE, COLOR_CARD_BG);
    tft.print(curRv5);
    tft.print("   ");
    strcpy(cache2.rv5Str, curRv5);
  }
}

// ------------------------------------------------------------------------------
// BACA POTENSIOMETER SETPOINT RV5 (GPIO 04) & SINKRON KE SLAVE
// - Oversampling 16x + low-pass filter -> tidak goyang walau pot kotor/noise
// - Dikuantisasi 0.1 Bar, histeresis 0.15 Bar -> tidak bolak-balik di tepi step
// ------------------------------------------------------------------------------
float setpointFiltered = -1.0;

void readSetpointPot() {
  if (millis() - lastAdcRead < 50) return;
  lastAdcRead = millis();

  uint32_t sum = 0;
  for (uint8_t i = 0; i < 16; i++) sum += analogRead(PIN_VR_SETPOINT);
  float raw = sum / 16.0f;
  setpointFiltered = (setpointFiltered < 0) ? raw : (setpointFiltered * 0.8f + raw * 0.2f);

  float ratio = constrain(setpointFiltered / 4095.0f, 0.0f, 1.0f);
  float mappedPressure = roundf((1.0f + ratio * 7.0f) * 10.0f) / 10.0f;   // 1.0 - 8.0 Bar
  float mappedTemp     = roundf(100.0f + ratio * 100.0f);                 // 100 - 200 C

  if (lastSentSetpoint < 0 || fabsf(mappedPressure - lastSentSetpoint) >= 0.15f) {
    setpointPressure = mappedPressure;
    setpointTemp     = mappedTemp;
    lastSentSetpoint = mappedPressure;
    sendSetpointToSlaves();
  }
}

// ------------------------------------------------------------------------------
// ANTRIAN PERINTAH DENGAN PENGULANGAN
// SoftwareSerial di Nano tidak bisa menerima saat sedang mengirim telemetri,
// sehingga perintah bisa hilang. Perintah (PURGE/MUTE) bersifat idempoten,
// jadi dikirim 3x (0 ms, 150 ms, 300 ms). Perintah SET dikirim berkala (heartbeat).
// ------------------------------------------------------------------------------
struct PendingCmd {
  int8_t        target;
  char          cmd[16];
  uint8_t       repeatsLeft;    // 0 = slot kosong
  unsigned long nextAt;
};
PendingCmd cmdQueue[6];
unsigned long lastHeartbeat = 0;

void txRaw(int targetId, const char *cmd) {
  char pkt[40];
  snprintf(pkt, sizeof(pkt), "#CMD%d,%s*\n", targetId, cmd);
  if (targetId == 1 || targetId == 0) Serial1.print(pkt);
  if (targetId == 2 || targetId == 0) Serial2.print(pkt);
}

void sendCommandToSlave(int targetId, String cmd) {
  if (targetId < 0 || targetId > 2 || cmd.length() == 0 || cmd.length() >= 16) return;
  txRaw(targetId, cmd.c_str());
  if (cmd.startsWith("SET,")) return;               // SET tidak perlu diulang (ada heartbeat)

  // Batalkan sisa pengulangan perintah sejenis yang menimpa target ini
  // (mencegah "PURGE,1" lama terkirim ulang setelah "PURGE,0" baru).
  bool isPurge = cmd.startsWith("PURGE");
  for (auto &q : cmdQueue) {
    if (!q.repeatsLeft) continue;
    bool sameFamily = isPurge ? (strncmp(q.cmd, "PURGE", 5) == 0) : (strncmp(q.cmd, "MUTE", 4) == 0);
    bool overlap = (q.target == targetId) || q.target == 0 || targetId == 0;
    if (sameFamily && overlap) q.repeatsLeft = 0;
  }
  for (auto &q : cmdQueue) {
    if (q.repeatsLeft) continue;
    q.target = targetId;
    strncpy(q.cmd, cmd.c_str(), sizeof(q.cmd) - 1);
    q.cmd[sizeof(q.cmd) - 1] = 0;
    q.repeatsLeft = 2;
    q.nextAt = millis() + 150;
    break;
  }
}

void sendSetpointToSlaves() {
  char buf[16];
  snprintf(buf, sizeof(buf), "SET,%.1f,%d", setpointPressure, (int)setpointTemp);
  txRaw(0, buf);
  lastHeartbeat = millis();
}

void serviceCommandQueue() {
  unsigned long now = millis();
  for (auto &q : cmdQueue) {
    if (q.repeatsLeft && (long)(now - q.nextAt) >= 0) {
      txRaw(q.target, q.cmd);
      q.repeatsLeft--;
      q.nextAt = now + 150;
    }
  }
  // Heartbeat: kirim setpoint tiap 1 detik -> slave yang baru menyala / terlewat
  // paket tetap sinkron, dan slave bisa mendeteksi link Master hidup.
  if (now - lastHeartbeat >= 1000) sendSetpointToSlaves();
}

// ------------------------------------------------------------------------------
// PENANGANAN DUAL UART: MEMBACA SLAVE 1 & SLAVE 2 (buffer char[], tanpa String)
// ------------------------------------------------------------------------------
void pumpUart(HardwareSerial &port, int id, char *buf, uint8_t &len, bool &over) {
  while (port.available() > 0) {
    char c = port.read();
    if (c == '\n' || c == '*') {
      if (!over && len > 0) {
        buf[len] = 0;
        parseSlavePacket(id, buf);
      }
      len = 0;
      over = false;
    } else if (c != '\r') {
      if (len < RX_MAX - 1) buf[len++] = c;
      else over = true;                              // Paket rusak/terlalu panjang -> dibuang
    }
  }
}

void markOffline(BoilerData &b) {
  b.state  = "OFFLINE";
  b.valve  = "UNKNOWN";
  b.buzzer = false;
}

void handleDualUart() {
  pumpUart(Serial1, 1, rx1, rx1Len, rx1Over);
  pumpUart(Serial2, 2, rx2, rx2Len, rx2Over);

  // Timeout koneksi: 3 detik tanpa paket valid = putus. Sebelum paket pertama = putus.
  unsigned long now = millis();
  bool c1 = (boiler1.packetCount > 0) && (now - boiler1.lastSeen < 3000);
  bool c2 = (boiler2.packetCount > 0) && (now - boiler2.lastSeen < 3000);
  if (boiler1.isConnected && !c1) markOffline(boiler1);
  if (boiler2.isConnected && !c2) markOffline(boiler2);
  boiler1.isConnected = c1;
  boiler2.isConnected = c2;
}

// ------------------------------------------------------------------------------
// PARSER PAKET TELEMETRI DARI SLAVE (dengan validasi ketat)
// Format: #SLVx,P,T,SrcP,SrcT,Valve,Buzzer,State
// ------------------------------------------------------------------------------
void parseSlavePacket(int slaveId, char *pkt) {
  if (strncmp(pkt, "#SLV", 4) != 0) return;
  if ((pkt[4] - '0') != slaveId) return;             // Salah kabel / paket tercampur

  char *f[8];
  uint8_t n = 0;
  char *p = pkt;
  f[n++] = p;
  while (*p && n < 8) {
    if (*p == ',') { *p = 0; f[n++] = p + 1; }
    p++;
  }
  if (n != 8) return;                                // Jumlah field harus tepat 8

  float pr = atof(f[1]);
  float tc = atof(f[2]);
  if (pr < -1.0f || pr > 30.0f || tc < -100.0f || tc > 2000.0f) return;

  bool srcOk = (!strcmp(f[3], "REAL") || !strcmp(f[3], "SIM")) &&
               (!strcmp(f[4], "REAL") || !strcmp(f[4], "SIM"));
  bool valveOk = (!strcmp(f[5], "OPEN") || !strcmp(f[5], "CLOSED"));
  bool stateOk = (!strcmp(f[7], "NORMAL") || !strcmp(f[7], "WARNING") || !strcmp(f[7], "ALARM"));
  if (!srcOk || !valveOk || !stateOk) return;

  BoilerData &b = (slaveId == 1) ? boiler1 : boiler2;
  b.pressure    = pr;
  b.temp        = tc;
  b.srcPressure = f[3];
  b.srcTemp     = f[4];
  b.valve       = f[5];
  b.buzzer      = (f[6][0] == '1');
  b.state       = f[7];
  b.lastSeen    = millis();
  b.packetCount++;
}

// ------------------------------------------------------------------------------
// AKSI GLOBAL
// ------------------------------------------------------------------------------
void emergencyPurgeAll() {
  purgeAllActive = !purgeAllActive;                  // Toggle: tekan lagi untuk menutup katup
  sendCommandToSlave(0, purgeAllActive ? "PURGE,1" : "PURGE,0");
}

void silenceAllAlarms() {
  sendCommandToSlave(0, "MUTE,1");
}

// ------------------------------------------------------------------------------
// PENANGANAN TOMBOL FISIK MASTER (deteksi tepi + debounce)
// ------------------------------------------------------------------------------
bool buttonPressed(MasterButton &b) {
  bool raw = (digitalRead(b.pin) == LOW);
  unsigned long now = millis();
  if (raw != b.lastRaw) {
    b.lastRaw = raw;
    b.changedAt = now;
  }
  if ((now - b.changedAt) >= 30 && raw != b.stable) {
    b.stable = raw;
    if (b.stable) return true;
  }
  return false;
}

void handleMasterButtons() {
  if (buttonPressed(btnPB1)) {
    tftScreenPage = (tftScreenPage + 1) % 3;         // 0 -> 1 -> 2 -> 0
    forceFullRedraw = true;
  }
  if (buttonPressed(btnPB2)) silenceAllAlarms();
  if (buttonPressed(btnPB3)) emergencyPurgeAll();
}

// ------------------------------------------------------------------------------
// SETUP WEBSERVER STANDALONE & REST API
// ------------------------------------------------------------------------------
String boilerJson(const BoilerData &b) {
  String j = "{";
  j += "\"pressure\":" + String(b.pressure, 2) + ",";
  j += "\"temp\":" + String(b.temp, 1) + ",";
  j += "\"src_p\":\"" + b.srcPressure + "\",";
  j += "\"src_t\":\"" + b.srcTemp + "\",";
  j += "\"valve\":\"" + b.valve + "\",";
  j += "\"buzzer\":" + String(b.buzzer ? "true" : "false") + ",";
  j += "\"state\":\"" + b.state + "\",";
  j += "\"connected\":" + String(b.isConnected ? "true" : "false");
  j += "}";
  return j;
}

void setupWebServer() {
  server.on("/", HTTP_GET, []() {
    server.sendHeader("Cache-Control", "no-cache");
    server.send_P(200, "text/html", INDEX_HTML);
  });

  server.on("/api/data", HTTP_GET, []() {
    String json = "{";
    json += "\"setpoint_p\":" + String(setpointPressure, 2) + ",";
    json += "\"setpoint_t\":" + String(setpointTemp, 1) + ",";
    json += "\"purge_all\":" + String(purgeAllActive ? "true" : "false") + ",";
    json += "\"ap_clients\":" + String(WiFi.softAPgetStationNum()) + ",";
    json += "\"uptime\":" + String(millis() / 1000) + ",";
    json += "\"slave1\":" + boilerJson(boiler1) + ",";
    json += "\"slave2\":" + boilerJson(boiler2);
    json += "}";
    server.sendHeader("Cache-Control", "no-store");
    server.send(200, "application/json", json);
  });

  // Perintah dibatasi whitelist agar tidak ada data liar yang masuk ke UART slave
  server.on("/api/command", HTTP_GET, []() {
    if (!server.hasArg("node") || !server.hasArg("action")) {
      server.send(400, "text/plain", "Missing node/action");
      return;
    }
    int node = server.arg("node").toInt();
    String action = server.arg("action");
    bool okAction = (action == "PURGE,1" || action == "PURGE,0" || action == "MUTE,1");
    if (node < 0 || node > 2 || !okAction) {
      server.send(400, "text/plain", "Invalid command");
      return;
    }
    if (node == 0 && action.startsWith("PURGE")) purgeAllActive = (action == "PURGE,1");
    sendCommandToSlave(node, action);
    server.send(200, "text/plain", "OK");
  });

  server.onNotFound([]() { server.send(404, "text/plain", "Not found"); });
  server.begin();
}

// ------------------------------------------------------------------------------
// PENGIRIMAN DATA TELEMETRI KE MQTT BROKER (hanya jika STA tersambung)
// Non-blocking: percobaan koneksi dibatasi tiap 5 detik, timeout socket 2 detik.
// ------------------------------------------------------------------------------
unsigned long lastMqttTry = 0;

void sendMqttTelemetry() {
  if (!ENABLE_STA_ROUTER || WiFi.status() != WL_CONNECTED) return;

  if (!mqttClient.connected()) {
    if (millis() - lastMqttTry >= 5000) {
      lastMqttTry = millis();
      mqttClient.connect("ESP32S3_Master_Boiler");
    }
    return;
  }
  mqttClient.loop();                                 // Wajib: keep-alive MQTT

  if (millis() - lastMqttSend >= 1000) {
    lastMqttSend = millis();
    const BoilerData *bs[2] = {&boiler1, &boiler2};
    for (int i = 0; i < 2; i++) {
      String p = "{\"node\":\"SLAVE_" + String(i + 1) + "\",\"pressure\":" + String(bs[i]->pressure, 2) +
                 ",\"temp\":" + String(bs[i]->temp, 1) +
                 ",\"valve\":\"" + bs[i]->valve + "\",\"state\":\"" + bs[i]->state + "\"}";
      mqttClient.publish(i == 0 ? "boiler/telemetry/slave1" : "boiler/telemetry/slave2", p.c_str());
    }
  }
}

// ------------------------------------------------------------------------------
// SETUP MASTER
// ------------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);

  // Dual Hardware UART: Serial1 -> Slave 1, Serial2 -> Slave 2
  Serial1.begin(9600, SERIAL_8N1, PIN_UART1_RX, PIN_UART1_TX);
  Serial2.begin(9600, SERIAL_8N1, PIN_UART2_RX, PIN_UART2_TX);

  pinMode(PIN_PB1_MASTER, INPUT_PULLUP);
  pinMode(PIN_PB2_MASTER, INPUT_PULLUP);
  pinMode(PIN_PB3_MASTER, INPUT_PULLUP);

  analogReadResolution(12);
  analogSetPinAttenuation(PIN_VR_SETPOINT, ADC_11db);

  initTft();

  // ---- WiFi ----
  WiFi.persistent(false);
  WiFi.disconnect(true);
  delay(100);

  if (ENABLE_STA_ROUTER) {
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(AP_SSID, AP_PASS);
    WiFi.setSleep(false);
    Serial.println("[WIFI] Mencoba koneksi ke Router STA: " + String(STA_SSID));
    WiFi.begin(STA_SSID, STA_PASS);

    // Jika router tidak ada, kembali ke AP murni agar tidak channel-hopping (putus-nyambung)
    unsigned long startConn = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startConn < 6000) {
      delay(200);
      Serial.print(".");
    }
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\n[WIFI] Terhubung ke Router! IP STA: " + WiFi.localIP().toString());
    } else {
      Serial.println("\n[WIFI] Router tidak merespon! Beralih ke murni Hotspot AP.");
      WiFi.disconnect(true);
      WiFi.mode(WIFI_AP);
      WiFi.softAP(AP_SSID, AP_PASS);
    }
  } else {
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID, AP_PASS);
    WiFi.setSleep(false);
    Serial.println("[WIFI] Mode Hotspot Mandiri Aktif. SSID: " + String(AP_SSID));
  }

  setupWebServer();

  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  mqttClient.setSocketTimeout(2);                    // Cegah loop() macet lama saat broker tidak ada

  readSetpointPot();                                 // Baca & kirim setpoint awal
}

// ------------------------------------------------------------------------------
// LOOP UTAMA
// ------------------------------------------------------------------------------
void loop() {
  handleDualUart();
  readSetpointPot();
  handleMasterButtons();
  serviceCommandQueue();
