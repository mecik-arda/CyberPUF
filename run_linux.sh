#!/bin/bash
echo "======================================================="
echo "CyberPUF Web Dashboard Baslatiliyor..."
echo "Tarayici otomatik olarak aciliyor..."
echo "======================================================="
# Arka planda 2 saniye bekleyip tarayıcıyı açar
(sleep 2 && xdg-open http://127.0.0.1:8000 2>/dev/null || open http://127.0.0.1:8000 2>/dev/null) &
python3 -m uvicorn main_app:app --host 127.0.0.1 --port 8000
