<template>
  <div class="screen">
    <header>
      <div class="brand">
        <div class="brand-mark">&#9889;</div>
        <div>
          <h1>东软电动汽车充电桩应用管理平台</h1>
          <small>CHARGING OPERATIONS INTELLIGENCE</small>
        </div>
      </div>
      <div class="header-actions">
        <span class="status-dot"></span>
        <span id="qualityText">{{ qualityText }}</span>
        <span class="refresh-meta">高峰时段：{{ peakText }}</span>
        <span class="refresh-meta">最后更新：{{ updatedAt }}</span>
        <button type="button" @click="refresh">刷新数据</button>
      </div>
    </header>
    <dv-decoration-10 style="width: 100%; height: 6px" />

    <main>
      <div class="hero">
        <div>
          <div class="eyebrow">运营总览 / ANALYTICS</div>
          <h2>充电业务全域分析</h2>
          <p>从订单、站点、用户、设备、天气与机器学习模型多维度观察运营表现 · Vue3 + DataV + ECharts + Flask</p>
        </div>
        <div class="quality-badge">{{ modelQuality }}</div>
      </div>

      <!-- KPI 指标 -->
      <section class="kpi-grid" aria-label="充电业务指标">
        <div v-for="k in kpis" :key="k.label" class="kpi-card">
          <div class="kpi-label">{{ k.label }}</div>
          <div class="kpi-value" :style="{ color: k.color }">
            <span v-if="k.prefix">{{ k.prefix }}</span>{{ k.num }}<span class="kpi-unit">{{ k.unit }}</span>
          </div>
        </div>
      </section>

      <!-- 汇总条 -->
      <div class="summary-strip">
        <div v-for="s in summary" :key="s.label" class="summary-item">
          <span>{{ s.label }}</span><strong>{{ s.value }}</strong>
        </div>
      </div>

      <!-- ===== 01 运营趋势 ===== -->
      <div class="section-head">
        <span class="section-number">01</span><h3>运营趋势</h3>
        <span class="section-desc">营收、充电量与充电次数的近 {{ rangeDays }} 日变化</span>
        <div class="range-switch" role="group" aria-label="时间范围">
          <button v-for="d in ranges" :key="d" type="button" :class="{ active: rangeDays === d }" @click="setRange(d)">近{{ d }}天</button>
        </div>
      </div>
      <section class="grid-3">
        <div class="panel"><div class="panel-title">营收趋势（近{{ rangeDays }}日 · 元）</div><div class="chart"><EChart :option="revenueOpt" height="100%" /></div></div>
        <div class="panel"><div class="panel-title">充电量趋势（近{{ rangeDays }}日 · kWh）</div><div class="chart"><EChart :option="volumeOpt" height="100%" /></div></div>
        <div class="panel"><div class="panel-title">充电次数趋势（近{{ rangeDays }}日）</div><div class="chart"><EChart :option="sessionOpt" height="100%" /></div></div>
      </section>

      <!-- ===== 02 时段与星期规律 ===== -->
      <div class="section-head">
        <span class="section-number">02</span><h3>时段与星期规律</h3>
        <span class="section-desc">24 小时充电分布、星期规律与星期 × 小时热力图（近 {{ rangeDays }} 日）</span>
      </div>
      <section class="grid-3">
        <div class="panel"><div class="panel-title">24小时充电时段分布</div><div class="chart chart-tall"><EChart :option="hourlyOpt" height="100%" /></div></div>
        <div class="panel"><div class="panel-title">星期充电规律（kWh）</div><div class="chart chart-tall"><EChart :option="weekdayOpt" height="100%" /></div></div>
        <div class="panel"><div class="panel-title">星期 × 小时 充电热力图</div><div class="chart chart-tall"><EChart :option="heatmapOpt" height="100%" /></div></div>
      </section>

      <!-- ===== 03 站点与设备 ===== -->
      <div class="section-head">
        <span class="section-number">03</span><h3>站点与设备</h3>
        <span class="section-desc">近 {{ rangeDays }} 日 充电量排名 · 电桩状态与在线率为当前快照</span>
      </div>
      <section class="grid-3">
        <div class="panel">
          <div class="panel-title">充电站充电量排名 TOP</div>
          <div class="table-wrap">
            <table>
              <thead><tr><th>排名</th><th>站点</th><th>电量(kWh)</th><th>次数</th></tr></thead>
              <tbody>
                <tr v-for="(r, i) in ranking.slice(0, 6)" :key="r.station_id">
                  <td><span class="rank">{{ i + 1 }}</span></td>
                  <td>{{ r.station_name }}</td>
                  <td>{{ num(r.energy_kwh, 1) }}</td>
                  <td>{{ num(r.sessions, 0) }}</td>
                </tr>
              </tbody>
            </table>
          </div>
        </div>
        <div class="panel"><div class="panel-title">电桩状态分布</div><div class="chart chart-tall"><EChart :option="statusOpt" height="100%" /></div></div>
        <div class="panel">
          <div class="panel-title">充电站在线率</div>
          <div class="table-wrap">
            <table>
              <thead><tr><th>站点</th><th>电桩</th><th>空闲</th><th>在线率</th></tr></thead>
              <tbody>
                <tr v-for="s in stations.slice(0, 6)" :key="s.name">
                  <td>{{ s.name }}</td>
                  <td>{{ num(s.pileCount, 0) }}</td>
                  <td>{{ num(s.freePileCount, 0) }}</td>
                  <td><span class="meter"><i :style="{ width: (Number(s.onlineRate) || 0) + '%' }"></i></span>{{ num(s.onlineRate, 1) }}%</td>
                </tr>
              </tbody>
            </table>
          </div>
        </div>
      </section>

      <!-- ===== 04 用户 · 设备运行 · 天气 ===== -->
      <div class="section-head">
        <span class="section-number">04</span><h3>用户 · 设备运行 · 天气</h3>
        <span class="section-desc">用户充电行为、设备健康评分与不同天气下的充电需求（近 {{ rangeDays }} 日）</span>
      </div>
      <section class="grid-3">
        <div class="panel"><div class="panel-title">用户充电行为雷达</div><div class="chart chart-tall"><EChart :option="userOpt" height="100%" /></div></div>
        <div class="panel"><div class="panel-title">设备运行健康（故障 vs 健康评分）</div><div class="chart chart-tall"><EChart :option="deviceOpt" height="100%" /></div></div>
        <div class="panel"><div class="panel-title">不同天气下的充电需求</div><div class="chart chart-tall"><EChart :option="weatherOpt" height="100%" /></div></div>
      </section>

      <!-- ===== 05 模型与预测（机器学习，ml/ 由邱辰笙维护） ===== -->
      <div class="section-head">
        <span class="section-number">05</span><h3>模型与预测</h3>
        <span class="section-desc">机器学习负荷预测（ml/prediction_service.py，接口 /api/predict/24h）</span>
      </div>
      <section class="grid-2">
        <div class="panel"><div class="panel-title">未来24小时负荷预测（机器学习模型）</div><div class="chart chart-tall"><EChart :option="predictOpt" height="100%" /></div></div>
        <div class="panel">
          <div class="panel-title">预测模型信息</div>
          <div class="panel-note">{{ modelInfo }}</div>
          <div class="panel-note">{{ modelInfo2 }}</div>
        </div>
      </section>
    </main>

    <dv-decoration-5 style="width: 100%; height: 18px; margin-top: 14px" />
    <footer>最后更新：{{ updatedAt }} · 数据源：真实充电订单 / 站点 / 设备 / 天气 CSV · 后端：Flask :8090</footer>
  </div>
