# CyberPUF: PUF Tabanlı Uç Yapay Zeka (Edge-AI) Model Ağırlığı Şifrelemesi

![CyberPUF Web Dashboard](ekran_goruntuleri/1.png)
[Türkçe](#turkce) | [English](#english)

---

<a name="turkce"></a>
## Türkçe

**Geliştirici:** Arda Meçik (AltaySec bünyesinde geliştirilmiştir)
**Sürüm:** v4.0.0-Gold (Edge-AI Tam Entegrasyon & Siber Güvenlik Modülleri)
**Lisans:** MIT

### 🚀 Hızlı Başlangıç (Quick Start)

#### Kurulum (Installation)

```bash
# Bağımlılıkları yükle (pytest-asyncio dahil)
pip install -r requirements.txt

# Environment dosyasını oluştur
cp .env.example .env

# .env dosyasını düzenle (güvenli token oluştur)
# Linux/Mac:
nano .env
# Windows:
# Not: .env dosyasında CYBERPUF_AES_KEY ve WEBSOCKET_TOKEN'ı güvenli değerlerle doldur
```

#### Çalıştırma (Running)

```bash
# Uygulamayı başlat:
python start_app.py

# Tarayıcıda açın:
# http://127.0.0.1:8000
```

---

CyberPUF, uç cihazlarda (FPGA ve SoC mimarileri gibi) konuşlandırılan yapay sinir ağı ağırlıklarının fikri mülkiyetini (IP) korumak için tasarlanmış bir Donanım Güvenliği ve Gömülü Yapay Zeka projesidir. Model ağırlıklarının harici flash/RAM'de şifrelenmiş olarak saklanmasını ve yalnızca çalışma zamanında, silikon sınırları içinde Ring Oscillator Fiziksel Klonlanamaz Fonksiyonu (RO-PUF) tarafından üretilen donanıma özgü bir anahtar kullanılarak deşifre edilmesini sağlar.

### 🖥️ CyberPUF Web Dashboard

Sistemin uçtan uca kontrolü ve görselleştirilmesi için geliştirilmiş modern web arayüzü:
- **Dinamik Görev Konsolları:** Her işlem (Eğitim, Donanım Sentezi, Simülasyon, Ağ Gözetimi) için dinamik olarak açılan ve kapatılabilen yan yana WebSocket konsolları.
- **Canlı Sistem Logları:** `asyncio.gather` destekli, asenkron WebSocket yayını.
- **Güvenlik Mimarisi:** Token Bucket algoritması ile ağ taşması (DDoS/Flooding) koruması, Bearer tabanlı kimlik doğrulama, Zamanlama Saldırısı koruması ve katı CORS/CSRF denetimleri.
- **Gizlilik:** Loglarda kullanıcının bilgisayarındaki asıl dosya yollarının maskelenmesi (Path Masking).
- **Ağırlık Görselleştirici (Weight Viz):** Şifrelenmiş anlamsız ağırlıkların, şifrelenmeden önceki (çözülmüş) haliyle arasındaki farkın piksel piksel ekranda gösterilmesi.

### Teknik Mimari

Proje, Yazılım Yapay Zekası, Donanım Kriptografisi ve Gömülü Sistemleri (Bare-Metal) birbirine bağlayan geniş bir yapıdan ve bu yapıyı orkestre eden Web Dashboard'dan oluşur:

#### Faz 1: Yapay Zeka Eğitimi ve Şifreleme (Python)
- **Model Eğitimi:** CIFAR-10 veri seti üzerinde TensorFlow/Keras kullanılarak Evrişimli Sinir Ağı (CNN) eğitilir.
- **CPUF Format Dönüşümü:** Ağırlıklar ve bias'lar `.h5` dosyasından çıkarılır ve dinamik meta veriler ile ham `float32` tensörlerini içeren özel bir ikili formata (`.cpuf`) dönüştürülür.
- **AES-256-CBC & SHA-256 Şifreleme:** `pycryptodome` kütüphanesi kullanılarak `.cpuf` dosyası PKCS7 dolgusu ile AES-256-CBC modunda şifrelenir. Güvenlik bütünlüğü (Tamper Detection) için ağırlıkların donanım öncesi `SHA-256` özeti hesaplanarak başlığa (header) dinamik olarak eklenir.

#### Faz 2: Donanım Güvenliği ve Kriptografi (VHDL)
- **Ring Oscillator PUF (RO-PUF):** 256 çift (toplam 512) Halka Osilatör ile inşa edilen özel bir fiziksel klonlanamaz fonksiyon.
- **AES-256 Kripto Motoru:** FIPS-197 uyumlu, şifreleme ve şifre çözme işlemlerini gerçekleştiren tam donanımlı AES motoru.
- **Yan Kanal Saldırı Koruması (SCA Countermeasures):** Güç Analizi (DPA/CPA) ve Elektromanyetik (EMA) saldırılarına karşı LFSR tabanlı rastgele gürültü/gecikme enjeksiyonu.

#### Faz 3: Gömülü Yapay Zeka Çıkarımı (C/C++ Bare-Metal)
- **Donanım Soyutlama Katmanı (HAL):** Memory-mapped I/O üzerinden VHDL modüllerini kontrol eden, PUF üretimini tetikleyen ve AES motoruna 16-byte'lık şifreli bloklar besleyen özel C sürücüleri (`cyberpuf_dsk.c`).
- **HMAC-SHA256 Bütünlük Doğrulaması:** Bare-metal uyumlu C tabanlı bütünlük kontrol kütüphanesi.
- **Fuzzy Extractor:** PUF anahtarındaki sıcaklık/voltaj kaynaklı bit hatalarını onaran Code-Offset tabanlı ECC hata ayıklama modülü.
- **Bare-Metal CNN Motoru:** Hiçbir harici kütüphane kullanılmadan C dilinde sıfırdan yazılmış yapay zeka çıkarım motoru.

#### Faz 5, 6 ve 7: Edge-AI Uç Nokta ve Ağ Güvenliği
- **Faz 5 - Uç Cihaz Dağıtımı (OTA):** Şifrelenmiş modellerin IoT cihazlarına (Edge Node) TLS 1.3 ve MQTT tabanlı kanal simülasyonu üzerinden Challenge-Response kimlik doğrulaması ile güvenli aktarımı.
- **Faz 6 - Ağ Trafiği Gözetimi (Network Monitor):** Dağıtım anında Derin Paket İnceleme (DPI) yapılarak MiTM (Ortadaki Adam) ve ARP Spoofing saldırı simülasyonlarını yakalayan, şifreli AES verisinin çalınmasını imkansız kılan modül.
- **Faz 7 - TEE Attestation (Donanım Onay Raporu):** SGX/TrustZone benzeri Güvenli Yürütme Ortamlarında modelin çalıştırılmadan önce ölçümlerinin (PCR) alınıp donanım anahtarı (AK) ile imzalandığı resmi sertifika doğrulama aşaması.

### 🛡️ Kapsamlı Güvenlik ve Entegrasyon Testleri (Pytest)
CyberPUF sisteminin hem kriptografik sağlamlığını hem de modüller arası uyumunu garanti altına almak için tasarlanmış otomatik test senaryoları (%100 Başarı Oranı):
- `test_manipulasyon_dayanikliligi.py`: Sisteme yönelik aktif saldırıları simüle eder. Kurcalanan veya kırpılan verilerin reddedildiğini doğrular.
- `test_puf_gurultu_simulasyonu.py`: PUF anahtarındaki 1 bitlik çevresel gürültünün bile tüm şifre çözme işlemini iptal ettiğini matematiksel olarak test eder.
- `test_uctan_uca_akis.py`: Şifrelenen ve deşifre edilen numpy dizilerinin bit-by-bit aynı olup olmadığını uçtan uca (E2E) test eder.
- `test_api.py` & `test_c_logic.py`: FastAPI Web Dashboard arka ucunun (Backend) güvenlik protokollerini asenkron (async/await) yapıları dahi test eden entegrasyon testleri.

---

<a name="english"></a>
## English

**Developer:** Arda Mecik (Developed at AltaySec)
**Version:** v4.0.0-Gold (Edge-AI Full Integration & Cyber Security Modules)
**License:** MIT

CyberPUF is an advanced Hardware Security and Embedded AI project designed to protect the intellectual property (IP) of neural network weights deployed on edge devices. It ensures that the model weights are encrypted while stored in external flash/RAM and are only decrypted at runtime within the silicon boundaries, using an intrinsic hardware key generated by a Ring Oscillator Physical Unclonable Function (RO-PUF).

### 🖥️ CyberPUF Web Dashboard

A modern, asynchronous, cyberpunk-themed web interface designed for end-to-end control and visualization of the system:
- **Dynamic Task Consoles:** Side-by-side, dismissible WebSocket consoles dynamically spawned for each pipeline stage.
- **Security Architecture:** Token Bucket rate limiting against DDoS/Flooding attacks, Static Bearer token-based authentication on API routes, Timing Attack protection, and strict Content Security Policies.
- **Weight Visualizer:** Real-time visual comparison engine displaying neural network weights before encryption and after encryption.

### Technical Architecture

#### Phase 1: Software AI Training & Encryption (Python)
- **Model Training:** CNN trained on CIFAR-10.
- **AES-256-CBC & SHA-256 Encryption:** Encrypts raw AI weights using AES-256 with robust cryptographic tamper-resistance via HMAC signatures.

#### Phase 2: Hardware Security & Cryptography (VHDL)
- **Ring Oscillator PUF:** Custom physical unclonable function built with 512 Ring Oscillators.
- **AES-256 Crypto Core & SCA Countermeasures:** FIPS-197 AES engine protected against Side Channel Attacks using random LFSR noise delays.

#### Phase 3: Embedded AI Inference (C/C++ Bare-Metal)
- **Hardware Abstraction Layer (HAL):** Custom C drivers controlling the VHDL IP.
- **Fuzzy Extractor & CNN Engine:** C-based module fixing temperature-induced PUF bit errors and a from-scratch Bare-Metal CNN Inference Engine.

#### Phase 5, 6 & 7: Edge-AI Endpoint and Network Security
- **Phase 5 - OTA Edge Deployment:** Simulated secure transmission of AES encrypted AI models to edge nodes using TLS 1.3 and Challenge-Response handshake.
- **Phase 6 - Network Traffic Monitor:** Deep Packet Inspection (DPI) simulation identifying MiTM and ARP Spoofing attacks during model transmission.
- **Phase 7 - TEE Attestation:** Generation of cryptographic Hardware Quotes simulating SGX/TrustZone to verify execution environments securely using PUF-derived Attestation Keys.

### 🛡️ Comprehensive Security and Integration Testing (Pytest)
Automated Pytest suites achieving a 100% pass rate:
- `test_manipulasyon_dayanikliligi.py`, `test_puf_gurultu_simulasyonu.py`, `test_uctan_uca_akis.py`, `test_api.py`.
Tests ranging from fuzzy extractor bounds, active ciphertext tampering, to asynchronous WebSocket security route authorizations.

### Getting Started
**1. Downloading the Project**
```bash
git clone https://github.com/mecik-arda/CyberPUF.git
cd CyberPUF
```

**2. Running the Web Dashboard & Core System**
```bash
pip install -r requirements.txt
cp .env.example .env
# Edit .env and securely configure CYBERPUF_AES_KEY and WEBSOCKET_TOKEN

# Start the application!
python start_app.py
```
