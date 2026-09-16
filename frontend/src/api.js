const API_BASE = ''

async function get(path, fullResponse = false) {
  const res = await fetch(`${API_BASE}${path}`, { cache: 'no-store' })
  if (!res.ok) throw new Error(`${path} -> HTTP ${res.status}`)
  const json = await res.json()
  if (json.code !== 0) throw new Error(json.msg || `${path} 返回错误`)
  return fullResponse ? json : json.data
}

export const api = {
  overview: () => get('/api/stats/overview'),
  compatStats: (days = 7) => get(`/api/stats?days=${days}`),
  revenueTrend: (days = 7) => get(`/api/stats/revenue-trend?days=${days}`),
  volumeTrend: (days = 7) => get(`/api/stats/charging-volume-trend?days=${days}`),
  sessionTrend: (days = 7) => get(`/api/stats/session-count-trend?days=${days}`),
  hourly: (days) => get(days ? `/api/stats/hourly-distribution?days=${days}` : '/api/stats/hourly-distribution'),
  heatmap: (days) => get(days ? `/api/stats/weekday-heatmap?days=${days}` : '/api/stats/weekday-heatmap'),
  rankings: (days) => get(days ? `/api/stats/station-ranking?limit=8&days=${days}` : '/api/stats/station-ranking?limit=8'),
  stations: () => get('/api/stats/station-distribution'),
  weekdayPattern: (days) => get(days ? `/api/stats/weekday-pattern?days=${days}` : '/api/stats/weekday-pattern'),
  userMetrics: (days) => get(days ? `/api/stats/user-metrics?days=${days}` : '/api/stats/user-metrics'),
  deviceOps: (days) => get(days ? `/api/stats/battery/device-operations?days=${days}` : '/api/stats/battery/device-operations'),
  weatherImpact: (days) => get(days ? `/api/stats/weather-impact?days=${days}` : '/api/stats/weather-impact'),
  forecast24h: () => get('/api/predict/24h', true)
}
