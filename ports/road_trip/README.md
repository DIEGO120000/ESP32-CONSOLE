# Road Trip

Port de **Road Trip** de Press Play on Tape (Filmote & Vampirics), inspirado en *Enduro*, licencia **BSD 3-Clause** (`filmote/RoadTrip`).

Carrera infinita: adelantar tráfico, gestionar la velocidad y llegar lo más lejos posible.

## Contenido

| Archivo | Descripción |
|---|---|
| `road_trip.h` | Interfaz `RoadTripGame` |
| `road_trip.cpp` | Lógica de carrera, tráfico, colisiones y render |
| `road_trip_assets.h` | Sprites, carretera y pantallas PROGMEM |
| `media/` | Portadas y capturas |

## Controles

| Entrada | Acción |
|---|---|
| LEFT / RIGHT | Dirección |
| UP o A | Acelerar |
| DOWN | Frenar |
| B | Pausa universal / salida al menú |

## Activación

```cpp
#define ENABLE_ROAD_TRIP
```

## Notas de integración

- El combo universal de salida A+B está desactivado aquí porque A acelera.
- No modifica el bus I2C ni los pines.
