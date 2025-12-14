import React, { useState, useEffect } from 'react'
import { MapContainer, TileLayer, Marker, Popup, Polyline, useMap } from 'react-leaflet'
import { Client } from '@stomp/stompjs'
import 'leaflet/dist/leaflet.css'
import './App.css'
import L from 'leaflet';
import icon from 'leaflet/dist/images/marker-icon.png';
import iconShadow from 'leaflet/dist/images/marker-shadow.png';

const DefaultIcon = L.icon({
    iconUrl: icon,
    shadowUrl: iconShadow,
    iconAnchor: [12, 41]
});
L.Marker.prototype.options.icon = DefaultIcon;

function PlaneController({ position }) {
    return <Marker position={position}><Popup>✈️ SKY-1 Uçuşta</Popup></Marker>;
}

function App() {
    const [position, setPosition] = useState([40.1281, 32.9951]);
    const [telemetry, setTelemetry] = useState({ speed: 0, altitude: 0 });
    const [historyPath, setHistoryPath] = useState([]);

    // Havaalanı Sistemi İçin State'ler
    const [airports, setAirports] = useState({});
    const [selectedStart, setSelectedStart] = useState("");
    const [selectedEnd, setSelectedEnd] = useState("");

    useEffect(() => {
        // 1. WebSocket Bağlantısı
        const client = new Client({
            brokerURL: 'ws://localhost:8080/ws',
            onConnect: () => {
                client.subscribe('/topic/telemetry', (message) => {
                    const data = JSON.parse(message.body);
                    setPosition([parseFloat(data.latitude), parseFloat(data.longitude)]);
                    setTelemetry({ speed: data.speed, altitude: data.altitude });
                });
            },
        });
        client.activate();

        // 2. Havalimanı Listesini Çek
        fetch("http://localhost:8080/api/telemetry/airports")
            .then(res => res.json())
            .then(data => setAirports(data));

        return () => client.deactivate();
    }, []);

    // Uçuşu Başlat Butonu
    const handleStartFlight = () => {
        if(!selectedStart || !selectedEnd) {
            alert("Lütfen Kalkış ve Varış Meydanı Seçin!");
            return;
        }

        const startCoords = airports[selectedStart];
        const endCoords = airports[selectedEnd];

        // Backend'e "Uçuşu Başlat" emri ver
        fetch(`http://localhost:8080/api/telemetry/start-flight?startLat=${startCoords[0]}&startLon=${startCoords[1]}&endLat=${endCoords[0]}&endLon=${endCoords[1]}`, {
            method: 'POST'
        }).then(() => {
            alert(`✈️ Rota Oluşturuldu: ${selectedStart} -> ${selectedEnd}`);
            // Geçmiş rotayı temizle ki yeni uçuş temiz görünsün
            setHistoryPath([]);
        });
    };

    // Geçmişi Göster Butonu
    const fetchHistory = async () => {
        const response = await fetch("http://localhost:8080/api/telemetry/all");
        const data = await response.json();
        setHistoryPath(data.map(p => [p.latitude, p.longitude]));
    };

    return (
        <div style={{ height: '100vh', width: '100vw', overflow: 'hidden', display: 'flex' }}>

            {/* --- SOL PANEL: UÇUŞ OPERASYON MERKEZİ --- */}
            <div style={{
                width: '300px',
                backgroundColor: '#1e272e',
                color: 'white',
                padding: '20px',
                boxShadow: '2px 0 10px rgba(0,0,0,0.5)',
                zIndex: 1000,
                display: 'flex',
                flexDirection: 'column',
                gap: '15px'
            }}>
                <h2 style={{color: '#00d4ff', borderBottom: '1px solid #555', paddingBottom: '10px'}}>🛫 FLIGHT OPS</h2>

                {/* Kalkış Seçimi */}
                <label>Kalkış Meydanı (Origin)</label>
                <select
                    style={{padding: '8px', borderRadius: '5px'}}
                    onChange={(e) => setSelectedStart(e.target.value)}
                >
                    <option value="">Seçiniz...</option>
                    {Object.keys(airports).map(name => (
                        <option key={name} value={name}>{name}</option>
                    ))}
                </select>

                {/* Varış Seçimi */}
                <label>Varış Meydanı (Dest)</label>
                <select
                    style={{padding: '8px', borderRadius: '5px'}}
                    onChange={(e) => setSelectedEnd(e.target.value)}
                >
                    <option value="">Seçiniz...</option>
                    {Object.keys(airports).map(name => (
                        <option key={name} value={name}>{name}</option>
                    ))}
                </select>

                <button
                    onClick={handleStartFlight}
                    style={{
                        marginTop: '10px', padding: '12px', backgroundColor: '#0be881',
                        border: 'none', borderRadius: '5px', color: '#1e272e', fontWeight: 'bold', cursor: 'pointer'
                    }}>
                    BAŞLAT ✈️
                </button>

                <hr style={{borderColor: '#555', width: '100%', margin: '20px 0'}} />

                {/* Telemetri Bilgileri */}
                <div style={{fontFamily: 'monospace', fontSize: '0.9rem'}}>
                    <p>HIZ: <span style={{color: '#00d4ff'}}>{telemetry.speed} km/s</span></p>
                    <p>İRTİFA: <span style={{color: '#00d4ff'}}>{telemetry.altitude} m</span></p>
                    <p>ENLEM: {position[0].toFixed(4)}</p>
                    <p>BOYLAM: {position[1].toFixed(4)}</p>
                </div>

                <button
                    onClick={fetchHistory}
                    style={{marginTop: 'auto', padding: '10px', backgroundColor: '#34ace0', border:'none', borderRadius:'5px', color:'white', cursor:'pointer'}}>
                    📂 Uçuş Kaydı İzle
                </button>
            </div>

            {/* --- SAĞ TARAF: HARİTA --- */}
            <div style={{ flex: 1 }}>
                <MapContainer center={[39.9334, 32.8597]} zoom={5} zoomControl={false} style={{ height: '100%', width: '100%' }}>
                    <TileLayer url="https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png" />
                    <PlaneController position={position} />
                    {historyPath.length > 0 && <Polyline positions={historyPath} color="red" weight={3} />}
                </MapContainer>
            </div>
        </div>
    )
}

export default App