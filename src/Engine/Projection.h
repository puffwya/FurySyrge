#ifndef PROJECTION_H
#define PROJECTION_H

#include <algorithm>
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

// Vertical look (mouse pitch, radians, positive = looking up): the screen
// horizon line, i.e. where world points at eye level (u = 0) land.
//     horizonY = screenH/2 + focalY * tan(pitch)
inline float horizonY(int screenH, float pitch)
{
    return screenH * 0.5f + focalY(screenH) * std::tan(pitch);
}

// --- Vertical projection with pitch -------------------------------------
//
// A world point in camera space is described by its depth and its vertical
// slope u = (z - eyeZ) / depth. Projecting it:
//
//     sy = horizon - focal * g(u),     g(u) = u * (1 + T^2) / (1 + u * T)
//     T = tan(pitch),  horizon = horizonY(screenH, pitch)
//
// This is a MOBIUS (hyperbolic) remap, not the plain y-shear the renderer
// used before (sy = horizon - focal * u). The shear only slid the pitch-0
// image down by focal*T with no perspective foreshortening, so the band of
// the world that should fill the screen got squeezed into it and stretched:
// at 40 deg up, a shear shows ~2 world units of a wall where a real camera
// shows ~4.5 - "everything looks stretched". g() is exactly the vertical
// mapping a true camera rotation produces, so the scale now matches.
//
// Horizontal projection still uses the UNROTATED depth, which is what keeps
// vertical faces vertical: a true rotation tilts them (they converge toward
// the vanishing point), which a column renderer cannot express.
//
// Properties, all relied on by the callers:
//  - T = 0 gives g(u) = u, so the pitch-0 image is unchanged.
//  - u = 0 lands on horizonY, so the horizon and the ceiling fill agree.
//  - g is strictly increasing on the valid side, so screen Y stays monotone
//    in world height (the top of a face always maps above its bottom).
//  - The screen centre row still maps to u = T: the aim ray through the
//    crosshair keeps slope tan(pitch), so Player::shoot needs no change.
//
// u on the far side of 1 + u*T <= 0 is behind the pitched image plane and
// would wrap to the wrong side of the screen; projectY clamps it to a huge
// off-screen Y on the CORRECT side instead, which the callers' span culling
// then reads as "drawn up to the screen edge" or "culled entirely".
//
// Pass T = std::tan(pitch) once per pass; these run per pixel row.
inline float projectY(float u, float T, float horizon, float focal)
{
    float d = 1.0f + u * T;
    if (d < 1e-4f) d = 1e-4f; // behind the pitched image plane
    const float sy = horizon - focal * (u * (1.0f + T * T) / d);
    // Keep the result int-safe for callers that do int(sy): an endpoint far
    // behind the plane can otherwise reach ~1e9 and overflow.
    return std::clamp(sy, -1e6f, 1e6f);
}

// Inverse of projectY: screen Y -> u. Used where a rendered row must be
// mapped back to a world height (wall texture V, floor distance, sprite
// texcoords); with pitch that mapping is hyperbolic, not linear.
// Only called for on-screen rows, where the denominator stays positive.
inline float unprojectY(float sy, float T, float horizon, float focal)
{
    const float g = (horizon - sy) / focal; // = g(u)
    float d = 1.0f + T * T - g * T;
    if (d < 1e-4f) d = 1e-4f;
    return g / d;
}

} // namespace Projection

#endif // PROJECTION_H
