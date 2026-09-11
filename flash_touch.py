import serial, time, os
import esptool

print("[1200-BAUD-TOUCH] Enviando señal de reinicio a modo Bootloader...")
try:
    s = serial.Serial('/dev/ttyACM0', 1200)
    s.dtr = False
    s.rts = False
    time.sleep(0.1)
    s.close()
    print("[1200-BAUD-TOUCH] Señal enviada. Esperando re-enumeración USB...")
except Exception as e:
    print("[1200-BAUD-TOUCH] Nota:", e)

# Wait up to 3 seconds for the port to be ready in Bootloader mode
for i in range(15):
    time.sleep(0.2)
    if os.path.exists('/dev/ttyACM0'):
        print(f"[BOOTLOADER] Puerto detectado en {i*0.2:.1f}s. Flasheando...")
        break

time.sleep(0.3)
args = [
    '--chip', 'esp32c3',
    '--port', '/dev/ttyACM0',
    '--baud', '460800',
    '--before', 'no-reset',
    '--after', 'hard-reset',
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
    print("==================================================")
    print("🎉 ¡FIRMWARE FLASHEADO CON ÉXITO A TU ESP32-C3!")
    print("==================================================")
except Exception as e:
    print("Error:", e)
