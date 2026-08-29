#include <stdio.h>
#include <array>
#include "dynos.cpp.h"

extern "C" {
#include "pc/network/network_player.h"
}

static constexpr std::array<const char *, CT_MAX> sCharacterHeadNames = {
    "mario_head",
    "luigi_head",
    "toad_head",
    "waluigi_head",
    "wario_head",
};

static_assert(sCharacterHeadNames.back() != nullptr, "sCharacterHeadNames needs an entry for every character up to CT_MAX");

static SysPath sHeadPath = "";
static u8 *sHeadData = NULL;
static s32 sHeadSize = 0;

// Heads found in activated mod folders, keyed by file name. DynOS packs are 
// searched first, so a pack the user explicitly enabled wins over a mod's head.
static std::map<std::string, SysPath> sModHeads;

static std::string sHeadOverride = "";

const u8 *DynOS_Goddard_GetData() {
    return sHeadData;
}

s32 DynOS_Goddard_GetSize() {
    return sHeadSize;
}

static SysPath DynOS_Goddard_FindHead(const std::string &aHeadName) {
    if (aHeadName.empty()) { return ""; }

    for (auto &_Pack : DynosPacks()) {
        if (!_Pack.mEnabled) { continue; }
        auto _PackIt = _Pack.mGoddardHeads.find(aHeadName);
        if (_PackIt != _Pack.mGoddardHeads.end()) { return _PackIt->second; }
    }

    auto _ModIt = sModHeads.find(aHeadName);
    if (_ModIt != sModHeads.end()) { return _ModIt->second; }

    return "";
}

static void DynOS_Goddard_LoadHead() {
    if (sHeadData != NULL) {
        free(sHeadData);
        sHeadData = NULL;
    }
    sHeadSize = 0;

    if (sHeadPath.empty()) { return; }

    BinFile *_File = BinFile::OpenR(sHeadPath.c_str());
    if (_File == NULL) {
        Print("[DynOS] Goddard: failed to open %s", sHeadPath.c_str());
        return;
    }

    s32 _Size = _File->Size();
    if (_Size <= 0) {
        BinFile::Close(_File);
        Print("[DynOS] Goddard: %s is empty", sHeadPath.c_str());
        return;
    }

    u8 *_Data = (u8 *) malloc(_Size);
    _File->Read<u8>(_Data, _Size);
    BinFile::Close(_File);

    sHeadData = _Data;
    sHeadSize = _Size;

    Print("[DynOS] Goddard: loaded %s (%d bytes)", sHeadPath.c_str(), _Size);
}

void DynOS_Goddard_SetHead(const char *aHeadName) {
    sHeadOverride = (aHeadName != NULL) ? aHeadName : "";
}

void DynOS_Goddard_Update() {
    u8 _CharacterIndex = gNetworkPlayers[0].overrideModelIndex;
    if (_CharacterIndex >= CT_MAX) { _CharacterIndex = CT_MARIO; }

    SysPath _HeadPath = DynOS_Goddard_FindHead(sHeadOverride);
    if (_HeadPath.empty()) { _HeadPath = DynOS_Goddard_FindHead(sCharacterHeadNames[_CharacterIndex]); }
    if (_HeadPath.empty()) { _HeadPath = DynOS_Goddard_FindHead(sCharacterHeadNames[CT_MARIO]); }
    if (_HeadPath == sHeadPath) { return; }

    sHeadPath = _HeadPath;
    DynOS_Goddard_LoadHead();
}

void DynOS_Goddard_ModShutdown() {
    sModHeads.clear();
    sHeadPath = "";

    if (sHeadData != NULL) {
        free(sHeadData);
        sHeadData = NULL;
    }
    sHeadSize = 0;
}

void DynOS_Goddard_ScanPack(struct PackData *aPack) {
    SysPath _GoddardFolder = fstring("%s/goddard", aPack->mPath.c_str());
    DIR *_GoddardDir = opendir(_GoddardFolder.c_str());
    if (!_GoddardDir) { return; }

    struct dirent *_GoddardEnt = NULL;
    while ((_GoddardEnt = readdir(_GoddardDir)) != NULL) {
        s32 length = strlen(_GoddardEnt->d_name);

        // check for goddard heads
        if (length > 3 && !strncmp(&_GoddardEnt->d_name[length - 3], ".gd", 3)) {
            std::string _HeadName(_GoddardEnt->d_name, length - 3);
            aPack->mGoddardHeads[_HeadName] = fstring("%s/%s", _GoddardFolder.c_str(), _GoddardEnt->d_name);
        }
    }

    closedir(_GoddardDir);
}

void DynOS_Goddard_AddHead(const SysPath &aFilename, const char *aHeadName) {
    sModHeads[aHeadName] = aFilename;
}