</template>

<script setup>
import { computed, onMounted, onBeforeUnmount, onErrorCaptured, ref } from 'vue'
import EChart from './components/EChart.vue'
import { api } from './api'

const num = (v, d = 2) => {
  const n = Number(v)
  return isFinite(n) ? n.toLocaleString('en-US', { minimumFractionDigits: d, maximumFractionDigits: d }) : '--'
}

onErrorCaptured((err, _inst, info) => {
  console.error('[onErrorCaptured]', info, err)
  return false
})

const PALETTE = ['#35d7ff', '#4f7cff', '#34d399', '#f6c453', '#fb923c', '#ff6b7a', '#a78bfa', '#4dd0e1']
const WEEK_CN = ['周一', '周二', '周三', '周四', '周五', '周六', '周日']
const WEATHER_CN = { sunny: '晴', cloudy: '多云', overcast: '阴', light_rain: '小雨', moderate_rain: '中雨', heavy_rain: '大雨', fog: '雾', haze: '霾', drizzle: '毛毛雨', snow: '雪', thunderstorm: '雷阵雨' }

const updatedAt = ref('加载中...')
const peakText = ref('--')
const qualityText = ref('数据快照已连接')
const modelInfo = ref('')
const modelInfo2 = ref('')
const modelQuality = ref('正在核对数据质量…')

