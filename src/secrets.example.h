// =====================================================================
// Modem bilgileri - ÖRNEK DOSYA
//
// Kullanmak için bu dosyayı aynı klasöre "secrets.h" adıyla kopyalayın ve
// tırnak içindeki değerleri kendi modeminizin ağ adı ve şifresiyle değiştirin.
// secrets.h, .gitignore'da olduğu için GitHub'a gönderilmez; şifreniz gizli kalır.
// =====================================================================

// Bu dosya bir derlemede birden fazla kez eklenirse içeriği yalnızca bir kez işlensin.
#pragma once

// Kartın bağlanacağı modemin Wi-Fi ağ adı (SSID). ESP32 sadece 2.4 GHz ağlara bağlanabilir.
#define WIFI_SSID "MODEMIN_AG_ADI"
// Modemin Wi-Fi şifresi.
#define WIFI_PASSWORD "MODEMIN_SIFRESI"
