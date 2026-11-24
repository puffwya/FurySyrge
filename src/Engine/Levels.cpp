// Level definitions: one self-contained LevelDef per level.
//
// The ASCII grid is the whole layout -- every tile, height, flag and spawn
// point is visible in one picture. Cell ceilings are derived by the loader
// (Map::loadLevel): h<0 => 10 (open above), 0<h<1 => h+1, h==0 => 1.0,
// walls => wallCH.
//
// Legend:
//   #  wall at wallHeight       S  sliding wall (opens when its wave starts)
//   E  exit door wall           W  crate block   (wall, h=0.5)
//   C  low cover (wall, h=0.25) ~  lava rim      (wall, h=-0.25, burns)
//   .  floor h=0                x  escape tile (step off => level complete)
//   s  enemy spawn point        1/2/3  step h=0.25/0.5/0.75 (walkable floor)
//   4  raised floor h=0.9       !  lava pit     (floor, h=-0.5, burns)
//
// Waves, per-wave sliding doors, wave rewards, the final corridor doors,
// the starting pickups, the player spawn and the spawn-room exit line are
// all level data -- adding a level means adding one entry here, no engine
// code.
//
// Round-trip verified: gen_grid encodes the live Map state back to these
// chars and dumps every cell field; the output must match the pre-migration
// golden snapshot (single known deviation: level 2's spawn tile now also
// occupies rawMap, so bullets stop on it like level 1's spawn tiles).
#include "LevelDef.h"

