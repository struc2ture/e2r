#include "e2r_draw.h"

#include <stdlib.h>
#include <string.h>

#include <font_loader.h>

#include "common/lin_math.h"
#include "common/types.h"
#include "common/util.h"
#include "vertex.h"

typedef struct _UIQuad
{
    v2 pos;
    v2 size;
    v4 color;
    v2i atlas_coord;

} _UIQuad;

typedef struct _TextQuad
{
    v2 pos_min;
    v2 pos_max;
    v2 uv_min;
    v2 uv_max;
    v4 color;

} _TextQuad;


list_define_type(_UIQuadList, _UIQuad);
list_define_type(_TextQuadList, _TextQuad);

typedef struct _DrawData
{
    _UIQuadList ui_quad_list;
    _TextQuadList text_quad_list;

    E2R_VertList ui_vert_list;
    E2R_IndexList ui_index_list;

    E2R_VertList text_vert_list;
    E2R_IndexList text_index_list;

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

void e2r_reset_ui_data()
{
    list_clear(&draw_data.ui_quad_list);
    list_clear(&draw_data.ui_vert_list);
    list_clear(&draw_data.ui_index_list);
}

void e2r_reset_text_data()
{
    list_clear(&draw_data.text_quad_list);
    list_clear(&draw_data.text_vert_list);
    list_clear(&draw_data.text_index_list);
}

void e2r_draw_quad(v2 pos, v2 size, v4 color)
{
    _UIQuadList *list = &draw_data.ui_quad_list;

    _UIQuad q =
    {
        .pos = pos,
        .size = size,
        .color = color,
        .atlas_coord = V2I(0, 0)
    };

    list_append(list, q);
}

void e2r_draw_circle(v2 pos, v2 size, v4 color)
{
    _UIQuadList *list = &draw_data.ui_quad_list;

    _UIQuad q =
    {
        .pos = pos,
        .size = size,
        .color = color,
        .atlas_coord = V2I(2, 0)
    };

    list_append(list, q);
}

void e2r_draw_char(char ch, f32 *pen_x, f32 * pen_y, const FontAtlas *font_atlas, v4 color)
{
    f32 x = *pen_x;
    f32 y = *pen_y + font_loader_get_ascender(font_atlas);

    GlyphQuad q = font_loader_get_glyph_quad(font_atlas, ch, x, y);

    _TextQuad text_quad =
    {
        .pos_min = V2(q.screen_min_x, q.screen_max_y),
        .pos_max = V2(q.screen_max_x, q.screen_min_y),
        .uv_min = V2(q.tex_min_x, q.tex_max_y),
        .uv_max = V2(q.tex_max_x, q.tex_min_y),
        .color = color
    };

    *pen_x += font_loader_get_advance_x(font_atlas, ch);

    list_append(&draw_data.text_quad_list, text_quad);
}

void e2r_draw_string(const char *str, f32 *pen_x, f32 *pen_y, const FontAtlas *font_atlas, v4 color)
{
    int len = strlen(str);
    f32 starting_x = *pen_x;

    for (int i = 0; i < len; i++)
    {
        if (str[i] != '\n')
        {
            e2r_draw_char(str[i], pen_x, pen_y, font_atlas, color);
        }
        else
        {
            *pen_y += font_loader_get_ascender(font_atlas);
            *pen_x = starting_x;
        }
    }
}

E2R_RenderData e2r_get_ui_render_data()
{
    _UIQuadList *ui_quad_list = &draw_data.ui_quad_list;
    E2R_VertList *vert_list = &draw_data.ui_vert_list;
    E2R_IndexList *index_list = &draw_data.ui_index_list;

    const _UIQuad *quad;
    list_iterate(ui_quad_list, quad_i ,quad)
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

        v2 atlas_q_verts[4] = {};
        _ui_get_atlas_q_verts(quad->atlas_coord, atlas_q_verts);

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

E2R_RenderData e2r_get_text_render_data()
{
    _TextQuadList *quad_list = &draw_data.text_quad_list;
    E2R_VertList *vert_list = &draw_data.text_vert_list;
    E2R_IndexList *index_list = &draw_data.text_index_list;

    const _TextQuad *quad;
    list_iterate(quad_list, quad_i ,quad)
    {
        v2 min = quad->pos_min;
        v2 max = quad->pos_max;

        v3 pos[] =
        {
            V3(quad->pos_min.x, quad->pos_min.y, 0.0f),
            V3(quad->pos_max.x, quad->pos_min.y, 0.0f),
            V3(quad->pos_max.x, quad->pos_max.y, 0.0f),
            V3(quad->pos_min.x, quad->pos_max.y, 0.0f)
        };

        v2 uv[] = 
        {
            V2(quad->uv_min.x, quad->uv_min.y),
            V2(quad->uv_max.x, quad->uv_min.y),
            V2(quad->uv_max.x, quad->uv_max.y),
            V2(quad->uv_min.x, quad->uv_max.y),
        };

        u32 base_index = vert_list->size;

        for (int i = 0; i < 4; i++)
        {
            VertexUI v =
            {
                .pos = pos[i],
                .uv = uv[i],
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
