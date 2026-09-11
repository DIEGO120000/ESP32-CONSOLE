#!/bin/bash
echo "[FAST-FLASHER] Listo esperando Bootloader en /dev/ttyACM0..."

while true; do
    if [ -e "/dev/ttyACM0" ]; then
        /home/diego/.platformio/penv/bin/esptool --chip esp32c3 --port /dev/ttyACM0 --baud 460800 \
            --connect-attempts 1 --before no-reset --after hard-reset write_flash -z \
            --flash_mode dio --flash_freq 80m --flash_size 4MB \
            0x0000 .pio/build/esp32c3_super_mini/bootloader.bin \
            0x8000 .pio/build/esp32c3_super_mini/partitions.bin \
            0x10000 .pio/build/esp32c3_super_mini/firmware.bin 2>/dev/null
        
        if [ $? -eq 0 ]; then
            echo "=================================================="
            echo "🎉 ¡CONSOLA GRABADA Y LISTA CON ÉXITO!"
            echo "=================================================="
            exit 0
        fi
    fi
    sleep 0.15
done
