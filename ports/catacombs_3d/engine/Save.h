#pragma once

// =============================================================================
// Persistencia del record de Catacombs of the Damned.
//
// El original de Arduboy no guardaba progreso: la puntuacion final solo se
// mostraba en pantalla. En la consola se conserva el mejor marcador en NVS
// (namespace "catacombs"), que en el emulador de PC se vuelca a sim_nvs.dat.
// =============================================================================

#include <stdint.h>

namespace Catacombs3D
{

namespace Save
{
	void Begin();
	uint16_t Best();
	void Submit(uint16_t score);
}

} // namespace Catacombs3D
