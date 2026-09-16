<template>
  <div class="kpi-card">
    <div class="kpi-label">{{ label }}</div>
    <div class="kpi-num-row">
      <span v-if="prefix" class="kpi-prefix" :style="style">{{ prefix }}</span>
      <span class="kpi-num" :style="style">{{ numText }}</span>
      <span v-if="suffix" class="kpi-suffix" :style="style">{{ suffix }}</span>
    </div>
    <div class="kpi-sub">{{ sub }}</div>
  </div>
</template>

<script setup>
import { computed } from 'vue'

const props = defineProps({
  label: { type: String, required: true },
  value: { type: [Number, String], default: 0 },
  precision: { type: Number, default: 2 },
  prefix: { type: String, default: '' },
  suffix: { type: String, default: '' },
  sub: { type: String, default: '' },
  color: { type: String, default: '#35d7ff' }
})

const numText = computed(() => {
  const n = Number(props.value)
  if (!isFinite(n)) return '0'
  const p = Math.max(0, Math.min(4, Number(props.precision) || 0))
  return n.toLocaleString('en-US', { minimumFractionDigits: p, maximumFractionDigits: p })
})

const style = computed(() => ({ color: props.color }))
</script>

<style scoped>
.kpi-card { padding: 8px 16px; height: 100%; box-sizing: border-box; display: flex; flex-direction: column; justify-content: space-between; }
.kpi-label { font-size: 15px; color: #7c93bb; letter-spacing: 1px; }
.kpi-num-row { display: flex; align-items: baseline; gap: 4px; }
.kpi-prefix { font-size: 24px; font-weight: 700; }
.kpi-num { font-size: 36px; font-weight: 700; font-family: 'DIN', 'Consolas', monospace; line-height: 1.1; text-shadow: 0 0 16px currentColor; }
.kpi-suffix { font-size: 16px; font-weight: 600; }
.kpi-sub { font-size: 12px; color: #4c6086; }
</style>