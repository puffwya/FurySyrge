#include "DoomRenderer.h"
#include "Projection.h"
#include <cmath>
#include <algorithm>
#include <cassert>
#include <limits>
#include <iostream>

// constants
static constexpr float EPS = 1e-6f;
static constexpr float WALL_WORLD_HEIGHT = 1.0f; // world units for a full-height wall

SpriteRenderer spriteRenderer;

// Vertical extent of the face between a tile and the tile across the segment.
//
// Map::height is an ABSOLUTE top elevation on both sides (0 = floor,
// 1 = full wall, 0<h<1 = step, <0 = pit), so a face always spans
// [min(hA, hB), max(hA, hB)]. Returns false when the two surfaces are
// coplanar, i.e. there is no vertical face at all.
static bool segmentFaceExtents(const Map& map, const GridSegment& seg,
                               int tileX, int tileY,
                               float& floorZ, float& ceilZ)
{
    // Neighbour across the segment, derived from geometry (not `dir`) so it
    // stays correct for segments the BSP has split in two.
    int nx = tileX;
    int ny = tileY;
    if (std::fabs(seg.a.x - seg.b.x) < 1e-4f) {          // vertical edge on x = seg.a.x
        nx = (seg.a.x < tileX + 0.5f) ? tileX - 1 : tileX + 1;
    } else {                                              // horizontal edge on y = seg.a.y
        ny = (seg.a.y < tileY + 0.5f) ? tileY - 1 : tileY + 1;
    }

    const float hA = map.get(tileX, tileY).height;
    // Map::get clamps out-of-range coordinates, so map-border tiles compare
    // against themselves and correctly produce no outward face.
    const float hB = map.get(nx, ny).height;

    floorZ = std::min(hA, hB);
    ceilZ  = std::max(hA, hB);
    return (ceilZ - floorZ) > 1e-4f;
}

DoomRenderer::DoomRenderer(const std::vector<GridSegment>& segments,
                           std::unique_ptr<BSPNode> bspRoot)
    : m_segments(segments), m_bspRoot(std::move(bspRoot))
{
}

