"""
Phase 2 backend entrypoint (data dashboard + ML forecasting + recommendation).

[Task #121/#122/#123/#124] Exposes three groups of endpoints that the
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
    python run_api.py
"""

from flask import Flask
import os

from bigdata.api_routes import register_stats_routes
from ml.api_routes import register_predict_routes, register_recommend_routes

app = Flask(__name__)

register_stats_routes(app)
register_predict_routes(app)
register_recommend_routes(app)


@app.get("/health")
def health():
    return {"status": "ok"}


@app.after_request
def add_cors_headers(response):
    """Allow the separately hosted Vite dashboard to call the API."""
    response.headers["Access-Control-Allow-Origin"] = "*"
    response.headers["Access-Control-Allow-Methods"] = "GET, OPTIONS"
    response.headers["Access-Control-Allow-Headers"] = "Content-Type"
    return response


if __name__ == "__main__":
    app.run(
        host=os.environ.get("CHARGING_API_HOST", "0.0.0.0"),
        port=int(os.environ.get("CHARGING_API_PORT", "8090")),
        debug=os.environ.get("CHARGING_API_DEBUG", "0") == "1",
    )
