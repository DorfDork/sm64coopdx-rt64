#ifndef GD_SHAPE_HELPER_H
#define GD_SHAPE_HELPER_H

#include <PR/ultratypes.h>
#include <stdbool.h>

#include "gd_types.h"

// data
extern struct ObjGroup *gMarioFaceGrp;
extern struct ObjShape *gSpotShape;
extern struct ObjShape *gShapeRedSpark;
extern struct ObjShape *gShapeSilverSpark;
extern struct ObjShape *gShapeRedStar;
extern struct ObjShape *gShapeSilverStar;

// functions
void calc_face_normal(struct ObjFace *face);
struct ObjVertex *gd_make_vertex(f32 x, f32 y, f32 z);
void add_3_vtx_to_face(struct ObjFace *face, struct ObjVertex *vtx1, struct ObjVertex *vtx2, struct ObjVertex *vtx3);
struct ObjShape *make_shape(s32 flag, const char *name);
void scale_verts_in_shape(struct ObjShape *shape, f32 x, f32 y, f32 z);
struct ObjNet *make_netfromshape(struct ObjShape *shape);
void animate_mario_head_gameover(struct ObjAnimator *self);
void animate_mario_head_normal(struct ObjAnimator *self);
s32 load_mario_head(void (*aniFn)(struct ObjAnimator *));
void load_shapes2(void);

// GDB2 skin weight accessors for variable poly count support
bool gd_goddard_has_skin_weights(void);
u32 gd_goddard_get_skin_joint_count(void);
bool gd_goddard_get_skin_joint_data(u32 index, u32 *out_joint_id, u32 *out_weight_count);
bool gd_goddard_get_skin_weight(u32 joint_index, u32 weight_index, u16 *out_vtx_idx, f32 *out_weight);
void gd_apply_gdb2_skin_weights(void);

// see bad_declarations.h
#ifndef GD_USE_BAD_DECLARATIONS
struct ObjFace* make_face_with_colour(f32 r, f32 g, f32 b);
#endif

#endif // GD_SHAPE_HELPER_H
