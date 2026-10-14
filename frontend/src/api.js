import { API_URL } from './config'

async function request(path, options) {
  const res = await fetch(API_URL + path, options)
  const text = await res.text()
  let body = null
  try {
    body = text ? JSON.parse(text) : null
  } catch {
    body = null
  }
  if (!res.ok) {
    const err = new Error((body && body.error) || `HTTP ${res.status}`)
    err.status = res.status
    throw err
  }
  return body
}

const post = (path, payload) =>
  request(path, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: payload === undefined ? undefined : JSON.stringify(payload),
  })

export const getUavs = () => request('/api/uavs')
export const getZones = () => request('/api/zones')
export const getAlerts = (limit = 50) => request(`/api/alerts?limit=${limit}`)
export const getWind = () => request('/api/wind')
export const setWind = (wind) => post('/api/wind', wind)
export const startUav = (id) => post(`/api/uavs/${id}/start`)
export const returnToLaunch = (id) => post(`/api/uavs/${id}/rtl`)
export const setLink = (id, up) => post(`/api/uavs/${id}/link`, { up })
export const getSorties = () => request('/api/sorties')
export const getTrack = (sortieId) => request(`/api/sorties/${encodeURIComponent(sortieId)}/track?max_points=2000`)
