#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"
#include <stdio.h>          
#include <stdlib.h>         
#define GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "Telemetry.hpp"
#include <iostream>
#include <chrono>
#include <thread>
#include <mutex>
#include <iomanip>
#include <algorithm>

struct CarState {
    TelemetryPoint current_data;
    bool is_running = true;
    bool force_stop = false;
    std::mutex mtx;
};

void replay_engine(const std::vector<TelemetryPoint>& data, CarState& state) {
    auto start_time = std::chrono::steady_clock::now();
    for (const auto& pt : data) {
        auto target_time = start_time + std::chrono::milliseconds(static_cast<long long>(pt.time_ms));
        while (std::chrono::steady_clock::now() < target_time) {
            {
                std::lock_guard<std::mutex> lock(state.mtx);

                if (state.force_stop) {
                    state.is_running = false;
                    return; 
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        
        std::lock_guard<std::mutex> lock(state.mtx); 
        state.current_data = pt;
    }
    std::lock_guard<std::mutex> lock(state.mtx);
    state.is_running = false;
}

#ifdef IMGUI_IMPL_VULKAN_USE_VOLK
#define VOLK_IMPLEMENTATION
#include <volk.h>
#endif

static VkAllocationCallbacks* g_Allocator = nullptr;
static VkInstance               g_Instance = VK_NULL_HANDLE;
static VkPhysicalDevice         g_PhysicalDevice = VK_NULL_HANDLE;
static VkDevice                 g_Device = VK_NULL_HANDLE;
static uint32_t                 g_QueueFamily = (uint32_t)-1;
static VkQueue                  g_Queue = VK_NULL_HANDLE;
static VkPipelineCache          g_PipelineCache = VK_NULL_HANDLE;
static VkDescriptorPool         g_DescriptorPool = VK_NULL_HANDLE;

static ImGui_ImplVulkanH_Window g_MainWindowData;
static uint32_t                 g_MinImageCount = 2;
static bool                     g_SwapChainRebuild = false;

static void glfw_error_callback(int error, const char* description) {
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

static void check_vk_result(VkResult err) {
    if (err == VK_SUCCESS) return;
    fprintf(stderr, "[vulkan] Error: VkResult = %d\n", err);
    if (err < 0) abort();
}

static bool IsExtensionAvailable(const ImVector<VkExtensionProperties>& properties, const char* extension) {
    for (const VkExtensionProperties& p : properties)
        if (strcmp(p.extensionName, extension) == 0) return true;
    return false;
}

static void SetupVulkan(ImVector<const char*> instance_extensions) {
    VkResult err;
#ifdef IMGUI_IMPL_VULKAN_USE_VOLK
    volkInitialize();
#endif
    {
        VkInstanceCreateInfo create_info = {};
        create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;

        uint32_t properties_count;
        ImVector<VkExtensionProperties> properties;
        vkEnumerateInstanceExtensionProperties(nullptr, &properties_count, nullptr);
        properties.resize(properties_count);
        err = vkEnumerateInstanceExtensionProperties(nullptr, &properties_count, properties.Data);
        check_vk_result(err);

        if (IsExtensionAvailable(properties, VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME))
            instance_extensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);

        create_info.enabledExtensionCount = (uint32_t)instance_extensions.Size;
        create_info.ppEnabledExtensionNames = instance_extensions.Data;
        err = vkCreateInstance(&create_info, g_Allocator, &g_Instance);
        check_vk_result(err);
#ifdef IMGUI_IMPL_VULKAN_USE_VOLK
        volkLoadInstance(g_Instance);
#endif
    }
    g_PhysicalDevice = ImGui_ImplVulkanH_SelectPhysicalDevice(g_Instance);
    IM_ASSERT(g_PhysicalDevice != VK_NULL_HANDLE);
    g_QueueFamily = ImGui_ImplVulkanH_SelectQueueFamilyIndex(g_PhysicalDevice);
    IM_ASSERT(g_QueueFamily != (uint32_t)-1);
    {
        ImVector<const char*> device_extensions;
        device_extensions.push_back("VK_KHR_swapchain");

        const float queue_priority[] = { 1.0f };
        VkDeviceQueueCreateInfo queue_info[1] = {};
        queue_info[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_info[0].queueFamilyIndex = g_QueueFamily;
        queue_info[0].queueCount = 1;
        queue_info[0].pQueuePriorities = queue_priority;
        VkDeviceCreateInfo create_info = {};
        create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        create_info.queueCreateInfoCount = 1;
        create_info.pQueueCreateInfos = queue_info;
        create_info.enabledExtensionCount = (uint32_t)device_extensions.Size;
        create_info.ppEnabledExtensionNames = device_extensions.Data;
        err = vkCreateDevice(g_PhysicalDevice, &create_info, g_Allocator, &g_Device);
        check_vk_result(err);
        vkGetDeviceQueue(g_Device, g_QueueFamily, 0, &g_Queue);
    }
    {
        VkDescriptorPoolSize pool_sizes[] = {
            { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
            { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
        };
        VkDescriptorPoolCreateInfo pool_info = {};
        pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        pool_info.maxSets = 1000;
        pool_info.poolSizeCount = (uint32_t)IM_COUNTOF(pool_sizes);
        pool_info.pPoolSizes = pool_sizes;
        err = vkCreateDescriptorPool(g_Device, &pool_info, g_Allocator, &g_DescriptorPool);
        check_vk_result(err);
    }
}

static void SetupVulkanWindow(ImGui_ImplVulkanH_Window* wd, VkSurfaceKHR surface, int width, int height) {
    wd->Surface = surface;
    const VkFormat requestSurfaceImageFormat[] = { VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM };
    const VkColorSpaceKHR requestSurfaceColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
    wd->SurfaceFormat = ImGui_ImplVulkanH_SelectSurfaceFormat(g_PhysicalDevice, wd->Surface, requestSurfaceImageFormat, (size_t)IM_COUNTOF(requestSurfaceImageFormat), requestSurfaceColorSpace);
    VkPresentModeKHR present_modes[] = { VK_PRESENT_MODE_FIFO_KHR };
    wd->PresentMode = ImGui_ImplVulkanH_SelectPresentMode(g_PhysicalDevice, wd->Surface, &present_modes[0], IM_COUNTOF(present_modes));
    ImGui_ImplVulkanH_CreateOrResizeWindow(g_Instance, g_PhysicalDevice, g_Device, wd, g_QueueFamily, g_Allocator, width, height, g_MinImageCount, 0);
}

static void CleanupVulkan() {
    vkDestroyDescriptorPool(g_Device, g_DescriptorPool, g_Allocator);
    vkDestroyDevice(g_Device, g_Allocator);
    vkDestroyInstance(g_Instance, g_Allocator);
}

static void CleanupVulkanWindow(ImGui_ImplVulkanH_Window* wd) {
    ImGui_ImplVulkanH_DestroyWindow(g_Instance, g_Device, wd, g_Allocator);
    vkDestroySurfaceKHR(g_Instance, wd->Surface, g_Allocator);
}

static void FrameRender(ImGui_ImplVulkanH_Window* wd, ImDrawData* draw_data) {
    VkSemaphore image_acquired_semaphore  = wd->FrameSemaphores[wd->SemaphoreIndex].ImageAcquiredSemaphore;
    VkSemaphore render_complete_semaphore = wd->FrameSemaphores[wd->SemaphoreIndex].RenderCompleteSemaphore;
    VkResult err = vkAcquireNextImageKHR(g_Device, wd->Swapchain, UINT64_MAX, image_acquired_semaphore, VK_NULL_HANDLE, &wd->FrameIndex);
    if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR) { g_SwapChainRebuild = true; return; }
    
    ImGui_ImplVulkanH_Frame* fd = &wd->Frames[wd->FrameIndex];
    vkWaitForFences(g_Device, 1, &fd->Fence, VK_TRUE, UINT64_MAX);
    vkResetFences(g_Device, 1, &fd->Fence);
    vkResetCommandPool(g_Device, fd->CommandPool, 0);
    
    VkCommandBufferBeginInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(fd->CommandBuffer, &info);
    
    VkRenderPassBeginInfo rp_info = {};
    rp_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rp_info.renderPass = wd->RenderPass;
    rp_info.framebuffer = fd->Framebuffer;
    rp_info.renderArea.extent.width = wd->Width;
    rp_info.renderArea.extent.height = wd->Height;
    rp_info.clearValueCount = 1;
    rp_info.pClearValues = &wd->ClearValue;
    vkCmdBeginRenderPass(fd->CommandBuffer, &rp_info, VK_SUBPASS_CONTENTS_INLINE);
    
    ImGui_ImplVulkan_RenderDrawData(draw_data, fd->CommandBuffer);
    vkCmdEndRenderPass(fd->CommandBuffer);
    
    VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submit_info = {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = &image_acquired_semaphore;
    submit_info.pWaitDstStageMask = &wait_stage;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &fd->CommandBuffer;
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = &render_complete_semaphore;
    vkEndCommandBuffer(fd->CommandBuffer);
    vkQueueSubmit(g_Queue, 1, &submit_info, fd->Fence);
}

static void FramePresent(ImGui_ImplVulkanH_Window* wd) {
    if (g_SwapChainRebuild) return;
    VkSemaphore render_complete_semaphore = wd->FrameSemaphores[wd->SemaphoreIndex].RenderCompleteSemaphore;
    VkPresentInfoKHR info = {};
    info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    info.waitSemaphoreCount = 1;
    info.pWaitSemaphores = &render_complete_semaphore;
    info.swapchainCount = 1;
    info.pSwapchains = &wd->Swapchain;
    info.pImageIndices = &wd->FrameIndex;
    VkResult err = vkQueuePresentKHR(g_Queue, &info);
    if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR) g_SwapChainRebuild = true;
    wd->SemaphoreIndex = (wd->SemaphoreIndex + 1) % wd->SemaphoreCount;
}

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
#include <cmath>
void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
{
    CarState* state = (CarState*)pDevice->pUserData;
    float* pOutputF32 = (float*)pOutput;

    int current_rpm = 0;
    int current_throttle = 0;
    {
        std::lock_guard<std::mutex> lock(state->mtx);
        current_rpm = state->current_data.rpm;
        current_throttle = state->current_data.throttle;
    }

    if (current_rpm < 4000) current_rpm = 4000;

    float f0 = (current_rpm / 60.0f) * 3.0f;
    float volume = 0.05f + (current_throttle / 100.0f) * 0.15f;
    float sampleRate = pDevice->sampleRate;

    static float ph[4] = {0, 0, 0, 0};

    const float weights[4] = { 0.5f, 0.35f, 0.1f, 0.05f };
    const float freqs[4]   = { f0,   f0*2,  f0*3, f0*4  };

    static float filtered_l = 0.0f;
    static float filtered_r = 0.0f;
    float alpha = 0.4f;

    for (ma_uint32 i = 0; i < frameCount; ++i) {
        float value = 0.0f;

        for (int h = 0; h < 4; ++h) {
            value += weights[h] * sinf(2.0f * M_PI * ph[h]);
            ph[h] += freqs[h] / sampleRate;
            if (ph[h] > 1.0f) ph[h] -= 1.0f;
        }

        value *= volume;
        filtered_l = alpha * value + (1.0f - alpha) * filtered_l;
        filtered_r = filtered_l;

        pOutputF32[i * pDevice->playback.channels + 0] = filtered_l;
        pOutputF32[i * pDevice->playback.channels + 1] = filtered_r;
    }
}

int main(int, char**) {
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) return 1;

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    float main_scale = ImGui_ImplGlfw_GetContentScaleForMonitor(glfwGetPrimaryMonitor());
    GLFWwindow* window = glfwCreateWindow((int)(1280 * main_scale), (int)(800 * main_scale), "Cars Data Analyse", nullptr, nullptr);
    if (!glfwVulkanSupported()) {
        printf("GLFW: Vulkan Not Supported\n");
        return 1;
    }

    ImVector<const char*> extensions;
    uint32_t extensions_count = 0;
    const char** glfw_extensions = glfwGetRequiredInstanceExtensions(&extensions_count);
    for (uint32_t i = 0; i < extensions_count; i++) extensions.push_back(glfw_extensions[i]);
    SetupVulkan(extensions);

    VkSurfaceKHR surface;
    VkResult err = glfwCreateWindowSurface(g_Instance, window, g_Allocator, &surface);
    check_vk_result(err);

    int w, h;
    glfwGetFramebufferSize(window, &w, &h);
    ImGui_ImplVulkanH_Window* wd = &g_MainWindowData;
    SetupVulkanWindow(wd, surface, w, h);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();

    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(main_scale);
    style.FontScaleDpi = main_scale;

    ImGui_ImplGlfw_InitForVulkan(window, true);
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = g_Instance;
    init_info.PhysicalDevice = g_PhysicalDevice;
    init_info.Device = g_Device;
    init_info.QueueFamily = g_QueueFamily;
    init_info.Queue = g_Queue;
    init_info.PipelineCache = g_PipelineCache;
    init_info.DescriptorPool = g_DescriptorPool;
    init_info.MinImageCount = g_MinImageCount;
    init_info.ImageCount = wd->ImageCount;
    init_info.Allocator = g_Allocator;
    init_info.PipelineInfoMain.RenderPass = wd->RenderPass;
    init_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    init_info.CheckVkResultFn = check_vk_result;
    ImGui_ImplVulkan_Init(&init_info);

    DataLoader loader;
    std::vector<TelemetryPoint> telemetryData = loader.loadCSV("data/telemetry_VER_Monza.csv");
    if (telemetryData.empty())
        return EXIT_FAILURE;

    CarState shared_state;
    shared_state.current_data = telemetryData[0]; 
    std::thread engine_thread(replay_engine, std::ref(telemetryData), std::ref(shared_state));
    auto display_start_time = std::chrono::steady_clock::now();

    ImVec4 clear_color = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);

    ma_engine audio_engine;
    ma_sound motor_sound;

    ma_engine_init(NULL, &audio_engine);
    ma_sound_init_from_file(&audio_engine, "sound/Formula1Sound.mp3",
        MA_SOUND_FLAG_LOOPING, NULL, NULL, &motor_sound);
    ma_sound_start(&motor_sound);


    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        int fb_width, fb_height;
        glfwGetFramebufferSize(window, &fb_width, &fb_height);
        if (fb_width > 0 && fb_height > 0 && (g_SwapChainRebuild || g_MainWindowData.Width != fb_width || g_MainWindowData.Height != fb_height)) {
            ImGui_ImplVulkan_SetMinImageCount(g_MinImageCount);
            ImGui_ImplVulkanH_CreateOrResizeWindow(g_Instance, g_PhysicalDevice, g_Device, wd, g_QueueFamily, g_Allocator, fb_width, fb_height, g_MinImageCount, 0);
            g_MainWindowData.FrameIndex = 0;
            g_SwapChainRebuild = false;
        }
        if (glfwGetWindowAttrib(window, GLFW_ICONIFIED) != 0) {
            ImGui_ImplGlfw_Sleep(10);
            continue;
        }

        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        TelemetryPoint display_data;
        bool is_active;
        {
            std::lock_guard<std::mutex> lock(shared_state.mtx);
            display_data = shared_state.current_data;
            is_active = shared_state.is_running;
        }

        float base_rpm = 8000.0f;
        float pitch = std::clamp(display_data.rpm / base_rpm, 0.3f, 3.0f);
        ma_sound_set_pitch(&motor_sound, pitch);


        float volume = 0.3f + (display_data.throttle / 100.0f) * 0.7f;
        ma_sound_set_volume(&motor_sound, volume);

        auto current_time = std::chrono::steady_clock::now();
        std::chrono::duration<double> elapsed_chrono = current_time - display_start_time;

        ImGui::Begin("Max Verstappen (Monza)", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
        
        ImGui::Text("Current Chrono : %.2f s", elapsed_chrono.count());
        
        ImGui::Separator();
        ImGui::Text("Vitesse : %d km/h", display_data.speed);
        ImGui::Text("Rapport : %d", display_data.nGear);
        
        ImGui::Spacing();

        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.2f, 0.8f, 0.2f, 1.0f));
        ImGui::ProgressBar(display_data.throttle / 100.0f, ImVec2(300.0f, 20.0f), "Accélérateur");
        ImGui::PopStyleColor();

        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
        ImGui::ProgressBar(display_data.rpm / 15000.0f, ImVec2(300.0f, 20.0f), "RPM");
        ImGui::PopStyleColor();

        ImGui::Spacing();

        if (display_data.brake) {
            ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "[FREINAGE ACTIF]");
        } else {
            ImGui::TextColored(ImVec4(0.4f, 0.4f, 0.4f, 1.0f), "[FREINAGE INACTIF]");
        }

        if (!is_active) {
            ImGui::Separator();
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Lap over !");
        }

        ImGui::End();

        ImGui::Render();
        ImDrawData* draw_data = ImGui::GetDrawData();
        const bool is_minimized = (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f);
        if (!is_minimized) {
            wd->ClearValue.color.float32[0] = clear_color.x * clear_color.w;
            wd->ClearValue.color.float32[1] = clear_color.y * clear_color.w;
            wd->ClearValue.color.float32[2] = clear_color.z * clear_color.w;
            wd->ClearValue.color.float32[3] = clear_color.w;
            FrameRender(wd, draw_data);
            FramePresent(wd);
        }
    }

    {
        std::lock_guard<std::mutex> lock(shared_state.mtx);
        shared_state.force_stop = true;
    }

    engine_thread.join();

    
    err = vkDeviceWaitIdle(g_Device);
    check_vk_result(err);
    ma_sound_uninit(&motor_sound);
    ma_engine_uninit(&audio_engine);
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    CleanupVulkanWindow(&g_MainWindowData);
    CleanupVulkan();
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}