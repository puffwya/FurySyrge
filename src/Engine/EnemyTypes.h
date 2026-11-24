#ifndef ENEMYTYPES_H
#define ENEMYTYPES_H

// Enemy archetypes. Leaf header (no Map/SDL includes) so level definitions
// can list wave compositions without include cycles.
enum class EnemyType {
    Base,
    Fast,
    Tank,
    Shooter
};

#endif