const ov = ref({})
const compat = ref(null)
const ranking = ref([])
const stations = ref([])
const hourly = ref([])
const heatmap = ref([])
const weekday = ref([])
const userM = ref({})
const deviceOps = ref([])
const weather = ref([])
const revenuePts = ref([])
const volumePts = ref([])
const sessionPts = ref([])
const predictPts = ref([])

const ranges = [1, 7, 30, 60, 90, 150, 365]
const rangeDays = ref(7)

const fmt = (v, p) => Number(v ?? 0).toLocaleString('en-US', { minimumFractionDigits: p, maximumFractionDigits: p })

const kpis = computed(() => [
  { label: '总营收（元）', prefix: '¥', num: fmt(ov.value.revenue_total, 2), unit: '', color: '#35d7ff' },
  { label: '今日营收（元）', prefix: '¥', num: fmt(ov.value.revenue_today, 2), unit: '', color: '#34d399' },
  { label: '总充电量（kWh）', prefix: '', num: fmt(ov.value.total_energy_kwh, 1), unit: '', color: '#4f7cff' },
  { label: '总充电次数', prefix: '', num: fmt(ov.value.total_sessions, 0), unit: ' 次', color: '#f6c453' },
  { label: '平均单次费用（元）', prefix: '¥', num: fmt(ov.value.avg_order_fee_cny, 2), unit: '', color: '#ff6b7a' }
])

const summary = computed(() => [
  { label: '充电站', value: fmt(ov.value.active_stations, 0) },
  { label: '活跃用户', value: fmt(ov.value.active_users, 0) },
  { label: '平均充电时长', value: fmt(ov.value.avg_duration_hrs, 2) + ' h' },
  { label: '数据跨度', value: fmt(ov.value.span_days, 0) + ' 天' }
])

const axis = (color = '#7890b0') => ({
  axisLine: { lineStyle: { color: '#20334f' } },
  axisLabel: { color },
  splitLine: { lineStyle: { color: '#182b45' } }
})

const baseText = { color: '#b6c7de' }
const baseGrid = { left: 56, right: 24, top: 40, bottom: 36 }

const revenueOpt = computed(() => {
  const p = revenuePts.value
  if (!p.length) return {}
  const vals = p.map(x => x.revenue)
  const vmin = Math.min(...vals, 0)
  const vmax = Math.max(...vals, 1)
  const pad = (vmax - vmin) * 0.12 || Math.max(vmax * 0.1, 1)
  return {
    backgroundColor: 'transparent', textStyle: baseText,
    tooltip: { trigger: 'axis' },
    grid: baseGrid,
    xAxis: { type: 'category', data: p.map(x => x.date), boundaryGap: false, ...axis() },
    yAxis: { type: 'value', min: Math.max(0, vmin - pad), max: vmax + pad, ...axis() },
    series: [{
      name: '营收', type: 'line', smooth: true, symbolSize: 7,
      data: p.map(x => x.revenue),
      lineStyle: { color: '#4fd1ff', width: 2.4, shadowColor: 'rgba(79,209,255,0.5)', shadowBlur: 8 },
      itemStyle: { color: '#35d7ff', borderColor: '#e8fbff', borderWidth: 1 },
      areaStyle: {
        color: { type: 'linear', x: 0, y: 0, x2: 0, y2: 1,
                 colorStops: [{ offset: 0, color: 'rgba(79,209,255,0.4)' }, { offset: 1, color: 'rgba(79,209,255,0.03)' }] }
      }
    }]
  }
})

const volumeOpt = computed(() => {
  const p = volumePts.value
  if (!p.length) return {}
  return {
    backgroundColor: 'transparent', textStyle: baseText,
    tooltip: { trigger: 'axis', axisPointer: { type: 'shadow' } },
    grid: baseGrid,
    xAxis: { type: 'category', data: p.map(x => x.date), ...axis() },
    yAxis: { type: 'value', min: 0, ...axis() },
    series: [{
      name: '充电量', type: 'bar', barWidth: '42%',
      data: p.map(x => x.energy_kwh),
      itemStyle: {
        borderRadius: [4, 4, 0, 0],
        color: { type: 'linear', x: 0, y: 0, x2: 0, y2: 1,
                 colorStops: [{ offset: 0, color: '#34d399' }, { offset: 1, color: '#0f6f9e' }] }
      }
    }]
  }
})

