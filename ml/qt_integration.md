# Qt client integration (Task #111)

Owner: 王清香

Phase 1's station query in the Qt client (`client/mainwindow_stationdetail.cpp`
/ `client/mainwindow_chargeselectors.cpp`, and `server/dispatcher_stations.cpp`
on the backend) currently sorts nearby stations purely by distance.

Phase 2 goal: once `/api/recommend/stations` (see `ml/api_routes.py`,
Task #109) is live, have the station list also take the recommendation score
into account instead of distance alone.

## Suggested approach

1. Decide whether the C++ server (`server/dispatcher_stations.cpp`) calls the
   new Python recommendation API server-side (simplest for the Qt client —
   no protocol change needed on the client side), or whether the Qt client
   calls it directly over HTTP alongside the existing TCP protocol.
   - Server-side call keeps `protocol/protocolcodec.cpp` and the existing
     JSON action format untouched — just enrich the existing station list
     response with a `recommend_score` field before sending it back.
2. On the client side, `mainwindow_chargeselectors.cpp` builds the sorted
   list shown to the user — change or add a sort mode that uses
   `recommend_score` (from the payload) rather than (or blended with)
   distance.
3. Add a toggle or setting so distance-only sorting is still available for
   testing/comparison, per Task #112 (functional testing across the whole
   pipeline).

## Data contract (draft — confirm before implementing)

```json
{
  "stations": [
    {
      "station_id": "string, matches db/schema.sql stations table",
      "distance_km": 1.2,
      "recommend_score": 0.83,
      "predicted_load_1h": "kWh, from /api/predict/1h",
      "status": "low_load | normal | high_load"
    }
  ]
}
```

This is a starting point, not a finalized spec — align it with whatever
`ml/recommend/scoring.py` (Task #102) actually returns once implemented.
