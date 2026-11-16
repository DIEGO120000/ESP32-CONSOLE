#pragma once
// =============================================================================
// PANEL DE ADMINISTRACION WEB + OTA (FASE 3)
// =============================================================================
// Se monta sobre el WebServer del AP de mandos 2P (WiFiGamepadServer):
//   GET  /admin          panel HTML/CSS/JS embebido en PROGMEM
//   GET  /config         alias de /admin (http://config, 192.168.4.1/config)
//   GET  /api/status     telemetria (bateria, flash, RAM, cartuchos)
//   GET  /api/settings   ajustes actuales (brillo, sonido)
//   POST /api/settings   aplica brillo (10..100) y sonido (0/1)
//   POST /api/restart    reinicia la consola
//   POST /update         actualiza el firmware por bloques (Update.h)
//
// El modulo no toca pines ni logica de juego: solo lee telemetria del nucleo y
// escribe ajustes por la API publica de console_core.h.

#include <Arduino.h>
#include <WebServer.h>

class ConsoleAdminWeb {
public:
    // Registra todas las rutas. Idempotente: se puede llamar una sola vez en
    // setup() antes de que el AP arranque (WebServer guarda los handlers).
    void begin(WebServer &server);

    // Debe llamarse en cada vuelta del loop: gestiona el reinicio diferido
    // posterior a una OTA correcta.
    void update();

    // Sirve el panel. Lo usa el enrutado por Host del AP para http://config.
    void servePage() { handleAdminPage(); }

    // true mientras hay una subida .bin en curso o pendiente de reinicio.
    bool isUpdating() const { return _updating || _restartAt != 0; }

    // Superpone la pantalla de progreso OTA al framebuffer actual. main.cpp lo
    // llama justo antes de arduboy.display().
    void drawOverlay();

private:
    WebServer *_server = nullptr;

    bool _updating = false;
    bool _uploadOk = false;
    String _uploadError;
    uint8_t _progress = 0;
    size_t _bodyTotal = 0; // Content-Length del multipart (para el porcentaje)
    unsigned long _restartAt = 0;

    void handleAdminPage();
    void handleStatus();
    void handleSettingsGet();
    void handleSettingsPost();
    void handleRestart();
    void handleOtaFinish();
    void handleOtaUpload();

    void sendJson(int code, const String &body);
    static void appendPortsJson(String &out);
};

extern ConsoleAdminWeb consoleAdmin;
