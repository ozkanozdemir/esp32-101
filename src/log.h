// =====================================================================
// Terminale (seri monitöre) mesaj yazma
//
// Kartta iki USB-C girişi var: "USB" (çipin kendi USB'si) ve "COM" (ayrı bir
// USB-seri çipi). Buradaki fonksiyonlar mesajları İKİSİNE BİRDEN yazar; kablo
// hangisine takılıysa terminalde görünür. Kodun kendisi log.cpp dosyasında.
// =====================================================================

// Bu dosya bir derlemede birden fazla kez eklenirse içeriği yalnızca bir kez işlensin.
#pragma once

// Her iki girişi de başlatır. setup()'ın en başında bir kez çağrılır.
void logBegin();

// printf gibi biçimli bir mesajı her iki girişe yazar. Örnek: logf("IP: %s\n", ip);
void logf(const char *format, ...);

// Terminalden bir tuşa (örneğin Enter) basıldıysa true döndürür ve gelen tuşları temizler.
bool logKeyPressed();
