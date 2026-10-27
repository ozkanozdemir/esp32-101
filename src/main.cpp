// =====================================================================
// ESP32-S3 Wi-Fi sohbet odası
//
// Kart "oozdemir" adında şifreli bir Wi-Fi ağı yayınlar. Bu ağa bağlanan
// cihazlar tarayıcıya http://ozkan.local (veya http://192.168.4.1) yazınca,
// kartın dosya alanındaki (LittleFS) data/site klasöründeki sohbet sayfasını görür.
//
// Dosyalar:
//   main.cpp        -> Wi-Fi yayını, ozkan.local adı (DNS/mDNS), açılış (bu dosya)
//   web_server.cpp  -> web sunucusu: site dosyalarını sunar
//   chat.cpp        -> sohbet odası: WebSocket, kullanıcılar, mesajlar
//   led.cpp         -> karttaki renkli LED: durum renkleri ve yavaş renk geçişi
// =====================================================================

// Arduino'nun temel fonksiyonlarını (Serial, delay, String vb.) projeye ekler.
#include <Arduino.h>
// Küçük bir DNS sunucusu. Cihazlar "ozkan.local hangi IP'de?" diye sorduğunda cevap verir.
#include <DNSServer.h>
// mDNS kütüphanesi. Kart ağa "ben ozkan.local'ım" diye kendini duyurur (iPhone, Mac, Windows için).
#include <ESPmDNS.h>
// LittleFS dosya sistemi. data/ klasöründen yüklenen site dosyalarını okumamızı sağlar.
#include <LittleFS.h>
// Wi-Fi kütüphanesi. Kartın Wi-Fi yayını yapmasını sağlar.
#include <WiFi.h>

// Karttaki renkli LED (ledSetColor, ledStartRainbow, loopLed).
#include "led.h"
// Web sunucusu (setupWebServer, loopWebServer).
#include "web_server.h"

// Yayınlanacak Wi-Fi ağının adı.
const char *AP_SSID = "oozdemir";
// Wi-Fi şifresi (WPA2 gereği en az 8 karakter).
const char *AP_PASSWORD = "12345678";
// Kartın ağdaki adı; http://ozkan.local adresi buradan oluşur.
const char *HOSTNAME = "ozkan";
// Wi-Fi'a aynı anda bağlanabilecek en fazla cihaz sayısı.
const uint8_t AP_MAX_CLIENTS = 8;
// DNS'in standart portu.
const byte DNS_PORT = 53;

// Kartın kendi ağındaki IP adresi.
const IPAddress apIP(192, 168, 4, 1);
// Alt ağ maskesi (ağdaki adresler 192.168.4.x olur).
const IPAddress apMask(255, 255, 255, 0);

// DNS sunucusu nesnesi.
DNSServer dnsServer;

// setup(): Kart açıldığında bir kez çalışır.
void setup() {
  // Seri portu başlat (bilgisayardaki seri monitör için).
  Serial.begin(115200);
  // USB seri bağlantının kurulması için 1 saniye bekle.
  delay(1000);
  // LED'i turuncu yak: "açılıyor".
  ledSetColor(32, 16, 0);

  // Dosya alanını bağla; başarısızsa...
  if (!LittleFS.begin()) {
    // ...uyarı yaz (genelde site dosyaları henüz yüklenmemişse olur).
    Serial.println("LittleFS acilamadi: once 'pio run -t uploadfs' ile site dosyalarini yukleyin");
  // if bloğunun sonu.
  }

  // Wi-Fi'ı erişim noktası moduna al (kart kendisi ağ yayınlar).
  WiFi.mode(WIFI_AP);
  // Ağın adres ayarları: kartın IP'si, ağ geçidi (kartın kendisi), alt ağ maskesi.
  WiFi.softAPConfig(apIP, apIP, apMask);
  // Wi-Fi yayınını başlat (ad, şifre, kanal 1, gizli değil, en fazla 8 cihaz); başarısızsa...
  if (!WiFi.softAP(AP_SSID, AP_PASSWORD, 1, 0, AP_MAX_CLIENTS)) {
    // ...hata yaz.
    Serial.println("Wi-Fi yayini baslatilamadi");
    // LED'i kırmızı yak: "hata" (renk geçişi başlamaz, kırmızı kalır).
    ledSetColor(32, 0, 0);
    // Gerisini çalıştırma.
    return;
  // if bloğunun sonu.
  }

  // DNS'te bilinmeyen adreslere "böyle bir adres yok" cevabı ver (telefon giriş sayfası açmasın).
  dnsServer.setErrorReplyCode(DNSReplyCode::NonExistentDomain);
  // DNS'i başlat; sadece "ozkan.local" sorulunca kartın IP'sini ver.
  dnsServer.start(DNS_PORT, String(HOSTNAME) + ".local", apIP);

  // mDNS'i "ozkan" adıyla başlat; başarılıysa...
  if (MDNS.begin(HOSTNAME)) {
    // ...80. portta web servisi olduğunu da duyur.
    MDNS.addService("http", "tcp", 80);
  // Başarısızsa...
  } else {
    // ...uyarı yaz (IP adresiyle erişim yine çalışır).
    Serial.println("mDNS baslatilamadi");
  // if/else bloğunun sonu.
  }

  // Web sunucusunu (site + sohbet) başlat. Ayrıntılar web_server.cpp ve chat.cpp'de.
  setupWebServer();

  // Seri porta kartın hazır olduğunu yaz.
  Serial.printf("Wi-Fi yayini: %s  adres: http://%s.local (http://%s)\n", AP_SSID, HOSTNAME,
                // IP adresini yazıya çevir.
                apIP.toString().c_str());
  // Kart hazır: LED tüm renkleri yavaşça dolaşmaya başlasın.
  ledStartRainbow();
// setup fonksiyonunun sonu.
}

// loop(): setup()'tan sonra sürekli tekrar eder. Web sunucusu ve WebSocket kendi arka plan
// görevlerinde çalıştığı için burada sadece DNS, LED ve düzenli temizlik işi var.
void loop() {
  // Gelen bir DNS sorusu varsa cevapla.
  dnsServer.processNextRequest();
  // Web sunucusunun ve sohbetin düzenli işleri.
  loopWebServer();
  // LED'in renk geçişini ilerlet.
  loopLed();
// loop fonksiyonunun sonu.
}
