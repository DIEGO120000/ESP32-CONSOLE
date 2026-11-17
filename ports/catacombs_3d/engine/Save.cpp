#include <Arduino.h>
#include <Preferences.h>

#include "Save.h"

namespace Catacombs3D
{

namespace Save
{

static uint16_t bestScore = 0;

void Begin()
{
	Preferences prefs;
	if (prefs.begin("catacombs", true))
	{
		bestScore = prefs.getUShort("best", 0);
		prefs.end();
	}
}

uint16_t Best()
{
	return bestScore;
}

void Submit(uint16_t score)
{
	if (score <= bestScore)
		return;

	bestScore = score;

	Preferences prefs;
	if (prefs.begin("catacombs", false))
	{
		prefs.putUShort("best", bestScore);
		prefs.end();
	}
}

} // namespace Save

} // namespace Catacombs3D
