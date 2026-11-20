#ifndef PROJECTION_H
#define PROJECTION_H

#include <cmath>

// World <-> screen projection.
//
// The projection is deliberately ANISOTROPIC: horizontal and vertical use
// different focal lengths. That is the original FurySyrge look - 90 degrees
// horizontal FOV with walls drawn ~1.5x taller than square - and walls and
// floors are meant to stay pixel-identical to it.
//
// Sprites, pickups and hitscan must use the SAME two focal lengths as the
// walls. They used to hardcode a 0.66 camera plane instead, which made them
// ~1.5x too wide and laterally displaced relative to the walls they stood
// next to. Take focalX() / focalY() / planeLength() from here and from
// nowhere else.
namespace Projection {

constexpr float PI = 3.14159265358979f;
constexpr float FOV_DEG = 66.0f;   // only used by the floor rasterizer

// Wall / sprite horizontal focal length: screenW/2 (= 90 deg hFOV).
inline float focalX(int screenW) { return screenW * 0.5f; }

// Wall / sprite vertical focal length: screenH.
inline float focalY(int screenH) { return float(screenH); }

// Camera-plane half length for the sprite / hitscan projection:
//     screenX = screenW/2 * (1 + transformX/transformY)
// which equals (screenW/2) / focalX(screenW). With focalX = screenW/2 that is
// exactly 1.0 at every resolution, so it takes no arguments (Player::shoot
// has no screen size to pass in).
inline float planeLength() { return 1.0f; }

// Floor rasterizer vertical projection distance, (W/2)/tan(fov/2) - 924px at
// 1200 wide. Kept separate from focalY because that is what the original
// floor code used; walls use screenH (900), a 2.7% mismatch that predates
// these fixes and is left alone on purpose.
inline float floorFocalY(int screenW)
{
    return (screenW * 0.5f) / std::tan(FOV_DEG * 0.5f * PI / 180.0f);
}

} // namespace Projection

#endif // PROJECTION_H
