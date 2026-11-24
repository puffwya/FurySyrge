#pragma once

#include "Player.h"
#include "Map.h"
#include "EnemyManager.h"
#include "PickupManager.h"
#include "WeaponManager.h"
#include "BulletHoleManager.h"
#include "WeaponTypes.h"
#include "HUD.h"
#include "MapToSegments.h"
#include "BSP.h"
#include "DoomRenderer.h"
#include "GameState.h"
#include "../settings/GameSettings.h"

#include <memory>
#include <string>
#include <vector>
#include <cstdint>
#include <SDL2/SDL.h>

class Renderer;

class TextureManager;

struct Wave {
    float spawnInterval;
    std::vector<EnemyType> enemies;
};

struct WallHeightAnim {
    int x;
    int y;

    float startHeight;
    float targetHeight;

    float progress;   // 0.0, 1.0
    float speed;      // units per second

    bool finished = false;
};

class GameSession {
public:
    GameSession(Renderer& renderer, int screenW, int screenH, Difficulty diff);
    ~GameSession();

    void update(float dt, const Uint8* keys, GameState& gameState, AudioManager& audio);
    void render(Renderer& renderer, uint32_t* pixels, int screenW, int screenH, TextureManager& textureManager);

    void renderPaused(Renderer& renderer, uint32_t* pixels, int screenW, int screenH, float pauseT, TextureManager& textureManager);

    // Debug helper for main.cpp's --dump mode: render one world-only frame
    // (no weapon/HUD/damage flash) at the given yaw/pitch into a binary PPM
    // file, so projection changes can be compared visually instead of being
    // playtested blind.
    void dumpWorldFrame(uint32_t* pixels, int screenW, int screenH,
                        TextureManager& textureManager,
                        float yawDeg, float pitchDeg, const std::string& path);

    std::vector<Wave> waves;
    std::vector<WallHeightAnim> wallAnims;
    int currentWaveIndex = -1;

    // Core gameplay state
    Player player;
    EnemyManager enemyManager;

    Difficulty difficulty;
    int currentLevel = 0;

private:
    // Core gameplay state
    Map worldMap;
    PickupManager pickupManager;
    WeaponManager weaponManager;
    Weapon weapon;
    BulletHoleManager bulletHoleManager;
    HUD hud;

    // BSP world rendering
    std::vector<GridSegment> segments;
    std::unique_ptr<BSPNode> bspRoot;
    std::unique_ptr<DoomRenderer> doomRenderer;

    float* zBuffer = nullptr;

    void initWorld(Renderer& renderer, int screenW, int levelIndex = 0);

    // Switch to a different level (rebuilds BSP + renderer)
    void loadLevel(int levelIndex);

    // Wave control 
    void startWave(int index);

    float spawnTimer = 0.0f;
    float postWaveTimer = 0.0f;

    size_t enemiesSpawned = 0;

    enum class WaveState {
        Spawning,
        WaitingForClear,
        PostWaveDelay
    };

    WaveState waveState = WaveState::PostWaveDelay;

    void startWaveWallAnimations(int waveIndex, AudioManager& audio);
    void updateWallAnimations(float dt, AudioManager& audio);

    bool exit_spawn = false;

};
