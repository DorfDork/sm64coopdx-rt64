#include <stdio.h>
#include <array>
#include "dynos.cpp.h"

static constexpr std::array<const char *, CT_MAX> sCharacterHeadNames = {
    "mario_head",
    "luigi_head",
    "toad_head",
    "waluigi_head",
    "wario_head",
};

static_assert(sCharacterHeadNames.back() != nullptr, "sCharacterHeadNames needs an entry for every character up to CT_MAX");

static SysPath sActiveMarioHeadBin = "";
static u8 *sActiveMarioHeadBinData = NULL;
static s32 sActiveMarioHeadBinSize = 0;

// Heads found in activated mod folders, indexed by character. DynOS packs are searched
// first, so a pack the user explicitly enabled always wins over one a mod happens to ship.
static SysPath sModCharacterHeadBins[CT_MAX];

// Character the active head was last resolved for. CT_MAX means "not resolved yet".
static u32 sActiveCharacterIndex = CT_MAX;

const u8 *DynOS_Goddard_GetActiveMarioHeadBinData() {
    return sActiveMarioHeadBinData;
}

s32 DynOS_Goddard_GetActiveMarioHeadBinSize() {
    return sActiveMarioHeadBinSize;
}

static u32 DynOS_Goddard_GetCharacterIndex() {
    return (configPlayerModel >= CT_MAX) ? CT_MARIO : configPlayerModel;
}

static const SysPath &DynOS_Goddard_FindHeadBin(u32 aCharacterIndex) {
    static const SysPath sNone = "";

    for (auto &_Pack : DynosPacks()) {
        if (!_Pack.mEnabled) { continue; }
        if (!_Pack.mGoddardCharacterHeadBins[aCharacterIndex].empty()) {
            return _Pack.mGoddardCharacterHeadBins[aCharacterIndex];
        }
        if (!_Pack.mGoddardCharacterHeadBins[CT_MARIO].empty()) {
            return _Pack.mGoddardCharacterHeadBins[CT_MARIO];
        }
    }

    if (!sModCharacterHeadBins[aCharacterIndex].empty()) {
        return sModCharacterHeadBins[aCharacterIndex];
    }
    if (!sModCharacterHeadBins[CT_MARIO].empty()) {
        return sModCharacterHeadBins[CT_MARIO];
    }

    return sNone;
}

static void DynOS_Goddard_LoadActiveMarioHeadBin() {
    if (sActiveMarioHeadBinData != NULL) {
        free(sActiveMarioHeadBinData);
        sActiveMarioHeadBinData = NULL;
    }
    sActiveMarioHeadBinSize = 0;

    if (sActiveMarioHeadBin.empty()) { return; }

    BinFile *_File = BinFile::OpenR(sActiveMarioHeadBin.c_str());
    if (_File == NULL) {
        Print("[DynOS] Goddard: failed to open %s", sActiveMarioHeadBin.c_str());
        return;
    }

    s32 _Size = _File->Size();
    if (_Size <= 0) {
        BinFile::Close(_File);
        Print("[DynOS] Goddard: %s is empty", sActiveMarioHeadBin.c_str());
        return;
    }

    u8 *_Data = (u8 *) malloc(_Size);
    _File->Read<u8>(_Data, _Size);
    BinFile::Close(_File);

    sActiveMarioHeadBinData = _Data;
    sActiveMarioHeadBinSize = _Size;

    Print("[DynOS] Goddard: loaded %s (%d bytes)", sActiveMarioHeadBin.c_str(), _Size);
}

// Re-resolves the head for the current character and reloads if the character has changed.
void DynOS_Goddard_RefreshActiveMarioHeadBin() {
    sActiveCharacterIndex = DynOS_Goddard_GetCharacterIndex();

    const SysPath &_HeadBin = DynOS_Goddard_FindHeadBin(sActiveCharacterIndex);
    if (_HeadBin == sActiveMarioHeadBin) { return; }

    sActiveMarioHeadBin = _HeadBin;
    DynOS_Goddard_LoadActiveMarioHeadBin();
}

void DynOS_Goddard_Update() {
    if (DynOS_Goddard_GetCharacterIndex() == sActiveCharacterIndex) { return; }
    DynOS_Goddard_RefreshActiveMarioHeadBin();
}

void DynOS_Goddard_ModShutdown() {
    for (SysPath &_HeadBin : sModCharacterHeadBins) {
        _HeadBin = "";
    }

    sActiveMarioHeadBin = "";
    sActiveCharacterIndex = CT_MAX;

    if (sActiveMarioHeadBinData != NULL) {
        free(sActiveMarioHeadBinData);
        sActiveMarioHeadBinData = NULL;
    }
    sActiveMarioHeadBinSize = 0;
}

void DynOS_Goddard_ScanPackBins(struct PackData *aPack) {
    for (u32 i = 0; i != CT_MAX; ++i) {
        SysPath _HeadBin = fstring("%s/goddard/%s.gd", aPack->mPath.c_str(), sCharacterHeadNames[i]);
        if (fs_sys_file_exists(_HeadBin.c_str())) {
            aPack->mGoddardCharacterHeadBins[i] = _HeadBin;
        }
    }
}

void DynOS_Goddard_AddModHead(const SysPath &aFilename, const char *aHeadName) {
    for (u32 i = 0; i != CT_MAX; ++i) {
        if (!strcmp(aHeadName, sCharacterHeadNames[i])) {
            sModCharacterHeadBins[i] = aFilename;
            DynOS_Goddard_RefreshActiveMarioHeadBin();
            return;
        }
    }
}
