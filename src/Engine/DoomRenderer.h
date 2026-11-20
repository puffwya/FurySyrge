#ifndef DOOM_RENDERER_H
#define DOOM_RENDERER_H

#include <vector>
#include <cstdint>
#include "BSP.h"         // uses Segment type (GridSegment)
#include "Map.h"
#include "Player.h"
#include "BulletHoleManager.h"
#include "SpriteRenderer.h"
#include "PickupManager.h"
#include "TextureManager.h"

class TextureManager;

class DoomRenderer {
public:
    // Provide segments and BSP root to the renderer (built by earlier steps)
    DoomRenderer(const std::vector<GridSegment>& segments,
                 std::unique_ptr<BSPNode> bspRoot);

    // Render into pixels buffer. zBuffer must be length screenW * screenH
    // (per-pixel depth, world units of perpendicular camera distance).
    void render(uint32_t* pixels, int screenW, int screenH,
                const Player& player, Map& map, float* zBuffer, EnemyManager& em, TextureManager& textureManager, BulletHoleManager& bulletHoleManager);

    void setPickupManager(PickupManager& manager) { pickupManager = &manager; }

    // Stamp bullet holes onto the already rendered world (depth tested).
    void drawBulletHoles(uint32_t* pixels, float* zBuffer, int screenW, int screenH,
                         const Player& player, const Map& map,
                         BulletHoleManager& bulletHoleManager);

private:
    std::unique_ptr<BSPNode> m_bspRoot;

    PickupManager* pickupManager = nullptr;

    static void renderWorldTileRasterized(uint32_t* pixels, float* zBuffer, int screenW, int screenH,
                                      const Player& player,
                                      float wx, float wy, float sizeWorld, float tileHeight,
                                      const Texture& floorTex, const Map& map);

    // helper drawing
    // Project endpoints to camera space...returns false if completely behind camera
    bool projectPointToCamera(float wx, float wy, const Player& player,
                              float& out_camX, float& out_camY);

    // Rasterize segment between two projected endpoints (screen Xs / depths)
    void rasterizeSegment(const GridSegment& seg, int mapTileX, int mapTileY,
                          uint32_t* pixels, int screenW, int screenH,
                          const Player& player, const Map& map, float* zBuffer, const Texture& wallTex);

    // BSP traversal: vertical wall faces only
    void traverseBSP(const BSPNode* node, const Player& player,
                     uint32_t* pixels, int screenW, int screenH,
                     const Map& map, float* zBuffer, TextureManager& textureManager);

    // small side test helper
    static float sideOfLine(float ax, float ay, float bx, float by, float px, float py);
};

#endif // DOOM_RENDERER_H

