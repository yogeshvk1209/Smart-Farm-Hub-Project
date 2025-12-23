-- BigQuery Schema Definition
-- Run this in the BigQuery Console Query Editor to recreate the infrastructure

-- 1. Create Dataset
CREATE SCHEMA IF NOT EXISTS `farm_telemetry`
OPTIONS (
  location = 'asia-south1'
);

-- 2. Create Table
CREATE TABLE IF NOT EXISTS `farm_telemetry.soil_readings` (
  event_ts TIMESTAMP NOT NULL OPTIONS(description="UTC Timestamp of reading"),
  device_id STRING NOT NULL OPTIONS(description="Unique ID of the Spoke"),
  moisture_raw INT64 OPTIONS(description="Raw ADC value (0-1024)"),
  moisture_pct INT64 OPTIONS(description="Calculated percentage (0-100)"),
  battery_volts FLOAT64 OPTIONS(description="Battery voltage level"),
  meta_data STRING OPTIONS(description="Debug info (e.g., transmission method)")
)
PARTITION BY DATE(event_ts); 
-- Partitioning by day reduces query costs significantly for IoT data
