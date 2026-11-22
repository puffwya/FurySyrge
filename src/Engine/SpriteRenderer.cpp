#include "SpriteRenderer.h"
#include "Projection.h"
#include <cmath>
#include <algorithm>

void SpriteRenderer::renderEnemies(
    uint32_t* pixels,
    int screenW,
    int screenH,
    EnemyManager& manager,
    const Player& player,
    float* zBuffer,
    Map& map
) {
    struct DrawInfo {
        Enemy* enemy;
        float dist;
    };

    DrawInfo drawList[EnemyManager::MAX_ENEMIES];
    int count = 0;

    // Collect active enemies and compute distance
    for (Enemy& e : manager.enemies) {
        if (!e.active) continue;
        float dx = e.x - player.x;
        float dy = e.y - player.y;
        drawList[count++] = { &e, std::sqrt(dx*dx + dy*dy) };
    }

    if (count == 0) return;

    // Sort back-to-front
    std::sort(drawList, drawList + count,
        [](const DrawInfo& a, const DrawInfo& b) { return a.dist > b.dist; }
    );

    // Camera vectors - must match the world projection (see Projection.h)
    float dirX = std::cos(player.angle);
    float dirY = std::sin(player.angle);
    float planeX = -dirY * Projection::planeLength();
    float planeY = dirX * Projection::planeLength();
    const float focalY = Projection::focalY(screenH);

    for (int i = 0; i < count; i++) {
        Enemy* e = drawList[i].enemy;
        std::vector<uint32_t>& framePixels = e->spritePixels;
        int frameW = e->spriteW;
        int frameH = e->spriteH;

        float dx = e->x - player.x;
        float dy = e->y - player.y;

        // Transform to camera space
        float invDet = 1.0f / (planeX * dirY - dirX * planeY);
        float transformX = invDet * ( dirY * dx - dirX * dy );
        float transformY = invDet * (-planeY * dx + planeX * dy );

        if (transformY <= 0.05f) continue;

        // Screen X
        int screenX = int((screenW / 2.0f) * (1 + transformX / transformY));

        // Vertical scaling with aspect ratio
        int spriteH = std::max(1, int(focalY / transformY * e->height));
        int spriteW = std::max(1, int(spriteH * (float(frameW) / float(frameH))));

        float enemyBottom = e->z - player.z;
        float enemyTop = e->z + e->height - player.z;

        // Pitch-aware vertical projection (Projection::projectY) - the same
        // mapping the walls use, so sprites line up with them.
        const float horizon = Projection::horizonY(screenH, player.pitch);
        const float T = std::tan(player.pitch);
        int drawStartY = int(Projection::projectY(enemyTop / transformY, T, horizon, focalY));
        int drawEndY = int(Projection::projectY(enemyBottom / transformY, T, horizon, focalY));
        int drawStartX = screenX - spriteW / 2;
        int drawEndX = drawStartX + spriteW;

        // Texcoords use the UNCLAMPED span: deriving them from the
        // screen-clamped range made sprites squash to fit the visible
        // area as they crossed an edge instead of being cropped.
        const int originX = drawStartX;
        const int fullW = drawEndX - drawStartX;

        // Clamp to screen (loop bounds only; ends are exclusive)
        drawStartX = std::max(0, drawStartX);
        drawEndX = std::min(screenW, drawEndX);
        drawStartY = std::max(0, drawStartY);
        drawEndY = std::min(screenH, drawEndY);
        if (drawEndX <= drawStartX || drawEndY <= drawStartY) continue;

        auto drawPixel = [&](int x, int y) {
            int srcX = std::clamp((x - originX) * frameW / fullW, 0, frameW - 1);
            // Vertical texcoord from the row's world height (hyperbolic with
            // pitch, exactly linear at pitch 0): a linear-in-screen-Y mapping
            // would warp the sprite as it crosses the top/bottom edge.
            const float rowZ = Projection::unprojectY(float(y), T, horizon, focalY) * transformY;
            int srcY = std::clamp(int((enemyTop - rowZ) / e->height * frameH), 0, frameH - 1);
            uint32_t color = framePixels[srcY * frameW + srcX];
            if ((color >> 24) == 0) return; // skip transparent
            pixels[y * screenW + x] = color;
        };

        // Draw with per-pixel z-buffer depth test
        for (int x = drawStartX; x < drawEndX; x++) {
            for (int y = drawStartY; y < drawEndY; y++) {
                if (zBuffer[y * screenW + x] >= transformY) {
                    drawPixel(x, y);
                }
            }
        }
    }
}
