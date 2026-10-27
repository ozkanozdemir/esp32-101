// =====================================================================
// Karttaki renkli LED (WS2812) - durum renkleri, yavaş renk geçişi,
// sohbetten gelen renk seçimi ve yanıp sönme bildirimleri.
// =====================================================================

// Bu dosyanın dışarıya açtığı fonksiyonların tanımları (açıklamaları led.h'de).
#include "led.h"

// Arduino'nun temel fonksiyonları (rgbLedWrite, millis vb.).
#include <Arduino.h>
// FreeRTOS kuyruğu. Sohbet kodu (başka bir görev) ile loop() arasında güvenle mesaj taşır.
#include <freertos/queue.h>

// Kartın üzerindeki renkli LED'in pini. Kütüphane zaten tanımladıysa ona dokunma.
#ifndef RGB_BUILTIN
// Tanımlı değilse 48 numaralı pini kullan (yanmazsa 38 deneyin).
#define RGB_BUILTIN 48
// #ifndef bloğunun sonu.
#endif

// İsimsiz namespace: içindekiler sadece bu dosyada görünür.
namespace {

// Renk geçişinde LED'in en yüksek parlaklığı (0-255). 255 göz alır; 40 rahat bir parlaklık.
const uint8_t RAINBOW_BRIGHTNESS = 40;
// Tüm renkleri bir kez dolaşmanın süresi (milisaniye). Büyüdükçe geçiş yavaşlar.
const uint32_t RAINBOW_CYCLE_MS = 12000;
// Renk geçişinde LED'in ne sıklıkla güncelleneceği (milisaniye). 20 ms = saniyede 50 kez.
const uint32_t UPDATE_INTERVAL_MS = 20;
// Yanıp sönmede LED'in sönük kalacağı süre (milisaniye).
const uint32_t FLASH_OFF_MS = 150;
// Yanıp sönmede LED'in yeşil/kırmızı yanacağı süre (milisaniye).
const uint32_t FLASH_ON_MS = 500;
// Kuyrukta bekleyebilecek en fazla istek sayısı. Dolarsa yeni istekler yok sayılır.
const UBaseType_t QUEUE_LENGTH = 16;

// Kuyruğa konabilecek istek türleri.
enum class CommandType : uint8_t {
  // Kısa söndür, bir renkte yak, sonra renk geçişine dön.
  Flash,
  // Renk geçişini belirli bir renk açısından (hue) sürdür.
  SetHue,
// CommandType sonu.
};

// Kuyruktaki bir istek.
struct Command {
  // İsteğin türü.
  CommandType type;
  // Flash için yanacak renk (kırmızı, yeşil, mavi).
  uint8_t r, g, b;
  // SetHue için renk tekerleğindeki açı (0-360 derece).
  float hue;
// Command yapısının sonu.
};

// LED'in o anki çalışma şekli.
enum class Mode : uint8_t {
  // Sabit durum rengi (açılış/hata). Renk geçişi ve istekler işlenmez.
  Static,
  // Tüm renkleri yavaşça dolaşıyor.
  Rainbow,
  // Yanıp sönmenin ilk adımı: LED kısa süre sönük.
  FlashOff,
  // Yanıp sönmenin ikinci adımı: LED yeşil/kırmızı yanıyor.
  FlashOn,
// Mode sonu.
};

// LED'in şu anki çalışma şekli.
Mode mode = Mode::Static;
// Renk geçişinin şu an renk tekerleğinde bulunduğu açı (0-360 derece).
float hue = 0;
// Renk geçişinin en son ilerletildiği an.
uint32_t lastUpdate = 0;
// Yanıp sönmenin şu anki adımının başladığı an.
uint32_t stepStart = 0;
// Şu an gösterilen yanıp sönme isteği (rengini hatırlamak için).
Command activeFlash;
// Sohbet kodundan gelen isteklerin bekletildiği kuyruk.
QueueHandle_t queue = nullptr;

// Kuyruk henüz yoksa oluşturur. setup() içinden, sohbet başlamadan önce çağrılır.
void ensureQueue() {
  // Kuyruk zaten varsa bir şey yapma.
  if (queue) return;
  // QUEUE_LENGTH tane Command alabilen bir kuyruk oluştur.
  queue = xQueueCreate(QUEUE_LENGTH, sizeof(Command));
// ensureQueue fonksiyonunun sonu.
}

// Renk tekerleğindeki bir açıyı (0-360 derece) kırmızı/yeşil/mavi değerlerine çevirir.
// 0 = kırmızı, 60 = sarı, 120 = yeşil, 180 = camgöbeği, 240 = mavi, 300 = mor, 360 = tekrar kırmızı.
void hueToRgb(float h, uint8_t brightness, uint8_t &r, uint8_t &g, uint8_t &b) {
  // Tekerleği 60'ar derecelik 6 dilime böl; açının hangi dilimde olduğunu bul (0-5).
  int sector = (int)(h / 60.0f) % 6;
  // Dilimin içinde ne kadar ilerlendiği (0.0 = dilimin başı, 1.0 = sonu).
  float f = h / 60.0f - (int)(h / 60.0f);
  // Dilim boyunca artan renk değeri.
  uint8_t rising = (uint8_t)(brightness * f);
  // Dilim boyunca azalan renk değeri.
  uint8_t falling = (uint8_t)(brightness * (1.0f - f));
  // Hangi dilimdeysek ona göre üç rengi ayarla: her dilimde bir renk tam, biri artıyor/azalıyor, biri kapalı.
  switch (sector) {
    // Kırmızı -> sarı: kırmızı tam, yeşil artıyor.
    case 0: r = brightness; g = rising;     b = 0;          break;
    // Sarı -> yeşil: kırmızı azalıyor, yeşil tam.
    case 1: r = falling;    g = brightness; b = 0;          break;
    // Yeşil -> camgöbeği: yeşil tam, mavi artıyor.
    case 2: r = 0;          g = brightness; b = rising;     break;
    // Camgöbeği -> mavi: yeşil azalıyor, mavi tam.
    case 3: r = 0;          g = falling;    b = brightness; break;
    // Mavi -> mor: kırmızı artıyor, mavi tam.
    case 4: r = rising;     g = 0;          b = brightness; break;
    // Mor -> kırmızı: kırmızı tam, mavi azalıyor.
    default: r = brightness; g = 0;         b = falling;    break;
  // switch bloğunun sonu.
  }
// hueToRgb fonksiyonunun sonu.
}

// Kırmızı/yeşil/mavi değerlerinden rengin tekerlekteki açısını (0-360 derece) bulur.
// Gri, beyaz, siyah gibi renklerin açısı yoktur; onlar için 0 (kırmızı) döner.
float rgbToHue(uint8_t r, uint8_t g, uint8_t b) {
  // Üç renkten en büyüğü.
  uint8_t maxC = max(r, max(g, b));
  // Üç renkten en küçüğü.
  uint8_t minC = min(r, min(g, b));
  // Aralarındaki fark; 0 ise renk gri tonudur.
  float delta = maxC - minC;
  // Gri tonların açısı olmaz.
  if (delta == 0) return 0;
  // Bulunacak açı.
  float h;
  // En baskın renk kırmızıysa açı kırmızı bölgesindedir (-60..60).
  if (maxC == r) h = 60.0f * ((g - b) / delta);
  // En baskın renk yeşilse açı yeşil bölgesindedir (60..180).
  else if (maxC == g) h = 60.0f * ((b - r) / delta) + 120.0f;
  // En baskın renk maviyse açı mavi bölgesindedir (180..300).
  else h = 60.0f * ((r - g) / delta) + 240.0f;
  // Negatif çıktıysa 360 ekleyerek 0-360 arasına getir.
  if (h < 0) h += 360.0f;
  // Açıyı döndür.
  return h;
// rgbToHue fonksiyonunun sonu.
}

// LED'i renk geçişinin şu anki açısına karşılık gelen renkte yakar.
void showRainbow() {
  // Açıya karşılık gelen renk değerleri.
  uint8_t r, g, b;
  // Açıyı renge çevir.
  hueToRgb(hue, RAINBOW_BRIGHTNESS, r, g, b);
  // LED'i bu renkte yak.
  rgbLedWrite(RGB_BUILTIN, r, g, b);
// showRainbow fonksiyonunun sonu.
}

// Bir isteği kuyruğa koyar. Kuyruk doluysa ya da LED henüz hazır değilse sessizce vazgeçer.
void enqueue(const Command &cmd) {
  // Kuyruk yoksa (setup bitmeden) bir şey yapma.
  if (!queue) return;
  // İsteği kuyruğun sonuna ekle; doluysa beklemeden (0) vazgeç.
  xQueueSend(queue, &cmd, 0);
// enqueue fonksiyonunun sonu.
}

// İsimsiz namespace'in sonu.
}  // namespace

