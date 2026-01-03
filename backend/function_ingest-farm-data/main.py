import functions_framework
from datetime import datetime
import os
from flask import jsonify
from google.cloud import storage
from google.cloud import bigquery

# --- CONFIGURATION ---
# 1. BigQuery Config
# Project ID comes from your logs: farm-hub-482111
BQ_TABLE_ID = "farm-hub-482111.farm_telemetry.soil_readings"

# 2. GCS Config
BUCKET_NAME = "farm-images-archive" 
EXPECTED_API_KEY = "FARM_SEC" 

@functions_framework.http
def ingest_data(request):
    """
    Unified Entry Point:
    - GET: Handles Sensor Telemetry
    - POST: Handles Image Uploads
    """
    
    # 1. SECURITY: Check Token
    token = request.args.get('token') or request.headers.get('X-Api-Key')
    
    if token != EXPECTED_API_KEY:
        print(f"!! Unauthorized Access Attempt. Token: {token}")
        return jsonify({"error": "Unauthorized"}), 401

    # 2. ROUTING
    if request.method == 'GET':
        return handle_telemetry(request)
    elif request.method == 'POST':
        return handle_image_upload(request)
    else:
        return jsonify({"error": "Method not allowed"}), 405

def handle_telemetry(request):
    """
    Logs sensor data and inserts into BigQuery using your Schema.
    """
    try:
        # 1. Extract params from URL
        device_id = request.args.get('device_id')
        raw = request.args.get('raw')
        pct = request.args.get('pct')
        bat = request.args.get('bat')
        
        # --- FIX IS HERE ---
        # Use datetime.now() directly because of 'from datetime import datetime'
        event_ts = datetime.utcnow().isoformat()

        print(f"TELEMETRY: Device={device_id}, Soil={pct}%, Bat={bat}V")

        # 3. Insert into BigQuery
        client = bigquery.Client()
        
        rows_to_insert = [
            {
                "event_ts": event_ts,
                "device_id": str(device_id),
                "moisture_raw": int(raw) if raw else 0,
                "moisture_pct": int(pct) if pct else 0,
                "battery_volts": float(bat) if bat else 0.0,
                "meta_data": "Ingested via Cloud Run Proxy" 
            }
        ]

        errors = client.insert_rows_json(BQ_TABLE_ID, rows_to_insert)

        if errors == []:
            print(">> SUCCESS: Telemetry inserted into BigQuery.")
            return jsonify({"status": "success", "type": "telemetry"}), 200
        else:
            print(f"!! BQ INSERT ERROR: {errors}")
            return jsonify({"error": str(errors)}), 500

    except Exception as e:
        print(f"Telemetry Exception: {e}")
        return jsonify({"error": str(e)}), 500
        
def handle_image_upload(request):
    """
    Streams image file to GCS with organized folders: uploads/YYYY/MM/DD/HH-MM-SS.jpg
    """
    try:
        if 'image' not in request.files:
            print("Error: No image in request.files")
            return jsonify({"error": "No image file provided"}), 400

        file = request.files['image']
        
        # 1. Get current UTC time
        now = datetime.utcnow()
        
        # 2. Build the folder path: uploads/2026/01/03
        folder_path = f"uploads/{now.year}/{now.month:02d}/{now.day:02d}"
        
        # 3. Build the filename: 18-30-05.jpg (ignores original 'pic' name)
        file_name = f"{now.strftime('%H-%M-%S')}.jpg"
        
        # 4. Combine them
        destination_blob_name = f"{folder_path}/{file_name}"

        storage_client = storage.Client()
        bucket = storage_client.bucket(BUCKET_NAME)
        blob = bucket.blob(destination_blob_name)
        
        # 5. Upload (Force JPEG content type since we know it's a camera stream)
        blob.upload_from_file(file, content_type='image/jpeg')

        print(f"IMAGE UPLOADED: gs://{BUCKET_NAME}/{destination_blob_name}")
        
        # Return the new path so you can debug/verify
        return jsonify({"status": "success", "gcs_path": destination_blob_name}), 200

    except Exception as e:
        print(f"Upload Error: {e}")
        return jsonify({"error": str(e)}), 500
