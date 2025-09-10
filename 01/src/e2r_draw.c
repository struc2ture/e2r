#include "e2r_draw.h"

#include <stdlib.h>

#include "common/lin_math.h"
#include "common/types.h"
#include "common/util.h"
#include "vertex.h"

typedef struct _Quad
{
    v2 pos;
    v2 size;
    v4 color;

} _Quad;

list_define_type(_QuadList, _Quad);

typedef struct _DrawData
{
    _QuadList quad_list;

    E2R_VertList vert_list;
    E2R_IndexList index_list;

} _DrawData;

globvar _DrawData draw_data;

// ------------------------------------------------

static void _ui_get_atlas_q_verts(v2i cell_p, v2 out_verts[4])
{
    const f32 atlas_dim = 1024.0f;
    const f32 atlas_cell_dim = 64.0f;
    const f32 atlas_cell_pad = 4.0f;
    f32 min_x = cell_p.x * atlas_cell_dim;
    f32 max_x = min_x + atlas_cell_dim;
    min_x += atlas_cell_pad;
    max_x -= atlas_cell_pad;

    f32 max_y = atlas_dim - cell_p.y * atlas_cell_dim;
    f32 min_y = max_y - atlas_cell_dim;
    max_y -= atlas_cell_pad;
    min_y += atlas_cell_pad;

    // TODO: figure this out. I think this actually makes it look worse
    // texel offset, to sample the middle of texels
    // min_x += 0.5f;
    // max_x += 0.5f;
    // min_y += 0.5f;
    // max_y += 0.5f;

    // Normalized range
    min_x /= atlas_dim;
    max_x /= atlas_dim;
    min_y /= atlas_dim;
    max_y /= atlas_dim;

    out_verts[0].x = min_x; out_verts[0].y = min_y;
    out_verts[1].x = max_x; out_verts[1].y = min_y;
    out_verts[2].x = max_x; out_verts[2].y = max_y;
    out_verts[3].x = min_x; out_verts[3].y = max_y;

    // for (int i = 0; i < 4; i++)
    //     trace("atlas_vert[%d]: %f, %f", i, out_verts[i].x, out_verts[i].y);
}

// ------------------------------------------------

void e2r_reset_draw_data()
{
    list_clear(&draw_data.quad_list);
    list_clear(&draw_data.vert_list);
    list_clear(&draw_data.index_list);
}

void e2r_draw_quad(v2 pos, v2 size, v4 color)
{
    _QuadList *list = &draw_data.quad_list;

    _Quad q =
    {
        .pos = pos,
        .size = size,
        .color = color
    };

    list_append(list, q);
}

E2R_RenderData e2r_get_render_data()
{
    _QuadList *quad_list = &draw_data.quad_list;
    E2R_VertList *vert_list = &draw_data.vert_list;
    E2R_IndexList *index_list = &draw_data.index_list;

    v2 atlas_q_verts[4] = {};
    _ui_get_atlas_q_verts(V2I(0, 0), atlas_q_verts);

    const _Quad *quad;
    list_iterate(quad_list, quad_i ,quad)
    {
        v2 min = quad->pos;
        v2 max = v2_add(quad->pos, quad->size);

        v3 pos[] =
        {
            V3(min.x, min.y, 0.0f),
            V3(max.x, min.y, 0.0f),
            V3(max.x, max.y, 0.0f),
            V3(min.x, max.y, 0.0f)
        };

        u32 base_index = vert_list->size;

        for (int i = 0; i < 4; i++)
        {
            VertexUI v =
            {
                .pos = pos[i],
                .uv = atlas_q_verts[i],
                .color = quad->color
            };
            list_append(vert_list, v);
        }

        u32 indices[] = { 0, 1, 2, 0, 2, 3};
        for (int i = 0; i < 6; i++)
        {
            list_append(index_list, indices[i] + base_index);
        }
    }

    return (E2R_RenderData){
        .vert_list = vert_list,
        .index_list = index_list
    };
}
