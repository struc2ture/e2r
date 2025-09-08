#include "e2r_core.h"

#include <string.h>

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <vulkan/vulkan_core.h>

#include "common/lin_math.h"
#include "common/types.h"
#include "common/util.h"

#define FRAMES_IN_FLIGHT 2
#define MAX_VERTEX_COUNT 1024

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
    // VkImageView depth_view;
    VkFramebuffer framebuffer;

} Vk_RenderTarget;

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
    VkDeviceSize max_size;

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

} Vk_PipelineBundle;

// ------------------------------------

typedef struct Vertex
{
    v3 pos;
    v4 color;

} Vertex;

typedef struct UBO_Layout_Global_2D
{
    m4 proj;

} UBO_Layout_Global_2D;

// typedef struct UBO_Layout_Global_3D
// {
//     m4 model;
//     m4 view_proj;

// } UBO_Layout_Global_3D;

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

    Vk_RenderTargetGroup vk_render_target_group;
    
    VkCommandPool vk_command_pool;

    Vk_FrameList vk_frame_list;
    u32 current_vk_frame;

    Vk_BufferBundleList global_ubo_2d;
    // Vk_BufferBundleList global_ubo_3d;

    Vk_PipelineBundle vk_tri_pipeline_bundle;

} E2R_Ctx;

globvar E2R_Ctx ctx;

