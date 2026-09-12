# ChargingPlatform — Phase 2 (大数据可视化大屏 + 机器学习智能分析 + 智能推荐)

Phase 2 lives as two plain top-level folders next to `client/`, `server/`,
`db/`, `protocol/`. No CMake involved — this is a separate Python
service (FastAPI + pandas/scikit-learn/XGBoost) that talks to the existing
C++ `ChargingServer` only through the HTTP API it exposes and through
`dashboard/dashboard.html`, which `ChargingServer` already serves.

```
ChargingPlatform/
├── bigdata/                  大数据可视化大屏（Web端）  (王清香/薛学刚/洪维斌/邱辰笙)
│   ├── etl/                  数据导入 + 数据预处理
│   │   ├── load_data.py
│   │   └── clean_charging.py
│   ├── analytics/            数据统计 + 电池数据分析
│   │   ├── charging_stats.py
│   │   ├── battery_stats.py
│   │   └── station_ranking.py
│   └── api_routes.py         /api/stats/* endpoints for the dashboard
├── ml/                        机器学习智能分析 + 智能推荐  (王清香/薛学刚/邱辰笙/特布新)
│   ├── features.py           特征选择 + 时间特征 + 历史负荷 + 滞后特征
│   ├── split.py               训练/验证/测试集划分
│   ├── train_baseline.py      线性回归基线
│   ├── train_models.py        Random Forest / XGBoost 训练 + 模型保存
│   ├── forecast.py            1h / 6h / 24h / 分站点 预测
│   ├── peak_analysis.py       高峰时段识别
│   ├── evaluate.py            MAE / RMSE / MAPE / R² / 模型比较
│   ├── infer.py               推理入口
│   ├── model_registry/        保存的模型文件（gitignored）
│   ├── recommend/             智能推荐 + 运营预警
│   │   ├── scoring.py
│   │   ├── peak_alert.py
│   │   └── ops_advice.py
│   ├── api_routes.py          /api/predict/* 和 /api/recommend/* endpoints
│   └── qt_integration.md      把推荐结果接入 Phase 1 Qt 客户端的说明
├── data/                     shared by bigdata/ and ml/
│   ├── raw/                  put nvv2t.csv, nvv2t_md_end.csv, dsv13r2.csv here (gitignored)
│   └── processed/            cleaned output (gitignored)
├── dashboard/dashboard.html   already annotated with Phase 2 chart TODOs
├── tests/test_phase2_integration.py   端到端联调测试 (王清香、薛学刚)
├── run_api.py                 FastAPI entrypoint: wires bigdata + ml routes together
└── requirements.txt
```

Every `.py` file has one stub function per task, each with a docstring
carrying the **task number, owner, and original task description**, plus a
`raise NotImplementedError(...)` marking where to start. Search for
`# TODO: implement` to find every open item.

## Getting started

```bash
python -m venv .venv
source .venv/bin/activate        # or .venv\Scripts\activate on Windows
pip install -r requirements.txt

# 1. copy the three teacher-provided CSVs into data/raw/ (see data/raw/README.md)
# 2. implement bigdata/etl/load_data.py + bigdata/etl/clean_charging.py, then
# 3. run the API (routes 501 until their functions are implemented):
uvicorn run_api:app --reload --port 8090
```

## Task → owner → file map

