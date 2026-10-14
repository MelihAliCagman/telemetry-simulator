import { useEffect, useRef, useState } from 'react'
import { WS_URL, AIRBORNE_PHASES } from '../config'
import { getAlerts } from '../api'

const MAX_TRAIL_POINTS = 3000
const MAX_ALERTS = 100

// Sunucuyla WebSocket bağlantısını kurar (kopunca yeniden bağlanır) ve canlı durumu tutar.
export function useTelemetry() {
  const [uavs, setUavs] = useState({})
  const [trails, setTrails] = useState({})
  const [alerts, setAlerts] = useState([])
  const [connected, setConnected] = useState(false)
  const [meta, setMeta] = useState({ simTime: 0, timeScale: 1 })
  const sortieByUav = useRef({})

  useEffect(() => {
    let cancelled = false
    getAlerts()
      .then((list) => !cancelled && setAlerts([...list].reverse().slice(0, MAX_ALERTS)))
      .catch(() => {})
    return () => {
      cancelled = true
    }
  }, [])

  useEffect(() => {
    let ws
    let timer
    let closed = false

    const onTelemetry = (msg) => {
      const byId = {}
      msg.uavs.forEach((u) => {
        byId[u.id] = u
      })
      setUavs(byId)
      setMeta({ simTime: msg.sim_time_s, timeScale: msg.time_scale })

      setTrails((prev) => {
        const next = { ...prev }
        msg.uavs.forEach((u) => {
          // Yeni görev (sortie) başladığında eski izi temizle.
          if (sortieByUav.current[u.id] !== u.sortie_id) {
            sortieByUav.current[u.id] = u.sortie_id
            next[u.id] = []
          }
          if (AIRBORNE_PHASES.includes(u.phase)) {
            const trail = next[u.id] ? [...next[u.id]] : []
            const last = trail[trail.length - 1]
            if (!last || last[0] !== u.lat || last[1] !== u.lon) trail.push([u.lat, u.lon])
            next[u.id] = trail.length > MAX_TRAIL_POINTS ? trail.slice(-MAX_TRAIL_POINTS) : trail
          }
        })
        return next
      })
    }

    const connect = () => {
      ws = new WebSocket(WS_URL)
      ws.onopen = () => setConnected(true)
      ws.onclose = () => {
        setConnected(false)
        if (!closed) timer = setTimeout(connect, 2000)
      }
      ws.onerror = () => ws.close()
      ws.onmessage = (event) => {
        let msg
        try {
          msg = JSON.parse(event.data)
        } catch {
          return
        }
        if (msg.type === 'telemetry') onTelemetry(msg)
        else if (msg.type === 'alert') setAlerts((prev) => [msg.alert, ...prev].slice(0, MAX_ALERTS))
      }
    }

    connect()
    return () => {
      closed = true
      clearTimeout(timer)
      if (ws) ws.close()
    }
  }, [])

  return { uavs, trails, alerts, connected, meta }
}
