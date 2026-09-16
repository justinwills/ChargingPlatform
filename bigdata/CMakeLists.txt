# The big-data subsystem is implemented in Python/PySpark, so CMake does not
# compile or run these files. This source-only target makes them visible in
# CMake-aware IDEs such as Qt Creator and CLion.

add_custom_target(bigdata_sources SOURCES
    README.md
    __init__.py
    charging_warehouse.py

    etl/__init__.py
    etl/schemas.py
    etl/ingest.py
    etl/load_data.py
    etl/clean_charging.py
    etl/clean_tables.py

    analytics/__init__.py
    analytics/charging_stats.py
    analytics/device_stats.py
    analytics/weather_stats.py

    tests/__init__.py
    tests/test_charging_pipeline.py

    ${CMAKE_SOURCE_DIR}/requirements.txt
)