| # | Subsystem | Task | Owner | File |
|---|---|---|---|---|
| 61 | 大数据可视化大屏（Web端） | 数据导入 / 充电订单数据导入 | 王清香 | `bigdata/etl/load_data.py` |
| 62 | 大数据可视化大屏（Web端） | 数据导入 / 充电站数据导入 | 薛学刚 | `bigdata/etl/load_data.py` |
| 63 | 大数据可视化大屏（Web端） | 数据导入 / 电池运行数据导入 | 薛学刚 | `bigdata/etl/load_data.py` |
| 64 | 大数据可视化大屏（Web端） | 数据预处理 / 充电时间处理 | 洪维斌 | `bigdata/etl/clean_charging.py` |
| 65 | 大数据可视化大屏（Web端） | 数据预处理 / 充电数据清洗 | 洪维斌 | `bigdata/etl/clean_charging.py` |
| 66 | 大数据可视化大屏（Web端） | 数据统计 / 充电业务指标 | 邱辰笙 | `bigdata/analytics/charging_stats.py` |
| 67 | 大数据可视化大屏（Web端） | 数据统计 / 充电站指标 | 邱辰笙 | `bigdata/analytics/charging_stats.py` |
| 68 | 大数据可视化大屏（Web端） | 数据统计 / 用户指标 | 邱辰笙 | `bigdata/analytics/charging_stats.py` |
| 69 | 大数据可视化大屏（Web端） | 数据统计 / 星期充电规律 | 邱辰笙 | `bigdata/analytics/charging_stats.py` |
| 70 | 大数据可视化大屏（Web端） | 数据可视化 / 充电量趋势图 | 邱辰笙 | `bigdata/api_routes.py` |
| 71 | 大数据可视化大屏（Web端） | 数据可视化 / 营收趋势图 | 邱辰笙 | `bigdata/api_routes.py` |
| 72 | 大数据可视化大屏（Web端） | 数据可视化 / 充电次数趋势图 | 洪维斌 | `bigdata/api_routes.py` |
| 73 | 大数据可视化大屏（Web端） | 数据可视化 / 充电站排名 | 洪维斌 | `bigdata/analytics/station_ranking.py` |
| 74 | 大数据可视化大屏（Web端） | 数据可视化 / 充电时段分析 | 洪维斌 | `bigdata/analytics/charging_stats.py` |
| 75 | 大数据可视化大屏（Web端） | 数据可视化 / 星期热力图 | 洪维斌 | `bigdata/analytics/charging_stats.py` |
| 76 | 大数据可视化大屏（Web端） | 数据可视化 / 充电站分布展示 | 洪维斌 | `bigdata/api_routes.py` |
| 77 | 大数据可视化大屏（Web端） | 电池数据分析 / SOC分析 | 邱辰笙 | `bigdata/analytics/battery_stats.py` |
| 78 | 大数据可视化大屏（Web端） | 电池数据分析 / 温度分析 | 邱辰笙 | `bigdata/analytics/battery_stats.py` |
| 79 | 大数据可视化大屏（Web端） | 电池数据分析 / 电压电流分析 | 邱辰笙 | `bigdata/analytics/battery_stats.py` |
| 80 | 大数据可视化大屏（Web端） | 电池数据分析 / 电池能量分析 | 邱辰笙 | `bigdata/analytics/battery_stats.py` |
| 81 | 大数据可视化大屏（Web端） | 页面交互 / 图表筛选 | 洪维斌 | `bigdata/api_routes.py` |
| 82 | 大数据可视化大屏（Web端） | 页面交互 / 数据自动刷新 | 特布新 | `dashboard/dashboard.html` |
| 83 | 机器学习智能分析子系统 | 数据准备 / 特征选择 | 王清香 | `ml/features.py` |
| 84 | 机器学习智能分析子系统 | 数据准备 / 时间特征工程 | 王清香 | `ml/features.py` |
| 85 | 机器学习智能分析子系统 | 数据准备 / 历史负荷构造 | 薛学刚 | `ml/features.py` |
| 86 | 机器学习智能分析子系统 | 数据准备 / 滞后特征构造 | 薛学刚 | `ml/features.py` |
| 87 | 机器学习智能分析子系统 | 数据准备 / 训练数据划分 | 薛学刚 | `ml/split.py` |
| 88 | 机器学习智能分析子系统 | 充电负荷预测 / 基线模型 | 薛学刚 | `ml/train_baseline.py` |
| 89 | 机器学习智能分析子系统 | 充电负荷预测 / 机器学习模型 | 薛学刚 | `ml/train_models.py` |
| 90 | 机器学习智能分析子系统 | 充电负荷预测 / 1小时预测 | 薛学刚 | `ml/forecast.py` |
| 91 | 机器学习智能分析子系统 | 充电负荷预测 / 6小时预测 | 薛学刚 | `ml/forecast.py` |
| 92 | 机器学习智能分析子系统 | 充电负荷预测 / 24小时预测 | 薛学刚 | `ml/forecast.py` |
| 93 | 机器学习智能分析子系统 | 站点预测 / 站点负荷预测 | 王清香 | `ml/forecast.py` |
| 94 | 机器学习智能分析子系统 | 高峰分析 / 高峰时段识别 | 王清香 | `ml/peak_analysis.py` |
| 95 | 机器学习智能分析子系统 | 模型评估 / MAE计算 | 王清香 | `ml/evaluate.py` |
| 96 | 机器学习智能分析子系统 | 模型评估 / RMSE计算 | 王清香 | `ml/evaluate.py` |
| 97 | 机器学习智能分析子系统 | 模型评估 / MAPE计算 | 王清香 | `ml/evaluate.py` |
| 98 | 机器学习智能分析子系统 | 模型评估 / R²计算 | 王清香 | `ml/evaluate.py` |
| 99 | 机器学习智能分析子系统 | 模型评估 / 模型比较 | 王清香 | `ml/evaluate.py` |
| 100 | 机器学习智能分析子系统 | 模型部署 / 模型保存 | 邱辰笙 | `ml/train_models.py` |
| 101 | 机器学习智能分析子系统 | 模型部署 / 模型推理 | 邱辰笙 | `ml/infer.py` |
| 102 | 智能推荐与系统集成 | 智能推荐 / 站点推荐评分 | 王清香 | `ml/recommend/scoring.py` |
| 103 | 智能推荐与系统集成 | 智能推荐 / 低负荷站点推荐 | 王清香 | `ml/recommend/scoring.py` |
| 104 | 智能推荐与系统集成 | 智能推荐 / 高峰站点识别 | 王清香 | `ml/recommend/peak_alert.py` |
| 105 | 智能推荐与系统集成 | 运营预警 / 负荷预警 | 特布新 | `ml/recommend/peak_alert.py` |
| 106 | 智能推荐与系统集成 | 运营分析 / 运维建议 | 特布新 | `ml/recommend/ops_advice.py` |
| 107 | 智能推荐与系统集成 | 后端接口 / 数据接口 | 邱辰笙 | `bigdata/api_routes.py` |
| 108 | 智能推荐与系统集成 | 后端接口 / 预测接口 | 邱辰笙 | `ml/api_routes.py` |
| 109 | 智能推荐与系统集成 | 后端接口 / 推荐接口 | 邱辰笙 | `ml/api_routes.py` |
| 110 | 智能推荐与系统集成 | 系统集成 / Web联调 | 王清香 | `dashboard/dashboard.html` |
| 111 | 智能推荐与系统集成 | 系统集成 / Qt联调 | 王清香 | `ml/qt_integration.md` |
| 112 | 智能推荐与系统集成 | 系统测试 / 功能测试 | 王清香、薛学刚 | `tests/test_phase2_integration.py` |

## Suggested build order

1. **bigdata/etl/** — get the three CSVs loading and cleaned (#61–65).
2. **bigdata/analytics/** — the stats the dashboard needs first (#66–69, 73–80).
3. **dashboard/dashboard.html** + **bigdata/api_routes.py** — wire real
   charts to real data (#70–72, 74–76, 81–82, 107).
4. **ml/** — features → split → baseline → RF/XGBoost → forecast →
   evaluate → save (#83–101). Longest chain; start early, in parallel with 3.
5. **ml/recommend/** + **ml/api_routes.py** (#102–109).
6. **ml/qt_integration.md** — hook the Qt client's station list up to
   `/api/recommend/stations` (#111).
7. **tests/test_phase2_integration.py** — end-to-end pass across CSV →
   processing → model → API → dashboard → Qt (#112).
