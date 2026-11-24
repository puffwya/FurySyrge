#include "GameSession.h"
#include "Renderer.h"
#include "pItemRenderer.h"
#include "Projection.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include "TextureManager.h"

struct DifficultyParams {
    float enemyCountMultiplier;
    float spawnIntervalMultiplier;
};

DifficultyParams getDifficultyParams(Difficulty d)
{
    switch (d)
    {
        case Difficulty::Easy:
            return { 0.7f, 1.25f }; // fewer enemies, slower spawns

        case Difficulty::Hard:
            return { 1.3f, 0.8f }; // more enemies, faster spawns

        case Difficulty::Medium:
        default:
            return { 1.0f, 1.0f }; // Default
    }
}

GameSession::GameSession(Renderer& renderer, int screenW, int screenH, Difficulty diff) : difficulty(diff) {
    // Pickup visuals first: initWorld() -> loadLevel() spawns the level's
    // initial pickups, which look up their textures immediately.
    pickupManager.loadPickupAssets();

    initWorld(renderer, screenW);

    zBuffer = new float[screenW * screenH];

    weaponManager.loadAssets(renderer.getSDLRenderer());

    bulletHoleManager.loadVisual(
        BulletHoleType::Pistol, "Assets/geometry_textures/bulletHole.png"
    );

    bulletHoleManager.loadVisual(
        BulletHoleType::Shotgun, "Assets/geometry_textures/bulletHoleS.png"
    );

    if (!hud.init(renderer.getSDLRenderer())) {
        std::cerr << "Failed to initialize HUD\n";
    }

    // Initialize enemy assets
    enemyManager.loadEnemyAssets();

    doomRenderer->setPickupManager(pickupManager);
}

GameSession::~GameSession() {
    delete[] zBuffer;
}

void GameSession::initWorld(Renderer& renderer, int screenW, int levelIndex) {
    (void)renderer;
    loadLevel(levelIndex);
}

void GameSession::loadLevel(int levelIndex) {
    currentLevel = levelIndex;
    const LevelDef& def = getLevel(levelIndex);

    worldMap.loadLevel(levelIndex);
    enemyManager.scanMapForSpawnPoints(worldMap);

    segments = buildSegmentsFromGrid(worldMap);
    bspRoot = buildBSP(segments);
    doomRenderer = std::make_unique<DoomRenderer>(segments, std::move(bspRoot));

    // Start the player at the level's authored spawn, standing on its
    // floor (eye height 0.5 above it).
    player.x = def.playerSpawnX;
    player.y = def.playerSpawnY;
    player.z = worldMap.get(int(def.playerSpawnX), int(def.playerSpawnY)).height + 0.5f;
    player.velX = player.velY = player.velZ = 0.0f;
    player.angle = def.playerSpawnAngle;

    // Fresh level: drop the previous level's runtime state and lay out
    // this level's starting pickups.
    exit_spawn = false;
    pickupManager.clear();
    for (const PickupSpawn& p : def.initialPickups)
        pickupManager.addPickup(p.x, p.y, p.type, p.id, worldMap);
    enemyManager.deactivateAll();
    wallAnims.clear();

    // This level's waves, scaled by difficulty.
    waves = def.waves;
    DifficultyParams params = getDifficultyParams(difficulty);
    for (auto& wave : waves)
    {
        // Scale spawn rate
        wave.spawnInterval *= params.spawnIntervalMultiplier;

        // Scale enemy count
        int originalCount = static_cast<int>(wave.enemies.size());
        int targetCount =
            static_cast<int>(std::round(originalCount * params.enemyCountMultiplier));

        targetCount = std::max(1, targetCount);

        if (targetCount > originalCount)
        {
            // Add more enemies by repeating existing ones
            while (wave.enemies.size() < static_cast<size_t>(targetCount))
            {
                wave.enemies.push_back(
                    wave.enemies[rand() % originalCount]
                );
            }
        }
        else if (targetCount < originalCount)
        {
            wave.enemies.resize(targetCount);
        }
    }

    // Restart the wave cycle: 5s of post-wave delay, then wave 1.
    currentWaveIndex = -1;
    waveState = WaveState::PostWaveDelay;
    postWaveTimer = 0.0f;
    enemiesSpawned = 0;
    spawnTimer = 0.0f;
}

