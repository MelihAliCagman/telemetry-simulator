import { useState } from 'react'
import { setWind } from '../api'

const fmt = (s) => {
  const t = Math.floor(s)
  const h = String(Math.floor(t / 3600)).padStart(2, '0')
  const m = String(Math.floor((t % 3600) / 60)).padStart(2, '0')
  const sec = String(t % 60).padStart(2, '0')
  return `${h}:${m}:${sec}`
}

export default function TopBar({ connected, meta, onHistory, onError }) {
  const [speed, setSpeed] = useState(6)
  const [from, setFrom] = useState(300)

  const applyWind = () =>
    setWind({ speed_mps: Number(speed), from_deg: Number(from), gust_mps: 1.5 }).catch((e) => onError(e.message))

  return (
    <header className="topbar">
      <div className="brand">
        <span className="brand-mark">▲</span> UAV YKİ
        <span className="muted small"> Yer Kontrol İstasyonu Simülatörü</span>
      </div>

      <div className="topbar-right">
        <span className={`conn ${connected ? 'on' : 'off'}`}>{connected ? 'Bağlı' : 'Bağlantı yok'}</span>
        <span className="muted">Sim. süresi <b>{fmt(meta.simTime)}</b> (x{meta.timeScale})</span>

        <span className="wind">
          Rüzgar
          <input type="number" min="0" max="40" value={speed} onChange={(e) => setSpeed(e.target.value)} /> m/s
          <input type="number" min="0" max="360" value={from} onChange={(e) => setFrom(e.target.value)} />° yönünden
          <button className="btn" onClick={applyWind}>Uygula</button>
        </span>

        <button className="btn" onClick={onHistory}>Sorti kayıtları</button>
      </div>
    </header>
  )
}
