// =====================================================================
// ESP32-S3 Wi-Fi sohbet odası
//
// Kart, secrets.h dosyasında adı ve şifresi yazan modemin Wi-Fi ağına bağlanır.
// Aynı modeme bağlı cihazlar tarayıcıya kartın IP adresini (seri monitörde ve
// modemin "bağlı cihazlar" listesinde görünür) ya da http://ozkan.local yazınca,
// kartın dosya alanındaki (LittleFS) data/site klasöründeki sohbet sayfasını görür.
//
// Dosyalar:
//   main.cpp        -> modeme bağlanma, ozkan.local adı (mDNS), açılış (bu dosya)
//   secrets.h       -> modemin ağ adı ve şifresi (GitHub'a gönderilmez)
//   web_server.cpp  -> web sunucusu: site dosyalarını sunar
//   chat.cpp        -> sohbet odası: WebSocket, kullanıcılar, mesajlar
//   led.cpp         -> karttaki renkli LED: durum renkleri ve yavaş renk geçişi
//   log.cpp         -> terminale yazma: kartın iki USB girişine birden
//
// IP adresini görmek için: seri monitörü açın (kablo hangi girişte olursa olsun)
// ve Enter'a basın. Kart bağlıysa adresini yazar.
// =====================================================================

// Arduino'nun temel fonksiyonlarını (Serial, delay, millis vb.) projeye ekler.
#include <Arduino.h>
// mDNS kütüphanesi. Kart modemin ağına "ben ozkan.local'ım" diye kendini duyurur.
#include <ESPmDNS.h>
// LittleFS dosya sistemi. data/ klasöründen yüklenen site dosyalarını okumamızı sağlar.
#include <LittleFS.h>
// Wi-Fi kütüphanesi. Kartın modeme bağlanmasını sağlar.
#include <WiFi.h>

// Karttaki renkli LED (ledSetColor, ledStartRainbow, loopLed).
#include "led.h"
// Terminale mesaj yazma: "USB" ve "COM" girişlerine birden (logBegin, logf, logKeyPressed).
#include "log.h"
// Web sunucusu (setupWebServer, loopWebServer).
#include "web_server.h"

// Modemin ağ adı ve şifresi. secrets.h yoksa (örneğin proje GitHub'dan yeni indirildiyse)
// derleme anlaşılır bir mesajla dursun.
#if __has_include("secrets.h")
// Modem bilgilerini ekle (WIFI_SSID, WIFI_PASSWORD).
#include "secrets.h"
// secrets.h bulunamadıysa...
#else
// ...derlemeyi durdur ve ne yapılacağını söyle.
#error "src/secrets.h yok: src/secrets.example.h dosyasini src/secrets.h adiyla kopyalayip modem bilgilerini yazin."
// #if bloğunun sonu.
#endif

// Kartın ağdaki adı. Modemin cihaz listesinde bu isim görünür; http://ozkan.local adresi de buradan oluşur.
const char *HOSTNAME = "ozkan";
// Modeme bu kadar süre (milisaniye) bağlanılamazsa LED kırmızı yanar ve seri porta uyarı yazılır.
const uint32_t CONNECT_WARN_MS = 20000;

// Kart şu an modeme bağlı mı (bir önceki loop turundaki durum).
bool wifiConnected = false;
// Modemle bağlantının en son koptuğu ya da bağlanmaya başlandığı an.
uint32_t disconnectedSince = 0;
// "Bağlanamadı" uyarısının en son yazıldığı an.
uint32_t lastWarning = 0;
// mDNS başlatıldı mı (sadece ilk bağlantıda bir kez başlatılır).
bool mdnsStarted = false;
// Modemin bağlantıyı en son neden reddettiği/kopardığı (Wi-Fi sürücüsünün sebep kodu; 0 = bilinmiyor).
volatile uint8_t lastDisconnectReason = 0;
// Bağlanamama durumunda çevredeki ağlar tarandı mı (tarama bağlanma denemesini yavaşlattığı için bir kez yapılır).
bool scannedForNetwork = false;

