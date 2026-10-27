// =====================================================================
// Terminale (seri monitöre) mesaj yazma - "USB" ve "COM" girişlerine birden.
// =====================================================================

// Bu dosyanın dışarıya açtığı fonksiyonların tanımları (logBegin, logf, logKeyPressed).
#include "log.h"

// Arduino'nun temel fonksiyonları (Serial, Serial0 vb.).
#include <Arduino.h>
// Değişken sayıda parametre alan fonksiyonlar için (va_list, va_start...).
#include <stdarg.h>

// Her iki girişi de başlatır (açıklaması log.h'de).
void logBegin() {
  // "USB" girişi (çipin kendi USB'si). platformio.ini'deki ARDUINO_USB_CDC_ON_BOOT=1 sayesinde Serial budur.
  Serial.begin(115200);
  // "COM" girişi (karttaki USB-seri çipine bağlı UART0).
  Serial0.begin(115200);
// logBegin fonksiyonunun sonu.
}

// Biçimli mesajı her iki girişe yazar (açıklaması log.h'de).
void logf(const char *format, ...) {
  // Mesajın yazılacağı geçici alan. Bundan uzun mesajlar kesilir.
  char buffer[256];
  // Parametre listesini tutacak değişken.
  va_list args;
  // Parametre listesini başlat ("format"tan sonraki parametreler).
  va_start(args, format);
  // Mesajı biçimlendirip geçici alana yaz (printf'in alana yazan hali).
  vsnprintf(buffer, sizeof(buffer), format, args);
  // Parametre listesiyle işimiz bitti.
  va_end(args);
  // "USB" girişine yaz.
  Serial.print(buffer);
  // "COM" girişine yaz.
  Serial0.print(buffer);
// logf fonksiyonunun sonu.
}

// Terminalden tuşa basıldı mı (açıklaması log.h'de).
bool logKeyPressed() {
  // Bir tuşa basıldı mı.
  bool pressed = false;
  // "USB" girişinden gelen tüm karakterleri oku ve at...
  while (Serial.available()) {
    // ...bir karakter oku.
    Serial.read();
    // Tuşa basılmış.
    pressed = true;
  // while döngüsünün sonu.
  }
  // "COM" girişinden gelen tüm karakterleri oku ve at...
  while (Serial0.available()) {
    // ...bir karakter oku.
    Serial0.read();
    // Tuşa basılmış.
    pressed = true;
  // while döngüsünün sonu.
  }
  // Sonucu döndür.
  return pressed;
// logKeyPressed fonksiyonunun sonu.
}