const sessionOpt = computed(() => {
  const p = sessionPts.value
  if (!p.length) return {}
  const vals = p.map(x => x.count)
  const vmin = Math.min(...vals, 0)
  const vmax = Math.max(...vals, 1)
  const pad = (vmax - vmin) * 0.12
  return {
    backgroundColor: 'transparent', textStyle: baseText,
    tooltip: { trigger: 'axis' },
    grid: baseGrid,
    xAxis: { type: 'category', data: p.map(x => x.date.slice(5)), ...axis(), axisLabel: { color: '#7890b0', rotate: 30 } },
    yAxis: { type: 'value', min: Math.max(0, vmin - pad), max: vmax + pad, ...axis() },
    series: [{
      name: '次数', type: 'line', smooth: true, symbolSize: 6,
      data: p.map(x => x.count),
      lineStyle: { color: '#fb923c', width: 2.4 },
      itemStyle: { color: '#fb923c' },
      areaStyle: { color: { type: 'linear', x: 0, y: 0, x2: 0, y2: 1,
                             colorStops: [{ offset: 0, color: 'rgba(251,146,60,0.4)' }, { offset: 1, color: 'rgba(251,146,60,0.03)' }] } }
    }]
  }
})

const hourlyOpt = computed(() => {
  const h = hourly.value
  if (!h.length) return {}
  const maxS = Math.max(1, ...h.map(x => x.sessions))
  return {
    backgroundColor: 'transparent', textStyle: baseText,
    tooltip: { trigger: 'axis' },
    legend: { top: 0, textStyle: { color: '#b6c7de' } },
    grid: { ...baseGrid, top: 46 },
    xAxis: { type: 'category', data: h.map(x => x.hour + '时'), ...axis() },
    yAxis: [
      { type: 'value', min: 0, name: 'kWh', nameTextStyle: { color: '#34d399' }, ...axis() },
      { type: 'value', min: 0, max: maxS * 1.15, splitLine: { show: false }, axisLabel: { color: '#7890b0' } }
    ],
    series: [
      { name: '充电量', type: 'bar', barWidth: '50%', data: h.map(x => x.energy_kwh),
        itemStyle: { borderRadius: [3, 3, 0, 0],
          color: { type: 'linear', x: 0, y: 0, x2: 0, y2: 1,
                   colorStops: [{ offset: 0, color: '#4f7cff' }, { offset: 1, color: '#0b7f9e' }] } } },
      { name: '充电次数', type: 'line', smooth: true, yAxisIndex: 1, data: h.map(x => x.sessions),
        lineStyle: { color: '#f6c453', width: 2.2 }, itemStyle: { color: '#f6c453' } }
    ]
  }
})

const weekdayOpt = computed(() => {
  const w = weekday.value
  if (!w.length) return {}
  const map = { Mon: 0, Tue: 1, Wed: 2, Thu: 3, Fri: 4, Sat: 5, Sun: 6 }
  const data = w.map(r => ({ value: r.energy_kwh, itemStyle: { color: PALETTE[(map[r.weekday] ?? 2) % PALETTE.length] } }))
  return {
    backgroundColor: 'transparent', textStyle: baseText,
    tooltip: { trigger: 'axis', axisPointer: { type: 'shadow' } },
    grid: baseGrid,
    xAxis: { type: 'category', data: w.map(r => WEEK_CN[map[r.weekday]] ?? r.weekday), ...axis() },
    yAxis: { type: 'value', min: 0, name: 'kWh', ...axis() },
    series: [{ type: 'bar', barWidth: '50%', data, label: { show: true, position: 'top', color: '#b6c7de', fontSize: 10 } }]
  }
})

const heatmapOpt = computed(() => {
  const data = heatmap.value.map(r => [r.hour, r.weekday, r.count])
  const max = Math.max(1, ...heatmap.value.map(r => r.count))
  return {
    backgroundColor: 'transparent', textStyle: baseText,
    tooltip: {},
    grid: { left: 56, right: 28, top: 20, bottom: 60 },
    xAxis: { type: 'category', data: Array.from({ length: 24 }, (_, i) => i + '时'), splitArea: { show: true }, ...axis() },
    yAxis: { type: 'category', data: WEEK_CN, splitArea: { show: true }, ...axis() },
    visualMap: {
      min: 0, max,
      orient: 'horizontal', left: 'center', bottom: 0, textStyle: { color: '#b6c7de' },
      inRange: { color: ['#0b1e3f', '#0f3f7a', '#0f6f9e', '#35d7ff', '#f6c453', '#ff9aa6'] }
    },
    series: [{ type: 'heatmap', data, itemStyle: { borderColor: '#0b1e3f', borderWidth: 1 } }]
  }
})

