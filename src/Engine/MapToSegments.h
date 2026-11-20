#pragma once
#include <vector>
#include "Map.h"

struct Vec2 { float x, y; };

struct GridSegment {
    Vec2 a, b;      // endpoints in world coordinates
    int tileX, tileY;  

    float frontHeight;   // build-time snapshot of map.get(tileX,tileY).height
    float backHeight;    // build-time snapshot of the neighbour's height
                         // (the renderer reads heights live from Map instead)

    // Optional, but VERY useful for debugging:
    enum class Dir { North, South, West, East } dir;
};

// Produces a compact set of segments from grid walls.
// - map.get(x,y).type == Wall are considered walls.
// - cell size is 1.0 unit per tile (world units).
// Each boundary whose two surfaces differ in height yields exactly one
// segment, owned by the higher tile; equal-height boundaries yield none.
std::vector<GridSegment> buildSegmentsFromGrid(const Map& map);