// Bağlanamayınca sebebini anlamak için çevredeki ağları tarar ve bizim ağımızı görüp görmediğini yazar.
void reportNetworkScan() {
  // Bağlanma denemesi sürerken tarama yapılamaz; denemeyi durdur.
  WiFi.disconnect();
  // Durmasını kısa bir süre bekle.
  delay(200);
  // Çevredeki Wi-Fi ağlarını tara (birkaç saniye sürer); bulunan ağ sayısı döner.
  int count = WiFi.scanNetworks();
  // Tarama bitti; modeme bağlanmayı yeniden başlat.
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  // Bulunan her ağa bak...
  for (int i = 0; i < count; i++) {
    // ...adı bizim ağımızla aynıysa...
    if (WiFi.SSID(i) == WIFI_SSID) {
      // ...sinyal gücünü ve kanalını yaz. RSSI: -50 çok iyi, -70 orta, -80'den kötüsü zayıf.
      logf("Tarama: '%s' gorunuyor. Sinyal: %d dBm, kanal: %d, sifreleme: %d\n", WIFI_SSID, WiFi.RSSI(i),
           // Kanal ve şifreleme türü (3 = WPA2, 4 = WPA/WPA2, 6 = WPA3, 7 = WPA2/WPA3).
           WiFi.channel(i), (int)WiFi.encryptionType(i));
      // Tarama sonuçlarını bellekten sil.
      WiFi.scanDelete();
      // Fonksiyondan çık.
      return;
    // if bloğunun sonu.
    }
  // for döngüsünün sonu.
  }
  // Ağ hiç görünmediyse: ad yanlış, ağ 5 GHz ya da kart modemden çok uzak.
  logf("Tarama: '%s' adli ag GORUNMUYOR (%d ag bulundu). Ad yanlis, ag 5 GHz ya da modem uzak olabilir.\n",
       // Ağ adı ve bulunan ağ sayısı.
       WIFI_SSID, count);
  // Tarama sonuçlarını bellekten sil.
  WiFi.scanDelete();
// reportNetworkScan fonksiyonunun sonu.
}

// Modeme ilk kez bağlanınca ya da bağlantı geri gelince yapılacaklar.
void onWifiConnected() {
  // Modemin karta verdiği IP adresi; tarayıcıya yazılacak adres bu.
  String ip = WiFi.localIP().toString();
  // Seri porta adresleri yaz.
  logf("Modeme baglandi (%s). Adres: http://%s  veya  http://%s.local\n", WIFI_SSID, ip.c_str(),
                // Kartın ağdaki adı.
                HOSTNAME);
  // mDNS henüz başlatılmadıysa...
  if (!mdnsStarted) {
    // ...kartı "ozkan.local" adıyla duyur; başarılıysa...
    if (MDNS.begin(HOSTNAME)) {
      // ...80. portta web servisi olduğunu da duyur.
      MDNS.addService("http", "tcp", 80);
      // Bir daha başlatma.
      mdnsStarted = true;
    // Başarısızsa...
    } else {
      // ...uyarı yaz (IP adresiyle erişim yine çalışır).
      logf("mDNS baslatilamadi\n");
    // if/else bloğunun sonu.
    }
  // if bloğunun sonu.
  }
  // Kart hazır: LED renkleri yavaşça dolaşsın (kaldığı renkten devam eder).
  ledStartRainbow();
// onWifiConnected fonksiyonunun sonu.
}

// Kartın o anki durumunu (bağlı mı, IP adresi ne) terminale yazar. Terminalde Enter'a basınca çağrılır.
void printStatus() {
  // Modeme bağlıysa...
  if (WiFi.status() == WL_CONNECTED) {
    // ...ağ adını ve adresleri yaz.
    logf("Durum: '%s' agina bagli. Adres: http://%s  veya  http://%s.local\n", WIFI_SSID,
         // Modemin karta verdiği IP adresi.
         WiFi.localIP().toString().c_str(), HOSTNAME);
  // Bağlı değilse...
  } else {
    // ...hâlâ bağlanmaya çalıştığını yaz.
    logf("Durum: '%s' agina henuz baglanilamadi, deneniyor...\n", WIFI_SSID);
  // if/else bloğunun sonu.
  }
// printStatus fonksiyonunun sonu.
}

