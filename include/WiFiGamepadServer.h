#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <SHA1Builder.h>
#include <base64.h>

// =============================================================================
// HTML/CSS/JS MOBILE GAMEPAD CONTROLLER (EMBEDDED IN PROGMEM)
// =============================================================================
static const char GAMEPAD_HTML[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="es">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no, viewport-fit=cover">
<title>RetroArcade Gamepad</title>
<style>
  * {
    box-sizing: border-box;
    margin: 0;
    padding: 0;
    -webkit-touch-callout: none;
    -webkit-user-select: none;
    user-select: none;
    touch-action: none;
  }
  body, html {
    width: 100%;
    height: 100%;
    overflow: hidden;
    background-color: #0d0f18;
    font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, "Courier New", monospace;
    color: #e0e6ed;
    display: flex;
    flex-direction: column;
  }
  header {
    height: 54px;
    background: linear-gradient(180deg, #181b29 0%, #101320 100%);
    border-bottom: 2px solid #262c45;
    display: flex;
    align-items: center;
    justify-content: space-between;
    padding: 0 16px;
    flex-shrink: 0;
  }
  .title {
    font-size: 14px;
    font-weight: 800;
    letter-spacing: 1px;
    color: #8fa0c0;
    text-transform: uppercase;
  }
  .badge {
    font-size: 12px;
    font-weight: 700;
    padding: 5px 12px;
    border-radius: 12px;
    letter-spacing: 0.5px;
    background: #2a2e45;
    color: #a0aec0;
    transition: all 0.2s ease;
  }
  .badge.p1 {
    background: rgba(0, 240, 255, 0.2);
    color: #00f0ff;
    border: 1px solid #00f0ff;
    box-shadow: 0 0 10px rgba(0, 240, 255, 0.4);
  }
  .badge.p2 {
    background: rgba(255, 120, 0, 0.2);
    color: #ff8c00;
    border: 1px solid #ff8c00;
    box-shadow: 0 0 10px rgba(255, 140, 0, 0.4);
  }
  .badge.connecting {
    background: rgba(255, 200, 0, 0.2);
    color: #ffd000;
  }
  .pad-container {
    flex: 1;
    display: flex;
    flex-direction: column;
    padding: 12px;
    gap: 12px;
    height: calc(100% - 54px);
  }
  .touch-btn {
    flex: 1;
    display: flex;
    flex-direction: column;
    align-items: center;
    justify-content: center;
    border-radius: 16px;
    background: #151928;
    border: 3px solid #28304f;
    box-shadow: inset 0 2px 0 rgba(255,255,255,0.06), 0 6px 16px rgba(0,0,0,0.4);
    cursor: pointer;
    transition: background 0.08s, transform 0.05s, border-color 0.08s, box-shadow 0.08s;
  }
  .touch-btn .arrow {
    font-size: 48px;
    line-height: 1;
    color: #8e9bb8;
    transition: color 0.08s;
  }
  .touch-btn .label {
    font-size: 16px;
    font-weight: 800;
    letter-spacing: 2px;
    margin-top: 6px;
    color: #8e9bb8;
    text-transform: uppercase;
  }
  .touch-btn.active {
    background: #1f2742 !important;
    transform: scale(0.98);
  }
  .p1-mode .touch-btn.active {
    border-color: #00f0ff;
    box-shadow: 0 0 20px rgba(0, 240, 255, 0.6), inset 0 0 12px rgba(0, 240, 255, 0.3);
  }
  .p1-mode .touch-btn.active .arrow,
  .p1-mode .touch-btn.active .label {
    color: #00f0ff;
  }
  .p2-mode .touch-btn.active {
    border-color: #ff8c00;
    box-shadow: 0 0 20px rgba(255, 140, 0, 0.6), inset 0 0 12px rgba(255, 140, 0, 0.3);
  }
  .p2-mode .touch-btn.active .arrow,
  .p2-mode .touch-btn.active .label {
    color: #ff8c00;
  }
</style>
</head>
<body id="body">
  <header>
    <div class="title">PING PONG 2.42"</div>
    <div id="statusBadge" class="badge connecting">CONECTANDO...</div>
  </header>
  <div class="pad-container">
    <div id="btnUp" class="touch-btn">
      <div class="arrow">&#9650;</div>
      <div class="label">ARRIBA</div>
    </div>
    <div id="btnDown" class="touch-btn">
      <div class="arrow">&#9660;</div>
      <div class="label">ABAJO</div>
    </div>
  </div>

  <script>
    let ws = null;
    let playerRole = "";
    const badge = document.getElementById("statusBadge");
    const body = document.getElementById("body");
    const btnUp = document.getElementById("btnUp");
    const btnDown = document.getElementById("btnDown");

    function haptic() {
      if (navigator.vibrate) {
        navigator.vibrate(15);
      }
    }

    function connectWS() {
      const host = window.location.hostname || "192.168.4.1";
      ws = new WebSocket("ws://" + host + ":81/");

      ws.onopen = function() {
        badge.textContent = "EN SALA (ESPERANDO ROL)";
        badge.className = "badge connecting";
      };

      ws.onmessage = function(e) {
        const msg = e.data;
        if (msg === "P1") {
          playerRole = "P1";
          badge.textContent = "JUGADOR 1 (IZQ)";
          badge.className = "badge p1";
          body.className = "p1-mode";
          haptic();
        } else if (msg === "P2") {
          playerRole = "P2";
          badge.textContent = "JUGADOR 2 (DER)";
          badge.className = "badge p2";
          body.className = "p2-mode";
          haptic();
        } else if (msg === "FULL") {
          badge.textContent = "SALA LLENA (ESPECTADOR)";
          badge.className = "badge";
        }
      };

      ws.onclose = function() {
        badge.textContent = "DESCONECTADO";
        badge.className = "badge";
        setTimeout(connectWS, 1500);
      };

      ws.onerror = function() {
        ws.close();
      };
    }

    function sendEvent(byteCode) {
      if (ws && ws.readyState === WebSocket.OPEN) {
        ws.send(byteCode);
      }
    }

    function bindTouch(element, pressCode, releaseCode) {
      let isPressed = false;

      function startPress(e) {
        e.preventDefault();
        if (!isPressed) {
          isPressed = true;
          element.classList.add("active");
          haptic();
          sendEvent(pressCode);
        }
      }

      function endPress(e) {
        e.preventDefault();
        if (isPressed) {
          isPressed = false;
          element.classList.remove("active");
          sendEvent(releaseCode);
        }
      }

      element.addEventListener("touchstart", startPress, {passive: false});
      element.addEventListener("touchend", endPress, {passive: false});
      element.addEventListener("touchcancel", endPress, {passive: false});
      element.addEventListener("mousedown", startPress);
      element.addEventListener("mouseup", endPress);
      element.addEventListener("mouseleave", endPress);
    }

    // Single-byte commands:
    // 'U' = Up Press, 'u' = Up Release
    // 'D' = Down Press, 'd' = Down Release
    bindTouch(btnUp, "U", "u");
    bindTouch(btnDown, "D", "d");

    window.addEventListener("load", connectWS);
  </script>
</body>
</html>)rawliteral";

// =============================================================================
// NATIVE WEBSOCKET CLIENT DATA STRUCTURE
// =============================================================================
struct WsClient {
    WiFiClient client;
    bool active = false;
    bool isHandshakeDone = false;
    uint8_t playerSlot = 0; // 1 = P1, 2 = P2, 0 = Spectator
};

// =============================================================================
// WIFI GAMEPAD MANAGER CLASS
// =============================================================================
class WiFiGamepadManager {
public:
    static const uint8_t MAX_CLIENTS = 4;
    WsClient clients[MAX_CLIENTS];

    // Real-time button states updated by 1-byte WebSocket frames
    volatile bool p1Up = false;
    volatile bool p1Down = false;
    volatile bool p2Up = false;
    volatile bool p2Down = false;

    // Connection tracking
    bool p1Connected = false;
    bool p2Connected = false;

    bool isRunning = false;

private:
    DNSServer dnsServer;
    WebServer webServer{80};
    WiFiServer wsServer{81};

public:
    void begin() {
        if (isRunning) return;

        WiFi.mode(WIFI_AP);
        WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
        WiFi.softAP("RetroArcade-ESP32", nullptr, 1, 0, 4);

        // DNS Server for Captive Portal redirection
        dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
        dnsServer.start(53, "*", IPAddress(192, 168, 4, 1));

        // HTTP Routes for Captive Portal and Gamepad UI
        webServer.on("/", [this]() {
            webServer.send_P(200, "text/html", GAMEPAD_HTML);
        });
        webServer.on("/hotspot-detect.html", [this]() {
            webServer.send_P(200, "text/html", GAMEPAD_HTML);
        });
        webServer.on("/generate_204", [this]() {
            webServer.send_P(200, "text/html", GAMEPAD_HTML);
        });
        webServer.on("/canonical.html", [this]() {
            webServer.send_P(200, "text/html", GAMEPAD_HTML);
        });
        webServer.onNotFound([this]() {
            webServer.send_P(200, "text/html", GAMEPAD_HTML);
        });

        webServer.begin();
        wsServer.begin();
        wsServer.setNoDelay(true);

        p1Up = false;
        p1Down = false;
        p2Up = false;
        p2Down = false;
        p1Connected = false;
        p2Connected = false;

        for (uint8_t i = 0; i < MAX_CLIENTS; i++) {
            clients[i].active = false;
            clients[i].isHandshakeDone = false;
            clients[i].playerSlot = 0;
        }

        isRunning = true;
    }

    void stop() {
        if (!isRunning) return;

        for (uint8_t i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].active) {
                clients[i].client.stop();
                clients[i].active = false;
                clients[i].isHandshakeDone = false;
                clients[i].playerSlot = 0;
            }
        }

        wsServer.end();
        webServer.stop();
        dnsServer.stop();
        WiFi.softAPdisconnect(true);
        WiFi.mode(WIFI_OFF);

        p1Up = false;
        p1Down = false;
        p2Up = false;
        p2Down = false;
        p1Connected = false;
        p2Connected = false;

        isRunning = false;
    }

    void update() {
        if (!isRunning) return;

        // Process DNS queries for instant captive portal popup
        dnsServer.processNextRequest();

        // Process Web Server requests
        webServer.handleClient();

        // Accept new WebSocket TCP clients
        WiFiClient newClient = wsServer.accept();
        if (newClient) {
                int freeSlot = -1;
                for (uint8_t i = 0; i < MAX_CLIENTS; i++) {
                    if (!clients[i].active || !clients[i].client.connected()) {
                        freeSlot = i;
                        break;
                    }
                }
                if (freeSlot >= 0) {
                    clients[freeSlot].client = newClient;
                    clients[freeSlot].client.setNoDelay(true);
                    clients[freeSlot].active = true;
                    clients[freeSlot].isHandshakeDone = false;
                    clients[freeSlot].playerSlot = 0;
                } else {
                    newClient.stop();
                }
            }

        // Process active WebSocket clients
        p1Connected = false;
        p2Connected = false;

        for (uint8_t i = 0; i < MAX_CLIENTS; i++) {
            if (!clients[i].active) continue;

            if (!clients[i].client.connected()) {
                disconnectClient(i);
                continue;
            }

            if (!clients[i].isHandshakeDone) {
                handleHandshake(i);
            } else {
                handleFrames(i);
            }

            if (clients[i].active && clients[i].isHandshakeDone) {
                if (clients[i].playerSlot == 1) p1Connected = true;
                else if (clients[i].playerSlot == 2) p2Connected = true;
            }
        }
    }

