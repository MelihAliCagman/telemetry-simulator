import { useEffect, useState } from 'react'
import { getSorties } from '../api'
import { PHASE_LABELS } from '../config'

export default function HistoryModal({ onClose, onLoad }) {
  const [sorties, setSorties] = useState(null)
  const [error, setError] = useState('')

  useEffect(() => {
    getSorties()
      .then(setSorties)
      .catch((e) =>
        setError(e.status === 503 ? 'Veritabanı devre dışı olduğu için geçmiş kayıtları yok.' : `Yüklenemedi: ${e.message}`),
      )
  }, [])

  return (
    <div className="modal-backdrop" onClick={onClose}>
      <div className="modal" onClick={(e) => e.stopPropagation()}>
        <div className="modal-head">
          <h3>Sorti kayıtları</h3>
          <button className="btn" onClick={onClose}>Kapat</button>
        </div>
        {error && <p className="bad">{error}</p>}
        {!error && sorties === null && <p className="muted">Yükleniyor...</p>}
        {sorties && sorties.length === 0 && <p className="muted">Kayıtlı sorti yok.</p>}
        {sorties && sorties.length > 0 && (
          <table>
            <thead>
              <tr><th>Sorti</th><th>İHA</th><th>Başlangıç</th><th>Sonuç</th><th>Örnek</th><th /></tr>
            </thead>
            <tbody>
              {sorties.map((s) => (
                <tr key={s.id}>
                  <td>{s.id}</td>
                  <td>{s.callsign}</td>
                  <td>{new Date(s.started_at).toLocaleString('tr-TR')}</td>
                  <td>{s.final_phase ? PHASE_LABELS[s.final_phase] ?? s.final_phase : 'devam ediyor'}</td>
                  <td>{s.sample_count}</td>
                  <td><button className="btn primary" onClick={() => onLoad(s)}>Rotayı göster</button></td>
                </tr>
              ))}
            </tbody>
          </table>
        )}
      </div>
    </div>
  )
}
