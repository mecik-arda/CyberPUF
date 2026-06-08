#!/usr/bin/env python
"""
CyberPUF App Launcher - Loads .env and starts Uvicorn
"""

import os
import sys
import subprocess
import webbrowser
import time
from dotenv import load_dotenv

def main():
    print("=" * 60)
    print("CyberPUF Web Dashboard Başlatılıyor...")
    print("=" * 60)
    print()
    
    # Load environment variables from .env
    load_dotenv()
    print("[✓] .env dosyası yüklendi")
    
    # Verify required variables
    required_vars = ['CYBERPUF_AES_KEY', 'WEBSOCKET_TOKEN', 'APP_HOST', 'APP_PORT']
    missing = []
    
    for var in required_vars:
        if not os.environ.get(var):
            missing.append(var)
        else:
            print(f"[✓] {var} = {os.environ.get(var)[:20]}...")
    
    if missing:
        print()
        print(f"❌ HATA: Eksik environment variables: {', '.join(missing)}")
        print("Lütfen .env dosyasını kontrol edin.")
        input("Devam etmek için herhangi bir tuşa basın...")
        sys.exit(1)
    
    print()
    host = os.environ.get('APP_HOST', '127.0.0.1')
    port = os.environ.get('APP_PORT', '8000')
    url = f"http://{host}:{port}"
    
    print(f"[→] Tarayıcı açılıyor: {url}")
    print()
    
    # Open browser after a short delay
    time.sleep(2)
    try:
        webbrowser.open(url)
    except:
        pass
    
    # Start Uvicorn
    print("[✓] Uvicorn başlatılıyor...")
    print("=" * 60)
    print()
    
    subprocess.run([
        sys.executable, '-m', 'uvicorn',
        'main_app:app',
        '--host', host,
        '--port', port,
        '--reload'
    ])

if __name__ == '__main__':
    main()
