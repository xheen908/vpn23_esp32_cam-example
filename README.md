# ESP32-CAM + WireGuard + TLS + Webinterface

This project demonstrates an **ESP32-CAM** setup that:

1. Provides a **Web interface** (Port 80) to configure:  
   - **Wi-Fi** (SSID, password)  
   - **API credentials** (`username`, `password`, `deviceName`) for a service like `vpn23.com`
2. Performs a **HTTPS** login (`POST https://vpn23.com/login`) to get a **JWT**.
3. Fetches the **WireGuard configuration** (`GET https://vpn23.com/clients/name/<deviceName>/config`) using the JWT.
4. Starts **WireGuard** via the [WireGuard-ESP32 library](https://github.com/ciniml/WireGuard-ESP32-Arduino).
5. Streams the camera via an **MJPEG** endpoint (`/stream`) on Port 80.

> **Important**:  
> - If you want to strictly validate TLS certificates by date, use **NTP** (`configTime(...)`) to set the correct system time before calling the HTTPS endpoints.  
> - The **Root CA** (`rootCACert`) in the code is an example (e.g. GTS Root R4). If your server uses a different CA, replace it.  
> - By default, the camera stream is available on **all** interfaces (local Wi-Fi + WireGuard). If you only want it accessible via WireGuard, consider firewall rules or advanced interface binding.

---

## Features

1. **Web Interface**  
   - Main page (`/`): Shows Wi-Fi status, WireGuard status, a live camera preview (`/stream`).  
   - Config page (`/config`): A form to enter Wi-Fi SSID/password and API credentials (`username`, `password`, `deviceName`).  
   - Save endpoint (`/saveConfig`): Persists settings in **Preferences (NVS)**.

2. **WireGuard**  
   - The sketch uses the `WireGuard-ESP32` library to start a VPN interface.  
   - The required data (`private_key`, `endpoint`, etc.) is fetched from your server (example: `vpn23.com`) in JSON format.

3. **ESP32-CAM**  
   - Uses typical AI-Thinker pin assignments.  
   - Provides an **MJPEG** stream on `/stream`.  
   - Adjust resolution, quality, and framerate in the code (`FRAMESIZE_QVGA`, `jpeg_quality = 12`, etc.).

4. **TLS (HTTPS)**  
   - Uses `WiFiClientSecure` with a **Root CA** (`rootCACert`).  
   - Replaces `vpn23.com` with your actual domain if needed, plus your CA certificate.

---

## Requirements

- **ESP32-CAM** board (AI-Thinker or similar).  
- Arduino IDE or PlatformIO with **ESP32 support**.  
- Additional libraries:
  - **ArduinoJson** (for JSON parsing)
  - **WireGuard-ESP32** ([GitHub link](https://github.com/ciniml/WireGuard-ESP32-Arduino))

Make sure you have enough flash space for camera + Wi-Fi + WireGuard.

---

## Setup & Usage

1. **Download / Copy** the `.ino` file (or `.cpp` if you prefer).  
2. Open in Arduino IDE (or PlatformIO) and ensure you have the libraries installed.  
3. **Edit** the code for your board (pin definitions if not AI-Thinker) and your domain if not `vpn23.com`.  
4. Flash to the ESP32-CAM.  

### First Start

- If the ESP32 cannot connect to the stored Wi-Fi, it **starts an AP** with SSID `ESP32_Config` (password `12345678`).  
- Connect your PC or phone to `ESP32_Config`, then go to `http://192.168.4.1/`.  
- On the config page, enter:
  - **Wi-Fi SSID/Password**  
  - **Username/Password/DeviceName** for the service (e.g. `vpn23.com`)  
- Save. The ESP32 reboots or continues, tries Wi-Fi again.

### WireGuard

- On successful Wi-Fi, the device `POST`s to `https://vpn23.com/login` for a JWT.  
- Then a `GET` to `https://vpn23.com/clients/name/<deviceName>/config` is done with `Bearer <JWT>`.  
- If the JSON parse is successful, it stores `private_key`, `endpoint`, etc., and calls `wg.begin(...)`.

### Camera Streaming

- If everything is up, you can reach:
  - `http://<esp-ip>/` for a simple page with a live preview (`<img src="/stream">`).  
  - `http://<esp-ip>/stream` for raw MJPEG streaming.

### Access Over WireGuard

- Once WireGuard is running, the device should be accessible via its **WG IP** (e.g. `10.x.x.x`).  
- If you only want streaming over the VPN, do **not** forward port 80 from your router. Possibly block local LAN access to port 80.

---

## Tips

- **NTP**: If you want real date/time checking on TLS certificates, do `configTime(...)`.  
- **Performance**: For streaming, smaller frames (`FRAMESIZE_QQVGA` or `FRAMESIZE_QVGA`) are smoother.  
- **Memory**: The ESP32-CAM can be tight on memory with too many tasks. Keep it minimal.  
- **Security**: The web interface is unencrypted on port 80. If you need higher security, consider restricting physical or LAN access.

---

## Deutsche Version

Dieses Projekt zeigt ein **ESP32-CAM**-Setup, das:

1. Ein **Webinterface** (Port 80) bietet, um  
   - **WLAN** (SSID, Passwort)  
   - **API-Credentials** (`username`, `password`, `deviceName`) für z.B. `vpn23.com` einzustellen.  
2. Über **HTTPS** (`POST https://vpn23.com/login`) ein JWT holt.  
3. Die **WireGuard-Konfiguration** (`GET https://vpn23.com/clients/name/<deviceName>/config`) ausliest und WireGuard startet.  
4. Einen **MJPEG-Kamera-Stream** unter Port 80 (`/stream`) anbietet.

> **Hinweis**:  
> - Für echte TLS-Zertifikatsvalidierung solltest du die Zeit über **NTP** setzen.  
> - Das **Root CA** im Code (z.B. GTS Root R4) muss ggf. an deinen echten Server angepasst werden.  
> - Willst du den Kamerastream nur über WireGuard zugänglich machen, musst du das **lokale WLAN** blockieren oder den Webserver anders binden.

### Hauptfunktionen

- **Webinterface**:  
  - `/`: Hauptseite mit Live-Vorschau (img-Tag auf `/stream`)  
  - `/config`: Formular für WLAN + API-Creds  
  - `/saveConfig`: Speichert Settings in **Preferences**  
  - `/stream`: MJPEG-Livebild  

- **WireGuard**:  
  - Nutzung der [WireGuard-ESP32-Library](https://github.com/ciniml/WireGuard-ESP32-Arduino).  
  - Daten (`privateKey`, `endpoint`, etc.) kommen per HTTPS von `vpn23.com` (oder deinem Server).  

- **ESP32-CAM**  
  - Pin-Zuweisung (AI-Thinker)  
  - MJPEG-Stream für schnelles Livebild  

### Voraussetzungen

- **Arduino IDE** oder PlatformIO mit **ESP32-Support**  
- Libraries:
  - **ArduinoJson**  
  - **WireGuard-ESP32**  
- Ausreichend Flash/RAM auf deinem Modul.

### Inbetriebnahme

1. Kopiere den Sketch in die Arduino IDE.  
2. Passe ggf. Pins, Root CA, Domain (`vpn23.com`) an.  
3. Hochladen.  

**Erster Start**:

- Kann sich das ESP32-CAM nicht mit WLAN verbinden, geht es in **AP-Modus** (SSID `ESP32_Config`, PW `12345678`).  
- Verbinde dich, rufe `http://192.168.4.1/` auf, trage WLAN + API-Creds ein.  
- Speichere. Das Board versucht erneut WLAN, holt JWT, ruft WG-Config ab, startet WireGuard.

### Zugriff

- Normalerweise ist der Stream unter `http://<IP>/stream` erreichbar.  
- Wenn WireGuard aktiv ist und dein Peer-Netz die Routen hat, kannst du auch `http://10.x.x.x/stream` im WG-Netz nutzen.  

### Hinweise

- **NTP**: Für echte Datumskontrolle im TLS-Zertifikat solltest du `configTime` verwenden.  
- **Sicherheit**: Das Webinterface ist unverschlüsselt auf Port 80. Leite es nicht ungeschützt ins Internet weiter.  
- **Leistung**: QVGA oder QQVGA sind oft flüssiger als hohe Auflösungen.  

Viel Erfolg beim Anpassen!  
