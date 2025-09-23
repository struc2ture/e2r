#include "common/types.h"
#include "common/util.h"

#include <font_loader.h>

#include "vertex.h"

list_define_type(E2R_UIVertList, VertexUI);
list_define_type(E2R_3DVertList, Vertex3D);
list_define_type(E2R_IndexList, VertIndex);

typedef struct E2R_UIRenderData
{
    const E2R_UIVertList *vert_list;
    const E2R_IndexList *index_list;

} E2R_UIRenderData;

typedef struct E2R_3DRenderData
{
    const E2R_3DVertList *vert_list;
    const E2R_IndexList *index_list;

} E2R_3DRenderData;

typedef struct E2R_3DDrawCall
{
    m4 model;

} E2R_3DDrawCall;

list_define_type(E2R_3DDrawCallList, E2R_3DDrawCall);

void e2r_draw_quad(v2 pos, v2 size, v4 color);
void e2r_draw_circle(v2 pos, v2 size, v4 color);
void e2r_draw_char(char ch, f32 *pen_x, f32 * pen_y, const FontAtlas *font_atlas, v4 color);
void e2r_draw_string(const char *str, f32 *pen_x, f32 *pen_y, const FontAtlas *font_atlas, v4 color);
void e2r_draw_line(const char *str, f32 *pen_x, f32 *pen_y, const FontAtlas *font_atlas, v4 color);
E2R_UIRenderData e2r_get_ui_render_data();
void e2r_reset_ui_data();

// ============================================

void e2r_draw_cube(m4 model);
E2R_3DRenderData e2r_get_cubes_render_data();
const E2R_3DDrawCallList *e2r_get_cubes_draw_calls();
void e2r_reset_cubes_data();
