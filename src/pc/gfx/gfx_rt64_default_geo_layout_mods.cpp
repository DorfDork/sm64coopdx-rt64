#if defined(_WIN32)

#include "gfx_rt64_default_config.hpp"

//
// Geo layout mods
//

const RT64DefaultMod gRT64DefaultGeoLayoutMods[] = {
    {
        .name = "amp_geo",
        .materialMod = {
            .selfLightColor = { 178, 178, 178 },
            .lightGroupMaskBits = 0,
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
        .lightMod = {
            .set = true,
            .position = { 0.0f, 0.0f, 0.0f },
            .diffuseColor = { 255, 255, 39 },
            .specularColor = { 166, 166, 26 },
            .attenuationRadius = 1500.0f,
            .pointRadius = 25.0f,
            .shadowOffset = 80.0f,
            .attenuationExponent = 1.0f,
            .flickerIntensity = 0.125f,
            .groupBits = 16,
            .intensity = 1.3f,
        },
    },
    {
        .name = "birds_geo",
        .materialMod = {
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "black_bobomb_geo",
        .materialMod = {
            .shadowEnabledSet = true,
            .shadowEnabled = 0,
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "blargg_geo",
        .materialMod = {
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "blue_coin_geo",
        .materialMod = {
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "blue_coin_no_shadow_geo",
        .materialMod = {
            .shadowAlphaMultiplier = 0.0f,
        },
    },
    {
        .name = "blue_coin_switch_geo",
    },
    {
        .name = "blue_flame_geo",
        .materialMod = {
            .ignoreNormalFactor = 1,
            .shadowAlphaMultiplier = 0.0f,
            .selfLightColor = { 178, 178, 178 },
            .lightGroupMaskBits = 0,
        },
        .lightMod = {
            .set = true,
            .position = { 0.0f, 0.0f, 0.0f },
            .diffuseColor = { 0, 51, 204 },
            .specularColor = { 0, 33, 133 },
            .attenuationRadius = 300.0f,
            .pointRadius = 25.0f,
            .shadowOffset = 25.0f,
            .attenuationExponent = 2.0f,
            .flickerIntensity = 0.075f,
            .groupBits = 64,
            .intensity = 1.0f,
        },
    },
    {
        .name = "bobomb_buddy_geo",
        .materialMod = {
            .shadowEnabledSet = true,
            .shadowEnabled = 0,
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "boo_castle_geo",
        .materialMod = {
            .refractionFactor = 0.975f,
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "boo_geo",
        .materialMod = {
            .refractionFactor = 0.975f,
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "bookend_geo",
        .materialMod = {
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "bookend_part_geo",
        .materialMod = {
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "bowling_ball_geo",
    },
    {
        .name = "bowling_ball_track_geo",
        .materialMod = {
            .ignoreNormalFactor = 1,
        },
    },
    {
        .name = "bowser2_geo",
        .materialMod = {
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "bowser_1_yellow_sphere_geo",
        .materialMod = {
            .ignoreNormalFactor = 1,
            .shadowEnabledSet = true,
            .shadowEnabled = 0,
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "bowser_bomb_geo",
        .materialMod = {
            .ignoreNormalFactor = 1,
            .shadowEnabledSet = true,
            .shadowEnabled = 0,
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "bowser_flames_geo",
        .materialMod = {
            .ignoreNormalFactor = 1,
            .shadowAlphaMultiplier = 0.0f,
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "bowser_geo",
        .materialMod = {
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "bowser_impact_smoke_geo",
        .materialMod = {
            .ignoreNormalFactor = 1,
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "bowser_key_cutscene_geo",
        .materialMod = {
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "bowser_key_geo",
        .materialMod = {
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "breakable_box_geo",
    },
    {
        .name = "breakable_box_small_geo",
    },
    {
        .name = "bub_geo",
        .materialMod = {
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "bubba_geo",
        .materialMod = {
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "bubble_geo",
        .materialMod = {
            .ignoreNormalFactor = 1,
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "bubbly_tree_geo",
        .materialMod = {
            .shadowEnabledSet = true,
            .shadowEnabled = 0,
        },
    },
    {
        .name = "bullet_bill_geo",
        .materialMod = {
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "bully_boss_geo",
        .materialMod = {
            .shadowEnabledSet = true,
            .shadowEnabled = 0,
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "bully_geo",
        .materialMod = {
            .shadowEnabledSet = true,
            .shadowEnabled = 0,
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "burn_smoke_geo",
        .materialMod = {
            .ignoreNormalFactor = 1,
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "butterfly_geo",
        .materialMod = {
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "cabin_door_geo",
    },
    {
        .name = "cannon_barrel_geo",
        .materialMod = {
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "cannon_base_geo",
        .materialMod = {
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "cap_switch_geo",
        .materialMod = {
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "cartoon_star_geo",
        .materialMod = {
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "castle_door_0_star_geo",
    },
    {
        .name = "castle_door_1_star_geo",
    },
    {
        .name = "castle_door_3_stars_geo",
    },
    {
        .name = "castle_door_geo",
    },
    {
        .name = "chain_chomp_geo",
        .materialMod = {
            .ignoreNormalFactor = 1,
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "checkerboard_platform_geo",
        .materialMod = {
            .shadowCenterSet = true,
            .shadowCenter = 0,
        },
    },
    {
        .name = "chilly_chief_big_geo",
        .materialMod = {
            .shadowEnabledSet = true,
            .shadowEnabled = 0,
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "chilly_chief_geo",
        .materialMod = {
            .shadowEnabledSet = true,
            .shadowEnabled = 0,
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "chuckya_geo",
        .materialMod = {
            .shadowEnabledSet = true,
            .shadowEnabled = 0,
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "clam_shell_geo",
        .materialMod = {
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "cyan_fish_geo",
        .materialMod = {
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "dirt_animation_geo",
        .materialMod = {
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "dorrie_geo",
        .materialMod = {
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "enemy_lakitu_geo",
        .materialMod = {
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "error_model_geo",
        .materialMod = {
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "exclamation_box_geo",
        .materialMod = {
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "exclamation_box_outline_geo",
        .materialMod = {
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "explosion_geo",
        .materialMod = {
            .ignoreNormalFactor = 1,
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "eyerok_left_hand_geo",
        .materialMod = {
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "eyerok_right_hand_geo",
        .materialMod = {
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "fish_geo",
        .materialMod = {
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "fish_shadow_geo",
        .materialMod = {
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "flyguy_geo",
        .materialMod = {
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "fwoosh_geo",
        .materialMod = {
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "goomba_geo",
        .materialMod = {
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "haunted_cage_geo",
        .materialMod = {
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "haunted_chair_geo",
        .materialMod = {
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "haunted_door_geo",
    },
    {
        .name = "hazy_maze_door_geo",
    },
    {
        .name = "heart_geo",
        .materialMod = {
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "heave_ho_geo",
        .materialMod = {
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "hoot_geo",
        .materialMod = {
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "idle_water_wave_geo",
        .materialMod = {
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "intro_geo_0002D0",
        .materialMod = {
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "intro_geo_000414",
        .materialMod = {
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "intro_geo_mario_head_dizzy",
        .materialMod = {
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "intro_geo_mario_head_regular",
        .materialMod = {
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "invisible_bowser_accessory_geo",
        .materialMod = {
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "key_door_geo",
    },
    {
        .name = "king_bobomb_geo",
        .materialMod = {
            .shadowEnabledSet = true,
            .shadowEnabled = 0,
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "klepto_geo",
        .materialMod = {
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "koopa_flag_geo",
        .materialMod = {
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "koopa_shell_geo",
        .materialMod = {
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "koopa_with_shell_geo",
        .materialMod = {
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "koopa_without_shell_geo",
        .materialMod = {
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "lakitu_geo",
        .materialMod = {
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "leaves_geo",
        .materialMod = {
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "luigi_geo",
        .materialMod = {
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "luigis_cap_geo",
        .materialMod = {
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "luigis_metal_cap_geo",
        .materialMod = {
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "luigis_wing_cap_geo",
        .materialMod = {
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "luigis_winged_metal_cap_geo",
        .materialMod = {
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "mad_piano_geo",
        .materialMod = {
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "manta_seg5_geo_05008D14",
        .materialMod = {
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "mario_geo",
        .materialMod = {
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "mario_TODO_geo_0000E0",
        .materialMod = {
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "marios_cap_geo",
        .materialMod = {
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "marios_metal_cap_geo",
        .materialMod = {
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "marios_wing_cap_geo",
        .materialMod = {
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "marios_winged_metal_cap_geo",
        .materialMod = {
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "metal_box_geo",
        .materialMod = {
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "metal_door_geo",
    },
    {
        .name = "metallic_ball_geo",
        .materialMod = {
            .ignoreNormalFactor = 1,
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "mips_geo",
        .materialMod = {
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "mist_geo",
        .materialMod = {
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "moneybag_geo",
        .materialMod = {
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "monty_mole_geo",
        .materialMod = {
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "mr_blizzard_geo",
        .materialMod = {
            .shadowEnabledSet = true,
            .shadowEnabled = 0,
        },
    },
    {
        .name = "mr_blizzard_hidden_geo",
        .materialMod = {
            .shadowEnabledSet = true,
            .shadowEnabled = 0,
        },
    },
    {
        .name = "mr_i_geo",
        .materialMod = {
            .shadowEnabledSet = true,
            .shadowEnabled = 0,
        },
    },
    {
        .name = "mr_i_iris_geo",
        .materialMod = {
            .shadowEnabledSet = true,
            .shadowEnabled = 0,
        },
    },
    {
        .name = "mushroom_1up_geo",
        .materialMod = {
            .selfLightColor = { 178, 178, 178 },
            .lightGroupMaskBits = 0,
            .shadowEnabledSet = true,
            .shadowEnabled = 0,
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
        .lightMod = {
            .set = true,
            .position = { 0.0f, 0.0f, 0.0f },
            .diffuseColor = { 51, 204, 0 },
            .specularColor = { 33, 133, 0 },
            .attenuationRadius = 500.0f,
            .pointRadius = 25.0f,
            .shadowOffset = 25.0f,
            .attenuationExponent = 2.0f,
            .flickerIntensity = 0.0f,
            .groupBits = 16,
            .intensity = 1.0f,
        },
    },
    {
        .name = "number_geo",
        .materialMod = {
            .ignoreNormalFactor = 1,
            .shadowAlphaMultiplier = 0.0f,
            .shadowEnabledSet = true,
            .shadowEnabled = 0,
        },
    },
    {
        .name = "palm_tree_geo",
        .materialMod = {
            .shadowEnabledSet = true,
            .shadowEnabled = 0,
        },
    },
    {
        .name = "peach_geo",
        .materialMod = {
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "penguin_geo",
        .materialMod = {
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "piranha_plant_geo",
        .materialMod = {
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "pokey_body_part_geo",
        .materialMod = {
            .shadowEnabledSet = true,
            .shadowEnabled = 0,
        },
    },
    {
        .name = "pokey_head_geo",
        .materialMod = {
            .shadowEnabledSet = true,
            .shadowEnabled = 0,
        },
    },
    {
        .name = "purple_marble_geo",
        .materialMod = {
            .shadowEnabledSet = true,
            .shadowEnabled = 0,
        },
    },
    {
        .name = "purple_switch_geo",
        .materialMod = {
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "red_coin_geo",
        .materialMod = {
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "red_coin_no_shadow_geo",
        .materialMod = {
            .shadowAlphaMultiplier = 0.0f,
        },
    },
    {
        .name = "red_flame_geo",
        .materialMod = {
            .ignoreNormalFactor = 1,
            .shadowAlphaMultiplier = 0.0f,
            .selfLightColor = { 178, 178, 178 },
            .lightGroupMaskBits = 0,
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
        .lightMod = {
            .set = true,
            .position = { 0.0f, 0.0f, 0.0f },
            .diffuseColor = { 204, 51, 0 },
            .specularColor = { 133, 33, 0 },
            .attenuationRadius = 300.0f,
            .pointRadius = 25.0f,
            .shadowOffset = 25.0f,
            .attenuationExponent = 2.0f,
            .flickerIntensity = 0.075f,
            .groupBits = 64,
            .intensity = 1.0f,
        },
    },
    {
        .name = "red_flame_shadow_geo",
        .materialMod = {
            .ignoreNormalFactor = 1,
            .selfLightColor = { 178, 178, 178 },
            .lightGroupMaskBits = 0,
        },
        .lightMod = {
            .set = true,
            .position = { 0.0f, 0.0f, 0.0f },
            .diffuseColor = { 204, 51, 0 },
            .specularColor = { 133, 33, 0 },
            .attenuationRadius = 300.0f,
            .pointRadius = 25.0f,
            .shadowOffset = 25.0f,
            .attenuationExponent = 2.0f,
            .flickerIntensity = 0.075f,
            .groupBits = 64,
            .intensity = 1.0f,
        },
    },
    {
        .name = "scuttlebug_geo",
        .materialMod = {
            .shadowEnabledSet = true,
            .shadowEnabled = 0,
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "seaweed_geo",
        .materialMod = {
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "skeeter_geo",
        .materialMod = {
            .shadowEnabledSet = true,
            .shadowEnabled = 0,
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "small_key_geo",
        .materialMod = {
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "small_water_splash_geo",
        .materialMod = {
            .ignoreNormalFactor = 1,
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "smoke_geo",
        .materialMod = {
            .ignoreNormalFactor = 1,
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "snow_tree_geo",
        .materialMod = {
            .shadowEnabledSet = true,
            .shadowEnabled = 0,
        },
    },
    {
        .name = "snufit_geo",
        .materialMod = {
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "sparkles_animation_geo",
        .materialMod = {
            .ignoreNormalFactor = 1,
            .selfLightColor = { 178, 178, 178 },
            .lightGroupMaskBits = 0,
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
        .lightMod = {
            .set = true,
            .position = { 0.0f, 0.0f, 0.0f },
            .diffuseColor = { 204, 204, 0 },
            .specularColor = { 133, 133, 0 },
            .attenuationRadius = 100.0f,
            .pointRadius = 25.0f,
            .shadowOffset = 25.0f,
            .attenuationExponent = 2.0f,
            .flickerIntensity = 0.0f,
            .groupBits = 128,
            .intensity = 1.0f,
        },
    },
    {
        .name = "sparkles_geo",
        .materialMod = {
            .ignoreNormalFactor = 1,
            .selfLightColor = { 178, 178, 178 },
            .lightGroupMaskBits = 0,
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
        .lightMod = {
            .set = true,
            .position = { 0.0f, 0.0f, 0.0f },
            .diffuseColor = { 204, 204, 0 },
            .specularColor = { 133, 133, 0 },
            .attenuationRadius = 100.0f,
            .pointRadius = 25.0f,
            .shadowOffset = 25.0f,
            .attenuationExponent = 2.0f,
            .flickerIntensity = 0.0f,
            .groupBits = 128,
            .intensity = 1.0f,
        },
    },
    {
        .name = "spiky_tree1_geo",
        .materialMod = {
            .shadowEnabledSet = true,
            .shadowEnabled = 0,
        },
    },
    {
        .name = "spiky_tree_geo",
        .materialMod = {
            .shadowEnabledSet = true,
            .shadowEnabled = 0,
        },
    },
    {
        .name = "spindrift_geo",
        .materialMod = {
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "spiny_ball_geo",
        .materialMod = {
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "spiny_geo",
        .materialMod = {
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "springboard_bottom_geo",
        .materialMod = {
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "springboard_spring_geo",
        .materialMod = {
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "springboard_top_geo",
        .materialMod = {
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "star_geo",
        .materialMod = {
            .selfLightColor = { 178, 178, 178 },
            .lightGroupMaskBits = 0,
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
        .lightMod = {
            .set = true,
            .position = { 0.0f, 0.0f, 0.0f },
            .diffuseColor = { 230, 230, 51 },
            .specularColor = { 115, 115, 26 },
            .attenuationRadius = 1125.0f,
            .pointRadius = 50.0f,
            .shadowOffset = 150.0f,
            .attenuationExponent = 1.0f,
            .flickerIntensity = 0.0f,
            .groupBits = 16,
            .intensity = 1.0f,
        },
    },
    {
        .name = "sushi_geo",
        .materialMod = {
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "swoop_geo",
        .materialMod = {
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "thwomp_geo",
        .materialMod = {
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "toad_geo",
        .materialMod = {
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "toad_player_geo",
        .materialMod = {
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "toads_cap_geo",
        .materialMod = {
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "toads_metal_cap_geo",
        .materialMod = {
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "toads_wing_cap_geo",
        .materialMod = {
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "toads_winged_cap_geo",
        .materialMod = {
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "transparent_star_geo",
        .materialMod = {
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "treasure_chest_base_geo",
        .materialMod = {
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "treasure_chest_lid_geo",
        .materialMod = {
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "tweester_geo",
        .materialMod = {
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "ukiki_geo",
        .materialMod = {
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "unagi_geo",
        .materialMod = {
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "waluigi_geo",
        .materialMod = {
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "waluigis_cap_geo",
        .materialMod = {
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "waluigis_metal_cap_geo",
        .materialMod = {
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "waluigis_wing_cap_geo",
        .materialMod = {
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "waluigis_winged_metal_cap_geo",
        .materialMod = {
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "wario_geo",
        .materialMod = {
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "warios_cap_geo",
        .materialMod = {
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "warios_metal_cap_geo",
        .materialMod = {
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "warios_wing_cap_geo",
        .materialMod = {
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "warios_winged_metal_cap_geo",
        .materialMod = {
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "warp_pipe_geo",
    },
    {
        .name = "water_bomb_geo",
    },
    {
        .name = "water_bomb_shadow_geo",
    },
    {
        .name = "water_mine_geo",
    },
    {
        .name = "water_ring_geo",
        .materialMod = {
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "water_splash_geo",
        .materialMod = {
            .ignoreNormalFactor = 1,
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "wave_trail_geo",
        .materialMod = {
            .ignoreNormalFactor = 1,
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "white_particle_geo",
        .materialMod = {
            .ignoreNormalFactor = 1,
            .shadowEnabledSet = true,
            .shadowEnabled = 0,
        },
    },
    {
        .name = "white_puff_geo",
        .materialMod = {
            .ignoreNormalFactor = 1,
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "whomp_geo",
        .materialMod = {
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "wiggler_body_geo",
        .materialMod = {
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "wiggler_head_geo",
        .materialMod = {
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "wooden_door2_geo",
    },
    {
        .name = "wooden_door_geo",
    },
    {
        .name = "wooden_post_geo",
    },
    {
        .name = "wooden_signpost_geo",
    },
    {
        .name = "yellow_coin_geo",
        .materialMod = {
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "yellow_coin_no_shadow_geo",
        .materialMod = {
            .shadowAlphaMultiplier = 0.0f,
        },
    },
    {
        .name = "yellow_sphere_geo",
        .materialMod = {
            .ignoreNormalFactor = 1,
            .shadowEnabledSet = true,
            .shadowEnabled = 0,
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "yoshi_egg_geo",
        .materialMod = {
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
    {
        .name = "yoshi_geo",
        .materialMod = {
            .shadowCenterSet = true,
            .shadowCenter = 1,
        },
    },
};

const s32 gRT64DefaultGeoLayoutModCount = sizeof(gRT64DefaultGeoLayoutMods) / sizeof(gRT64DefaultGeoLayoutMods[0]);

#endif
