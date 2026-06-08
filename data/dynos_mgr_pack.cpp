#include <deque>
#include <stdio.h>
#include "dynos.cpp.h"
extern "C" {
#include "engine/graph_node.h"
}

#define MAX_CHARACTER_HEADS 16
static const char* sCharacterHeadNames[MAX_CHARACTER_HEADS] = {
    "mario_head.gdbin",
    "luigi_head.gdbin",
    "toad_head.gdbin",
    "waluigi_head.gdbin",
    "wario_head.gdbin",
    NULL
};

static std::deque<PackData>& DynosPacks() {
    static std::deque<PackData> sDynosPacks;
    return sDynosPacks;
}

static SysPath& DynOS_Goddard_ActiveMarioHeadBin() {
    static SysPath sActive = "";
    return sActive;
}

static u8*& DynOS_Goddard_ActiveMarioHeadBinData() {
    static u8* sData = NULL;
    return sData;
}

static s32& DynOS_Goddard_ActiveMarioHeadBinSize() {
    static s32 sSize = 0;
    return sSize;
}

const u8* DynOS_Goddard_GetActiveMarioHeadBinData() {
    return DynOS_Goddard_ActiveMarioHeadBinData();
}

s32 DynOS_Goddard_GetActiveMarioHeadBinSize() {
    return DynOS_Goddard_ActiveMarioHeadBinSize();
}

const SysPath& DynOS_Goddard_GetActiveMarioHeadBin() {
    return DynOS_Goddard_ActiveMarioHeadBin();
}

void DynOS_Goddard_ModShutdown() {
    DynOS_Goddard_ActiveMarioHeadBin() = "";

    if (DynOS_Goddard_ActiveMarioHeadBinData() != NULL) {
        free(DynOS_Goddard_ActiveMarioHeadBinData());
        DynOS_Goddard_ActiveMarioHeadBinData() = NULL;
    }
    DynOS_Goddard_ActiveMarioHeadBinSize() = 0;
}

static void DynOS_Goddard_LoadActiveMarioHeadBinIfNeeded() {
    const SysPath& _Path = DynOS_Goddard_GetActiveMarioHeadBin();
    if (_Path.empty()) {
        DynOS_Goddard_ModShutdown();
        return;
    }

    // Already loaded from this path
    // (We store only one active bin, so path equality is enough.)
    static SysPath sLoadedPath = "";
    if (sLoadedPath == _Path && DynOS_Goddard_ActiveMarioHeadBinData() != NULL && DynOS_Goddard_ActiveMarioHeadBinSize() > 0) {
        return;
    }

    // Clear previous
    if (DynOS_Goddard_ActiveMarioHeadBinData() != NULL) {
        free(DynOS_Goddard_ActiveMarioHeadBinData());
        DynOS_Goddard_ActiveMarioHeadBinData() = NULL;
    }
    DynOS_Goddard_ActiveMarioHeadBinSize() = 0;
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

    DynOS_Goddard_ActiveMarioHeadBinData() = _Data;
    DynOS_Goddard_ActiveMarioHeadBinSize() = _Size;
    sLoadedPath = _Path;

    printf("[DynOS] Goddard: loaded mario_head.gdbin (%d bytes) from %s\n", _Size, _Path.c_str());
}

extern "C" void DynOS_Goddard_RecomputeActiveMarioHeadBin() {
    DynOS_Goddard_ActiveMarioHeadBin() = "";
    
    // Get the character index from save file
    s32 charIndex = (configPlayerModel >= CT_MAX) ? CT_MARIO : configPlayerModel;
    
    // First try to find a character-specific head, then fall back to mario_head
    for (auto& _Pack : DynosPacks()) {
        if (!_Pack.mEnabled) { continue; }
        
        // Try character-specific head first
        if (charIndex > 0 && charIndex < MAX_CHARACTER_HEADS && !_Pack.mGoddardCharacterHeadBins[charIndex].empty()) {
            DynOS_Goddard_ActiveMarioHeadBin() = _Pack.mGoddardCharacterHeadBins[charIndex];
        }
        // Fall back to mario_head (index 0) or legacy mGoddardMarioHeadBin
        else if (!_Pack.mGoddardCharacterHeadBins[0].empty()) {
            DynOS_Goddard_ActiveMarioHeadBin() = _Pack.mGoddardCharacterHeadBins[0];
        }
        else if (!_Pack.mGoddardMarioHeadBin.empty()) {
            DynOS_Goddard_ActiveMarioHeadBin() = _Pack.mGoddardMarioHeadBin;
        }
    }

    if (DynOS_Goddard_ActiveMarioHeadBin().empty()) {
        printf("[DynOS] Goddard: no active head.gdbin (no enabled packs provide it)\n");
    } else {
        const char* charName = (charIndex < MAX_CHARACTER_HEADS && sCharacterHeadNames[charIndex]) ? sCharacterHeadNames[charIndex] : "mario_head.gdbin";
        printf("[DynOS] Goddard: active head for character %d (%s): %s\n", charIndex, charName, DynOS_Goddard_ActiveMarioHeadBin().c_str());
    }

    DynOS_Goddard_LoadActiveMarioHeadBinIfNeeded();
}