const statusOpt = computed(() => {
  const s = compat.value?.pileStatus || {}
  const names = ['在用', '闲置', '故障']
  const colors = { '在用': '#4fd1ff', '闲置': '#39d98a', '故障': '#ff6b6b' }
  const data = names.filter(n => s[n]).map(n => ({ name: n, value: s[n], itemStyle: { color: colors[n] } }))
  return {
    backgroundColor: 'transparent', textStyle: baseText,
    tooltip: { trigger: 'item', formatter: '{b}: {c} 台 ({d}%)' },
    legend: { bottom: 0, textStyle: { color: '#b6c7de' } },
    series: [{
      type: 'pie', radius: ['40%', '66%'], center: ['50%', '46%'],
      label: { color: '#cfe0fa' }, data
    }]
  }
})

const userOpt = computed(() => {
  const u = userM.value || {}
  const items = [
    { name: '活跃用户', value: u.active_users ?? 0, max: 600 },
    { name: '人均充电次数', value: u.avg_sessions_per_user ?? 0, max: 60 },
    { name: '人均充电量(kWh)', value: u.avg_energy_per_user_kwh ?? 0, max: 1200 }
  ]
  return {
    backgroundColor: 'transparent', textStyle: baseText,
    radar: {
      indicator: items.map(i => ({ name: i.name, max: i.max })),
      radius: '68%', splitArea: { areaStyle: { color: ['rgba(167,139,250,0.06)', 'rgba(53,215,255,0.09)'] } },
      axisName: { color: '#c9d9ef' }, splitLine: { lineStyle: { color: '#20334f' } }
    },
    series: [{
      type: 'radar',
      data: [{ value: items.map(i => i.value), name: '用户行为' }],
      areaStyle: { color: 'rgba(167,139,250,0.4)' },
      lineStyle: { color: '#a78bfa', width: 2 },
      symbol: 'circle', symbolSize: 5, itemStyle: { color: '#f6c453' }
    }]
  }
})

const deviceOpt = computed(() => {
  const rows = deviceOps.value.slice(0, 15)
  if (!rows.length) return {}
  const maxS = Math.max(1, ...rows.map(r => r.successful_sessions))
  const minAvail = Math.min(...rows.map(r => r.avg_availability), 0.9)
  return {
    backgroundColor: 'transparent', textStyle: baseText,
    tooltip: {
      formatter: p => {
        const d = p.data
        return `可用率 ${d[2].toFixed(2)}<br/>健康评分 ${d[1].toFixed(1)}<br/>故障 ${d[0]} 次`
      }
    },
    grid: baseGrid,
    xAxis: { type: 'value', name: '故障次数', nameTextStyle: { color: '#b6c7de' }, ...axis() },
    yAxis: { type: 'value', name: '健康评分', min: Math.max(0, minAvail * 100 - 10), max: 100, ...axis() },
    visualMap: {
      dimension: 2, min: minAvail, max: 1, show: true,
      orient: 'horizontal', left: 'center', bottom: 0, textStyle: { color: '#b6c7de' },
      inRange: { color: ['#ff6b7a', '#f6c453', '#34d399'] }
    },
    series: [{
      type: 'scatter', symbolSize: d => 6 + (d[3] * 20) / maxS,
      data: rows.map(r => [r.total_faults, r.avg_health, r.avg_availability, r.successful_sessions]),
      itemStyle: { borderColor: '#eaf9ff', borderWidth: 1 }
    }]
  }
})

