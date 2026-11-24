#ifndef ENEMY_MANAGER_H
#define ENEMY_MANAGER_H

#include <vector>
#include "Enemy.h"
#include "PickupManager.h"
#include "Map.h"
#include "LevelDef.h"
#include <unordered_map>
#include <SDL2/SDL.h>

struct SpriteFrame {
    int w = 0;
    int h = 0;
    std::vector<uint32_t> pixels;
};

struct Animation {
    std::vector<SpriteFrame> frames;
    float frameDuration = 0.1f; // seconds per frame
};

struct EnemyVisual {
    std::unordered_map<EnemyAnimState, Animation> animations;
};

class EnemyManager {
public:
    static const int MAX_ENEMIES = 128;

    EnemyManager();

    Enemy enemies[MAX_ENEMIES];

    std::unordered_map<EnemyType, EnemyVisual> enemyVisuals;
    std::unordered_map<EnemyType, EnemyVisual> enemyVisualsDamaged;

    void scanMapForSpawnPoints(const Map& map);

    // All spawn points found on the loaded level.
    const std::vector<GridPos>& getSpawnPoints() const { return spawnPoints; }

    // Spawn at one of `candidates` (the level's points, or a wave's own
    // group), preferring points far enough from the player and out of its
    // line of sight; falls back to plain round-robin when nothing qualifies.
    Enemy* spawnEnemy(EnemyType type, const std::vector<GridPos>& candidates,
                      const Player& player, const Map& map);

    void loadEnemyAssets();
    void update(float dt, const Player& player, PickupManager& pickupManager, const Map& map, AudioManager& audio);

    bool hasActiveEnemies() const;
    int getActiveEnemyCount() const;
    void deactivateAll();

    int enemiesKilled = 0;

private:
    std::vector<GridPos> spawnPoints;
    int nextSpawnIndex = 0;
    void trySpawnAmmoDrop(const Enemy& e, const Player& player, PickupManager& pickupManager, const Map& map);
};

#endif

