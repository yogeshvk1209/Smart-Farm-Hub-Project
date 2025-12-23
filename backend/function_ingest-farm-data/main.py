import functions_framework
from google.cloud import bigquery
import datetime

# CONFIG (Update if needed)
PROJECT_ID = "farm-hub"
DATASET_ID = "farm_telemetry"
TABLE_ID = "soil_readings"

client = bigquery.Client(project=PROJECT_ID)
table_ref = f"{PROJECT_ID}.{DATASET_ID}.{TABLE_ID}"

@functions_framework.http
def ingest_data(request):
    """
    Ingest IoT data via GET (Query Params) or POST (JSON).
    """
    # 1. Try to get JSON (for testing)
    request_data = request.get_json(silent=True)
    
    # 2. If no JSON, grab URL Query Params (For the Modem)
    if not request_data:
        request_data = request.args

    if not request_data:
        return 'Missing Payload', 400

    try:
        row_to_insert = [
            {
                "event_ts": datetime.datetime.utcnow().isoformat(),
                "device_id": request_data.get("device_id", "unknown"),
                # We cast to int/float because URL params are always strings
                "moisture_raw": int(request_data.get("raw", 0)),
                "moisture_pct": int(request_data.get("pct", 0)),
                "battery_volts": float(request_data.get("bat", 0.0)),
                "meta_data": "via_get"
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
