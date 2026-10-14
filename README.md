# UAV YKİ — İHA Yer Kontrol İstasyonu ve Çoklu İHA Telemetri Simülatörü

Birden fazla insansız hava aracının (İHA) uçuşunu simüle eden, telemetrisini **gerçek zamanlı** harita üzerinde gösteren ve **PostgreSQL**'e kaydeden bir yer kontrol istasyonu (YKİ) uygulaması. Arka uç tamamen **modern C++ (C++20)** ile yazılmıştır; arayüz React + Leaflet'tir.

![Kontrol paneli](docs/img/dashboard.png)

> **Kapsam notu:** Proje yalnızca gözetleme/keşif sınıfı bir İHA'nın görev takibi, güvenlik (failsafe) davranışları ve veri kaydı üzerine kuruludur. Silah, hedefleme veya benzeri bir işlev içermez. Gerçek bir standarda (STANAG 4586, MAVLink vb.) uyumluluk **iddia edilmez**; alan modeli bu sistemlerden esinlenmiştir.

## Neler yapar?

- **Çoklu İHA filosu:** Her İHA'nın çağrı adı, tipi (`MINI_RECON`, `ENDURANCE`), performans modeli ve görevi vardır. REST ile çalışma anında yeni İHA eklenebilir.
- **Uçuş durum makinesi:** `Yerde → Kalkış → Tırmanış → Seyir ⇄ Bekleme (loiter) → Eve dönüş (RTL) → İniş → İndi` ve batarya biterse `Düştü`.
- **Uçuş dinamiği:** Sınırlı dönüş hızı, ivmeli hız değişimi, tırmanış/alçalma oranı, **rüzgar vektörü ve ani esinti**; rüzgar yer hızını gerçekten etkiler.
- **Enerji modeli:** Taban güç + sürtünme (`v³`) + tırmanış gücü. Eve dönüş için gereken enerji sürekli tahmin edilir.
- **Güvenlik davranışları (failsafe):**
  - Veri bağı kaybı belirli süreyi aşarsa otomatik **RTL**.
  - **Coğrafi sınır (geofence):** Yasak bölgeye (no-fly) girme veya izinli bölgeden çıkma tespiti, otomatik RTL.
  - Kritik batarya veya "eve dönmeye yetecek enerji kalmadı" durumunda otomatik RTL.
- **Uyarı motoru:** Kenar tetiklemeli (aynı durum için tekrar tekrar alarm üretmez); önem düzeyi `WARNING` / `CRITICAL`.
- **Canlı arayüz:** Yönlü İHA simgeleri, canlı iz, yasak bölge poligonları, batarya/bağ göstergeleri, uyarı paneli, rüzgar kontrolü, bağ kesme ve manuel RTL düğmeleri.
- **Kayıt ve geri izleme:** Her uçuş (sorti) PostgreSQL'e yazılır; geçmiş bir sortinin rotası haritada yeniden çizilebilir.

## Mimari

```mermaid
flowchart LR
    UI["React + Leaflet<br/>(tarayıcı)"]
    subgraph SRV["C++ sunucu (uav_gcs_server)"]
        NET["net/<br/>Boost.Beast<br/>HTTP + WebSocket"]
        API["api/<br/>REST yönlendirme + JSON"]
        CORE["core/<br/>Filo, İHA modeli,<br/>geofence, uyarılar"]
        STORE["storage/<br/>libpqxx, göçler,<br/>toplu yazma"]
        NET --> API --> CORE
        API --> STORE
    end
    DB[("PostgreSQL")]
    UI -- "REST /api/*" --> NET
    NET -- "WebSocket /ws (10 Hz)" --> UI
    STORE --> DB
```

| Katman | Sorumluluk | Bağımlılık |
|---|---|---|
| `src/core` | Saf simülasyon: G/Ç yok, tamamen birim test edilebilir | Yalnızca standart kütüphane |
| `src/storage` | Telemetri/uyarı kaydı, SQL göçleri, sorti sorguları | libpqxx |
| `src/api` | REST uç noktaları, doğrulama, JSON dönüşümü | nlohmann/json |
| `src/net` | HTTP + WebSocket sunucusu, CORS, yayın | Boost.Beast |
| `src/app` | Yapılandırma, simülasyon döngüsü, kapanış | — |

**Tasarım kararları**

