#pragma once
// BinfoTypes.h
// Shared types for the .binfo entity I/O system.
// Mirrors Source's CEntityOutput / CBaseEntityOutput model.
// -------------------------------------------------------
// Wire format in entities.binfo:
//
//   func_door:
//   OnSpawn  > self       > SetOpen(false)
//   OnTrigger > self      > SetOpen(true)
//   OnTrigger > doorbell  > PlaySound("door_open.wav")
//   RETURN
//
// Columns:
//   [OutputName] > [TargetName] > [InputName]([Param])
//
//   OutputName  — event fired by THIS entity  (OnSpawn, OnTrigger, OnDeath …)
//   TargetName  — "self" or named entity targetname from BSP KVs
//   InputName   — action called on the target (SetOpen, PlaySound, Kill …)
//   Param       — optional string argument, may be empty

#include <string>
#include <vector>
#include <unordered_map>

namespace veex {

// One wired connection — exactly mirrors Source's CEntityConnection.
struct EntityConnection {
    std::string outputName;  // e.g. "OnTrigger"
    std::string targetName;  // e.g. "self" or "door_sound"
    std::string inputName;   // e.g. "PlaySound"
    std::string param;       // e.g. "door_open.wav"  (may be empty)
};

// All connections defined for one classname.
// Key: classname string.
// Value: flat list of connections (may have multiple per output).
using ConnectionList = std::vector<EntityConnection>;
using BinfoTable     = std::unordered_map<std::string, ConnectionList>;

} // namespace veex