// Bullet holes live on a tile face, identified by (tileX, tileY, dir) plus the
// fraction along that face as recorded by the shooter. They are drawn once per
// frame, after the world, clipped to the face they belong to and depth tested
// so they can never land on nearer geometry (they used to be alpha-blended once
// per screen column of the owning segment, with no depth test at all).
void DoomRenderer::drawBulletHoles(
    uint32_t* pixels,
    float* zBuffer,
    int screenW,
    int screenH,
    const Player& player,
    const Map& map,
    BulletHoleManager& bulletHoleManager
) {
    const float focal = Projection::focalLength(screenW);
    const float cx = screenW * 0.5f;
    const float cy = screenH * 0.5f;

    for (const BulletHole& hole : bulletHoleManager.getAll()) {
        const int tx = hole.tileX;
        const int ty = hole.tileY;
        const float f = std::clamp(hole.hitFraction, 0.0f, 1.0f);

        // World position of the impact on the face. The fraction orientation
        // recorded by Player/raycastWall differs per face direction, so it is
        // resolved explicitly here instead of assuming seg.a -> seg.b.
        float hx = 0.0f, hy = 0.0f;
        int nx = tx, ny = ty;
        switch (hole.dir) {
            case GridSegment::Dir::West:  hx = float(tx);         hy = float(ty + 1) - f; break;
            case GridSegment::Dir::East:  hx = float(tx + 1);     hy = float(ty) + f;     break;
            case GridSegment::Dir::North: hx = float(tx + 1) - f; hy = float(ty);         break;
            case GridSegment::Dir::South: hx = float(tx + 1) - f; hy = float(ty + 1);     break;
        }
        switch (hole.dir) { // neighbour across the face
            case GridSegment::Dir::North: ny = ty - 1; break;
            case GridSegment::Dir::South: ny = ty + 1; break;
            case GridSegment::Dir::West:  nx = tx - 1; break;
            case GridSegment::Dir::East:  nx = tx + 1; break;
        }

        float camX, camY;
        if (!projectPointToCamera(hx, hy, player, camX, camY)) continue;
        if (camY < 1e-4f) continue;

        // Same face extents as the wall renderer
        const float hA = map.get(tx, ty).height;
        const float hB = map.get(nx, ny).height;
        const float floorZ = std::min(hA, hB);
        const float ceilZ  = std::max(hA, hB);
        if (ceilZ - floorZ <= 1e-4f) continue;

        const int px = int(cx + (camX / camY) * focal + 0.5f);
        if (px < 0 || px >= screenW) continue;

        // Vertical span of the face at this depth (same rounding as the wall)
        const float faceCeilY   = cy - (ceilZ  - player.z) * focal / camY;
        const float faceFloorY  = cy - (floorZ - player.z) * focal / camY;
        const int   faceTopRow    = std::max(0, int(std::ceil(std::min(faceCeilY, faceFloorY))));
        const int   faceBottomRow = std::min(screenH - 1, int(std::floor(std::max(faceCeilY, faceFloorY))));
        if (faceBottomRow < faceTopRow) continue;

        // Impact at eye height projects onto the horizon; holeZ carries the
        // shooter's eye height, so this is the real projection of that point.
        const int midY = int(cy - (hole.holeZ - player.z) * focal / camY) + hole.verticalOffset;

        const auto& visual = bulletHoleManager.getVisual(hole.type);
        const int texW = visual.w;
        const int texH = visual.h;

        const int x0 = px - texW / 2;
        const int y0 = midY - texH / 2;

        for (int row = 0; row < texH; ++row) {
            const int py = y0 + row;
            if (py < faceTopRow || py > faceBottomRow) continue; // clip to the face
            if (py < 0 || py >= screenH) continue;

            for (int col = 0; col < texW; ++col) {
                const int pxx = x0 + col;
                if (pxx < 0 || pxx >= screenW) continue;

                const int idx = py * screenW + pxx;
                if (zBuffer[idx] + 1e-3f < camY) continue; // something nearer in front

                const uint32_t texPixel = visual.pixels[row * texW + col];
                const uint8_t alpha = texPixel >> 24;
                if (alpha == 0) continue;

                const uint32_t dst = pixels[idx];

                const uint8_t srcR = (texPixel >> 16) & 0xFF;
                const uint8_t srcG = (texPixel >> 8) & 0xFF;
                const uint8_t srcB = texPixel & 0xFF;

                const uint8_t dstR = (dst >> 16) & 0xFF;
                const uint8_t dstG = (dst >> 8) & 0xFF;
                const uint8_t dstB = dst & 0xFF;

                const float a = alpha / 255.0f;
                const uint8_t outR = uint8_t(srcR * a + dstR * (1.0f - a));
                const uint8_t outG = uint8_t(srcG * a + dstG * (1.0f - a));
                const uint8_t outB = uint8_t(srcB * a + dstB * (1.0f - a));

                pixels[idx] = (0xFFu << 24) | (outR << 16) | (outG << 8) | outB;
            }
        }
    }
}