GLFWwindow *_glfw_create_window(int width, int height, const char *window_name)
{
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    GLFWwindow *window = glfwCreateWindow(width, height, window_name, NULL, NULL);
    return window;
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

//Vk_DepthImageBundle _vk_create_depth_images() ???

Vk_RenderTargetGroup _vk_create_render_target_group()
{
    Vk_RenderTargetGroup render_target_group =
    {
        .color_format = ctx.vk_swapchain_bundle.format.format,
        // .depth_format = VK_FORMAT_D32_SFLOAT,
        .render_target_count = ctx.vk_swapchain_bundle.image_count
    };

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

    // VkAttachmentDescription depth_attachment_description = {};
    // depth_attachment_description.format = render_target_group.depth_format;
    // depth_attachment_description.samples = VK_SAMPLE_COUNT_1_BIT;
    // depth_attachment_description.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    // depth_attachment_description.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    // depth_attachment_description.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    // depth_attachment_description.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    // depth_attachment_description.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    // depth_attachment_description.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    // VkAttachmentReference depth_attachment_reference = {};
    // depth_attachment_reference.attachment = 1; // index in pAttachments
    // depth_attachment_reference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass_description = {};
    subpass_description.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass_description.colorAttachmentCount = 1;
    subpass_description.pColorAttachments = &color_attachment_reference;
    // subpass_description.pDepthStencilAttachment = &depth_attachment_reference;

    // VkAttachmentDescription render_pass_attachments[] = { color_attachment_description, depth_attachment_description };
    VkAttachmentDescription render_pass_attachments[] = { color_attachment_description };
    VkRenderPassCreateInfo render_pass_create_info = {};
    render_pass_create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_create_info.attachmentCount = array_count(render_pass_attachments);
    render_pass_create_info.pAttachments = render_pass_attachments;
    render_pass_create_info.subpassCount = 1;
    render_pass_create_info.pSubpasses = &subpass_description;

    VkRenderPass render_pass;
    VkResult result = vkCreateRenderPass(ctx.vk_device, &render_pass_create_info, NULL, &render_pass);
    if (result != VK_SUCCESS) fatal("Failed to create render pass");

    render_target_group.render_pass = render_pass;

    Vk_RenderTarget *render_targets = xmalloc(render_target_group.render_target_count * sizeof(render_targets[0]));

    for (u32 i = 0; i < render_target_group.render_target_count; i++)
    {
        VkImageViewCreateInfo image_view_create_info = {};
        image_view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        image_view_create_info.image = ctx.vk_swapchain_bundle.images[i];
        image_view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        image_view_create_info.format = ctx.vk_swapchain_bundle.format.format;
        image_view_create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        image_view_create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        image_view_create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        image_view_create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        image_view_create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        image_view_create_info.subresourceRange.baseMipLevel = 0;
        image_view_create_info.subresourceRange.levelCount = 1;
        image_view_create_info.subresourceRange.baseArrayLayer = 0;
        image_view_create_info.subresourceRange.layerCount = 1;

        VkResult result = vkCreateImageView(ctx.vk_device, &image_view_create_info, NULL, &render_targets[i].color_view);
        if (result != VK_SUCCESS) fatal("Failed to create image view");

        // VkImageView attachments[] = { temp_vulkan.image_views[i], temp_vulkan.depth_buffer_image_view };
        VkImageView attachments[] = { render_targets[i].color_view };

        VkFramebufferCreateInfo framebuffer_create_info = {};
        framebuffer_create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebuffer_create_info.renderPass = render_pass;
        framebuffer_create_info.attachmentCount = array_count(attachments);
        framebuffer_create_info.pAttachments = attachments;
        framebuffer_create_info.width = ctx.vk_swapchain_bundle.extent.width;
        framebuffer_create_info.height = ctx.vk_swapchain_bundle.extent.height;
        framebuffer_create_info.layers = 1;

        result = vkCreateFramebuffer(ctx.vk_device, &framebuffer_create_info, NULL, &render_targets[i].framebuffer);
        if (result != VK_SUCCESS) fatal("Failed to create framebuffer");
    }

    render_target_group.render_targets = render_targets;

    return render_target_group;
}

VkCommandPool _vk_create_command_pool()
{
    VkCommandPoolCreateInfo command_pool_create_info = {};
    command_pool_create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    command_pool_create_info.queueFamilyIndex = ctx.vk_queue_family_index;
    command_pool_create_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT; // allows resetting individual buffers

    VkCommandPool command_pool;
    VkResult result = vkCreateCommandPool(ctx.vk_device, &command_pool_create_info, NULL, &command_pool);
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

Vk_BufferBundle _vk_create_buffer_bundle(VkDeviceSize max_size, VkBufferUsageFlags usage)
{
    // Create uniform buffer
    VkBufferCreateInfo uniform_buffer_create_info = {};
    uniform_buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    uniform_buffer_create_info.size = max_size;
    uniform_buffer_create_info.usage = usage;
    uniform_buffer_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VkBuffer buffer;
    VkResult result = vkCreateBuffer(ctx.vk_device, &uniform_buffer_create_info, NULL, &buffer);
    if (result != VK_SUCCESS) fatal("Failed to create uniform buffer");

    // Allocate memory for uniform buffer
    VkMemoryRequirements memory_requirements;
    (void)vkGetBufferMemoryRequirements(ctx.vk_device, buffer, &memory_requirements);

    VkMemoryAllocateInfo memory_allocate_info = {};
    memory_allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memory_allocate_info.allocationSize = memory_requirements.size;
    memory_allocate_info.memoryTypeIndex = _vk_find_memory_type(
        ctx.vk_physical_device,
        memory_requirements.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );

    VkDeviceMemory device_memory;
    result = vkAllocateMemory(ctx.vk_device, &memory_allocate_info, NULL, &device_memory);
    if (result != VK_SUCCESS) fatal("Failed to allocate memory for uniform buffer");

    result = vkBindBufferMemory(ctx.vk_device, buffer, device_memory, 0);
    if (result != VK_SUCCESS) fatal("Failed to bind memory to uniform buffer");

    void *data;
    result = vkMapMemory(ctx.vk_device, device_memory, 0, max_size, 0, &data);
    if (result != VK_SUCCESS) fatal("Failed to map vertex buffer memory");

    return (Vk_BufferBundle){
        .buffer = buffer,
        .memory = device_memory,
        .data_ptr = data,
        .max_size = max_size
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
    descriptor_set_layout_binding_0.pImmutableSamplers = NULL;

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
    // pipeline_layout_create_info.pushConstantRangeCount = 1;
    // pipeline_layout_create_info.pPushConstantRanges = &mvp_push_constant_range;

    VkPipelineLayout pipeline_layout;
    result = vkCreatePipelineLayout(ctx.vk_device, &pipeline_layout_create_info, NULL, &pipeline_layout);
    if (result != VK_SUCCESS) fatal("Failed to create pipeline layout");

    pipeline_bundle.vertex_buffer_bundle = _vk_create_buffer_bundle(
        pipeline_bundle.max_vertex_count * sizeof(Vertex),
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
    );

    pipeline_bundle.descriptor_set_layout = descriptor_set_layout;
    pipeline_bundle.pipeline_layout = pipeline_layout;

    // Descriptor pool
    VkDescriptorPoolSize descriptor_pool_sizes[1] = {};
    descriptor_pool_sizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptor_pool_sizes[0].descriptorCount = frame_count;
    // descriptor_pool_sizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    // descriptor_pool_sizes[1].descriptorCount = 1;

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
        descriptor_buffer_info.range = buffer_bundle->max_size;

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
    vertex_input_binding_description.stride = sizeof(Vertex);
    vertex_input_binding_description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    int vert_attrib_count = 2;
    VkVertexInputAttributeDescription *vertex_input_attribute_descriptions = xmalloc(vert_attrib_count * sizeof(vertex_input_attribute_descriptions[0]));
    vertex_input_attribute_descriptions[0] = (VkVertexInputAttributeDescription){
        .location = 0,
        .binding = 0,
        .format = VK_FORMAT_R32G32B32_SFLOAT,
        .offset = offsetof(Vertex, pos)
    };
    vertex_input_attribute_descriptions[1] = (VkVertexInputAttributeDescription){
        .location = 1,
        .binding = 0,
        .format = VK_FORMAT_R32G32B32A32_SFLOAT,
        .offset = offsetof(Vertex, color)
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

    // VkPipelineDepthStencilStateCreateInfo pipeline_depth_stencil_state_create_info = {};
    // pipeline_depth_stencil_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    // pipeline_depth_stencil_state_create_info.depthTestEnable = VK_TRUE;
    // pipeline_depth_stencil_state_create_info.depthWriteEnable = VK_TRUE;
    // pipeline_depth_stencil_state_create_info.depthCompareOp = VK_COMPARE_OP_LESS;
    // pipeline_depth_stencil_state_create_info.depthBoundsTestEnable = VK_FALSE;
    // pipeline_depth_stencil_state_create_info.stencilTestEnable = VK_FALSE;

    // VkPushConstantRange mvp_push_constant_range = {};
    // mvp_push_constant_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    // mvp_push_constant_range.offset = 0;
    // mvp_push_constant_range.size = sizeof(m4);

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
    // graphics_pipeline_create_info.pDepthStencilState = &pipeline_depth_stencil_state_create_info;
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

void _vk_destroy_render_target_group(Vk_RenderTargetGroup *group)
{
    for (u32 i = 0; i < group->render_target_count; i++)
    {
        vkDestroyImageView(ctx.vk_device, group->render_targets[i].color_view, NULL);
        // vkDestroyImageView(ctx.vk_device, group->render_targets[i].depth_view)
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

void _vk_destroy_pipeline_bundle(Vk_PipelineBundle *bundle)
{
    _vk_destroy_buffer_bundle(&bundle->vertex_buffer_bundle);

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
    ctx.vk_render_target_group = _vk_create_render_target_group();
    ctx.vk_command_pool = _vk_create_command_pool();
    ctx.vk_frame_list = _vk_create_frame_list();

    ctx.global_ubo_2d = _vk_create_buffer_bundle_list(sizeof(UBO_Layout_Global_2D), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
    // ctx.global_ubo_3d = _vk_create_buffer_bundle_list(sizeof(UBO_Layout_Global_3D), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

    ctx.vk_tri_pipeline_bundle = _vk_create_pipeline_bundle_tri();
}

void e2r_destroy()
{
    vkDeviceWaitIdle(ctx.vk_device);

    _vk_destroy_pipeline_bundle(&ctx.vk_tri_pipeline_bundle);

    _vk_destroy_buffer_bundle_list(&ctx.global_ubo_2d);
    // _vk_destroy_buffer_bundle_list(&ctx.global_ubo_3d);

    _vk_destroy_frame_list(&ctx.vk_frame_list);

    _vk_destroy_command_pool(&ctx.vk_command_pool);

    _vk_destroy_render_target_group(&ctx.vk_render_target_group);
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
    const Vk_Frame *frame = &ctx.vk_frame_list.frames[ctx.current_vk_frame];

    const Vertex verts[] =
    {
        {V3(100.0f, 200.0f, 0.0f), V4(1.0f, 0.0f, 0.0f, 1.0f)},
        {V3(200.0f, 200.0f, 0.0f), V4(0.0f, 1.0f, 0.0f, 1.0f)},
        {V3(150.0f, 100.0f, 0.0f), V4(0.0f, 0.0f, 1.0f, 1.0f)},
    };

    memcpy(ctx.vk_tri_pipeline_bundle.vertex_buffer_bundle.data_ptr, verts, sizeof(verts));

    u32 next_image_index;
    VkResult result = vkAcquireNextImageKHR(ctx.vk_device, ctx.vk_swapchain_bundle.swapchain, UINT64_MAX, frame->acquire_semaphore, VK_NULL_HANDLE, &next_image_index);
    if (result == VK_SUBOPTIMAL_KHR || result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        // recreate_everything = true;
        // continue;
    }
    else if (result != VK_SUCCESS) fatal("Failed to acquire next image");

    UBO_Layout_Global_2D ubo_data =
    {
        .proj = m4_proj_ortho(
            0.0f, (f32) ctx.vk_swapchain_bundle.extent.width,
            0.0f, (f32) ctx.vk_swapchain_bundle.extent.height,
            -1.0f, 1.0f
        )
    };
    memcpy(ctx.global_ubo_2d.buffer_bundles[ctx.current_vk_frame].data_ptr, &ubo_data, sizeof(ubo_data));

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
