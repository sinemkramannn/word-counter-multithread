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
void kelimeleri_sirala(const std::string& dosya_adi, std::vector<std::string>* kelimeler) {
    std::ifstream dosya(dosya_adi);
    std::string kelime;
    while (dosya >> kelime) {
        std::transform(kelime.begin(), kelime.end(), kelime.begin(), ::tolower);
        if (std::find(kelimeler->begin(), kelimeler->end(), kelime) != kelimeler->end()) {
            continue;
        }
        else {
            kelimeler->push_back(kelime);
        }
    }
    dosya.close();
}

void dosya_ekleme_vector(const std::string& dosya_adi, std::vector<std::string>* kelimeler) {
    kelimeleri_sirala(dosya_adi, kelimeler);

    std::string dosya_adi_no_ext = fs::path(dosya_adi).stem();
    std::ofstream cikti(dosya_adi_no_ext + " Farkli Kelime Dosyasi.txt");
    for (const auto& kelime : *kelimeler) {
        cikti << kelime << std::endl;
    }
    cikti.close();
    ++fileCount;
}

//fonksiyonu PDF dosyasından kelimeleri buluyor ve tekrar eden kelimeleri çıkartıyor
void pdf_kelime_bul_cikart(const std::string& dosya_adi, std::vector<std::string>* kelimeler) {
    poppler::document* dokuman = poppler::document::load_from_file(dosya_adi);
    if (!dokuman) {
        std::cout << "PDF dosyasi acilamadi." << std::endl;
        return;
    }

    std::string metin;
    for (int sayfa_numarasi = 0; sayfa_numarasi < dokuman->pages(); ++sayfa_numarasi) {
        const poppler::page* sayfa = dokuman->create_page(sayfa_numarasi);
        if (!sayfa) {
            continue;
        }

        metin += sayfa->text().to_latin1();
        delete sayfa;
    }

    delete dokuman;

    std::istringstream iss(metin);
    std::string kelime;
    while (iss >> kelime) {
        std::transform(kelime.begin(), kelime.end(), kelime.begin(), ::tolower);
        if (std::find(kelimeler->begin(), kelimeler->end(), kelime) == kelimeler->end()) {
            kelimeler->push_back(kelime);
        }
    }

    std::sort(kelimeler->begin(), kelimeler->end());
}

// fonksiyonu bu işlevi PDF dosyası için çağırıyor ve sıralanmış kelimeleri dosyaya yazdırıyor.
void pdf_kelimeleri_txt_yaz(const std::string& dosya_adi, std::vector<std::string>* kelimeler) {
    pdf_kelime_bul_cikart(dosya_adi, kelimeler);

    std::string dosya_adi_no_ext = dosya_adi.substr(0, dosya_adi.find_last_of('.'));
    std::ofstream cikti(dosya_adi_no_ext + " Farkli Kelime.txt");
    for (const auto& kelime : *kelimeler) {
        cikti << kelime << std::endl;
    }
    cikti.close();
    ++fileCount;
}

void pdfThread1Func(const std::string& dosya_adi, std::vector<std::string>* kelimeler) {
    pdf_kelime_bul_cikart(dosya_adi, kelimeler);
}

// PDF dosyasını işleyen thread 2
void pdfThread2Func(const std::string& dosya_adi, std::vector<std::string>* kelimeler) {
    pdf_kelime_bul_cikart(dosya_adi, kelimeler);
}

void zipDosyasindanKelimeleriCikar(zip_t* arsiv, const char* girdiAdi, std::unordered_set<std::string>& kelimeKumesi) {
    zip_file_t* dosya = zip_fopen(arsiv, girdiAdi, ZIP_FL_UNCHANGED);
    if (dosya == nullptr) {
        std::cerr << "ZIP dosyasi acilamadi: " << girdiAdi << std::endl;
        return;
    }

    std::string kelime;
    char c;
    while (zip_fread(dosya, &c, 1) == 1) {
        if (isalnum(c)) {
            kelime += tolower(c);
        }
        else {
            if (!kelime.empty())
                kelimeKumesi.insert(kelime);
            kelime.clear();
        }
    }
    if (!kelime.empty()) {
        std::lock_guard<std::mutex> lock(wordsSetMutex);
        kelimeKumesi.insert(kelime);
    }

    zip_fclose(dosya);
}

void zipDosyasiniIsle(const std::string& zipDosyasi, std::unordered_set<std::string>& kelimeKumesi) {
    zip_t* arsiv = zip_open(zipDosyasi.c_str(), ZIP_RDONLY, nullptr);
    if (arsiv == nullptr) {
        std::cerr << "ZIP dosyasi acilamadi: " << zipDosyasi << std::endl;
        return;
    }

    int girdiSayisi = zip_get_num_entries(arsiv, ZIP_FL_UNCHANGED);
    if (girdiSayisi < 0) {
        std::cerr << "ZIP dosyasindaki girdiler okunamadi: " << zipDosyasi << std::endl;
        zip_close(arsiv);
        return;
    }

    for (int i = 0; i < girdiSayisi; ++i) {
        zip_stat_t bilgi;
        if (zip_stat_index(arsiv, i, ZIP_FL_UNCHANGED, &bilgi) != 0) {
            std::cerr << "ZIP dosyasindaki girdi bilgileri alinamadi: " << zipDosyasi << std::endl;
            zip_close(arsiv);
            return;
        }

        zipDosyasindanKelimeleriCikar(arsiv, bilgi.name, kelimeKumesi);
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
    }
    else {
        std::cout << "Kullanilabilir thread sayisi: " << threadSayisi << std::endl;
    }

    std::vector<std::string> kelimeler;

    // Txt dosyasinin islenmesi
    std::string dosya_adi_txt = "dosya1.txt";
    std::thread txt(dosya_ekleme_vector, dosya_adi_txt, &kelimeler);
    txt.join();
    
    // Zip dosyasının işlenmesi işlemi
    std::string zipDosyasi = "dosya2.zip";
    std::unordered_set<std::string> kelimeKumesi;
    std::string ciktiDosyasi = "dosya2.txt Farkli Kelimeler Txt";
    std::thread zipThread(zipDosyasiniIsle, zipDosyasi, std::ref(kelimeKumesi)); // ZIP dosyasını işlemek için thread kullanarak fonksiyonu çağırın
    zipThread.join(); // Ana thread'in devam etmesini bekleyin
    FarkliKelimeleriDosyayaYaz_zip(kelimeKumesi, ciktiDosyasi); // Benzersiz kelimeleri txt dosyasına yaz

    // pdf dosyasına yazılması işlemi
    std::string dosya_adi = "dosya3.pdf";
    std::vector<std::string> pdfKelimeler1;
    std::vector<std::string> pdfKelimeler2;
    std::thread pdfThread1(pdf_kelimeleri_txt_yaz, dosya_adi, &pdfKelimeler1);
    std::thread pdfThread2(pdf_kelimeleri_txt_yaz, dosya_adi, &pdfKelimeler2);

    pdfThread1.join();
    pdfThread2.join();

    std::cout << " Tüm yazma işlemleri tamamlandı." << std::endl;
    std::cout << "Toplam dosya sayısı: " << fileCount << std::endl; // Dosya sayısını yazdırma

    return 0;
}

