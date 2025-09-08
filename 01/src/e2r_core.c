#include "e2r_core.h"
#include "e2r_camera.h"

#include <string.h>

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <vulkan/vulkan_core.h>

#include "common/lin_math.h"
#include "common/print_helpers.h"
#include "common/types.h"
#include "common/util.h"

#define FRAMES_IN_FLIGHT 2
#define MAX_VERTEX_COUNT 1024
#define MAX_INDEX_COUNT 4096

typedef struct Vk_SwapchainBundle
{
    VkSwapchainKHR swapchain;
    VkSurfaceFormatKHR format;
    VkImage *images;
    VkSemaphore *submit_semaphores;
    u32 image_count;
    VkExtent2D extent;

} Vk_SwapchainBundle;

typedef struct Vk_RenderTarget
{
    VkImageView color_view;
    VkImageView depth_view;
    VkFramebuffer framebuffer;

} Vk_RenderTarget;

typedef struct Vk_DepthImageBundle
{
    VkImage *images;
    VkDeviceMemory *memory_list;
    u32 image_count;
    VkFormat depth_format;

} Vk_DepthImageBundle;

typedef struct Vk_RenderTargetGroup
{
    VkRenderPass render_pass;
    Vk_RenderTarget *render_targets;
    u32 render_target_count;
    VkFormat color_format;
    VkFormat depth_format;

} Vk_RenderTargetGroup;

typedef struct Vk_BufferBundle
{
    VkBuffer buffer;
    VkDeviceMemory memory;
    void *data_ptr;
    VkDeviceSize size;

} Vk_BufferBundle;

typedef struct Vk_BufferBundleList
{
    Vk_BufferBundle *buffer_bundles;
    u32 count;

} Vk_BufferBundleList;

typedef struct Vk_Frame
{
    VkCommandBuffer command_buffer;
    VkFence in_flight_fence;
    VkSemaphore acquire_semaphore;

} Vk_Frame;

typedef struct Vk_FrameList
{
    Vk_Frame *frames;
    u32 count;

} Vk_FrameList;

typedef struct Vk_TextureBundle
{
    VkImage image;
    VkDeviceMemory memory;
    VkImageView image_view;
    VkSampler sampler;

} Vk_TextureBundle;

typedef struct Vk_PipelineBundle
{
    VkDescriptorSetLayout descriptor_set_layout;
    VkPipelineLayout pipeline_layout;

    VkDescriptorPool descriptor_pool;
    VkDescriptorSet *descriptor_sets;
    u32 descriptor_set_count;

    VkPipeline pipeline;

    u32 max_vertex_count;
    Vk_BufferBundle vertex_buffer_bundle;

    u32 max_index_count;
    Vk_BufferBundle index_buffer_bundle;

} Vk_PipelineBundle;

// ------------------------------------

typedef struct Vertex2D
{
    v3 pos;
    v4 color;

} Vertex2D;

typedef struct Vertex3D
{
    v3 pos;
    v3 normal;
    v2 tex_coord;
    v4 color;

} Vertex3D;

typedef u32 VertIndex;
#define VERT_INDEX_TYPE VK_INDEX_TYPE_UINT32

typedef struct UBOLayoutGlobal2D
{
    m4 proj;

} UBOLayoutGlobal2D;

typedef struct UBOLayoutGlobal3D
{
    m4 model;
    m4 view_proj;

} UBOLayoutGlobal3D;

// ------------------------------------

typedef struct E2R_Ctx
{
    GLFWwindow *glfw_window;

    VkInstance vk_instance;
    VkSurfaceKHR vk_surface;
    VkPhysicalDevice vk_physical_device;
    u32 vk_queue_family_index;
    VkDevice vk_device;
    VkQueue vk_queue;

    Vk_SwapchainBundle vk_swapchain_bundle;

    Vk_DepthImageBundle vk_depth_image_bundle;

    Vk_RenderTargetGroup vk_render_target_group;
    
    VkCommandPool vk_command_pool;

    Vk_FrameList vk_frame_list;
    u32 current_vk_frame;

    Vk_BufferBundleList global_ubo_2d;
    Vk_BufferBundleList global_ubo_3d;

    Vk_TextureBundle ducks_texture;

    Vk_PipelineBundle vk_tri_pipeline_bundle;
    Vk_PipelineBundle vk_cubes_pipeline_bundle;

    E2R_Camera camera;

} E2R_Ctx;

globvar E2R_Ctx ctx;

GLFWwindow *_glfw_create_window(int width, int height, const char *window_name)
{
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    GLFWwindow *window = glfwCreateWindow(width, height, window_name, NULL, NULL);
    ctx.glfw_window = window;
    return window;
}

v2 _glfw_get_window_size()
{
    int w, h;
    glfwGetWindowSize(ctx.glfw_window, &w, &h);
    return V2(w, h);
}

VkInstance _vk_create_instance()
{
    VkApplicationInfo app_info = {};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.apiVersion = VK_API_VERSION_1_3;

    u32 glfw_ext_count = 0;
    const char **glfw_ext = glfwGetRequiredInstanceExtensions(&glfw_ext_count);
    const char *other_exts[] =
    {
        VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME,
        VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME
    };

    u32 ext_count = 0;
    const char **extensions = xmalloc((glfw_ext_count + array_count(other_exts)) * sizeof(extensions[0]));
    for (u32 i = 0; i < glfw_ext_count; i++)
    {
        extensions[ext_count++] = glfw_ext[i];
    }
    for (u32 i = 0; i < array_count(other_exts); i++)
    {
        extensions[ext_count++] = other_exts[i];
    }

    const char *validation_layers[] = { "VK_LAYER_KHRONOS_validation" };

    VkInstanceCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &app_info;
    create_info.enabledExtensionCount = ext_count;
    create_info.ppEnabledExtensionNames = extensions;
    create_info.enabledLayerCount = array_count(validation_layers);
    create_info.ppEnabledLayerNames = validation_layers;
    create_info.flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;

    VkInstance instance;
    VkResult result = vkCreateInstance(&create_info, NULL, &instance);
    if (result != VK_SUCCESS) fatal("Failed to create instance");

    free(extensions);

    return instance;
}

VkSurfaceKHR _vk_create_surface()
{
    VkSurfaceKHR vk_surface;
    VkResult result = glfwCreateWindowSurface(ctx.vk_instance, ctx.glfw_window, NULL, &vk_surface);
    if (result != VK_SUCCESS) fatal("Failed to create surface");
    return vk_surface;
}

VkPhysicalDevice _vk_find_physical_device()
{
    // Physical device
    u32 count;
    VkResult result = vkEnumeratePhysicalDevices(ctx.vk_instance, &count, NULL);
    if (result != VK_SUCCESS) fatal("Failed to enumerate physical devices");

    VkPhysicalDevice *physical_devices = xmalloc(count * sizeof(physical_devices[0]));

    result = vkEnumeratePhysicalDevices(ctx.vk_instance, &count, physical_devices);
    if (result != VK_SUCCESS) fatal("Failed to enumerate physical devices 2");
    VkPhysicalDevice vk_physical_device = physical_devices[0];

    free(physical_devices);

    return vk_physical_device;
}

