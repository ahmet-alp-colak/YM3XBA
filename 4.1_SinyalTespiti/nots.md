

## 1. Konfigürasyon ve Ayarlar (Configuration)
* **Frekans ve Tarama Ayarları:** Sistem `START_FREQ` (1 MHz) ile `END_FREQ` (6 GHz) arasını `STEP_FREQ` (10 MHz) adımlarla tarar. HackRF saniyede 20 milyon örnek (`SAMPLE_RATE = 20000000`) toplar.
* **DSP (Sinyal İşleme) Ayarları:** Sinyaller 1024'lük parçalara (`FFT_SIZE`) bölünür. Welch yöntemi için bu parçalar %50 oranında (`OVERLAP = 512`) üst üste bindirilir.
* **Yapay Zeka Ayarları:** Sistem her sinyalden 8 farklı özellik (`FEATURES = 8`) çıkarır. Tüm spektrumu öğrenmek için 8 farklı küme (`GMM_COMPONENTS = 8`) kullanır ve öğrenme aşamasında toplam 2000 örnek (`TRAINING_SAMPLES`) toplar.
* **Dosya Yolları:** Sistemin öğrendiklerini unutmaması için kayıt dosyalarının isimleri (`MODEL_FILE` ve `SPECTRUM_FILE`) belirlenir.

---

## 2. Veri Yapıları (Data Structures)
* **`SpectrumBin`:** Kullanıcıya veya ekrana gösterilecek nihai harita verisini tutar. O frekansın doluluk oranını, gürültü tabanını, sinyalin tipini (0=Gürültü, 1=Burst/Anlık, 2=Sürekli), gücünü, geliş açısını (AoA) ve hedef coğrafi koordinatlarını (enlem/boylam) barındırır.
* **`GeoCoordinate`:** Coğrafi koordinat bilgisini tutar (enlem ve boylam). Çapraz kesiştirme algoritması sonucunda hesaplanan hedef konumunu temsil eder.
* **`SignalRecord`:** Veritabanına kaydedilecek tam sinyal kaydını içerir. Zaman damgası, frekans, bant genişliği, aktivite sınıfı, modülasyon türü, protokol, geliş açısı (AoA), hedef konumu (enlem/boylam) ve payload verilerini barındırır.
* **`TemporalState`:** Sistemin "Zaman Hafızası"dır. Her frekans için son 100 ölçümü (`likelihood_history`) hafızada tutar. Ortalama almak yerine sağlam istatistikler olan **Medyan** ve **MAD (Medyan Mutlak Sapma)** hesaplayarak, o frekansın dinamik gürültü tabanını (`adaptive_noise_floor`) ve sapmasını (`adaptive_noise_sigma`) bulur.
* **`GlobalRFEngine`:** Yapay zeka modelini (OpenCV EM) ve bu modelin ürettiği matrisleri (ağırlıklar, ortalamalar, ters kovaryanslar ve logaritmik determinantlar) tutan ana motor yapısıdır.

---

## 3. Kesintisiz Veri Akışı (Lock-Free SPSC Ring Buffer)
* **`LockFreeQueue`:** HackRF saniyede 40 MB veri üretirken, yapay zeka işlemcisi bu veriyi işlemekle meşgul olabilir. Bu yapı, işletim sistemini kilitlemeden (mutex kullanmadan) donanımdan gelen veriyi sıraya dizer. Eğer işlemci yetişemezse, programın çökmemesi için en eski veriyi sessizce düşürür. Gerçek zamanlı sistemlerin kalbidir.

---

