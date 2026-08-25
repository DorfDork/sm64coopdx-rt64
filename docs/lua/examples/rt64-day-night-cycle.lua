-- name: RT64 Day/Night Cycle
-- description: A minimal day/night cycle for the RT64 ray tracing backend - swings the sun across the sky and shifts ambient/eye light color between day and night over a repeating cycle, in outdoor levels only. Has no effect unless RT64 is the running backend.

local CYCLE_SECONDS = 120

local COLOR_NIGHT = { r = 40, g = 50, b = 90 }
local COLOR_SUNSET = { r = 255, g = 140, b = 80 }

local AMBIENT_NIGHT = { r = 3, g = 4, b = 12 }
local AMBIENT_DAY = { r = 60, g = 60, b = 70 }

local AMBIENT_NO_GI_NIGHT = { r = 2, g = 3, b = 6 }
local AMBIENT_NO_GI_DAY = { r = 26, g = 38, b = 51 }

local EYE_LIGHT_NIGHT = { r = 26, g = 26, b = 26 }
local EYE_LIGHT_DAY = { r = 26, g = 26, b = 26 }

-- Absolute lightness for the skybox's HSL shift at night
local SKY_LIGHTNESS_NIGHT = -0.500
local SUN_INTENSITY_NIGHT = 0.02

local OUTDOOR_LEVELS = {
    [LEVEL_BOB] = true,
    [LEVEL_WF] = true,
    [LEVEL_CCM] = true,
    [LEVEL_JRB] = true,
    [LEVEL_DDD] = true,
    [LEVEL_SSL] = true,
    [LEVEL_TTM] = true,
    [LEVEL_RR] = true,
    [LEVEL_LLL] = true,
    [LEVEL_CASTLE_GROUNDS] = true,
    [LEVEL_CASTLE_COURTYARD] = true,
    [LEVEL_WDW] = true,
    [LEVEL_WMOTR] = true,
    [LEVEL_TOTWC] = true,
    [LEVEL_THI] = true,
    [LEVEL_SL] = true,
}

local function lerp(a, b, t)
    return a + (b - a) * t
end

local function color_lerp(a, b, t)
    return {
        r = lerp(a.r, b.r, t),
        g = lerp(a.g, b.g, t),
        b = lerp(a.b, b.b, t),
    }
end

local function hsl_lerp(a, b, t)
    return {
        h = lerp(a.h, b.h, t),
        s = lerp(a.s, b.s, t),
        l = lerp(a.l, b.l, t),
    }
end

local function night_day_lerp(nightColor, dayColor, dayAmount)
    if dayAmount > 0.5 then
        return color_lerp(COLOR_SUNSET, dayColor, (dayAmount - 0.5) * 2)
    end
    return color_lerp(nightColor, COLOR_SUNSET, dayAmount * 2)
end

-- 0 at midnight, 1 at noon
local function get_day_amount()
    local ticksPerCycle = CYCLE_SECONDS * 30
    local phase = (get_global_timer() % ticksPerCycle) / ticksPerCycle
    return (math.sin((phase - 0.25) * math.pi * 2) + 1) / 2
end

local areaDefaultsCache = {}

local function get_area_defaults(lighting, levelNum, areaIndex)
    local key = levelNum .. "_" .. areaIndex
    local cached = areaDefaultsCache[key]
    if cached then return cached end

    local sun = lighting.lights[0]
    local sky = lighting.scene.skyHSLModifier
    cached = {
        diffuse = { r = sun.diffuseColor.x, g = sun.diffuseColor.y, b = sun.diffuseColor.z },
        specular = { r = sun.specularColor.x, g = sun.specularColor.y, b = sun.specularColor.z },
        intensity = sun.intensity,
        sky = { h = sky.x, s = sky.y, l = sky.z },
    }
    areaDefaultsCache[key] = cached
    return cached
end

local function update_rt64_lighting()
    -- rt64_* functions will be a no-op on rasterizer backends
    if not gfx_rt64_is_active() then return end

    local np = gNetworkPlayers[0]
    if not OUTDOOR_LEVELS[np.currLevelNum] then return end
    if np.currAreaIndex ~= 1 then return end

    local lighting = gfx_rt64_get_area_lighting(np.currLevelNum, np.currAreaIndex)
    if lighting == nil then return end

    local defaults = get_area_defaults(lighting, np.currLevelNum, np.currAreaIndex)

    local dayAmount = get_day_amount()
    local sunDiffuse = night_day_lerp(COLOR_NIGHT, defaults.diffuse, dayAmount)
    local sunSpecular = night_day_lerp(COLOR_NIGHT, defaults.specular, dayAmount)
    local ambient = color_lerp(AMBIENT_NIGHT, AMBIENT_DAY, dayAmount)
    local ambientNoGI = color_lerp(AMBIENT_NO_GI_NIGHT, AMBIENT_NO_GI_DAY, dayAmount)
    local eyeLight = color_lerp(EYE_LIGHT_NIGHT, EYE_LIGHT_DAY, dayAmount)
    local sunIntensity = lerp(SUN_INTENSITY_NIGHT, defaults.intensity, dayAmount)
    local skyHSLNight = { h = defaults.sky.h, s = defaults.sky.s, l = SKY_LIGHTNESS_NIGHT }
    local skyHSL = hsl_lerp(skyHSLNight, defaults.sky, dayAmount)

    lighting.scene.ambientBaseColor.x = ambient.r
    lighting.scene.ambientBaseColor.y = ambient.g
    lighting.scene.ambientBaseColor.z = ambient.b

    lighting.scene.ambientNoGIColor.x = ambientNoGI.r
    lighting.scene.ambientNoGIColor.y = ambientNoGI.g
    lighting.scene.ambientNoGIColor.z = ambientNoGI.b

    lighting.scene.eyeLightDiffuseColor.x = eyeLight.r
    lighting.scene.eyeLightDiffuseColor.y = eyeLight.g
    lighting.scene.eyeLightDiffuseColor.z = eyeLight.b
    lighting.scene.eyeLightSpecularColor.x = eyeLight.r
    lighting.scene.eyeLightSpecularColor.y = eyeLight.g
    lighting.scene.eyeLightSpecularColor.z = eyeLight.b

    lighting.scene.skyHSLModifier.x = skyHSL.h
    lighting.scene.skyHSLModifier.y = skyHSL.s
    lighting.scene.skyHSLModifier.z = skyHSL.l

    -- The sun should be the first light source in any level
    local sun = lighting.lights[0]
    sun.diffuseColor.x = sunDiffuse.r
    sun.diffuseColor.y = sunDiffuse.g
    sun.diffuseColor.z = sunDiffuse.b
    sun.specularColor.x = sunSpecular.r
    sun.specularColor.y = sunSpecular.g
    sun.specularColor.z = sunSpecular.b
    sun.intensity = sunIntensity

    -- Swings from just below the horizon at midnight to overhead at noon
    sun.pitch = lerp(-10, 80, dayAmount)
end

-- hooks --
hook_event(HOOK_UPDATE, update_rt64_lighting)
