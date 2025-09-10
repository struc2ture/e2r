#pragma once

#include <vulkan/vulkan.h>
#include "common/types.h"

typedef struct Vertex2D
{
    v3 pos;
    v4 color;

} Vertex2D;

typedef struct VertexUI
{
    v3 pos;
    v2 uv;
    v4 color;

} VertexUI;

typedef struct Vertex3D
{
    v3 pos;
    v3 normal;
    v2 uv;
    v4 color;

} Vertex3D;

typedef u32 VertIndex;
#define VERT_INDEX_TYPE VK_INDEX_TYPE_UINT32
