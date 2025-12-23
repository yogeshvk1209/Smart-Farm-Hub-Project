# ☁️ FarmHub Backend Services

This directory contains the serverless infrastructure code for the Smart Farm Hub. It consists of a Python ingestion function hosted on **Google Cloud Run** and a data warehouse schema on **BigQuery**.

## 📂 Architecture

* **Ingest Function:** A Python (Gen 2) Cloud Run function that accepts HTTP requests from the IoT Gateway.
* **Database:** A time-partitioned BigQuery table for storing telemetry data.

## 📁 Directory Structure

```text
/backend
├── /function-ingest     # Python Source Code
│   ├── main.py          # Logic for GET/POST handling
│   └── requirements.txt # Dependencies
│
└── /database            # Infrastructure as Code
    └── schema.sql       # BigQuery DDL Script
```
🚀 Deployment Guide
Prerequisites
Google Cloud SDK (gcloud) installed and authenticated.

A GCP Project created with billing enabled.

1. Database Setup (BigQuery)
Open the BigQuery Console.

Copy the contents of database/schema.sql.

Run the query in the Editor.

This will create the farm_telemetry dataset and the soil_readings table.

2. Function Deployment (Cloud Run)
Navigate to the function directory and deploy using the CLI.

Bash

cd function-ingest

gcloud functions deploy ingest-farm-data \
  --gen2 \
  --runtime=python311 \
  --region=asia-south1 \
  --source=. \
  --entry-point=ingest_data \
  --trigger-http \
  --allow-unauthenticated
Note: The --allow-unauthenticated flag is required because the IoT modem cannot handle complex OAuth token generation.

🔌 API Usage
The ingestion function is designed to be robust against modem limitations. It supports two modes of data transfer.

Method A: URL Parameters (GET)
Used by the IoT Modem (Quectel EC200U) to bypass header injection issues.

HTTP

GET https://[YOUR-URL].run.app/?device_id=spoke_1&raw=600&pct=45&bat=4.2
Method B: JSON Payload (POST)
Used for testing or future capable devices.

HTTP

POST https://[YOUR-URL].run.app/
Content-Type: application/json

{
  "device_id": "spoke_1",
  "raw": 600,
  "pct": 45,
  "bat": 4.2
}
📊 Verification
To verify data arrival, run this SQL query in BigQuery:

SQL

SELECT * FROM `farm_telemetry.soil_readings` 
ORDER BY event_ts DESC 
LIMIT 10


