// =============================================================================
// sfx_pcm.cpp - definicion unica de los arrays del generador de efectos
// =============================================================================
// include/sfx_pcm.h solo declara los arrays (extern) para los ports. Este .cpp
// es el unico que define SFX_PCM_IMPLEMENTATION, de modo que la Flash guarda
// una sola copia de los 30 KB de muestras PCM.
// =============================================================================

#define SFX_PCM_IMPLEMENTATION
#include "sfx_pcm.h"
