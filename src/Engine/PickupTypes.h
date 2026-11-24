#ifndef PICKUPTYPES_H
#define PICKUPTYPES_H

// What a pickup grant is. Leaf header so level definitions can list
// initial pickups and wave rewards without pulling in PickupManager.
enum class PickupType {
    Health,
    Armor,
    Ammo,
    Weapon
};

#endif
