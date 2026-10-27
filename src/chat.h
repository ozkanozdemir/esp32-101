// =====================================================================
// Sohbet odası (WebSocket)
//
// Sayfa, kartla WebSocket (ws://ozkan.local/ws) üzerinden sürekli açık bir
// bağlantı kurar. Kullanıcı bir isim girip odaya katılır; kart odadakilerin
// listesini herkese gönderir ve yazılan her mesajı odadaki herkese iletir.
//
// Sayfa ile kart arasındaki mesajlar JSON biçimindedir:
//   Sayfa -> kart:  {"type":"join","name":"Ali","token":"..."}   odaya katıl
//                   {"type":"chat","text":"Merhaba"}              mesaj gönder
//                   {"type":"leave"}                              odadan çık
//   Kart -> sayfa:  {"type":"welcome","name":"Ali","token":"..."} katılım onayı
//                   {"type":"history","messages":[...]}            son mesajlar
//                   {"type":"users","users":["Ali","Ayşe"]}        odadakiler
//                   {"type":"chat","from":"Ali","text":"Merhaba"}  yeni mesaj
//                   {"type":"system","text":"Ali katıldı"}          bilgi mesajı
//                   {"type":"error","code":"...","text":"..."}     hata
//
// Kodun kendisi chat.cpp dosyasında. Bu dosya sadece dışarıdan (web_server.cpp)
// kullanılan iki fonksiyonu tanıtır.
// =====================================================================

// Bu dosya bir derlemede birden fazla kez eklenirse içeriği yalnızca bir kez işlensin.
#pragma once

// AsyncWebServer türünü tanımak için web sunucusu kütüphanesi.
#include <ESPAsyncWebServer.h>

// Sohbet odasını hazırlar ve "/ws" adresindeki WebSocket'i verilen web sunucusuna ekler.
// Web sunucusu başlatılmadan (server.begin()) önce çağrılmalı.
void setupChat(AsyncWebServer &server);

// Sohbetin düzenli yapılması gereken işleri (kopmuş bağlantıların temizliği). loop() içinden çağrılır.
void loopChat();