void DoomRenderer::renderWorldTileRasterized(uint32_t* pixels, float* zBuffer, int screenW, int screenH,
                                             const Player& player,
                                             float wx, float wy, float sizeWorld, float tileHeight,
                                             const Texture& floorTex, const Map& map)
{
    if (tileHeight == 1.0f) return; // no need to render full-height wall tops

    const float minX = wx;
    const float maxX = wx + sizeWorld;
    const float minY = wy;
    const float maxY = wy + sizeWorld;

    const float sa = std::sin(player.angle);
    const float ca = std::cos(player.angle);

    const float cx = screenW * 0.5f;
    const float cy = screenH * 0.5f;
    const float EPS = 1e-6f;

    // Shared projection distance (see Projection.h) - was recomputed with its
    // own tan() call inside the column loop before.
    const float projPlaneDist = Projection::focalLength(screenW);

    // Get time in seconds for animation
    float timeSec = float(SDL_GetTicks()) / 1000.0f;

    // Precompute lava animation for this tile
    bool isLava = map.get(wx, wy).isLava;
    float lavaOffsetX = 0.0f, lavaOffsetY = 0.0f;
    float lavaFlicker = 1.0f;

    if (isLava) {
        // Slower, smooth diagonal flow
        float lavaSpeedX = 0.08f; // tiles/sec
        float lavaSpeedY = 0.05f;

        float tileSeed = std::fmod(wx * 13.1f + wy * 7.7f, 1.0f);

        lavaOffsetX = fmod((timeSec + tileSeed) * lavaSpeedX * floorTex.w, float(floorTex.w));
        lavaOffsetY = fmod((timeSec + tileSeed) * lavaSpeedY * floorTex.h, float(floorTex.h));

        // Subtle flicker per tile
        lavaFlicker = 0.97f + 0.03f * sinf(timeSec * 2.0f + wx + wy);
    }

    for (int sx = 0; sx < screenW; ++sx) {
        float s = (float(sx) - cx) / cx;

        // Camera-space direction
        float dirx = s * (-sa) + ca;
        float diry = s * ( ca) + sa;

        // Ray origin
        float ox = player.x;
        float oy = player.y;

        // 2D AABB slab test
        float tmin = -INFINITY, tmax = INFINITY;

        // X slab
        if (fabs(dirx) >= EPS) {
            float tx1 = (minX - ox) / dirx;
            float tx2 = (maxX - ox) / dirx;
            if (tx1 > tx2) std::swap(tx1, tx2);
            tmin = std::max(tmin, tx1);
            tmax = std::min(tmax, tx2);
        } else if (ox < minX || ox > maxX) continue;

        // Y slab
        if (fabs(diry) >= EPS) {
            float ty1 = (minY - oy) / diry;
            float ty2 = (maxY - oy) / diry;
            if (ty1 > ty2) std::swap(ty1, ty2);
            tmin = std::max(tmin, ty1);
            tmax = std::min(tmax, ty2);
        } else if (oy < minY || oy > maxY) continue;

        if (tmax < tmin) continue; // miss

        float t_enter = std::max(tmin, 1e-5f);
        if (tmax <= 1e-5f) continue; // fully behind camera

        // Project vertical position
        float eyeHeight = player.z - tileHeight;

        float screenY_enter = cy - ((tileHeight - player.z) * projPlaneDist / t_enter);
        float screenY_exit  = cy - ((tileHeight - player.z) * projPlaneDist / tmax);

        int yTop = std::max(0, int(std::ceil(std::min(screenY_enter, screenY_exit))));
        int yBottom = std::min(screenH - 1, int(std::floor(std::max(screenY_enter, screenY_exit))));

        if (yBottom < 0 || yTop >= screenH) continue;

        // Precompute constants
        float invSize = 1.0f / sizeWorld;

        // Draw vertical span with per-pixel depth test
        uint32_t* px = pixels + yTop * screenW + sx;
        float* zPtr = zBuffer + yTop * screenW + sx;

        for (int y = yTop; y <= yBottom; ++y) {
            float p = float(y - screenH * 0.5f); // row relative to center
            if (fabs(p) < 1e-6f) { px += screenW; zPtr += screenW; continue; }

            float rowDist = eyeHeight * projPlaneDist / p;
            if (rowDist <= 0.0f) { px += screenW; zPtr += screenW; continue; } // behind camera

            // Depth test: skip if wall or closer floor already here
            if (rowDist >= *zPtr) { px += screenW; zPtr += screenW; continue; }

            // Compute world coordinates along the ray for this column
            float worldX = player.x + dirx * rowDist;
            float worldY = player.y + diry * rowDist;

            // Convert world to tile local
            float localX = (worldX - wx) * invSize;
            float localY = (worldY - wy) * invSize;

            int texX = int(localX * floorTex.w) % floorTex.w;
            int texY = int(localY * floorTex.h) % floorTex.h;
            if (texX < 0) texX += floorTex.w;
            if (texY < 0) texY += floorTex.h;

            uint32_t color = floorTex.pixels[texY * floorTex.w + texX];

            // Apply lava animation only for lava tiles
            if (isLava) {
                // Slight per-row variation for a bubbly effect
                int rowOffsetX = int(lavaOffsetX + (y * 0.05f)) % floorTex.w;  // small fractional shift per row
                int lavaTexX = (texX + rowOffsetX) % floorTex.w;
                int lavaTexY = (int(texY + lavaOffsetY)) % floorTex.h;

                color = floorTex.pixels[lavaTexY * floorTex.w + lavaTexX];

                // Apply subtle flicker/glow
                uint8_t r = std::min(255, int(((color >> 16) & 0xFF) * lavaFlicker));
                uint8_t g = std::min(255, int(((color >> 8) & 0xFF) * (lavaFlicker * 0.7f)));
                uint8_t b = std::min(255, int((color & 0xFF) * (lavaFlicker * 0.5f)));
                color = (r << 16) | (g << 8) | b;
            }

            *px = color;
            *zPtr = rowDist;
            px += screenW;
            zPtr += screenW;
        }
    }
}

