#!/usr/bin/env python3
# =============================================================================
# build_sfx.py - genera include/sfx_pcm.h con efectos PCM reales
# =============================================================================
# Sintetiza los nueve efectos con DSP en Python (numpy): ruido blanco/rosa
# filtrado con biquads RBJ, barridos de frecuencia, formantes vocales y
# envolventes ADSR. Despues normaliza a WAV PCM unsigned 8-bit mono 8000 Hz
# usando ffmpeg (si esta disponible) y escribe los arrays en un header PROGMEM.
#
# Uso:  python3 tools/build_sfx.py
# Requisitos: numpy (obligatorio), ffmpeg (recomendado, se usa para el
# remuestreo/quantizacion final; si falta se cuantiza directamente en Python).
# =============================================================================

import os
import subprocess
import sys
import tempfile
import wave

import numpy as np

SR = 8000
RNG = np.random.default_rng(0xC6C0DE)


# -----------------------------------------------------------------------------
# Filtros biquad (RBJ cookbook), sin dependencia de scipy
# -----------------------------------------------------------------------------
def _biquad(x, b0, b1, b2, a1, a2):
    y = np.zeros_like(x)
    x1 = x2 = y1 = y2 = 0.0
    for i, xi in enumerate(x):
        yi = b0 * xi + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2
        x2, x1 = x1, xi
        y2, y1 = y1, yi
        y[i] = yi
    return y


def _coeffs(kind, freq, q):
    w0 = 2.0 * np.pi * freq / SR
    alpha = np.sin(w0) / (2.0 * q)
    cw = np.cos(w0)
    if kind == "lp":
        b0 = (1 - cw) / 2
        b1 = 1 - cw
        b2 = (1 - cw) / 2
    elif kind == "hp":
        b0 = (1 + cw) / 2
        b1 = -(1 + cw)
        b2 = (1 + cw) / 2
    else:  # band-pass (0 dB peak)
        b0, b1, b2 = alpha, 0.0, -alpha
    a0 = 1 + alpha
    return b0 / a0, b1 / a0, b2 / a0, (-2 * cw) / a0, (1 - alpha) / a0


def lowpass(x, freq, q=0.707):
    return _biquad(x, *_coeffs("lp", freq, q))


def highpass(x, freq, q=0.707):
    return _biquad(x, *_coeffs("hp", freq, q))


def bandpass(x, freq, q=1.0):
    return _biquad(x, *_coeffs("bp", freq, q))


def env_exp(n, decay, attack=0.002):
    t = np.arange(n) / SR
    return (1.0 - np.exp(-t / max(attack, 1e-4))) * np.exp(-t * decay)


def adsr(n, attack=0.005, decay=0.08, sustain=0.5, release=0.15):
    a = int(attack * SR)
    d = int(decay * SR)
    r = int(release * SR)
    s = max(0, n - a - d - r)
    parts = [
        np.linspace(0, 1, a, endpoint=False) if a else np.array([]),
        np.linspace(1, sustain, d, endpoint=False) if d else np.array([]),
        np.full(s, sustain),
        np.linspace(sustain, 0, r, endpoint=False) if r else np.array([]),
    ]
    e = np.concatenate(parts)
    if len(e) < n:
        e = np.pad(e, (0, n - len(e)))
    return e[:n]


def sweep_sine(n, f_start, f_end, curve=1.0):
    t = np.arange(n) / SR
    k = np.exp(-curve * t)
    f = f_end + (f_start - f_end) * k
    phase = 2.0 * np.pi * np.cumsum(f) / SR
    return np.sin(phase)


def saw_from_phase(phase):
    return 2.0 * ((phase / (2.0 * np.pi)) % 1.0) - 1.0


