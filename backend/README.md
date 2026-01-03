# ☁️ FarmHub Backend Services

This directory contains the serverless infrastructure code for the Smart Farm Hub. It consists of a Python ingestion function hosted on **Google Cloud Run** and a data warehouse schema on **BigQuery**.

## 📂 Architecture

* **Ingest Function:** A Python (Gen 2) Cloud Run function that acts as a Unified Endpoint.
    *   **GET:** Handles Sensor Telemetry -> BigQuery.
    *   **POST:** Handles Image Uploads -> Google Cloud Storage (GCS).
* **Database:** A time-partitioned BigQuery table for storing telemetry data.
* **Storage:** A GCS Bucket for archiving camera images.

## 📁 Directory Structure

```text
/backend
├── /function_ingest-farm-data # Python Source Code
│   ├── main.py                # Logic for GET/POST handling
│   └── requirements.txt       # Dependencies
│
└── /database                  # Infrastructure as Code
    └── schema.sql             # BigQuery DDL Script
```
## ⚙️ Configuration

### Environment Variables
The ingestion function code currently contains configuration values at the top of `function_ingest-farm-data/main.py`.

**Before deploying**, please open `backend/function_ingest-farm-data/main.py` and update the following variables to match your GCP project:

```python
# 1. BigQuery Config
BQ_TABLE_ID = "farm-hub-482111.farm_telemetry.soil_readings"

# 2. GCS Config
BUCKET_NAME = "farm-images-archive" 
EXPECTED_API_KEY = "FARM_SEC" 
```

## 🚀 Deployment Guide
Prerequisites:
*   Google Cloud SDK (gcloud) installed and authenticated.
*   A GCP Project created with billing enabled.

1. **Database Setup (BigQuery)**
    *   Open the BigQuery Console.
    *   Copy the contents of `database/schema.sql`.
    *   Run the query to create the `farm_telemetry` dataset and `soil_readings` table.

2. **Function Deployment (Cloud Run)**
    *   Navigate to the function directory and deploy using the CLI.

```bash
cd function_ingest-farm-data

gcloud functions deploy ingest-farm-data \
  --gen2 \
  --runtime=python311 \
  --region=asia-south1 \
  --source=. \
  --entry-point=ingest_data \
  --trigger-http \
  --allow-unauthenticated
```
*Note: The `--allow-unauthenticated` flag is required because the simple IoT modem cannot handle complex OAuth token generation. Instead, we use an API Key check inside the code.*

## 🔌 API Usage
The ingestion function handles two distinct flows based on the HTTP Method.

> **🔒 Security:** All requests must include a `token` parameter (query param or header) matching `EXPECTED_API_KEY`. If the token is missing or invalid, the server returns `401 Unauthorized`.

### Method A: Telemetry (GET)
Used by the IoT Modem (Quectel EC200U) to upload sensor data.

```http
GET https://[YOUR-URL].run.app/?device_id=spoke_1&raw=600&pct=45&bat=4.2&token=FARM_SEC
```

### Method B: Image Upload (POST)
Used by the Hub to stream images from the Camera Spoke.
*   **Content-Type:** `multipart/form-data`
*   **File Field Name:** `image`

```http
POST https://[YOUR-URL].run.app/?token=FARM_SEC
Content-Type: multipart/form-data; boundary=...
... [Image Data] ...
```

## 📊 Verification
To verify data arrival, run this SQL query in BigQuery:

```sql
SELECT * FROM `farm_telemetry.soil_readings` 
ORDER BY event_ts DESC 
LIMIT 10
```


