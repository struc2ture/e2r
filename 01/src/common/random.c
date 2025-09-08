#include "random.h"

#include <stdlib.h>

#include "lin_math.h"
#include "types.h"

f32 rand_float()
{
    f32 v = rand() / (f32)RAND_MAX;
    return v;
}

v3 rand_v3(f32 mag)
{
    v3 v = V3(rand_float(), rand_float(), rand_float());
    v = v3_normalize(v);
    v = v3_scale(v, mag);
    return v;
}
