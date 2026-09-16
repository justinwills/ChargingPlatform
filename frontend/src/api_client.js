// Vite can proxy relative `/api` requests in development, but the dashboard is
// often opened from Windows while Flask runs inside the VM. When a VM target is
// configured, call it directly so static builds and dev mode behave the same.
const configuredBase = (
  import.meta.env.VITE_API_BASE_URL ||
  import.meta.env.VITE_API_PROXY_TARGET ||
  ''
).replace(/\/$/, '')
const API_BASE = configuredBase
const runtimeHost = typeof window !== 'undefined' && window.location.hostname
  ? window.location.hostname
  : 'localhost'
const FALLBACK_API_BASE = configuredBase || `http://${runtimeHost}:8090`
const REQUEST_TIMEOUT_MS = 8000

async function request(url, path) {
  const controller = new AbortController()
  const timeoutId = window.setTimeout(() => controller.abort(), REQUEST_TIMEOUT_MS)
  let res
  try {
    res = await fetch(url, { cache: 'no-store', signal: controller.signal })
  } catch (error) {
    if (error?.name === 'AbortError') {
      throw new Error(`${path} 请求超时：${url}`)
    }
    throw new Error(`${path} 连接失败：${url}`)
  } finally {
    window.clearTimeout(timeoutId)
  }
  const text = await res.text()
  if (!res.ok) {
    const preview = text.trim().slice(0, 120)
    throw new Error(`${path} -> HTTP ${res.status}${preview ? `：${preview}` : ''}`)
  }
  try {
    return JSON.parse(text)
  } catch (error) {
    const preview = text.trim().slice(0, 120)
    throw new Error(`${path} 返回的不是 JSON：${preview || '空响应'}`)
  }
}

async function get(path, fullResponse = false) {
  let json
  try {
    json = await request(`${API_BASE}${path}`, path)
  } catch (firstError) {
    // A static page commonly gets a 404, HTML, or a connection error for its
    // relative API URL. Retry once against the Flask service in that case.
    if (!FALLBACK_API_BASE || FALLBACK_API_BASE === API_BASE) throw firstError
    json = await request(`${FALLBACK_API_BASE}${path}`, path)
  }
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
  forecast: (hours = 24) => get(`/api/predict/${hours}h`, true),
  forecast24h: () => get('/api/predict/24h', true),
  recommend: () => get('/api/recommend/stations', true)
}