// Project (wx,wy) into camera space relative to player. Returns camera-space X and forward (Y).
bool DoomRenderer::projectPointToCamera(float wx, float wy, const Player& player,
                                       float& out_camX, float& out_camY)
{
    float ca = std::cos(player.angle);
    float sa = std::sin(player.angle);
    float relX = wx - player.x;
    float relY = wy - player.y;
    // camera right = (-sin, cos), forward = (cos, sin)
    out_camX = relX * (-sa) + relY * ca; // cam X
    out_camY = relX * ca + relY * sa;    // cam Y (forward)
    return out_camY > 1e-5f; // in front of camera
}

// Rasterize a single segment (as vertical wall) in screen columns.
// Endpoint screen positions are interpolated in screen space (a straight
// world line projects to a straight screen line); depth is interpolated as
// 1/depth, which is the quantity that is linear in screen X.
// seg endpoints: seg.a (wx,wy) -> seg.b
void DoomRenderer::rasterizeSegment(const GridSegment& seg, int mapTileX, int mapTileY,
                                     uint32_t* pixels, int screenW, int screenH,
                                     const Player& player, const Map& map, float* zBuffer, const Texture& wallTex)
{
    // Live face extents: elevation on both sides, not "thickness".
    float floorZ, ceilZ;
    if (!segmentFaceExtents(map, seg, mapTileX, mapTileY, floorZ, ceilZ))
        return; // coplanar surfaces -> no vertical face (kills the phantom slabs)

    // Project endpoints to camera space
    float a_camX, a_camY, b_camX, b_camY;
    bool aFront = projectPointToCamera(seg.a.x, seg.a.y, player, a_camX, a_camY);
    bool bFront = projectPointToCamera(seg.b.x, seg.b.y, player, b_camX, b_camY);

    if (!aFront && !bFront) return;

    const float clipNear = 0.001f;
    if (!aFront || !bFront) {
        // Clip segment against near plane
        float t = (clipNear - a_camY) / (b_camY - a_camY);
        t = std::clamp(t, 0.0f, 1.0f);
        if (!aFront) {
            float newX = seg.a.x + t * (seg.b.x - seg.a.x);
            float newY = seg.a.y + t * (seg.b.y - seg.a.y);
            projectPointToCamera(newX, newY, player, a_camX, a_camY);
        } else {
            float newX = seg.a.x + t * (seg.b.x - seg.a.x);
            float newY = seg.a.y + t * (seg.b.y - seg.a.y);
            projectPointToCamera(newX, newY, player, b_camX, b_camY);
        }
    }

    // Project to screen X (same focal length as the Y projection below)
    const float focal = Projection::focalLength(screenW);
    const float cx = screenW * 0.5f;
    const float cy = screenH * 0.5f;

    float sxA = (a_camX / a_camY) * focal + cx;
    float sxB = (b_camX / b_camY) * focal + cx;

    // Skip if entirely off-screen horizontally
    if ((sxA < -screenW && sxB < -screenW) || (sxA > 2*screenW && sxB > 2*screenW)) return;

    int x0 = std::clamp(int(std::floor(std::min(sxA, sxB))), 0, screenW - 1);
    int x1 = std::clamp(int(std::ceil (std::max(sxA, sxB))), 0, screenW - 1);
    if (x1 < x0) std::swap(x0, x1);

    // Project top/bottom for endpoints (same focal length as screen X)
    float a_sy_floor   = cy - (floorZ - player.z) * (focal / a_camY);
    float a_sy_ceiling = cy - (ceilZ  - player.z) * (focal / a_camY);
    float b_sy_floor   = cy - (floorZ - player.z) * (focal / b_camY);
    float b_sy_ceiling = cy - (ceilZ  - player.z) * (focal / b_camY);

    // 1/depth is linear in screen X along a world-space line, depth itself is not.
    const float invA = 1.0f / a_camY;
    const float invB = 1.0f / b_camY;

    for (int sx = x0; sx <= x1; ++sx) {
        float t = (fabs(sxB - sxA) > 1e-6f) ? (sx - sxA) / (sxB - sxA) : 0.0f;
        t = std::clamp(t, 0.0f, 1.0f);

        float depth = 1.0f / (invA + t * (invB - invA));
        if (depth <= 0.0001f) continue;

        float colFloorY = a_sy_floor + t * (b_sy_floor - a_sy_floor);
        float colCeilY = a_sy_ceiling + t * (b_sy_ceiling - a_sy_ceiling);

        int drawStart = std::max(0, int(std::ceil(colCeilY)));
        int drawEnd = std::min(screenH - 1, int(std::floor(colFloorY)));
        if (drawEnd < 0 || drawStart >= screenH) continue;

        // Fractional horizontal coordinate along the wall segment (0 -> 1).
        // Camera-space depth is affine in the segment's world parameter, so
        // invert the projection instead of treating the screen-space t as a
        // world-space fraction (which skewed texture U on angled walls).
        float u;
        if (std::fabs(b_camY - a_camY) > 1e-6f)
            u = std::clamp((depth - a_camY) / (b_camY - a_camY), 0.0f, 1.0f);
        else
            u = t; // segment parallel to the image plane: screen X is linear in world X

        // Draw textured column with per-pixel depth test
        uint32_t* px = pixels + drawStart * screenW + sx;
        float* zPtr = zBuffer + drawStart * screenW + sx;
        int columnHeight = drawEnd - drawStart + 1;
        for (int y = drawStart; y <= drawEnd; ++y) {
            float ty = float(y - drawStart) / float(columnHeight + 1e-6f);

            int texX = int(u * wallTex.w) % wallTex.w; // horizontal
            int texY = int((floorZ + ty * (ceilZ - floorZ)) * wallTex.h) % wallTex.h; // vertical
            if (texX < 0) texX += wallTex.w;
            if (texY < 0) texY += wallTex.h;

            if (depth < *zPtr) {
                *px = wallTex.pixels[texY * wallTex.w + texX];
                *zPtr = depth;
            }
            px += screenW;
            zPtr += screenW;
        }
    }
}