bool GameSession::advanceLevel() {
    if (currentLevel + 1 >= levelCount()) return false;
    loadLevel(currentLevel + 1);
    return true;
}

void GameSession::startWave(int index) {
    currentWaveIndex = index;
    enemiesSpawned = 0;
    spawnTimer = 0.0f;
    waveState = WaveState::Spawning;
}

void GameSession::startWaveWallAnimations(int waveIndex, AudioManager& audio) {
    const LevelDef& levelDef = getLevel(worldMap.currentLevel);

    // One sliding-wall animation: open = slide down to floor level,
    // close = slide back up to the level's wall height.
    auto slideWall = [&](int x, int y, float targetHeight) {
        WallHeightAnim anim;
        anim.x = x;
        anim.y = y;
        anim.startHeight = worldMap.get(x, y).height;
        anim.targetHeight = targetHeight;
        anim.progress = 0.0f;
        anim.speed = 0.75f;
        anim.finished = false;
        wallAnims.push_back(anim);
    };

    // Special index 100: the player left the spawn room, so raise the
    // start-room doors again (they are wave 1's openWalls).
    if (waveIndex == 100) {
        audio.playSFX("wall_slide");
        if (!waves.empty())
            for (const GridPos& g : waves[0].openWalls)
                slideWall(g.x, g.y, levelDef.wallHeight);
        return;
    }

    if (waveIndex >= 0 && waveIndex < (int)waves.size()) {
        audio.playSFX("wall_slide");
        const WaveDef& wave = waves[waveIndex];
        for (const GridPos& g : wave.openWalls)
            slideWall(g.x, g.y, 0.0f);
        // Rewards are granted once per wave (previously the addPickup call
        // sat inside the per-tile loop, granting one copy per door tile).
        for (const PickupSpawn& p : wave.rewards)
            pickupManager.addPickup(p.x, p.y, p.type, p.id, worldMap);
        return;
    }

    // Past the last wave: open the final corridor to the exit.
    if (waveIndex == (int)waves.size()) {
        audio.playSFX("wall_slide");
        for (const GridPos& g : levelDef.finalWalls)
            slideWall(g.x, g.y, 0.0f);
    }
}

void GameSession::updateWallAnimations(float dt, AudioManager& audio) {
    for (auto& anim : wallAnims) {
        if (anim.finished)
            continue;

        anim.progress += anim.speed * dt;
        anim.progress = std::min(anim.progress, 1.0f);

        // Smooth interpolation (linear for now)
        float t = anim.progress;
        float height =
            anim.startHeight +
            (anim.targetHeight - anim.startHeight) * t;

        worldMap.get(anim.x, anim.y).height = height;

        if (anim.progress >= 1.0f) {
            anim.finished = true;
        }
    }

    // cleanup
    wallAnims.erase(
        std::remove_if(wallAnims.begin(), wallAnims.end(),
                       [](const WallHeightAnim& a) { return a.finished; }),
        wallAnims.end()
    );
}

