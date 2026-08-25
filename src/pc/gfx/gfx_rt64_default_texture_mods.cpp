#if defined(_WIN32)

#include "gfx_rt64_default_config.hpp"

//
// Texture mods
//

const RT64DefaultMod gRT64DefaultTextureMods[] = {
    {
        .name = "amp_seg8_texture_08000F18",
        .materialMod = {
            .shadowAlphaMultiplier = 0.0f,
        },
    },
    {
        .name = "amp_seg8_texture_08002318",
        .materialMod = {
            .shadowAlphaMultiplier = 0.0f,
        },
    },
    {
        .name = "bitdw_seg7_texture_07000000",
        .materialMod = {
            .reflectionFactor = 0.05f,
            .reflectionFresnelFactor = 0.05f,
        },
    },
    {
        .name = "bitdw_seg7_texture_07000800",
        .materialMod = {
            .reflectionFactor = 0.075f,
            .reflectionFresnelFactor = 0.75f,
        },
    },
    {
        .name = "bitdw_seg7_texture_07001800",
        .materialMod = {
            .reflectionFactor = 0.2f,
            .refractionFactor = 0.9f,
        },
    },
    {
        .name = "bobomb_seg8_texture_0801DA60",
        .materialMod = {
            .ignoreNormalFactor = 1,
        },
    },
    {
        .name = "bobomb_seg8_texture_0801EA60",
        .materialMod = {
            .ignoreNormalFactor = 1,
        },
    },
    {
        .name = "bobomb_seg8_texture_0801FA60",
        .materialMod = {
            .ignoreNormalFactor = 1,
        },
    },
    {
        .name = "bobomb_seg8_texture_08020A60",
        .materialMod = {
            .ignoreNormalFactor = 1,
        },
    },
    {
        .name = "bomb_seg6_texture_06057AC0",
        .materialMod = {
            .ignoreNormalFactor = 1,
        },
    },
    {
        .name = "bomb_seg6_texture_06058AC0",
        .materialMod = {
            .ignoreNormalFactor = 1,
        },
    },
    {
        .name = "bowser_1_seg7_texture_07001800",
        .materialMod = {
            .reflectionFactor = 0.05f,
            .reflectionFresnelFactor = 0.15f,
        },
    },
    {
        .name = "bowser_3_seg7_texture_07000000",
        .materialMod = {
            .reflectionFactor = 0.2f,
            .reflectionFresnelFactor = 0.14f,
            .reflectionShineFactor = 0.66f,
        },
    },
    {
        .name = "bowser_seg6_texture_0601F438",
        .materialMod = {
            .specularColor = { 5, 5, 5 },
        },
    },
    {
        .name = "bowser_seg6_texture_0602AC38",
        .materialMod = {
            .reflectionFactor = 0.07f,
            .reflectionFresnelFactor = 0.0f,
        },
    },
    {
        .name = "bubba_seg5_texture_05001C08",
        .materialMod = {
            .shadowRayBias = 50,
        },
    },
    {
        .name = "bubba_seg5_texture_05002408",
        .materialMod = {
            .shadowRayBias = 50,
        },
    },
    {
        .name = "bubble_seg4_texture_0401CD60",
        .materialMod = {
            .ignoreNormalFactor = 1,
            .selfLightColor = { 255, 255, 255 },
            .lightGroupMaskBits = 0,
        },
    },
    {
        .name = "bubble_seg4_texture_0401D560",
        .materialMod = {
            .ignoreNormalFactor = 1,
        },
    },
    {
        .name = "bully_seg5_texture_050000E0",
        .materialMod = {
            .reflectionFactor = 0.04f,
        },
    },
    {
        .name = "bully_seg5_texture_05000468",
        .materialMod = {
            .ignoreNormalFactor = 1,
        },
    },
    {
        .name = "bully_seg5_texture_05001468",
        .materialMod = {
            .ignoreNormalFactor = 1,
        },
    },
    {
        .name = "capswitch_seg5_texture_05002C48",
        .materialMod = {
            .reflectionFactor = 0.075f,
            .reflectionFresnelFactor = 0.075f,
            .shadowRayBias = 60,
        },
    },
    {
        .name = "cave_09002800",
        .materialMod = {
            .reflectionFactor = 0.1f,
            .reflectionFresnelFactor = 0.1f,
        },
    },
    {
        .name = "cave_09007000",
        .materialMod = {
            .solidAlphaMultiplier = 0.0f,
        },
    },
    {
        .name = "cave_0900A000",
        .materialMod = {
            .selfLightColor = { 255, 204, 64 },
            .lightGroupMaskBits = 0,
        },
    },
    {
        .name = "cave_0900B800",
        .materialMod = {
            .solidAlphaMultiplier = 0.5f,
            .shadowAlphaMultiplier = 0.0f,
            .selfLightColor = { 76, 64, 26 },
            .lightGroupMaskBits = 0,
        },
    },
    {
        .name = "cave_0900C000",
        .materialMod = {
            .solidAlphaMultiplier = 0.5f,
            .selfLightColor = { 255, 128, 128 },
            .lightGroupMaskBits = 0,
            .specularFactor = 1.0f,
            .specularEccentricity = 0.3f,
        },
    },
    {
        .name = "ccm_seg7_texture_07011958",
        .materialMod = {
            .shadowRayBias = 50,
        },
    },
    {
        .name = "chain_chomp_seg6_texture_060213D0",
        .materialMod = {
            .reflectionFactor = 0.1f,
            .reflectionFresnelFactor = 0.0f,
            .reflectionShineFactor = 0.0f,
            .refractionFactor = 0.0f,
            .shadowRayBias = 10,
        },
    },
    {
        .name = "chain_chomp_seg6_texture_06021BD0",
        .materialMod = {
            .reflectionFactor = 0.1f,
            .reflectionFresnelFactor = 0.0f,
            .reflectionShineFactor = 0.0f,
            .refractionFactor = 0.0f,
            .shadowRayBias = 10,
        },
    },
    {
        .name = "chain_chomp_seg6_texture_060223D0",
        .materialMod = {
            .reflectionFactor = 0.05f,
            .reflectionFresnelFactor = 0.0f,
            .reflectionShineFactor = 0.0f,
            .refractionFactor = 0.0f,
            .shadowRayBias = 10,
        },
    },
    {
        .name = "chain_chomp_seg6_texture_06022BD0",
        .materialMod = {
            .reflectionFactor = 0.05f,
            .reflectionFresnelFactor = 0.0f,
            .reflectionShineFactor = 0.0f,
            .refractionFactor = 0.0f,
        },
    },
    {
        .name = "checkerboard_platform_seg8_texture_0800CC40",
        .materialMod = {
            .reflectionFactor = 0.05f,
            .reflectionFresnelFactor = 0.05f,
        },
    },
    {
        .name = "chuckya_seg8_texture_08007778",
        .materialMod = {
            .ignoreNormalFactor = 1,
        },
    },
    {
        .name = "chuckya_seg8_texture_08007F78",
        .materialMod = {
            .ignoreNormalFactor = 1,
        },
    },
    {
        .name = "chuckya_seg8_texture_08008F78",
        .materialMod = {
            .ignoreNormalFactor = 1,
        },
    },
    {
        .name = "cotmc_seg7_texture_07001800",
        .materialMod = {
            .reflectionFactor = 0.5f,
            .reflectionFresnelFactor = 0.5f,
            .reflectionShineFactor = 1.0f,
            .selfLightColor = { 191, 191, 191 },
            .lightGroupMaskBits = 0,
        },
    },
    {
        .name = "dirt_seg3_texture_0302BDF8",
        .materialMod = {
            .ignoreNormalFactor = 1,
        },
    },
    {
        .name = "door_seg3_texture_0300D510",
        .materialMod = {
            .reflectionFactor = 0.05f,
            .reflectionFresnelFactor = 0.1f,
        },
    },
    {
        .name = "door_seg3_texture_03013510",
        .materialMod = {
            .reflectionFactor = 0.1f,
            .reflectionFresnelFactor = 0.1f,
        },
    },
    {
        .name = "dorrie_seg6_texture_06009DA0",
        .materialMod = {
            .shadowRayBias = 100,
        },
    },
    {
        .name = "effect_0B002020",
        .materialMod = {
            .selfLightColor = { 125, 125, 125 },
        },
    },
    {
        .name = "effect_0B002820",
        .materialMod = {
            .selfLightColor = { 125, 125, 125 },
        },
    },
    {
        .name = "effect_0B003020",
        .materialMod = {
            .selfLightColor = { 125, 125, 125 },
        },
    },
    {
        .name = "effect_0B003820",
        .materialMod = {
            .selfLightColor = { 125, 125, 125 },
        },
    },
    {
        .name = "effect_0B004020",
        .materialMod = {
            .selfLightColor = { 125, 125, 125 },
        },
    },
    {
        .name = "effect_0B004820",
        .materialMod = {
            .selfLightColor = { 125, 125, 125 },
        },
    },
    {
        .name = "effect_0B005020",
        .materialMod = {
            .selfLightColor = { 125, 125, 125 },
        },
    },
    {
        .name = "effect_0B005820",
        .materialMod = {
            .selfLightColor = { 125, 125, 125 },
        },
    },
    {
        .name = "effect_0B00684C",
        .materialMod = {
            .ignoreNormalFactor = 1,
            .shadowAlphaMultiplier = 0.0f,
        },
    },
    {
        .name = "fire_09000000",
        .materialMod = {
            .reflectionFactor = 0.1f,
            .reflectionFresnelFactor = 0.1f,
        },
    },
    {
        .name = "fire_09000800",
        .materialMod = {
            .reflectionFactor = 0.1f,
            .reflectionFresnelFactor = 0.1f,
        },
    },
    {
        .name = "fire_0900A000",
        .materialMod = {
            .reflectionFactor = 0.075f,
            .reflectionFresnelFactor = 0.25f,
        },
    },
    {
        .name = "fire_0900B800",
        .materialMod = {
            .reflectionFactor = 0.1f,
            .reflectionFresnelFactor = 0.1f,
        },
    },
    {
        .name = "flyguy_seg8_texture_0800E088",
        .materialMod = {
            .shadowRayBias = 50,
        },
    },
    {
        .name = "flyguy_seg8_texture_0800F088",
        .materialMod = {
            .shadowRayBias = 50,
        },
    },
    {
        .name = "fwoosh_seg5_texture_05015808",
        .materialMod = {
            .depthBias = 50,
        },
    },
    {
        .name = "gd_texture_red_star_0",
        .materialMod = {
            .selfLightColor = { 178, 178, 178 },
            .lightGroupMaskBits = 0,
        },
        .lightMod = {
            .set = true,
            .position = { 0.0f, 0.0f, 0.0f },
            .diffuseColor = { 255, 51, 51 },
            .specularColor = { 166, 33, 33 },
            .attenuationRadius = 900.0f,
            .pointRadius = 50.0f,
            .shadowOffset = 50.0f,
            .attenuationExponent = 1.0f,
            .flickerIntensity = 0.0f,
            .groupBits = 16,
            .intensity = 1.25f,
        },
    },
    {
        .name = "gd_texture_red_star_1",
        .materialMod = {
            .selfLightColor = { 178, 178, 178 },
            .lightGroupMaskBits = 0,
        },
        .lightMod = {
            .set = true,
            .position = { 0.0f, 0.0f, 0.0f },
            .diffuseColor = { 255, 51, 51 },
            .specularColor = { 166, 33, 33 },
            .attenuationRadius = 900.0f,
            .pointRadius = 50.0f,
            .shadowOffset = 50.0f,
            .attenuationExponent = 1.0f,
            .flickerIntensity = 0.0f,
            .groupBits = 16,
            .intensity = 1.25f,
        },
    },
    {
        .name = "gd_texture_red_star_2",
        .materialMod = {
            .selfLightColor = { 178, 178, 178 },
            .lightGroupMaskBits = 0,
        },
        .lightMod = {
            .set = true,
            .position = { 0.0f, 0.0f, 0.0f },
            .diffuseColor = { 255, 51, 51 },
            .specularColor = { 166, 33, 33 },
            .attenuationRadius = 900.0f,
            .pointRadius = 50.0f,
            .shadowOffset = 50.0f,
            .attenuationExponent = 1.0f,
            .flickerIntensity = 0.0f,
            .groupBits = 16,
            .intensity = 1.25f,
        },
    },
    {
        .name = "gd_texture_red_star_3",
        .materialMod = {
            .selfLightColor = { 178, 178, 178 },
            .lightGroupMaskBits = 0,
        },
        .lightMod = {
            .set = true,
            .position = { 0.0f, 0.0f, 0.0f },
            .diffuseColor = { 255, 51, 51 },
            .specularColor = { 166, 33, 33 },
            .attenuationRadius = 900.0f,
            .pointRadius = 50.0f,
            .shadowOffset = 50.0f,
            .attenuationExponent = 1.0f,
            .flickerIntensity = 0.0f,
            .groupBits = 16,
            .intensity = 1.25f,
        },
    },
    {
        .name = "gd_texture_red_star_4",
        .materialMod = {
            .selfLightColor = { 178, 178, 178 },
            .lightGroupMaskBits = 0,
        },
        .lightMod = {
            .set = true,
            .position = { 0.0f, 0.0f, 0.0f },
            .diffuseColor = { 255, 51, 51 },
            .specularColor = { 166, 33, 33 },
            .attenuationRadius = 900.0f,
            .pointRadius = 50.0f,
            .shadowOffset = 50.0f,
            .attenuationExponent = 1.0f,
            .flickerIntensity = 0.0f,
            .groupBits = 16,
            .intensity = 1.25f,
        },
    },
    {
        .name = "gd_texture_red_star_5",
        .materialMod = {
            .selfLightColor = { 178, 178, 178 },
            .lightGroupMaskBits = 0,
        },
        .lightMod = {
            .set = true,
            .position = { 0.0f, 0.0f, 0.0f },
            .diffuseColor = { 255, 51, 51 },
            .specularColor = { 166, 33, 33 },
            .attenuationRadius = 900.0f,
            .pointRadius = 50.0f,
            .shadowOffset = 50.0f,
            .attenuationExponent = 1.0f,
            .flickerIntensity = 0.0f,
            .groupBits = 16,
            .intensity = 1.25f,
        },
    },
    {
        .name = "gd_texture_red_star_6",
        .materialMod = {
            .selfLightColor = { 178, 178, 178 },
            .lightGroupMaskBits = 0,
        },
        .lightMod = {
            .set = true,
            .position = { 0.0f, 0.0f, 0.0f },
            .diffuseColor = { 255, 51, 51 },
            .specularColor = { 166, 33, 33 },
            .attenuationRadius = 900.0f,
            .pointRadius = 50.0f,
            .shadowOffset = 50.0f,
            .attenuationExponent = 1.0f,
            .flickerIntensity = 0.0f,
            .groupBits = 16,
            .intensity = 1.25f,
        },
    },
    {
        .name = "gd_texture_red_star_7",
        .materialMod = {
            .selfLightColor = { 178, 178, 178 },
            .lightGroupMaskBits = 0,
        },
        .lightMod = {
            .set = true,
            .position = { 0.0f, 0.0f, 0.0f },
            .diffuseColor = { 255, 51, 51 },
            .specularColor = { 166, 33, 33 },
            .attenuationRadius = 900.0f,
            .pointRadius = 50.0f,
            .shadowOffset = 50.0f,
            .attenuationExponent = 1.0f,
            .flickerIntensity = 0.0f,
            .groupBits = 16,
            .intensity = 1.25f,
        },
    },
    {
        .name = "gd_texture_white_star_0",
        .materialMod = {
            .selfLightColor = { 178, 178, 178 },
            .lightGroupMaskBits = 0,
        },
        .lightMod = {
            .set = true,
            .position = { 0.0f, 0.0f, 0.0f },
            .diffuseColor = { 255, 255, 255 },
            .specularColor = { 166, 166, 166 },
            .attenuationRadius = 900.0f,
            .pointRadius = 50.0f,
            .shadowOffset = 50.0f,
            .attenuationExponent = 1.0f,
            .flickerIntensity = 0.0f,
            .groupBits = 16,
            .intensity = 1.25f,
        },
    },
    {
        .name = "gd_texture_white_star_1",
        .materialMod = {
            .selfLightColor = { 178, 178, 178 },
            .lightGroupMaskBits = 0,
        },
        .lightMod = {
            .set = true,
            .position = { 0.0f, 0.0f, 0.0f },
            .diffuseColor = { 255, 255, 255 },
            .specularColor = { 166, 166, 166 },
            .attenuationRadius = 900.0f,
            .pointRadius = 50.0f,
            .shadowOffset = 50.0f,
            .attenuationExponent = 1.0f,
            .flickerIntensity = 0.0f,
            .groupBits = 16,
            .intensity = 1.25f,
        },
    },
    {
        .name = "gd_texture_white_star_2",
        .materialMod = {
            .selfLightColor = { 178, 178, 178 },
            .lightGroupMaskBits = 0,
        },
        .lightMod = {
            .set = true,
            .position = { 0.0f, 0.0f, 0.0f },
            .diffuseColor = { 255, 255, 255 },
            .specularColor = { 166, 166, 166 },
            .attenuationRadius = 900.0f,
            .pointRadius = 50.0f,
            .shadowOffset = 50.0f,
            .attenuationExponent = 1.0f,
            .flickerIntensity = 0.0f,
            .groupBits = 16,
            .intensity = 1.25f,
        },
    },
    {
        .name = "gd_texture_white_star_3",
        .materialMod = {
            .selfLightColor = { 178, 178, 178 },
            .lightGroupMaskBits = 0,
        },
        .lightMod = {
            .set = true,
            .position = { 0.0f, 0.0f, 0.0f },
            .diffuseColor = { 255, 255, 255 },
            .specularColor = { 166, 166, 166 },
            .attenuationRadius = 900.0f,
            .pointRadius = 50.0f,
            .shadowOffset = 50.0f,
            .attenuationExponent = 1.0f,
            .flickerIntensity = 0.0f,
            .groupBits = 16,
            .intensity = 1.25f,
        },
    },
    {
        .name = "gd_texture_white_star_4",
        .materialMod = {
            .selfLightColor = { 178, 178, 178 },
            .lightGroupMaskBits = 0,
        },
        .lightMod = {
            .set = true,
            .position = { 0.0f, 0.0f, 0.0f },
            .diffuseColor = { 255, 255, 255 },
            .specularColor = { 166, 166, 166 },
            .attenuationRadius = 900.0f,
            .pointRadius = 50.0f,
            .shadowOffset = 50.0f,
            .attenuationExponent = 1.0f,
            .flickerIntensity = 0.0f,
            .groupBits = 16,
            .intensity = 1.25f,
        },
    },
    {
        .name = "gd_texture_white_star_5",
        .materialMod = {
            .selfLightColor = { 178, 178, 178 },
            .lightGroupMaskBits = 0,
        },
        .lightMod = {
            .set = true,
            .position = { 0.0f, 0.0f, 0.0f },
            .diffuseColor = { 255, 255, 255 },
            .specularColor = { 166, 166, 166 },
            .attenuationRadius = 900.0f,
            .pointRadius = 50.0f,
            .shadowOffset = 50.0f,
            .attenuationExponent = 1.0f,
            .flickerIntensity = 0.0f,
            .groupBits = 16,
            .intensity = 1.25f,
        },
    },
    {
        .name = "gd_texture_white_star_6",
        .materialMod = {
            .selfLightColor = { 178, 178, 178 },
            .lightGroupMaskBits = 0,
        },
        .lightMod = {
            .set = true,
            .position = { 0.0f, 0.0f, 0.0f },
            .diffuseColor = { 255, 255, 255 },
            .specularColor = { 166, 166, 166 },
            .attenuationRadius = 900.0f,
            .pointRadius = 50.0f,
            .shadowOffset = 50.0f,
            .attenuationExponent = 1.0f,
            .flickerIntensity = 0.0f,
            .groupBits = 16,
            .intensity = 1.25f,
        },
    },
    {
        .name = "gd_texture_white_star_7",
        .materialMod = {
            .selfLightColor = { 178, 178, 178 },
            .lightGroupMaskBits = 0,
        },
        .lightMod = {
            .set = true,
            .position = { 0.0f, 0.0f, 0.0f },
            .diffuseColor = { 255, 255, 255 },
            .specularColor = { 166, 166, 166 },
            .attenuationRadius = 900.0f,
            .pointRadius = 50.0f,
            .shadowOffset = 50.0f,
            .attenuationExponent = 1.0f,
            .flickerIntensity = 0.0f,
            .groupBits = 16,
            .intensity = 1.25f,
        },
    },
    {
        .name = "generic_0900B000",
        .materialMod = {
            .solidAlphaMultiplier = 0.0f,
        },
    },
    {
        .name = "goomba_seg8_texture_08019530",
        .materialMod = {
            .ignoreNormalFactor = 1,
        },
    },
    {
        .name = "goomba_seg8_texture_08019D30",
        .materialMod = {
            .shadowRayBias = 50,
        },
    },
    {
        .name = "goomba_seg8_texture_0801A530",
        .materialMod = {
            .shadowRayBias = 50,
        },
    },
    {
        .name = "grass_0900B000",
        .materialMod = {
            .solidAlphaMultiplier = 0.0f,
        },
    },
    {
        .name = "grass_0900B800",
        .materialMod = {
            .solidAlphaMultiplier = 0.5f,
            .selfLightColor = { 128, 128, 128 },
            .lightGroupMaskBits = 0,
        },
    },
    {
        .name = "heave_ho_seg5_texture_0500E9C8",
        .materialMod = {
            .shadowRayBias = 50,
        },
    },
    {
        .name = "heave_ho_seg5_texture_0500F1C8",
        .materialMod = {
            .shadowRayBias = 50,
        },
    },
    {
        .name = "heave_ho_seg5_texture_0500F9C8",
        .materialMod = {
            .shadowRayBias = 50,
        },
    },
    {
        .name = "heave_ho_seg5_texture_050109C8",
        .materialMod = {
            .shadowRayBias = 50,
        },
    },
    {
        .name = "hmc_seg7_texture_07003000",
        .materialMod = {
            .reflectionFactor = 0.15f,
            .reflectionFresnelFactor = 0.15f,
        },
    },
    {
        .name = "hmc_seg7_texture_07024CE0",
        .materialMod = {
            .reflectionFactor = 0.9f,
            .reflectionFresnelFactor = 0.9f,
            .reflectionShineFactor = 0.75f,
            .diffuseColorMix = { 0, 0, 0, 1.0f },
        },
    },
    {
        .name = "inside_09000000",
        .materialMod = {
            .shadowRayBias = 0,
        },
    },
    {
        .name = "inside_09004000",
        .materialMod = {
            .reflectionFactor = 0.04f,
            .reflectionFresnelFactor = 0.04f,
        },
    },
    {
        .name = "inside_castle_seg7_texture_07001000",
        .materialMod = {
            .lightGroupMaskBits = 0,
        },
    },
    {
        .name = "inside_castle_seg7_texture_07002000",
        .materialMod = {
            .reflectionFactor = 0.1f,
            .reflectionFresnelFactor = 0.0f,
        },
    },
    {
        .name = "inside_castle_seg7_texture_07003800",
        .materialMod = {
            .reflectionFactor = 0.05f,
            .reflectionFresnelFactor = 0.05f,
        },
    },
    {
        .name = "inside_castle_seg7_texture_07004800",
        .materialMod = {
            .reflectionFactor = 0.05f,
            .reflectionFresnelFactor = 0.05f,
        },
    },
    {
        .name = "inside_castle_seg7_texture_07016800",
        .materialMod = {
            .reflectionFactor = 0.9f,
            .reflectionFresnelFactor = 1.0f,
            .reflectionShineFactor = 0.75f,
            .diffuseColorMix = { 0, 0, 0, 1.0f },
        },
    },
    {
        .name = "intro_seg7_texture_07007EA0",
        .materialMod = {
            .shadowRayBias = 0,
        },
    },
    {
        .name = "intro_seg7_texture_070086A0",
        .materialMod = {
            .shadowRayBias = 0,
        },
    },
    {
        .name = "king_bobomb_seg5_texture_05002078",
        .materialMod = {
            .ignoreNormalFactor = 1,
        },
    },
    {
        .name = "king_bobomb_seg5_texture_05005878",
        .materialMod = {
            .ignoreNormalFactor = 1,
        },
    },
    {
        .name = "king_bobomb_seg5_texture_05006078",
        .materialMod = {
            .shadowRayBias = 5,
        },
    },
    {
        .name = "king_bobomb_seg5_texture_05008478",
        .materialMod = {
            .ignoreNormalFactor = 1,
        },
    },
    {
        .name = "king_bobomb_seg5_texture_05009478",
        .materialMod = {
            .ignoreNormalFactor = 1,
        },
    },
    {
        .name = "lakitu_enemy_seg5_texture_050114E0",
        .materialMod = {
            .shadowRayBias = 50,
        },
    },
    {
        .name = "lakitu_enemy_seg5_texture_05011CE0",
        .materialMod = {
            .shadowRayBias = 50,
        },
    },
    {
        .name = "lll_seg7_texture_07006000",
        .materialMod = {
            .reflectionFactor = 0.1f,
            .reflectionFresnelFactor = 0.1f,
        },
    },
    {
        .name = "lll_seg7_texture_07006800",
        .materialMod = {
            .reflectionFactor = 0.1f,
            .reflectionFresnelFactor = 0.1f,
        },
    },
    {
        .name = "lll_seg7_texture_07007000",
        .materialMod = {
            .reflectionFactor = 0.1f,
            .reflectionFresnelFactor = 0.1f,
        },
    },
    {
        .name = "lll_seg7_texture_07007800",
        .materialMod = {
            .reflectionFactor = 0.1f,
            .reflectionFresnelFactor = 0.1f,
        },
    },
    {
        .name = "lll_seg7_texture_07008000",
        .materialMod = {
            .reflectionFactor = 0.1f,
            .reflectionFresnelFactor = 0.1f,
        },
    },
    {
        .name = "lll_seg7_texture_07008800",
        .materialMod = {
            .reflectionFactor = 0.1f,
            .reflectionFresnelFactor = 0.1f,
        },
    },
    {
        .name = "lll_seg7_texture_07009000",
        .materialMod = {
            .reflectionFactor = 0.1f,
            .reflectionFresnelFactor = 0.1f,
        },
    },
    {
        .name = "lll_seg7_texture_07009800",
        .materialMod = {
            .reflectionFactor = 0.1f,
            .reflectionFresnelFactor = 0.1f,
        },
    },
    {
        .name = "lll_seg7_texture_0700A000",
        .materialMod = {
            .reflectionFactor = 0.1f,
            .reflectionFresnelFactor = 0.1f,
        },
    },
    {
        .name = "lll_seg7_texture_0700A800",
        .materialMod = {
            .reflectionFactor = 0.1f,
            .reflectionFresnelFactor = 0.1f,
        },
    },
    {
        .name = "lll_seg7_texture_0700B000",
        .materialMod = {
            .reflectionFactor = 0.1f,
            .reflectionFresnelFactor = 0.1f,
        },
    },
    {
        .name = "lll_seg7_texture_0700B800",
        .materialMod = {
            .reflectionFactor = 0.1f,
            .reflectionFresnelFactor = 0.1f,
        },
    },
    {
        .name = "lll_seg7_texture_0700C000",
        .materialMod = {
            .reflectionFactor = 0.1f,
            .reflectionFresnelFactor = 0.1f,
        },
    },
    {
        .name = "lll_seg7_texture_0700C800",
        .materialMod = {
            .reflectionFactor = 0.1f,
            .reflectionFresnelFactor = 0.1f,
        },
    },
    {
        .name = "machine_09003800",
        .materialMod = {
            .reflectionFactor = 0.025f,
            .reflectionFresnelFactor = 0.025f,
        },
    },
    {
        .name = "machine_09005000",
        .materialMod = {
            .reflectionFactor = 0.025f,
            .reflectionFresnelFactor = 0.025f,
        },
    },
    {
        .name = "machine_09006800",
        .materialMod = {
            .reflectionFactor = 0.025f,
            .reflectionFresnelFactor = 0.025f,
        },
    },
    {
        .name = "machine_09008000",
        .materialMod = {
            .reflectionFactor = 0.025f,
            .reflectionFresnelFactor = 0.025f,
        },
    },
    {
        .name = "mad_piano_seg5_texture_050072F0",
        .materialMod = {
            .shadowRayBias = 0,
        },
    },
    {
        .name = "manta_seg5_texture_050037A0",
        .materialMod = {
            .shadowRayBias = 30,
        },
    },
    {
        .name = "mist_seg3_texture_03000080",
        .materialMod = {
            .selfLightColor = { 191, 191, 191 },
            .lightGroupMaskBits = 0,
        },
    },
    {
        .name = "mountain_09004800",
        .materialMod = {
            .shadowRayBias = 100,
        },
    },
    {
        .name = "mountain_09007800",
        .materialMod = {
            .selfLightColor = { 255, 204, 26 },
            .lightGroupMaskBits = 0,
        },
    },
    {
        .name = "mr_i_eyeball_seg6_texture_06000080",
        .materialMod = {
            .ignoreNormalFactor = 1,
        },
    },
    {
        .name = "mr_i_eyeball_seg6_texture_06001080",
        .materialMod = {
            .ignoreNormalFactor = 1,
        },
    },
    {
        .name = "mr_i_iris_seg6_texture_06002170",
        .materialMod = {
            .ignoreNormalFactor = 1,
        },
    },
    {
        .name = "outside_09001000",
        .materialMod = {
            .shadowRayBias = 0,
        },
    },
    {
        .name = "outside_09002000",
        .materialMod = {
            .shadowRayBias = 0,
        },
    },
    {
        .name = "outside_09004800",
        .materialMod = {
            .shadowRayBias = 0,
        },
    },
    {
        .name = "outside_0900BC00",
        .materialMod = {
            .solidAlphaMultiplier = 0.0f,
        },
    },
    {
        .name = "piranha_plant_seg6_texture_060123F8",
        .materialMod = {
            .shadowRayBias = 75,
        },
    },
    {
        .name = "piranha_plant_seg6_texture_060133F8",
        .materialMod = {
            .shadowRayBias = 75,
        },
    },
    {
        .name = "pss_seg7_texture_07000800",
        .materialMod = {
            .lightGroupMaskBits = 0,
        },
    },
    {
        .name = "scuttlebug_seg6_texture_06010108",
        .materialMod = {
            .ignoreNormalFactor = 1,
        },
    },
    {
        .name = "scuttlebug_seg6_texture_06010908",
        .materialMod = {
            .ignoreNormalFactor = 1,
        },
    },
    {
        .name = "scuttlebug_seg6_texture_06011908",
        .materialMod = {
            .ignoreNormalFactor = 1,
        },
    },
    {
        .name = "skeeter_seg6_texture_06000090",
        .materialMod = {
            .ignoreNormalFactor = 1,
        },
    },
    {
        .name = "sky_09002000",
        .materialMod = {
            .reflectionFactor = 0.05f,
            .reflectionFresnelFactor = 0.5f,
        },
    },
    {
        .name = "sky_09005800",
        .materialMod = {
            .shadowAlphaMultiplier = 0.0f,
            .selfLightColor = { 255, 255, 255 },
            .lightGroupMaskBits = 0,
        },
    },
    {
        .name = "sky_09008000",
        .materialMod = {
            .reflectionFactor = 0.05f,
            .reflectionFresnelFactor = 0.01f,
        },
    },
    {
        .name = "smoke_seg4_texture_0401DEA0",
        .materialMod = {
            .selfLightColor = { 191, 191, 191 },
            .lightGroupMaskBits = 0,
        },
    },
    {
        .name = "smoke_seg4_texture_0401E6A0",
        .materialMod = {
            .selfLightColor = { 191, 191, 191 },
            .lightGroupMaskBits = 0,
        },
    },
    {
        .name = "smoke_seg4_texture_0401EEA0",
        .materialMod = {
            .selfLightColor = { 191, 191, 191 },
            .lightGroupMaskBits = 0,
        },
    },
    {
        .name = "smoke_seg4_texture_0401F6A0",
        .materialMod = {
            .selfLightColor = { 191, 191, 191 },
            .lightGroupMaskBits = 0,
        },
    },
    {
        .name = "smoke_seg4_texture_0401FEA0",
        .materialMod = {
            .selfLightColor = { 191, 191, 191 },
            .lightGroupMaskBits = 0,
        },
    },
    {
        .name = "smoke_seg4_texture_040206A0",
        .materialMod = {
            .selfLightColor = { 191, 191, 191 },
            .lightGroupMaskBits = 0,
        },
    },
    {
        .name = "smoke_seg4_texture_04020EA0",
        .materialMod = {
            .selfLightColor = { 191, 191, 191 },
            .lightGroupMaskBits = 0,
        },
    },
    {
        .name = "snow_09000800",
        .materialMod = {
            .reflectionFactor = 0.15f,
            .reflectionFresnelFactor = 0.25f,
            .refractionFactor = 0.975f,
            .selfLightColor = { 64, 64, 64 },
            .lightGroupMaskBits = 0,
        },
    },
    {
        .name = "snow_09002000",
        .materialMod = {
            .specularColor = { 0, 0, 0 },
        },
    },
    {
        .name = "snow_09006000",
        .materialMod = {
            .specularColor = { 0, 0, 0 },
        },
    },
    {
        .name = "snow_09008800",
        .materialMod = {
            .specularColor = { 0, 0, 0 },
        },
    },
    {
        .name = "snow_09009000",
        .materialMod = {
            .solidAlphaMultiplier = 0.4f,
            .lightGroupMaskBits = 0,
        },
    },
    {
        .name = "snow_09009800",
        .materialMod = {
            .solidAlphaMultiplier = 0.0f,
            .depthBias = 1,
            .lightGroupMaskBits = 0,
        },
    },
    {
        .name = "snowman_seg5_texture_05008C70",
        .materialMod = {
            .shadowRayBias = 0,
        },
    },
    {
        .name = "snowman_seg5_texture_05009470",
        .materialMod = {
            .ignoreNormalFactor = 1,
        },
    },
    {
        .name = "snowman_seg5_texture_0500A470",
        .materialMod = {
            .ignoreNormalFactor = 1,
        },
    },
    {
        .name = "snufit_seg6_texture_060080E0",
        .materialMod = {
            .reflectionFactor = 0.1f,
            .reflectionFresnelFactor = 1.0f,
        },
    },
    {
        .name = "snufit_seg6_texture_060084E0",
        .materialMod = {
            .reflectionFactor = 0.1f,
            .reflectionFresnelFactor = 1.0f,
        },
    },
    {
        .name = "spindrift_seg5_texture_050016D0",
        .materialMod = {
            .shadowRayBias = 11,
        },
    },
    {
        .name = "spindrift_seg5_texture_05001ED0",
        .materialMod = {
            .ignoreNormalFactor = 1,
        },
    },
    {
        .name = "spooky_09006000",
        .materialMod = {
            .selfLightColor = { 255, 255, 64 },
            .lightGroupMaskBits = 0,
        },
    },
    {
        .name = "spooky_09006800",
        .materialMod = {
            .selfLightColor = { 255, 204, 51 },
            .lightGroupMaskBits = 0,
        },
    },
    {
        .name = "spooky_0900A800",
        .materialMod = {
            .lightGroupMaskBits = 0,
        },
    },
    {
        .name = "spooky_0900B000",
        .materialMod = {
            .solidAlphaMultiplier = 0.0f,
            .lightGroupMaskBits = 1,
        },
    },
    {
        .name = "spooky_0900B800",
        .materialMod = {
            .selfLightColor = { 128, 128, 128 },
            .lightGroupMaskBits = 0,
        },
    },
    {
        .name = "ssl_seg7_texture_07000800",
        .materialMod = {
            .solidAlphaMultiplier = 0.333f,
            .shadowAlphaMultiplier = 0.0f,
            .lightGroupMaskBits = 0,
        },
    },
    {
        .name = "ssl_seg7_texture_0700BFA8",
        .materialMod = {
            .reflectionFactor = 0.1f,
            .reflectionFresnelFactor = 0.1f,
        },
    },
    {
        .name = "ssl_seg7_texture_0700C7A8",
        .materialMod = {
            .reflectionFactor = 0.1f,
            .reflectionFresnelFactor = 0.1f,
        },
    },
    {
        .name = "ssl_seg7_texture_0700D7A8",
        .materialMod = {
            .reflectionFactor = 0.1f,
            .reflectionFresnelFactor = 0.1f,
        },
    },
    {
        .name = "ssl_seg7_texture_0700E7A8",
        .materialMod = {
            .reflectionFactor = 0.1f,
            .reflectionFresnelFactor = 0.1f,
        },
    },
    {
        .name = "star_seg3_texture_0302A6F0",
        .materialMod = {
            .selfLightColor = { 178, 178, 178 },
            .lightGroupMaskBits = 0,
        },
        .lightMod = {
            .set = true,
            .position = { 0.0f, 0.0f, 0.0f },
            .diffuseColor = { 255, 255, 39 },
            .specularColor = { 166, 166, 26 },
            .attenuationRadius = 4500.0f,
            .pointRadius = 200.0f,
            .shadowOffset = 600.0f,
            .attenuationExponent = 1.0f,
            .flickerIntensity = 0.0f,
            .groupBits = 16,
            .intensity = 1.3f,
        },
    },
    {
        .name = "sushi_seg5_texture_05008ED0",
        .materialMod = {
            .shadowRayBias = 16,
        },
    },
    {
        .name = "texture_91a596dc5b3fd70d",
        .materialMod = {
            .specularTintSet = true,
            .specularTint = 0,
        },
    },
    {
        .name = "texture_castle_light",
        .materialMod = {
            .selfLightColor = { 128, 128, 128 },
            .lightGroupMaskBits = 0,
        },
    },
    {
        .name = "texture_menu_file",
        .materialMod = {
            .specularColor = { 6, 6, 6 },
        },
    },
    {
        .name = "texture_menu_mario_save",
        .materialMod = {
            .specularColor = { 6, 6, 6 },
        },
    },
    {
        .name = "texture_quarter_flying_carpet",
        .materialMod = {
            .ignoreNormalFactor = 1,
        },
    },
    {
        .name = "texture_shadow_quarter_circle",
        .materialMod = {
            .refractionFactor = 0.0f,
            .solidAlphaMultiplier = 0.0f,
            .shadowAlphaMultiplier = 0.0f,
            .depthBias = 2,
            .lightGroupMaskBits = 0,
        },
    },
    {
        .name = "texture_shadow_quarter_square",
        .materialMod = {
            .refractionFactor = 0.0f,
            .solidAlphaMultiplier = 0.0f,
            .shadowAlphaMultiplier = 0.0f,
            .depthBias = 5,
            .lightGroupMaskBits = 0,
        },
    },
    {
        .name = "texture_waterbox_jrb_water",
        .materialMod = {
            .uvDetailScale = 0.4f,
            .reflectionFactor = 0.375f,
            .reflectionFresnelFactor = 0.75f,
            .refractionFactor = 0.95f,
            .solidAlphaMultiplier = 0.75f,
            .diffuseColorMix = { 0, 0, 0, 0.7f },
        },
        .normalMap = "texture_water_nrm",
    },
    {
        .name = "texture_waterbox_lava",
        .materialMod = {
            .selfLightColor = { 51, 51, 51 },
        },
    },
    {
        .name = "texture_waterbox_mist",
        .materialMod = {
            .selfLightColor = { 128, 128, 128 },
            .lightGroupMaskBits = 0,
        },
    },
    {
        .name = "texture_waterbox_water",
        .materialMod = {
            .uvDetailScale = 0.2f,
            .reflectionFactor = 0.375f,
            .reflectionFresnelFactor = 0.75f,
            .refractionFactor = 0.95f,
            .solidAlphaMultiplier = 0.75f,
            .diffuseColorMix = { 0, 0, 0, 0.7f },
        },
        .normalMap = "texture_water_nrm",
    },
    {
        .name = "thwomp_seg5_texture_05009900",
        .materialMod = {
            .shadowRayBias = 100,
        },
    },
    {
        .name = "thwomp_seg5_texture_0500A900",
        .materialMod = {
            .shadowRayBias = 100,
        },
    },
    {
        .name = "totwc_seg7_texture_07002000",
        .materialMod = {
            .ignoreNormalFactor = 1,
            .selfLightColor = { 255, 255, 255 },
            .lightGroupMaskBits = 0,
        },
    },
    {
        .name = "tree_seg3_texture_0302DE28",
        .materialMod = {
            .shadowRayBias = 3,
        },
    },
    {
        .name = "tree_seg3_texture_0302EE28",
        .materialMod = {
            .shadowRayBias = 3,
        },
    },
    {
        .name = "tree_seg3_texture_03031048",
        .materialMod = {
            .ignoreNormalFactor = 1,
            .specularColor = { 0, 0, 0 },
        },
    },
    {
        .name = "tree_seg3_texture_03032218",
        .materialMod = {
            .ignoreNormalFactor = 0,
        },
    },
    {
        .name = "ukiki_seg5_texture_05007BC0",
        .materialMod = {
            .shadowRayBias = 50,
        },
    },
    {
        .name = "unagi_seg5_texture_0500B720",
        .materialMod = {
            .selfLightColor = { 26, 76, 102 },
        },
    },
    {
        .name = "warp_pipe_seg3_texture_03007E40",
        .materialMod = {
            .specularColor = { 26, 26, 26 },
            .specularShinyness = 1.0f,
        },
    },
    {
        .name = "warp_pipe_seg3_texture_03009168",
        .materialMod = {
            .specularColor = { 26, 26, 26 },
            .specularShinyness = 1.0f,
        },
    },
    {
        .name = "water_bubble_seg5_texture_0500FE80",
        .materialMod = {
            .reflectionFactor = 0.6f,
            .reflectionFresnelFactor = 1.0f,
            .reflectionShineFactor = 0.5f,
            .refractionFactor = 0.9f,
            .diffuseColorMix = { 64, 64, 64, 1.0f },
        },
    },
    {
        .name = "white_particle_texture",
        .materialMod = {
            .ignoreNormalFactor = 1,
        },
    },
    {
        .name = "whomp_seg6_texture_0601E360",
        .materialMod = {
            .ignoreNormalFactor = 0,
        },
    },
    {
        .name = "wiggler_seg5_texture_05005A30",
        .materialMod = {
            .ignoreNormalFactor = 1,
        },
    },
    {
        .name = "wiggler_seg5_texture_05006A30",
        .materialMod = {
            .ignoreNormalFactor = 1,
        },
    },
    {
        .name = "wiggler_seg5_texture_05009230",
        .materialMod = {
            .ignoreNormalFactor = 1,
        },
    },
    {
        .name = "wiggler_seg5_texture_0500A230",
        .materialMod = {
            .ignoreNormalFactor = 1,
        },
    },
    {
        .name = "wmotr_seg7_texture_07000C00",
        .materialMod = {
            .specularColor = { 72, 72, 72 },
            .specularEccentricity = 0.2f,
            .specularTintSet = true,
            .specularTint = 0,
        },
    },
    {
        .name = "wmotr_seg7_texture_07001400",
        .materialMod = {
            .shadowAlphaMultiplier = 0.0f,
        },
    },
    {
        .name = "yellow_sphere_seg6_texture_0601EB88",
        .materialMod = {
            .ignoreNormalFactor = 1,
        },
    },
};

const s32 gRT64DefaultTextureModCount = sizeof(gRT64DefaultTextureMods) / sizeof(gRT64DefaultTextureMods[0]);

#endif
