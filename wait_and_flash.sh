#!/usr/bin/env bash
echo "=== ESP32-C6 AUTO-FLASH LISTENER ACTIVADO ==="
echo "Esperando a que conectes la ESP32-C6 en modo BOOT..."
echo "(Mantén presionado BOOT, conecta el USB o presiona RST, y suelta BOOT)"

while true; do
    PORT=$(ls /dev/ttyACM* /dev/ttyUSB* 2>/dev/null | head -n 1)
    if [ -n "$PORT" ]; then
        echo ""
        echo ">>> Dispositivo detectado en $PORT! <<<"
        echo "Iniciando flasheo inmediato..."
        sleep 0.5
        pio run -e esp32c6_super_mini -t upload --upload-port "$PORT"
        RET=$?
        if [ $RET -eq 0 ]; then
            echo ""
            echo "==========================================="
            echo ">>> ¡FLASHEO COMPLETADO EXITOSAMENTE! <<<"
            echo "==========================================="
            exit 0
        else
            echo "El intento de flasheo devolvió código $RET. Reintentando si el puerto sigue presente..."
            sleep 1
        fi
    fi
    sleep 0.5
done
