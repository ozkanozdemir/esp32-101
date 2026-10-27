// =====================================================================
// Web sunucusu - site dosyalarını sunar, sohbet WebSocket'ini barındırır.
// =====================================================================

// Bu dosyanın dışarıya açtığı fonksiyonların tanımları (setupWebServer, loopWebServer).
#include "web_server.h"

// Sohbet odası (setupChat, loopChat).
#include "chat.h"

// Eşzamansız (async) web sunucusu. Aynı anda birden çok isteği ve WebSocket bağlantısını yönetir.
#include <ESPAsyncWebServer.h>
// LittleFS dosya sistemi. data/ klasöründen yüklenen site dosyalarını okumamızı sağlar.
#include <LittleFS.h>

// İsimsiz namespace: içindeki "server" sadece bu dosyada görünür.
namespace {

// 80. portta (HTTP) çalışan web sunucusu.
AsyncWebServer server(80);

// İsimsiz namespace'in sonu.
}  // namespace

// Web sunucusunu hazırlayıp başlatır (açıklaması web_server.h'de).
void setupWebServer() {
  // Sohbet odasının WebSocket'ini sunucuya ekle (ws://ozkan.local/ws). Site dosyalarından önce
  // eklenmeli ki "/ws" isteği dosya sanılmasın.
  setupChat(server);
  // Diğer tüm adresleri LittleFS'teki /site klasöründen sun. "/" istenince index.html verilir;
  // tarayıcıya "eski sürümü saklama" denir; yanında .gz sürümü olan dosyalar sıkıştırılmış gönderilir.
  server.serveStatic("/", LittleFS, "/site/").setDefaultFile("index.html").setCacheControl("no-cache");
  // Bulunamayan adreslere 404 cevabı ver.
  server.onNotFound([](AsyncWebServerRequest *request) {
    // 404 = "Bulunamadı".
    request->send(404, "text/plain", "Bulunamadi");
  // onNotFound tanımının sonu.
  });
  // Web sunucusunu başlat.
  server.begin();
// setupWebServer fonksiyonunun sonu.
}

// Web sunucusunun düzenli işlerini yapar (açıklaması web_server.h'de).
void loopWebServer() {
  // Sohbet odasının düzenli işleri (kopmuş bağlantıların temizliği).
  loopChat();
// loopWebServer fonksiyonunun sonu.
}
