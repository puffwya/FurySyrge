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
// Round-trip verified: gen_grid encodes the live Map state back to these
// chars and dumps every cell field; the output must match the pre-migration
// golden snapshot (single known deviation: level 2's spawn tile now also
// occupies rawMap, so bullets stop on it like level 1's spawn tiles).
#include "LevelDef.h"

static const LevelDef LEVELS[] = {
    {  // ---- Level 1: arena -- crate cover, lava rim, sliding wave rooms, south exit ----
        {
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
    },
    {  // ---- Level 2: vertical arena -- central stairs, raised second floor, ring walls ----
        {
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
