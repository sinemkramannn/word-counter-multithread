#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>
#include <thread>
#include <filesystem>
#include <poppler/cpp/poppler-document.h>
#include <poppler/cpp/poppler-page.h>
#include <unordered_set>
#include <zip.h>
#include <mutex>
#include <atomic> // std::atomic eklendi

// kutuphanaler eklendi
namespace fs = std::filesystem;
std::mutex wordsSetMutex;
std::atomic<int> fileCount(0); // Atomic sayaç oluşturuldu


// kelimeleri vektorde toplama ve alfabetik siraya gore siralama

void kelime_cikar_ekle_txt(const std::string& dosya_adi) {
    std::ifstream dosya(dosya_adi);
    if (!dosya) {
        std::cerr << "Dosya acilamadi: " << dosya_adi << std::endl;
        return;
    }

    std::vector<std::string> kelimeler;
    std::string kelime;
    while (dosya >> kelime) {
        std::transform(kelime.begin(), kelime.end(), kelime.begin(), ::tolower);
        if (std::find(kelimeler.begin(), kelimeler.end(), kelime) == kelimeler.end()) {
            kelimeler.push_back(kelime);
        }
    }
    dosya.close();
    std::sort(kelimeler.begin(), kelimeler.end());

    std::string dosya_adi_no_ext = fs::path(dosya_adi).stem();
    std::ofstream cikti(dosya_adi_no_ext + " Farkli Kelime Dosyasi.txt");
    for (const auto& kelime : kelimeler) {
        cikti << kelime << std::endl;
    }
    cikti.close();
    ++fileCount;
}
//fonksiyonu PDF dosyasından kelimeleri buluyor ve tekrar eden kelimeleri çıkartıyor
void pdf_kelime_bul_cikart_recursive(const std::string& dosya_adi, std::vector<std::string>& kelimeler, int sayfa_numarasi = 0) {
    poppler::document* dokuman = poppler::document::load_from_file(dosya_adi);
    if (!dokuman) {
        std::cout << "PDF dosyasi açilamadi." << std::endl;
        return;
    }

    if (sayfa_numarasi >= dokuman->pages()) {
        delete dokuman;
        return;
    }

    const poppler::page* sayfa = dokuman->create_page(sayfa_numarasi);
    if (!sayfa) {
        delete dokuman;
        return;
    }

    std::string metin = sayfa->text().to_latin1();
    delete sayfa;
    delete dokuman;

    std::istringstream iss(metin);
    std::string kelime;
    while (iss >> kelime) {
        std::transform(kelime.begin(), kelime.end(), kelime.begin(), ::tolower);
        if (std::find(kelimeler.begin(), kelimeler.end(), kelime) == kelimeler.end()) {
            kelimeler.push_back(kelime);
        }
    }
    //// Bir sonraki sayfaya geçmek için fonksiyonu kendisini çağırın
    pdf_kelime_bul_cikart_recursive(dosya_adi, kelimeler, sayfa_numarasi + 1);
}
//// İlk sayfadan başlayarak PDF dosyasındaki farklı kelimeleri bulmak için özyinelemeli fonksiyonu çağırın
void pdf_kelime_bul_cikart(const std::string& dosya_adi, std::vector<std::string>& kelimeler) {
    pdf_kelime_bul_cikart_recursive(dosya_adi, kelimeler);
}