## 4. Donanım İletişimi (Hardware Sweep & RX Callback)
* **`rx_callback`:** HackRF'ten yeni bir IQ verisi paketi geldiğinde donanım tarafından otomatik tetiklenen fonksiyondur. Gelen veriyi alır, `LockFreeQueue` içine atar. 15 blok veri toplandığında tarayıcı thread'e "ben işimi bitirdim, frekansı değiştirebilirsin" sinyalini (`is_capturing = false`) verir.
* **`hardware_sweep_thread`:** Donanıma frekans değiştirme emri verir. Frekans değiştikten sonra PLL kilitlenmesi ve analog filtrelerin oturması için fiziksel bir zorunluluk olan **40 milisaniye** bekler. Ardından okumayı başlatır ve `rx_callback` 15 blok toplayana kadar bekler. Sonra bir sonraki frekansa geçer.

---

## 5. Sinyal İşleme Katmanı (DSP Layer & Welch PSD)
* **`generate_hamming_window`:** Sinyalin başındaki ve sonundaki keskin kopmaları yumuşatarak FFT sızıntısını (leakage) önleyen matematiksel bir pencere (Hamming) üretir.
* **`compute_welch_psd`:** Gelen ham IQ verisini %50 örtüşecek şekilde dilimler (Welch Metodu). Her dilimi FFTW kütüphanesi ile frekans uzayına çevirir, gücünü hesaplar. Tüm dilimlerin ortalamasını alarak tertemiz bir Güç Spektrumu (PSD) çıkarır. Ayrıca anlık sinyalleri yakalamak için alt blok varyanslarını da (`sub_block_variances`) hesaplar.

---

## 6. Özellik Çıkarımı (Feature Engineering Layer)
* **`extract_feature_vector`:** FFT'den çıkan spektrumu alır ve yapay zekanın anlayabileceği 8 boyutlu, şehir gürültüsünden bağımsız (scale-invariant) bir parmak izine dönüştürür:
    1.  **Spektral Entropi:** Gürültü kaotiktir (yüksek), sinyal düzenlidir (düşük).
    2.  **Spektral Flatness:** Sinyal dar bantlı mı yoksa geniş mi?
    3.  **Crest Factor:** Anlık tepe gücünün ortalamaya oranı.
    4.  **Skewness (Çarpıklık):** Sinyal dağılımının asimetrisi.
    5.  **Kurtosis (Basıklık):** Sinyalin kuyruk kalınlığı.
    6.  **Micro-Variance:** Milisaniyelik zaman dilimindeki enerji dalgalanması (Wi-Fi gibi patlamalı sinyalleri yakalar).
    7.  **Relative Power:** Normalize edilmiş ortalama güç.
    8.  **Normalized Frequency:** Sinyalin 1 MHz - 6 GHz bandının neresinde bulunduğu.

---

## 7. Yapay Zeka Motoru (Unsupervised RF Model)
* **`train_global_gmm`:** Cihaz ilk açıldığında toplanan 2000 örnekle **Gaussian Mixture Model (GMM)** eğitilir. Model, matris çökmesini (Underflow/Singularity) engellemek için **SVD (Tekil Değer Ayrışımı)** kullanılarak kararlı hale getirilir. Ters kovaryanslar ve determinantlar işlemci hızını artırmak için önceden hesaplanır.
* **`compute_gmm_log_likelihood`:** Yeni gelen bir sinyalin 8 boyutlu özelliklerini alır ve eğitilmiş modele sorar: "Bu sinyalin şu anki ortama ait olma olasılığı nedir?". Mahalanobis uzaklığı üzerinden bir matematiksel olasılık puanı (Log-Likelihood) döndürür.

---

