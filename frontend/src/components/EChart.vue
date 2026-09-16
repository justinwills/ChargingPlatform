<template>
  <div ref="el" :style="{ width: '100%', height: height }"></div>
</template>

<script setup>
import * as echarts from 'echarts'
import { onBeforeUnmount, onMounted, ref, watch } from 'vue'

const props = defineProps({
  option: { type: Object, required: true },
  height: { type: String, default: '220px' }
})

const el = ref(null)
let chart = null
let ro = null

function render() {
  if (!chart) return
  try {
    chart.setOption(props.option || {}, true)
  } catch (e) {
    console.error('[EChart] setOption failed:', e)
  }
}

function resize() {
  try { chart && chart.resize() } catch (e) { console.error('[EChart] resize failed:', e) }
}

onMounted(() => {
  try {
    chart = echarts.init(el.value)
  } catch (e) {
    console.error('[EChart] init failed:', e)
    return
  }
  render()
  window.addEventListener('resize', resize)
  if (typeof ResizeObserver !== 'undefined') {
    ro = new ResizeObserver(() => resize())
    ro.observe(el.value)
  }
})

watch(() => props.option, render, { deep: true })

onBeforeUnmount(() => {
  if (ro) ro.disconnect()
  window.removeEventListener('resize', resize)
  chart && chart.dispose()
})

defineExpose({ resize })
</script>