void pdf_kelimeleri_txt_yaz(const std::string& dosya_adi) {
    std::vector<std::string> kelimeler;
    pdf_kelime_bul_cikart(dosya_adi, kelimeler);
    //PDF dosyasındaki farklı kelimeleri bulmak için fonksiyonu çağırın
    std::string dosya_adi_no_ext = dosya_adi.substr(0, dosya_adi.find_last_of('.'));
    std::ofstream cikti(dosya_adi_no_ext + " Farkli Kelime.txt");
    for (const auto& kelime : kelimeler) {
        cikti << kelime << std::endl;
    }
    cikti.close();
    ++fileCount;
}
void zipDosyasiniIsle(const std::string& zipDosyasi, std::unordered_set<std::string>& kelimeKumesi) {
    zip_t* arsiv = zip_open(zipDosyasi.c_str(), ZIP_RDONLY, nullptr); // ZIP dosyasını okuma modunda aç
    if (arsiv == nullptr) {
        //Eğer arsiv değeri nullptr ise, ZIP dosyasının açılamadığına dair bir hata mesajı yazdırılır ve fonksiyondan çıkılır.
        std::cerr << "ZIP dosyasi acilamadi: " << zipDosyasi << std::endl;
        return;
    }
    /// ZIP dosyasındaki girdi sayısını al
    int girdiSayisi = zip_get_num_entries(arsiv, ZIP_FL_UNCHANGED);
    if (girdiSayisi < 0) {
        // zip_get_num_entries fonksiyonuyla ZIP dosyasındaki girdi sayısı girdiSayisi değişkenine atanır. Eğer girdiSayisi değeri negatif ise, girdi sayısının alınamadığına dair bir hata mesajı yazdırılır, ZIP dosyası kapatılır ve fonksiyondan çıkılır.
        std::cerr << "ZIP dosyasindaki girdiler okunamadi: " << zipDosyasi << std::endl;
        zip_close(arsiv);
        return;
    }

    //// Dosyadan kelime kelime oku ve kelime kümesine ekle
    std::vector<std::thread> threadler;
    for (int i = 0; i < girdiSayisi; ++i) {
        zip_stat_t bilgi; //zip_stat_index fonksiyonuyla ZIP dosyasındaki girdi hakkında bilgi alınır ve bilgi değişkenine atanır. 
        if (zip_stat_index(arsiv, i, ZIP_FL_UNCHANGED, &bilgi) != 0) {
            std::cerr << "ZIP dosyasindaki girdi bilgileri alinamadi: " << zipDosyasi << std::endl; //eğer bilgi alınmazsa hata mesajı verilir
            zip_close(arsiv);
            return;
        }
        //zip_fopen_index fonksiyonuyla girdiye ait dosya açılır ve dosya değişkenine atanır. 
        zip_file_t* dosya = zip_fopen_index(arsiv, i, ZIP_FL_UNCHANGED);
        if (dosya == nullptr) {//açılmazsa
            std::cerr << "ZIP dosyasi acilamadi: " << bilgi.name << std::endl;
            zip_close(arsiv);
            return;
        }
        //Her karakter zip_fread fonksiyonu kullanılarak okunur. Eğer okunan karakter alfanümerik ise, kelimeye eklenir 
        std::string kelime;
        char c;
        while (zip_fread(dosya, &c, 1) == 1) {
            if (isalnum(c)) {
                kelime += tolower(c); //tolower kullanarak karakterin küçük harfli hali elde edilir
            } else {
                if (!kelime.empty())//Okunan karakter alfanümerik değilse, kelime kontrol edilir. Eğer kelime boş değilse, kelime kümesine (kelimeKumesi) eklenir 
                    kelimeKumesi.insert(kelime); //ve kelime sıfırlanır.
                kelime.clear();
            }
        }
        if (!kelime.empty()) { //Döngü sonunda, kelime hala doluysa (dosyanın sonunda kelime varsa), kelime kümesine eklenir. 
            std::lock_guard<std::mutex> lock(wordsSetMutex); //Bu işlem, wordsSetMutex adlı mutex kullanılarak eşzamanlı erişim kontrolü yapılır.
            kelimeKumesi.insert(kelime);
        }

        zip_fclose(dosya);
    }

    zip_close(arsiv);
    ++fileCount;
}
void FarkliKelimeleriDosyayaYaz_zip(const std::unordered_set<std::string>& kelimeKumesi, const std::string& ciktiDosyasi) {
    std::ofstream cikti(ciktiDosyasi);
    if (!cikti) {
        std::cerr << "Cikti dosyasi olusturulamadi: " << ciktiDosyasi << std::endl;
        return;
    }

    std::vector<std::string> kelimeler;
    for (const auto& kelime : kelimeKumesi) {
        kelimeler.push_back(kelime);
    }
    std::sort(kelimeler.begin(), kelimeler.end());

    for (const auto& kelime : kelimeler) {
        cikti << kelime << std::endl;
    }

    cikti.close();
}
int main() {

    unsigned int threadSayisi = std::thread::hardware_concurrency();

    if (threadSayisi == 0) {
        std::cout << "Thread sayisi belirlenemedi." << std::endl;
    } else {
        std::cout << "Kullanilabilir thread sayisi: " << threadSayisi << std::endl;
    }

    std::vector<std::thread> threads; // Ana thread 
    //Txt dosyasinin islenmesi
    std::string dosya_adi_txt = "dosya1.txt";
    std::thread kelime(kelime_cikar_ekle_txt, dosya_adi_txt); // Ayni kelimeleri bulmak icim diger is parcacigi olusuturuldu.
    threads.push_back(std::move(kelime));
    //kelime.join(); // thread bitmesini bekleriz

    // Zip dosyasının işlenmesi işlemi
    std::string zipDosyasi = "dosya2.zip";
    std::unordered_set<std::string> kelimeKumesi;
    std::string ciktiDosyasi = "dosya2.txt Farkli Kelimeler Txt";
    std::thread zipThread(zipDosyasiniIsle, zipDosyasi, std::ref(kelimeKumesi)); // ZIP dosyasını işlemek için thread kullanarak fonksiyonu çağırın
    zipThread.join(); // Ana thread'in devam etmesini bekleyin
    FarkliKelimeleriDosyayaYaz_zip(kelimeKumesi, ciktiDosyasi); // Benzersiz kelimeleri txt dosyasına yaz

    // pdf dosyasına yazılması işlemi
    std::string dosya_adi = "dosya3.pdf";
    std::thread pdfThread(pdf_kelimeleri_txt_yaz, dosya_adi);
    threads.push_back(std::move(pdfThread));


    // Tüm thread'lerin tamamlanması beklenir
    for (auto& thread : threads) {
        thread.join();
    }

    std::cout << " Tüm yazilma islemleri tamamlandı." << std::endl;
    std::cout << "Toplam dosya sayisi: " << fileCount << std::endl; // Dosya sayısını yazdır

    
    return 0;
}     