private:
    void disconnectClient(uint8_t idx) {
        if (clients[idx].playerSlot == 1) {
            p1Up = false;
            p1Down = false;
        } else if (clients[idx].playerSlot == 2) {
            p2Up = false;
            p2Down = false;
        }
        clients[idx].client.stop();
        clients[idx].active = false;
        clients[idx].isHandshakeDone = false;
        clients[idx].playerSlot = 0;
    }

    void handleHandshake(uint8_t idx) {
        WiFiClient &client = clients[idx].client;
        if (!client.available()) return;

        String req = "";
        while (client.available()) {
            String line = client.readStringUntil('\n');
            req += line + "\n";
            if (line == "\r" || line.length() == 0) break;
        }

        int keyIdx = req.indexOf("Sec-WebSocket-Key: ");
        if (keyIdx < 0) {
            keyIdx = req.indexOf("sec-websocket-key: ");
        }

        if (keyIdx >= 0) {
            int keyStart = keyIdx + 19;
            int keyEnd = req.indexOf("\r", keyStart);
            if (keyEnd < 0) keyEnd = req.indexOf("\n", keyStart);
            String clientKey = req.substring(keyStart, keyEnd);
            clientKey.trim();

            String acceptKey = computeSecWebSocketAccept(clientKey);

            String response = "HTTP/1.1 101 Switching Protocols\r\n";
            response += "Upgrade: websocket\r\n";
            response += "Connection: Upgrade\r\n";
            response += "Sec-WebSocket-Accept: " + acceptKey + "\r\n\r\n";

            client.print(response);
            clients[idx].isHandshakeDone = true;

            // Assign Player Slot
            bool p1Taken = false;
            bool p2Taken = false;
            for (uint8_t j = 0; j < MAX_CLIENTS; j++) {
                if (clients[j].active && clients[j].isHandshakeDone && j != idx) {
                    if (clients[j].playerSlot == 1) p1Taken = true;
                    if (clients[j].playerSlot == 2) p2Taken = true;
                }
            }

            if (!p1Taken) {
                clients[idx].playerSlot = 1;
                sendWsText(idx, "P1");
            } else if (!p2Taken) {
                clients[idx].playerSlot = 2;
                sendWsText(idx, "P2");
            } else {
                clients[idx].playerSlot = 0; // Spectator
                sendWsText(idx, "FULL");
            }
        }
    }

    void handleFrames(uint8_t idx) {
        WiFiClient &client = clients[idx].client;
        while (client.available() >= 2) {
            uint8_t b0 = client.read();
            uint8_t b1 = client.read();

            uint8_t opcode = b0 & 0x0F;
            if (opcode == 0x08) { // Close connection opcode
                disconnectClient(idx);
                return;
            }

            bool isMasked = (b1 & 0x80) != 0;
            uint64_t payloadLen = b1 & 0x7F;

            if (payloadLen == 126) {
                if (client.available() < 2) return;
                payloadLen = (client.read() << 8) | client.read();
            } else if (payloadLen == 127) {
                if (client.available() < 8) return;
                for (int i = 0; i < 8; i++) client.read(); // Skip 64-bit length
                payloadLen = 0;
            }

            uint8_t mask[4] = {0, 0, 0, 0};
            if (isMasked) {
                if (client.available() < 4) return;
                for (int m = 0; m < 4; m++) mask[m] = client.read();
            }

            // Read payload bytes (typically 1 byte for our gamepad)
            for (uint64_t p = 0; p < payloadLen; p++) {
                if (!client.available()) break;
                uint8_t dataByte = client.read();
                if (isMasked) {
                    dataByte ^= mask[p % 4];
                }

                if (p == 0) { // First byte is our command
                    dispatchPlayerInput(clients[idx].playerSlot, (char)dataByte);
                }
            }
        }
    }

    void dispatchPlayerInput(uint8_t slot, char cmd) {
        if (slot == 1) {
            if (cmd == 'U') p1Up = true;
            else if (cmd == 'u') p1Up = false;
            else if (cmd == 'D') p1Down = true;
            else if (cmd == 'd') p1Down = false;
        } else if (slot == 2) {
            if (cmd == 'U') p2Up = true;
            else if (cmd == 'u') p2Up = false;
            else if (cmd == 'D') p2Down = true;
            else if (cmd == 'd') p2Down = false;
        }
    }

    void sendWsText(uint8_t idx, const String &msg) {
        if (!clients[idx].active || !clients[idx].client.connected()) return;
        WiFiClient &client = clients[idx].client;

        uint8_t header[2];
        header[0] = 0x81; // FIN + Text frame
        header[1] = (uint8_t)msg.length(); // Server-to-client frames are unmasked

        client.write(header, 2);
        client.print(msg);
    }

    String computeSecWebSocketAccept(const String &key) {
        String combined = key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
        SHA1Builder sha1;
        sha1.begin();
        sha1.add((const uint8_t*)combined.c_str(), combined.length());
        sha1.calculate();
        uint8_t hash[20];
        sha1.getBytes(hash);
        return base64::encode(hash, 20);
    }
};

extern WiFiGamepadManager wifiGamepads;
