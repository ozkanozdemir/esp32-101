// =====================================================================
// Web sunucusu
//
// 80. portta çalışır. Site dosyalarını (LittleFS'teki data/site klasörü)
// tarayıcılara sunar ve sohbet odasının WebSocket'ini (/ws) barındırır.
// Kodun kendisi web_server.cpp dosyasında.
// =====================================================================

// Bu dosya bir derlemede birden fazla kez eklenirse içeriği yalnızca bir kez işlensin.
#pragma once

// Web sunucusunu (site dosyaları + sohbet WebSocket'i) hazırlayıp başlatır.
// LittleFS bağlandıktan ve Wi-Fi yayını başladıktan sonra çağrılmalı.
void setupWebServer();

// Web sunucusunun düzenli yapılması gereken işleri. loop() içinden çağrılır.
void loopWebServer();
