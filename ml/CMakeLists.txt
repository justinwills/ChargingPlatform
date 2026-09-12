# ml/CMakeLists.txt
#
# Plain Python (feature engineering, model training/forecasting/evaluation,
# recommendation logic, FastAPI routes) — CMake does not compile any of it.
# This file exists only so these files show up in the project tree of
# CMake-aware IDEs (Qt Creator, CLion, VS Code CMake Tools).
# `add_custom_target(... SOURCES ...)` with no COMMAND creates no build step.

add_custom_target(ml_sources SOURCES
    __init__.py
    api_routes.py
    features.py
    split.py
    train_baseline.py
    train_models.py
    forecast.py
    peak_analysis.py
    evaluate.py
    infer.py
    qt_integration.md
    recommend/__init__.py
    recommend/scoring.py
    recommend/peak_alert.py
    recommend/ops_advice.py
)
