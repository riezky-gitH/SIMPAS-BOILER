// ==============================================================================
// FIRMWARE SLAVE 2: ARDUINO NANO (BOILER CHAMBER 2)
// Proyek: Sistem Monitoring Tekanan & Suhu dengan Kontrol Solenoid Otomatis
// Mata Kuliah: Workshop Mikrokontroler Lanjut dan Teknik Antarmuka (WMLTA)
//
// PRINSIP DESAIN (v2 - anti bug & anti tabrakan fitur):
//  1. Satu sumber kebenaran: variabel flag (alarmActive, manualPurge, buzzerMuted).
//     Relay/LED/Buzzer HANYA ditulis di applyActuators(). Tombol & UART cuma
//     mengubah flag, tidak pernah menyentuh pin aktuator langsung.
//  2. Prioritas katup:  ALARM (otomatis)  >  MANUAL PURGE  >  TUTUP.
//     Katup terbuka bila (alarmActive || manualPurge).
//  3. Buzzer menyala KONTINU selama alarmActive dan belum di-mute (PB2 / web / master).
//     Mute hanya berlaku untuk alarm yang sedang berjalan; alarm baru -> bunyi lagi.
//  4. Tombol memakai deteksi tepi (1 tekan = 1 aksi, tahan tombol tidak mengulang).
//  5. LCD hanya menulis karakter yang berubah (tanpa lcd.clear -> tidak berkedip),
//     setiap baris selalu tepat 16 karakter.
//  6. UART: buffer char[] terbatas (tanpa String -> tidak ada fragmentasi heap),
//     paket telemetri dikirim sekali tulis, ada watchdog link Master.
// ==============================================================================

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <SoftwareSerial.h>

// ------------------------------------------------------------------------------
// IDENTITAS SLAVE
// ------------------------------------------------------------------------------
#define SLAVE_ID          2
#define SLAVE_NAME        "BOILER 2"     // Maksimal 8 karakter (agar muat di LCD)

// ------------------------------------------------------------------------------
// DEFINISI PIN HARDWARE (SESUAI REVISI KEBUTUHAN PIN)
// ------------------------------------------------------------------------------
#define PIN_SOFTSERIAL_RX 2    // D2 <- dari ESP32 Master
#define PIN_SOFTSERIAL_TX 3    // D3 -> ke ESP32 Master (via divider)

#define PIN_LED           4    // D4: LED indikator aktuator (ON = katup BUKA)
#define PIN_BUZZER        5    // D5: Driver transistor NPN buzzer
#define PIN_RELAY_VALVE   9    // D9: Modul Relay Katup Solenoid (Pengganti Servo)

// Logika Aktif Relay (Modul Relay Arduino 5V umumnya ACTIVE-LOW)
// Jika modul Anda Active-HIGH, cukup balik definisi ini (ON = HIGH, OFF = LOW)
#define RELAY_ON          LOW   // Relay ON / Kontak NO menutup -> Katup Terbuka
#define RELAY_OFF         HIGH  // Relay OFF / Kontak NO membuka -> Katup Tertutup

#define PIN_PB1           6    // D6: PB1 = Manual Purge (toggle)
#define PIN_PB2           7    // D7: PB2 = Mute buzzer
#define PIN_PB3           8    // D8: PB3 = Ganti halaman LCD

#define PIN_MAX6675_CS    10   // D10: CS MAX6675
#define PIN_MAX6675_SO    12   // D12: MISO MAX6675
#define PIN_MAX6675_SCK   A0   // A0: Clock MAX6675 (D13 rusak -> dipindah ke A0)

#define PIN_SPDT1_SUHU    11   // D11: LOW = MAX6675 (real), HIGH = potensio (sim)
#define PIN_SPDT2_TEKANAN A3   // A3 : LOW = transmitter (real), HIGH = potensio (sim)

#define PIN_PRESSURE_REAL A6   // A6: Transmitter 0.5 - 4.5 V = 0 - 10 Bar (pindah dari A0)
#define PIN_TEMP_SIM      A1   // A1: VR2 simulasi suhu 0 - 200 C
#define PIN_PRESSURE_SIM  A2   // A2: VR1 simulasi tekanan 0 - 10 Bar

