#include "e2r_core.h"

#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

#include "types.h"
#include "util.h"

typedef struct E2R_Ctx
{
    GLFWwindow *window;
    VkInstance vk_instance;
} E2R_Ctx;

globvar E2R_Ctx ctx;

VkInstance _create_vk_instance()
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

GLFWwindow *create_window(int width, int height, const char *window_name)
{
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    GLFWwindow *window = glfwCreateWindow(width, height, window_name, NULL, NULL);
    return window;
}

// --------------------------------

void e2r_init(int width, int height, const char *name)
{
    ctx.window = create_window(width, height, name);
    ctx.vk_instance = _create_vk_instance();
}

void e2r_destroy()
{
    (void)vkDestroyInstance(ctx.vk_instance, NULL);
    glfwDestroyWindow(ctx.window);
    glfwTerminate();
    ctx = (E2R_Ctx){};
}

bool e2r_is_running()
{
    return !glfwWindowShouldClose(ctx.window);
}

void e2r_start_frame()
{
    glfwPollEvents();
}
