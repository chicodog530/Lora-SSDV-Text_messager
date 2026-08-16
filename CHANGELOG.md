# Changelog

## [v4.0.0] - Motion Detection Security Camera
- **Added:** "Enable Motion Detection" feature. The web app now performs offline Javascript pixel analysis every 1 second.
- **Added:** Auto-trigger logic. If a >10% pixel change is detected, it automatically snaps a high-res photo, applies compression, and transmits it over LoRa.
- **Added:** 30-second cooldown timer for motion detection to prevent continuous radio spam.
- **Fixed:** Android Chrome aggressive video suspending issue by explicitly forcing `remoteVideo.play()` and adding a 500ms sensor wake-up delay before snapping photos.
- **Fixed:** Added a cache-buster timestamp parameter to the `/status` polling loop to prevent mobile browsers from aggressively caching the JSON response.

## [v3.0.0] - Remote Camera Control
- **Added:** "Request Remote Photo" feature. One node can now send a `REQUEST_IMAGE` packet over LoRa to the other node.
- **Added:** "Enable Remote Camera Mode" feature. The web app uses the `navigator.mediaDevices.getUserMedia` API to hook into the phone's rear camera.
- **Added:** Automatic photo snapping. The receiving phone polls the ESP32 `/status` endpoint, sees the request flag, snaps a frame from the live video feed, and automatically transmits it back over LoRa.

## [v2.0.0] - Text Messaging Integration
- **Added:** Two-way Text Messaging interface to the web app.
- **Added:** Dynamic string chunking. Javascript automatically splices long text messages into 96-byte chunks (LoRa Tweets) to comply with the LoRa module's hardware buffer limits.
- **Added:** Support for sending and displaying emojis over LoRa.
- **Changed:** Refactored the C++ backend state machine to safely handle concurrent HTTP requests and radio interrupts, resolving WDT (Watchdog Timer) panics.

## [v1.0.0] - Initial Release
- **Added:** Fast Mode (downscales to 400x300) and 2-Bit Grayscale compression algorithms running directly in the browser via Javascript `<canvas>`.
- **Added:** Custom CRC32, ACK, and MISSING packet protocol to ensure lossless image transmission over raw UART LoRa streams.
- **Added:** Real-time upload and receive progress bars in the web interface.
