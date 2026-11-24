#ifndef LEVELDEF_H
#define LEVELDEF_H

#include <vector>
#include "EnemyTypes.h"
#include "PickupTypes.h"
#include "WeaponTypes.h"

// Integer tile coordinate (grid space: get(x, y) / data[y][x]).
struct GridPos {
    int x;
    int y;
};

// A pickup placed at a world position. Its z is resolved from the tile
// height when the level spawns it.
struct PickupSpawn {
    float x;
    float y;
    PickupType type;
    WeaponType id;
};

// One wave: what spawns, which sliding walls open when it starts, and the
// reward pickups granted exactly once at that moment.
struct WaveDef {
    float spawnInterval;              // seconds between individual spawns
    std::vector<EnemyType> enemies;   // spawn order
    std::vector<GridPos> openWalls;   // sliding walls opened for this wave
    std::vector<PickupSpawn> rewards; // granted once when the wave starts
    std::vector<GridPos> spawnPoints; // empty = use all of the level's points
};

// A level's complete authored definition: the ASCII layout plus the few
// per-level parameters the cell loader needs. Everything a level IS lives
// here (see Levels.cpp for the grid legend); engine code reads LevelDef
// instead of hardcoding coordinates.
struct LevelDef {
    static const int SIZE = 30;  // grid is SIZE rows of SIZE chars

    const char* rows[SIZE];     // ASCII layout, one string per row (y)
    float wallHeight;           // height of '#', 'S', 'E' wall tiles
    float wallCH;               // ceiling Z for those full-height walls

    float playerSpawnX;         // where the player starts (sub-tile coords)
    float playerSpawnY;
    float playerSpawnAngle;     // initial yaw, radians
    float spawnExitY;           // crossing this y closes the start room;
                                // negative = no spawn room on this level

    std::vector<WaveDef> waves;              // played in order
    std::vector<GridPos> finalWalls;         // opened once all waves are cleared
    std::vector<PickupSpawn> initialPickups; // on the floor when the level starts
};

// Registered levels, indexed the way loadLevel()/currentLevel use.
const LevelDef& getLevel(int index);
int levelCount();

#endif