# -----------------------------------------------------------------------------
# Efectos
# -----------------------------------------------------------------------------
def golf_roll(dur=0.5):
    """Ruido paso-banda grave con modulacion lenta: bola rodando/raspando."""
    n = int(SR * dur)
    t = np.arange(n) / SR
    noise = RNG.standard_normal(n)

    body = bandpass(noise, 230, 1.1) + 0.7 * bandpass(noise, 95, 2.0)
    grain = bandpass(noise, 1400, 2.5) * np.maximum(0, np.sin(2 * np.pi * 3.1 * t))
    x = body * (0.55 + 0.45 * np.sin(2 * np.pi * 6.8 * t)) \
        * (0.70 + 0.30 * np.sin(2 * np.pi * 17.3 * t + 1.1)) \
        + 0.18 * grain

    # Crossfade circular para que el bucle no tenga clic.
    fade = 220
    head = x[:fade].copy()
    tail = x[-fade:].copy()
    x[:fade] = head * np.linspace(0, 1, fade) + tail * np.linspace(1, 0, fade)
    x = x[:-fade]
    return x


def blaster(dur=0.3):
    """'Pichiu' laser: chirrido descendente con parciales metalicos."""
    n = int(SR * dur)
    t = np.arange(n) / SR
    x = sweep_sine(n, 1900, 240, 8.0)
    x += 0.55 * sweep_sine(n, 2600, 420, 11.0)
    x += 0.28 * sweep_sine(n, 3800, 760, 14.0)
    x += 0.22 * np.sin(2 * np.pi * 2450 * t) * np.exp(-t * 13)
    return x * env_exp(n, 6.5, 0.001)


def explosion(dur=0.65):
    """Impacto grave + cola de ruido blanco filtrado paso-bajo."""
    n = int(SR * dur)
    t = np.arange(n) / SR
    noise = RNG.standard_normal(n)
    tail = lowpass(noise, 420, 0.8) * np.exp(-t * 3.0)
    tail += 0.55 * lowpass(noise, 1300, 0.7) * np.exp(-t * 8.0)
    thump = np.sin(2 * np.pi * 52 * t) * np.exp(-t * 8.5)
    thump += 0.5 * np.sin(2 * np.pi * 34 * t) * np.exp(-t * 5.0)
    x = 0.95 * tail + 0.9 * thump
    # Crepitar del frente de explosion.
    for _ in range(18):
        p = int(RNG.uniform(0.002, 0.30) * SR)
        ln = int(SR * 0.006)
        if p + ln < n:
            x[p:p + ln] += RNG.standard_normal(ln) * np.exp(-np.arange(ln) / (ln / 3)) * 0.5
    return x * env_exp(n, 0.7, 0.0008)


def wolf_shot(dur=0.3):
    """Disparo seco: transiente de impacto + ruido explosivo corto."""
    n = int(SR * dur)
    t = np.arange(n) / SR
    noise = RNG.standard_normal(n)
    crack = noise * np.exp(-t * 90)
    body = lowpass(noise, 1600, 0.8) * np.exp(-t * 15)
    thump = np.sin(2 * np.pi * 120 * t) * np.exp(-t * 26)
    ring = np.sin(2 * np.pi * 3100 * t) * np.exp(-t * 40) * 0.35
    return (0.75 * crack + 0.9 * body + 0.8 * thump + ring) * env_exp(n, 1.8, 0.0004)


def wolf_death(dur=0.45):
    """Quejido grave descendente con formantes vocales ('ough')."""
    n = int(SR * dur)
    t = np.arange(n) / SR
    f0 = 62 + 105 * np.exp(-t * 3.2)
    phase = 2.0 * np.pi * np.cumsum(f0) / SR
    source = saw_from_phase(phase)
    voice = 0.9 * bandpass(source, 430, 3.5) + 0.75 * bandpass(source, 780, 4.5)
    voice += 0.25 * lowpass(source, 180, 0.7)
    breath = lowpass(RNG.standard_normal(n), 700, 0.8) * 0.25
    x = voice + breath
    return x * adsr(n, 0.012, 0.10, 0.55, 0.20) * (0.8 + 0.2 * np.sin(2 * np.pi * 7 * t))


def wolf_gulp(dur=0.45):
    """'Glup-glup' liquido: tres resonancias con caida de tono."""
    n = int(SR * dur)
    t = np.arange(n) / SR
    x = np.zeros(n)
    for start, f_hi, f_lo, amp in ((0.00, 620, 190, 1.0),
                                   (0.13, 520, 160, 0.85),
                                   (0.27, 430, 130, 0.70)):
        p = int(start * SR)
        ln = int(0.11 * SR)
        if p + ln >= n:
            ln = n - p
        if ln <= 0:
            continue
        seg_t = np.arange(ln) / SR
        sweep = sweep_sine(ln, f_hi, f_lo, 10.0)
        bubble = sweep * np.exp(-seg_t * 26)
        blub = bandpass(RNG.standard_normal(ln), 700, 2.2) * np.exp(-seg_t * 34) * 0.45
        x[p:p + ln] += (bubble + blub) * amp
    return x * adsr(n, 0.005, 0.05, 0.6, 0.10)