static const LevelDef LEVELS[] = {
    {  // ---- Level 1: arena -- crate cover, lava rim, sliding wave rooms, south exit ----
        {  // ASCII grid
            "##############################",  // y=00
            "#####...####......####...#####",  // y=01
            "####......................####",  // y=02
            "#####...####......####...#####",  // y=03
            "##############SS##############",  // y=04
            "##############SS##############",  // y=05
            "######..................######",  // y=06
            "###..S..WWW........WWW..S..###",  // y=07
            "###..S..WWW.s......WWW.sS..###",  // y=08
            "###..S..WWW........WWW..S..###",  // y=09
            "######..................######",  // y=10
            "######..................######",  // y=11
            "######...C.~~~~~~~~.C...######",  // y=12
            "######.....~......~.....######",  // y=13
            "######.....~......~.....######",  // y=14
            "######.....~......~.....######",  // y=15
            "######.....~......~.....######",  // y=16
            "######...C.~~~~~~~~.C...######",  // y=17
            "######..................######",  // y=18
            "######..................######",  // y=19
            "###..S..WWW........WWW..S..###",  // y=20
            "###..Ss.WWW..#..#s.WWW..S..###",  // y=21
            "###..S..WWW........WWW..S..###",  // y=22
            "######..................######",  // y=23
            "##############SS##############",  // y=24
            "#############....#############",  // y=25
            "#############....#############",  // y=26
            "##############xx##############",  // y=27
            "##############EE##############",  // y=28
            "##############################",  // y=29
        },
        1.0f, 1.0f,  // wallHeight, wallCH
        14.5f, 2.5f, 0.0f, 10.0f,  // playerSpawnX/Y, angle, spawnExitY
        {  // waves (spawnPoints left empty = all "s" tiles)
        {  // wave 1: opening rush from the start room
            6.0f,
            { EnemyType::Base, EnemyType::Base, EnemyType::Base,
              EnemyType::Fast, EnemyType::Base, EnemyType::Fast },
            { {14,4}, {14,5}, {15,4}, {15,5} },
            {}
        },
        {  // wave 2: east door opens, shotgun behind it
            5.0f,
            { EnemyType::Base, EnemyType::Fast, EnemyType::Base,
              EnemyType::Fast, EnemyType::Base, EnemyType::Shooter,
              EnemyType::Base, EnemyType::Shooter },
            { {24,7}, {24,8}, {24,9} },
            { {25.5f, 8.5f, PickupType::Weapon, WeaponType::Shotgun} }
        },
        {  // wave 3: south-east door, health + armor reward
            5.0f,
            { EnemyType::Base, EnemyType::Shooter, EnemyType::Fast,
              EnemyType::Fast, EnemyType::Shooter, EnemyType::Base,
              EnemyType::Shooter, EnemyType::Base, EnemyType::Base,
              EnemyType::Fast },
            { {24,20}, {24,21}, {24,22} },
            { {25.0f, 21.5f, PickupType::Health, WeaponType::None},
              {26.0f, 21.5f, PickupType::Armor,  WeaponType::None} }
        },
        {  // wave 4: west door opens, MG behind it
            5.0f,
            { EnemyType::Fast, EnemyType::Shooter, EnemyType::Base,
              EnemyType::Shooter, EnemyType::Shooter, EnemyType::Base,
              EnemyType::Shooter, EnemyType::Base, EnemyType::Shooter,
              EnemyType::Fast, EnemyType::Shooter, EnemyType::Fast },
            { {5,7}, {5,8}, {5,9} },
            { {4.5f, 8.5f, PickupType::Weapon, WeaponType::Mg} }
        },
        {  // wave 5: tank finale, south-west door, health + armor reward
            5.0f,
            { EnemyType::Tank, EnemyType::Tank, EnemyType::Tank },
            { {5,20}, {5,21}, {5,22} },
            { {5.0f, 21.5f, PickupType::Health, WeaponType::None},
              {4.0f, 21.5f, PickupType::Armor,  WeaponType::None} }
        },
        },
        { {14,24}, {15,24} },  // finalWalls: corridor to the exit, opens after the last wave
        {  // initialPickups
            { 23.5f, 2.5f, PickupType::Health, WeaponType::None },
            { 5.5f, 2.5f, PickupType::Weapon, WeaponType::Pistol },
        },
    },
    {  // ---- Level 2: vertical arena -- central stairs, raised second floor, ring walls ----
        {  // ASCII grid
            "##############################",  // y=00
            "#............................#",  // y=01
            "#............................#",  // y=02
            "#............................#",  // y=03
            "#............................#",  // y=04
            "#....s.......................#",  // y=05
            "#............................#",  // y=06
            "#............................#",  // y=07
            "#............................#",  // y=08
            "#............................#",  // y=09
            "#............................#",  // y=10
            "#............................#",  // y=11
            "#............................#",  // y=12
            "#............1111............#",  // y=13
            "#............2222............#",  // y=14
            "#############3333#############",  // y=15
            "#4444444444444444444444444444#",  // y=16
            "#4444444444444444444444444444#",  // y=17
            "#4444444441144444441144444444#",  // y=18
            "#4444444444444444444444444444#",  // y=19
            "#4444444444444444444444444444#",  // y=20
            "#4444444444444444444444444444#",  // y=21
            "#4444444444441144!!!444444444#",  // y=22
            "#4444444444444444!!!444444444#",  // y=23
            "#4444444444444444444444444444#",  // y=24
            "#4444444444444444444444444444#",  // y=25
            "#4444444441444444444144444444#",  // y=26
            "#4444444444444444444444444444#",  // y=27
            "##############################",  // y=28
            "..............................",  // y=29
        },
        1.9f, 2.9f,  // wallHeight, wallCH
        5.5f, 5.5f, 0.0f, -1.0f,  // playerSpawnX/Y, angle, spawnExitY
        {},  // waves (given level-2 waves with the next-level flow)
        {},  // finalWalls
        {},  // initialPickups
    },
};

const LevelDef& getLevel(int index) {
    if (index < 0) index = 0;
    if (index >= (int)(sizeof(LEVELS) / sizeof(LEVELS[0])))
        index = (int)(sizeof(LEVELS) / sizeof(LEVELS[0])) - 1;
    return LEVELS[index];
}

int levelCount() {
    return (int)(sizeof(LEVELS) / sizeof(LEVELS[0]));
}
