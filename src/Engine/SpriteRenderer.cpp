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

        // Horizon carries the pitch shear so sprites line up with walls
        const float horizon = Projection::horizonY(screenH, player.pitch);
        int drawStartY = int(horizon - enemyTop / transformY * focalY);
        int drawEndY = int(horizon - enemyBottom / transformY * focalY);
        int drawStartX = screenX - spriteW / 2;
        int drawEndX = drawStartX + spriteW;

        // Clamp to screen
        drawStartX = std::max(0, drawStartX);
        drawEndX = std::min(screenW - 1, drawEndX);
        drawStartY = std::max(0, drawStartY);
        drawEndY = std::min(screenH - 1, drawEndY);

        int spanW = drawEndX - drawStartX;
        int spanH = drawEndY - drawStartY;
        if (spanW <= 0 || spanH <= 0) continue;

        auto drawPixel = [&](int x, int y) {
            int srcX = std::clamp((x - drawStartX) * frameW / spanW, 0, frameW - 1);
            int srcY = std::clamp((y - drawStartY) * frameH / spanH, 0, frameH - 1);
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
