#pragma once

#include <stdint.h>

namespace Catacombs3D
{

class Entity
{
public:
	bool IsOverlappingPoint(int16_t pointX, int16_t pointY) const;
	bool IsOverlappingEntity(const Entity& other) const;

	int16_t x, y;
};

} // namespace Catacombs3D
