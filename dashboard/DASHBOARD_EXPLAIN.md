# 仪表盘图表说明 / Penjelasan Chart Dashboard

版本 / Versi: 13 chart, 10 section. Data dari snapshot Spark: `dashboard/data/bigdata.json`.

---

## 三个"蓝色"图表 / Tiga Chart "Biru" (Scatter + Bar + Heatmap)

### 1. 设备额定功率与平均输出功率（散点图 / Scatter）— Section 05
**Fungsi:** Setiap titik = satu perangkat charging. Sumbu X = daya terpasang (rated power, kW), sumbu Y = daya rata-rata aktual saat charging (avg output power). Diameter titik = jumlah sesi charging (makin besar, makin sering dipakai). Titik merah = perangkat dalam status fault.

**Kenapa biru / sebagian tak biru:** Warna biru (`#35d7ff`) adalah pilihan visual untuk menandai perangkat normal/online — *bukan* representasi data. Titik diwarnai berdasarkan status perangkat, bukan nilai datanya; semua perangkat normal jadi biru, perangkat fault diwarnai merah agar langsung terlihat. Area kosong/bukan-biru di kanvas hanyalah latar belakang gelap tema, artinya tidak ada perangkat pada kombinasi daya tersebut.

---

### 2. 充电站排名 Top 10（柱状图 / Bar）— Section 08
**Fungsi:** Menampilkan 10 stasiun teratas berdasarkan metrik yang dipilih di dropdown (充电次数 / jumlah sesi, 充电量 / kWh, 充电费用 / biaya). Panjang bar = nilai metrik; membantu membandingkan stasiun mana yang paling sibuk / paling banyak menghasilkan.

**Kenapa biru / sebagian tak biru:** Bar menggunakan warna `#a78bfa`-ish yang khas per panel agar beda dari bar biru di panel lain. Warna seragam untuk semua bar — hal itu memang disengaja: yang membedakan bar adalah **panjangnya**, bukan warnanya, sehingga "bagian tidak biru" = grafik kosong/kecil di metrik tertentu, bukan data hilang.

---

### 3. 星期 × 小时活跃热力图（Heatmap）— Section 08
**Fungsi:** Grid aktivitas: baris = hari dalam seminggu (Senin–Minggu), kolom = jam (0–23), warna = jumlah sesi charging. Blok lebih terang → jam itu lebih ramai. Menjawab "kapan pengguna paling sering ngecharge".

**Kenapa biru / sebagian tak biru:** Heatmap memakai *gradasi* warna (blau gelap → terang) untuk menyatakan intensitas, dengan `visualMap`. Bagian yang tidak biru/hampir gelap = jam dengan sedikit atau nol sesi (misal dini hari). Jadi biru-nya = skala intensitas data, bukan status; itu bedanya dengan scatter (warna = status).

---

## Slide/Layout 一列两栏（Section 02/03/06/08/09: grid-wide）
Layout dua kolom (kiri lebar `1.2fr`, kanan `0.8fr`): panel kiri = chart utama (chart panjang/tall), panel kanan = tabel pendukung/metrik. Di layar sempit (≤1050px) otomatis menumpuk jadi satu kolom. Tujuan: chart besar tetap mudah dibaca sambil tabel penunjang tersedia tanpa scroll horizontal.

---

## 分页改动 (Paging)
- 全部充电站清单 (Section 09) dan 图表筛选 (Section 10) sekarang **10 baris / halaman** dengan navigasi 上一页/下一页 + nomor halaman.
- Data tetap dimuat semua dari JSON; hanya *tampilan* yang dipaginasi sehingga tidak perlu scroll panjang.
- Saat mencari stasiun / memilih stasiun lain, halaman otomatis reset ke 1.

---

## 中文摘要

三个带蓝色的图分别是：**(1) 设备额定功率–平均输出功率散点图**（点=设备，直径=充电次数，蓝=正常设备，红=故障）；**(2) 充电站排名柱状图**（可切换按次数/电量/费用，颜色为统一面板色，区别靠柱长）；**(3) 星期×小时热力图**（颜色深浅表示该时段充电量，暗色=低峰时段）。热力图的蓝色是强度渐变（数据），散点图的蓝色是设备状态（分类），柱状图颜色仅为样式统一。大盘布局为两栏（左图右表），窄屏自动变单栏。分页：两个长列表（全站清单、筛选设备）现改为每页 10 条，带翻页按钮。