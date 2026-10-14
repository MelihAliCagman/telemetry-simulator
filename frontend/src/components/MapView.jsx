import { MapContainer, TileLayer, Marker, Popup, Polyline, Polygon, CircleMarker } from 'react-leaflet'
import L from 'leaflet'
import 'leaflet/dist/leaflet.css'
import { PHASE_LABELS, colorFor } from '../config'

function uavIcon(uav, selected) {
  const color = colorFor(uav.id)
  const alert = uav.active_alerts.length > 0
  const html = `
    <div class="uav-marker ${selected ? 'selected' : ''} ${alert ? 'alert' : ''}" style="--c:${color}">
      <svg viewBox="0 0 24 24" width="30" height="30" style="transform:rotate(${uav.heading_deg}deg)">
        <path d="M12 2 L20 21 L12 17 L4 21 Z" fill="${color}" stroke="#0b1220" stroke-width="1.5" stroke-linejoin="round"/>
      </svg>
      <span class="uav-label">${uav.callsign}</span>
    </div>`
  return L.divIcon({ html, className: 'uav-icon', iconSize: [30, 30], iconAnchor: [15, 15] })
}

export default function MapView({ uavs, trails, zones, selectedId, onSelect, historyTrack }) {
  const list = Object.values(uavs)
  return (
    <MapContainer center={[39.92, 32.95]} zoom={10} zoomControl={true} className="map">
      <TileLayer
        url="https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png"
        attribution='&copy; <a href="https://www.openstreetmap.org/copyright">OpenStreetMap</a> katkıda bulunanlar'
      />

      {zones.map((z) => (
        <Polygon
          key={z.name}
          positions={z.polygon.map((p) => [p.lat, p.lon])}
          pathOptions={{
            color: z.kind === 'NO_FLY' ? '#ef4444' : '#22c55e',
            fillOpacity: 0.15,
            dashArray: z.kind === 'NO_FLY' ? undefined : '6 6',
          }}
        >
          <Popup>
            {z.name}
            <br />
            {z.kind === 'NO_FLY' ? 'Yasak bölge' : 'İzinli bölge'}
          </Popup>
        </Polygon>
      ))}

      {list.map((u) => (
        <CircleMarker
          key={`home-${u.id}`}
          center={[u.home.lat, u.home.lon]}
          radius={5}
          pathOptions={{ color: colorFor(u.id), fillOpacity: 0.3 }}
        />
      ))}

      {list.map(
        (u) =>
          trails[u.id] &&
          trails[u.id].length > 1 && (
            <Polyline key={`trail-${u.id}`} positions={trails[u.id]} pathOptions={{ color: colorFor(u.id), weight: 3, opacity: 0.8 }} />
          ),
      )}

      {historyTrack && historyTrack.length > 1 && (
        <Polyline positions={historyTrack} pathOptions={{ color: '#f59e0b', weight: 4, dashArray: '8 6' }} />
      )}

      {list.map((u) => (
        <Marker
          key={u.id}
          position={[u.lat, u.lon]}
          icon={uavIcon(u, u.id === selectedId)}
          eventHandlers={{ click: () => onSelect(u.id) }}
        >
          <Popup>
            <b>{u.callsign}</b> ({u.type})
            <br />
            Faz: {PHASE_LABELS[u.phase] ?? u.phase}
            <br />
            İrtifa: {Math.round(u.alt_m)} m, Hız: {u.groundspeed_mps} m/s
            <br />
            Batarya: %{u.battery_pct}
          </Popup>
        </Marker>
      ))}
    </MapContainer>
  )
}