const weatherOpt = computed(() => {
  const rows = weather.value
  if (!rows.length) return {}
  const wcn = r => WEATHER_CN[r.weather_type] || (r.weather_name && r.weather_name !== '?' ? r.weather_name : r.weather_type)
  const names = rows.map(wcn)
  return {
    backgroundColor: 'transparent', textStyle: baseText,
    tooltip: { trigger: 'axis', axisPointer: { type: 'shadow' } },
    legend: { top: 0, textStyle: { color: '#b6c7de' } },
    grid: { ...baseGrid, top: 46 },
    xAxis: { type: 'category', data: names, ...axis(), axisLabel: { color: '#7890b0', rotate: 16 } },
    yAxis: [
      { type: 'value', min: 0, name: '充电次数', ...axis() },
      { type: 'value', name: '℃', splitLine: { show: false }, axisLabel: { color: '#7890b0' } }
    ],
    series: [
      { name: '充电次数', type: 'bar', barWidth: '42%',
        data: rows.map((r, i) => ({ value: r.sessions, itemStyle: { color: PALETTE[i % PALETTE.length] } })) },
      { name: '平均温度', type: 'line', smooth: true, yAxisIndex: 1, data: rows.map(r => r.avg_temp_c),
        lineStyle: { color: '#f6c453', width: 2 }, itemStyle: { color: '#f6c453' } }
    ]
  }
})

const predictOpt = computed(() => {
  const p = predictPts.value
  if (!p.length) return {}
  const vals = p.map(x => x.load)
  const vmax = Math.max(...vals, 1)
  const vmin = Math.min(...vals, 0)
  const pad = (vmax - vmin) * 0.1
  return {
    backgroundColor: 'transparent', textStyle: baseText,
    tooltip: { trigger: 'axis' },
    grid: baseGrid,
    xAxis: { type: 'category', data: p.map(x => x.time.slice(11, 16)), boundaryGap: false, ...axis() },
    yAxis: { type: 'value', min: Math.max(0, vmin - pad), max: vmax + pad, name: 'kWh', ...axis() },
    series: [{
      name: '预测负荷', type: 'line', smooth: true, symbolSize: 5,
      data: p.map(x => x.load),
      lineStyle: { color: '#34d399', width: 2.4, shadowColor: 'rgba(52,211,153,0.5)', shadowBlur: 8 },
      itemStyle: { color: '#34d399' },
      areaStyle: { color: { type: 'linear', x: 0, y: 0, x2: 0, y2: 1,
                             colorStops: [{ offset: 0, color: 'rgba(52,211,153,0.4)' }, { offset: 1, color: 'rgba(52,211,153,0.03)' }] } },
      markPoint: {
        data: [{ type: 'max', name: '峰值', symbolSize: 46, label: { color: '#fff' }, itemStyle: { color: '#fb923c' } }]
      }
    }]
  }
})

async function loadRangeData() {
  const d = rangeDays.value
  const [rev, vol, ses, h, wd, hm, rank, um, dop, wea] = await Promise.all([
    api.revenueTrend(d), api.volumeTrend(d), api.sessionTrend(d),
    api.hourly(d), api.weekdayPattern(d), api.heatmap(d),
    api.rankings(d), api.userMetrics(d), api.deviceOps(d), api.weatherImpact(d)
  ])
  revenuePts.value = rev.points
  volumePts.value = vol.points
  sessionPts.value = ses.points
  hourly.value = h
  weekday.value = wd
  heatmap.value = hm
  ranking.value = rank
  userM.value = um
  deviceOps.value = dop
  weather.value = wea
}

async function setRange(d) {
  rangeDays.value = d
  try {
    await loadRangeData()
  } catch (e) {
    console.warn('时间范围切换失败', e.message)
  }
}

async function refresh() {
  try {
    updatedAt.value = '加载中...'
    const [o, c, st] = await Promise.all([
      api.overview(), api.compatStats(7), api.stations()
    ])
    ov.value = o; compat.value = c
    stations.value = st
    await loadRangeData()
    updatedAt.value = new Date().toLocaleTimeString('zh-CN')
    qualityText.value = '数据快照已连接'
  } catch (e) {
    updatedAt.value = '数据获取失败：' + e.message
    qualityText.value = '数据连接异常'
  }
  try {
    const resp = await api.forecast24h()
    const byHour = new Map()
    for (const row of resp.data || []) {
      const t = String(row.datetime).slice(0, 13) + ':00:00'
      byHour.set(t, (byHour.get(t) || 0) + Number(row.predicted_load_kwh || 0))
    }
    predictPts.value = [...byHour.entries()].sort().map(([time, load]) => ({ time, load }))
    let peak = null
    for (const p of predictPts.value) if (!peak || p.load > peak.load) peak = p
    peakText.value = peak ? peak.time.slice(11, 16) + ' 时' : '--'
    modelInfo.value = resp.model ? `模型：${resp.model}` : '未加载模型'
    modelInfo2.value = resp.isFallback ? '当前为持久化回退预测（未加载已训练模型）' : '机器学习模型预测'
    modelQuality.value = resp.isFallback ? '预测模型：持久化回退' : `预测模型：${resp.model}`
  } catch (e) {
    predictPts.value = []
    modelInfo.value = '预测接口不可用：' + e.message
    modelQuality.value = '预测接口不可用'
  }
}