## 8. Hafıza ve Kalıcılık (Persistence Layer)
* **`load_rf_model`:** Program açıldığında `rf_scene_model.xml` dosyasını arar. Bulursa, daha önce eğitilmiş yapay zeka matrislerini RAM'e yükler ve 2000 örneklik keşif (eğitim) aşamasını atlar. Cihaz direkt avlanmaya başlar.
* **`save_rf_model`:** Program kapatılırken (kullanıcı ENTER'a bastığında), RAM'deki öğrenilmiş yapay zeka matrislerini tek tek XML dosyasına yazar. Böylece sistem öğrendiklerini unutmaz.

---

## 9. Karar Mekanizması (Decision Mechanism)
* **`classify_activity`:** Yeni gelen sinyalin olasılık puanını, `TemporalState` içindeki dinamik gürültü tabanı (Medyan) ve sapma (MAD) ile kıyaslar:
    * Puan yüksekse (Ortama çok benziyorsa): Bu normal bir gürültüdür (0).
    * Puan düşük ama arada sırada geliyorsa: Bu bir Burst (anlık/patlamalı) sinyaldir (1).
    * Puan çok düşük ve sürekli geliyorsa: Bu Sürekli/Kalıcı bir sinyaldir (2).

---

## 10. Yapay Zeka ve Karar Akışı (DSP Engine Thread)
* **`dsp_engine_thread`:** Programın ana beynidir. Kuyruktan IQ verisini çeker, DSP işlemini (`compute_welch_psd`) yapar, özelliklerini (`extract_feature_vector`) çıkarır.
    * **Faz 1:** Model eğitilmemişse 2000 örnek toplayıp eğitimi (`train_global_gmm`) başlatır.
    * **Faz 2:** Model hazırsa (veya XML'den yüklenmişse), anlık olasılığı hesaplar. `TemporalState` ile gürültü tabanını günceller. Sinyali EMA (Hareketli Ortalama) filtresinden geçirerek anlık sapmaları önler. Sinyalin doluluk oranı (occupancy) %80'i aşarsa, ekrana **[!!! SIGNAL DETECTED !!!]** uyarısını ve sinyalin tipini basar.

---

## 11. Ana Program (Main)
* Kullanıcıya karşılama metnini basar.
* `load_rf_model()` çağrılır, XML varsa model uyanır.
* HackRF cihazını başlatır, örnekleme hızını (20 MSPS) ve donanım kazançlarını ayarlar.
* Donanım okuma thread'i ile Yapay Zeka işlem thread'ini eşzamanlı olarak başlatır.
* Kullanıcı programı kapatmak için **ENTER** tuşuna basana kadar sonsuz döngüde çalışır.
* Kapatma emri geldiğinde thread'leri durdurur, öğrenilenleri diske yazar (`save_rf_model()`) ve donanım bağlantısını güvenli bir şekilde keserek kapanır.

---

## 12. Yön Bulma ve Konum Belirleme Entegrasyonu (Direction Finding & Geo-Location Integration)
* **Pseudo-Doppler Yön Bulma (DF):** Tespit edilen sinyaller için 4'lü dairesel anten dizisi ve yüksek hızlı RF anahtarlama kullanılarak Geliş Açısı (AoA - Angle of Arrival) hesaplanır. Yapay Doppler kayması yaratılarak faz karşılaştırması yapılır ve ±10° doğrulukla yön belirlenir.
* **Çapraz Kesiştirme (Cross-Fixing):** Minimum 2 dağıtık ED istasyonundan gelen AoA verileri birleştirilerek hedefin coğrafi koordinatları (enlem/boylam) En Küçük Kareler (Least Squares) yöntemiyle hesaplanır. LOB (Line of Bearing) doğrularının kesişimi optimize edilerek hedef konumu belirlenir.
* **Veri Yapıları:** `SpectrumBin` ve `SignalRecord` yapılarına `aoa`, `target_lat` ve `target_lon` alanları eklenmiştir. Bu sayede tespit edilen her sinyal için yön ve konum bilgisi saklanabilir.
* **Threading Mimarisi:** Pipeline'a Thread-4 (Direction Finding) ve Thread-5 (Geo-Location) eklenerek yön bulma ve konum belirleme işlemleri paralel olarak gerçekleştirilir.
* **UI Güncellemesi:** Operatör arayüzüne coğrafi harita katmanı eklenerek kestirilen hedef konumları ve LOB doğruları görselleştirilir.