// ------------------------------------------------------------------------------
// OBJEK DRIVER
// ------------------------------------------------------------------------------
LiquidCrystal_I2C lcd(0x27, 16, 2);   // Ganti ke 0x3F bila LCD tidak tampil
SoftwareSerial masterSerial(PIN_SOFTSERIAL_RX, PIN_SOFTSERIAL_TX);

// ------------------------------------------------------------------------------
// PARAMETER
// ------------------------------------------------------------------------------
const float HYST_PRESSURE = 0.20;          // Alarm reset bila P < TH - 0.2 Bar
const float HYST_TEMP     = 3.0;           // Alarm reset bila T < TH - 3 C
const unsigned long SENSOR_PERIOD_MS = 250; // MAX6675 butuh >= 220 ms per konversi
const unsigned long LCD_PERIOD_MS    = 200;
const unsigned long TX_PERIOD_MS     = 500;
const unsigned long LINK_TIMEOUT_MS  = 5000;
const unsigned long DEBOUNCE_MS      = 30;

// ------------------------------------------------------------------------------
// STATUS SISTEM
// ------------------------------------------------------------------------------
float pressure_bar   = 0.0;
float temperature_c  = 0.0;
bool  is_pressure_sim = false;
bool  is_temp_sim     = false;
bool  tempFault       = false;   // Termokopel putus / data tidak valid

float threshold_pressure = 4.0;   // Bar   (disinkronkan dari Master)
float threshold_temp     = 160.0; // Celcius

enum SystemState { STATE_NORMAL, STATE_WARNING, STATE_ALARM };
SystemState currentState = STATE_NORMAL;

bool alarmActive  = false;   // Kondisi melebihi batas (latched + hysteresis)
bool manualPurge  = false;   // Katup dibuka manual (PB1 / web / master)
bool buzzerMuted  = false;   // Operator sudah mematikan buzzer untuk alarm ini
bool valveOpen    = false;   // Hasil akhir: posisi katup
bool buzzerActive = false;   // Hasil akhir: buzzer berbunyi

uint8_t lcdMode = 0;         // 0: Monitor, 1: Aktuator, 2: Komunikasi

unsigned long lastSensorMs = 0;
unsigned long lastLcdMs    = 0;
unsigned long lastTxMs     = 0;
unsigned long lastMasterRx = 0;
bool masterSeen = false;     // true setelah minimal 1 perintah valid diterima

// ------------------------------------------------------------------------------
// TOMBOL (deteksi tepi + debounce)
// ------------------------------------------------------------------------------
struct Button {
  uint8_t pin;
  bool stable;               // true = ditekan (stabil)
  bool lastRaw;
  unsigned long changedAt;
};
Button btnPB1 = {PIN_PB1, false, false, 0};
Button btnPB2 = {PIN_PB2, false, false, 0};
Button btnPB3 = {PIN_PB3, false, false, 0};

// Mengembalikan true SATU KALI saat tombol baru saja ditekan
bool buttonPressed(Button &b) {
  bool raw = (digitalRead(b.pin) == LOW);
  unsigned long now = millis();
  if (raw != b.lastRaw) {
    b.lastRaw = raw;
    b.changedAt = now;
  }
  if ((now - b.changedAt) >= DEBOUNCE_MS && raw != b.stable) {
    b.stable = raw;
    if (b.stable) return true;
  }
  return false;
}

// ------------------------------------------------------------------------------
// SENSOR
// ------------------------------------------------------------------------------
// Mengembalikan false bila termokopel putus / data tidak valid
bool readMAX6675(float &outC) {
  digitalWrite(PIN_MAX6675_CS, LOW);
  delayMicroseconds(2);
  uint16_t raw = 0;
  for (uint8_t i = 0; i < 16; i++) {
    digitalWrite(PIN_MAX6675_SCK, HIGH);
    delayMicroseconds(2);
    raw = (raw << 1) | (digitalRead(PIN_MAX6675_SO) ? 1 : 0);
    digitalWrite(PIN_MAX6675_SCK, LOW);
    delayMicroseconds(2);
  }
  digitalWrite(PIN_MAX6675_CS, HIGH);

  if (raw == 0xFFFF) return false;   // Bus mengambang / modul tidak terpasang
  if (raw & 0x0004)  return false;   // Bit D2: termokopel terbuka
  outC = (raw >> 3) * 0.25;
  return true;
}

