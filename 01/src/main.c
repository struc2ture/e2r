#include <stdio.h>

#include "e2r_core.h"
#include "common/types.h"
#include "common/util.h"

int main()
{
    e2r_init(1000, 900, "E2R!!!");

    while (e2r_is_running())
    {
        e2r_start_frame();

        e2r_draw();

        e2r_end_frame();
    }

    e2r_destroy();

    return 0;
}
