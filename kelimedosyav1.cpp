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
namespace fs = std::filesystem; //std::filesystem isim alanı fs olarak kısaltıldı
std::mutex wordsSetMutex; //wordsSetMutex kelime kümesine eşzamanlı erişimi kontrol etmek için bir mutex oluşturuldu
std::atomic<int> fileCount(0); // Atomic sayaç oluşturuldu.FileCount, dosya sayısını atomik bir şekilde artırmak için kullanılan bir sayacı temsil eder.


// kelimeleri vektorde toplama ve alfabetik siraya gore siralama

void kelime_cikar_ekle_txt(const std::string& dosya_adi) {
    std::ifstream dosya(dosya_adi); //kelime_cikar_ekle_txt fonksiyonu, bir metin dosyasını okur.
    if (!dosya) {
        std::cerr << "Dosya acilamadi: " << dosya_adi << std::endl;
        return;
    }

    std::vector<std::string> kelimeler; //Dosyadan okunan her kelime, küçük harflere dönüştürülerek kelimeler vektörüne eklenir. 
    std::string kelime; //Aynı kelime birden fazla kez bulunursa, sadece bir kopyası vektöre eklenir. Son olarak, kelimeler alfabetik olarak sıralanır.
    while (dosya >> kelime) {
        std::transform(kelime.begin(), kelime.end(), kelime.begin(), ::tolower);
        if (std::find(kelimeler.begin(), kelimeler.end(), kelime) == kelimeler.end()) {
            kelimeler.push_back(kelime);
        }
    }
    dosya.close();
    std::sort(kelimeler.begin(), kelimeler.end()); //sıralama yapılır

    std::string dosya_adi_no_ext = fs::path(dosya_adi).stem(); //Elde edilen farklı kelimeler, çıktı dosyasına yazılır. . 
    std::ofstream cikti(dosya_adi_no_ext + " Farkli Kelime Dosyasi.txt"); //Dosya adı, orijinal dosyanın adının uzantısı olmadan alınır ve sonuna " Farkli Kelime Dosyasi.txt" eklenir
    for (const auto& kelime : kelimeler) {
        cikti << kelime << std::endl;
    }
    cikti.close();
    ++fileCount; //fileCount sayacı bir artırılır.
}
//fonksiyonu PDF dosyasından kelimeleri buluyor ve tekrar eden kelimeleri çıkartıyor
void pdf_kelime_bul_cikart(const std::string& dosya_adi, std::vector<std::string>& kelimeler) { 
    poppler::document* dokuman = poppler::document::load_from_file(dosya_adi);
    if (!dokuman) {
        std::cout << "PDF dosyasi açilamadi." << std::endl;
        return;
    }

    std::string metin;
    for (int sayfa_numarasi = 0; sayfa_numarasi < dokuman->pages(); ++sayfa_numarasi) {//dokuman üzerinden sayfa_numarasi'na göre bir poppler::page oluşturulur. Bu, PDF sayfasının bir temsilidir.
        const poppler::page* sayfa = dokuman->create_page(sayfa_numarasi); //
        if (!sayfa) { 
            continue;
        }

        metin += sayfa->text().to_latin1(); //eğer sayfa nullptr değilse (yani geçerli bir sayfaysa), sayfa üzerindeki metin to_latin1() işleviyle alınır ve metin değişkenine eklenir.
        delete sayfa; // sayfayı sertbest bırakır
    }

    delete dokuman; // döküman serbest bırakılır
    
    //Bu kod bloğu, metin adlı bir std::string içeriğindeki kelimeleri bulur, kelimeleri küçük harflere dönüştürür, 
    //tekrar eden kelimeleri çıkartır ve ardından kelimeleri alfabetik olarak sıralar.
    std::istringstream iss(metin); //bu sınıf kullanılarak metin adında bir string oluşturulur ve kelime kelime okumamızı sağlar.
    std::string kelime;
    while (iss >> kelime) { //Sonra, while döngüsü içinde iss akışından kelimeler okunur.
        std::transform(kelime.begin(), kelime.end(), kelime.begin(), ::tolower); //transform ile küçük harflere dönüştürüldü.
        if (std::find(kelimeler.begin(), kelimeler.end(), kelime) == kelimeler.end()) {//find ile vekötrde arama yapılır eğer bulunmuyorsa kelime eklenir
            kelimeler.push_back(kelime);
        }
    }

    std::sort(kelimeler.begin(), kelimeler.end()); //döngü bittikten sonra alfabetik sıraya göre sıralanır.
}
// fonksiyonu bu işlevi PDF dosyası için çağırıyor ve sıralanmış kelimeleri dosyaya yazdırıyor. 
void pdf_kelimeleri_txt_yaz(const std::string& dosya_adi) { 
    std::vector<std::string> kelimeler;//std::vector<std::string> olan kelimeler vektörü oluşturulur.
    pdf_kelime_bul_cikart(dosya_adi, kelimeler); // fonksiyon çağırıldı.

    std::string dosya_adi_no_ext = dosya_adi.substr(0, dosya_adi.find_last_of('.'));//dosya_adi_no_ext adında bir std::string oluşturulur ve dosya_adi string'inden dosya uzantısı çıkarılarak elde edilir.
    //Örneğin, "dosya.pdf" adlı bir PDF dosyası için "dosya" adı elde edilir
    std::ofstream cikti(dosya_adi_no_ext + " Farkli Kelime.txt"); //dosya_adi_no_ext string'ine " Farkli Kelime.txt" eklenir ve tam bir çıktı dosya adı oluşturulur.
    for (const auto& kelime : kelimeler) {//td::ofstream sınıfı kullanılarak cikti adlı bir çıktı dosyası oluşturulur buradaki kelimeler vekötüründe her bir kelime için for döngüsüyle yazılır.
        cikti << kelime << std::endl;
    }
    cikti.close();
    ++fileCount; //işlenen dosya sayısı 1 arttırılır.
}
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
    //Bilgisyarda kaç thread var onu verir bize.
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
   // kelime.join(); // thread bitmesini bekleriz

    // Zip dosyasının işlenmesi işlemi
    std::string zipDosyasi = "dosya2.zip";
    std::unordered_set<std::string> kelimeKumesi;
    std::string ciktiDosyasi = "dosya2.txt Farkli Kelimeler Txt";
    std::thread zipThread(zipDosyasiniIsle, zipDosyasi, std::ref(kelimeKumesi)); // ZIP dosyasını işlemek için thread kullanarak fonksiyonu çağırın
    //threads.push_back(std::move(zipThread));
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

    std::cout << "Tüm yazilma islemleri tamamlandı." << std::endl;
    std::cout << "Toplam dosya sayisi: " << fileCount << std::endl; // Dosya sayısını yazdır

    
    return 0;
}