// Rata-rata 8 sampel ADC (meredam noise)
int readAdcAvg(uint8_t pin) {
  long sum = 0;
  for (uint8_t i = 0; i < 8; i++) sum += analogRead(pin);
  return (int)(sum >> 3);
}

float readPressureTransmitter() {
  float voltage = (readAdcAvg(PIN_PRESSURE_REAL) / 1023.0) * 5.0;
  float bar = (voltage - 0.5) * (10.0 / 4.0);
  if (bar < 0.0)  bar = 0.0;
  if (bar > 10.0) bar = 10.0;
  return bar;
}

void readSensors() {
  // --- Suhu ---
  is_temp_sim = (digitalRead(PIN_SPDT1_SUHU) == HIGH);
  if (is_temp_sim) {
    temperature_c = (readAdcAvg(PIN_TEMP_SIM) / 1023.0) * 200.0;
    tempFault = false;
  } else {
    float t;
    if (readMAX6675(t)) {
      temperature_c = t;
      tempFault = false;
    } else {
      tempFault = true;
      temperature_c = 999.9;   // Fail-safe: sensor putus dianggap BAHAYA
    }
  }

  // --- Tekanan ---
  is_pressure_sim = (digitalRead(PIN_SPDT2_TEKANAN) == HIGH);
  if (is_pressure_sim) {
    pressure_bar = (readAdcAvg(PIN_PRESSURE_SIM) / 1023.0) * 10.0;
  } else {
    pressure_bar = readPressureTransmitter();
  }
}

// ------------------------------------------------------------------------------
// LOGIKA KESELAMATAN (tanpa menyentuh pin aktuator)
// ------------------------------------------------------------------------------
void evaluateSafety() {
  bool over = (pressure_bar >= threshold_pressure) || (temperature_c >= threshold_temp);
  bool stillHigh = (pressure_bar > (threshold_pressure - HYST_PRESSURE)) ||
                   (temperature_c > (threshold_temp - HYST_TEMP));

  if (over) {
    if (!alarmActive) {          // Transisi NORMAL -> ALARM
      alarmActive = true;
      buzzerMuted = false;       // Alarm baru: buzzer wajib bunyi lagi
    }
  } else if (alarmActive && !stillHigh) {
    alarmActive = false;         // Sudah aman di bawah zona hysteresis
    buzzerMuted = false;
  }
  // (alarmActive && stillHigh && !over) -> tetap ALARM (zona hysteresis)

  if (alarmActive) {
    currentState = STATE_ALARM;
  } else if (pressure_bar >= threshold_pressure * 0.85 ||
             temperature_c >= threshold_temp * 0.90) {
    currentState = STATE_WARNING;
  } else {
    currentState = STATE_NORMAL;
  }

  valveOpen    = alarmActive || manualPurge;
  buzzerActive = alarmActive && !buzzerMuted;
}

// SATU-SATUNYA tempat yang menulis ke relay katup, LED, dan buzzer
void applyActuators() {
  static int8_t lastValve = -1;
  static int8_t lastBuzz  = -1;

  int8_t v = valveOpen ? 1 : 0;
  if (v != lastValve) {                       // Tulis relay hanya saat berubah
    digitalWrite(PIN_RELAY_VALVE, v ? RELAY_ON : RELAY_OFF);
    digitalWrite(PIN_LED, v ? HIGH : LOW);    // LED = cermin posisi katup
    lastValve = v;
  }

  int8_t b = buzzerActive ? 1 : 0;
  if (b != lastBuzz) {
    digitalWrite(PIN_BUZZER, b ? HIGH : LOW); // Kontinu, tanpa putus-putus
    lastBuzz = b;
  }
}

// ------------------------------------------------------------------------------
// TOMBOL
// ------------------------------------------------------------------------------
void handleButtons() {
  if (buttonPressed(btnPB1)) {
    manualPurge = !manualPurge;               // Toggle buang manual
  }
  if (buttonPressed(btnPB2)) {
    if (alarmActive) buzzerMuted = true;      // Mute alarm yang sedang berbunyi
  }
  if (buttonPressed(btnPB3)) {
    lcdMode = (lcdMode + 1) % 3;
    invalidateLcd();                          // Paksa gambar ulang tanpa lcd.clear()
  }
}

// ------------------------------------------------------------------------------
// LCD 16x2: DIFFERENTIAL WRITE (hanya karakter yang berubah)
// ------------------------------------------------------------------------------
char lcdShadow[2][17];