- **Çekirdek bağımsızdır.** `core` ağ ve veritabanını bilmez; bu yüzden uçuş modeli, geofence ve uyarılar milisaniyeler içinde test edilir.
- **Simülasyon zamanı ölçeklenebilir** (`UAVGCS_TIME_SCALE`). Gerçek zamanda 20 dakikalık bir görev, varsayılan x10 ile 2 dakikada izlenir.
- **Veritabanı yazması simülasyonu bloklamaz.** Örnekler bir kuyruğa alınır, arka plan iş parçacığı `COPY` ile toplu yazar. Telemetri saniyede bir örneklenir (simülasyon 10 Hz çalışır). Bağlantı koparsa bir kez yeniden bağlanıp yeniden dener.
- **Veritabanı opsiyoneldir.** Bağlanılamazsa sunucu uyarı verip kayıt yapmadan çalışır; geçmiş uç noktaları `503` döner.
- **Girdi doğrulaması:** Tüm REST girdileri aralık ve tip denetiminden geçer, gövde boyutu 1 MiB ile sınırlıdır, eşzamanlı bağlantı sayısı sınırlıdır.

### Uçuş durum makinesi

```mermaid
stateDiagram-v2
    [*] --> Preflight
    Preflight --> Takeoff: görevi başlat
    Takeoff --> Climb: 30 m
    Climb --> Cruise: görev irtifası
    Cruise --> Loiter: bekleme süresi olan nokta
    Loiter --> Cruise: süre doldu
    Cruise --> RTL: son nokta
    Takeoff --> RTL: komut / failsafe
    Climb --> RTL: komut / failsafe
    Cruise --> RTL: komut / failsafe
    Loiter --> RTL: komut / failsafe
    RTL --> Landing: üsse yaklaştı
    Landing --> Landed
    Landed --> Takeoff: yeni sorti (batarya yenilenir)
    Takeoff --> Crashed: batarya bitti
    Climb --> Crashed: batarya bitti
    Cruise --> Crashed: batarya bitti
    Loiter --> Crashed: batarya bitti
    RTL --> Crashed: batarya bitti
    Landing --> Crashed: batarya bitti
```

## REST API

| Yöntem | Uç nokta | Açıklama |
|---|---|---|
| `GET` | `/api/health` | Durum, İHA sayısı, veritabanı bağlı mı |
| `GET` / `POST` | `/api/uavs` | İHA'ları listele / yeni İHA ekle |
| `GET` | `/api/uavs/{id}` | Tek İHA durumu |
| `POST` | `/api/uavs/{id}/start` | Görevi başlat |
| `POST` | `/api/uavs/{id}/rtl` | Eve dönüş komutu |
| `POST` | `/api/uavs/{id}/link` | Veri bağını aç/kapat: `{"up": false}` |
| `GET` | `/api/zones` | Coğrafi sınır bölgeleri |
| `GET` | `/api/alerts?limit=` | Son uyarılar |
| `GET` / `POST` | `/api/wind` | Rüzgar: `{"speed_mps":8,"from_deg":270,"gust_mps":2}` |
| `GET` | `/api/sorties` | Kayıtlı uçuşlar (veritabanı gerekir) |
| `GET` | `/api/sorties/{id}/track?max_points=` | Bir uçuşun rotası |

Yeni İHA örneği:

```bash
curl -X POST http://localhost:8080/api/uavs -H "Content-Type: application/json" -d '{
  "callsign": "YENI-4", "type": "MINI_RECON",
  "home": {"lat": 39.80, "lon": 32.80},
  "waypoints": [{"lat": 39.85, "lon": 32.85, "alt_m": 300, "loiter_s": 20}],
  "start": true
}'
```

**WebSocket** `ws://localhost:8080/ws` yalnızca sunucudan istemciye yayındır:

- `{"type":"telemetry","sim_time_s":…,"time_scale":…,"uavs":[…]}` — saniyede 10 kez
- `{"type":"alert","alert":{…}}` — yeni uyarı oluştuğunda

## Kurulum

### Gereksinimler

C++20 derleyicisi (GCC 15 / MinGW-w64 ve Ubuntu 24.04 üzerindeki GCC 13 hedeflenmiştir; CI bunu doğrular), CMake 3.20+, Ninja, Boost (başlık kütüphaneleri), libpqxx, nlohmann-json, GoogleTest, PostgreSQL ve arayüz için Node.js 18+.