let timer = null
onMounted(() => { refresh(); timer = setInterval(refresh, 15000) })
onBeforeUnmount(() => clearInterval(timer))
</script>

<style scoped>
:global(body) { margin: 0; color: #eaf2ff; min-width: 320px;
  background: radial-gradient(circle at 15% 0%, rgba(53, 215, 255, .10), transparent 30%),
    radial-gradient(circle at 90% 8%, rgba(79, 124, 255, .12), transparent 28%),
    #07111f;
  font-family: "Microsoft YaHei", "PingFang SC", system-ui, sans-serif; }
.screen { min-height: 100vh; }

header {
  position: sticky; z-index: 10; top: 0; display: flex; align-items: center;
  justify-content: space-between; gap: 20px; padding: 14px max(24px, calc((100vw - 1500px) / 2));
  background: rgba(7, 17, 31, .90); border-bottom: 1px solid rgba(53, 215, 255, .18); backdrop-filter: blur(16px);
}
.brand { display: flex; align-items: center; gap: 12px; }
.brand-mark {
  width: 38px; height: 38px; border-radius: 12px; display: grid; place-items: center;
  color: #07111f; font-size: 20px; background: linear-gradient(135deg, #35d7ff, #34d399);
  box-shadow: 0 0 28px rgba(53, 215, 255, .25);
}
h1 { margin: 0; font-size: 18px; letter-spacing: 1px; }
.brand small { display: block; margin-top: 3px; color: #8da0bd; font-size: 11px; }
.header-actions { display: flex; align-items: center; gap: 12px; color: #8da0bd; font-size: 12px; }
.status-dot { width: 7px; height: 7px; border-radius: 50%; background: #34d399; box-shadow: 0 0 12px #34d399; }
.refresh-meta { color: #b6c7de; font-variant-numeric: tabular-nums; }
button { border: 1px solid #2a4568; border-radius: 8px; padding: 7px 12px; color: #cfe0fa; background: #12243c; cursor: pointer; }
button:hover { border-color: #35d7ff; color: white; }

main { width: min(1540px, calc(100% - 40px)); margin: 0 auto; padding: 26px 0 34px; }
.hero { display: flex; align-items: end; justify-content: space-between; gap: 20px; margin-bottom: 18px; }
.eyebrow { margin-bottom: 7px; color: #35d7ff; font-size: 12px; letter-spacing: 2px; }
h2 { margin: 0; font-size: clamp(23px, 3vw, 34px); }
.hero p { margin: 8px 0 0; color: #8da0bd; font-size: 13px; }
.quality-badge { flex: 0 0 auto; padding: 9px 13px; border: 1px solid rgba(52, 211, 153, .32); border-radius: 999px; color: #9df3ce; background: rgba(52, 211, 153, .08); font-size: 12px; }

.kpi-grid { display: grid; grid-template-columns: repeat(5, minmax(0, 1fr)); gap: 14px; }
.kpi-card, .panel, .summary-item {
  background: linear-gradient(145deg, rgba(16, 31, 53, .96), rgba(10, 23, 40, .96));
  border: 1px solid #20334f; box-shadow: 0 16px 36px rgba(0, 0, 0, .16);
}
.kpi-card { position: relative; min-height: 122px; padding: 18px; border-radius: 13px; overflow: hidden; }
.kpi-card::after { content: ""; position: absolute; right: -18px; bottom: -42px; width: 105px; height: 105px; border-radius: 50%; background: rgba(53, 215, 255, .06); }
.kpi-label { color: #8da0bd; font-size: 12px; }
.kpi-value { margin-top: 14px; font-size: clamp(23px, 2.3vw, 33px); font-weight: 750; white-space: nowrap; }
.kpi-unit { margin-left: 4px; color: #afc4df; font-size: 12px; font-weight: 400; }

.summary-strip { display: grid; grid-template-columns: repeat(4, 1fr); gap: 1px; margin: 14px 0 26px; overflow: hidden; border: 1px solid #20334f; border-radius: 12px; background: #20334f; }
.summary-item { border: 0; padding: 13px 16px; box-shadow: none; }
.summary-item span { color: #8da0bd; font-size: 12px; }
.summary-item strong { float: right; color: #ddecff; font-size: 16px; }

.section-head { display: flex; align-items: baseline; gap: 10px; margin: 30px 2px 14px; }
.section-head h3 { display: inline; margin: 0; font-size: 17px; }
.section-desc { color: #8da0bd; font-size: 12px; }
.section-number { color: #35d7ff; font: 700 12px ui-monospace, monospace; }
.range-switch { display: flex; align-items: center; gap: 4px; margin-left: auto; flex-wrap: wrap; justify-content: flex-end; }
.range-switch button { padding: 4px 9px; border-radius: 999px; border-color: #20334f; background: rgba(7, 17, 31, .65); color: #8da0bd; font-size: 11px; }
.range-switch button:hover { border-color: #35d7ff; color: #cfe0fa; }
.range-switch button.active { color: #07111f; background: linear-gradient(135deg, #35d7ff, #4f7cff); border-color: transparent; font-weight: 700; }

.grid-3 { display: grid; grid-template-columns: repeat(3, minmax(0, 1fr)); gap: 16px; }
.grid-2 { display: grid; grid-template-columns: repeat(2, minmax(0, 1fr)); gap: 16px; }
.panel { min-width: 0; padding: 14px 16px 16px; border-radius: 13px; }
.panel-title { margin: 1px 4px 2px; color: #c9d9ef; font-size: 13px; font-weight: 600; }
.panel-note { margin: 8px 4px 0; color: #8da0bd; font-size: 11px; }
.chart { width: 100%; height: 278px; margin-top: 6px; }
.chart-tall { height: 330px; }

.table-wrap { width: 100%; overflow-x: auto; margin-top: 10px; }
table { width: 100%; border-collapse: collapse; font-size: 12px; }
th, td { padding: 10px 9px; border-bottom: 1px solid rgba(32, 51, 79, .72); text-align: right; white-space: nowrap; }
th { position: sticky; top: 0; color: #7890b0; background: #0f1d31; font-weight: 500; }
th:first-child, td:first-child { text-align: left; }
tbody tr:hover { background: rgba(53, 215, 255, .04); }

.rank { display: inline-grid; width: 21px; height: 21px; margin-right: 8px; place-items: center; border-radius: 6px; color: #35d7ff; background: rgba(53, 215, 255, .09); font-size: 10px; }
.meter { display: inline-block; width: 64px; height: 5px; margin-right: 7px; overflow: hidden; vertical-align: middle; border-radius: 8px; background: #182b45; }
.meter i { display: block; height: 100%; border-radius: inherit; background: linear-gradient(90deg, #4f7cff, #35d7ff); }
.tag { display: inline-block; padding: 3px 7px; border-radius: 999px; font-size: 10px; }
.tag-online { color: #8df0c4; background: rgba(52, 211, 153, .12); }
.tag-fault { color: #ff9ca6; background: rgba(255, 107, 122, .13); }

.operation-cards { display: grid; grid-template-columns: repeat(3, 1fr); gap: 10px; margin: 12px 4px 2px; }
.mini-card { padding: 12px; border-radius: 10px; background: rgba(7, 17, 31, .55); border: 1px solid rgba(32, 51, 79, .72); }
.mini-card span { display: block; color: #8da0bd; font-size: 10px; }
.mini-card strong { display: block; margin-top: 7px; color: #ddecff; font-size: 18px; white-space: nowrap; }
.train-btn { margin: 10px 4px 0; width: calc(100% - 8px); padding: 9px; border-radius: 8px; }

footer { padding: 16px 0 28px; text-align: center; color: #5f7392; font-size: 11px; }

@media (max-width: 1200px) { .kpi-grid { grid-template-columns: repeat(3, 1fr); } .grid-3, .grid-2 { grid-template-columns: 1fr; } }
@media (max-width: 700px) {
  header { padding: 12px 16px; } .brand small, .header-actions span:not(.status-dot) { display: none; }
  main { width: min(100% - 24px, 1500px); padding-top: 18px; } .hero { align-items: start; flex-direction: column; }
  .kpi-grid { grid-template-columns: repeat(2, 1fr); } .kpi-card { min-height: 100px; padding: 14px; }
  .summary-strip { grid-template-columns: repeat(2, 1fr); }
}
</style>
