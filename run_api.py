"""
Phase 2 backend entrypoint (data dashboard + ML forecasting + recommendation).

[Task #107/#108/#109/#110] Exposes three groups of endpoints that the
Web dashboard (and, indirectly, the Phase 1 Qt client for recommendations)
consume:

  /api/stats/...      -> bigdata/api_routes.py  (owner: 邱辰笙)
  /api/predict/...    -> ml/api_routes.py       (owner: 邱辰笙)
  /api/recommend/...  -> ml/api_routes.py       (owner: 邱辰笙)

This is a separate Python service from the existing C++ ChargingServer
(which still serves the static dashboard/ folder and its own Phase 1
/api/stats on port 8080). Run this on a different port (8090 below) and
point new dashboard calls at it, or reverse-proxy both under one host
once the team decides on final deployment.

Run with:
    pip install -r requirements.txt
    uvicorn run_api:app --reload --port 8090
"""

from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware

from bigdata.api_routes import register_stats_routes
from ml.api_routes import register_predict_routes, register_recommend_routes

app = FastAPI(title="ChargingPlatform Phase 2 API")

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],  # TODO: tighten before demo/deployment
    allow_methods=["*"],
    allow_headers=["*"],
)

register_stats_routes(app)
register_predict_routes(app)
register_recommend_routes(app)


@app.get("/health")
def health():
    return {"status": "ok"}
