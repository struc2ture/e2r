#include "common/types.h"
#include "common/util.h"

#include "vertex.h"

list_define_type(E2R_VertList, VertexUI);
list_define_type(E2R_IndexList, VertIndex);

typedef struct E2R_RenderData
{
    E2R_VertList *vert_list;
    E2R_IndexList *index_list;

} E2R_RenderData;

void e2r_reset_draw_data();
void e2r_draw_quad(v2 pos, v2 size, v4 color);
E2R_RenderData e2r_get_render_data();