**Windows (MSYS2 UCRT64):**

```bash
pacman -S --needed mingw-w64-ucrt-x86_64-{gcc,cmake,ninja,boost,libpqxx,nlohmann-json,gtest}
```

**Ubuntu / Debian:**

```bash
sudo apt install cmake ninja-build g++ libboost-dev libpqxx-dev nlohmann-json3-dev libgtest-dev
```

### Derleme ve test

```bash
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
```

### Veritabanı

```sql
CREATE DATABASE uavgcs;
```

Tablolar, sunucu ilk açıldığında `db/migrations/*.sql` dosyalarından otomatik oluşturulur. Bağlantı bilgisi ortam değişkeninden okunur; **şifre koda veya depoya yazılmaz**:

```bash
export UAVGCS_DB_URL="postgresql://kullanici:sifre@localhost:5432/uavgcs"
# Windows PowerShell: $env:UAVGCS_DB_URL = "postgresql://..."
```

### Çalıştırma

```bash
./build/uav_gcs_server                 # Windows: .\build\uav_gcs_server.exe
cd frontend && npm install && npm run dev
```

Arayüz `http://localhost:5173` adresinde açılır. Soldaki kartlardan **Göreve başla**'ya basın.

### Yapılandırma (ortam değişkenleri)

| Değişken | Varsayılan | Açıklama |
|---|---|---|
| `UAVGCS_BIND` | `127.0.0.1` | Dinlenecek adres |
| `UAVGCS_PORT` | `8080` | Port |
| `UAVGCS_TIME_SCALE` | `10` | Simülasyon hız çarpanı (0–100) |
| `UAVGCS_CORS_ORIGIN` | `http://localhost:5173` | İzin verilen arayüz kökeni |
| `UAVGCS_DB_URL` | `postgresql://postgres@localhost:5432/uavgcs` | PostgreSQL bağlantısı |
| `UAVGCS_DB` | `on` | `off` ise veritabanı kullanılmaz |
| `UAVGCS_MIGRATIONS_DIR` | `db/migrations` | SQL göçleri dizini |

Arayüz için sunucu adresi gerekirse `frontend/.env` içinde `VITE_API_URL=http://host:port` verilir.

## Testler

`55` birim testi: coğrafi hesaplar (Haversine, kerteriz, çokgen içinde nokta), İHA durum makinesi ve enerji modeli, rüzgarın yer hızına etkisi, deterministik tekrar (aynı tohum = aynı rota), link-loss failsafe, geofence ve uyarı motoru, filo yönetimi (eşzamanlı okuma/yazma dahil) ve REST doğrulaması. Veritabanı katmanı geliştirme sırasında gerçek bir PostgreSQL'e karşı elle doğrulanmıştır; otomatik entegrasyon testi yol haritasındadır.

## Bilinen sınırlamalar

- Uçuş modeli bir nokta-kütle yaklaşımıdır: rüzgar için "crab" açısı düzeltmesi, atmosfer yoğunluğu ve gerçek aerodinamik yoktur.
- WebSocket istemcileri yalnızca dinler; komutlar REST ile gelir. Sunucu istemci kapanışını bir sonraki yazma hatasında fark eder.
- Kimlik doğrulama yoktur; sunucu varsayılan olarak yalnızca `127.0.0.1` üzerinde dinler. Ağa açmadan önce bir kimlik doğrulama katmanı eklenmelidir.
- Coğrafi sınır kontrolü sunucuda (C++) yapılır; PostGIS henüz kullanılmamaktadır.

## Yol haritası

- [ ] PostGIS ile geofence sorguları ve uçuş izlerinin mekânsal analizi
- [ ] JWT ile kimlik doğrulama ve rol bazlı yetki (operatör / gözlemci)
- [ ] Harita üzerinde tıklayarak görev (waypoint) planlama
- [ ] Veritabanı katmanı için otomatik entegrasyon testleri
- [ ] Docker Compose ile tek komutla kurulum
- [ ] Daha ayrıntılı uçuş dinamiği (rüzgar düzeltmeli rota takibi)
- [ ] İngilizce dokümantasyon

## Geliştirici

**Melih Ali Çağman** — [GitHub: MelihAliCagman](https://github.com/MelihAliCagman) · cagmanmelihali@gmail.com
