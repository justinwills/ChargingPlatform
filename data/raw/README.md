Put the teacher-provided datasets here (not included in this scaffold — copy them from the course materials):

- `nvv2t.csv` — charging session data (session id, start/end time, kWh, fee, station, user)
- `nvv2t_md_end.csv` — charging station metadata (station id, name, address, pile count, opening hours)
- `dsv13r2.csv` — battery telemetry (SOC, pack voltage, charge current, temperature, available energy/capacity)

`etl/load_data.py` expects these three files at these paths by default.