u32 _vk_get_queue_family_index()
{
    u32 vk_queue_family_index = 0;
    u32 count;
    vkGetPhysicalDeviceQueueFamilyProperties(ctx.vk_physical_device, &count, NULL);

    VkQueueFamilyProperties *queue_families = xmalloc(count * sizeof(queue_families[0]));

    vkGetPhysicalDeviceQueueFamilyProperties(ctx.vk_physical_device, &count, queue_families);
    for (u32 i = 0; i < count; i++)
    {
        VkBool32 present_support;
        vkGetPhysicalDeviceSurfaceSupportKHR(ctx.vk_physical_device, i, ctx.vk_surface, &present_support);
        if ((queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && present_support)
        {
            vk_queue_family_index = i;
        }
    }

    free(queue_families);

    assert(vk_queue_family_index > 0);

    return vk_queue_family_index;
}

VkDevice _vk_create_device()
{
    float priority = 1.0f;
    VkDeviceQueueCreateInfo queue_create_info = {};
    queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_create_info.queueFamilyIndex = ctx.vk_queue_family_index;
    queue_create_info.queueCount = 1;
    queue_create_info.pQueuePriorities = &priority;

    // VK_KHR_portability_subset must be enabled because physical device VkPhysicalDevice 0x600001667be0 supports it.
    const char *device_extensions[] = {"VK_KHR_portability_subset", "VK_KHR_swapchain"};
    VkDeviceCreateInfo device_create_info = {};
    device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_create_info.queueCreateInfoCount = 1;
    device_create_info.pQueueCreateInfos = &queue_create_info;
    device_create_info.enabledExtensionCount = array_count(device_extensions);
    device_create_info.ppEnabledExtensionNames = device_extensions;

    VkDevice vk_device;
    VkResult result = vkCreateDevice(ctx.vk_physical_device, &device_create_info, NULL, &vk_device);
    if (result != VK_SUCCESS) fatal("Failed to create logical device");

    return vk_device;
}

VkQueue _vk_get_queue()
{
    VkQueue vk_graphics_queue;
    vkGetDeviceQueue(ctx.vk_device, ctx.vk_queue_family_index, 0, &vk_graphics_queue);
    return vk_graphics_queue;
}

Vk_SwapchainBundle _vk_create_swapchain_bundle()
{
    VkSurfaceCapabilitiesKHR capabilities;
    VkResult result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(ctx.vk_physical_device, ctx.vk_surface, &capabilities);
    if (result != VK_SUCCESS) fatal("Failed to get physical device-surface capabilities");

    uint32_t format_count;
    result = vkGetPhysicalDeviceSurfaceFormatsKHR(ctx.vk_physical_device, ctx.vk_surface, &format_count, NULL);
    if (result != VK_SUCCESS) fatal("Failed to get physical device-surface formats");

    VkSurfaceFormatKHR *formats = xmalloc(format_count * sizeof(formats[0]));

    result = vkGetPhysicalDeviceSurfaceFormatsKHR(ctx.vk_physical_device, ctx.vk_surface, &format_count, formats);
    if (result != VK_SUCCESS) fatal("Failed to get physical device-surface formats 2");

    VkSurfaceFormatKHR surface_format = formats[0];
    assert(surface_format.format == VK_FORMAT_B8G8R8A8_UNORM && surface_format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR);

    free(formats);

    u32 image_count = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && image_count > capabilities.maxImageCount)
    {
        image_count = capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR swapchain_create_info = {};
    swapchain_create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchain_create_info.surface = ctx.vk_surface;
    swapchain_create_info.minImageCount = image_count;
    swapchain_create_info.imageFormat = surface_format.format;
    swapchain_create_info.imageColorSpace = surface_format.colorSpace;
    swapchain_create_info.imageExtent = capabilities.currentExtent;
    swapchain_create_info.imageArrayLayers = 1;
    swapchain_create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapchain_create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapchain_create_info.preTransform = capabilities.currentTransform;
    swapchain_create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchain_create_info.presentMode = VK_PRESENT_MODE_FIFO_KHR; // vsync
    swapchain_create_info.clipped = VK_TRUE;

    VkSwapchainKHR swapchain;
    result = vkCreateSwapchainKHR(ctx.vk_device, &swapchain_create_info, NULL, &swapchain);
    if (result != VK_SUCCESS) fatal("Failed to create swapchain");

    result = vkGetSwapchainImagesKHR(ctx.vk_device, swapchain, &image_count, NULL);
    if (result != VK_SUCCESS) fatal("Failed to get swapchain images");

    VkImage *images = xmalloc(image_count * sizeof(images[0]));

    result = vkGetSwapchainImagesKHR(ctx.vk_device, swapchain, &image_count, images);
    if (result != VK_SUCCESS) fatal("Failed to get swapchain images 2");

    VkSemaphoreCreateInfo semaphore_create_info = {};
    semaphore_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkSemaphore *submit_semaphores = xmalloc(image_count * sizeof(submit_semaphores[0]));

    for (u32 i = 0; i < image_count; i++)
    {
        result = vkCreateSemaphore(ctx.vk_device, &semaphore_create_info, NULL, &submit_semaphores[i]);
        if (result != VK_SUCCESS) fatal("Failed to create submit semaphore");
    }

    return (Vk_SwapchainBundle){
        .format = surface_format,
        .swapchain = swapchain,
        .image_count = image_count,
        .images = images,
        .submit_semaphores = submit_semaphores,
        .extent = capabilities.currentExtent
    };
}

u32 _vk_find_memory_type(VkPhysicalDevice physical_device, u32 type_filter, VkMemoryPropertyFlags props)
{
    VkPhysicalDeviceMemoryProperties mem_props;
    vkGetPhysicalDeviceMemoryProperties(physical_device, &mem_props);
    for (u32 i = 0; i < mem_props.memoryTypeCount; i++)
    {
        if ((type_filter & (1 << i)) &&
            (mem_props.memoryTypes[i].propertyFlags & props) == props)
        {
            return i;
        }
    }
    fatal("Failed to find suitable memory type");
    return 0;
}

Vk_DepthImageBundle _vk_create_depth_image_bundle()
{
    VkFormat depth_format = VK_FORMAT_D32_SFLOAT;

    Vk_DepthImageBundle depth_image_bundle =
    {
        .image_count = ctx.vk_swapchain_bundle.image_count,
        .depth_format = depth_format
    };

    VkResult result;

    VkImage *images = xmalloc(depth_image_bundle.image_count * sizeof(images[0]));
    VkDeviceMemory *memory_list = xmalloc(depth_image_bundle.image_count * sizeof(memory_list[0]));

    for (u32 i = 0; i < depth_image_bundle.image_count; i++)
    {
        VkImageCreateInfo image_create_info = {};
        image_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        image_create_info.imageType = VK_IMAGE_TYPE_2D;
        image_create_info.extent.width = ctx.vk_swapchain_bundle.extent.width;
        image_create_info.extent.height = ctx.vk_swapchain_bundle.extent.height;
        image_create_info.extent.depth = 1;
        image_create_info.mipLevels = 1;
        image_create_info.arrayLayers = 1;
        image_create_info.format = depth_format;
        image_create_info.tiling = VK_IMAGE_TILING_OPTIMAL;
        image_create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        image_create_info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        image_create_info.samples = VK_SAMPLE_COUNT_1_BIT;
        image_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        result = vkCreateImage(ctx.vk_device, &image_create_info, NULL, &images[i]);
        if (result != VK_SUCCESS) fatal("Failed to create depth buffer image");

        VkMemoryRequirements mem_req;
        vkGetImageMemoryRequirements(ctx.vk_device, images[i], &mem_req);

        VkMemoryAllocateInfo allocate_info = {};
        allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocate_info.allocationSize = mem_req.size;
        allocate_info.memoryTypeIndex = _vk_find_memory_type(
            ctx.vk_physical_device,
            mem_req.memoryTypeBits,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        result = vkAllocateMemory(ctx.vk_device, &allocate_info, NULL, &memory_list[i]);
        if (result != VK_SUCCESS) fatal("Failed to allocate memory for depth buffer image");

        result = vkBindImageMemory(ctx.vk_device, images[i], memory_list[i], 0);
        if (result != VK_SUCCESS) fatal("Failed to bind memory for depth buffer image");
    }

    depth_image_bundle.images = images;
    depth_image_bundle.memory_list = memory_list;

    return depth_image_bundle;
}

Vk_RenderTargetGroup _vk_create_render_target_group()
{
    Vk_RenderTargetGroup render_target_group =
    {
        .color_format = ctx.vk_swapchain_bundle.format.format,
        .depth_format = ctx.vk_depth_image_bundle.depth_format,
        .render_target_count = ctx.vk_swapchain_bundle.image_count
    };

    VkResult result;

    VkRenderPass render_pass;
    {
        VkAttachmentDescription color_attachment_description = {};
        color_attachment_description.format = render_target_group.color_format;
        color_attachment_description.samples = VK_SAMPLE_COUNT_1_BIT;
        color_attachment_description.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        color_attachment_description.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        color_attachment_description.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        color_attachment_description.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentReference color_attachment_reference = {};
        color_attachment_reference.attachment = 0;
        color_attachment_reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentDescription depth_attachment_description = {};
        depth_attachment_description.format = render_target_group.depth_format;
        depth_attachment_description.samples = VK_SAMPLE_COUNT_1_BIT;
        depth_attachment_description.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depth_attachment_description.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depth_attachment_description.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depth_attachment_description.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depth_attachment_description.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depth_attachment_description.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkAttachmentReference depth_attachment_reference = {};
        depth_attachment_reference.attachment = 1;
        depth_attachment_reference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass_description = {};
        subpass_description.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass_description.colorAttachmentCount = 1;
        subpass_description.pColorAttachments = &color_attachment_reference;
        subpass_description.pDepthStencilAttachment = &depth_attachment_reference;

        VkAttachmentDescription render_pass_attachments[] = { color_attachment_description, depth_attachment_description };
        VkRenderPassCreateInfo render_pass_create_info = {};
        render_pass_create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        render_pass_create_info.attachmentCount = array_count(render_pass_attachments);
        render_pass_create_info.pAttachments = render_pass_attachments;
        render_pass_create_info.subpassCount = 1;
        render_pass_create_info.pSubpasses = &subpass_description;

        result = vkCreateRenderPass(ctx.vk_device, &render_pass_create_info, NULL, &render_pass);
        if (result != VK_SUCCESS) fatal("Failed to create render pass");
    }
    render_target_group.render_pass = render_pass;

    Vk_RenderTarget *render_targets = xmalloc(render_target_group.render_target_count * sizeof(render_targets[0]));
    {
        for (u32 i = 0; i < render_target_group.render_target_count; i++)
        {
            VkImageView color_view;
            {
                VkImageViewCreateInfo create_info = {};
                create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
                create_info.image = ctx.vk_swapchain_bundle.images[i];
                create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
                create_info.format = render_target_group.color_format;
                create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
                create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
                create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
                create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
                create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                create_info.subresourceRange.baseMipLevel = 0;
                create_info.subresourceRange.levelCount = 1;
                create_info.subresourceRange.baseArrayLayer = 0;
                create_info.subresourceRange.layerCount = 1;

                VkResult result = vkCreateImageView(ctx.vk_device, &create_info, NULL, &color_view);
                if (result != VK_SUCCESS) fatal("Failed to create image view");
            }


            VkImageView depth_view;
            {
                VkImageViewCreateInfo create_info = {};
                create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
                create_info.image = ctx.vk_depth_image_bundle.images[i];
                create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
                create_info.format = render_target_group.depth_format;
                create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
                create_info.subresourceRange.baseMipLevel = 0;
                create_info.subresourceRange.levelCount = 1;
                create_info.subresourceRange.baseArrayLayer = 0;
                create_info.subresourceRange.layerCount = 1;

                VkResult result = vkCreateImageView(ctx.vk_device, &create_info, NULL, &depth_view);
                if (result != VK_SUCCESS) fatal("Failed to create image view");
            }

            VkFramebuffer framebuffer;
            {
                VkImageView attachments[] = { color_view, depth_view };

                VkFramebufferCreateInfo create_info = {};
                create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
                create_info.renderPass = render_pass;
                create_info.attachmentCount = array_count(attachments);
                create_info.pAttachments = attachments;
                create_info.width = ctx.vk_swapchain_bundle.extent.width;
                create_info.height = ctx.vk_swapchain_bundle.extent.height;
                create_info.layers = 1;

                result = vkCreateFramebuffer(ctx.vk_device, &create_info, NULL, &framebuffer);
                if (result != VK_SUCCESS) fatal("Failed to create framebuffer");
            }

            render_targets[i].color_view = color_view;
            render_targets[i].depth_view = depth_view;
            render_targets[i].framebuffer = framebuffer;
        }
    }
    render_target_group.render_targets = render_targets;

    return render_target_group;
}

VkCommandPool _vk_create_command_pool()
{
    VkCommandPoolCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    create_info.queueFamilyIndex = ctx.vk_queue_family_index;
    create_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT; // allows resetting individual buffers

    VkCommandPool command_pool;
    VkResult result = vkCreateCommandPool(ctx.vk_device, &create_info, NULL, &command_pool);
    if (result != VK_SUCCESS) fatal("Failed to create command pool");

    return command_pool;
}

Vk_FrameList _vk_create_frame_list()
{
    Vk_FrameList frame_bundle =
    {
        .count = FRAMES_IN_FLIGHT,
    };

    Vk_Frame *frames = xmalloc(frame_bundle.count * sizeof(frames[0]));

    for (u32 i = 0; i < frame_bundle.count; i++)
    {
        VkCommandBufferAllocateInfo command_buffer_allocate_info = {};
        command_buffer_allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        command_buffer_allocate_info.commandPool = ctx.vk_command_pool;
        command_buffer_allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        command_buffer_allocate_info.commandBufferCount = 1;

        VkCommandBuffer command_buffer;
        VkResult result = vkAllocateCommandBuffers(ctx.vk_device, &command_buffer_allocate_info, &command_buffer);
        if (result != VK_SUCCESS) fatal("Failed to allocate command buffer");

        VkFenceCreateInfo fence_create_info = {};
        fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fence_create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        VkFence in_flight_fence;
        result = vkCreateFence(ctx.vk_device, &fence_create_info, NULL, &in_flight_fence);
        if (result != VK_SUCCESS) fatal("Failed to create in flight fence");

        VkSemaphoreCreateInfo semaphore_create_info = {};
        semaphore_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkSemaphore acquire_semaphore;
        result = vkCreateSemaphore(ctx.vk_device, &semaphore_create_info, NULL, &acquire_semaphore);
        if (result != VK_SUCCESS) fatal("Failed to create acquire semaphore");

        frames[i] = (Vk_Frame){
            .command_buffer = command_buffer,
            .in_flight_fence = in_flight_fence,
            .acquire_semaphore = acquire_semaphore
        };
    }

    frame_bundle.frames = frames;

    return frame_bundle;
}

VkShaderModule _vk_create_shader_module(const char *path)
{
    FILE *file = fopen(path, "rb");
    assert(file);
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    rewind(file);
    char* buffer = xmalloc(size);
    assert(buffer);
    fread(buffer, 1, size, file);
    fclose(file);

    VkShaderModuleCreateInfo shader_module_create_info = {};
    shader_module_create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shader_module_create_info.codeSize = (size_t)size;
    shader_module_create_info.pCode = (uint32_t *)buffer;

    VkShaderModule module;
    VkResult result = vkCreateShaderModule(ctx.vk_device, &shader_module_create_info, NULL, &module);
    if (result != VK_SUCCESS) fatal("Failed to create shader module");

    free(buffer);
    return module;
}

Vk_BufferBundle _vk_create_buffer_bundle(VkDeviceSize size, VkBufferUsageFlags usage)
{
    VkResult result;

    VkBuffer buffer;
    {
        VkBufferCreateInfo create_info = {};
        create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        create_info.size = size;
        create_info.usage = usage;
        create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        result = vkCreateBuffer(ctx.vk_device, &create_info, NULL, &buffer);
        if (result != VK_SUCCESS) fatal("Failed to create uniform buffer");
    }

    VkDeviceMemory device_memory;
    {
        VkMemoryRequirements memory_requirements;
        vkGetBufferMemoryRequirements(ctx.vk_device, buffer, &memory_requirements);

        VkMemoryAllocateInfo allocate_info = {};
        allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocate_info.allocationSize = memory_requirements.size;
        allocate_info.memoryTypeIndex = _vk_find_memory_type(
            ctx.vk_physical_device,
            memory_requirements.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        );

        result = vkAllocateMemory(ctx.vk_device, &allocate_info, NULL, &device_memory);
        if (result != VK_SUCCESS) fatal("Failed to allocate memory for uniform buffer");
    }

    result = vkBindBufferMemory(ctx.vk_device, buffer, device_memory, 0);
    if (result != VK_SUCCESS) fatal("Failed to bind memory to uniform buffer");

    void *data;
    result = vkMapMemory(ctx.vk_device, device_memory, 0, size, 0, &data);
    if (result != VK_SUCCESS) fatal("Failed to map vertex buffer memory");

    return (Vk_BufferBundle){
        .buffer = buffer,
        .memory = device_memory,
        .data_ptr = data,
        .size = size
    };
}

Vk_BufferBundleList _vk_create_buffer_bundle_list(VkDeviceSize max_size, VkBufferUsageFlags usage)
{
    Vk_BufferBundleList buffer_bundle_list =
    {
        .count = FRAMES_IN_FLIGHT
    };

    Vk_BufferBundle *buffer_bundles = xmalloc(buffer_bundle_list.count * sizeof(buffer_bundles[0]));

    for (u32 i = 0; i < buffer_bundle_list.count; i++)
    {
        buffer_bundles[i] = _vk_create_buffer_bundle(max_size, usage);
    }

    buffer_bundle_list.buffer_bundles = buffer_bundles;

    return buffer_bundle_list;
}

Vk_PipelineBundle _vk_create_pipeline_bundle_tri()
{
    Vk_PipelineBundle pipeline_bundle =
    {
        .max_vertex_count = MAX_VERTEX_COUNT
    };

    u32 frame_count = FRAMES_IN_FLIGHT;

    // Descriptor set layout
    VkDescriptorSetLayoutBinding descriptor_set_layout_binding_0 = {};
    descriptor_set_layout_binding_0.binding = 0;
    descriptor_set_layout_binding_0.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptor_set_layout_binding_0.descriptorCount = 1;
    descriptor_set_layout_binding_0.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutBinding descriptor_set_layout_bindings[] = {descriptor_set_layout_binding_0};
    VkDescriptorSetLayoutCreateInfo descriptor_set_layout_create_info = {};
    descriptor_set_layout_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptor_set_layout_create_info.bindingCount = array_count(descriptor_set_layout_bindings);
    descriptor_set_layout_create_info.pBindings = descriptor_set_layout_bindings;

    VkDescriptorSetLayout descriptor_set_layout;
    VkResult result = vkCreateDescriptorSetLayout(ctx.vk_device, &descriptor_set_layout_create_info, NULL, &descriptor_set_layout);
    if (result != VK_SUCCESS) fatal("Failed to create descriptor set layout");

    VkPipelineLayoutCreateInfo pipeline_layout_create_info = {};
    pipeline_layout_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_create_info.setLayoutCount = 1;
    pipeline_layout_create_info.pSetLayouts = &descriptor_set_layout;

    VkPipelineLayout pipeline_layout;
    result = vkCreatePipelineLayout(ctx.vk_device, &pipeline_layout_create_info, NULL, &pipeline_layout);
    if (result != VK_SUCCESS) fatal("Failed to create pipeline layout");

    pipeline_bundle.vertex_buffer_bundle = _vk_create_buffer_bundle(
        pipeline_bundle.max_vertex_count * sizeof(Vertex2D),
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
    );

    pipeline_bundle.descriptor_set_layout = descriptor_set_layout;
    pipeline_bundle.pipeline_layout = pipeline_layout;

    // Descriptor pool
    VkDescriptorPoolSize descriptor_pool_sizes[1] = {};
    descriptor_pool_sizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptor_pool_sizes[0].descriptorCount = frame_count;

    VkDescriptorPoolCreateInfo decriptor_pool_create_info = {};
    decriptor_pool_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    decriptor_pool_create_info.poolSizeCount = 1;
    decriptor_pool_create_info.pPoolSizes = descriptor_pool_sizes;
    decriptor_pool_create_info.maxSets = frame_count;

    VkDescriptorPool descriptor_pool;
    result = vkCreateDescriptorPool(ctx.vk_device, &decriptor_pool_create_info, NULL, &descriptor_pool);
    if (result != VK_SUCCESS) fatal("Failed to create descriptor pool");

    VkDescriptorSetAllocateInfo descriptor_set_allocate_info = {};
    descriptor_set_allocate_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descriptor_set_allocate_info.descriptorPool = descriptor_pool;
    descriptor_set_allocate_info.descriptorSetCount = 1;
    descriptor_set_allocate_info.pSetLayouts = &descriptor_set_layout;

    VkDescriptorSet *descriptor_sets = xmalloc(frame_count * sizeof(descriptor_sets[0]));
    for (u32 i = 0; i < frame_count; i++)
    {
        result = vkAllocateDescriptorSets(ctx.vk_device, &descriptor_set_allocate_info, &descriptor_sets[i]);
        if (result != VK_SUCCESS) fatal("Failed to allocate descriptor set");

        const Vk_BufferBundle *buffer_bundle = &ctx.global_ubo_2d.buffer_bundles[i];
        VkDescriptorBufferInfo descriptor_buffer_info = {};
        descriptor_buffer_info.buffer = buffer_bundle->buffer;
        descriptor_buffer_info.offset = 0;
        descriptor_buffer_info.range = buffer_bundle->size;

        VkWriteDescriptorSet uniform_buffer_write_descriptor_set = {};
        uniform_buffer_write_descriptor_set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        uniform_buffer_write_descriptor_set.dstSet = descriptor_sets[i];
        uniform_buffer_write_descriptor_set.dstBinding = 0;
        uniform_buffer_write_descriptor_set.dstArrayElement = 0;
        uniform_buffer_write_descriptor_set.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        uniform_buffer_write_descriptor_set.descriptorCount = 1;
        uniform_buffer_write_descriptor_set.pBufferInfo = &descriptor_buffer_info;

        vkUpdateDescriptorSets(ctx.vk_device, 1, &uniform_buffer_write_descriptor_set, 0, NULL);
    }

    pipeline_bundle.descriptor_pool = descriptor_pool;
    pipeline_bundle.descriptor_sets = descriptor_sets;
    pipeline_bundle.descriptor_set_count = frame_count;

    VkShaderModule vk_vert_shader_module = _vk_create_shader_module("bin/shaders/tri.vert.spv");
    VkShaderModule vk_frag_shader_module = _vk_create_shader_module("bin/shaders/tri.frag.spv");

    VkPipelineShaderStageCreateInfo pipeline_shader_stage_create_infos[2] = {};
    pipeline_shader_stage_create_infos[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipeline_shader_stage_create_infos[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    pipeline_shader_stage_create_infos[0].module = vk_vert_shader_module;
    pipeline_shader_stage_create_infos[0].pName = "main";
    pipeline_shader_stage_create_infos[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipeline_shader_stage_create_infos[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    pipeline_shader_stage_create_infos[1].module = vk_frag_shader_module;
    pipeline_shader_stage_create_infos[1].pName = "main";

    VkVertexInputBindingDescription vertex_input_binding_description = {};
    vertex_input_binding_description.binding = 0;
    vertex_input_binding_description.stride = sizeof(Vertex2D);
    vertex_input_binding_description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    int vert_attrib_count = 2;
    VkVertexInputAttributeDescription *vertex_input_attribute_descriptions = xmalloc(vert_attrib_count * sizeof(vertex_input_attribute_descriptions[0]));
    vertex_input_attribute_descriptions[0] = (VkVertexInputAttributeDescription){
        .location = 0,
        .binding = 0,
        .format = VK_FORMAT_R32G32B32_SFLOAT,
        .offset = offsetof(Vertex2D, pos)
    };
    vertex_input_attribute_descriptions[1] = (VkVertexInputAttributeDescription){
        .location = 1,
        .binding = 0,
        .format = VK_FORMAT_R32G32B32A32_SFLOAT,
        .offset = offsetof(Vertex2D, color)
    };

    VkPipelineVertexInputStateCreateInfo pipeline_vertex_input_state_create_info = {};
    pipeline_vertex_input_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    pipeline_vertex_input_state_create_info.vertexBindingDescriptionCount = 1;
    pipeline_vertex_input_state_create_info.pVertexBindingDescriptions = &vertex_input_binding_description;
    pipeline_vertex_input_state_create_info.vertexAttributeDescriptionCount = vert_attrib_count;
    pipeline_vertex_input_state_create_info.pVertexAttributeDescriptions = vertex_input_attribute_descriptions;

    VkPipelineInputAssemblyStateCreateInfo pipeline_input_assembly_create_info = {};
    pipeline_input_assembly_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    pipeline_input_assembly_create_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkViewport viewport = {0, 0, (float)ctx.vk_swapchain_bundle.extent.width, (float)ctx.vk_swapchain_bundle.extent.height, 0.0f, 1.0f};
    VkRect2D scissor = {{0, 0}, {ctx.vk_swapchain_bundle.extent.width, ctx.vk_swapchain_bundle.extent.height}};
    VkPipelineViewportStateCreateInfo pipeline_viewport_state_create_info = {};
    pipeline_viewport_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    pipeline_viewport_state_create_info.viewportCount = 1;
    pipeline_viewport_state_create_info.pViewports = &viewport;
    pipeline_viewport_state_create_info.scissorCount = 1;
    pipeline_viewport_state_create_info.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo pipeline_rasterization_state_create_info = {};
    pipeline_rasterization_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    pipeline_rasterization_state_create_info.polygonMode = VK_POLYGON_MODE_FILL;
    pipeline_rasterization_state_create_info.lineWidth = 1.0f;
    pipeline_rasterization_state_create_info.cullMode = VK_CULL_MODE_NONE;
    pipeline_rasterization_state_create_info.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    VkPipelineMultisampleStateCreateInfo pipeline_multisample_state_create_info = {};
    pipeline_multisample_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    pipeline_multisample_state_create_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState pipeline_color_blend_attachment_state = {};
    pipeline_color_blend_attachment_state.colorWriteMask = (VK_COLOR_COMPONENT_R_BIT |
                                                            VK_COLOR_COMPONENT_G_BIT |
                                                            VK_COLOR_COMPONENT_B_BIT |
                                                            VK_COLOR_COMPONENT_A_BIT);
    pipeline_color_blend_attachment_state.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo pipeline_color_blend_state_create_info = {};
    pipeline_color_blend_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    pipeline_color_blend_state_create_info.attachmentCount = 1;
    pipeline_color_blend_state_create_info.pAttachments = &pipeline_color_blend_attachment_state;

    VkGraphicsPipelineCreateInfo graphics_pipeline_create_info = {};
    graphics_pipeline_create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    graphics_pipeline_create_info.stageCount = array_count(pipeline_shader_stage_create_infos);
    graphics_pipeline_create_info.pStages = pipeline_shader_stage_create_infos;
    graphics_pipeline_create_info.pVertexInputState = &pipeline_vertex_input_state_create_info;
    graphics_pipeline_create_info.pInputAssemblyState = &pipeline_input_assembly_create_info;
    graphics_pipeline_create_info.pViewportState = &pipeline_viewport_state_create_info;
    graphics_pipeline_create_info.pRasterizationState = &pipeline_rasterization_state_create_info;
    graphics_pipeline_create_info.pMultisampleState = &pipeline_multisample_state_create_info;
    graphics_pipeline_create_info.pColorBlendState = &pipeline_color_blend_state_create_info;
    graphics_pipeline_create_info.layout = pipeline_layout;
    graphics_pipeline_create_info.renderPass = ctx.vk_render_target_group.render_pass;
    graphics_pipeline_create_info.subpass = 0;
    
    VkPipeline pipeline;
    result = vkCreateGraphicsPipelines(ctx.vk_device, VK_NULL_HANDLE, 1, &graphics_pipeline_create_info, NULL, &pipeline);
    if (result != VK_SUCCESS) fatal("Failed to create graphics pipeline");

    free(vertex_input_attribute_descriptions);

    vkDestroyShaderModule(ctx.vk_device, vk_vert_shader_module, NULL);
    vkDestroyShaderModule(ctx.vk_device, vk_frag_shader_module, NULL);

    pipeline_bundle.pipeline = pipeline;

    return pipeline_bundle;
}

void _vk_destroy_buffer_bundle(Vk_BufferBundle *bundle);

Vk_TextureBundle _vk_load_texture(const char *path)
{
    int tex_w, tex_h, tex_ch;
    stbi_set_flip_vertically_on_load(true);
    stbi_uc *pixels = stbi_load(path, &tex_w, &tex_h, &tex_ch, STBI_rgb_alpha);
    VkDeviceSize image_size = tex_w * tex_h * 4; // 4 bytes per pixel

    VkResult result;

    Vk_BufferBundle texture_staging_buffer = _vk_create_buffer_bundle(image_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);

    memcpy(texture_staging_buffer.data_ptr, pixels, (size_t)image_size);

    free(pixels);

    // Create image for the texture
    VkImage texture_image;
    {
        VkImageCreateInfo texture_image_create_info = {};
        texture_image_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        texture_image_create_info.imageType = VK_IMAGE_TYPE_2D;
        texture_image_create_info.format = VK_FORMAT_R8G8B8A8_UNORM;
        texture_image_create_info.extent = (VkExtent3D){ (u32)tex_w, (u32)tex_h, 1 };
        texture_image_create_info.mipLevels = 1;
        texture_image_create_info.arrayLayers = 1;
        texture_image_create_info.samples = VK_SAMPLE_COUNT_1_BIT;
        texture_image_create_info.tiling = VK_IMAGE_TILING_OPTIMAL;
        texture_image_create_info.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        texture_image_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        texture_image_create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        result = vkCreateImage(ctx.vk_device, &texture_image_create_info, NULL, &texture_image);
        if (result != VK_SUCCESS) fatal("Failed to create texture image");
    }

    VkDeviceMemory texture_image_memory;
    {
        VkMemoryRequirements mem_req;
        vkGetImageMemoryRequirements(ctx.vk_device, texture_image, &mem_req);

        VkMemoryAllocateInfo allocate_info = {};
        allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocate_info.allocationSize = mem_req.size;
        allocate_info.memoryTypeIndex = _vk_find_memory_type(
            ctx.vk_physical_device,
            mem_req.memoryTypeBits,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        );

        result = vkAllocateMemory(ctx.vk_device, &allocate_info, NULL, &texture_image_memory);
        if (result != VK_SUCCESS) fatal("Failed to allocate memory for texture image");
    }

    result = vkBindImageMemory(ctx.vk_device, texture_image, texture_image_memory, 0);
    if (result != VK_SUCCESS) fatal("Failed to bind memory to texture image");

    VkCommandBuffer command_buffer;
    {
        VkCommandBufferAllocateInfo texture_command_buffer_allocate_info = {};
        texture_command_buffer_allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        texture_command_buffer_allocate_info.commandPool = ctx.vk_command_pool;
        texture_command_buffer_allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        texture_command_buffer_allocate_info.commandBufferCount = 1;

        result = vkAllocateCommandBuffers(ctx.vk_device, &texture_command_buffer_allocate_info, &command_buffer);
        if (result != VK_SUCCESS) fatal("Failed to allocate command buffer for texture");
    }

    // Record commands for copying texture from staging buffer to device-local image memory
    {
        {
            VkCommandBufferBeginInfo begin_info = {};
            begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            result = vkBeginCommandBuffer(command_buffer, &begin_info);
            if (result != VK_SUCCESS) fatal("Failed to begin texture command buffer");
        }

        {
            VkImageMemoryBarrier barrier1 = {};
            barrier1.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier1.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            barrier1.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier1.srcAccessMask = 0;
            barrier1.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier1.image = texture_image;
            barrier1.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            barrier1.subresourceRange.baseMipLevel = 0;
            barrier1.subresourceRange.levelCount = 1;
            barrier1.subresourceRange.baseArrayLayer = 0;
            barrier1.subresourceRange.layerCount = 1;

            vkCmdPipelineBarrier(
                command_buffer,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                0,
                0, NULL,
                0, NULL,
                1, &barrier1
            );
        }

        {
            VkBufferImageCopy buffer_image_copy = {};
            buffer_image_copy.bufferOffset = 0;
            buffer_image_copy.bufferRowLength = 0;
            buffer_image_copy.bufferImageHeight = 0;
            buffer_image_copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            buffer_image_copy.imageSubresource.mipLevel = 0;
            buffer_image_copy.imageSubresource.baseArrayLayer = 0;
            buffer_image_copy.imageSubresource.layerCount = 1;
            buffer_image_copy.imageOffset = (VkOffset3D){0, 0, 0};
            buffer_image_copy.imageExtent = (VkExtent3D){(u32)tex_w, (u32)tex_h, 1};

            vkCmdCopyBufferToImage(
                command_buffer,
                texture_staging_buffer.buffer,
                texture_image,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                1,
                &buffer_image_copy
            );
        }

        {
            VkImageMemoryBarrier barrier2 = {};
            barrier2.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier2.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier2.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            barrier2.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier2.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            barrier2.image = texture_image;
            barrier2.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            barrier2.subresourceRange.baseMipLevel = 0;
            barrier2.subresourceRange.levelCount = 1;
            barrier2.subresourceRange.baseArrayLayer = 0;
            barrier2.subresourceRange.layerCount = 1;

            vkCmdPipelineBarrier(
                command_buffer,
                VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                0,
                0, NULL,
                0, NULL,
                1, &barrier2
            );
        }

        result = vkEndCommandBuffer(command_buffer);
        if (result != VK_SUCCESS) fatal("Failed to end texture command buffer");
    }

    {
        VkSubmitInfo submit_info = {};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &command_buffer;

        result = vkQueueSubmit(ctx.vk_queue, 1, &submit_info, VK_NULL_HANDLE);
        if (result != VK_SUCCESS) fatal("Failed to submit texture command buffer to queue");
    }

    result = vkQueueWaitIdle(ctx.vk_queue);
    if (result != VK_SUCCESS) fatal("Failed to wait idle for queue");

    _vk_destroy_buffer_bundle(&texture_staging_buffer);

    VkImageView texture_image_view;
    {
        VkImageViewCreateInfo texture_image_view_create_info = {};
        texture_image_view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        texture_image_view_create_info.image = texture_image;
        texture_image_view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        texture_image_view_create_info.format = VK_FORMAT_R8G8B8A8_UNORM;
        texture_image_view_create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        texture_image_view_create_info.subresourceRange.baseMipLevel = 0;
        texture_image_view_create_info.subresourceRange.levelCount = 1;
        texture_image_view_create_info.subresourceRange.baseArrayLayer = 0;
        texture_image_view_create_info.subresourceRange.layerCount = 1;

        result = vkCreateImageView(ctx.vk_device, &texture_image_view_create_info, NULL, &texture_image_view);
        if (result != VK_SUCCESS) fatal("Failed to create texture image view");
    }

    VkSampler texture_sampler;
    {
        VkSamplerCreateInfo texture_sampler_create_info = {};
        texture_sampler_create_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        texture_sampler_create_info.magFilter = VK_FILTER_LINEAR;
        texture_sampler_create_info.minFilter = VK_FILTER_LINEAR;
        texture_sampler_create_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        texture_sampler_create_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        texture_sampler_create_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        texture_sampler_create_info.anisotropyEnable = VK_FALSE;
        texture_sampler_create_info.maxAnisotropy = 1.0f;
        texture_sampler_create_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        texture_sampler_create_info.unnormalizedCoordinates = VK_FALSE;
        texture_sampler_create_info.compareEnable = VK_FALSE;
        texture_sampler_create_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

        result = vkCreateSampler(ctx.vk_device, &texture_sampler_create_info, NULL, &texture_sampler);
        if (result != VK_SUCCESS) fatal("Failed to create texture sampler");
    }

    return (Vk_TextureBundle){
        .image = texture_image,
        .memory = texture_image_memory,
        .image_view = texture_image_view,
        .sampler = texture_sampler
    };
}

Vk_PipelineBundle _vk_create_pipeline_bundle_cubes()
{
    const char *vert_shader_path = "bin/shaders/cubes.vert.spv";
    const char *frag_shader_path = "bin/shaders/cubes.frag.spv";

    Vk_PipelineBundle pipeline_bundle =
    {
        .max_vertex_count = MAX_VERTEX_COUNT,
        .max_index_count = MAX_INDEX_COUNT
    };

    u32 frame_count = FRAMES_IN_FLIGHT;

    VkResult result;

    VkDescriptorSetLayout descriptor_set_layout;
    {
        // Global 3D UBO
        VkDescriptorSetLayoutBinding descriptor_set_layout_binding_0 = {};
        descriptor_set_layout_binding_0.binding = 0;
        descriptor_set_layout_binding_0.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptor_set_layout_binding_0.descriptorCount = 1;
        descriptor_set_layout_binding_0.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

        // Texture Sampler
        VkDescriptorSetLayoutBinding descriptor_set_layout_binding_1 = {};
        descriptor_set_layout_binding_1.binding = 1;
        descriptor_set_layout_binding_1.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptor_set_layout_binding_1.descriptorCount = 1;
        descriptor_set_layout_binding_1.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutBinding descriptor_set_layout_bindings[] = {descriptor_set_layout_binding_0, descriptor_set_layout_binding_1};
        VkDescriptorSetLayoutCreateInfo descriptor_set_layout_create_info = {};
        descriptor_set_layout_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        descriptor_set_layout_create_info.bindingCount = array_count(descriptor_set_layout_bindings);
        descriptor_set_layout_create_info.pBindings = descriptor_set_layout_bindings;

        result = vkCreateDescriptorSetLayout(ctx.vk_device, &descriptor_set_layout_create_info, NULL, &descriptor_set_layout);
        if (result != VK_SUCCESS) fatal("Failed to create descriptor set layout");
    }

    VkPipelineLayout pipeline_layout;
    {
        // VkPushConstantRange model_push_constant = {};
        // model_push_constant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        // model_push_constant.offset = 0;
        // model_push_constant.size = sizeof(m4);

        VkPipelineLayoutCreateInfo pipeline_layout_create_info = {};
        pipeline_layout_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipeline_layout_create_info.setLayoutCount = 1;
        pipeline_layout_create_info.pSetLayouts = &descriptor_set_layout;
        // pipeline_layout_create_info.pushConstantRangeCount = 1;
        // pipeline_layout_create_info.pPushConstantRanges = &model_push_constant;

        result = vkCreatePipelineLayout(ctx.vk_device, &pipeline_layout_create_info, NULL, &pipeline_layout);
        if (result != VK_SUCCESS) fatal("Failed to create pipeline layout");
    }

    pipeline_bundle.vertex_buffer_bundle = _vk_create_buffer_bundle(
        pipeline_bundle.max_vertex_count * sizeof(Vertex3D),
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
    );

    pipeline_bundle.index_buffer_bundle = _vk_create_buffer_bundle(
        pipeline_bundle.max_index_count * sizeof(VertIndex),
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT
    );

    pipeline_bundle.descriptor_set_layout = descriptor_set_layout;
    pipeline_bundle.pipeline_layout = pipeline_layout;

    VkDescriptorPool descriptor_pool;
    {
        VkDescriptorPoolSize descriptor_pool_sizes[2] = {};
        descriptor_pool_sizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptor_pool_sizes[0].descriptorCount = frame_count;
        descriptor_pool_sizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptor_pool_sizes[1].descriptorCount = 1;

        VkDescriptorPoolCreateInfo decriptor_pool_create_info = {};
        decriptor_pool_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        decriptor_pool_create_info.poolSizeCount = array_count(descriptor_pool_sizes);
        decriptor_pool_create_info.pPoolSizes = descriptor_pool_sizes;
        decriptor_pool_create_info.maxSets = frame_count;

        result = vkCreateDescriptorPool(ctx.vk_device, &decriptor_pool_create_info, NULL, &descriptor_pool);
        if (result != VK_SUCCESS) fatal("Failed to create descriptor pool");
    }

    VkDescriptorSet *descriptor_sets = xmalloc(frame_count * sizeof(descriptor_sets[0]));
    {
        VkDescriptorSetAllocateInfo descriptor_set_allocate_info = {};
        descriptor_set_allocate_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        descriptor_set_allocate_info.descriptorPool = descriptor_pool;
        descriptor_set_allocate_info.descriptorSetCount = 1;
        descriptor_set_allocate_info.pSetLayouts = &descriptor_set_layout;

        for (u32 i = 0; i < frame_count; i++)
        {
            result = vkAllocateDescriptorSets(ctx.vk_device, &descriptor_set_allocate_info, &descriptor_sets[i]);
            if (result != VK_SUCCESS) fatal("Failed to allocate descriptor set");

            const Vk_BufferBundle *buffer_bundle = &ctx.global_ubo_3d.buffer_bundles[i];
            VkDescriptorBufferInfo descriptor_buffer_info = {};
            descriptor_buffer_info.buffer = buffer_bundle->buffer;
            descriptor_buffer_info.offset = 0;
            descriptor_buffer_info.range = buffer_bundle->size;

            VkWriteDescriptorSet uniform_buffer_write_descriptor_set = {};
            uniform_buffer_write_descriptor_set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            uniform_buffer_write_descriptor_set.dstSet = descriptor_sets[i];
            uniform_buffer_write_descriptor_set.dstBinding = 0;
            uniform_buffer_write_descriptor_set.dstArrayElement = 0;
            uniform_buffer_write_descriptor_set.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            uniform_buffer_write_descriptor_set.descriptorCount = 1;
            uniform_buffer_write_descriptor_set.pBufferInfo = &descriptor_buffer_info;

            vkUpdateDescriptorSets(ctx.vk_device, 1, &uniform_buffer_write_descriptor_set, 0, NULL);

            VkDescriptorImageInfo texture_sampler_descriptor_image_info = {};
            texture_sampler_descriptor_image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            texture_sampler_descriptor_image_info.imageView = ctx.ducks_texture.image_view;
            texture_sampler_descriptor_image_info.sampler = ctx.ducks_texture.sampler;

            VkWriteDescriptorSet texture_sampler_write_descriptor_set = {};
            texture_sampler_write_descriptor_set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            texture_sampler_write_descriptor_set.dstSet = descriptor_sets[i];
            texture_sampler_write_descriptor_set.dstBinding = 1;
            texture_sampler_write_descriptor_set.dstArrayElement = 0;
            texture_sampler_write_descriptor_set.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            texture_sampler_write_descriptor_set.descriptorCount = 1;
            texture_sampler_write_descriptor_set.pImageInfo = &texture_sampler_descriptor_image_info;

            vkUpdateDescriptorSets(ctx.vk_device, 1, &texture_sampler_write_descriptor_set, 0, NULL);
        }
    }

    pipeline_bundle.descriptor_pool = descriptor_pool;
    pipeline_bundle.descriptor_sets = descriptor_sets;
    pipeline_bundle.descriptor_set_count = frame_count;

    VkPipeline pipeline;
    {
        VkShaderModule vert_shader_module = _vk_create_shader_module(vert_shader_path);
        VkShaderModule frag_shader_module = _vk_create_shader_module(frag_shader_path);

        VkPipelineShaderStageCreateInfo shader_stages[2] = {};
        shader_stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shader_stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        shader_stages[0].module = vert_shader_module;
        shader_stages[0].pName = "main";
        shader_stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shader_stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        shader_stages[1].module = frag_shader_module;
        shader_stages[1].pName = "main";

        VkVertexInputBindingDescription vertex_input_binding_description = {};
        vertex_input_binding_description.binding = 0;
        vertex_input_binding_description.stride = sizeof(Vertex3D);
        vertex_input_binding_description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        int vert_attrib_count = 4;
        VkVertexInputAttributeDescription *vertex_input_attribute_descriptions = xmalloc(vert_attrib_count * sizeof(vertex_input_attribute_descriptions[0]));
        vertex_input_attribute_descriptions[0] = (VkVertexInputAttributeDescription){
            .location = 0,
            .binding = 0,
            .format = VK_FORMAT_R32G32B32_SFLOAT,
            .offset = offsetof(Vertex3D, pos)
        };
        vertex_input_attribute_descriptions[1] = (VkVertexInputAttributeDescription){
            .location = 1,
            .binding = 0,
            .format = VK_FORMAT_R32G32B32_SFLOAT,
            .offset = offsetof(Vertex3D, normal)
        };
        vertex_input_attribute_descriptions[2] = (VkVertexInputAttributeDescription){
            .location = 2,
            .binding = 0,
            .format = VK_FORMAT_R32G32_SFLOAT,
            .offset = offsetof(Vertex3D, tex_coord)
        };
        vertex_input_attribute_descriptions[3] = (VkVertexInputAttributeDescription){
            .location = 3,
            .binding = 0,
            .format = VK_FORMAT_R32G32B32A32_SFLOAT,
            .offset = offsetof(Vertex3D, color)
        };

        VkPipelineVertexInputStateCreateInfo vertex_input_state = {};
        vertex_input_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertex_input_state.vertexBindingDescriptionCount = 1;
        vertex_input_state.pVertexBindingDescriptions = &vertex_input_binding_description;
        vertex_input_state.vertexAttributeDescriptionCount = vert_attrib_count;
        vertex_input_state.pVertexAttributeDescriptions = vertex_input_attribute_descriptions;

        VkPipelineInputAssemblyStateCreateInfo input_assembly_state = {};
        input_assembly_state.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        input_assembly_state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        VkViewport viewport = {0, 0, (float)ctx.vk_swapchain_bundle.extent.width, (float)ctx.vk_swapchain_bundle.extent.height, 0.0f, 1.0f};
        VkRect2D scissor = {{0, 0}, {ctx.vk_swapchain_bundle.extent.width, ctx.vk_swapchain_bundle.extent.height}};
        VkPipelineViewportStateCreateInfo viewport_state = {};
        viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewport_state.viewportCount = 1;
        viewport_state.pViewports = &viewport;
        viewport_state.scissorCount = 1;
        viewport_state.pScissors = &scissor;

        VkPipelineRasterizationStateCreateInfo rasterization_state = {};
        rasterization_state.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterization_state.polygonMode = VK_POLYGON_MODE_FILL;
        rasterization_state.lineWidth = 1.0f;
        rasterization_state.cullMode = VK_CULL_MODE_NONE;
        rasterization_state.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

        VkPipelineMultisampleStateCreateInfo multisample_state = {};
        multisample_state.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisample_state.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineColorBlendAttachmentState color_blend_attachment = {};
        color_blend_attachment.colorWriteMask = (VK_COLOR_COMPONENT_R_BIT |
                                                                VK_COLOR_COMPONENT_G_BIT |
                                                                VK_COLOR_COMPONENT_B_BIT |
                                                                VK_COLOR_COMPONENT_A_BIT);
        color_blend_attachment.blendEnable = VK_FALSE;

        VkPipelineColorBlendStateCreateInfo color_blend_state = {};
        color_blend_state.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        color_blend_state.attachmentCount = 1;
        color_blend_state.pAttachments = &color_blend_attachment;

        VkPipelineDepthStencilStateCreateInfo depth_stencil_state = {};
        depth_stencil_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depth_stencil_state.depthTestEnable = VK_TRUE;
        depth_stencil_state.depthWriteEnable = VK_TRUE;
        depth_stencil_state.depthCompareOp = VK_COMPARE_OP_LESS;
        depth_stencil_state.depthBoundsTestEnable = VK_FALSE;
        depth_stencil_state.stencilTestEnable = VK_FALSE;

        VkGraphicsPipelineCreateInfo create_info = {};
        create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        create_info.stageCount = array_count(shader_stages);
        create_info.pStages = shader_stages;
        create_info.pVertexInputState = &vertex_input_state;
        create_info.pInputAssemblyState = &input_assembly_state;
        create_info.pViewportState = &viewport_state;
        create_info.pRasterizationState = &rasterization_state;
        create_info.pMultisampleState = &multisample_state;
        create_info.pColorBlendState = &color_blend_state;
        create_info.pDepthStencilState = &depth_stencil_state;
        create_info.layout = pipeline_layout;
        create_info.renderPass = ctx.vk_render_target_group.render_pass;
        create_info.subpass = 0;
        
        result = vkCreateGraphicsPipelines(ctx.vk_device, VK_NULL_HANDLE, 1, &create_info, NULL, &pipeline);
        if (result != VK_SUCCESS) fatal("Failed to create graphics pipeline");

        free(vertex_input_attribute_descriptions);

        vkDestroyShaderModule(ctx.vk_device, vert_shader_module, NULL);
        vkDestroyShaderModule(ctx.vk_device, frag_shader_module, NULL);
    }

    pipeline_bundle.pipeline = pipeline;

    return pipeline_bundle;
}

// --------------------------------

void _vk_destroy_swapchain_bundle(Vk_SwapchainBundle *bundle)
{
    for (u32 i = 0; i < bundle->image_count; i++)
    {
        vkDestroySemaphore(ctx.vk_device, bundle->submit_semaphores[i], NULL);
    }
    free(bundle->submit_semaphores);
    vkDestroySwapchainKHR(ctx.vk_device, bundle->swapchain, NULL);
    *bundle = (Vk_SwapchainBundle){};
}

void _vk_destroy_depth_image_bundle(Vk_DepthImageBundle *bundle)
{
    for (u32 i = 0; i < bundle->image_count; i++)
    {
        vkDestroyImage(ctx.vk_device, bundle->images[i], NULL);
        vkFreeMemory(ctx.vk_device, bundle->memory_list[i], NULL);
    }
    free(bundle->images);
    free(bundle->memory_list);
    *bundle = (Vk_DepthImageBundle){};
}

void _vk_destroy_render_target_group(Vk_RenderTargetGroup *group)
{
    for (u32 i = 0; i < group->render_target_count; i++)
    {
        vkDestroyImageView(ctx.vk_device, group->render_targets[i].color_view, NULL);
        vkDestroyImageView(ctx.vk_device, group->render_targets[i].depth_view, NULL);
        vkDestroyFramebuffer(ctx.vk_device, group->render_targets[i].framebuffer, NULL);
    }
    vkDestroyRenderPass(ctx.vk_device, group->render_pass, NULL);
    *group = (Vk_RenderTargetGroup){};
}

void _vk_destroy_command_pool(VkCommandPool *command_pool)
{
    vkDestroyCommandPool(ctx.vk_device, *command_pool, NULL);
    *command_pool = VK_NULL_HANDLE;
}

void _vk_destroy_frame_list(Vk_FrameList *list)
{
    for (u32 i = 0; i < list->count; i++)
    {
        vkDestroySemaphore(ctx.vk_device, list->frames[i].acquire_semaphore, NULL);
        vkDestroyFence(ctx.vk_device, list->frames[i].in_flight_fence, NULL);
    }

    free(list->frames);

    *list = (Vk_FrameList){};
}

void _vk_destroy_buffer_bundle(Vk_BufferBundle *bundle)
{
    vkUnmapMemory(ctx.vk_device, bundle->memory);
    vkFreeMemory(ctx.vk_device, bundle->memory, NULL);
    vkDestroyBuffer(ctx.vk_device, bundle->buffer, NULL);
    *bundle = (Vk_BufferBundle){};
}

void _vk_destroy_buffer_bundle_list(Vk_BufferBundleList *list)
{
    for (u32 i = 0; i < list->count; i++)
    {
        _vk_destroy_buffer_bundle(&list->buffer_bundles[i]);
    }
    free(list->buffer_bundles);
    *list = (Vk_BufferBundleList){};
}

void _vk_destroy_texture_bundle(Vk_TextureBundle *bundle)
{
    vkDestroyImage(ctx.vk_device, bundle->image, NULL);
    vkFreeMemory(ctx.vk_device, bundle->memory, NULL);
    vkDestroyImageView(ctx.vk_device, bundle->image_view, NULL);
    vkDestroySampler(ctx.vk_device, bundle->sampler, NULL);
}

void _vk_destroy_pipeline_bundle(Vk_PipelineBundle *bundle)
{
    _vk_destroy_buffer_bundle(&bundle->vertex_buffer_bundle);

    if (bundle->index_buffer_bundle.buffer != VK_NULL_HANDLE)
    {
        _vk_destroy_buffer_bundle(&bundle->index_buffer_bundle);
    }

    // VkResult result = vkFreeDescriptorSets(ctx.vk_device, bundle->descriptor_pool, bundle->descriptor_set_count, bundle->descriptor_sets);
    // if (result != VK_SUCCESS) fatal("Failed to free descriptor sets");

    free(bundle->descriptor_sets);

    vkDestroyDescriptorPool(ctx.vk_device, bundle->descriptor_pool, NULL);

    vkDestroyDescriptorSetLayout(ctx.vk_device, bundle->descriptor_set_layout, NULL);

    vkDestroyPipelineLayout(ctx.vk_device, bundle->pipeline_layout, NULL);

    vkDestroyPipeline(ctx.vk_device, bundle->pipeline, NULL);
    *bundle = (Vk_PipelineBundle){};
}

// --------------------------------

void e2r_init(int width, int height, const char *name)
{
    ctx.glfw_window = _glfw_create_window(width, height, name);
    ctx.vk_instance = _vk_create_instance();
    ctx.vk_surface = _vk_create_surface();
    ctx.vk_physical_device = _vk_find_physical_device();
    ctx.vk_queue_family_index = _vk_get_queue_family_index();
    ctx.vk_device = _vk_create_device();
    ctx.vk_queue = _vk_get_queue();

    ctx.vk_swapchain_bundle = _vk_create_swapchain_bundle();
    ctx.vk_depth_image_bundle = _vk_create_depth_image_bundle();
    ctx.vk_render_target_group = _vk_create_render_target_group();
    ctx.vk_command_pool = _vk_create_command_pool();

    ctx.vk_frame_list = _vk_create_frame_list();

    ctx.global_ubo_2d = _vk_create_buffer_bundle_list(sizeof(UBOLayoutGlobal2D), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
    ctx.global_ubo_3d = _vk_create_buffer_bundle_list(sizeof(UBOLayoutGlobal3D), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

    ctx.ducks_texture = _vk_load_texture("res/DUCKS.png");

    ctx.vk_tri_pipeline_bundle = _vk_create_pipeline_bundle_tri();
    ctx.vk_cubes_pipeline_bundle = _vk_create_pipeline_bundle_cubes();

    ctx.camera = e2r_camera_set_from_pos_target(V3(0.0f, 0.0f, 5.0f), V3(0.0f, 0.0f, 0.0f));
}

void e2r_destroy()
{
    vkDeviceWaitIdle(ctx.vk_device);

    _vk_destroy_pipeline_bundle(&ctx.vk_cubes_pipeline_bundle);
    _vk_destroy_pipeline_bundle(&ctx.vk_tri_pipeline_bundle);

    _vk_destroy_texture_bundle(&ctx.ducks_texture);

    _vk_destroy_buffer_bundle_list(&ctx.global_ubo_2d);
    _vk_destroy_buffer_bundle_list(&ctx.global_ubo_3d);

    _vk_destroy_frame_list(&ctx.vk_frame_list);

    _vk_destroy_command_pool(&ctx.vk_command_pool);
    _vk_destroy_render_target_group(&ctx.vk_render_target_group);
    _vk_destroy_depth_image_bundle(&ctx.vk_depth_image_bundle);
    _vk_destroy_swapchain_bundle(&ctx.vk_swapchain_bundle);

    vkDestroyDevice(ctx.vk_device, NULL);
    vkDestroySurfaceKHR(ctx.vk_instance, ctx.vk_surface, NULL);
    vkDestroyInstance(ctx.vk_instance, NULL);
    glfwDestroyWindow(ctx.glfw_window);
    glfwTerminate();
    ctx = (E2R_Ctx){};
}

bool e2r_is_running()
{
    return !glfwWindowShouldClose(ctx.glfw_window);
}

void e2r_start_frame()
{
    const Vk_Frame *frame = &ctx.vk_frame_list.frames[ctx.current_vk_frame];
    vkWaitForFences(ctx.vk_device, 1, &frame->in_flight_fence, true, UINT64_MAX);
    vkResetFences(ctx.vk_device, 1, &frame->in_flight_fence);

    glfwPollEvents();
}

void e2r_end_frame()
{
    ctx.current_vk_frame = (ctx.current_vk_frame + 1) % FRAMES_IN_FLIGHT;
}

void e2r_draw()
{
    const f32 delta = 1 / 120.0f;

    // Update camera based on mouse
    {
        static f64 last_mouse_x, last_mouse_y;
        static f64 mouse_dx_smoothed, mouse_dy_smoothed;
        static bool first_mouse = true;

        f64 mouse_x, mouse_y;
        glfwGetCursorPos(ctx.glfw_window, &mouse_x, &mouse_y);

        if (first_mouse)
        {
            last_mouse_x = mouse_x;
            last_mouse_y = mouse_y;
            first_mouse = false;
        }

        f64 mouse_dx = mouse_x - last_mouse_x;
        f64 mouse_dy = mouse_y - last_mouse_y;
        last_mouse_x = mouse_x;
        last_mouse_y = mouse_y;

        const f64 factor = 0.3;
        mouse_dx_smoothed = factor * mouse_dx_smoothed + (1.0 - factor) * mouse_dx;
        mouse_dy_smoothed = factor * mouse_dy_smoothed + (1.0 - factor) * mouse_dy;

        f32 mouse_sens = 0.2f;
        ctx.camera.pitch_deg -= mouse_sens * mouse_dy_smoothed;
        ctx.camera.yaw_deg += mouse_sens * mouse_dx_smoothed;
        if (ctx.camera.pitch_deg > 89.9f) ctx.camera.pitch_deg = 89.9f;
        else if (ctx.camera.pitch_deg < -89.9f) ctx.camera.pitch_deg = -89.9f;
    }

    // Update camera based on keyboard
    {
        f32 speed = 3.0f;
        v3 dir = e2r_camera_get_dir(&ctx.camera);
        v3 right = e2r_camera_get_right(&ctx.camera);
        v3 up = e2r_camera_get_up(&ctx.camera);
        if (glfwGetKey(ctx.glfw_window, GLFW_KEY_W) == GLFW_PRESS) ctx.camera.pos = v3_add(ctx.camera.pos, v3_scale(dir, speed * delta));
        if (glfwGetKey(ctx.glfw_window, GLFW_KEY_S) == GLFW_PRESS) ctx.camera.pos = v3_add(ctx.camera.pos, v3_scale(dir, -speed * delta));
        if (glfwGetKey(ctx.glfw_window, GLFW_KEY_A) == GLFW_PRESS) ctx.camera.pos = v3_add(ctx.camera.pos, v3_scale(right, speed * delta));
        if (glfwGetKey(ctx.glfw_window, GLFW_KEY_D) == GLFW_PRESS) ctx.camera.pos = v3_add(ctx.camera.pos, v3_scale(right, -speed * delta));
        if (glfwGetKey(ctx.glfw_window, GLFW_KEY_SPACE) == GLFW_PRESS) ctx.camera.pos = v3_add(ctx.camera.pos, v3_scale(up, speed * delta));
        if (glfwGetKey(ctx.glfw_window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) ctx.camera.pos = v3_add(ctx.camera.pos, v3_scale(up, -speed * delta));
    }

    const Vk_Frame *frame = &ctx.vk_frame_list.frames[ctx.current_vk_frame];

    // Tri pipeline data
    {
        const Vertex2D verts[] =
        {
            {V3(100.0f, 200.0f, 0.0f), V4(1.0f, 0.0f, 0.0f, 1.0f)},
            {V3(200.0f, 200.0f, 0.0f), V4(0.0f, 1.0f, 0.0f, 1.0f)},
            {V3(150.0f, 100.0f, 0.0f), V4(0.0f, 0.0f, 1.0f, 1.0f)},
        };

        memcpy(ctx.vk_tri_pipeline_bundle.vertex_buffer_bundle.data_ptr, verts, sizeof(verts));
    }

    // Cubes pipeline data
    u32 index_count;
    {
        const Vertex3D verts[] =
        {
            // a
            { V3(-0.5f, -0.5f, -0.5f), V3( 0.0f, -1.0f,  0.0f), V2(0.0f, 0.0f), V4(0.9f, 0.9f, 0.8f, 1.0f) }, // 0
            { V3( 0.5f, -0.5f, -0.5f), V3( 0.0f, -1.0f,  0.0f), V2(1.0f, 0.0f), V4(0.9f, 0.9f, 0.8f, 1.0f) }, // 1
            { V3( 0.5f, -0.5f,  0.5f), V3( 0.0f, -1.0f,  0.0f), V2(1.0f, 1.0f), V4(0.9f, 0.9f, 0.8f, 1.0f) }, // 2
            { V3(-0.5f, -0.5f,  0.5f), V3( 0.0f, -1.0f,  0.0f), V2(0.0f, 1.0f), V4(0.9f, 0.9f, 0.8f, 1.0f) }, // 3
            // b
            { V3(-0.5f,  0.5f,  0.5f), V3( 0.0f,  1.0f,  0.0f), V2(0.0f, 0.0f), V4(0.9f, 0.9f, 0.8f, 1.0f) }, // 4
            { V3( 0.5f,  0.5f,  0.5f), V3( 0.0f,  1.0f,  0.0f), V2(1.0f, 0.0f), V4(0.9f, 0.9f, 0.8f, 1.0f) }, // 5
            { V3( 0.5f,  0.5f, -0.5f), V3( 0.0f,  1.0f,  0.0f), V2(1.0f, 1.0f), V4(0.9f, 0.9f, 0.8f, 1.0f) }, // 6
            { V3(-0.5f,  0.5f, -0.5f), V3( 0.0f,  1.0f,  0.0f), V2(0.0f, 1.0f), V4(0.9f, 0.9f, 0.8f, 1.0f) }, // 7
            // c
            { V3(-0.5f, -0.5f,  0.5f), V3( 0.0f,  0.0f,  1.0f), V2(0.0f, 0.0f), V4(0.9f, 0.9f, 0.8f, 1.0f) }, // 8
            { V3( 0.5f, -0.5f,  0.5f), V3( 0.0f,  0.0f,  1.0f), V2(1.0f, 0.0f), V4(0.9f, 0.9f, 0.8f, 1.0f) }, // 9
            { V3( 0.5f,  0.5f,  0.5f), V3( 0.0f,  0.0f,  1.0f), V2(1.0f, 1.0f), V4(0.9f, 0.9f, 0.8f, 1.0f) }, // 10
            { V3(-0.5f,  0.5f,  0.5f), V3( 0.0f,  0.0f,  1.0f), V2(0.0f, 1.0f), V4(0.9f, 0.9f, 0.8f, 1.0f) }, // 11
            // d
            { V3( 0.5f, -0.5f,  0.5f), V3( 1.0f,  0.0f,  0.0f), V2(0.0f, 0.0f), V4(0.9f, 0.9f, 0.8f, 1.0f) }, // 12
            { V3( 0.5f, -0.5f, -0.5f), V3( 1.0f,  0.0f,  0.0f), V2(1.0f, 0.0f), V4(0.9f, 0.9f, 0.8f, 1.0f) }, // 13
            { V3( 0.5f,  0.5f, -0.5f), V3( 1.0f,  0.0f,  0.0f), V2(1.0f, 1.0f), V4(0.9f, 0.9f, 0.8f, 1.0f) }, // 14
            { V3( 0.5f,  0.5f,  0.5f), V3( 1.0f,  0.0f,  0.0f), V2(0.0f, 1.0f), V4(0.9f, 0.9f, 0.8f, 1.0f) }, // 15
            // e
            { V3( 0.5f, -0.5f, -0.5f), V3( 0.0f,  0.0f, -1.0f), V2(0.0f, 0.0f), V4(0.9f, 0.9f, 0.8f, 1.0f) }, // 16
            { V3(-0.5f, -0.5f, -0.5f), V3( 0.0f,  0.0f, -1.0f), V2(1.0f, 0.0f), V4(0.9f, 0.9f, 0.8f, 1.0f) }, // 17
            { V3(-0.5f,  0.5f, -0.5f), V3( 0.0f,  0.0f, -1.0f), V2(1.0f, 1.0f), V4(0.9f, 0.9f, 0.8f, 1.0f) }, // 18
            { V3( 0.5f,  0.5f, -0.5f), V3( 0.0f,  0.0f, -1.0f), V2(0.0f, 1.0f), V4(0.9f, 0.9f, 0.8f, 1.0f) }, // 19
            // f
            { V3(-0.5f, -0.5f, -0.5f), V3(-1.0f,  0.0f,  0.0f), V2(0.0f, 0.0f), V4(0.9f, 0.9f, 0.8f, 1.0f) }, // 20
            { V3(-0.5f, -0.5f,  0.5f), V3(-1.0f,  0.0f,  0.0f), V2(1.0f, 0.0f), V4(0.9f, 0.9f, 0.8f, 1.0f) }, // 21
            { V3(-0.5f,  0.5f,  0.5f), V3(-1.0f,  0.0f,  0.0f), V2(1.0f, 1.0f), V4(0.9f, 0.9f, 0.8f, 1.0f) }, // 22
            { V3(-0.5f,  0.5f, -0.5f), V3(-1.0f,  0.0f,  0.0f), V2(0.0f, 1.0f), V4(0.9f, 0.9f, 0.8f, 1.0f) }, // 23
        };

        memcpy(ctx.vk_cubes_pipeline_bundle.vertex_buffer_bundle.data_ptr, verts, sizeof(verts));

        const VertIndex indices[] =
        {
            0,  1,  2,  0,  2,  3, // a
            4,  5,  6,  4,  6,  7, // b
            8,  9, 10,  8, 10, 11, // c
            12, 13, 14, 12, 14, 15, // d
            16, 17, 18, 16, 18, 19, // e
            20, 21, 22, 20, 22, 23, // f
        };
        index_count = array_count(indices);

        memcpy(ctx.vk_cubes_pipeline_bundle.index_buffer_bundle.data_ptr, indices, sizeof(indices));
    }

    // Global 2D and 3D uniforms
    v2 window_dim = _glfw_get_window_size();

    m4 ortho_proj = m4_proj_ortho(0.0f, window_dim.x, 0.0f, window_dim.y, -1.0f, 1.0f);

    {
        UBOLayoutGlobal2D ubo_data =
        {
            .proj = ortho_proj
        };
        memcpy(ctx.global_ubo_2d.buffer_bundles[ctx.current_vk_frame].data_ptr, &ubo_data, sizeof(ubo_data));
    }

    m4 camera_view = e2r_camera_get_view(&ctx.camera);
    m4 perspective_proj = m4_proj_perspective(deg_to_rad(60), window_dim.x / window_dim.y, 0.1f, 100.0f);
    m4 view_proj = m4_mul(perspective_proj, camera_view);

    {
        UBOLayoutGlobal3D ubo_data =
        {
            .model = m4_identity(),
            .view_proj = view_proj
        };

        memcpy(ctx.global_ubo_3d.buffer_bundles[ctx.current_vk_frame].data_ptr, &ubo_data, sizeof(ubo_data));
    }

    u32 next_image_index;
    VkResult result = vkAcquireNextImageKHR(ctx.vk_device, ctx.vk_swapchain_bundle.swapchain, UINT64_MAX, frame->acquire_semaphore, VK_NULL_HANDLE, &next_image_index);
    if (result == VK_SUBOPTIMAL_KHR || result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        // recreate_everything = true;
        // continue;
    }
    else if (result != VK_SUCCESS) fatal("Failed to acquire next image");

    result = vkResetCommandBuffer(frame->command_buffer, 0);
    if (result != VK_SUCCESS) fatal("Failed to reset command buffer");

    VkCommandBufferBeginInfo command_buffer_begin_info = {};
    command_buffer_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    result = vkBeginCommandBuffer(frame->command_buffer, &command_buffer_begin_info);
    if (result != VK_SUCCESS) fatal("Failed to begin command buffer");

    VkClearValue clear_values[2] = {};
    clear_values[0].color = (VkClearColorValue){{1.0f, 1.0f, 0.0f, 1.0f}};
    clear_values[1].depthStencil = (VkClearDepthStencilValue){1.0f, 0};
    VkRect2D render_area = {};
    render_area.offset = (VkOffset2D){0, 0};
    render_area.extent = ctx.vk_swapchain_bundle.extent;
    VkRenderPassBeginInfo render_pass_begin_info = {};
    render_pass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_pass_begin_info.renderPass = ctx.vk_render_target_group.render_pass;
    render_pass_begin_info.framebuffer = ctx.vk_render_target_group.render_targets[next_image_index].framebuffer;
    render_pass_begin_info.renderArea = render_area;
    render_pass_begin_info.clearValueCount = array_count(clear_values);
    render_pass_begin_info.pClearValues = clear_values;
    vkCmdBeginRenderPass(frame->command_buffer, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);

    {
        vkCmdBindPipeline(frame->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, ctx.vk_cubes_pipeline_bundle.pipeline);

        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(frame->command_buffer, 0, 1, &ctx.vk_cubes_pipeline_bundle.vertex_buffer_bundle.buffer, offsets);

        vkCmdBindIndexBuffer(frame->command_buffer, ctx.vk_cubes_pipeline_bundle.index_buffer_bundle.buffer, 0, VERT_INDEX_TYPE);

        vkCmdBindDescriptorSets(
            frame->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
            ctx.vk_cubes_pipeline_bundle.pipeline_layout,
            0,
            1, &ctx.vk_cubes_pipeline_bundle.descriptor_sets[ctx.current_vk_frame],
            0, NULL
        );

        vkCmdDrawIndexed(frame->command_buffer, index_count, 1, 0, 0, 0);
    }

    {
        vkCmdBindPipeline(frame->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, ctx.vk_tri_pipeline_bundle.pipeline);

        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(frame->command_buffer, 0, 1, &ctx.vk_tri_pipeline_bundle.vertex_buffer_bundle.buffer, offsets);

        vkCmdBindDescriptorSets(
            frame->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
            ctx.vk_tri_pipeline_bundle.pipeline_layout,
            0,
            1, &ctx.vk_tri_pipeline_bundle.descriptor_sets[ctx.current_vk_frame],
            0, NULL
        );

        vkCmdDraw(frame->command_buffer, 3, 1, 0, 0);
    }

    vkCmdEndRenderPass(frame->command_buffer);
    result = vkEndCommandBuffer(frame->command_buffer);
    if (result != VK_SUCCESS) fatal("Failed to end command buffer");

    // Submit command buffer
    VkPipelineStageFlags wait_destination_stage_mask[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    VkSubmitInfo submit_info = {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = &frame->acquire_semaphore;
    submit_info.pWaitDstStageMask = wait_destination_stage_mask;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &frame->command_buffer;
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = &ctx.vk_swapchain_bundle.submit_semaphores[next_image_index];

    result = vkQueueSubmit(ctx.vk_queue, 1, &submit_info, frame->in_flight_fence);
    if (result != VK_SUCCESS) fatal("Failed to submit command buffer to queue");

    // Present
    VkPresentInfoKHR present_info = {};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = &ctx.vk_swapchain_bundle.submit_semaphores[next_image_index];
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &ctx.vk_swapchain_bundle.swapchain;
    present_info.pImageIndices = &next_image_index;
    result = vkQueuePresentKHR(ctx.vk_queue, &present_info);
    if (result == VK_SUBOPTIMAL_KHR || result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        // recreate_everything = true;
        // continue;
    }
    else if (result != VK_SUCCESS) fatal("Error when presenting");
}
