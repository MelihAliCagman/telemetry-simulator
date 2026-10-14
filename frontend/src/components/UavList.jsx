import { PHASE_LABELS, ALERT_LABELS, AIRBORNE_PHASES, colorFor } from '../config'

function BatteryBar({ pct }) {
  const level = pct < 10 ? 'crit' : pct < 25 ? 'warn' : 'ok'
  return (
    <div className="bar" title={`Batarya %${pct}`}>
      <div className={`bar-fill ${level}`} style={{ width: `${Math.max(0, Math.min(100, pct))}%` }} />
    </div>
  )
}

function UavCard({ uav, selected, onSelect, onAction }) {
  const airborne = AIRBORNE_PHASES.includes(uav.phase)
  const canStart = uav.phase === 'PREFLIGHT' || uav.phase === 'LANDED'
  const canRtl = ['TAKEOFF', 'CLIMB', 'CRUISE', 'LOITER'].includes(uav.phase)

  return (
    <div className={`card ${selected ? 'selected' : ''}`} onClick={() => onSelect(uav.id)}>
      <div className="card-head">
        <span className="dot" style={{ background: colorFor(uav.id) }} />
        <strong>{uav.callsign}</strong>
        <span className="muted">{uav.type}</span>
        <span className={`phase phase-${uav.phase}`}>{PHASE_LABELS[uav.phase] ?? uav.phase}</span>
      </div>

      <BatteryBar pct={uav.battery_pct} />

      <div className="stats">
        <span>İrtifa <b>{Math.round(uav.alt_m)} m</b></span>
        <span>Hız <b>{uav.groundspeed_mps} m/s</b></span>
        <span>Yön <b>{Math.round(uav.heading_deg)}°</b></span>
        <span>Batarya <b>%{uav.battery_pct}</b></span>
        <span>Üsse <b>{(uav.distance_to_home_m / 1000).toFixed(1)} km</b></span>
        <span>
          Bağ <b className={uav.link_ok ? '' : 'bad'}>{uav.link_ok ? `%${Math.round(uav.link_quality * 100)}` : 'YOK'}</b>
        </span>
      </div>

      {uav.waypoint_count > 0 && (
        <div className="muted small">
          Görev noktası {Math.min(uav.waypoint_index + 1, uav.waypoint_count)}/{uav.waypoint_count}
          {uav.failsafe_active && ' • FAILSAFE'}
        </div>
      )}

      {uav.active_alerts.length > 0 && (
        <div className="chips">
          {uav.active_alerts.map((a) => (
            <span key={a} className="chip">{ALERT_LABELS[a] ?? a}</span>
          ))}
        </div>
      )}

      <div className="actions" onClick={(e) => e.stopPropagation()}>
        <button className="btn primary" disabled={!canStart} onClick={() => onAction('start', uav)}>
          {uav.phase === 'LANDED' ? 'Yeni sorti' : 'Göreve başla'}
        </button>
        <button className="btn warn" disabled={!canRtl} onClick={() => onAction('rtl', uav)}>Eve dön</button>
        <button className="btn" disabled={!airborne} onClick={() => onAction('link', uav)}>
          {uav.link_ok ? 'Bağı kes' : 'Bağı aç'}
        </button>
      </div>
    </div>
  )
}

export default function UavList({ uavs, selectedId, onSelect, onAction }) {
  const list = Object.values(uavs)
  if (list.length === 0) return <p className="muted">Sunucudan veri bekleniyor...</p>
  return (
    <div className="cards">
      {list.map((u) => (
        <UavCard key={u.id} uav={u} selected={u.id === selectedId} onSelect={onSelect} onAction={onAction} />
      ))}
    </div>
  )
}