// LED'i sabit bir renkte yakar (açıklaması led.h'de).
void ledSetColor(uint8_t r, uint8_t g, uint8_t b) {
  // Kuyruğu hazırla (sohbet başlamadan önce).
  ensureQueue();
  // Sabit renk moduna geç; renk geçişi durur.
  mode = Mode::Static;
  // LED'i istenen renkte yak.
  rgbLedWrite(RGB_BUILTIN, r, g, b);
// ledSetColor fonksiyonunun sonu.
}

// Renk geçişini başlatır (açıklaması led.h'de).
void ledStartRainbow() {
  // Kuyruğu hazırla (sohbet başlamadan önce).
  ensureQueue();
  // Renk geçişi moduna geç.
  mode = Mode::Rainbow;
  // Zaman sayacını şimdiden başlat.
  lastUpdate = millis();
  // İlk rengi hemen göster.
  showRainbow();
// ledStartRainbow fonksiyonunun sonu.
}

// Renk geçişini seçilen renkten sürdürür (açıklaması led.h'de).
void ledSetRainbowColor(uint8_t r, uint8_t g, uint8_t b) {
  // İstek: renk geçişini bu rengin açısından devam ettir.
  enqueue(Command{CommandType::SetHue, 0, 0, 0, rgbToHue(r, g, b)});
// ledSetRainbowColor fonksiyonunun sonu.
}

