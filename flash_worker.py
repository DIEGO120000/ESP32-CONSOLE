import sys, time
import esptool

print("[FLASHER] Iniciando intento de conexión con ESP32-C3...")
args = [
    '--chip', 'esp32c3',
    '--port', '/dev/ttyACM0',
    '--baud', '460800',
    '--before', 'default_reset',
    '--after', 'hard_reset',
    'write_flash', '-z',
    '--flash_mode', 'dio',
    '--flash_freq', '80m',
    '--flash_size', '4MB',
    '0x0000', '.pio/build/esp32c3_super_mini/bootloader.bin',
    '0x8000', '.pio/build/esp32c3_super_mini/partitions.bin',
    '0x10000', '.pio/build/esp32c3_super_mini/firmware.bin'
]

try:
    esptool.main(args)
    print("=============================================")
    print("🎉 ¡FIRMWARE GRABADO EXITOSAMENTE AL ESP32-C3!")
    print("=============================================")
except Exception as e:
    print("Error:", e)
