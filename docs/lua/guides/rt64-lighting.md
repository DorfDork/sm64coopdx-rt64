## [:rewind: Lua Reference](../lua.md)

# RT64 Lighting

RT64 is the ray tracing backend. These functions only affect RT64. Check `gfx_rt64_is_active()` before using them.

## Section 1: Variables

These are the fields on [`Rt64Light`](../structs.md#Rt64Light) and [`Rt64SceneDesc`](../structs.md#Rt64SceneDesc) whose units aren't obvious from the type alone.

| Field | Type | Notes |
| ----- | ---- | ----- |
| `diffuseColor`|[`Vec3f`](../structs.md#Vec3f)| A light's diffuse color. Uses **0-255 per channel**, not 0-1.
| `specularColor`|[`Vec3f`](../structs.md#Vec3f)| A light's specular color. Same **0-255 per channel** range as `diffuseColor`.
| `intensity`|`number`| Multiplies a light's color, letting it go brighter than the 0-255 range on its own would allow. Defaults to `1`.
| `pitch`|`number`| A light's up/down angle. In **degrees**.
| `yaw`|`number`| A light's left/right angle. In **degrees**.
| `roll`|`number`| A light's roll angle. In **degrees**.
| `aperturePitch`|`number`| The up/down angle of a light's aperture cone, only used when `apertureEnabled` is set. In **degrees**.
| `apertureYaw`|`number`| The left/right angle of a light's aperture cone, only used when `apertureEnabled` is set. In **degrees**.
| `ambientBaseColor`|[`Vec3f`](../structs.md#Vec3f)| The scene's ambient light color. Same **0-255 per channel** range as `diffuseColor`.
| `ambientNoGIColor`|[`Vec3f`](../structs.md#Vec3f)| The ambient color used on surfaces global illumination doesn't reach. Same **0-255 per channel** range as `diffuseColor`.
| `eyeLightDiffuseColor`|[`Vec3f`](../structs.md#Vec3f)| The diffuse color of the camera-attached eye light. Same **0-255 per channel** range as `diffuseColor`.
| `eyeLightSpecularColor`|[`Vec3f`](../structs.md#Vec3f)| The specular color of the camera-attached eye light. Same **0-255 per channel** range as `diffuseColor`.
| `skyYawOffset`|`number`| Rotates the sky around the level's vertical axis. In **degrees**.

## Section 2: Registering versus Editing Live

Use `gfx_rt64_set_level_lights`, `gfx_rt64_set_texture_mod`, and `gfx_rt64_set_geo_layout_mod` for settings that are set when the mod loads.

Use `gfx_rt64_get_area_lighting(levelNum, areaIndex)` for lighting that changes while the game is running. It returns the live area lighting data.

```lua
hook_event(HOOK_UPDATE, function()
    if not gfx_rt64_is_active() then return end

    local np = gNetworkPlayers[0]
    local lighting = gfx_rt64_get_area_lighting(np.currLevelNum, np.currAreaIndex)
    if lighting == nil then return end

    local sun = lighting.lights[0]
    sun.diffuseColor.x = 220
    sun.yaw = (get_global_timer() % 3600) / 10.0
end)
```

Both methods use the same lighting data, so registered lighting can be used as a baseline and then modified live.

## Section 3: Reflection Color

`reflectionFactor` controls how much a surface reflects. `reflectionColor` controls the reflection color.

```lua
gfx_rt64_set_texture_mod("texture_waterbox_jrb_water", {
    materialMod = {
        reflectionFactor = 1.0,
        reflectionColor = { 255, 215, 0 },
    },
})
```

A `reflectionFactor` of `1` makes the surface fully reflective.

## Section 4: Cast Shadows Staying Put

`shadowCenter` makes a surface's shadow stay directly underneath it instead of following the light direction.

Set it on the object that casts the shadow:

```lua
gfx_rt64_set_geo_layout_mod("mario_geo", {
    materialMod = {
        shadowCenter = true,
    },
})
```
The object itself continues using normal light-direction shadows. `shadowAlphaMultiplier` controls shadow darkness.

## Section 5: Receiving Shadows

`shadowEnabled` controls whether a surface can receive shadows. It defaults to `true`.

```lua
gfx_rt64_set_texture_mod("texture_grass", {
    materialMod = {
        shadowEnabled = false,
    },
})
```

With `shadowEnabled = false`, the surface cannot receive shadows.

## Section 6: See Also

- [`gfx_rt64_set_level_lights`](../functions.md#gfx_rt64_set_level_lights), [`gfx_rt64_set_texture_mod`](../functions.md#gfx_rt64_set_texture_mod), [`gfx_rt64_set_geo_layout_mod`](../functions.md#gfx_rt64_set_geo_layout_mod) - one-time registration
- [`gfx_rt64_get_area_lighting`](../functions.md#gfx_rt64_get_area_lighting), [`gfx_rt64_is_active`](../functions.md#gfx_rt64_is_active) - live editing
- [`Rt64Light`](../structs.md#Rt64Light), [`Rt64SceneDesc`](../structs.md#Rt64SceneDesc), [`Rt64AreaLighting`](../structs.md#Rt64AreaLighting) - the cobjects `gfx_rt64_get_area_lighting` hands back
