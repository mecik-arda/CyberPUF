# CypherPUF: PUF Tabanlı Uç Yapay Zeka (Edge-AI) Model Ağırlığı Şifrelemesi

[Türkçe](#turkce) | [English](#english)

---

<a name="turkce"></a>
## Türkçe

**Geliştirici:** Arda Meçik (AltaySec bünyesinde geliştirilmiştir)
**Sürüm:** 1.0.0  
**Lisans:** MIT

CypherPUF, uç cihazlarda (FPGA ve SoC mimarileri gibi) konuşlandırılan yapay sinir ağı ağırlıklarının fikri mülkiyetini (IP) korumak için tasarlanmış gelişmiş bir Donanım Güvenliği ve Gömülü Yapay Zeka projesidir. Model ağırlıklarının harici flash/RAM'de şifrelenmiş olarak saklanmasını ve yalnızca çalışma zamanında, silikon sınırları içinde Ring Oscillator Fiziksel Klonlanamaz Fonksiyonu (RO-PUF) tarafından üretilen donanıma özgü bir anahtar kullanılarak deşifre edilmesini sağlar.

### Teknik Mimari

Proje, Yazılım Yapay Zekası, Donanım Kriptografisi ve Gömülü Sistemleri (Bare-Metal) birbirine bağlayan üç aşamalı bir yapıdan oluşur:

#### Faz 1: Yapay Zeka Eğitimi ve Şifreleme (Python)
- **Model Eğitimi:** CIFAR-10 veri seti üzerinde TensorFlow/Keras kullanılarak Evrişimli Sinir Ağı (CNN) eğitilir.
- **CPUF Format Dönüşümü:** Ağırlıklar ve bias'lar `.h5` dosyasından çıkarılır ve dinamik meta veriler ile ham `float32` tensörlerini içeren özel bir ikili formata (`.cpuf`) dönüştürülür.
- **AES-256 Şifreleme:** `pycryptodome` kütüphanesi kullanılarak `.cpuf` dosyası AES-256 (GCM/CBC) ile şifrelenir. C header formatında ve `.cpfe` ikili formatında dışa aktarılır.
- **Doğrulama:** Şifreleme sürecinin bütünlüğünü ve kurcalama korumasını test eden uçtan uca Python doğrulama betiği.

#### Faz 2: Donanım Güvenliği ve Kriptografi (VHDL)
- **Ring Oscillator PUF (RO-PUF):** 16 çift (toplam 32) Ring Oscillator ile inşa edilen özel bir fiziksel klonlanamaz fonksiyon. Üretim varyasyonlarını kullanarak eşsiz bir 256-bit anahtar üretir.
- **Anahtar Üretimi:** Ortam gürültüsünü ortadan kaldırmak için bit başına 16 kez PUF salınımlarını örnekleyen çoğunluk oylaması (majority-voting) mekanizması.
- **AES-256 Çözümleme Motoru:** FIPS-197 uyumlu, tam donanımlı AES çözümleme boru hattı (pipeline).
- **Yan Kanal Saldırı Koruması (SCA Countermeasures):** AES şifre çözme modülüne entegre edilmiş LFSR tabanlı yapay güç gürültüsü üreteci ve rastgele gecikme (Random Stall) enjeksiyonu. Güç Analizi (DPA/CPA) ve Elektromanyetik (EMA) saldırılarına karşı donanımsal koruma sağlar.
- **AXI4-Lite Wrapper:** İşlemci Sistemi (ARM Cortex-A) ile haberleşmeyi sağlamak için tüm kriptografi çekirdeğinin 20 farklı bellek eşlemeli (memory-mapped) yazmaç ile (0x00 - 0x4C) AXI4-Lite arayüzüne sarılması.

#### Faz 3: Gömülü Yapay Zeka Çıkarımı (C/C++ Bare-Metal)
- **Donanım Soyutlama Katmanı (HAL):** Memory-mapped I/O üzerinden VHDL modüllerini kontrol eden, PUF üretimini tetikleyen ve AES motoruna 16-byte'lık şifreli bloklar besleyen özel C sürücüleri (`cypherpuf_dsk.c`).
- **Yardımcı Veri Üretici (Fuzzy Extractor):** PUF anahtarındaki sıcaklık/voltaj kaynaklı bit hatalarını (gürültüyü) silikon dışında Code-Offset ve Hamming(7,4) Hata Düzeltme Kodları (ECC) ile %100 oranında onaran akıllı hata ayıklama modülü (`yardimci_veri_uretici.c`).
- **Dinamik İkili Ayrıştırıcı (Parser):** Çözülen `float32` ağırlık dizilerini bellekte eşlemek için özel CPUF başlıklarını dinamik olarak tarayan ayrıştırıcı.
- **Bare-Metal CNN Motoru:** Hiçbir harici kütüphane kullanılmadan C dilinde sıfırdan yazılmış yapay zeka çıkarım motoru. RAM parçalanmasını en aza indirmek için ping-pong bellek tekniği kullanarak Conv2D, MaxPool, Dense ve BatchNorm+ReLU katmanlarını destekler.

### Test Sonuçları (Donanım AES-256 Crypto Core)
GHDL simülasyonu ile donanımın şifre çözme performansı ve doğruluğu başarıyla test edilmiştir. Aşağıda VHDL Testbench'inin doğrudan çıktısı bulunmaktadır:

```text
========================================
TEST 1: NIST AES-256 Key Expansion
========================================
Key expansion tamamlandi.

========================================
TEST 2: AES-256 Encryption
========================================
Plaintext  : 00112233445566778899aabbccddeeff
Ciphertext : 8EA2B7CA516745BFEAFC49904B496089
Expected   : 8ea2b7ca516745bfeafc49904b496089
TEST 2 BASARILI: Encryption dogru!

========================================
TEST 3: AES-256 Decryption (Sifre Cozme)
========================================
Ciphertext  : 8EA2B7CA516745BFEAFC49904B496089
Decrypted   : 00112233445566778899AABBCCDDEEFF
Expected PT : 00112233445566778899aabbccddeeff
TEST 3 BASARILI: Decryption dogru!

========================================
TEST 4: Encrypt-then-Decrypt Round Trip
========================================
Orijinal PT : DEADBEEFCAFEBABE1234567890ABCDEF
Encrypted   : F15CFE4C4CBC4D547A96C9CCC8BC3F38
Decrypted   : DEADBEEFCAFEBABE1234567890ABCDEF
TEST 4 BASARILI: Round-trip dogru!

========================================
TEST 5: Farkli Anahtar ile Sifreleme
========================================
Key         : 603DEB1015CA71BE2B73AEF0857D77811F352C073B6108D72D9810A30914DFF4
Plaintext   : 6BC1BEE22E409F96E93D7E117393172A
Encrypted   : F3EED1BDB5D2A03C064B5A7E3DB181F8
Decrypted   : 6BC1BEE22E409F96E93D7E117393172A
TEST 5 BASARILI: Farkli anahtar round-trip dogru!

========================================
GENEL SONUC
========================================
TUM TESTLER BASARILI!
Simulasyon tamamlandi.
```

### Kullanılan Diller ve Teknolojiler
* **Yazılım ve Yapay Zeka:** Python 3.10+, TensorFlow / Keras, NumPy, PyCryptodome
* **Donanım Tasarımı (HDL):** VHDL-2008, GHDL / TerosHDL, Xilinx Vivado (Sentez, XDC Kısıtlamaları)
* **Gömülü Sistemler:** C / C++ (Bare-metal), Xilinx Vitis / GCC, AXI4-Lite Protokolü

### Başlangıç Kılavuzu
**1. Projeyi İndirme**
```bash
git clone https://github.com/mecik-arda/CyberPUF.git
cd CyberPUF
```

**2. Faz 1'i Çalıştırma (Yapay Zeka ve Şifreleme)**
```bash
pip install -r requirements.txt
python run_phase1.py
```

**3. Faz 2'yi Derleme (VHDL Donanımı)**
```powershell
cd donanim
.\compile.ps1
```

**4. Faz 3'ü Çalıştırma (Gömülü C Simülasyonu)**
```bash
cd gomulu
gcc src/main.c src/cypherpuf_dsk.c src/yapay_zeka_cikarimi.c src/yardimci_veri_uretici.c -lm -o cypherpuf_sim.exe
./cypherpuf_sim.exe
```

---

<a name="english"></a>
## English

**Developer:** Arda Mecik (Developed at AltaySec)
**Version:** 1.0.0  
**License:** MIT

CypherPUF is an advanced Hardware Security and Embedded AI project designed to protect the intellectual property (IP) of neural network weights deployed on edge devices (like FPGAs and SoC architectures). It ensures that the model weights are encrypted while stored in external flash/RAM and are only decrypted at runtime within the silicon boundaries, using an intrinsic hardware key generated by a Ring Oscillator Physical Unclonable Function (RO-PUF).

### Technical Architecture

The project is structured into three continuous phases that bridge Software AI, Hardware Cryptography, and Bare-Metal Embedded Systems:

#### Phase 1: Software AI Training & Encryption (Python)
- **Model Training:** A Convolutional Neural Network (CNN) is trained on the CIFAR-10 dataset using TensorFlow/Keras. The architecture features Conv2D blocks with Batch Normalization and ReLU.
- **CPUF Format Serialization:** Weights and biases are extracted from the `.h5` file and serialized into a custom binary format (`.cpuf`), which includes dynamic metadata, magic numbers, and raw `float32` tensors.
- **AES-256 Encryption:** The `pycryptodome` library encrypts the `.cpuf` binary using AES-256 in GCM/CBC mode. The ciphertext is exported both as a `.cpfe` binary and a C-header file for embedded integration.
- **Verification:** An end-to-end Python test suite ensures the integrity, decryption accuracy, and tamper-resistance of the encryption pipeline.

#### Phase 2: Hardware Security & Cryptography (VHDL)
- **Ring Oscillator PUF (RO-PUF):** A custom physical unclonable function built with 16 pairs of Ring Oscillators (32 total). It leverages silicon manufacturing variations to generate a unique, unpredictable 256-bit key. Includes `DONT_TOUCH` and combinatorial loop synthesis attributes to prevent logic optimization.
- **Key Generation:** A robust majority-voting mechanism samples the PUF oscillations 16 times per bit to eliminate environmental noise and temperature variations.
- **AES-256 Decryption Engine:** A full FIPS-197 compliant AES decryption pipeline with Inverse S-Boxes, Inverse ShiftRows, Inverse MixColumns (using GF(2^8) multipliers), and Key Expansion modules.
- **AXI4-Lite Wrapper:** The entire crypto-core is wrapped in an AXI4-Lite slave interface with 20 distinct memory-mapped registers (0x00 to 0x4C) for seamless communication with the Processing System (ARM Cortex-A).

#### Phase 3: Embedded AI Inference (C/C++ Bare-Metal)
- **Hardware Abstraction Layer (HAL):** Custom C drivers (`cypherpuf_dsk.c`) to control the VHDL IP via memory-mapped I/O, trigger PUF generation, and feed encrypted 16-byte blocks to the AES engine.
- **Fuzzy Extractor (Helper Data Generator):** A smart error correction module (`yardimci_veri_uretici.c`) that perfectly corrects temperature/voltage-induced bit errors (noise) in the PUF key using Code-Offset and Hamming(7,4) Error Correction Codes (ECC).
- **Dynamic Binary Parser:** An embedded parser that scans the custom CPUF headers dynamically in memory to map the decrypted `float32` weight arrays.
- **Bare-Metal CNN Engine:** A C-based AI inference engine written entirely from scratch without external libraries. It supports Conv2D, MaxPool, Dense, and Fused BatchNorm+ReLU layers, utilizing a ping-pong buffer technique to minimize RAM fragmentation.

### Test Results (Hardware AES-256 Crypto Core)
The decryption accuracy and performance of the hardware have been successfully tested via GHDL simulation. Below is the direct output from the VHDL Testbench:

```text
========================================
TEST 1: NIST AES-256 Key Expansion
========================================
Key expansion tamamlandi.

========================================
TEST 2: AES-256 Encryption
========================================
Plaintext  : 00112233445566778899aabbccddeeff
Ciphertext : 8EA2B7CA516745BFEAFC49904B496089
Expected   : 8ea2b7ca516745bfeafc49904b496089
TEST 2 BASARILI: Encryption dogru!

========================================
TEST 3: AES-256 Decryption
========================================
Ciphertext  : 8EA2B7CA516745BFEAFC49904B496089
Decrypted   : 00112233445566778899AABBCCDDEEFF
Expected PT : 00112233445566778899aabbccddeeff
TEST 3 BASARILI: Decryption dogru!

========================================
TEST 4: Encrypt-then-Decrypt Round Trip
========================================
Orijinal PT : DEADBEEFCAFEBABE1234567890ABCDEF
Encrypted   : F15CFE4C4CBC4D547A96C9CCC8BC3F38
Decrypted   : DEADBEEFCAFEBABE1234567890ABCDEF
TEST 4 BASARILI: Round-trip dogru!

========================================
TEST 5: Farkli Anahtar ile Sifreleme
========================================
Key         : 603DEB1015CA71BE2B73AEF0857D77811F352C073B6108D72D9810A30914DFF4
Plaintext   : 6BC1BEE22E409F96E93D7E117393172A
Encrypted   : F3EED1BDB5D2A03C064B5A7E3DB181F8
Decrypted   : 6BC1BEE22E409F96E93D7E117393172A
TEST 5 BASARILI: Farkli anahtar round-trip dogru!

========================================
GENEL SONUC
========================================
TUM TESTLER BASARILI!
Simulasyon tamamlandi.
```

### Languages & Technologies Used
* **Software & AI:** Python 3.10+, TensorFlow / Keras, NumPy, PyCryptodome
* **Hardware Design (HDL):** VHDL-2008, GHDL / TerosHDL, Xilinx Vivado (Synthesis, Implementation, XDC)
* **Embedded Systems:** C / C++ (Bare-metal), Xilinx Vitis / GCC, AXI4-Lite Protocol

### Directory Structure
```text
CypherPUF/
├── ai_sifreleme/                  # Python AI & Crypto Tools
│   ├── train_model.py             # Trains the CNN model
│   ├── export_weights.py          # Creates the .cpuf binary
│   ├── encrypt_weights.py         # Encrypts CPUF to .cpfe / .h
│   └── verify_encryption.py       # Validates the encryption
├── donanim/                       # VHDL Hardware Design
│   ├── vhdl/                      # PUF, AES, and AXI source codes
│   ├── testbench/                 # VHDL Simulation Testbenches
│   └── constraints/               # Xilinx XDC constraints files
├── gomulu/                        # Bare-Metal C Application
│   ├── src/                       # HAL, AI Inference, and main program
├── run_phase1.py                  # Python Automation Orchestrator
├── requirements.txt               # Python Dependencies
├── .gitignore                     # Git ignore rules
└── LICENSE                        # MIT License
```

### Getting Started
**1. Downloading the Project**
```bash
git clone https://github.com/mecik-arda/CyberPUF.git
cd CyberPUF
```

**2. Running Phase 1 (AI Training & Encryption)**
```bash
pip install -r requirements.txt
python run_phase1.py
```

**3. Compiling Phase 2 (VHDL Hardware)**
```powershell
cd donanim
.\compile.ps1
```

**4. Running Phase 3 (Embedded C Simulation)**
```bash
cd gomulu
gcc src/main.c src/cypherpuf_dsk.c src/yapay_zeka_cikarimi.c src/yardimci_veri_uretici.c -lm -o cypherpuf_sim.exe
./cypherpuf_sim.exe
```
