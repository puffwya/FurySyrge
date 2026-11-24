#ifndef MAP_H
#define MAP_H

#include <algorithm>
#include "LevelDef.h"

struct RayHit {
    int tileX;
    int tileY;
    float hitX;        // world X coordinate of ray intersection
    float hitY;        // world Y coordinate of ray intersection
    bool vertical;     // true if hit vertical wall
    float hitFraction; // 0-1 along the tile (horizontal or vertical)
    float hitHeight;   // world Z coordinate of impact
};

class Map {
public:
    static const int SIZE = 30;
    static const int CHUNK_SIZE = 10;

    // -----------------------------
    // Tile type for readability
    // -----------------------------
    enum class TileType {
        Empty = 0,
        Wall  = 1,
        Spawn = 2
    };

    // -----------------------------
    // Full cell data structure
    // -----------------------------
    struct Cell {
        TileType type;
        float height; // 0 = floor, 1 = normal wall, 0> & <1 = step, <0 = pit
        float cHeight; // Ceiling Height 
        bool isExit = false;
        bool isEscape = false;
        bool isSliding = false; // for sliding walls
        bool isLava = false; // For lava pit
    };

    // -----------------------------
    // Editable raw tile map
    // -----------------------------
    int rawMap[SIZE][SIZE] = {};  // rebuilt from the LevelDef grid on load

    // -----------------------------
    // Current active level
    // -----------------------------
    int currentLevel = 0;

    // -----------------------------
    // Struct-based map
    // -----------------------------
    Cell data[SIZE][SIZE];

    // -----------------------------
    // Construct: auto-build struct map (loads level 1 by default)
    // -----------------------------
    Map() {
        loadLevel(0);
    }

    // -----------------------------
    // Load a level from its LevelDef: the ASCII grid becomes cells, and
    // rawMap (bullet raycasts) is derived from the same cells so both
    // consumers can never disagree about the layout.
    // -----------------------------
    void loadLevel(int levelIndex) {
        currentLevel = levelIndex;
        const LevelDef& lvl = getLevel(levelIndex);

        for (int y = 0; y < SIZE; ++y) {
            for (int x = 0; x < SIZE; ++x) {
                Cell c{};
                c.type = TileType::Empty;
                c.height = 0.0f;
                c.cHeight = 1.0f;

                switch (lvl.rows[y][x]) {
                    case '#':
                        c.type = TileType::Wall;
                        c.height = lvl.wallHeight;
                        break;
                    case 'S':
                        c.type = TileType::Wall;
                        c.height = lvl.wallHeight;
                        c.isSliding = true;
                        break;
                    case 'E':
                        c.type = TileType::Wall;
                        c.height = lvl.wallHeight;
                        c.isExit = true;
                        break;
                    case 'W':
                        c.type = TileType::Wall;
                        c.height = 0.5f;
                        break;
                    case 'C':
                        c.type = TileType::Wall;
                        c.height = 0.25f;
                        break;
                    case '~':
                        c.type = TileType::Wall;
                        c.height = -0.25f;
                        c.isLava = true;
                        break;
                    case 's':
                        c.type = TileType::Spawn;
                        break;
                    case 'x':
                        c.isEscape = true;
                        break;
                    case 'X':
                        c.isEscape = true;
                        c.height = 0.9f;  // escape on the raised second floor
                        break;
                    case '1': c.height = 0.25f; break;
                    case '2': c.height = 0.5f;  break;
                    case '3': c.height = 0.75f; break;
                    case '4': c.height = 0.9f;  break;
                    case '!': c.height = -0.5f; c.isLava = true; break;
                    case '.':
                    default:
                        break;
                }

                // Ceiling derivation, the rule both levels always used:
                // open above pits, one unit of headroom over raised floors,
                // full-height walls take the level's wall ceiling.
                if (c.height < 0.0f)
                    c.cHeight = 10.0f;
                else if (c.height > 0.0f && c.height < 1.0f)
                    c.cHeight = c.height + 1.0f;
                else if (c.height >= 1.0f)
                    c.cHeight = lvl.wallCH;

                data[y][x] = c;
                rawMap[y][x] =
                    c.type == TileType::Spawn ? 2 :
                    c.type == TileType::Wall  ? 1 : 0;
            }
        }
    }

