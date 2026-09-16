# ChargingPlatform - Phase 2 task scaffold

Phase 2 is a separate Python service next to the C++ client/server project.
The Python service contains the big-data dashboard pipeline, machine-learning
forecasting, recommendations, and API integration. The data preparation
tasks (#92-#98) are implemented in `ml/features.py`.

## Updated Raw Data

Place these six files in `data/raw/`:

- `charging_orders.csv`
- `stations.csv`
- `devices.csv`
- `users.csv`
- `weather_hourly.csv`
- `device_status_log.csv`

The new order schema uses fields such as `session_id`, `user_id`, `station_id`,
`device_id`, `created_at`, `ended_at`, `energy_kwh`, and `fee_amount_cny`.
The related tables are joined through `user_id`, `station_id`, `device_id`,
`location_id`, and `weather_id`.

## Task Map

| Tasks | Area | File |
|---|---|---|
| 61-67 | Import six source tables and associate orders, users, stations, devices, and weather | `bigdata/etl/load_data.py` |
| 68-71 | Process order times, clean orders, process weather, and process device status | `bigdata/etl/clean_charging.py` |
| 72-79 | Business, station, user, device, weekday, holiday, weather, and device-runtime statistics | `bigdata/analytics/charging_stats.py` |
| 80-82 | Charging-volume, revenue, and session-count trend charts | `bigdata/api_routes.py` |
| 83 | Charging-station ranking | `bigdata/analytics/station_ranking.py` |
| 84-89 | Hourly, weekday, holiday, weather, station-distribution, and device-status visualizations | `bigdata/analytics/charging_stats.py`, `bigdata/api_routes.py` |
| 90-91 | Dashboard filtering and automatic refresh | `bigdata/api_routes.py`, `dashboard/dashboard.html` |
| 92-101 | Multi-source ML preparation, feature engineering, historical/lag loads, and time-based splitting | `ml/features.py`, `ml/split.py` |
| 102-103 | Baseline and machine-learning load models | `ml/train_baseline.py`, `ml/train_models.py` |
| 104-108 | 1-hour, 6-hour, 24-hour, station forecasts, and peak-period identification | `ml/forecast.py`, `ml/peak_analysis.py` |
| 109-113 | MAE, RMSE, MAPE, R2, and model comparison | `ml/evaluate.py` |
| 114-115 | Model saving and inference | `ml/train_models.py`, `ml/infer.py` |
| 116-120 | Recommendation scoring, low-load stations, peak alerts, load alerts, and operations advice | `ml/recommend/` |
| 121-123 | Statistics, prediction, and recommendation backend interfaces | `bigdata/api_routes.py`, `ml/api_routes.py` |
| 124-125 | Web and Qt integration | `dashboard/dashboard.html`, `ml/qt_integration.md` |
| 126 | End-to-end functional testing | `tests/test_phase2_integration.py` |

## Getting Started

```bash
python -m venv .venv
.venv\Scripts\activate
pip install -r requirements.txt

# Copy the six CSV files into data/raw/.
# Build the station-hour ML dataset:
spark-submit --master 'local[2]' ml/features.py \
  --input-root data/raw \
  --output data/processed/ml_training_dataset.csv

# The remaining task stubs belong to other task owners.
python run_api.py
```

The feature pipeline reads `charging_orders.csv`, `stations.csv`,
`devices.csv`, `users.csv`, `weather_hourly.csv`, and
`device_status_log.csv` through the existing PySpark ODS/DWD cleaners. It
joins weather by `weather_id`, aggregates device status by station, preserves
zero-demand station-hours, and excludes `weather_id`, `user_id`, `device_id`,
`fee_amount_cny`, and `ended_at` from model features.

Task #91 automatic refresh is implemented in `dashboard/dashboard.html`.
The page polls `/api/bigdata` every 60 seconds with cache disabled and updates
all dashboard charts from the latest `dashboard/data/bigdata.json` snapshot.
The existing manual refresh button still triggers the same refresh path.

The Spark CSV output is a directory named
`data/processed/ml_training_dataset.csv` containing one or more
`part-*.csv` files and a header file.
