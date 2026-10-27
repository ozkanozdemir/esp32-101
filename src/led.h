// =====================================================================
// Karttaki renkli LED (WS2812)
//
// Açılışta durumu gösterir (turuncu: açılıyor, kırmızı: hata). Kart hazır
// olunca tüm renkleri yavaşça, yumuşak geçişlerle dolaşmaya başlar.
//
// Sohbetten gelen olaylar:
//   - Biri LED rengini seçerse, renk geçişi o renkten itibaren devam eder.
//   - Biri katılınca LED kısa bir an söner, yeşil yanar, kaldığı renkten devam eder.
//   - Biri ayrılınca aynısı kırmızıyla olur.
//
// Kodun kendisi led.cpp dosyasında.
// =====================================================================

// Bu dosya bir derlemede birden fazla kez eklenirse içeriği yalnızca bir kez işlensin.
#pragma once

// uint8_t gibi sayı türleri için.
#include <stdint.h>

// LED'i sabit bir renkte yakar (r, g, b: 0-255). Renk geçişi çalışıyorsa onu durdurur.
// Sadece setup() içinden (açılış/hata durumları için) çağrılır.
void ledSetColor(uint8_t r, uint8_t g, uint8_t b);

// LED'in tüm renkleri yavaşça dolaşmasını başlatır. setup() sonunda çağrılır.
void ledStartRainbow();

// Renk geçişini bu renkten itibaren sürdürür (sohbetteki renk seçiciden).
// Sohbet kodu başka bir görevde (task) çalıştığı için istek kuyruğa konur, loopLed() uygular.
void ledSetRainbowColor(uint8_t r, uint8_t g, uint8_t b);

// LED'i kısa bir an söndürüp verilen renkte yakar, sonra renk geçişi kaldığı yerden devam eder.
// Arka arkaya gelen istekler sırayla, birbirini ezmeden gösterilir.
void ledFlash(uint8_t r, uint8_t g, uint8_t b);

// LED'i günceller (renk geçişi, yanıp sönme, kuyruktaki istekler). loop() içinden sürekli çağrılır.
void loopLed();
