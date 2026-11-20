#ifndef PROJECTION_H
#define PROJECTION_H

#include <cmath>

// Single source of truth for the world <-> screen projection.
//
// Walls, floors, sprites, pickups and hitscan all have to agree on these two
// numbers. They didn't: walls used screenW/2 (600) for X and screenH (900) for
// Y, floors used (screenW/2)/tan(fov/2) (924), sprites used a 0.66 camera
// plane (909). The result was a vertically stretched world with sprites that
// were ~1.5x too wide and laterally displaced relative to the walls.
namespace Projection {

constexpr float PI = 3.14159265358979f;
constexpr float FOV_DEG = 66.0f;              // horizontal field of view
constexpr float FOV_RAD = FOV_DEG * PI / 180.0f;

// Distance from the eye to the projection plane, in pixels. Used for every
// screen-space projection: screenY = screenH/2 - (z - eyeZ) * focal / depth.
inline float focalLength(int screenW)
{
    return (screenW * 0.5f) / std::tan(FOV_RAD * 0.5f);
}

// Camera-plane half length for the sprite / hitscan projection:
//     screenX = screenW/2 * (1 + transformX / transformY)
// which is equivalent to focalLength() when planeLength() == (screenW/2)/focal.
inline float planeLength()
{
    return std::tan(FOV_RAD * 0.5f);
}

} // namespace Projection

#endif // PROJECTION_H
