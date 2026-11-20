#include "MapToSegments.h"
#include "Map.h"

std::vector<GridSegment> buildSegmentsFromGrid(const Map& map)
{
    std::vector<GridSegment> edges;

    // One segment per boundary whose two surfaces differ in height. The higher
    // tile owns the face (it also owns the texture choice), so every boundary
    // is emitted exactly once and drawn exactly once. Equal-height boundaries
    // produce nothing at all - and map borders produce nothing either, because
    // Map::get clamps the out-of-range neighbour back onto the tile itself.
    for (int y = 0; y < Map::SIZE; ++y) {
        for (int x = 0; x < Map::SIZE; ++x) {
            const float h = map.get(x, y).height;

            // Top edge: north
            {
                const Map::Cell& nb = map.get(x, y - 1);
                if (h > nb.height)
                    edges.push_back({ {float(x),   float(y)}, {float(x+1), float(y)},
                                      x, y, h, nb.height, GridSegment::Dir::North });
            }

            // Bottom edge: south
            {
                const Map::Cell& nb = map.get(x, y + 1);
                if (h > nb.height)
                    edges.push_back({ {float(x+1), float(y+1)}, {float(x),   float(y+1)},
                                      x, y, h, nb.height, GridSegment::Dir::South });
            }

            // Left edge: west
            {
                const Map::Cell& nb = map.get(x - 1, y);
                if (h > nb.height)
                    edges.push_back({ {float(x),   float(y+1)}, {float(x),   float(y)},
                                      x, y, h, nb.height, GridSegment::Dir::West });
            }

            // Right edge: east
            {
                const Map::Cell& nb = map.get(x + 1, y);
                if (h > nb.height)
                    edges.push_back({ {float(x+1), float(y)}, {float(x+1), float(y+1)},
                                      x, y, h, nb.height, GridSegment::Dir::East });
            }
        }
    }

    return edges;
}


