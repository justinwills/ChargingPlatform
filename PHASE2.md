# ChargingPlatform - Phase 2 task scaffold

Phase 2 is a separate Python service next to the C++ client/server project.
The Python service contains the big-data dashboard pipeline, machine-learning
forecasting, recommendations, and API integration. This repository currently
keeps the task functions as stubs until they are implemented.

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
# Implement the stubs in task order.
uvicorn run_api:app --reload --port 8090
```

Search for `TODO: implement` or `NotImplementedError` to find the remaining
task scaffolds. The old `nvv2t.csv`, `nvv2t_md_end.csv`, and `dsv13r2.csv`
dataset descriptions are no longer part of the updated tasklist.