def wolf_coin(dur=0.3):
    """Tintineo metalico agudo ('cling')."""
    n = int(SR * dur)
    t = np.arange(n) / SR
    x = np.zeros(n)
    for f, a, d in ((2093, 1.0, 7.0), (2637, 0.7, 9.0),
                    (3136, 0.55, 11.0), (4186, 0.35, 14.0)):
        x += a * np.sin(2 * np.pi * f * t) * np.exp(-t * d)
    x += 0.18 * bandpass(RNG.standard_normal(n), 3400, 5.0) * np.exp(-t * 30)
    return x * env_exp(n, 1.0, 0.0006)


def wolf_reload(dur=0.4):
    """Doble chasquido de cerrojo: click metalico + ring corto."""
    n = int(SR * dur)
    x = np.zeros(n)
    for start, amp in ((0.015, 1.0), (0.19, 0.9), (0.26, 0.65)):
        p = int(start * SR)
        ln = int(0.055 * SR)
        if p + ln >= n:
            ln = n - p
        if ln <= 0:
            continue
        t = np.arange(ln) / SR
        click = RNG.standard_normal(ln) * np.exp(-t * 220)
        metal = bandpass(RNG.standard_normal(ln), 2400, 4.0) * np.exp(-t * 55)
        ring = np.sin(2 * np.pi * 1750 * t) * np.exp(-t * 40) * 0.5
        x[p:p + ln] += (0.8 * click + 0.9 * metal + ring) * amp
    return x * adsr(n, 0.001, 0.02, 0.9, 0.05)


def fire_flame(dur=0.55):
    """'Fishiuu': barrido de ruido filtrado + crepitar de fuego."""
    n = int(SR * dur)
    t = np.arange(n) / SR
    noise = RNG.standard_normal(n)
    # Paso-bajo de un polo con corte descendente 4.5 kHz -> 350 Hz.
    y = np.zeros(n)
    prev = 0.0
    for i in range(n):
        cutoff = 350 + 4200 * np.exp(-t[i] * 5.5)
        alpha = 1.0 - np.exp(-2.0 * np.pi * cutoff / SR)
        prev += alpha * (noise[i] - prev)
        y[i] = prev
    whoosh = highpass(y, 180, 0.7) * (0.55 + 0.45 * np.sin(2 * np.pi * 3.5 * t))
    # Crepitar: chispas de ruido de banda ancha.
    crackle = np.zeros(n)
    for _ in range(45):
        p = int(RNG.uniform(0, 0.52) * SR)
        ln = int(SR * 0.004)
        if p + ln < n:
            crackle[p:p + ln] += RNG.standard_normal(ln) * np.exp(-np.arange(ln) / (ln / 2.5)) * 0.6
    x = whoosh + 0.35 * bandpass(crackle, 2200, 1.5)
    return x * adsr(n, 0.008, 0.25, 0.5, 0.22)


EFFECTS = [
    ("SFX_GOLF_ROLL",  golf_roll),
    ("SFX_BLASTER",    blaster),
    ("SFX_EXPLOSION",  explosion),
    ("SFX_WOLF_SHOT",  wolf_shot),
    ("SFX_WOLF_DEATH", wolf_death),
    ("SFX_WOLF_GULP",  wolf_gulp),
    ("SFX_WOLF_COIN",  wolf_coin),
    ("SFX_WOLF_RELOAD", wolf_reload),
    ("SFX_FIRE_FLAME", fire_flame),
]


# -----------------------------------------------------------------------------
# Cuantizacion a unsigned 8-bit mono 8000 Hz (ffmpeg si esta disponible)
# -----------------------------------------------------------------------------
def to_u8(x):
    x = np.asarray(x, dtype=np.float64)
    x = x - np.mean(x)
    peak = np.max(np.abs(x))
    if peak > 1e-9:
        x = x / peak * 0.92
    return np.clip(np.round(x * 127.0 + 128.0), 0, 255).astype(np.uint8)