void GameSession::update(float dt, const Uint8* keys, GameState& gameState, AudioManager& audio) {
    player.update(dt, keys, worldMap, enemyManager, weaponManager, weapon, gameState, audio, bulletHoleManager);
    enemyManager.update(dt, player, pickupManager, worldMap, audio);
    pickupManager.update(player, dt, weapon, audio);
    weaponManager.update(dt, player);
    bulletHoleManager.update(dt);
    updateWallAnimations(dt, audio);

    if (currentWaveIndex >= (int)waves.size())
        return;

    // Raise the start-room doors once the player crosses the level's
    // spawn-room exit line (negative = the level has no spawn room).
    const LevelDef& levelDef = getLevel(currentLevel);
    if (levelDef.spawnExitY >= 0.0f &&
        player.y > levelDef.spawnExitY && exit_spawn == false) {
        startWaveWallAnimations(100, audio);
        exit_spawn = true;
    }

    // currentWaveIndex starts at -1 during the initial post-wave delay and
    // only ever dereferences a real wave once it reaches 0.
    WaveDef* wave =
        (currentWaveIndex >= 0) ? &waves[currentWaveIndex] : nullptr;

    switch (waveState) {

        case WaveState::Spawning: {
            if (!wave) break;  // still in the initial -1 delay state
            spawnTimer += dt;

            if (spawnTimer >= wave->spawnInterval &&
                enemiesSpawned < wave->enemies.size()) {

                const std::vector<GridPos>& points =
                    wave->spawnPoints.empty() ? enemyManager.getSpawnPoints()
                                              : wave->spawnPoints;
                enemyManager.spawnEnemy(wave->enemies[enemiesSpawned],
                                        points, player, worldMap);
                enemiesSpawned++;

                spawnTimer = 0.0f;
            }

            // Finished spawning, wait until cleared
            if (enemiesSpawned == wave->enemies.size()) {
                waveState = WaveState::WaitingForClear;
            }
            break;
        }

        case WaveState::WaitingForClear: {
            if (!enemyManager.hasActiveEnemies()) {
                postWaveTimer = 0.0f;
                waveState = WaveState::PostWaveDelay;
            }
            break;
        }

        case WaveState::PostWaveDelay: {
            postWaveTimer += dt;

            if (postWaveTimer >= 5.0f) {
                currentWaveIndex++;
                enemiesSpawned = 0;
                spawnTimer = 0.0f;

                enemyManager.deactivateAll();

                waveState = WaveState::Spawning;

                startWaveWallAnimations(currentWaveIndex, audio);
            }
            break;
        }
    }
}

void GameSession::render(Renderer& renderer, uint32_t* pixels, int w, int h, TextureManager& textureManager) {
    std::fill(pixels, pixels + w * h, 0xFF202020);

    doomRenderer->render(
        pixels, w, h,
        player, worldMap, zBuffer, enemyManager, textureManager, bulletHoleManager
    );

    player.renderDamageFlash(pixels, w, h, player.damageFlashIntensity);

    renderer.updateTexture(pixels);
    renderer.beginFrame();
    renderer.drawScreenTexture();

    WeaponType wType = WeaponType::None;
    if (player.currentItem == ItemType::Pistol) wType = WeaponType::Pistol;
    else if (player.currentItem == ItemType::Shotgun) wType = WeaponType::Shotgun;
    else if (player.currentItem == ItemType::Mg) wType = WeaponType::Mg;

    SDL_Texture* itemTex = weaponManager.getCurrentFrame(wType);

    pItemRenderer::renderPItem(
        renderer.getSDLRenderer(),
        itemTex,
        w, h,
        player.currentItem,
        weaponManager
    );

    int safeCurrentWave = std::max(0, currentWaveIndex); // never negative
    int totalWaves = (int)waves.size();
    int enemiesRemaining = 0;
 
    // Only calculate if there is a valid wave
    if (currentWaveIndex >= 0 && currentWaveIndex < totalWaves) {
        WaveDef& wave = waves[currentWaveIndex];
        int enemiesLeftToSpawn = std::max(0, (int)wave.enemies.size() - (int)enemiesSpawned);
        int activeEnemies = enemyManager.getActiveEnemyCount();
        enemiesRemaining = enemiesLeftToSpawn + activeEnemies;
    }
 
    // Always pass safe values to HUD
    hud.render(renderer.getSDLRenderer(), player, w, h, weapon,
           safeCurrentWave,    // display wave number starting at 1
           totalWaves,
           enemiesRemaining,
           postWaveTimer);

    renderer.present();
}