// side test: positive if P is left of AB
float DoomRenderer::sideOfLine(float ax, float ay, float bx, float by, float px, float py)
{
    float vx = bx - ax, vy = by - ay;
    float wx = px - ax, wy = py - ay;
    return vx * wy - vy * wx;
}

// traverse BSP front to back relative to player's position 
void DoomRenderer::traverseBSP(
    const BSPNode* node,
    const Player& player,
    uint32_t* pixels,
    int screenW,
    int screenH,
    const Map& map,
    float* zBuffer,
    uint8_t* tileDrawn,
    TextureManager& textureManager
) {
    if (!node) return;

    // Determine traversal order
    float side = sideOfLine(
        node->splitA.x, node->splitA.y,
        node->splitB.x, node->splitB.y,
        player.x, player.y
    );

    const BSPNode* first = (side > 0.0f) ? node->front.get() : node->back.get();
    const BSPNode* second = (side > 0.0f) ? node->back.get()  : node->front.get();

    // Traverse far side first
    if (second)
        traverseBSP(second, player, pixels, screenW, screenH, map, zBuffer, tileDrawn, textureManager);

    // Pass 1: vertical walls only
    for (const auto& seg : node->onPlane) {
        int tx = seg.tileX;
        int ty = seg.tileY;

        if (tx < 0 || tx >= Map::SIZE || ty < 0 || ty >= Map::SIZE)
            continue;

        const Map::Cell& cell = map.get(tx, ty);
        float h = cell.height;

        const Texture* wallTex = nullptr;

        if (cell.isSliding == true) {
            wallTex = &textureManager.get("wallSliding");
        }
        else if (cell.isExit == true) {
            wallTex = &textureManager.get("wallDoor");
        }
        else {
            wallTex = &textureManager.get("wall1");
        }

        // Draw vertical walls (normal or pit)
        if (h != 0.0f) {
            rasterizeSegment(seg, tx, ty, pixels, screenW, screenH, player, map, zBuffer, *wallTex);
        }
    }

    // Pass 2: floors, pits, wall tops
    for (const auto& seg : node->onPlane) {
        int tx = seg.tileX;
        int ty = seg.tileY;

        if (tx < 0 || tx >= Map::SIZE || ty < 0 || ty >= Map::SIZE)
            continue;

        int idx = tx + ty * Map::SIZE;
        if (tileDrawn[idx])
            continue;
        if (map.get(tx+1,ty).height < 0 || map.get(tx,ty+1).height < 0 || map.get(tx,ty-1).height < 0 || map.get(tx-1,ty).height < 0 || map.get(tx+1,ty+1).height < 0 || map.get(tx-1,ty-1).height < 0) {
            tileDrawn[idx] = false;
        }
        else if (map.get(tx+2,ty).height < 0 || map.get(tx,ty+2).height < 0 || map.get(tx,ty-2).height < 0 || map.get(tx-2,ty).height < 0 || map.get(tx+2,ty+2).height < 0 || map.get(tx-2,ty-2).height < 0) {
            tileDrawn[idx] = false;
        }
        else {
            tileDrawn[idx] = true;
        }

        const Map::Cell& cell = map.get(tx, ty);
        float h = cell.height;

        const Texture* floorTex = nullptr;

        if (h < 0.0f) {
            // Pit floor
            floorTex = &textureManager.get("lava1");
        }
        else if (h > 0.0f && h != WALL_WORLD_HEIGHT) {
            // Wall top
            floorTex = &textureManager.get("wallTop1");
        }
        else {
            // Normal flat floor
            floorTex = &textureManager.get("floor1");
        }

        renderWorldTileRasterized(
            pixels, zBuffer,
            screenW, screenH,
            player,
            float(tx), float(ty),
            1.0f,
            h,
            *floorTex,
            map
        );
    }

    // Traverse near side last
    if (first)
        traverseBSP(first, player, pixels, screenW, screenH, map, zBuffer, tileDrawn, textureManager);
}

