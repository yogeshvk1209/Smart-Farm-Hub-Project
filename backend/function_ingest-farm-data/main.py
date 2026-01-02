import functions_framework
from google.cloud import bigquery
import datetime

# --- CONFIG ---
# CHANGE THIS to something complex!
#  update your Secrete here
#API_SECRET = "FARM_SECRET"

# CONFIG (Update if needed)
PROJECT_ID = "farm-hub-482111"
DATASET_ID = "farm_telemetry"
TABLE_ID = "soil_readings"

client = bigquery.Client(project=PROJECT_ID)
table_ref = f"{PROJECT_ID}.{DATASET_ID}.{TABLE_ID}"

@functions_framework.http
def ingest_data(request):
    # 1. SECURITY CHECK
    # We look for a query param '?token=...'
    request_args = request.args
    token = request_args.get('token')
    
    if token != API_SECRET:
        print(f"Unauthorized access attempt. Token: {token}")
        return 'Unauthorized', 401

    # 2. Extract Data (Same as before)
    try:
        row_to_insert = [
            {
                "event_ts": datetime.datetime.utcnow().isoformat(),
                "device_id": request_args.get("device_id", "unknown"),
                "moisture_raw": int(request_args.get("raw", 0)),
                "moisture_pct": int(request_args.get("pct", 0)),
                "battery_volts": float(request_args.get("bat", 0.0)),
                "meta_data": "via_secure_get"
            }
        ]
        
        errors = client.insert_rows_json(table_ref, row_to_insert)
        if errors == []:
            return 'Success', 200
        else:
            print(f"BQ Errors: {errors}")
            return f"Error: {errors}", 500
            
    except Exception as e:
        return f"Error parsing data: {str(e)}", 400