// Kısa yanıp sönme ister (açıklaması led.h'de).
void ledFlash(uint8_t r, uint8_t g, uint8_t b) {
  // İstek: kısa söndür, bu renkte yak, sonra renk geçişine dön.
  enqueue(Command{CommandType::Flash, r, g, b, 0});
// ledFlash fonksiyonunun sonu.
}

// LED'i günceller (açıklaması led.h'de).
void loopLed() {
  // Sabit durum rengindeysek (açılış/hata) dokunma.
  if (mode == Mode::Static) return;
  // Şu anki zaman (kart açıldığından beri geçen milisaniye).
  uint32_t now = millis();

  // Yanıp sönmenin "sönük" adımındaysak...
  if (mode == Mode::FlashOff) {
    // ...süresi dolduysa...
    if (now - stepStart >= FLASH_OFF_MS) {
      // ...LED'i istenen renkte (yeşil/kırmızı) yak.
      rgbLedWrite(RGB_BUILTIN, activeFlash.r, activeFlash.g, activeFlash.b);
      // "Yanık" adımına geç.
      mode = Mode::FlashOn;
      // Bu adımın başlangıç zamanını kaydet.
      stepStart = now;
    // if bloğunun sonu.
    }
    // Yanıp sönme sürerken renk geçişi ilerlemez.
    return;
  // if bloğunun sonu.
  }

  // Yanıp sönmenin "yanık" adımındaysak...
  if (mode == Mode::FlashOn) {
    // ...süresi dolduysa...
    if (now - stepStart >= FLASH_ON_MS) {
      // ...renk geçişine geri dön.
      mode = Mode::Rainbow;
      // Zaman sayacını şimdiye çek; böylece renk, yanıp sönme öncesinde kaldığı yerden devam eder.
      lastUpdate = now;
      // Kaldığı rengi hemen göster.
      showRainbow();
    // if bloğunun sonu.
    }
    // Yanıp sönme sürerken başka bir şey yapma.
    return;
  // if bloğunun sonu.
  }

  // Buraya gelindiyse renk geçişindeyiz. Kuyrukta bekleyen bir istek var mı bak.
  Command cmd;
  // Kuyruktan bir istek al; yoksa beklemeden (0) devam et.
  if (queue && xQueueReceive(queue, &cmd, 0) == pdTRUE) {
    // Renk seçimi isteğiyse...
    if (cmd.type == CommandType::SetHue) {
      // ...renk geçişini o açıya taşı.
      hue = cmd.hue;
      // Zaman sayacını şimdiye çek.
      lastUpdate = now;
      // Yeni rengi hemen göster.
      showRainbow();
      // Bu turluk iş bitti.
      return;
    // if bloğunun sonu.
    }
    // Yanıp sönme isteğiyse rengini hatırla...
    activeFlash = cmd;
    // ...LED'i söndür...
    rgbLedWrite(RGB_BUILTIN, 0, 0, 0);
    // ..."sönük" adımına geç...
    mode = Mode::FlashOff;
    // ...ve başlangıç zamanını kaydet.
    stepStart = now;
    // Bu turluk iş bitti.
    return;
  // if bloğunun sonu.
  }

  // Son güncellemeden bu yana 20 ms geçmediyse bekle (LED'i gereksiz yere sık güncellemeyelim).
  if (now - lastUpdate < UPDATE_INTERVAL_MS) return;
  // Geçen süre kadar açıyı ilerlet: RAINBOW_CYCLE_MS sürede 360 derece.
  hue += (now - lastUpdate) * 360.0f / RAINBOW_CYCLE_MS;
  // 360'ı geçtiyse başa sar (360 derece = 0 derece).
  while (hue >= 360.0f) hue -= 360.0f;
  // Güncelleme zamanını kaydet.
  lastUpdate = now;
  // LED'i yeni renkte yak.
  showRainbow();
// loopLed fonksiyonunun sonu.
}
