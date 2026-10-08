# SkyWatcher Telemetry Simulator: Gerçek Zamanlı Uçuş Telemetri Paneli

Havalimanları arasında bir uçağın uçuşunu simüle eden ve **telemetri verilerini (konum, irtifa, hız, yakıt, hava durumu, uçuş durumu) gerçek zamanlı olarak** harita üzerinde gösteren bir yer kontrol istasyonu uygulamasıdır. Backend **Spring Boot** verileri üretip **WebSocket (STOMP)** ile yayınlar ve **PostgreSQL**'e kaydeder; **React + Leaflet** arayüzü canlı rotayı çizer ve geçmiş uçuşların seyir defterini sunar.

## Özellikler

- **Havalimanı seçimi:** Türkiye'den (Ankara Esenboğa, İstanbul Havalimanı, İzmir Adnan Menderes, Antalya) ve dünyadan (New York JFK, Londra Heathrow, Tokyo Haneda, Dubai) 8 havalimanı arasından kalkış ve varış seçimi
- **Uçuş planı hesabı:** Haversine formülüyle iki havalimanı arasındaki gerçek mesafe (km) ve tahmini yakıt ihtiyacı
- **Canlı telemetri:** Saniyede 10 güncelleme ile konum, irtifa, hız ve kalan yakıt; uçağın izlediği rota haritada canlı çizilir
- **Hava durumu etkisi:** Her uçuşta rastgele bir hava durumu seçilir ve uçuş performansını etkiler

  | Hava durumu | Hız | Yakıt tüketimi | İrtifa |
  |---|---|---|---|
  | Güneşli | 800 | Normal | Stabil |
  | Rüzgarlı | 900 | Düşük | Hafif sallantı |
  | Karlı | 700 | Yüksek | Stabil |
  | Fırtınalı | 600 | Çok yüksek | Şiddetli türbülans |

- **Uçuş durumları:** `FLYING`, `LANDED`, yakıt biterse `CRASHED`; düşük yakıt (< 300 L) ve düşüş uyarıları
- **Seyir defteri:** Tüm uçuşlar veritabanında saklanır; geçmiş bir uçuş seçilip rotası haritada yeniden çizilebilir

## Mimari

```
React + Leaflet (frontend, :5173)
   │  REST  /api/telemetry/**
   │  WebSocket (STOMP)  ws://localhost:8080/ws  →  /topic/telemetry
   ▼
Spring Boot (:8080)
 ├── SimulationService   Uçuş simülasyonu (@Scheduled, 100 ms), yakıt/hava modeli
 ├── AirportService      Havalimanı veritabanı
 ├── TelemetryController REST uç noktaları
 └── WebSocketConfig     STOMP yayın altyapısı
   ▼
PostgreSQL (telemetry_db)  ← her telemetri örneği kaydedilir
```

### REST API

| Yöntem | Uç nokta | Açıklama |
|---|---|---|
| `GET` | `/api/telemetry/airports` | Havalimanları ve koordinatları |
| `GET` | `/api/telemetry/calculate` | Mesafe (Haversine) ve tahmini yakıt |
| `POST` | `/api/telemetry/start-flight` | İki koordinat arasında uçuş başlatır |
| `GET` | `/api/telemetry/flights` | Kayıtlı uçuş kodları |
| `GET` | `/api/telemetry/history/{flightId}` | Seçilen uçuşun tüm telemetri kayıtları |
| `GET` | `/api/telemetry/all` | Tüm telemetri kayıtları |

WebSocket kanalı: `/topic/telemetry`

## Kullanılan Teknolojiler

- **Backend:** Java 17, Spring Boot 4 (Web MVC, WebSocket, Data JPA), Hibernate, PostgreSQL, Maven
- **Frontend:** React 19, Vite 7, React-Leaflet / Leaflet (OpenStreetMap), @stomp/stompjs
- **Kavramlar:** Gerçek zamanlı veri akışı, publish/subscribe (STOMP), zamanlanmış görevler, telemetri kaydı ve geriye dönük oynatma

## Kurulum ve Çalıştırma

### Gereksinimler
- JDK 17+
- Node.js 18+ ve npm
- PostgreSQL

### 1. Veritabanı

```sql
CREATE DATABASE telemetry_db;
```

Tablo, uygulama ilk çalıştığında otomatik oluşturulur. Veritabanı şifresi ortam değişkeninden okunur:

```bash
export DB_PASSWORD=postgres_sifreniz        # Windows PowerShell: $env:DB_PASSWORD="..."
```

### 2. Backend

```bash
git clone https://github.com/MelihAliCagman/telemetry-simulator.git
cd telemetry-simulator
./mvnw spring-boot:run                      # Windows: mvnw.cmd spring-boot:run
```

### 3. Frontend

```bash
cd frontend
npm install
npm run dev
```

Arayüz `http://localhost:5173` adresinde açılır.

### Kullanım

1. Soldaki panelden **Kalkış** ve **Varış** havalimanını seçin; mesafe ve planlanan yakıt görüntülenir.
2. **Uçuşu Başlat** düğmesine basın. Uçak haritada ilerler, rota mavi çizgiyle çizilir.
3. Sol panelin altındaki göstergelerden hava durumu, hız, irtifa ve yakıtı izleyin.
4. **Geçmiş Uçuşlarım** ile kayıtlı uçuşları açıp rotalarını turuncu çizgiyle yeniden görüntüleyin.

## Yol Haritası

- Gerçekçi uçuş dinamikleri (rota üzerinde kalkış, seyir, iniş fazları)
- Telemetri kayıtları için toplu yazma / örnekleme aralığı
- Birden fazla uçağın eş zamanlı takibi
- Gerçek hava durumu API'si entegrasyonu
- Docker Compose ile tek komutla kurulum
