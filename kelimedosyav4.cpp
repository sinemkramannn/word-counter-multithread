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
void kelimeleri_sirala(const std::string& dosya_adi, std::vector<std::string>* kelimeler) { //fonksiyonu, std::vector<std::string>* kelimeler parametresi aracılığıyla bir vektör pointer'ını alır.
    std::ifstream dosya(dosya_adi); //
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

void dosya_ekleme_vector(const std::string& dosya_adi, std::vector<std::string>* kelimeler) {//std::vector<std::string>* kelimeler parametresi aracılığıyla bir vektör pointer'ını alır.
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
void pdf_kelime_bul_cikart(const std::string& dosya_adi, std::vector<std::string>* kelimeler) { //fonksiyonu, std::vector<std::string>* kelimeler parametresi aracılığıyla bir vektör pointer'ını alır.
    poppler::document* dokuman = poppler::document::load_from_file(dosya_adi);
    if (!dokuman) {
        std::cout << "PDF dosyasi acilamadi." << std::endl;
        return;
    }

    std::string metin;
    for (int sayfa_numarasi = 0; sayfa_numarasi < dokuman->pages(); ++sayfa_numarasi) {//dokuman->pages() ile sayfa sayısını alır ve her sayfa için işlem yapar
        const poppler::page* sayfa = dokuman->create_page(sayfa_numarasi); // sayfanın bir kopyası oluşturulur
        if (!sayfa) {
            continue;
        }

        metin += sayfa->text().to_latin1(); //sayfa->text().to_latin1() ifadesiyle sayfanın metni to_latin1() fonksiyonu kullanılarak alınır ve metin değişkenine eklenir. Bu işlem, sayfaların metinlerinin birleştirilerek tek bir metin oluşturulmasını sağlar.
        delete sayfa;
    }

    delete dokuman;

    std::istringstream iss(metin);//Metin string'i, std::istringstream sınıfı kullanılarak bir iss nesnesine dönüştürülür. Bu, metni okumak için bir akış sağlar
    std::string kelime;
    while (iss >> kelime) { //Bir döngü kullanarak, iss akışından kelimeler okunur ve kelime değişkenine atanır. Döngü, tüm kelimeler okunana kadar devam eder.
        std::transform(kelime.begin(), kelime.end(), kelime.begin(), ::tolower); //üçük harfe dönüştürür
        if (std::find(kelimeler->begin(), kelimeler->end(), kelime) == kelimeler->end()) { //ğer okunan kelime, kelimeler vektöründe bulunmuyorsa (std::find(kelimeler->begin(), kelimeler->end(), kelime) == kelimeler->end() durumu), kelime vektöre eklenir 
            kelimeler->push_back(kelime);
        }
    }

    std::sort(kelimeler->begin(), kelimeler->end());//sıralama yapılır
}
// fonksiyonu bu işlevi PDF dosyası için çağırıyor ve sıralanmış kelimeleri dosyaya yazdırıyor. 
void pdf_kelimeleri_txt_yaz(const std::string& dosya_adi) {
    std::vector<std::string>* kelimeler = new std::vector<std::string>(); //std::vector<std::string>* kelimeler adlı bir vektör pointer'ı oluşturulur.Buraya depo edilir
    pdf_kelime_bul_cikart(dosya_adi, kelimeler);//pdf_kelime_bul_cikart fonksiyonu kullanılarak dosya_adi ile belirtilen PDF dosyasından kelimeler bulunur ve kelimeler vektörüne eklenir.

    std::string dosya_adi_no_ext = dosya_adi.substr(0, dosya_adi.find_last_of('.')); //Ardından, dosya_adi string'inin uzantısız hali dosya_adi_no_ext değişkenine atanır.
    std::ofstream cikti(dosya_adi_no_ext + " Farkli Kelime.txt");//Ardından, dosya_adi string'inin uzantısız hali dosya_adi_no_ext değişkenine atanır,Farkli Kelime.txt" ifadesi birleştirilerek bir çıktı dosyasının adı oluşturulur.
    for (const auto& kelime : *kelimeler) { //kelimeler vektöründeki her bir kelimeyi döngüyle gezerek, cikti dosyasına yazılır.
        cikti << kelime << std::endl;
    }
    cikti.close();
    ++fileCount;

    delete kelimeler;//serbest bırakılır
}
//fonksiyonu, verilen bir zip dosyasından kelimeleri çıkaran bir işlevdir.
void zipDosyasindanKelimeleriCikar(zip_t* arsiv, const char* girdiAdi, std::unordered_set<std::string>& kelimeKumesi) { //fonksiyonunda, zip_t* arsiv adlı bir zip dosyası işaretçisi kullanılır.
    zip_file_t* dosya = zip_fopen(arsiv, girdiAdi, ZIP_FL_UNCHANGED);//Fonksiyon, zip_fopen fonksiyonu kullanılarak arsiv içindeki girdiAdi girdisini açar ve bir zip_file_t* (zip dosyası işaretçisi) nesnesini döndürür.
    if (dosya == nullptr) {//açılmazsa
        std::cerr << "ZIP dosyasi acilamadi: " << girdiAdi << std::endl;
        return;
    }

    std::string kelime;
    char c;
    while (zip_fread(dosya, &c, 1) == 1) {
        if (isalnum(c)) { // alfa numerik karakterse (isalnum(c) kontrolü), karakter küçük harfe dönüştürülerek kelime stringine eklenir.
            kelime += tolower(c);
        } else {//değil isekelime tamamlandığı kabul edilir ve kelimeKumesi adlı std::unordered_set<std::string> yapısına eklenir.
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

void zipDosyasiniIsle(const std::string& zipDosyasi, std::unordered_set<std::string>& kelimeKumesi) {//fonksiyonu, verilen bir zip dosyasını işleyen ve içindeki kelimeleri çıkaran bir işlevdir.
    zip_t* arsiv = zip_open(zipDosyasi.c_str(), ZIP_RDONLY, nullptr);//zip_open fonksiyonu kullanılarak zipDosyasi.c_str() ile belirtilen zip dosyasını açar ve bir zip_t* (zip dosyası işaretçisi) nesnesini döndürür.
    if (arsiv == nullptr) {
        std::cerr << "ZIP dosyasi acilamadi: " << zipDosyasi << std::endl;
        return;
    }

    int girdiSayisi = zip_get_num_entries(arsiv, ZIP_FL_UNCHANGED);//get_num_entries fonksiyonu kullanılarak zip dosyasındaki girdi sayısı (girdiSayisi) elde edilir.
    if (girdiSayisi < 0) {//ğer girdi sayısı geçersizse 
        std::cerr << "ZIP dosyasindaki girdiler okunamadi: " << zipDosyasi << std::endl;
        zip_close(arsiv);
        return;
    }

    std::vector<std::thread> threadler; //Bu vektör, kelimeleri çıkarmak için oluşturulan thread'leri tutmak için kullanılır.
    for (int i = 0; i < girdiSayisi; ++i) {
        zip_stat_t bilgi; //zip_stat_index fonksiyonu kullanılarak zip dosyasındaki girdinin bilgileri (bilgi) alınır.
        if (zip_stat_index(arsiv, i, ZIP_FL_UNCHANGED, &bilgi) != 0) {
            std::cerr << "ZIP dosyasindaki girdi bilgileri alinamadi: " << zipDosyasi << std::endl;
            zip_close(arsiv);
            return;
        }

        std::thread t(zipDosyasindanKelimeleriCikar, arsiv, bilgi.name, std::ref(kelimeKumesi));//zipDosyasindanKelimeleriCikar fonksiyonu bir thread olarak çağrılır ve oluşturulan thread vektörüne eklenir.
        threadler.push_back(std::move(t));
    }

    for (auto& t : threadler) {
        t.join();//kullanılan threadlarin sonlanması bekleniyor
    }

    zip_close(arsiv);
    ++fileCount;
}

void FarkliKelimeleriDosyayaYaz_zip(const std::unordered_set<std::string>& kelimeKumesi, const std::string& ciktiDosyasi) {//std::unordered_set<std::string> yapısındaki farklı kelimeleri bir dosyaya yazan bir işlevdir.
    std::ofstream cikti(ciktiDosyasi);
    if (!cikti) {
        std::cerr << "Cikti dosyasi olusturulamadi: " << ciktiDosyasi << std::endl;
        return;
    }

    std::vector<std::string> kelimeler;//kelimeKumesi adlı std::unordered_set<std::string> yapısında bulunan kelimeleri işlemek için kullanılır.
    for (const auto& kelime : kelimeKumesi) {
        kelimeler.push_back(kelime);
    }
    std::sort(kelimeler.begin(), kelimeler.end());//sıralama yapar

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
    std::vector<std::string> kelimeler;
    //Txt dosyasinin islenmesi
    std::string dosya_adi_txt = "dosya1.txt";
    threads.emplace_back(dosya_ekleme_vector, dosya_adi_txt, &kelimeler); // Ayni kelimeleri bulmak icim diger is parcacigi olusuturuldu.
   
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