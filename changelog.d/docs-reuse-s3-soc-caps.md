### Modifié

- **Documentation, partir de ce projet : l'ESP32-S3 sans MIPI-DSI ni JPEG matériel** ([`docs/reuse.md`](docs/reuse.md)) : les deux « non vérifié » sont remplacés par ce que déclare ESP-IDF v5.5.5 (`soc_caps.h` du S3 et du P4 : `SOC_MIPI_DSI_SUPPORTED` et `SOC_JPEG_CODEC_SUPPORTED` déclarés pour le P4, absents pour le S3), avec les liens et la date de lecture.