const SysPath& DynOS_Pack_GetGoddardMarioHeadBin(PackData* aPackData) {
    static SysPath sEmpty = "";
    if (aPackData == NULL) { return sEmpty; }
    return aPackData->mGoddardMarioHeadBin;
}

static void ScanPackBins(struct PackData* aPack) {
    DIR *_PackDir = opendir(aPack->mPath.c_str());
    if (!_PackDir) { return; }

    struct dirent *_PackEnt = NULL;
    while ((_PackEnt = readdir(_PackDir)) != NULL) {
        // Skip . and ..
        if (SysPath(_PackEnt->d_name) == ".") continue;
        if (SysPath(_PackEnt->d_name) == "..") continue;

        SysPath _FileName = fstring("%s/%s", aPack->mPath.c_str(), _PackEnt->d_name);
        s32 length = strlen(_PackEnt->d_name);

        // check for actors
        if (length > 4 && !strncmp(&_PackEnt->d_name[length - 4], ".bin", 4)) {
            String _ActorName = _PackEnt->d_name;
            _ActorName[length - 4] = '\0';
            DynOS_Actor_LoadFromBinary(aPack->mPath, _ActorName.begin(), _FileName, true);
        }

        // check for textures
        if (length > 4 && !strncmp(&_PackEnt->d_name[length - 4], ".tex", 4)) {
            String _TexName = _PackEnt->d_name;
            _TexName[length - 4] = '\0';
            DynOS_Tex_LoadFromBinary(aPack->mPath, _FileName, _TexName.begin(), true);
        }
    }

    // check for goddard
    // Pack layout: dynos/packs/<pack>/goddard/mario_head.gdbin
    // Also check for character-specific heads: luigi_head.gdbin, toad_head.gdbin, etc.
    for (s32 i = 0; sCharacterHeadNames[i] != NULL && i < MAX_CHARACTER_HEADS; i++) {
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

static void DynOS_Pack_ActivateActor(s32 aPackIndex, std::pair<std::string, GfxData *> &pair) {
    const char* aActorName = pair.first.c_str();
    GfxData* aGfxData = pair.second;

    auto& geoNode = *(aGfxData->mGeoLayouts.end() - 1);
    u32 id = 0;
    GraphNode* graphNode = DynOS_Model_LoadGeo(&id, MODEL_POOL_PERMANENT, geoNode->mData, true);
    if (graphNode == NULL) { return; }

    const void* georef = DynOS_Builtin_Actor_GetFromName(aActorName);
    graphNode->georef = georef;

    ActorGfx actorGfx;
    actorGfx.mGfxData   = aGfxData;
    actorGfx.mGraphNode = graphNode;
    actorGfx.mPackIndex = aPackIndex;

    for (const auto &vtxNode : aGfxData->mVertices) {
        if (vtxNode->mFlags & GRAPH_EXTRA_FORCE_3D) {
            actorGfx.mGraphNode->extraFlags |= GRAPH_EXTRA_FORCE_3D;
            break;
        }
    }

    DynOS_Actor_Valid(georef, actorGfx);
}

static void DynOS_Pack_DeactivateActor(s32 aPackIndex, std::pair<std::string, GfxData *> &pair) {
    const char* aActorName = pair.first.c_str();
    const void* georef = DynOS_Builtin_Actor_GetFromName(aActorName);
    DynOS_Actor_Invalid(georef, aPackIndex);

    // figure out which actor to replace it with
    std::pair<std::string, GfxData *> *_Replacement = NULL;
    s32 _ReplacementPackIndex = 0;
    for (auto& _Pack : DynosPacks()) {
        if (!_Pack.mEnabled) { continue; }
        auto _Tmp = DynOS_Pack_GetActor(&_Pack, aActorName);
        if (_Tmp != NULL) {
            _Replacement = _Tmp;
            _ReplacementPackIndex = _Pack.mIndex;
        }
    }
    if (_Replacement != NULL) {
        DynOS_Pack_ActivateActor(_ReplacementPackIndex, *_Replacement);
    }
}

s32 DynOS_Pack_GetCount() {
    return DynosPacks().size();
}

void DynOS_Pack_SetEnabled(PackData* aPack, bool aEnabled) {
    if (aPack == NULL) { return; }
    aPack->mEnabled = aEnabled;

    if (aEnabled && !aPack->mLoaded) {
        ScanPackBins(aPack);
        aPack->mLoaded = true;
    }

    if (aEnabled) {
        for (auto& pair : aPack->mGfxData) {
            DynOS_Pack_ActivateActor(aPack->mIndex, pair);
        }
        for (auto& _Tex : aPack->mTextures) {
            DynOS_Tex_Activate(_Tex, false);
        }
    } else {
        for (auto& pair : aPack->mGfxData) {
            DynOS_Pack_DeactivateActor(aPack->mIndex, pair);
        }
        for (auto& _Tex : aPack->mTextures) {
            DynOS_Tex_Deactivate(_Tex);
        }
    }

    DynOS_Goddard_RecomputeActiveMarioHeadBin();
    DynOS_Actor_Override_All();
}

PackData* DynOS_Pack_GetFromIndex(s32 aIndex) {
    auto& _DynosPacks = DynosPacks();
    if (aIndex < 0 || aIndex >= _DynosPacks.size()) {
        return NULL;
    }
    return &_DynosPacks[aIndex];
}

PackData* DynOS_Pack_GetFromPath(const SysPath& aPath) {
    for (auto& packData : DynosPacks()) {
        if (packData.mPath == aPath) {
            return &packData;
        }
    }
    return NULL;
}

PackData* DynOS_Pack_GetFromDisplayName(const char* aDisplayName) {
    for (auto& packData : DynosPacks()) {
        if (!strcmp(packData.mDisplayName.begin(), aDisplayName)) {
            return &packData;
        }
    }
    return NULL;
}

PackData* DynOS_Pack_Add(const SysPath& aPath) {
    PackData* existing = DynOS_Pack_GetFromPath(aPath);
    if (existing != NULL) { return existing; }

    // extract basename
    const char* displayName = aPath.c_str();
    const char* ctoken = displayName;
    while (*ctoken != '\0') {
        if (*ctoken == *PATH_SEPARATOR || *ctoken == *PATH_SEPARATOR_ALT) {
            if (*(ctoken + 1) != '\0') {
                displayName = (ctoken + 1);
            }
        }
        ctoken++;
    }

    existing = DynOS_Pack_GetFromDisplayName(displayName);
    if (existing != NULL) { return existing; }


    auto& _DynosPacks = DynosPacks();
    s32 index = _DynosPacks.size();
    const PackData packData = {
        .mIndex = index,
        .mEnabled = false,
        .mPath = aPath,
        .mDisplayName = "",
        .mGfxData = {},
        .mTextures = {},
        .mGoddardMarioHeadBin = "",
        .mGoddardCharacterHeadBins = {},
        .mLoaded = false,
    };
    _DynosPacks.push_back(packData);

    PackData* _Pack = &_DynosPacks.back();

    _Pack->mDisplayName = displayName;

    return _Pack;
}

std::pair<std::string, GfxData *>* DynOS_Pack_GetActor(PackData* aPackData, const char* aActorName) {
    if (aPackData == NULL || aActorName == NULL) {
        return NULL;
    }
    for (auto& pair : aPackData->mGfxData) {
        if (pair.first == aActorName) {
            return &pair;
        }
    }
    return NULL;
}

void DynOS_Pack_AddActor(PackData* aPackData, const char* aActorName, GfxData* aGfxData) {
    if (aPackData == NULL || aActorName == NULL || aGfxData == NULL) {
        return;
    }

    s32 index = aPackData->mGfxData.size();
    aPackData->mGfxData.emplace_back(aActorName, aGfxData);

    if (aPackData->mEnabled) {
        DynOS_Pack_ActivateActor(aPackData->mIndex, aPackData->mGfxData[index]);
    }
}

DataNode<TexData>* DynOS_Pack_GetTex(PackData* aPackData, const char* aTexName) {
    if (aPackData == NULL || aTexName == NULL) {
        return NULL;
    }

    for (auto& _Tex : aPackData->mTextures) {
        if (!strcmp(_Tex->mName.begin(), aTexName)) {
            return _Tex;
        }
    }
    return NULL;
}

void DynOS_Pack_AddTex(PackData* aPackData, DataNode<TexData>* aTexData) {
    if (aPackData == NULL || aTexData == NULL) {
        return;
    }

    aPackData->mTextures.push_back(aTexData);

    if (aPackData->mEnabled) {
        DynOS_Tex_Activate(aTexData, false);
    }
}
