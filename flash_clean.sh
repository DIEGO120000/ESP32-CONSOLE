#!/bin/bash
echo "[READY] Esperando conexión limpia de la placa ESP32-C3..."

while true; do
    if [ -e "/dev/ttyACM0" ]; then
        echo "[CONNECT] Placa conectada en /dev/ttyACM0. Grabando firmware ahora..."
        sleep 0.3
        /home/diego/.platformio/penv/bin/esptool --chip esp32c3 --port /dev/ttyACM0 --baud 460800 \
            --before default-reset --after hard-reset write_flash -z \
            --flash_mode dio --flash_freq 80m --flash_size 4MB \
            0x0000 .pio/build/esp32c3_super_mini/bootloader.bin \
            0x8000 .pio/build/esp32c3_super_mini/partitions.bin \
            0x10000 .pio/build/esp32c3_super_mini/firmware.bin
        
        EXIT_CODE=$?
        if [ $EXIT_CODE -eq 0 ]; then
            echo "=================================================="
            echo "🎉 ¡EXITO! FIRMWARE FLASHEADO AL 100% CORRECTAMENTE"
            echo "=================================================="
            exit 0
        else
            echo "[REINTENTO] Intentando con modo directo no-reset..."
            /home/diego/.platformio/penv/bin/esptool --chip esp32c3 --port /dev/ttyACM0 --baud 460800 \
                --before no-reset --after hard-reset write_flash -z \
                --flash_mode dio --flash_freq 80m --flash_size 4MB \
                0x0000 .pio/build/esp32c3_super_mini/bootloader.bin \
                0x8000 .pio/build/esp32c3_super_mini/partitions.bin \
                0x10000 .pio/build/esp32c3_super_mini/firmware.bin
            if [ $? -eq 0 ]; then
                echo "=================================================="
                echo "🎉 ¡EXITO! FIRMWARE FLASHEADO AL 100% CORRECTAMENTE"
                echo "=================================================="
                exit 0
            fi
        fi
    fi
    sleep 0.5
done
