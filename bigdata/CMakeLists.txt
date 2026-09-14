# bigdata/CMakeLists.txt
#
# This is plain Python (ETL + analytics + FastAPI routes for the Web
# dashboard) — CMake does not compile any of it. This file exists only so
# these files show up in the project tree of CMake-aware IDEs (Qt Creator,
# CLion, VS Code CMake Tools). `add_custom_target(... SOURCES ...)` with no
# COMMAND creates no build step.

add_custom_target(bigdata_sources SOURCES
    __init__.py
    api_routes.py
    etl/__init__.py
    etl/load_data.py
    etl/clean_charging.py
    analytics/__init__.py
    analytics/charging_stats.py
    analytics/battery_stats.py
    analytics/station_ranking.py
)
