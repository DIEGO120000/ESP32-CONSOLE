#include "ConsoleAdminWeb.h"
#include "console_core.h"
#include "games_config.h"

#include <Update.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>

ConsoleAdminWeb consoleAdmin;

// =============================================================================
// PANEL HTML (PROGMEM, sin dependencias externas)
// =============================================================================
static const char ADMIN_HTML[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="es">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0, viewport-fit=cover">
<meta name="theme-color" content="#0b0f19">
<title>RetroArcade · Admin</title>
<style>
  :root {
    --bg: #0b0f19;
    --panel: #121728;
    --panel2: #0e1322;
    --line: #262c45;
    --text: #e0e6ed;
    --muted: #8e9bb8;
    --cyan: #00f0ff;
    --orange: #ff8c00;
    --green: #34d399;
    --red: #f87171;
  }
  * { box-sizing: border-box; }
  html, body {
    margin: 0; padding: 0;
    background: var(--bg);
    color: var(--text);
    font-family: "Courier New", ui-monospace, SFMono-Regular, Menlo, monospace;
    -webkit-text-size-adjust: 100%;
  }
  body { padding: 14px 14px 28px; max-width: 980px; margin: 0 auto; }
  header {
    display: flex; align-items: center; justify-content: space-between; gap: 10px;
    background: linear-gradient(180deg, #181b29 0%, #101320 100%);
    border: 1px solid var(--line); border-radius: 12px;
    padding: 10px 14px; margin-bottom: 14px;
  }
  h1 { font-size: 15px; margin: 0; letter-spacing: 1.5px; color: var(--cyan); text-transform: uppercase; }
  h1 small { display: block; color: var(--muted); font-size: 10px; letter-spacing: 1px; margin-top: 3px; }
  .badge {
    font-size: 10px; font-weight: 700; padding: 4px 9px; border-radius: 8px;
    background: #2a2e45; color: #a0aec0; letter-spacing: .5px; white-space: nowrap;
  }
  .badge.ok { color: var(--green); border: 1px solid rgba(52,211,153,.5); background: rgba(52,211,153,.12); }
  .badge.err { color: var(--red); border: 1px solid rgba(248,113,113,.5); background: rgba(248,113,113,.12); }
  .grid { display: grid; gap: 12px; grid-template-columns: 1fr; }
  @media (min-width: 720px) { .grid { grid-template-columns: 1fr 1fr; } .grid .wide { grid-column: 1 / -1; } }
  .card {
    background: var(--panel); border: 1px solid var(--line); border-radius: 12px;
    padding: 14px; box-shadow: 0 6px 18px rgba(0,0,0,.35);
  }
  .card h2 {
    font-size: 11px; letter-spacing: 1.6px; text-transform: uppercase;
    color: var(--muted); margin: 0 0 12px; font-weight: 700;
  }
  .big { font-size: 34px; line-height: 1; color: var(--cyan); letter-spacing: -1px; }
  .big small { font-size: 13px; color: var(--muted); letter-spacing: 0; margin-left: 4px; }
  .sub { font-size: 11px; color: var(--muted); margin-top: 6px; letter-spacing: .5px; }
  .bar { height: 9px; background: var(--panel2); border: 1px solid var(--line); border-radius: 6px; overflow: hidden; margin-top: 10px; }
  .bar > span { display: block; height: 100%; width: 0%; background: linear-gradient(90deg, #0ea5b7, var(--cyan)); transition: width .35s ease; }
  .bar.warn > span { background: linear-gradient(90deg, #b45309, var(--orange)); }
  .bar.crit > span { background: linear-gradient(90deg, #b91c1c, var(--red)); }
  .rows { display: flex; justify-content: space-between; font-size: 12px; color: var(--muted); margin-top: 8px; }
  .control { display: flex; align-items: center; justify-content: space-between; gap: 14px; padding: 10px 0; border-bottom: 1px dashed #1e2438; }
  .control:last-child { border-bottom: 0; }
  .control label { font-size: 12px; letter-spacing: .5px; }
  .control .hint { display: block; color: var(--muted); font-size: 10px; margin-top: 3px; }
  input[type=range] { -webkit-appearance: none; appearance: none; width: 150px; max-width: 45%; height: 6px; border-radius: 4px; background: #222941; outline: none; }
  input[type=range]::-webkit-slider-thumb { -webkit-appearance: none; width: 18px; height: 18px; border-radius: 50%; background: var(--cyan); border: 2px solid #0b0f19; box-shadow: 0 0 10px rgba(0,240,255,.6); cursor: pointer; }
  input[type=range]::-moz-range-thumb { width: 16px; height: 16px; border-radius: 50%; background: var(--cyan); border: 2px solid #0b0f19; cursor: pointer; }
  .val { font-size: 12px; color: var(--cyan); min-width: 42px; text-align: right; }
  .switch { position: relative; width: 52px; height: 28px; flex: 0 0 auto; }
  .switch input { opacity: 0; width: 0; height: 0; }
  .switch span { position: absolute; inset: 0; background: #222941; border: 1px solid var(--line); border-radius: 20px; transition: .2s; cursor: pointer; }
  .switch span::before { content: ""; position: absolute; width: 20px; height: 20px; left: 3px; top: 3px; border-radius: 50%; background: #64748b; transition: .2s; }
  .switch input:checked + span { background: rgba(0,240,255,.2); border-color: var(--cyan); }
  .switch input:checked + span::before { transform: translateX(24px); background: var(--cyan); box-shadow: 0 0 10px rgba(0,240,255,.7); }
  button {
    font-family: inherit; font-size: 12px; letter-spacing: 1px; text-transform: uppercase;
    background: #1b2136; color: var(--text); border: 1px solid var(--line);
    border-radius: 9px; padding: 10px 14px; cursor: pointer; transition: .15s;
  }
  button:hover { border-color: var(--cyan); color: var(--cyan); }
  button.danger { border-color: rgba(248,113,113,.45); color: var(--red); }
  button.danger:hover { background: rgba(248,113,113,.12); border-color: var(--red); }
  button.primary { border-color: rgba(0,240,255,.5); color: var(--cyan); background: rgba(0,240,255,.08); }
  button:disabled { opacity: .45; cursor: not-allowed; }
  .ports { display: flex; flex-wrap: wrap; gap: 7px; }
  .chip { font-size: 10px; padding: 5px 9px; border-radius: 7px; background: var(--panel2); border: 1px solid var(--line); color: #b6c2d9; letter-spacing: .4px; }
  .chip.on { border-color: rgba(0,240,255,.35); color: var(--cyan); }
  .gamerow { display: flex; align-items: center; justify-content: space-between; gap: 12px; padding: 7px 0; border-bottom: 1px dashed #1e2438; }
  .gamerow:last-child { border-bottom: 0; }
  .gamerow .num { color: var(--muted); font-size: 11px; margin-right: 6px; }
  .gamerow .gname { font-size: 12px; letter-spacing: .4px; }
  .gamerow.off .gname { color: var(--muted); text-decoration: line-through; }
  .note { font-size: 10.5px; color: var(--muted); line-height: 1.55; margin: 10px 0 0; }
  .note code { color: #b6c2d9; background: var(--panel2); padding: 1px 4px; border-radius: 4px; }
  .drop {
    border: 2px dashed #2b3350; border-radius: 11px; padding: 18px 12px; text-align: center;
    color: var(--muted); font-size: 12px; cursor: pointer; transition: .15s; background: var(--panel2);
  }
  .drop:hover, .drop.over { border-color: var(--cyan); color: var(--cyan); }
  .drop strong { color: var(--text); display: block; margin-bottom: 4px; font-size: 13px; }
  .file { display: none; }
  .ota-bar { height: 14px; margin-top: 12px; }
  .ota-bar > span { background: linear-gradient(90deg, #0ea5b7, var(--green)); }
  .log { font-size: 11px; color: var(--muted); margin-top: 10px; min-height: 16px; line-height: 1.5; word-break: break-word; }
  .log.ok { color: var(--green); }
  .log.err { color: var(--red); }
  footer { margin-top: 18px; display: flex; justify-content: space-between; align-items: center; gap: 10px; font-size: 11px; color: var(--muted); }
  footer a { color: var(--muted); text-decoration: none; border-bottom: 1px dotted #3a4568; }
  footer a:hover { color: var(--cyan); border-color: var(--cyan); }
</style>
</head>
<body>
  <header>
    <h1>RetroArcade · Admin<small>ESP32-C6 · SSD1309 2.42"</small></h1>
    <div id="conn" class="badge">SIN DATOS</div>
  </header>

  <div class="grid">
    <section class="card">
      <h2>Bateria</h2>
      <div class="big"><span id="batPct">--</span><small>%</small></div>
      <div class="sub" id="batVolt">-- V · GPIO 2</div>
      <div class="bar" id="batBar"><span></span></div>
      <div class="rows"><span id="batState">Leyendo…</span><span id="uptime">--</span></div>
    </section>

    <section class="card">
      <h2>Memoria</h2>
      <div class="rows"><span>Flash de aplicacion</span><span id="flashTxt">--</span></div>
      <div class="bar" id="flashBar"><span></span></div>
      <div class="rows"><span>RAM libre</span><span id="ramTxt">--</span></div>
      <div class="bar" id="ramBar"><span></span></div>
      <div class="sub" id="slotTxt">Particion OTA: --</div>
    </section>

    <section class="card">
      <h2>Ajustes de la consola</h2>
      <div class="control">
        <label>Brillo OLED<span class="hint">Contraste I2C 0x81</span></label>
        <input id="bright" type="range" min="10" max="100" step="5" value="80">
        <span class="val" id="brightVal">80%</span>
      </div>
      <div class="control">
        <label>Sonido<span class="hint">Buzzer GPIO 1</span></label>
        <div class="switch"><input id="sound" type="checkbox" checked><span></span></div>
      </div>
      <div class="control">
        <label>Reiniciar consola<span class="hint">Corta la partida en curso</span></label>
        <button id="reboot" class="danger">Reiniciar</button>
      </div>
    </section>

    <section class="card">
      <h2>Cartuchos en Menu</h2>
      <div class="gamelist" id="games"><span class="chip">Cargando…</span></div>
      <p class="note">Los interruptores ocultan el cartucho del carrusel al instante y se guardan en NVS; el codigo sigue ocupando Flash. Para liberar espacio, comenta su <code>#define</code> en <code>include/games_config.h</code> y sube el nuevo <code>.bin</code> por OTA.</p>
    </section>

    <section class="card wide">
      <h2>Actualizacion OTA · pack .bin</h2>
      <div class="drop" id="drop">
        <strong>Toca para elegir el .bin</strong>
        o arrastralo aqui · maximo el tamano de la particion OTA
      </div>
      <input class="file" id="file" type="file" accept=".bin,application/octet-stream">
      <div class="bar ota-bar" id="otaBar"><span></span></div>
      <div class="log" id="log">Sin archivo seleccionado.</div>
      <button id="upload" class="primary" disabled>Subir firmware</button>
    </section>
  </div>

  <footer>
    <span>192.168.4.1 · RetroArcade-ESP32</span>
    <a href="/">← Mandos 2P</a>
  </footer>

<script>
(function () {
  var $ = function (id) { return document.getElementById(id); };
  var fileInput = $("file"), drop = $("drop"), uploadBtn = $("upload");
  var logEl = $("log"), otaBar = $("otaBar").firstElementChild;
  var selected = null, uploading = false;

  function fmtKB(bytes) { return (bytes / 1024).toFixed(0) + " KB"; }

  function setBar(el, pct, warnAt, critAt) {
    var bar = el.parentNode;
    el.style.width = Math.max(0, Math.min(100, pct)) + "%";
    bar.classList.toggle("warn", pct >= (warnAt || 75) && pct < (critAt || 92));
    bar.classList.toggle("crit", pct >= (critAt || 92));
  }

  function setConn(ok, text) {
    var c = $("conn");
    c.className = "badge " + (ok ? "ok" : "err");
    c.textContent = text;
  }

  function applyStatus(s) {
    setConn(true, "ONLINE");
    if (s.battery) {
      $("batPct").textContent = s.battery.percent;
      $("batVolt").textContent = s.battery.voltage.toFixed(2) + " V · " + s.battery.raw_mv + " mV";
      $("batState").textContent = s.battery.percent > 70 ? "SALUDABLE" :
        s.battery.percent > 35 ? "MEDIA" : s.battery.percent > 12 ? "BAJA" : "CRITICA";
      setBar($("batBar").firstElementChild, s.battery.percent, 60, 25);
    }
    if (s.flash) {
      var usedPct = s.flash.app_size ? (s.flash.app_used / s.flash.app_size) * 100 : 0;
      $("flashTxt").textContent = fmtKB(s.flash.app_used) + " / " + fmtKB(s.flash.app_size);
      setBar($("flashBar").firstElementChild, usedPct, 85, 96);
      $("slotTxt").textContent = "Particion OTA libre: " + fmtKB(s.flash.ota_free) + " de " + fmtKB(s.flash.ota_size);
    }
    if (s.ram) {
      $("ramTxt").textContent = fmtKB(s.ram.free) + " libres de " + fmtKB(s.ram.total);
      setBar($("ramBar").firstElementChild, 100 - (s.ram.free / s.ram.total) * 100, 80, 92);
    }
    if (s.settings) {
      $("bright").value = s.settings.brightness;
      $("brightVal").textContent = s.settings.brightness + "%";
      $("sound").checked = !!s.settings.sound;
    }
    if (s.uptime_ms) {
      var sec = Math.floor(s.uptime_ms / 1000);
      $("uptime").textContent = "UP " + Math.floor(sec / 60) + "m " + (sec % 60) + "s";
    }
    if (s.games) renderGames(s.games);
  }

  // Toggles de cartuchos: se construyen una vez y luego solo se sincroniza el
  // estado, para no perder el foco ni el gesto mientras sondea /api/status.
  var gamesBusy = 0;
  function renderGames(list, force) {
    var box = $("games");
    if (box.getAttribute("data-count") !== String(list.length)) {
      box.innerHTML = "";
      list.forEach(function (g) {
        var row = document.createElement("div");
        row.className = "gamerow";
        var label = document.createElement("span");
        label.className = "gname";
        label.innerHTML = '<span class="num">' + (g.id + 1) + '. </span>' + g.name;
        var sw = document.createElement("label");
        sw.className = "switch";
        sw.innerHTML = '<input type="checkbox" data-id="' + g.id + '"><span></span>';
        row.appendChild(label);
        row.appendChild(sw);
        box.appendChild(row);
      });
      box.setAttribute("data-count", list.length);
    }
    list.forEach(function (g) {
      var cb = box.querySelector('input[data-id="' + g.id + '"]');
      if (!cb) return;
      if (force || document.activeElement !== cb) cb.checked = !!g.enabled;
      cb.disabled = gamesBusy > 0;
      cb.closest(".gamerow").classList.toggle("off", !g.enabled);
    });
  }

  $("games").addEventListener("change", function (e) {
    var cb = e.target;
    if (!cb.matches || !cb.matches('input[type=checkbox][data-id]')) return;
    var id = parseInt(cb.getAttribute("data-id"), 10);
    gamesBusy++;
    cb.disabled = true;
    fetch("/api/games", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ id: id, enabled: cb.checked ? 1 : 0 })
    })
      .then(function (r) { return r.json(); })
      .then(function (res) {
        if (!res.ok) {
          cb.checked = !cb.checked;
          alert(res.error || "No se pudo guardar");
        }
        if (res.games) renderGames(res.games, true);
        setConn(true, "ONLINE");
      })
      .catch(function () { cb.checked = !cb.checked; setConn(false, "SIN CONEXION"); })
      .then(function () { gamesBusy--; cb.disabled = false; });
  });

  function poll() {
    if (uploading) return;
    fetch("/api/status", { cache: "no-store" })
      .then(function (r) { if (!r.ok) throw 0; return r.json(); })
      .then(applyStatus)
      .catch(function () { setConn(false, "SIN CONEXION"); });
  }

  var saveTimer = null;
  function pushSettings() {
    clearTimeout(saveTimer);
    saveTimer = setTimeout(function () {
      fetch("/api/settings", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({
          brightness: parseInt($("bright").value, 10),
          sound: $("sound").checked ? 1 : 0
        })
      }).catch(function () { setConn(false, "SIN CONEXION"); });
    }, 180);
  }

  $("bright").addEventListener("input", function () {
    $("brightVal").textContent = this.value + "%";
    pushSettings();
  });
  $("sound").addEventListener("change", pushSettings);

  $("reboot").addEventListener("click", function () {
    if (!confirm("Reiniciar la consola ahora?")) return;
    fetch("/api/restart", { method: "POST" })
      .then(function () { setConn(false, "REINICIANDO"); })
      .catch(function () { setConn(false, "REINICIANDO"); });
  });

  function pickFile(f) {
    selected = f || null;
    uploadBtn.disabled = !selected || uploading;
    if (!selected) return;
    if (!/\.bin$/i.test(selected.name)) {
      logEl.className = "log err";
      logEl.textContent = "Aviso: el archivo no termina en .bin. Se subira igualmente.";
    } else {
      logEl.className = "log";
      logEl.textContent = selected.name + " · " + fmtKB(selected.size) + " listo para subir.";
    }
  }

  drop.addEventListener("click", function () { if (!uploading) fileInput.click(); });
  fileInput.addEventListener("change", function () { pickFile(this.files[0]); });
  ["dragenter", "dragover"].forEach(function (ev) {
    drop.addEventListener(ev, function (e) { e.preventDefault(); drop.classList.add("over"); });
  });
  ["dragleave", "drop"].forEach(function (ev) {
    drop.addEventListener(ev, function (e) { e.preventDefault(); drop.classList.remove("over"); });
  });
  drop.addEventListener("drop", function (e) {
    if (e.dataTransfer && e.dataTransfer.files.length) pickFile(e.dataTransfer.files[0]);
  });

  uploadBtn.addEventListener("click", function () {
    if (!selected || uploading) return;
    if (!confirm("Actualizar el firmware con " + selected.name + "? La consola se reiniciara al terminar.")) return;

    uploading = true;
    uploadBtn.disabled = true;
    logEl.className = "log";
    logEl.textContent = "Subiendo... no cierres esta pagina.";
    otaBar.style.width = "0%";

    var xhr = new XMLHttpRequest();
    xhr.open("POST", "/update", true);
    xhr.setRequestHeader("X-Filename", selected.name);

    xhr.upload.onprogress = function (e) {
      if (!e.lengthComputable) return;
      var pct = (e.loaded / e.total) * 100;
      otaBar.style.width = pct.toFixed(1) + "%";
      logEl.textContent = "Transfiriendo... " + pct.toFixed(1) + "% (" + fmtKB(e.loaded) + " de " + fmtKB(e.total) + ")";
    };

    xhr.onload = function () {
      uploading = false;
      var res = null;
      try { res = JSON.parse(xhr.responseText); } catch (err) {}
      if (xhr.status === 200 && res && res.ok) {
        otaBar.style.width = "100%";
        logEl.className = "log ok";
        logEl.textContent = "Firmware escrito y verificado. La consola se esta reiniciando...";
        setConn(false, "OTA OK");
        setTimeout(function () { logEl.textContent += " Si la pagina no responde, reconectate al AP RetroArcade-ESP32."; }, 4000);
      } else {
        logEl.className = "log err";
        logEl.textContent = "Fallo OTA: " + ((res && res.error) || ("HTTP " + xhr.status));
        uploadBtn.disabled = false;
      }
    };

    xhr.onerror = function () {
      uploading = false;
      uploadBtn.disabled = false;
      logEl.className = "log err";
      logEl.textContent = "Error de red durante la subida. Comprueba que sigues conectado al AP.";
    };

    var fd = new FormData();
    fd.append("firmware", selected, selected.name);
    xhr.send(fd);
  });

  poll();
  setInterval(poll, 2500);
})();
</script>
</body>
</html>)rawliteral";

// =============================================================================
// HELPERS
// =============================================================================
void ConsoleAdminWeb::sendJson(int code, const String &body) {
    if (!_server) return;
    _server->sendHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0", true);
    _server->sendHeader("Pragma", "no-cache", true);
    _server->send(code, "application/json", body);
}

// Lista de cartuchos compilados en este firmware (misma fuente de verdad que
// games_config.h). Sin acentos para no engordar el JSON ni el binario.
void ConsoleAdminWeb::appendPortsJson(String &out) {
    bool first = true;
    auto add = [&](const char *name) {
        if (!first) out += ',';
        first = false;
        out += '"';
        out += name;
        out += '"';
    };
    out += '[';
#ifdef ENABLE_PING_PONG
    add("Ping Pong");
#endif
#ifdef ENABLE_BREAKOUT
    add("Breakout");
#endif
#ifdef ENABLE_SPACE_INVADERS
    add("Space Invaders");
#endif
#ifdef ENABLE_1943
    add("1943");
#endif
#ifdef ENABLE_FLAPPY_BIRD
    add("Flappy Bird");
#endif
#ifdef ENABLE_SNAKE
    add("Snake");
#endif
#ifdef ENABLE_JUMP_MAN
    add("Jump Man");
#endif
    add("Proyecto BLE");
#ifdef ENABLE_2048
    add("2048");
#endif
#ifdef ENABLE_TREX_RUNNER
    add("T-Rex Runner");
#endif
#ifdef ENABLE_TIC_TAC_TOE
    add("Tic-Tac-Toe");
#endif
#ifdef ENABLE_BRAINDU
    add("BrainduTrain");
#endif
#ifdef ENABLE_HANGMAN
    add("Hangman");
#endif
#ifdef ENABLE_TRIS
    add("TRIS");
#endif
#ifdef ENABLE_MATH_CHALLENGE
    add("Math Challenge");
#endif
#ifdef ENABLE_SOPA_LETRAS
    add("Sopa de Letras");
#endif
#ifdef ENABLE_TRENCH_RUN
    add("Trench Run");
#endif
#ifdef ENABLE_ROAD_TRIP
    add("Road Trip");
#endif
#ifdef ENABLE_WOLFENDUINO_3D
    add("Wolfenduino 3D");
#endif
#ifdef ENABLE_CYBER_ESCAPE
    add("Cyber Escape");
#endif
#ifdef ENABLE_SEA_BATTLE
    add("Sea Battle");
#endif
    out += ']';
}

static int jsonInt(const String &body, const char *key, int def) {
    int k = body.indexOf(key);
    if (k < 0) return def;
    int colon = body.indexOf(':', k);
    if (colon < 0) return def;
    int i = colon + 1;
    while (i < (int)body.length() && (body[i] == ' ' || body[i] == '"')) i++;
    bool neg = false;
    if (i < (int)body.length() && body[i] == '-') { neg = true; i++; }
    int val = 0;
    bool any = false;
    while (i < (int)body.length() && isDigit(body[i])) {
        val = val * 10 + (body[i] - '0');
        any = true;
        i++;
    }
    if (!any) return def;
    return neg ? -val : val;
}

static int jsonBool(const String &body, const char *key, int def) {
    int k = body.indexOf(key);
    if (k < 0) return def;
    int colon = body.indexOf(':', k);
    if (colon < 0) return def;
    int i = colon + 1;
    while (i < (int)body.length() && (body[i] == ' ' || body[i] == '"')) i++;
    if (body.startsWith("true", i)) return 1;
    if (body.startsWith("false", i)) return 0;
    if (i < (int)body.length() && (body[i] == '0' || body[i] == '1')) return body[i] - '0';
    return def;
}

// =============================================================================
// REGISTRO DE RUTAS
// =============================================================================
void ConsoleAdminWeb::begin(WebServer &server) {
    if (_server == &server) return; // ya registrado
    _server = &server;

    server.on("/admin", HTTP_GET, [this]() { handleAdminPage(); });
    server.on("/config", HTTP_GET, [this]() { handleAdminPage(); });

    server.on("/api/status", HTTP_GET, [this]() { handleStatus(); });
    server.on("/api/settings", HTTP_GET, [this]() { handleSettingsGet(); });
    server.on("/api/settings", HTTP_POST, [this]() { handleSettingsPost(); });
    server.on("/api/games", HTTP_POST, [this]() { handleGamesPost(); });
    server.on("/api/restart", HTTP_POST, [this]() { handleRestart(); });

    server.on("/update", HTTP_POST,
              [this]() { handleOtaFinish(); },
              [this]() { handleOtaUpload(); });
}

void ConsoleAdminWeb::handleAdminPage() {
    _server->sendHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0", true);
    _server->sendHeader("Pragma", "no-cache", true);
    _server->send_P(200, "text/html", ADMIN_HTML);
}

void ConsoleAdminWeb::handleStatus() {
    String json;
    json.reserve(1400);

    const float v = getBatteryVoltage();
    const int pct = getBatteryPercentage();
    const uint32_t rawMv = Arduboy2Core::getBatteryRawMilliVolts();

    const esp_partition_t *running = esp_ota_get_running_partition();
    const esp_partition_t *next = esp_ota_get_next_update_partition(nullptr);
    const uint32_t appSize = running ? running->size : 0;
    const uint32_t appUsed = ESP.getSketchSize();
    const uint32_t otaSize = next ? next->size : 0;
    const uint32_t otaFree = (otaSize > appUsed) ? (otaSize - appUsed) : 0;

    json += F("{\"battery\":{\"percent\":");
    json += pct;
    json += F(",\"voltage\":");
    json += String(v, 2);
    json += F(",\"raw_mv\":");
    json += rawMv;
    json += F("},\"flash\":{\"chip\":");
    json += ESP.getFlashChipSize();
    json += F(",\"app_size\":");
    json += appSize;
    json += F(",\"app_used\":");
    json += appUsed;
    json += F(",\"ota_size\":");
    json += otaSize;
    json += F(",\"ota_free\":");
    json += otaFree;
    json += F("},\"ram\":{\"free\":");
    json += ESP.getFreeHeap();
    json += F(",\"total\":");
    json += ESP.getHeapSize();
    json += F("},\"settings\":{\"brightness\":");
    json += sysBrightness;
    json += F(",\"sound\":");
    json += (sysVolume > 0) ? 1 : 0;
    json += F("},\"uptime_ms\":");
    json += millis();
    json += F(",\"build\":\"");
    json += __DATE__;
    json += ' ';
    json += __TIME__;
    json += F("\",\"ports\":");
    appendPortsJson(json);
    json += F(",\"games\":");
    appendGamesJson(json);
    json += '}';

    sendJson(200, json);
}

void ConsoleAdminWeb::handleSettingsGet() {
    String json = F("{\"brightness\":");
    json += sysBrightness;
    json += F(",\"sound\":");
    json += (sysVolume > 0) ? 1 : 0;
    json += '}';
    sendJson(200, json);
}

void ConsoleAdminWeb::handleSettingsPost() {
    int brightness = -1;
    int sound = -1;
    if (_server->hasArg("brightness")) brightness = _server->arg("brightness").toInt();
    if (_server->hasArg("sound")) sound = _server->arg("sound").toInt();

    const String body = _server->arg("plain");
    if (body.length()) {
        brightness = jsonInt(body, "\"brightness\"", brightness);
        sound = jsonBool(body, "\"sound\"", sound);
    }

    if (brightness >= 0) {
        if (brightness < 10) brightness = 10;
        if (brightness > 100) brightness = 100;
        applyBrightness((uint8_t)brightness);
    }
    if (sound >= 0) {
        setSoundEnabled(sound != 0);
    }
    if (brightness >= 0 || sound >= 0) {
        saveSystemSettings();
    }

    String json = F("{\"ok\":true,\"brightness\":");
    json += sysBrightness;
    json += F(",\"sound\":");
    json += (sysVolume > 0) ? 1 : 0;
    json += '}';
    sendJson(200, json);
}

// Lista del menu principal con su visibilidad actual. La numeracion (id) es
// la posicion compilada en menuEntries, no la posicion visible.
void ConsoleAdminWeb::appendGamesJson(String &out) {
    const uint8_t count = menuGameCount();
    out += '[';
    for (uint8_t i = 0; i < count; i++) {
        if (i) out += ',';
        out += F("{\"id\":");
        out += i;
        out += F(",\"name\":\"");
        const char* name = menuGameLabel(i);
        if (name) {
            for (const char* p = name; *p; p++) {
                if (*p == '"' || *p == '\\') out += '\\';
                out += *p;
            }
        }
        out += F("\",\"enabled\":");
        out += menuGameEnabled(i) ? 1 : 0;
        out += '}';
    }
    out += ']';
}

void ConsoleAdminWeb::handleGamesPost() {
    int id = -1;
    int enabled = -1;
    if (_server->hasArg("id")) id = _server->arg("id").toInt();
    if (_server->hasArg("enabled")) enabled = _server->arg("enabled").toInt();

    const String body = _server->arg("plain");
    if (body.length()) {
        id = jsonInt(body, "\"id\"", id);
        enabled = jsonBool(body, "\"enabled\"", enabled);
    }

    if (id < 0 || id >= (int)menuGameCount() || enabled < 0) {
        sendJson(400, F("{\"ok\":false,\"error\":\"id/enabled invalidos\"}"));
        return;
    }

    const bool ok = menuGameSetEnabled((uint8_t)id, enabled != 0);
    String json = F("{\"ok\":");
    json += ok ? F("true") : F("false");
    if (!ok) json += F(",\"error\":\"deja al menos un cartucho visible\"");
    json += F(",\"games\":");
    appendGamesJson(json);
    json += '}';
    sendJson(ok ? 200 : 409, json);
}

void ConsoleAdminWeb::handleRestart() {
    sendJson(200, F("{\"ok\":true,\"restarting\":true}"));
    _server->client().flush();
    delay(250);
    Serial.println("[ADMIN] Reinicio remoto solicitado");
    ESP.restart();
}

// =============================================================================
// OTA
// =============================================================================
void ConsoleAdminWeb::handleOtaFinish() {
    String json;
    if (_uploadOk) {
        json = F("{\"ok\":true}");
        _restartAt = millis() + 1500;
    } else {
        json = F("{\"ok\":false,\"error\":\"");
        if (_uploadError.length() == 0) {
            _uploadError = "Subida incompleta";
        }
        for (size_t i = 0; i < _uploadError.length() && i < 120; i++) {
            char c = _uploadError[i];
            if (c == '"' || c == '\\') json += '\\';
            if (c >= 32) json += c;
        }
        json += F("\"}");
    }
    sendJson(_uploadOk ? 200 : 500, json);
}

void ConsoleAdminWeb::handleOtaUpload() {
    if (!_server) return;
    HTTPUpload &up = _server->upload();

    if (up.status == UPLOAD_FILE_START) {
        _uploadOk = false;
        _uploadError = "";
        _progress = 0;
        _restartAt = 0;
        _updating = true;
        _bodyTotal = (size_t)_server->header("Content-Length").toInt();

        melodyPlayer.stop();
        sound.noTone();

        // Si la consola estaba en standby, encender el panel para el progreso.
        Arduboy2Core::sendCommand(SSD1306_DISPLAYON);
        applyBrightness(sysBrightness);

        uint32_t maxSize = UPDATE_SIZE_UNKNOWN;
        const esp_partition_t *target = esp_ota_get_next_update_partition(nullptr);
        if (target) maxSize = target->size;

        if (!Update.begin(maxSize)) {
            _uploadError = Update.errorString();
            _updating = false;
        }
        Serial.printf("[OTA] Inicio: %s (max %u bytes)\n", up.filename.c_str(), (unsigned)maxSize);
    } else if (up.status == UPLOAD_FILE_WRITE) {
        if (!_updating) return;
        if (Update.write(up.buf, up.currentSize) != up.currentSize) {
            _uploadError = Update.errorString();
            Update.abort();
            _updating = false;
            Serial.printf("[OTA] Error de escritura: %s\n", _uploadError.c_str());
            return;
        }
        // Update.size() es el tamano de la particion cuando se usa
        // UPDATE_SIZE_UNKNOWN; para el porcentaje real usamos el
        // Content-Length del multipart y dejamos el 100% para Update.end().
        size_t total = _bodyTotal;
        if (total == 0) total = Update.size();
        if (total > 0) {
            unsigned long pct = (Update.progress() * 100ULL) / total;
            _progress = (uint8_t)(pct > 99 ? 99 : pct);
        }
    } else if (up.status == UPLOAD_FILE_END) {
        if (_updating) {
            if (Update.end(true)) {
                _uploadOk = true;
                _progress = 100;
                Serial.printf("[OTA] OK: %u bytes escritos y verificados\n", (unsigned)up.totalSize);
            } else {
                _uploadError = Update.errorString();
                Serial.printf("[OTA] Error al cerrar: %s\n", _uploadError.c_str());
            }
            _updating = false;
        }
    } else if (up.status == UPLOAD_FILE_ABORTED) {
        Update.abort();
        _updating = false;
        _uploadError = "Subida cancelada";
        Serial.println("[OTA] Subida cancelada por el cliente");
    }
}

void ConsoleAdminWeb::update() {
    if (_restartAt == 0) return;
    if ((long)(millis() - _restartAt) < 0) return;

    arduboy.fillScreen(BLACK);
    arduboy.drawRoundRect(8, 18, 112, 30, 4, WHITE);
    arduboy.setCursor(26, 26);
    arduboy.print(F("ACTUALIZADO OK"));
    arduboy.setCursor(22, 37);
    arduboy.print(F("Reiniciando..."));
    arduboy.display();
    delay(120);
    ESP.restart();
}

void ConsoleAdminWeb::drawOverlay() {
    if (!_updating) return;

    arduboy.fillRect(4, 14, 120, 38, BLACK);
    arduboy.drawRoundRect(4, 14, 120, 38, 4, WHITE);

    arduboy.setCursor(18, 19);
    arduboy.print(F("ACTUALIZANDO OTA"));

    arduboy.drawRect(14, 31, 100, 9, WHITE);
    const int w = (98 * _progress) / 100;
    if (w > 0) arduboy.fillRect(15, 32, w, 7, WHITE);

    arduboy.setCursor(56, 44);
    arduboy.print(_progress);
    arduboy.print('%');
}
