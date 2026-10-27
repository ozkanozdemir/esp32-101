// =====================================================================
// Sohbet odası (WebSocket) - kullanıcılar, mesaj geçmişi ve mesajlaşma.
// Mesaj biçimlerinin açıklaması chat.h dosyasının başında.
// =====================================================================

// Bu dosyanın dışarıya açtığı fonksiyonların tanımları (setupChat, loopChat).
#include "chat.h"
// Karttaki renkli LED (renk seçimi, katılma/ayrılma yanıp sönmeleri).
#include "led.h"
// Terminale mesaj yazma ("USB" ve "COM" girişlerine birden).
#include "log.h"

// Arduino'nun temel fonksiyonlarını (Serial, String, millis vb.) projeye ekler.
#include <Arduino.h>
// JSON okuma/yazma kütüphanesi. Sayfayla aradaki mesajları JSON olarak çözer ve oluşturur.
#include <ArduinoJson.h>
// C++ "deque" (iki uçlu kuyruk) yapısı. Son mesajları saklamak için kullanılır.
#include <deque>
// C++ "map" (sözlük) yapısı. Hangi bağlantının hangi kullanıcıya ait olduğunu tutar.
#include <map>

// İsimsiz (anonymous) namespace: içindeki her şey sadece bu dosyada görünür. Böylece buradaki
// "users", "broadcast" gibi isimler başka dosyalardaki aynı isimlerle karışmaz.
namespace {

// Bir kullanıcı adının en fazla kaç karakter olabileceği.
const size_t MAX_NAME_CHARS = 20;
// Bir mesajın en fazla kaç karakter olabileceği.
const size_t MAX_TEXT_CHARS = 300;
// Yeni katılanlara gösterilecek en fazla geçmiş mesaj sayısı.
const size_t MAX_HISTORY = 30;
// Sayfadan gelen tek bir WebSocket mesajının en fazla kaç bayt olabileceği (kötü niyetli büyük veriye karşı).
const size_t MAX_FRAME_BYTES = 2048;
// Bağlantı kontrolü (ping) aralığı, saniye. Telefonu kapanan/uzaklaşan kişiler bu sayede listeden düşer.
const uint16_t KEEPALIVE_SECONDS = 10;
// Bir kişinin LED rengini en sık ne aralıkla değiştirebileceği (milisaniye). Renk seçiciyi
// art arda oynatan biri LED'i ve odayı mesaja boğmasın diye.
const uint32_t LED_MIN_INTERVAL_MS = 300;
// Katılma (yeşil) ve ayrılma (kırmızı) yanıp sönmesinin parlaklığı (0-255).
const uint8_t FLASH_LEVEL = 80;

// "/ws" adresinde çalışan WebSocket sunucusu. Sayfalar buraya bağlanır.
AsyncWebSocket ws("/ws");

// Odaya katılmış bir kullanıcının bilgileri.
struct User {
  // Kullanıcının seçtiği isim.
  String name;
  // Kullanıcıya özel gizli anahtar. Sayfa yenilenince aynı ismi geri alabilmesi için.
  String token;
  // Bu kişinin LED rengini en son değiştirdiği an (çok sık değiştirmeyi engellemek için).
  uint32_t lastLedAt = 0;
// User yapısının sonu.
};

// Geçmişte saklanan bir sohbet mesajı.
struct ChatMessage {
  // Mesajı gönderenin ismi.
  String from;
  // Mesajın metni.
  String text;
  // Mesajın geldiği an (kart açıldığından beri geçen milisaniye).
  uint32_t at;
// ChatMessage yapısının sonu.
};

// Bağlantı numarası -> kullanıcı eşlemesi. Sadece isim girip odaya katılmış olanlar burada.
std::map<uint32_t, User> users;
// Son mesajlar (en eskisi başta). Yeni katılanlara gönderilir.
std::deque<ChatMessage> history;

// Bir metnin kaç karakter olduğunu sayar. Türkçe harfler (ç, ş, ğ...) bellekte 2 bayt
// tuttuğu için String::length() yerine bunu kullanıyoruz; aksi halde "Şükrü" 7 sayılırdı.
size_t utf8Length(const String &s) {
  // Karakter sayacı.
  size_t count = 0;
  // Metnin her baytına bak.
  for (size_t i = 0; i < s.length(); i++) {
    // Bir karakterin devam baytları 10xxxxxx biçimindedir; sadece ilk baytları say.
    if ((s[i] & 0xC0) != 0x80) count++;
  // for döngüsünün sonu.
  }
  // Toplam karakter sayısını döndür.
  return count;
// utf8Length fonksiyonunun sonu.
}

// Metinde satır sonu, sekme gibi kontrol karakterleri var mı bakar (isimde bunlara izin vermiyoruz).
bool hasControlChars(const String &s) {
  // Metnin her baytına bak.
  for (size_t i = 0; i < s.length(); i++) {
    // 32'den küçük kodlar ve 127 (DEL) kontrol karakteridir.
    if ((uint8_t)s[i] < 32 || (uint8_t)s[i] == 127) return true;
  // for döngüsünün sonu.
  }
  // Hiç kontrol karakteri bulunamadı.
  return false;
// hasControlChars fonksiyonunun sonu.
}

// Rastgele 16 haneli onaltılık (hex) bir anahtar üretir. Kullanıcının "token"ı olarak kullanılır.
String makeToken() {
  // Sonucun biriktirileceği metin.
  String token;
  // İki tane 32 bitlik rastgele sayı üret (toplam 16 hex hane).
  for (int i = 0; i < 2; i++) {
    // esp_random() kartın donanımsal rastgele sayı üretecini kullanır.
    char part[9];
    // Sayıyı 8 haneli hex yazıya çevir (örn. "0a1b2c3d").
    snprintf(part, sizeof(part), "%08lx", (unsigned long)esp_random());
    // Parçayı sonuca ekle.
    token += part;
  // for döngüsünün sonu.
  }
  // Üretilen anahtarı döndür.
  return token;
// makeToken fonksiyonunun sonu.
}

// Bir JSON belgesini yazıya çevirip tek bir bağlantıya gönderir.
void sendTo(uint32_t clientId, const JsonDocument &doc) {
  // JSON'un yazı hali.
  String out;
  // Belgeyi yazıya çevir.
  serializeJson(doc, out);
  // Yazıyı o bağlantıya WebSocket metin mesajı olarak gönder.
  ws.text(clientId, out);
// sendTo fonksiyonunun sonu.
}

// Bir JSON belgesini odaya katılmış herkese gönderir (isim girmemiş olanlara gitmez).
void broadcast(const JsonDocument &doc) {
  // JSON'un yazı hali.
  String out;
  // Belgeyi bir kez yazıya çevir.
  serializeJson(doc, out);
  // Odadaki her kullanıcıya...
  for (const auto &entry : users) {
    // ...aynı yazıyı gönder (entry.first = bağlantı numarası).
    ws.text(entry.first, out);
  // for döngüsünün sonu.
  }
// broadcast fonksiyonunun sonu.
}

// Bir bağlantıya hata mesajı gönderir. code: sayfanın ne yapacağını anlaması için kısa kod,
// text: kullanıcıya gösterilecek açıklama.
void sendError(uint32_t clientId, const char *code, const char *text) {
  // Boş bir JSON belgesi oluştur.
  JsonDocument doc;
  // Mesaj türü: hata.
  doc["type"] = "error";
  // Hata kodu (örn. "name_taken").
  doc["code"] = code;
  // Hata açıklaması.
  doc["text"] = text;
  // Belgeyi o bağlantıya gönder.
  sendTo(clientId, doc);
// sendError fonksiyonunun sonu.
}

// Odadakilerin güncel listesini herkese gönderir.
void broadcastUsers() {
  // Boş bir JSON belgesi oluştur.
  JsonDocument doc;
  // Mesaj türü: kullanıcı listesi.
  doc["type"] = "users";
  // "users" adında bir JSON dizisi aç.
  JsonArray list = doc["users"].to<JsonArray>();
  // Odadaki her kullanıcının...
  for (const auto &entry : users) {
    // ...ismini diziye ekle (entry.second = User bilgisi).
    list.add(entry.second.name);
  // for döngüsünün sonu.
  }
  // Listeyi odadaki herkese gönder.
  broadcast(doc);
// broadcastUsers fonksiyonunun sonu.
}

// "Ali katıldı" gibi bir bilgi mesajını odadaki herkese gönderir.
void broadcastSystem(const String &text) {
  // Boş bir JSON belgesi oluştur.
  JsonDocument doc;
  // Mesaj türü: sistem (bilgi) mesajı.
  doc["type"] = "system";
  // Bilgi metni.
  doc["text"] = text;
  // Odadaki herkese gönder.
  broadcast(doc);
// broadcastSystem fonksiyonunun sonu.
}

// Son mesajları (geçmişi) yeni katılan bir kullanıcıya gönderir.
void sendHistory(uint32_t clientId) {
  // Boş bir JSON belgesi oluştur.
  JsonDocument doc;
  // Mesaj türü: geçmiş.
  doc["type"] = "history";
  // "messages" adında bir JSON dizisi aç.
  JsonArray list = doc["messages"].to<JsonArray>();
  // Şu anki zaman (kart açıldığından beri geçen milisaniye).
  uint32_t now = millis();
  // Saklanan her mesaj için...
  for (const auto &msg : history) {
    // ...diziye yeni bir JSON nesnesi ekle.
    JsonObject item = list.add<JsonObject>();
    // Gönderenin ismi.
    item["from"] = msg.from;
    // Mesaj metni.
    item["text"] = msg.text;
    // Mesajın kaç saniye önce yazıldığı. Kartın saati olmadığından sayfa, kendi saatinden
    // bu kadar saniye çıkararak mesajın saatini hesaplar.
    item["ago"] = (now - msg.at) / 1000;
  // for döngüsünün sonu.
  }
  // Geçmişi o kullanıcıya gönder.
  sendTo(clientId, doc);
// sendHistory fonksiyonunun sonu.
}

// Verilen isimde (büyük/küçük harf fark etmeksizin) odada biri var mı arar.
// Bulursa o kişinin kaydını, bulamazsa users.end() döndürür.
std::map<uint32_t, User>::iterator findUserByName(const String &name) {
  // Odadaki her kayda bak.
  for (auto it = users.begin(); it != users.end(); ++it) {
    // İsimler eşleşiyorsa bu kaydı döndür.
    if (it->second.name.equalsIgnoreCase(name)) return it;
  // for döngüsünün sonu.
  }
  // Kimse bulunamadı.
  return users.end();
// findUserByName fonksiyonunun sonu.
}

// Sayfadan gelen {"type":"join"} mesajını işler: kullanıcıyı isimle odaya alır.
void handleJoin(AsyncWebSocketClient *client, JsonDocument &doc) {
  // Bu bağlantının numarası.
  uint32_t id = client->id();
  // Bu bağlantı zaten odadaysa ikinci kez katılmasına gerek yok.
  if (users.count(id)) return;

  // Gelen ismi al (yoksa boş metin).
  String name = doc["name"] | "";
  // Başındaki ve sonundaki boşlukları sil.
  name.trim();
  // Sayfanın sakladığı anahtarı al (sayfa yenilendiyse dolu gelir, ilk girişte boştur).
  String token = doc["token"] | "";

  // İsim boşsa, çok uzunsa veya kontrol karakteri içeriyorsa...
  if (name.isEmpty() || utf8Length(name) > MAX_NAME_CHARS || hasControlChars(name)) {
    // ...hata gönder ve dur.
    sendError(id, "invalid_name", "İsim 1-20 karakter olmalı.");
    // Fonksiyondan çık.
    return;
  // if bloğunun sonu.
  }

  // Bu kişinin odaya yeni mi katıldığı, yoksa eski bağlantısı kopmadan geri mi geldiği.
  bool isNew = true;

  // Bu isimde odada başka biri var mı bak.
  auto existing = findUserByName(name);
  // Aynı isimde biri varsa...
  if (existing != users.end()) {
    // ...ve gelen anahtar o kişininkiyle aynıysa, bu aynı kişidir (sayfayı yenilemiş ya da
    // bağlantısı kopup geri gelmiş). Eski bağlantıyı kapatıp ismi yeni bağlantıya devret.
    if (!token.isEmpty() && token == existing->second.token) {
      // Eski bağlantının numarası.
      uint32_t oldId = existing->first;
      // Eski kaydı listeden sil (kapanınca "ayrıldı" mesajı gitmesin diye önce siliyoruz).
      users.erase(existing);
      // Eski bağlantıyı kapat.
      ws.close(oldId);
      // Bu kişi yeni değil; "katıldı" mesajı gönderme.
      isNew = false;
    // Anahtar tutmuyorsa başka biri bu ismi kullanıyordur...
    } else {
      // ...hata gönder.
      sendError(id, "name_taken", "Bu isim şu an kullanılıyor, başka bir isim seçin.");
      // Fonksiyondan çık.
      return;
    // if/else bloğunun sonu.
    }
  // if bloğunun sonu.
  }

  // Yeni katılana yeni bir anahtar üret; sayfasını yenileyene eskisini geri ver.
  if (isNew) token = makeToken();
  // Kullanıcıyı odaya ekle.
  users[id] = User{name, token};

  // Katılımı onaylayan mesajı hazırla.
  JsonDocument welcome;
  // Mesaj türü: hoş geldin (katılım onayı).
  welcome["type"] = "welcome";
  // Kabul edilen isim (boşlukları temizlenmiş hali).
  welcome["name"] = name;
  // Sayfanın saklayacağı anahtar.
  welcome["token"] = token;
  // Onayı yeni kullanıcıya gönder.
  sendTo(id, welcome);
  // Son mesajları gönder ki sohbetin neresinde olunduğunu görsün.
  sendHistory(id);

  // Gerçekten yeni katıldıysa...
  if (isNew) {
    // ...herkese haber ver.
    broadcastSystem(name + " katıldı");
    // LED kısa bir an sönüp yeşil yansın, sonra kaldığı renkten devam etsin.
    ledFlash(0, FLASH_LEVEL, 0);
    // Seri porta da yaz.
    logf("Katildi: %s (%s)\n", name.c_str(), client->remoteIP().toString().c_str());
  // if bloğunun sonu.
  }
  // Herkese güncel kullanıcı listesini gönder.
  broadcastUsers();
// handleJoin fonksiyonunun sonu.
}

// Sayfadan gelen {"type":"chat"} mesajını işler: mesajı odadaki herkese iletir.
void handleChat(AsyncWebSocketClient *client, JsonDocument &doc) {
  // Bu bağlantının numarası.
  uint32_t id = client->id();
  // Bu bağlantının kullanıcı kaydını bul.
  auto it = users.find(id);
  // Kayıt yoksa (isim girmeden mesaj göndermeye çalışıyorsa)...
  if (it == users.end()) {
    // ...hata gönder.
    sendError(id, "not_joined", "Mesaj göndermek için önce odaya katılın.");
    // Fonksiyondan çık.
    return;
  // if bloğunun sonu.
  }

  // Mesaj metnini al.
  String text = doc["text"] | "";
  // Başındaki ve sonundaki boşlukları sil.
  text.trim();
  // Boş mesajları sessizce yok say.
  if (text.isEmpty()) return;
  // Çok uzunsa...
  if (utf8Length(text) > MAX_TEXT_CHARS) {
    // ...hata gönder.
    sendError(id, "invalid_text", "Mesaj en fazla 300 karakter olabilir.");
    // Fonksiyondan çık.
    return;
  // if bloğunun sonu.
  }

  // Gönderenin ismi. Sayfanın yolladığı isme değil, kartın kaydına güveniyoruz;
  // böylece kimse başkasının adına mesaj gönderemez.
  const String &from = it->second.name;
  // Mesajı geçmişe ekle.
  history.push_back(ChatMessage{from, text, millis()});
  // Geçmiş sınırı aşıldıysa en eski mesajı sil.
  if (history.size() > MAX_HISTORY) history.pop_front();

  // Herkese gidecek mesajı hazırla.
  JsonDocument out;
  // Mesaj türü: sohbet.
  out["type"] = "chat";
  // Gönderen.
  out["from"] = from;
  // Metin.
  out["text"] = text;
  // Odadaki herkese (gönderen dahil) ilet.
  broadcast(out);
// handleChat fonksiyonunun sonu.
}

// "#ff8800" biçimindeki bir rengi kırmızı/yeşil/mavi değerlerine çevirir.
// Biçim hatalıysa false döndürür.
bool parseHexColor(const String &hex, uint8_t &r, uint8_t &g, uint8_t &b) {
  // Tam 7 karakter olmalı ve "#" ile başlamalı.
  if (hex.length() != 7 || hex[0] != '#') return false;
  // "#"tan sonraki 6 karakterin her birine bak...
  for (int i = 1; i < 7; i++) {
    // ...hepsi onaltılık (0-9, a-f) rakam olmalı.
    if (!isxdigit((unsigned char)hex[i])) return false;
  // for döngüsünün sonu.
  }
  // "ff8800" kısmını tek bir sayıya çevir (16 tabanında).
  uint32_t value = strtoul(hex.c_str() + 1, nullptr, 16);
  // En soldaki iki hane kırmızı.
  r = (value >> 16) & 0xFF;
  // Ortadaki iki hane yeşil.
  g = (value >> 8) & 0xFF;
  // En sağdaki iki hane mavi.
  b = value & 0xFF;
  // Başarılı.
  return true;
// parseHexColor fonksiyonunun sonu.
}

// Sayfadan gelen {"type":"led","color":"#ff8800"} mesajını işler: karttaki LED'in renk geçişini
// seçilen renkten sürdürür ve odadaki herkese kimin hangi rengi seçtiğini bildirir.
void handleLed(AsyncWebSocketClient *client, JsonDocument &doc) {
  // Bu bağlantının numarası.
  uint32_t id = client->id();
  // Bu bağlantının kullanıcı kaydını bul.
  auto it = users.find(id);
  // Odaya katılmamış biri LED'i değiştiremez.
  if (it == users.end()) {
    // Hata gönder.
    sendError(id, "not_joined", "LED'i değiştirmek için önce odaya katılın.");
    // Fonksiyondan çık.
    return;
  // if bloğunun sonu.
  }
  // Bu kişi LED'i çok kısa süre önce değiştirdiyse bu isteği sessizce yok say.
  if (millis() - it->second.lastLedAt < LED_MIN_INTERVAL_MS) return;

  // Gelen rengi al (örn. "#ff8800").
  String color = doc["color"] | "";
  // Renk değerleri.
  uint8_t r, g, b;
  // Biçim hatalıysa...
  if (!parseHexColor(color, r, g, b)) {
    // ...hata gönder.
    sendError(id, "invalid_color", "Geçersiz renk.");
    // Fonksiyondan çık.
    return;
  // if bloğunun sonu.
  }
  // Değiştirme zamanını kaydet.
  it->second.lastLedAt = millis();
  // LED'in renk geçişini bu renkten sürdür.
  ledSetRainbowColor(r, g, b);

  // Odadakilere haber vermek için mesajı hazırla.
  JsonDocument out;
  // Mesaj türü: LED rengi değişti.
  out["type"] = "led";
  // Rengi seçen kişi.
  out["from"] = it->second.name;
  // Seçilen renk (küçük harfe çevrilmiş).
  color.toLowerCase();
  // Renk.
  out["color"] = color;
  // Odadaki herkese gönder.
  broadcast(out);
  // Seri porta yaz.
  logf("LED: %s -> %s\n", it->second.name.c_str(), color.c_str());
// handleLed fonksiyonunun sonu.
}

// Bir kullanıcıyı odadan hemen çıkarır: isim listeden kalkar, herkese "... ayrıldı" yazılır.
// "Çıkış"a basınca ({"type":"leave"}) ve bağlantı kopunca (tarayıcı/sekme kapatma,
// sayfadan ayrılma, Wi-Fi'dan çıkma) çağrılır.
void removeUser(uint32_t clientId) {
  // Bu bağlantının kullanıcı kaydını bul.
  auto it = users.find(clientId);
  // Kayıt yoksa (isim girmeden ayrıldıysa ya da zaten çıkarıldıysa) yapılacak bir şey yok.
  if (it == users.end()) return;
  // Ayrılanın ismini sakla (kaydı silmeden önce).
  String name = it->second.name;
  // Kaydı sil.
  users.erase(it);
  // Herkese haber ver.
  broadcastSystem(name + " ayrıldı");
  // Güncel listeyi gönder.
  broadcastUsers();
  // LED kısa bir an sönüp kırmızı yansın, sonra kaldığı renkten devam etsin.
  ledFlash(FLASH_LEVEL, 0, 0);
  // Seri porta yaz.
  logf("Ayrildi: %s\n", name.c_str());
// removeUser fonksiyonunun sonu.
}

// WebSocket'te bir olay olduğunda (bağlantı, kopma, mesaj...) kütüphane bu fonksiyonu çağırır.
void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg,
               // data: gelen veri, len: verinin bayt uzunluğu.
               uint8_t *data, size_t len) {
  // Yeni bir sayfa bağlandıysa...
  if (type == WS_EVT_CONNECT) {
    // ...her 10 saniyede bir yoklama (ping) gönder. Cevap gelmezse bağlantı kapanır ve
    // kişi listeden düşer (telefonu kilitlenen ya da ağdan çıkanlar için).
    client->keepAlivePeriod(KEEPALIVE_SECONDS);
  // Bir sayfanın bağlantısı koptuysa...
  } else if (type == WS_EVT_DISCONNECT) {
    // ...kişiyi hemen odadan çıkar (tarayıcıyı kapatmak da çıkış sayılır).
    removeUser(client->id());
  // Sayfadan veri geldiyse...
  } else if (type == WS_EVT_DATA) {
    // ...gelen parçanın bilgilerini al (metin mi, tamamı mı geldi vb.).
    AwsFrameInfo *info = (AwsFrameInfo *)arg;
    // Sadece tek parça halinde gelen, makul boyuttaki metin mesajlarını kabul et.
    if (!info->final || info->index != 0 || info->len != len || info->opcode != WS_TEXT) return;
    // Çok büyük mesajları yok say.
    if (len > MAX_FRAME_BYTES) return;

    // Gelen JSON'u çözmek için boş bir belge.
    JsonDocument doc;
    // Veriyi JSON olarak çöz; bozuksa yok say.
    if (deserializeJson(doc, data, len)) return;
    // Mesajın türünü al.
    String msgType = doc["type"] | "";
    // Katılma isteğiyse...
    if (msgType == "join") {
      // ...katılmayı işle.
      handleJoin(client, doc);
    // Sohbet mesajıysa...
    } else if (msgType == "chat") {
      // ...mesajı işle.
      handleChat(client, doc);
    // Çıkış isteğiyse...
    } else if (msgType == "leave") {
      // ...kişiyi hemen odadan çıkar.
      removeUser(client->id());
    // LED rengi seçimiyse...
    } else if (msgType == "led") {
      // ...LED'i o renkten sürdür.
      handleLed(client, doc);
    // if/else bloğunun sonu.
    }
  // Olay türü kontrollerinin sonu.
  }
// onWsEvent fonksiyonunun sonu.
}

