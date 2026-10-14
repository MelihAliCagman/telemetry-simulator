import { ALERT_LABELS } from '../config'

const clock = (iso) => (iso ? new Date(iso).toLocaleTimeString('tr-TR') : '')

export default function AlertPanel({ alerts }) {
  if (alerts.length === 0) return <p className="muted small">Aktif uyarı yok.</p>
  return (
    <ul className="alerts">
      {alerts.map((a) => (
        <li key={a.seq} className={`alert-item sev-${a.severity}`}>
          <div>
            <b>{a.callsign}</b> — {ALERT_LABELS[a.type] ?? a.type}
          </div>
          <div className="small">{a.message}</div>
          <div className="muted small">{clock(a.raised_at)}</div>
        </li>
      ))}
    </ul>
  )
}
