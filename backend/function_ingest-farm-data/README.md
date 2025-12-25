# 🛰️ IoT Ingestion Function (Cloud Run)

This Python function serves as the entry point for telemetry data from the **Quectel EC200U** IoT modem (and other devices). It processes incoming HTTP requests and streams the data into **Google BigQuery**.

## 🚀 Overview

The function is built using the `functions-framework` and is designed to handle the specific constraints of low-power IoT modems, such as limited support for complex HTTP headers or JSON serialization.

### Key Features:
- **Dual Input Support:** Accepts data via standard JSON POST or URL Query Parameters.
- **Data Casting:** Automatically converts string-based query parameters to correct numeric types (Integer/Float).
- **Auto-Timestamping:** Appends a UTC timestamp (`event_ts`) to every record upon arrival.
- **Direct BQ Integration:** Uses the BigQuery Storage Write API (via `insert_rows_json`) for real-time ingestion.

## 📥 API Specification

### 1. URL Parameter Mode (Recommended for Modems)
Used when the device cannot easily construct a JSON body.

**Endpoint:** `ANY https://[YOUR-REGION]-[YOUR-PROJECT].a.run.app/`

| Parameter | Type | Description | BigQuery Field |
| :--- | :--- | :--- | :--- |
| `device_id` | String | Unique ID of the spoke/hub | `device_id` |
| `raw` | Integer | Raw ADC moisture value | `moisture_raw` |
| `pct` | Integer | Moisture percentage (0-100) | `moisture_pct` |
| `bat` | Float | Battery voltage (e.g., 4.2) | `battery_volts` |

**Example:**
```http
GET /?device_id=spoke_01&raw=650&pct=42&bat=3.95
```

### 2. JSON Payload Mode
Used for testing or by more capable gateways.

**Example:**
```json
{
  "device_id": "hub_01",
  "raw": 580,
  "pct": 60,
  "bat": 4.1
}
```

## 🛠️ Configuration

Before deployment, ensure the constants in `main.py` match your GCP infrastructure:

```python
PROJECT_ID = "your-project-id"
DATASET_ID = "farm_telemetry"
TABLE_ID = "soil_readings"
```

## 📦 Deployment

Deploy to Google Cloud Functions (Gen 2) using the following command:

```bash
gcloud functions deploy ingest-farm-data \
  --gen2 \
  --runtime=python311 \
  --region=asia-south1 \
  --source=. \
  --entry-point=ingest_data \
  --trigger-http \
  --allow-unauthenticated
```

## 📊 Data Schema (BigQuery)

The function expects a table with the following schema:

| Field Name | Type | Mode |
| :--- | :--- | :--- |
| `event_ts` | TIMESTAMP | REQUIRED |
| `device_id` | STRING | NULLABLE |
| `moisture_raw` | INTEGER | NULLABLE |
| `moisture_pct` | INTEGER | NULLABLE |
| `battery_volts` | FLOAT | NULLABLE |
| `meta_data` | STRING | NULLABLE |

```