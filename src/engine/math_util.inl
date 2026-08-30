/*
.inl files are for inlined functions and function templates.

It's best to put them in this file so they can be recompiled if needed.

Credit to PeachyPeach, Isaac0, Blockyyy, and others for suggestions
optimizations and bug reports.
*/

#ifndef MATH_UTIL_INL
#define MATH_UTIL_INL

/* |description|
Returns `replacement` if `replacement` is not zero. Otherwise, returns `value`
|descriptionEnd| */
INLINE OPTIMIZE_O3 f32 replace_value_if_not_zero(f32 value, f32 replacement) {
    if (replacement != 0) {
        return replacement;
    }
    return value;
}

/* |description|
Converts an angle from SM64 format to radians
|descriptionEnd| */
INLINE OPTIMIZE_O3 f32 sm64_to_radians(s16 sm64Angle) {
    return sm64Angle * M_PI / 0x8000;
}

/* |description|
Converts an angle from radians to SM64 format
|descriptionEnd| */
INLINE OPTIMIZE_O3 s16 radians_to_sm64(f32 radiansAngle) {
    return radiansAngle * 0x8000 / M_PI;
}

/* |description|
Converts an angle from SM64 format to degrees
|descriptionEnd| */
INLINE OPTIMIZE_O3 f32 sm64_to_degrees(s16 sm64Angle) {
    return sm64Angle * 180.0f / 0x8000;
}

/* |description|
Converts an angle from degrees to SM64 format
|descriptionEnd| */
INLINE OPTIMIZE_O3 s16 degrees_to_sm64(f32 degreesAngle) {
    return degreesAngle * 0x8000 / 180.0f;
}

/* |description|
Converts an angle from degrees to radians
|descriptionEnd| */
INLINE OPTIMIZE_O3 f32 degrees_to_radians(f32 degreesAngle) {
    return degreesAngle * M_PI / 180.0f;
}

/* |description|
Converts an angle from radians to degrees
|descriptionEnd| */
INLINE OPTIMIZE_O3 f32 radians_to_degrees(f32 radiansAngle) {
    return radiansAngle * 180.0f / M_PI;
}

/* |description|
Sets the components of the 4D floating-point vector `v` to 0
|descriptionEnd| */
INLINE OPTIMIZE_O3 Vec4fp vec4f_zero(VEC_OUT Vec4f v) {
    memset(v, 0, sizeof(Vec4f));
    return v;
}

/* |description|
Copies the contents of a 4D floating-point vector (`src`) into another 4D floating-point vector (`dest`)
|descriptionEnd| */
INLINE OPTIMIZE_O3 Vec4fp vec4f_copy(VEC_OUT Vec4f dest, Vec4f src) {
    memcpy(dest, src, sizeof(Vec4f));
    return dest;
}

/* |description|
Sets the values of the 4D floating-point vector `dest` to the given x, y, z, and w values
|descriptionEnd| */
INLINE OPTIMIZE_O3 Vec4fp vec4f_set(VEC_OUT Vec4f dest, f32 x, f32 y, f32 z, f32 w) {
    dest[0] = x;
    dest[1] = y;
    dest[2] = z;
    dest[3] = w;
    return dest;
}

/* |description|
Sets the components of the 4D integer vector `v` to 0
|descriptionEnd| */
INLINE OPTIMIZE_O3 Vec4ip vec4i_zero(VEC_OUT Vec4i v) {
    memset(v, 0, sizeof(Vec4i));
    return v;
}

/* |description|
Copies the contents of a 4D integer vector (`src`) into another 4D integer vector (`dest`)
|descriptionEnd| */
INLINE OPTIMIZE_O3 Vec4ip vec4i_copy(VEC_OUT Vec4i dest, Vec4i src) {
    memcpy(dest, src, sizeof(Vec4i));
    return dest;
}

/* |description|
Sets the values of the 4D integer vector `dest` to the given x, y, z, and w values
|descriptionEnd| */
INLINE OPTIMIZE_O3 Vec4ip vec4i_set(VEC_OUT Vec4i dest, s32 x, s32 y, s32 z, s32 w) {
    dest[0] = x;
    dest[1] = y;
    dest[2] = z;
    dest[3] = w;
    return dest;
}

#endif // MATH_UTIL_INL