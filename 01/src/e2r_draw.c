#include <stdlib.h>

#include "common/types.h"
#include "common/util.h"

typedef struct _Quad
{
    v2 pos;
    v2 size;
    v4 color;

} _Quad;

typedef struct _QuadList
{
    _Quad *data;
    size_t size;
    size_t cap;
} _QuadList;

typedef struct E2R_DrawData
{
    _QuadList quad_list;

} E2R_DrawData;

globvar E2R_DrawData draw_data;

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
    list_append(list, q);
    list_append(list, q);
    list_append(list, q);
    list_append(list, q);
    list_append(list, q);
    list_append(list, q);

    bp();
}
