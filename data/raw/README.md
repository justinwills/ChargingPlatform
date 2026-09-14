Put the six teacher-provided datasets here:

- `charging_orders.csv` - charging sessions, users, stations, devices, time features, energy, fees, weekdays, and holidays
- `stations.csv` - station IDs, names, addresses, coordinates, device counts, opening times, prices, and status
- `devices.csv` - device IDs, station IDs, charger types, rated power, connectors, manufacturers, and current status
- `users.csv` - user IDs, registration time, city, membership, vehicle type, battery capacity, platform, and status
- `weather_hourly.csv` - hourly weather type, temperature, humidity, precipitation, wind, visibility, and rain status
- `device_status_log.csv` - device availability, faults, successful/failed sessions, output power, health, and maintenance status

The ETL task scaffold in `bigdata/etl/load_data.py` uses these filenames as
the default raw-data paths.
