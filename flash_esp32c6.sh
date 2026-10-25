#!/bin/bash
echo "[AUTO-FLASHER] Esperando activamente a que el ESP32-C6 Super Mini se conecte en /dev/ttyACM* o /dev/ttyUSB*..."

while true; do
    PORT=""
    for p in /dev/ttyACM* /dev/ttyUSB*; do
        if [ -e "$p" ]; then
            PORT="$p"
            break
        fi
    done

    if [ -n "$PORT" ]; then
        echo "[AUTO-FLASHER] 🚀 ¡Dispositivo detectado en $PORT! Flasheando firmware ESP32-C6..."
        
        # Give permission if needed
        sudo chmod 666 "$PORT" 2>/dev/null || true
        
        /home/diego/.platformio/penv/bin/esptool --chip esp32c6 --port "$PORT" --baud 115200 \
            --connect-attempts 1 --before no-reset --after hard-reset write-flash -z \
            --flash-mode dio --flash-freq 80m --flash-size 4MB \
            0x0000 .pio/build/esp32c6_super_mini/bootloader.bin \
            0x8000 .pio/build/esp32c6_super_mini/partitions.bin \
            0xe000 /home/diego/.platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin \
            0x10000 .pio/build/esp32c6_super_mini/firmware.bin
        
        EXIT_CODE=$?
        if [ $EXIT_CODE -eq 0 ]; then
            echo "[AUTO-FLASHER] ✅ ¡FLASHEO COMPLETADO EXITOSAMENTE A LA PLACA ESP32-C6!"
            exit 0
        else
            echo "[AUTO-FLASHER] Error durante la transferencia. Reintentando..."
            sleep 1
        fi
    fi
    sleep 0.4
done
