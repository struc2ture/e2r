#include "common/types.h"
#include "common/util.h"

#include <font_loader.h>

#include "vertex.h"

list_define_type(E2R_VertList, VertexUI);
list_define_type(E2R_IndexList, VertIndex);

typedef struct E2R_RenderData
{
    const E2R_VertList *vert_list;
    const E2R_IndexList *index_list;

} E2R_RenderData;

void e2r_reset_ui_data();
void e2r_reset_text_data();
void e2r_draw_quad(v2 pos, v2 size, v4 color);
void e2r_draw_circle(v2 pos, v2 size, v4 color);
void e2r_draw_char(char ch, f32 *pen_x, f32 * pen_y, const FontAtlas *font_atlas, v4 color);
void e2r_draw_string(const char *str, f32 *pen_x, f32 *pen_y, const FontAtlas *font_atlas, v4 color);
E2R_RenderData e2r_get_ui_render_data();
E2R_RenderData e2r_get_text_render_data();