def quantize_with_ffmpeg(x):
    """Pasa por ffmpeg a pcm_u8 8 kHz; si falla, cuantiza en Python."""
    pcm16 = np.clip(np.round(x / max(np.max(np.abs(x)), 1e-9) * 0.92 * 32767.0),
                    -32768, 32767).astype("<i2")
    tmpdir = tempfile.mkdtemp(prefix="sfx_")
    src = os.path.join(tmpdir, "in.wav")
    dst = os.path.join(tmpdir, "out.wav")
    with wave.open(src, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(SR)
        w.writeframes(pcm16.tobytes())
    try:
        subprocess.run(
            ["ffmpeg", "-y", "-loglevel", "error", "-i", src,
             "-ar", str(SR), "-ac", "1", "-c:a", "pcm_u8", dst],
            check=True,
        )
        with wave.open(dst, "rb") as w:
            frames = w.readframes(w.getnframes())
        return np.frombuffer(frames, dtype=np.uint8)
    except (OSError, subprocess.CalledProcessError, wave.Error) as exc:
        print(f"  ! ffmpeg no disponible ({exc}); cuantizando en Python")
        return to_u8(x)
    finally:
        for path in (src, dst):
            try:
                os.remove(path)
            except OSError:
                pass
        try:
            os.rmdir(tmpdir)
        except OSError:
            pass


# -----------------------------------------------------------------------------
# Generacion del header
# -----------------------------------------------------------------------------
def write_header(path, samples):
    total = sum(len(data) for _, data in samples)
    lines = []
    lines.append("// =============================================================================")
    lines.append("// sfx_pcm.h - GENERADO AUTOMATICAMENTE por tools/build_sfx.py")
    lines.append("// No editar a mano: ejecuta `python3 tools/build_sfx.py`.")
    lines.append("//")
    lines.append(f"// {len(samples)} efectos PCM unsigned 8-bit, mono, {SR} Hz (uno por cada")
    lines.append(f"// fila del array): {total} bytes en total.")
    lines.append("// =============================================================================")
    lines.append("#pragma once")
    lines.append("#include <Arduino.h>")
    lines.append("")
    lines.append("// Definir SFX_PCM_IMPLEMENTATION en UN solo .cpp (src/sfx_pcm.cpp) para")
    lines.append("// emitir los arrays; el resto de modulos solo ve las declaraciones y la")
    lines.append("// Flash guarda una unica copia.")
    for name, data in samples:
        lines.append(f"#define {name}_LEN {len(data)}u")
    lines.append("")
    lines.append("#ifdef SFX_PCM_IMPLEMENTATION")
    for name, data in samples:
        # `extern` en la definicion fuerza enlace externo: sin el, una variable
        # const de ambito de namespace tiene enlace interno y el linker no la
        # ve desde los ports.
        lines.append(f"extern const uint8_t {name}[] PROGMEM = {{")
        for i in range(0, len(data), 16):
            chunk = ", ".join(f"0x{b:02X}" for b in data[i:i + 16])
            lines.append(f"    {chunk},")
        lines.append("};")
        lines.append("")
    lines.append("#else")
    for name, _ in samples:
        lines.append(f"extern const uint8_t {name}[] PROGMEM;")
    lines.append("#endif")
    lines.append("")
    with open(path, "w", encoding="ascii") as f:
        f.write("\n".join(lines))
    return total


def main():
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    out_path = os.path.join(root, "include", "sfx_pcm.h")
    samples = []

    print(f"Sintetizando {len(EFFECTS)} efectos a {SR} Hz...")
    for name, fn in EFFECTS:
        raw = fn()
        pcm = quantize_with_ffmpeg(raw)
        samples.append((name, pcm))
        print(f"  {name:16s} {len(pcm):5d} muestras  "
              f"{len(pcm) / SR:4.2f} s  {len(pcm):5d} bytes")

    total = write_header(out_path, samples)
    print(f"\nEscrito {out_path}: {total} bytes ({total / 1024:.1f} KB) de PCM.")


if __name__ == "__main__":
    sys.exit(main())
