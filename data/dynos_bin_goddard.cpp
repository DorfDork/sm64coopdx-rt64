#include <stdio.h>
#include <array>
#include "dynos.cpp.h"

static constexpr std::array<const char*, CT_MAX> sCharacterHeadNames = {
    "mario_head.gdbin",
    "luigi_head.gdbin",
    "toad_head.gdbin",
    "waluigi_head.gdbin",
    "wario_head.gdbin",
};

static_assert(sCharacterHeadNames.size() == CT_MAX, "sCharacterHeadNames size must match CT_MAX");

static SysPath& Goddard_ActiveMarioHeadBin() {
    static SysPath sActive = "";
    return sActive;
}

static u8*& Goddard_ActiveMarioHeadBinData() {
    static u8* sData = NULL;
    return sData;
}

static s32& Goddard_ActiveMarioHeadBinSize() {
    static s32 sSize = 0;
    return sSize;
}

const u8* goddard_get_active_mario_head_bin_data() {
    return Goddard_ActiveMarioHeadBinData();
}

s32 goddard_get_active_mario_head_bin_size() {
    return Goddard_ActiveMarioHeadBinSize();
}

const SysPath& goddard_get_active_mario_head_bin() {
    return Goddard_ActiveMarioHeadBin();
}

void goddard_set_active_mario_head_bin(const SysPath& path) {
    Goddard_ActiveMarioHeadBin() = path;
}

void goddard_mod_shutdown() {
    Goddard_ActiveMarioHeadBin() = "";

    if (Goddard_ActiveMarioHeadBinData() != NULL) {
        free(Goddard_ActiveMarioHeadBinData());
        Goddard_ActiveMarioHeadBinData() = NULL;
    }
    Goddard_ActiveMarioHeadBinSize() = 0;
}

void Goddard_LoadActiveMarioHeadBinIfNeeded() {
    const SysPath& _Path = goddard_get_active_mario_head_bin();
    if (_Path.empty()) {
        goddard_mod_shutdown();
        return;
    }

    // Already loaded from this path
    // (We store only one active bin, so path equality is enough.)
    static SysPath sLoadedPath = "";
    if (sLoadedPath == _Path && Goddard_ActiveMarioHeadBinData() != NULL && Goddard_ActiveMarioHeadBinSize() > 0) {
        return;
    }

    // Clear previous
    if (Goddard_ActiveMarioHeadBinData() != NULL) {
        free(Goddard_ActiveMarioHeadBinData());
        Goddard_ActiveMarioHeadBinData() = NULL;
    }
    Goddard_ActiveMarioHeadBinSize() = 0;
    sLoadedPath = "";

    BinFile* _File = BinFile::OpenR(_Path.c_str());
    if (_File == NULL) {
        printf("[DynOS] Goddard: failed to open mario_head.gdbin: %s\n", _Path.c_str());
        return;
    }

    s32 _Size = _File->Size();
    if (_Size <= 0) {
        BinFile::Close(_File);
        printf("[DynOS] Goddard: mario_head.gdbin is empty: %s\n", _Path.c_str());
        return;
    }

    u8* _Data = (u8*) calloc(_Size, 1);
    _File->Read<u8>(_Data, _Size);
    BinFile::Close(_File);

    Goddard_ActiveMarioHeadBinData() = _Data;
    Goddard_ActiveMarioHeadBinSize() = _Size;
    sLoadedPath = _Path;

    printf("[DynOS] Goddard: loaded mario_head.gdbin (%d bytes) from %s\n", _Size, _Path.c_str());
}

SysPath Goddard_CalculateActiveMarioHeadBin() {
    // Get the character index from save file
    u32 charIndex = (configPlayerModel >= CT_MAX) ? CT_MARIO : configPlayerModel;
    
    // First try to find a character-specific head, then fall back to mario_head
    for (auto& _Pack : DynosPacks()) {
        if (!_Pack.mEnabled) { continue; }
        
        // Try character-specific head first
        if (!_Pack.mGoddardCharacterHeadBins[charIndex].empty()) {
            return _Pack.mGoddardCharacterHeadBins[charIndex];
        }
        // Fall back to mario_head (index 0) or legacy mGoddardMarioHeadBin
        else if (!_Pack.mGoddardCharacterHeadBins[0].empty()) {
            return _Pack.mGoddardCharacterHeadBins[0];
        }
        else if (!_Pack.mGoddardMarioHeadBin.empty()) {
            return _Pack.mGoddardMarioHeadBin;
        }
    }
    
    return "";
}

const SysPath& pack_get_goddard_mario_head_bin(PackData* aPackData) {
    static SysPath sEmpty = "";
    if (aPackData == NULL) { return sEmpty; }
    return aPackData->mGoddardMarioHeadBin;
}

void goddard_scan_pack_bins(struct PackData* aPack) {
    // check for goddard
    // Pack layout: dynos/packs/<pack>/goddard/mario_head.gdbin
    // Also check for character-specific heads: luigi_head.gdbin, toad_head.gdbin, etc.
    for (u32 i = 0; i < sCharacterHeadNames.size(); i++) {
        SysPath _GoddardBin = fstring("%s/goddard/%s", aPack->mPath.c_str(), sCharacterHeadNames[i]);
        if (fs_sys_file_exists(_GoddardBin.c_str())) {
            aPack->mGoddardCharacterHeadBins[i] = _GoddardBin;
            printf("[DynOS] Goddard: found %s in pack %s\n", sCharacterHeadNames[i], aPack->mPath.c_str());
            
            // Also set mGoddardMarioHeadBin for backwards compatibility (use mario_head as default)
            if (i == 0) {
                aPack->mGoddardMarioHeadBin = _GoddardBin;
            }
        }
    }
}