// Modemle bağlantı durumunu takip eder. loop() içinden sürekli çağrılır.
void checkWifi() {
  // Şu an bağlı mıyız.
  bool connected = WiFi.status() == WL_CONNECTED;
  // Bağlantı yeni kurulduysa (önceden yoktu, şimdi var)...
  if (connected && !wifiConnected) {
    // ...bağlanınca yapılacakları yap.
    onWifiConnected();
  // Bağlantı yeni koptuysa (önceden vardı, şimdi yok)...
  } else if (!connected && wifiConnected) {
    // ...seri porta yaz. Kart kendiliğinden yeniden bağlanmayı dener.
    logf("Modemle baglanti koptu, yeniden baglaniliyor...\n");
    // LED turuncu: "bağlanıyor".
    ledSetColor(32, 16, 0);
    // Kopma zamanını kaydet.
    disconnectedSince = millis();
  // if/else bloğunun sonu.
  }
  // Yeni durumu sakla.
  wifiConnected = connected;

  // Uzun süredir bağlanamıyorsak (şifre yanlış, modem kapalı, 5 GHz ağ vb.) 10 saniyede bir uyar.
  if (!connected && millis() - disconnectedSince > CONNECT_WARN_MS && millis() - lastWarning > 10000) {
    // Uyarıyı yaz.
    logf("'%s' agina baglanilamiyor (sebep: %s). secrets.h'deki ad/sifreyi ve agin 2.4 GHz oldugunu kontrol edin.\n",
                  // Ağ adı ve Wi-Fi sürücüsünün bildirdiği sebep (örn. AUTH_FAIL = şifre yanlış).
                  WIFI_SSID, WiFi.disconnectReasonName((wifi_err_reason_t)lastDisconnectReason));
    // İlk uyarıda bir kez çevredeki ağları tarayıp bizim ağımız görünüyor mu yaz.
    if (!scannedForNetwork) {
      // Taramayı yap ve sonucu yaz.
      reportNetworkScan();
      // Bir daha tarama.
      scannedForNetwork = true;
    // if bloğunun sonu.
    }
    // LED kırmızı: "bağlanamıyor".
    ledSetColor(32, 0, 0);
    // Uyarı zamanını kaydet.
    lastWarning = millis();
  // if bloğunun sonu.
  }
// checkWifi fonksiyonunun sonu.
}

// setup(): Kart açıldığında bir kez çalışır.
void setup() {
  // Terminal çıkışını başlat: kartın "USB" ve "COM" girişlerinin ikisi de (seri monitör için).
  logBegin();
  // USB seri bağlantının kurulması için 1 saniye bekle.
  delay(1000);
  // LED'i turuncu yak: "açılıyor / modeme bağlanıyor".
  ledSetColor(32, 16, 0);

  // Dosya alanını bağla; başarısızsa...
  if (!LittleFS.begin()) {
    // ...uyarı yaz (genelde site dosyaları henüz yüklenmemişse olur).
    logf("LittleFS acilamadi: once 'pio run -t uploadfs' ile site dosyalarini yukleyin\n");
  // if bloğunun sonu.
  }

  // Wi-Fi'ı istemci (STA) moduna al: kart, telefon gibi bir modeme bağlanır.
  WiFi.mode(WIFI_STA);
  // Modemin cihaz listesinde kartın "ozkan" adıyla görünmesini sağla.
  WiFi.setHostname(HOSTNAME);
  // Bağlantı koparsa kart kendiliğinden yeniden bağlanmayı denesin.
  WiFi.setAutoReconnect(true);
  // Modem bağlantıyı reddettiğinde ya da kopardığında sebep kodunu sakla (terminalde göstermek için).
  WiFi.onEvent([](arduino_event_id_t event, arduino_event_info_t info) {
    // Sebep kodunu kaydet.
    lastDisconnectReason = info.wifi_sta_disconnected.reason;
  // Bu fonksiyon sadece "bağlantı koptu/kurulamadı" olayında çağrılsın.
  }, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
  // Modeme bağlanmaya başla. Beklemiyoruz; bağlantı durumu loop() içinde takip edilir.
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  // Bağlanma denemesinin başladığı an.
  disconnectedSince = millis();
  // Seri porta yaz.
  logf("'%s' agina baglaniliyor...\n", WIFI_SSID);
  // Terminali sonradan açanlar için ipucu.
  logf("Ipucu: IP adresini gormek icin terminalde Enter'a basin.\n");

  // Web sunucusunu (site + sohbet) başlat. Modeme bağlanınca kendiliğinden erişilebilir olur.
  setupWebServer();
// setup fonksiyonunun sonu.
}

// loop(): setup()'tan sonra sürekli tekrar eder. Web sunucusu ve WebSocket kendi arka plan
// görevlerinde çalıştığı için burada sadece bağlantı takibi, LED ve düzenli temizlik işi var.
void loop() {
  // Modemle bağlantıyı takip et (bağlanınca IP'yi yaz, koparsa LED'le haber ver).
  checkWifi();
  // Terminalde Enter'a (ya da herhangi bir tuşa) basıldıysa kartın durumunu ve IP adresini yaz.
  if (logKeyPressed()) printStatus();
  // Web sunucusunun ve sohbetin düzenli işleri.
  loopWebServer();
  // LED'in renk geçişini ilerlet.
  loopLed();
// loop fonksiyonunun sonu.
}