void invalidateLcd() {
  for (uint8_t r = 0; r < 2; r++) {
    for (uint8_t c = 0; c < 16; c++) lcdShadow[r][c] = 0;
    lcdShadow[r][16] = 0;
  }
}

// Isi buf dengan spasi sampai tepat 16 karakter
void pad16(char *buf) {
  uint8_t i = 0;
  while (buf[i] && i < 16) i++;
  while (i < 16) buf[i++] = ' ';
  buf[16] = 0;
}

void lcdWriteLine(uint8_t row, char *line) {
  pad16(line);
  uint8_t c = 0;
  while (c < 16) {
    if (line[c] == lcdShadow[row][c]) { c++; continue; }
    uint8_t start = c;                        // Cari rentang karakter berbeda
    while (c < 16 && line[c] != lcdShadow[row][c]) c++;
    lcd.setCursor(start, row);
    for (uint8_t k = start; k < c; k++) {
      lcd.write(line[k]);
      lcdShadow[row][k] = line[k];
    }
  }
}

void updateLcd() {
  if (millis() - lastLcdMs < LCD_PERIOD_MS) return;
  lastLcdMs = millis();

  char l1[24], l2[24], num[8];

  if (lcdMode == 0) {
    // L1: "P:3.4B[R]T:145C"  (maks 16 karakter, P sampai "10.0")
    dtostrf(pressure_bar, 1, 1, num);
    char tbuf[6];
    if (tempFault) strcpy(tbuf, "ERR"); else snprintf(tbuf, sizeof(tbuf), "%d", (int)temperature_c);
    snprintf(l1, sizeof(l1), "P:%sB[%c]T:%s%s", num, is_pressure_sim ? 'S' : 'R', tbuf, tempFault ? "" : "C");

    // L2: "V:TUTUP T[R] NRM"
    const char *vs = alarmActive ? "BUKA " : (manualPurge ? "MANU " : (valveOpen ? "BUKA " : "TUTUP"));
    const char *st = (currentState == STATE_ALARM) ? "ALM" : (currentState == STATE_WARNING ? "WRN" : "NRM");
    snprintf(l2, sizeof(l2), "V:%s T[%c] %s", vs, is_temp_sim ? 'S' : 'R', st);

  } else if (lcdMode == 1) {
    // Halaman AKTUATOR
    if (valveOpen) snprintf(l1, sizeof(l1), "KATUP:90* BUKA");
    else           snprintf(l1, sizeof(l1), "KATUP: 0* TUTUP");

    if (alarmActive) {
      snprintf(l2, sizeof(l2), buzzerMuted ? "ALARM:BUZZ MUTE" : "ALARM:BUZZ ON");
    } else if (manualPurge) {
      snprintf(l2, sizeof(l2), "MODE:MANUAL PB1");
    } else {
      dtostrf(threshold_pressure, 1, 1, num);
      snprintf(l2, sizeof(l2), "TH:%sB/%dC", num, (int)threshold_temp);
    }

  } else {
    // Halaman KOMUNIKASI
    dtostrf(threshold_pressure, 1, 1, num);
    snprintf(l1, sizeof(l1), "%s SP:%sB", SLAVE_NAME, num);
    bool linkOk = masterSeen && (millis() - lastMasterRx < LINK_TIMEOUT_MS);
    snprintf(l2, sizeof(l2), "MASTER:%s T:%dC", linkOk ? "OK" : "--", (int)threshold_temp);
  }

  lcdWriteLine(0, l1);
  lcdWriteLine(1, l2);
}

// ------------------------------------------------------------------------------
// UART KE MASTER
// ------------------------------------------------------------------------------
#define RX_BUF_SIZE 48
char rxBuf[RX_BUF_SIZE];
uint8_t rxLen = 0;
bool rxOverflow = false;