// Main render entry
void DoomRenderer::render(uint32_t* pixels, int screenW, int screenH,
                          const Player& player, Map& map, float* zBuffer, EnemyManager& em, TextureManager& textureManager, BulletHoleManager& bulletHoleManager)
{
    const uint32_t CEIL_COLOR = 0xFF202040; // World ceiling color (change to texture in the future)

    // Clear framebuffer
    static constexpr uint32_t CLEAR_PIXEL = 0x00000000; // fully transparent black
    std::fill(pixels, pixels + screenW * screenH, CLEAR_PIXEL);

    // Init zBuffer (per-pixel)
    for (int i = 0; i < screenW * screenH; ++i)
        zBuffer[i] = 1e6f;

    static std::vector<uint8_t> tileDrawn;
    tileDrawn.assign(Map::SIZE * Map::SIZE, 0);

    // Traverse BSP and draw segments front-to-back
    traverseBSP(m_bspRoot.get(), player, pixels, screenW, screenH, map, zBuffer, tileDrawn.data(), textureManager);

    // Fill Ceiling
    for (int y = 0; y < screenH / 2; ++y)
    {
        for (int x = 0; x < screenW; ++x)
        {
            int idx = y * screenW + x;
            if (pixels[idx] == CLEAR_PIXEL)
                pixels[idx] = CEIL_COLOR;
        }
    }

    // Bullet holes: stamped once, clipped to their face and depth tested
    drawBulletHoles(pixels, zBuffer, screenW, screenH, player, map, bulletHoleManager);

    // Draw enemies                  
    spriteRenderer.renderEnemies(pixels, screenW, screenH, em, player, zBuffer, map, colWallTop);

    // Draw pickups
    if (pickupManager)
        pickupManager->renderPickups(pixels, screenW, screenH, player, zBuffer, map, colWallTop);
}