// Debug: render the world at a chosen orientation to a PPM file. Driven by
// the --dump flag in main.cpp so projection changes (the pitch remap in
// particular) can be eyeballed and compared instead of playtested blind.
void GameSession::dumpWorldFrame(uint32_t* pixels, int screenW, int screenH,
                                 TextureManager& textureManager,
                                 float yawDeg, float pitchDeg, const std::string& path) {
    // The level's authored spawn (level 1: 14.5, 2.5 on flat floor).
    const LevelDef& def = getLevel(worldMap.currentLevel);
    player.x = def.playerSpawnX;
    player.y = def.playerSpawnY;
    player.z = worldMap.get(int(def.playerSpawnX), int(def.playerSpawnY)).height + 0.5f;
    player.velX = player.velY = player.velZ = 0.0f;
    player.angle = yawDeg * Projection::PI / 180.0f;
    player.pitch = pitchDeg * Projection::PI / 180.0f;
    player.damageFlashIntensity = 0.0f;

    // One enemy a few metres ahead on flat floor, so the sprite path is
    // covered by the dump too. Falls back to a real spawn point (which may
    // be off-screen) when nothing usable lies ahead.
    enemyManager.deactivateAll();
    if (Enemy* e = enemyManager.spawnEnemy(EnemyType::Base,
                                           enemyManager.getSpawnPoints(),
                                           player, worldMap)) {
        for (float d = 2.0f; d <= 8.0f; d += 1.0f) {
            const float ex = player.x + std::cos(player.angle) * d;
            const float ey = player.y + std::sin(player.angle) * d;
            if (ex < 0.0f || ey < 0.0f || ex >= float(Map::SIZE) || ey >= float(Map::SIZE))
                continue;
            const Map::Cell& cell = worldMap.get(int(ex), int(ey));
            if (cell.height >= 0.0f && cell.height <= 0.25f && cell.cHeight > 0.75f) {
                e->x = ex;
                e->y = ey;
                e->z = cell.height;
                break;
            }
        }
    }

    doomRenderer->render(pixels, screenW, screenH, player, worldMap, zBuffer,
                         enemyManager, textureManager, bulletHoleManager);

    // Binary PPM (P6); pixels are 0xAARRGGBB (see the ceiling/bullet-hole
    // writes in DoomRenderer).
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        std::cerr << "dumpWorldFrame: cannot open " << path << "\n";
        return;
    }
    out << "P6\n" << screenW << " " << screenH << "\n255\n";
    std::vector<unsigned char> row(size_t(screenW) * 3);
    for (int y = 0; y < screenH; ++y) {
        for (int x = 0; x < screenW; ++x) {
            const uint32_t c = pixels[y * screenW + x];
            row[size_t(x) * 3 + 0] = uint8_t((c >> 16) & 0xFF);
            row[size_t(x) * 3 + 1] = uint8_t((c >> 8) & 0xFF);
            row[size_t(x) * 3 + 2] = uint8_t(c & 0xFF);
        }
        out.write(reinterpret_cast<const char*>(row.data()), std::streamsize(row.size()));
    }
    if (!out) std::cerr << "dumpWorldFrame: write failed for " << path << "\n";
}

void GameSession::renderPaused(
    Renderer& renderer,
    uint32_t* pixels,
    int w,
    int h,
    float pauseT,
    TextureManager& textureManager
) {
    doomRenderer->render(
        pixels, w, h,
        player, worldMap, zBuffer, enemyManager, textureManager, bulletHoleManager
    );

    player.renderDamageFlash(pixels, w, h, player.damageFlashIntensity);

    renderer.updateTexture(pixels);
    renderer.beginFrame();
    renderer.drawScreenTexture();

    // draw weapon + HUD (same as normal)
    WeaponType wType = WeaponType::None;
    if (player.currentItem == ItemType::Pistol) wType = WeaponType::Pistol;
    else if (player.currentItem == ItemType::Shotgun) wType = WeaponType::Shotgun;

    SDL_Texture* itemTex = weaponManager.getCurrentFrame(wType);
    pItemRenderer::renderPItem(
        renderer.getSDLRenderer(),
        itemTex,
        w, h,
        player.currentItem,
        weaponManager
    );

    int safeCurrentWave = std::max(0, currentWaveIndex); // never negative
    int totalWaves = (int)waves.size();
    int enemiesRemaining = 0;

    // Only calculate if there is a valid wave
    if (currentWaveIndex >= 0 && currentWaveIndex < totalWaves) {
        WaveDef& wave = waves[currentWaveIndex];
        int enemiesLeftToSpawn = std::max(0, (int)wave.enemies.size() - (int)enemiesSpawned);
        int activeEnemies = enemyManager.getActiveEnemyCount();
        enemiesRemaining = enemiesLeftToSpawn + activeEnemies;
    }

    // Always pass safe values to HUD
    hud.render(renderer.getSDLRenderer(), player, w, h, weapon,
           safeCurrentWave,    // display wave number starting at 1
           totalWaves,
           enemiesRemaining,
           postWaveTimer);
}
