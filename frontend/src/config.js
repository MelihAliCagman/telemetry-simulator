// Sunucu adresi: geliştirmede varsayılan olarak yerel C++ sunucusu.
// Başka bir adres için frontend/.env içinde VITE_API_URL=http://host:port tanımlayın.
export const API_URL = import.meta.env.VITE_API_URL || 'http://localhost:8080'
export const WS_URL = API_URL.replace(/^http/, 'ws') + '/ws'

export const PHASE_LABELS = {
  PREFLIGHT: 'Yerde',
  TAKEOFF: 'Kalkış',
  CLIMB: 'Tırmanış',
  CRUISE: 'Seyir',
  LOITER: 'Bekleme',
  RTL: 'Eve dönüş',
  LANDING: 'İniş',
  LANDED: 'İndi',
  CRASHED: 'Düştü',
}

export const ALERT_LABELS = {
  LOW_BATTERY: 'Düşük batarya',
  CRITICAL_BATTERY: 'Kritik batarya',
  GEOFENCE_BREACH: 'Sınır ihlali',
  LINK_LOST: 'Bağ kaybı',
  INSUFFICIENT_RETURN_ENERGY: 'Dönüş enerjisi yetersiz',
}

export const AIRBORNE_PHASES = ['TAKEOFF', 'CLIMB', 'CRUISE', 'LOITER', 'RTL', 'LANDING']

const COLORS = ['#38bdf8', '#a3e635', '#f472b6', '#fbbf24', '#c084fc', '#2dd4bf']
export const colorFor = (id) => COLORS[(id - 1) % COLORS.length]
