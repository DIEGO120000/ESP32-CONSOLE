#!/bin/bash
echo "[WAIT] Esperando a que el cable USB sea reconectado..."
while true; do
    if [ -e "/dev/ttyACM0" ]; then
        echo "[CONNECT] ¡ESP32 detectado en /dev/ttyACM0! Grabando firmware..."
        sleep 0.5
        /home/diego/.platformio/penv/bin/esptool --chip esp32c3 --port /dev/ttyACM0 --baud 460800 \
            --before default-reset --after hard-reset write_flash -z \
            --flash_mode dio --flash_freq 80m --flash_size 4MB \
            0x0000 .pio/build/esp32c3_super_mini/bootloader.bin \
            0x8000 .pio/build/esp32c3_super_mini/partitions.bin \
            0x10000 .pio/build/esp32c3_super_mini/firmware.bin
        if [ $? -eq 0 ]; then
            echo "================================================="
            echo "🎉 ¡EXITO! FIRMWARE GRABADO COMPLETAMENTE AL ESP32-C3"
            echo "================================================="
            exit 0
        fi
    fi
    sleep 0.4
done
