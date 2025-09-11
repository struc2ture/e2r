#include "print_helpers.h"

#include <stdio.h>

#include "lin_math.h"

void print_m4(m4 m)
{
    printf("m4:\n");
    for (int row = 0; row < 4; row++)
    {
        for (int col = 0; col < 4; col++)
        {
            printf("%.3f%s", m.d[col * 4 + row], ((col < 3) ? ", " : (row < 3 ? ",\n" : "\n")));
        }
    }
}

void print_m4_cols(m4 m)
{
    printf("m4 cols:\n");
    for (int col = 0; col < 4; col++)
    {
        printf("vec4(");
        for (int row = 0; row < 4; row++)
        {
            printf("%.3f%s", m.d[col * 4 + row], ((row < 3) ? ", " : ""));
        }
        printf(");\n");
    }
}

void print_v2(v2 v)
{
    printf("v2(%.3f, %.3f)\n", v.x, v.y);
}