    // Safe accessor (read-only)
    inline const Cell& get(int x, int y) const {
        x = std::max(0, std::min(x, SIZE - 1));
        y = std::max(0, std::min(y, SIZE - 1));
        return data[y][x];
    }

    // Safe accessor (mutable)
    inline Cell& get(int x, int y) {
        x = std::max(0, std::min(x, SIZE - 1));
        y = std::max(0, std::min(y, SIZE - 1));
        return data[y][x];
    }

    // -----------------------------
    // Chunk querying
    // -----------------------------
    inline int getChunkID(int x, int y) const {
        x = std::max(0, std::min(x, SIZE - 1));
        y = std::max(0, std::min(y, SIZE - 1));
        int chunkX = x / CHUNK_SIZE;
        int chunkY = y / CHUNK_SIZE;
        return chunkY * 2 + chunkX;
    }

    // Simple 2D grid DDA
    bool raycastWall(float x, float y, float dirX, float dirY, float maxRange, RayHit& outHit) const {
        int mapX = int(x);
        int mapY = int(y);

        float deltaDistX = (dirX != 0) ? std::abs(1.0f / dirX) : 1e30f;
        float deltaDistY = (dirY != 0) ? std::abs(1.0f / dirY) : 1e30f;

        int stepX = (dirX < 0) ? -1 : 1;
        int stepY = (dirY < 0) ? -1 : 1;

        float sideDistX = (dirX < 0) ? (x - mapX) * deltaDistX : (mapX + 1.0f - x) * deltaDistX;
        float sideDistY = (dirY < 0) ? (y - mapY) * deltaDistY : (mapY + 1.0f - y) * deltaDistY;

        bool hit = false;
        bool verticalHit = false;

        float distance = 0.0f;

        while (!hit && distance < maxRange) {
            if (sideDistX < sideDistY) {
                sideDistX += deltaDistX;
                mapX += stepX;
                verticalHit = true;
                distance = sideDistX - deltaDistX;
            } else {
                sideDistY += deltaDistY;
                mapY += stepY;
                verticalHit = false;
                distance = sideDistY - deltaDistY;
            }

            if (mapY >= 0 && mapY < SIZE && mapX >= 0 && mapX < SIZE && rawMap[mapY][mapX] != 0) {
                hit = true;
            }
        }

        if (!hit) return false;

        // Compute exact intersection position in world space
        float hitX = x + dirX * distance;
        float hitY = y + dirY * distance;

        outHit.tileX = mapX;
        outHit.tileY = mapY;
        outHit.vertical = verticalHit;
        if (verticalHit) {
            outHit.hitFraction = hitY - mapY;  // normal
            if (dirX > 0) outHit.hitFraction = 1.0f - outHit.hitFraction; // flip if coming from right
        } else {
            outHit.hitFraction = hitX - mapX;  // normal
            if (dirY > 0) outHit.hitFraction = 1.0f - outHit.hitFraction; // flip if coming from bottom
        }
        outHit.hitHeight = 0.5f; // just use player height for now

        return true;
    }

    // Map.h (inside class Map)
    inline bool isVerticalWall(int tileX, int tileY, float hitFraction, float rayDirX, float rayDirY) const {
        // Basic approach: compare delta along X vs Y
        // If the ray hits closer to a vertical edge, treat as vertical wall
        // If the ray hits closer to a horizontal edge, treat as horizontal wall

        // If hitFraction is near 0 or 1 along X/Y we can decide
        // A more robust approach: check which neighbor tile is empty to determine orientation
        int left  = std::max(0, tileX-1);
        int right = std::min(SIZE-1, tileX+1);
        int top   = std::max(0, tileY-1);
        int bottom= std::min(SIZE-1, tileY+1);

        // Check neighbors to see which side has a wall
        if (rawMap[tileY][left] == 0 || rawMap[tileY][right] == 0) return true;  // vertical wall
        if (rawMap[top][tileX] == 0 || rawMap[bottom][tileX] == 0) return false; // horizontal wall

        // fallback: pick based on ray direction
        return std::fabs(rayDirX) > std::fabs(rayDirY);
    }
};

#endif