// İsimsiz namespace'in sonu. Aşağıdakiler dışarıdan (web_server.cpp, main.cpp) çağrılabilir.
}  // namespace

// Sohbet odasını hazırlar ve WebSocket'i web sunucusuna ekler (açıklaması chat.h'de).
void setupChat(AsyncWebServer &server) {
  // WebSocket olaylarını onWsEvent fonksiyonuna yönlendir.
  ws.onEvent(onWsEvent);
  // WebSocket'i web sunucusuna ekle (ws://ozkan.local/ws adresinde çalışır).
  server.addHandler(&ws);
// setupChat fonksiyonunun sonu.
}

// Sohbetin düzenli işlerini yapar (açıklaması chat.h'de).
void loopChat() {
  // En son ne zaman temizlik yapıldığı (static: değer çağrılar arasında korunur).
  static uint32_t lastCleanup = 0;
  // Son temizlikten bu yana 1 saniye geçtiyse...
  if (millis() - lastCleanup > 1000) {
    // ...kopmuş WebSocket bağlantılarının bellekte kalan kayıtlarını temizle.
    ws.cleanupClients();
    // Temizlik zamanını güncelle.
    lastCleanup = millis();
  // if bloğunun sonu.
  }
// loopChat fonksiyonunun sonu.
}
