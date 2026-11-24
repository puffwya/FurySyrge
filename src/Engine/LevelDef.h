#ifndef LEVELDEF_H
#define LEVELDEF_H

// A level's complete authored definition: the ASCII layout plus the few
// per-level parameters the cell loader needs. Everything a level IS lives
// here (see Levels.cpp for the grid legend); engine code reads LevelDef
// instead of hardcoding coordinates.
struct LevelDef {
    static const int SIZE = 30;  // grid is SIZE rows of SIZE chars

    const char* rows[SIZE];     // ASCII layout, one string per row (y)
    float wallHeight;           // height of '#', 'S', 'E' wall tiles
    float wallCH;               // ceiling Z for those full-height walls
};

// Registered levels, indexed the way loadLevel()/currentLevel use.
const LevelDef& getLevel(int index);
int levelCount();

#endif