void parseMasterCommand(char *cmd) {
  // Format: #CMD<target>,<aksi>[,args]     target 0 = broadcast
  if (strncmp(cmd, "#CMD", 4) != 0) return;
  if (cmd[4] < '0' || cmd[4] > '2') return;
  int targetId = cmd[4] - '0';
  if (targetId != 0 && targetId != SLAVE_ID) return;
  if (cmd[5] != ',') return;

  char *action = cmd + 6;

  // Perintah valid dari Master -> link hidup
  lastMasterRx = millis();
  masterSeen = true;

  if (strncmp(action, "SET,", 4) == 0) {
    char *p = action + 4;
    char *comma = strchr(p, ',');
    if (!comma) return;
    *comma = 0;
    float sp = atof(p);
    float st = atof(comma + 1);
    if (sp >= 0.5 && sp <= 10.0)  threshold_pressure = sp;   // Validasi rentang
    if (st >= 50.0 && st <= 300.0) threshold_temp = st;
  } else if (strcmp(action, "PURGE,1") == 0) {
    manualPurge = true;
  } else if (strcmp(action, "PURGE,0") == 0) {
    manualPurge = false;                      // Alarm tetap menahan katup terbuka
  } else if (strcmp(action, "MUTE,1") == 0) {
    if (alarmActive) buzzerMuted = true;
  }
  // PING / lainnya hanya memperbarui watchdog link
}

void sendTelemetry() {
  char pbuf[8], tbuf[8], out[64];
  dtostrf(pressure_bar, 1, 2, pbuf);
  dtostrf(temperature_c, 1, 1, tbuf);
  snprintf(out, sizeof(out), "#SLV%d,%s,%s,%s,%s,%s,%d,%s*\n",
           SLAVE_ID, pbuf, tbuf,
           is_pressure_sim ? "SIM" : "REAL",
           is_temp_sim ? "SIM" : "REAL",
           valveOpen ? "OPEN" : "CLOSED",
           buzzerActive ? 1 : 0,
           currentState == STATE_ALARM ? "ALARM" : (currentState == STATE_WARNING ? "WARNING" : "NORMAL"));
  masterSerial.print(out);                    // Satu kali tulis -> blocking minimum
}

void handleUart() {
  while (masterSerial.available() > 0) {
    char c = masterSerial.read();
    if (c == '\n' || c == '*') {
      if (!rxOverflow && rxLen > 0) {
        rxBuf[rxLen] = 0;
        parseMasterCommand(rxBuf);
      }
      rxLen = 0;
      rxOverflow = false;
    } else if (c != '\r') {
      if (rxLen < RX_BUF_SIZE - 1) rxBuf[rxLen++] = c;
      else rxOverflow = true;                 // Paket rusak -> dibuang
    }
  }

  if (millis() - lastTxMs >= TX_PERIOD_MS) {
    lastTxMs = millis();
    sendTelemetry();
  }
}

// ------------------------------------------------------------------------------
// SETUP & LOOP
// ------------------------------------------------------------------------------
void setup() {
  pinMode(PIN_LED, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_LED, LOW);
  digitalWrite(PIN_BUZZER, LOW);

  // Inisialisasi Relay: Tulis RELAY_OFF terlebih dahulu sebelum OUTPUT
  // agar relay tidak "klik" sesaat saat booting (Active-LOW safety)
  digitalWrite(PIN_RELAY_VALVE, RELAY_OFF);
  pinMode(PIN_RELAY_VALVE, OUTPUT);

  pinMode(PIN_PB1, INPUT_PULLUP);
  pinMode(PIN_PB2, INPUT_PULLUP);
  pinMode(PIN_PB3, INPUT_PULLUP);
  pinMode(PIN_SPDT1_SUHU, INPUT_PULLUP);
  pinMode(PIN_SPDT2_TEKANAN, INPUT_PULLUP);

  pinMode(PIN_MAX6675_CS, OUTPUT);
  pinMode(PIN_MAX6675_SCK, OUTPUT);
  pinMode(PIN_MAX6675_SO, INPUT);
  digitalWrite(PIN_MAX6675_CS, HIGH);
  digitalWrite(PIN_MAX6675_SCK, LOW);

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("PLTU BOILER SLV");
  lcd.print(SLAVE_ID);
  lcd.setCursor(0, 1);
  lcd.print("INIT HARDWARE...");

  masterSerial.begin(9600);

  delay(1000);
  lcd.clear();
  invalidateLcd();

  readSensors();                              // Nilai awal valid sebelum loop
  lastSensorMs = millis();
  evaluateSafety();
  applyActuators();
}

void loop() {
  unsigned long now = millis();

  if (now - lastSensorMs >= SENSOR_PERIOD_MS) {
    lastSensorMs = now;
    readSensors();
  }

  handleButtons();
  handleUart();
  evaluateSafety();
  applyActuators();
  updateLcd();
}
