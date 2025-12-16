import React, { useState, useEffect } from 'react'
import { MapContainer, TileLayer, Marker, Popup, Polyline } from 'react-leaflet'
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
    return <Marker position={position}><Popup>✈️ SKY-1</Popup></Marker>;
}

function App() {
    const [position, setPosition] = useState([40.1281, 32.9951]);
    // Weather eklendi
    const [telemetry, setTelemetry] = useState({ speed: 0, altitude: 0, fuel: 0, flightId: "", status: "", weather: "" });

    const [livePath, setLivePath] = useState([]);
    const [airports, setAirports] = useState({});
    const [selectedStart, setSelectedStart] = useState("");
    const [selectedEnd, setSelectedEnd] = useState("");
    const [flightInfo, setFlightInfo] = useState({ distance: 0, fuel: 0 });
    const [warning, setWarning] = useState("");
    const [showSuccess, setShowSuccess] = useState(false);
    const [showHistoryModal, setShowHistoryModal] = useState(false);
    const [pastFlights, setPastFlights] = useState([]);
    const [historyPath, setHistoryPath] = useState([]);

    useEffect(() => {
        const client = new Client({
            brokerURL: 'ws://localhost:8080/ws',
            onConnect: () => {
                client.subscribe('/topic/telemetry', (message) => {
                    const data = JSON.parse(message.body);

                    setPosition([data.latitude, data.longitude]);
                    setTelemetry({
                        speed: data.speed,
                        altitude: Math.round(data.altitude),
                        fuel: Math.round(data.remainingFuel),
                        flightId: data.flightId,
                        status: data.status,
                        weather: data.weather // YENİ
                    });

                    if (data.status === "FLYING") {
                        setLivePath(prev => [...prev, [data.latitude, data.longitude]]);
                        setShowSuccess(false);
                    }
                    else if (data.status === "LANDED") {
                        setShowSuccess(true);
                        setWarning("");
                    }
                    // YENİ DURUM: CRASHED (Yakıt bitti)
                    else if (data.status === "CRASHED") {
                        setWarning("⚠️ YAKIT BİTTİ! DÜŞÜŞ!");
                        setShowSuccess(false);
                    }

                    if (data.status === "FLYING" && data.remainingFuel < 300) {
                        setWarning("⚠️ DÜŞÜK YAKIT!");
                    }
                    else if (data.status === "FLYING") {
                        setWarning("");
                    }
                });
            },
        });
        client.activate();

        fetch("http://localhost:8080/api/telemetry/airports")
            .then(res => res.json())
            .then(data => setAirports(data));

        return () => client.deactivate();
    }, []);

    useEffect(() => {
        if(selectedStart && selectedEnd) {
            const start = airports[selectedStart];
            const end = airports[selectedEnd];
            fetch(`http://localhost:8080/api/telemetry/calculate?startLat=${start[0]}&startLon=${start[1]}&endLat=${end[0]}&endLon=${end[1]}`)
                .then(res => res.json())
                .then(data => setFlightInfo({ distance: data.distanceKm, fuel: data.estimatedFuel }));
        }
    }, [selectedStart, selectedEnd, airports]);

    const handleStartFlight = () => {
        if(!selectedStart || !selectedEnd) return;
        const start = airports[selectedStart];
        const end = airports[selectedEnd];

        fetch(`http://localhost:8080/api/telemetry/start-flight?startLat=${start[0]}&startLon=${start[1]}&endLat=${end[0]}&endLon=${end[1]}`, {
            method: 'POST'
        }).then(() => {
            setLivePath([]);
            setHistoryPath([]);
            setWarning("");
            setShowSuccess(false);
        });
    };

    const openHistory = () => {
        fetch("http://localhost:8080/api/telemetry/flights")
            .then(res => res.json())
            .then(data => {
                setPastFlights(data);
                setShowHistoryModal(true);
            });
    };

    const loadFlightHistory = (flightId) => {
        fetch(`http://localhost:8080/api/telemetry/history/${flightId}`)
            .then(res => res.json())
            .then(data => {
                const path = data.map(p => [p.latitude, p.longitude]);
                setHistoryPath(path);
                setLivePath([]);
                setShowHistoryModal(false);
                alert(`${flightId} yüklendi!`);
            });
    };

    return (
        <div style={{ height: '100vh', width: '100vw', display: 'flex' }}>

            {/* SOL PANEL */}
            <div style={{
                width: '320px', backgroundColor: '#1e272e', color: 'white', padding: '20px',
                boxShadow: '2px 0 10px rgba(0,0,0,0.5)', zIndex: 1000, display: 'flex', flexDirection: 'column', gap: '15px'
            }}>
                <h2 style={{color: '#00d4ff', borderBottom: '1px solid #555', paddingBottom: '10px'}}>🛫 FLIGHT OPS</h2>

                <div style={{fontSize:'0.8rem', color:'#aaa'}}>
                    ID: {telemetry.flightId} <br/>
                    DURUM: <span style={{color: telemetry.status === 'LANDED' ? '#0be881' : telemetry.status === 'CRASHED' ? 'red' : 'white'}}>{telemetry.status}</span>
                </div>

                <label>Kalkış</label>
                <select style={{padding:'8px'}} onChange={(e) => setSelectedStart(e.target.value)}>
                    <option value="">Seçiniz...</option>
                    {Object.keys(airports).map(name => <option key={name} value={name}>{name}</option>)}
                </select>

                <label>Varış</label>
                <select style={{padding:'8px'}} onChange={(e) => setSelectedEnd(e.target.value)}>
                    <option value="">Seçiniz...</option>
                    {Object.keys(airports).map(name => <option key={name} value={name}>{name}</option>)}
                </select>

                {flightInfo.distance > 0 && (
                    <div style={{backgroundColor:'#485460', padding:'10px', borderRadius:'8px', fontSize:'0.9rem'}}>
                        <div>📏 Mesafe: {flightInfo.distance} km</div>
                        <div>⛽ Planlanan: {flightInfo.fuel} L</div>
                    </div>
                )}

                <button onClick={handleStartFlight}
                        style={{marginTop:'10px', padding:'12px', backgroundColor:'#0be881', border:'none', borderRadius:'5px', fontWeight:'bold', cursor:'pointer'}}>
                    UÇUŞU BAŞLAT 🚀
                </button>

                <hr style={{borderColor:'#555', width:'100%'}} />

                <button onClick={openHistory} style={{padding: '10px', backgroundColor: '#ffa502', border:'none', borderRadius:'5px', color:'#1e272e', fontWeight:'bold', cursor:'pointer'}}>
                    📂 GEÇMİŞ UÇUŞLARIM
                </button>

                <div style={{fontFamily:'monospace', fontSize:'1rem', marginTop:'auto'}}>
                    {/* HAVA DURUMU GÖSTERGESİ */}
                    <p>HAVA: <span style={{color: '#fff'}}>{telemetry.weather}</span></p>
                    <p>HIZ: {telemetry.speed} km/s</p>
                    <p>İRTİFA: {telemetry.altitude} m</p>
                    <p>YAKIT: <span style={{color: telemetry.fuel < 300 ? 'red' : '#ffa502'}}>{telemetry.fuel} L</span></p>
                </div>

                {warning && (
                    <div style={{backgroundColor: 'red', color: 'white', padding: '10px', borderRadius: '5px', animation: 'pulse 1s infinite', textAlign:'center'}}>
                        {warning}
                    </div>
                )}
            </div>

            {/* HARİTA */}
            <div style={{ flex: 1, position: 'relative' }}>
                <MapContainer center={[39.9334, 32.8597]} zoom={5} zoomControl={false} style={{ height: '100%', width: '100%' }}>
                    <TileLayer url="https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png" />
                    <PlaneController position={position} />
                    {livePath.length > 0 && <Polyline positions={livePath} color="#00d4ff" weight={4} />}
                    {historyPath.length > 0 && <Polyline positions={historyPath} color="#ffa502" weight={4} opacity={0.8} />}
                </MapContainer>

                {/* BAŞARI MESAJI (Tıklayınca Kapanır) */}
                {showSuccess && (
                    <div
                        onClick={() => setShowSuccess(false)} // TIKLAYINCA GİZLE
                        style={{
                            position: 'absolute', top: '0', left: '0', width: '100%', height: '100%',
                            backgroundColor: 'rgba(0,0,0,0.5)', // Arka planı biraz karart
                            display: 'flex', justifyContent: 'center', alignItems: 'center',
                            zIndex: 9999, cursor: 'pointer' // Tıklanabilir olduğunu göster
                        }}>
                        <div style={{
                            backgroundColor: 'rgba(11, 232, 129, 0.9)', color: '#1e272e',
                            padding: '40px', borderRadius: '20px', textAlign: 'center',
                            boxShadow: '0 0 50px rgba(11, 232, 129, 0.8)', border: '2px solid white'
                        }}>
                            <h1 style={{margin:0, fontSize:'2.5rem'}}>🎉 UÇUŞ TAMAMLANDI</h1>
                            <p>Kapatmak için ekrana tıklayın</p>
                        </div>
                    </div>
                )}

                {/* MODAL */}
                {showHistoryModal && (
                    <div style={{
                        position: 'absolute', top: '50%', left: '50%', transform: 'translate(-50%, -50%)',
                        backgroundColor: '#1e272e', color: 'white', width: '300px', maxHeight: '400px',
                        overflowY: 'auto', padding: '20px', borderRadius: '10px', zIndex: 10000,
                        boxShadow: '0 10px 30px rgba(0,0,0,0.8)', border: '1px solid #444'
                    }}>
                        <div style={{display:'flex', justifyContent:'space-between', alignItems:'center', marginBottom:'15px'}}>
                            <h3>📂 SEYİR DEFTERİ</h3>
                            <button onClick={() => setShowHistoryModal(false)} style={{background:'red', color:'white', border:'none', padding:'5px 10px', cursor:'pointer', borderRadius:'5px'}}>X</button>
                        </div>
                        {pastFlights.length === 0 ? <p>Kayıtlı uçuş yok.</p> : (
                            <ul style={{listStyle:'none', padding:0}}>
                                {pastFlights.map(fid => (
                                    <li key={fid} style={{marginBottom:'10px'}}>
                                        <button onClick={() => loadFlightHistory(fid)} style={{width: '100%', padding: '10px', backgroundColor: '#485460', color: 'white', border: 'none', borderRadius: '5px', cursor: 'pointer', textAlign:'left'}}>✈️ {fid}</button>
                                    </li>
                                ))}
                            </ul>
                        )}
                    </div>
                )}
            </div>

            <style>{`
        @keyframes pulse { 0% { opacity: 1; } 50% { opacity: 0.5; } 100% { opacity: 1; } }
      `}</style>
        </div>
    )
}

export default App