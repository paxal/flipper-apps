#include "level.h"

IceTileKind ice_tile_kind(uint8_t raw) {
    if(raw >= ICE_TILE_WATER) {
        return IceTileWater;
    }
    if(raw >= ICE_TILE_WORM) {
        return IceTileWorm;
    }
    if(raw >= ICE_TILE_BLOCK) {
        return IceTileBlock;
    }
    if(raw >= ICE_TILE_WALL) {
        return IceTileWall;
    }
    if(raw == ICE_TILE_PAWN) {
        return IceTilePawn;
    }
    if(raw == ICE_TILE_EXIT_L || raw == ICE_TILE_EXIT_R) {
        return IceTileExit;
    }
    if(raw == ICE_TILE_MINE) {
        return IceTileMine;
    }
    if(raw >= ICE_TILE_PUSH_UP && raw <= ICE_TILE_PUSH_RIGHT) {
        return IceTilePush;
    }
    if(raw >= ICE_TILE_SWITCH1 && raw < ICE_TILE_SLOT0) {
        return IceTileSwitch;
    }
    if(raw >= ICE_TILE_SLOT0 && raw < ICE_TILE_DYNAMITE) {
        return IceTileSlot;
    }
    if(raw == ICE_TILE_DYNAMITE) {
        return IceTileDynamite;
    }
    return IceTileFloor;
}

bool ice_tile_deadly(uint8_t raw) {
    IceTileKind kind = ice_tile_kind(raw);
    return kind == IceTileWater || kind == IceTileWorm || kind == IceTileMine;
}

bool ice_tile_breakable(uint8_t raw) {
    return raw >= ICE_TILE_WALL && raw < ICE_TILE_BREAK_END;
}

IceOutcome ice_tile_outcome(uint8_t raw) {
    switch(ice_tile_kind(raw)) {
    case IceTileWater:
        return IceOutcomeWater;
    case IceTileWorm:
        return IceOutcomeWorm;
    default:
        return IceOutcomeBoom;
    }
}
