#include "e2r_core.h"

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <vulkan/vulkan_core.h>

#include "types.h"
#include "util.h"

typedef struct E2R_Ctx
{
    GLFWwindow *glfw_window;

    VkInstance vk_instance;
    VkSurfaceKHR vk_surface;
    VkPhysicalDevice vk_physical_device;
    u32 vk_queue_family_index;
    VkDevice vk_device;
    VkQueue vk_queue;

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
}

void e2r_destroy()
{
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
    glfwPollEvents();
}
