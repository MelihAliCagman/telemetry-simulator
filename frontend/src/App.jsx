import { useEffect, useState } from 'react'
import './App.css'
import { getZones, startUav, returnToLaunch, setLink, getTrack } from './api'
import { useTelemetry } from './hooks/useTelemetry'
import MapView from './components/MapView'
import UavList from './components/UavList'
import AlertPanel from './components/AlertPanel'
import HistoryModal from './components/HistoryModal'
import TopBar from './components/TopBar'

export default function App() {
  const { uavs, trails, alerts, connected, meta } = useTelemetry()
  const [zones, setZones] = useState([])
  const [selectedId, setSelectedId] = useState(null)
  const [showHistory, setShowHistory] = useState(false)
  const [historyTrack, setHistoryTrack] = useState(null)
  const [notice, setNotice] = useState('')

  useEffect(() => {
    getZones().then(setZones).catch(() => {})
  }, [])

  useEffect(() => {
    if (!notice) return undefined
    const t = setTimeout(() => setNotice(''), 4000)
    return () => clearTimeout(t)
  }, [notice])

  const handleAction = (action, uav) => {
    const call =
      action === 'start' ? startUav(uav.id) : action === 'rtl' ? returnToLaunch(uav.id) : setLink(uav.id, !uav.link_ok)
    call.catch((e) => setNotice(`${uav.callsign}: ${e.message}`))
    if (action === 'start') setHistoryTrack(null)
  }

  const handleLoadHistory = (sortie) => {
    getTrack(sortie.id)
      .then((points) => {
        setHistoryTrack(points.map((p) => [p.lat, p.lon]))
        setShowHistory(false)
        setNotice(`${sortie.id} rotası haritada gösteriliyor (${points.length} nokta)`)
      })
      .catch((e) => setNotice(`Rota yüklenemedi: ${e.message}`))
  }

  return (
    <div className="app">
      <TopBar connected={connected} meta={meta} onHistory={() => setShowHistory(true)} onError={setNotice} />

      <div className="body">
        <aside className="side">
          <section>
            <h2>Filo</h2>
            <UavList uavs={uavs} selectedId={selectedId} onSelect={setSelectedId} onAction={handleAction} />
          </section>
          <section>
            <h2>
              Uyarılar
              {alerts.length > 0 && <span className="badge">{alerts.length}</span>}
            </h2>
            <AlertPanel alerts={alerts} />
          </section>
        </aside>

        <main className="map-wrap">
          <MapView
            uavs={uavs}
            trails={trails}
            zones={zones}
            selectedId={selectedId}
            onSelect={setSelectedId}
            historyTrack={historyTrack}
          />
          {historyTrack && (
            <button className="btn floating" onClick={() => setHistoryTrack(null)}>Geçmiş rotayı gizle</button>
          )}
          {notice && <div className="notice">{notice}</div>}
        </main>
      </div>

      {showHistory && <HistoryModal onClose={() => setShowHistory(false)} onLoad={handleLoadHistory} />}
    </div>
  )
}
