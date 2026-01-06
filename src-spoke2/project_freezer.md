## Spoke 2 (ESP32 Cam) - Image Transmission Logic
**Status:** Functional but High Packet Loss
**Date Logged:** Jan 6, 2026

### The "Garbage Header" Issue
* **Symptom:** Received files have 5 bytes of junk (`08 00 12...`) at the start, shifting the JPEG header (`FF D8`) and breaking the preview.
* **Cause:** The Hub's `OnDataRecv` logic treats *any* packet >22 bytes as image data. The Spoke sends a metadata/config packet before the image stream, which gets written to the file as junk.
* **Fix (Hub Side):** Update `OnDataRecv` to inspect the first 2 bytes of the incoming payload. Only open the SPIFFS file if the packet starts with `FF D8`.

### The "Truncated Image" Issue
* **Symptom:** Images are cut off at ~14KB (grey/maroon artifacting), even though the source is ~40KB.
* **Cause:** "Firehose" effect. The Hub writes to SPIFFS (slow) inside the ESP-NOW callback. The Spoke sends packets (fast) without waiting. The Hub misses packets while its flash storage is busy.
* **Required Fix (Spoke Side):** Introduce a `delay(20);` in the Spoke's transmission loop after every packet sent. This gives the Hub's SPIFFS time to finish writing.
* **Note:** Do not rely on increasing `jpeg_quality` to fix this; it is a timing/buffer issue, not a compression issue.
