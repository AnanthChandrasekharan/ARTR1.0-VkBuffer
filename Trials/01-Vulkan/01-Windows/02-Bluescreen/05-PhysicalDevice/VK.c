#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

#include "VK.h"
#define LOG_FILE (char*)"Log.txt"

// Vulkan
#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>

#define WIN_WIDTH 800
#define WIN_HEIGHT 600

#pragma comment(lib, "vulkan-1.lib")

// Global Function Declarations
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

const char* gpszAppName = "ARTR";

HWND ghwnd = NULL;
BOOL gbActive = FALSE;
DWORD dwStyle = 0;
WINDOWPLACEMENT wpPrev;
BOOL gbFullscreen = FALSE;

FILE* gFILE = NULL;

// Vulkan related globals
uint32_t enabledInstanceExtensionsCount = 0;
const char* enabledInstanceExtensionNames_array[2];

VkInstance vkInstance = VK_NULL_HANDLE;
VkSurfaceKHR vkSurfaceKHR = VK_NULL_HANDLE;

VkPhysicalDevice vkPhysicalDevice_selected = VK_NULL_HANDLE;
uint32_t graphicsQuequeFamilyIndex_selected = UINT32_MAX;
VkPhysicalDeviceMemoryProperties vkPhysicalDeviceMemoryProperties;

// Entry-Point Function
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpszCmdLine, int iCmdShow)
{
    // Function Declarations
    VkResult initialize(void);
    void uninitialize(void);
    void display(void);
    void update(void);

    WNDCLASSEX wndclass;
    HWND hwnd;
    MSG msg;
    TCHAR szAppName[256];
    int iResult = 0;

    int SW = GetSystemMetrics(SM_CXSCREEN);
    int SH = GetSystemMetrics(SM_CYSCREEN);
    int xCoordinate = ((SW / 2) - (WIN_WIDTH / 2));
    int yCoordinate = ((SH / 2) - (WIN_HEIGHT / 2));

    BOOL bDone = FALSE;
    VkResult vkResult = VK_SUCCESS;

    // Log File
    gFILE = fopen(LOG_FILE, "w");
    if (!gFILE)
    {
        MessageBox(NULL, TEXT("Program cannot open log file!"), TEXT("Error"), MB_OK | MB_ICONERROR);
        exit(0);
    }
    else
    {
        fprintf(gFILE, "WinMain()-> Program started successfully\n");
    }

    wsprintf(szAppName, TEXT("%s"), gpszAppName);

    // WNDCLASSEX Initialization
    wndclass.cbSize        = sizeof(WNDCLASSEX);
    wndclass.style         = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wndclass.cbClsExtra    = 0;
    wndclass.cbWndExtra    = 0;
    wndclass.lpfnWndProc   = WndProc;
    wndclass.hInstance     = hInstance;
    wndclass.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wndclass.hIcon         = LoadIcon(hInstance, MAKEINTRESOURCE(MYICON));
    wndclass.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wndclass.lpszClassName = szAppName;
    wndclass.lpszMenuName  = NULL;
    wndclass.hIconSm       = LoadIcon(hInstance, MAKEINTRESOURCE(MYICON));

    // Register
    RegisterClassEx(&wndclass);

    // Create Window
    hwnd = CreateWindowEx(WS_EX_APPWINDOW,
                          szAppName,
                          TEXT("05_PhysicalDevice"),
                          WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_VISIBLE,
                          xCoordinate,
                          yCoordinate,
                          WIN_WIDTH,
                          WIN_HEIGHT,
                          NULL,
                          NULL,
                          hInstance,
                          NULL);

    ghwnd = hwnd;

    // Initialize
    vkResult = initialize();
    if (vkResult != VK_SUCCESS)
    {
        fprintf(gFILE, "WinMain(): initialize()  function failed\n");
        DestroyWindow(hwnd);
        hwnd = NULL;
    }
    else
    {
        fprintf(gFILE, "WinMain(): initialize() succeeded\n");
    }

    // Show Window
    ShowWindow(hwnd, iCmdShow);
    UpdateWindow(hwnd);
    SetForegroundWindow(hwnd);
    SetFocus(hwnd);

    // Message Loop
    while (bDone == FALSE)
    {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
                bDone = TRUE;
            else
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
        else
        {
            if (gbActive == TRUE)
            {
                display();
                update();
            }
        }
    }

    // Uninitialize
    uninitialize();

    return((int)msg.wParam);
}

// Window Procedure
LRESULT CALLBACK WndProc(HWND hwnd, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
    void ToggleFullscreen(void);
    void resize(int, int);

    switch (iMsg)
    {
        case WM_CREATE:
            memset((void*)&wpPrev, 0, sizeof(WINDOWPLACEMENT));
            wpPrev.length = sizeof(WINDOWPLACEMENT);
        break;
        
        case WM_SETFOCUS:
            gbActive = TRUE;
            break;

        case WM_KILLFOCUS:
            gbActive = FALSE;
            break;

        case WM_SIZE:
            resize(LOWORD(lParam), HIWORD(lParam));
            break;

        case WM_KEYDOWN:
            switch (LOWORD(wParam))
            {
            case VK_ESCAPE:
                fprintf(gFILE, "WndProc() VK_ESCAPE-> Program ended successfully.\n");
                fclose(gFILE);
                gFILE = NULL;
                DestroyWindow(hwnd);
                break;
            }
            break;

        case WM_CHAR:
            switch (LOWORD(wParam))
            {
            case 'F':
            case 'f':
                if (gbFullscreen == FALSE)
                {
                    ToggleFullscreen();
                    gbFullscreen = TRUE;
                    fprintf(gFILE, "WndProc() WM_CHAR(F key)-> Program entered Fullscreen.\n");
                }
                else
                {
                    ToggleFullscreen();
                    gbFullscreen = FALSE;
                    fprintf(gFILE, "WndProc() WM_CHAR(F key)-> Program ended Fullscreen.\n");
                }
                break;
            }
            break;

        case WM_RBUTTONDOWN:
            DestroyWindow(hwnd);
            break;

        case WM_CLOSE:
            uninitialize();
            DestroyWindow(hwnd);
            break;

        case WM_DESTROY:
            PostQuitMessage(0);
            break;
        
        default:
            break;
    }

    return(DefWindowProc(hwnd, iMsg, wParam, lParam));
}

void ToggleFullscreen(void)
{
    MONITORINFO mi = { sizeof(MONITORINFO) };

    if (gbFullscreen == FALSE)
    {
        dwStyle = GetWindowLong(ghwnd, GWL_STYLE);

        if (dwStyle & WS_OVERLAPPEDWINDOW)
        {
            if (GetWindowPlacement(ghwnd, &wpPrev) &&
                GetMonitorInfo(MonitorFromWindow(ghwnd, MONITORINFOF_PRIMARY), &mi))
            {
                SetWindowLong(ghwnd, GWL_STYLE, dwStyle & ~WS_OVERLAPPEDWINDOW);

                SetWindowPos(ghwnd,
                             HWND_TOP,
                             mi.rcMonitor.left,
                             mi.rcMonitor.top,
                             mi.rcMonitor.right - mi.rcMonitor.left,
                             mi.rcMonitor.bottom - mi.rcMonitor.top,
                             SWP_NOZORDER | SWP_FRAMECHANGED);
            }
        }
        ShowCursor(FALSE);
    }
    else
    {
        SetWindowPlacement(ghwnd, &wpPrev);
        SetWindowLong(ghwnd, GWL_STYLE, dwStyle | WS_OVERLAPPEDWINDOW);
        SetWindowPos(ghwnd,
                     HWND_TOP,
                     0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOOWNERZORDER | SWP_NOZORDER | SWP_FRAMECHANGED);
        ShowCursor(TRUE);
    }
}

VkResult initialize(void)
{
    VkResult CreateVulkanInstance(void);
    VkResult GetSupportedSurface(void);
    VkResult GetPhysicalDevice(void);

    VkResult vkResult = VK_SUCCESS;

    vkResult = CreateVulkanInstance();
    if (vkResult != VK_SUCCESS)
    {
        fprintf(gFILE, "initialize(): CreateVulkanInstance() function failed\n");
        return vkResult;
    }
    else
    {
        fprintf(gFILE, "initialize(): CreateVulkanInstance() succeeded\n");
    }

    vkResult = GetSupportedSurface();
    if (vkResult != VK_SUCCESS)
    {
        fprintf(gFILE, "initialize(): GetSupportedSurface() function failed\n");
        return vkResult;
    }
    else
    {
        fprintf(gFILE, "initialize(): GetSupportedSurface() succeeded\n");
    }

    vkResult = GetPhysicalDevice();
    if (vkResult != VK_SUCCESS)
    {
        fprintf(gFILE, "initialize(): GetPhysicalDevice() function failed\n");
        return vkResult;
    }
    else
    {
        fprintf(gFILE, "initialize(): GetPhysicalDevice() succeeded\n");
    }

    return vkResult;
}

void resize(int width, int height)
{
    // Code for resizing if needed (e.g. swapchain recreation)
}

void display(void)
{
    // Rendering code stub
}

void update(void)
{
    // Animation/logic code stub
}

void uninitialize(void)
{
    if (gbFullscreen == TRUE)
    {
        ToggleFullscreen();
        gbFullscreen = FALSE;
    }

    if (ghwnd)
    {
        DestroyWindow(ghwnd);
        ghwnd = NULL;
    }
    
    if (vkSurfaceKHR)
    {
        vkDestroySurfaceKHR(vkInstance, vkSurfaceKHR, NULL);
        vkSurfaceKHR = VK_NULL_HANDLE;
        fprintf(gFILE, "uninitialize(): vkDestroySurfaceKHR() succeeded\n");
    }

    if (vkInstance)
    {
        vkDestroyInstance(vkInstance, NULL);
        vkInstance = VK_NULL_HANDLE;
        fprintf(gFILE, "uninitialize(): vkDestroyInstance() succeeded\n");
    }

    if (gFILE)
    {
        fprintf(gFILE, "uninitialize()-> Program ended successfully.\n");
        fclose(gFILE);
        gFILE = NULL;
    }
}

// Vulkan-related functions

VkResult CreateVulkanInstance(void)
{
    VkResult FillInstanceExtensionNames(void);
    VkResult vkResult = VK_SUCCESS;

    vkResult = FillInstanceExtensionNames();
    if (vkResult != VK_SUCCESS)
    {
        fprintf(gFILE, "CreateVulkanInstance(): FillInstanceExtensionNames() function failed\n");
        return vkResult;
    }
    else
    {
        fprintf(gFILE, "CreateVulkanInstance(): FillInstanceExtensionNames() succeeded\n");
    }

    VkApplicationInfo vkApplicationInfo;
    memset((void*)&vkApplicationInfo, 0, sizeof(VkApplicationInfo));

    vkApplicationInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    vkApplicationInfo.pNext              = NULL;
    vkApplicationInfo.pApplicationName   = gpszAppName;
    vkApplicationInfo.applicationVersion = 1;
    vkApplicationInfo.pEngineName        = gpszAppName;
    vkApplicationInfo.engineVersion      = 1;
    vkApplicationInfo.apiVersion         = VK_API_VERSION_1_4; // Adjust if needed

    VkInstanceCreateInfo vkInstanceCreateInfo;
    memset((void*)&vkInstanceCreateInfo, 0, sizeof(VkInstanceCreateInfo));

    vkInstanceCreateInfo.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    vkInstanceCreateInfo.pNext                   = NULL;
    vkInstanceCreateInfo.pApplicationInfo        = &vkApplicationInfo;
    vkInstanceCreateInfo.enabledExtensionCount   = enabledInstanceExtensionsCount;
    vkInstanceCreateInfo.ppEnabledExtensionNames = enabledInstanceExtensionNames_array;

    vkResult = vkCreateInstance(&vkInstanceCreateInfo, NULL, &vkInstance);
    if (vkResult == VK_ERROR_INCOMPATIBLE_DRIVER)
    {
        fprintf(gFILE, "CreateVulkanInstance(): incompatible driver, error code %d\n", vkResult);
        return vkResult;
    }
    else if (vkResult == VK_ERROR_EXTENSION_NOT_PRESENT)
    {
        fprintf(gFILE, "CreateVulkanInstance(): required extension not present, error code %d\n", vkResult);
        return vkResult;
    }
    else if (vkResult != VK_SUCCESS)
    {
        fprintf(gFILE, "CreateVulkanInstance(): unknown error, code %d\n", vkResult);
        return vkResult;
    }
    else
    {
        fprintf(gFILE, "CreateVulkanInstance(): vkCreateInstance() succeeded\n");
    }

    return vkResult;
}

VkResult FillInstanceExtensionNames(void)
{
    VkResult vkResult = VK_SUCCESS;
    uint32_t instanceExtensionCount = 0;

    vkResult = vkEnumerateInstanceExtensionProperties(NULL, &instanceExtensionCount, NULL);
    if (vkResult != VK_SUCCESS)
    {
        fprintf(gFILE, "FillInstanceExtensionNames(): first call to vkEnumerateInstanceExtensionProperties() failed\n");
        return vkResult;
    }
    else
    {
        fprintf(gFILE, "FillInstanceExtensionNames(): first call succeeded\n");
    }

    VkExtensionProperties* vkExtensionProperties_array =
        (VkExtensionProperties*)malloc(sizeof(VkExtensionProperties) * instanceExtensionCount);

    vkResult = vkEnumerateInstanceExtensionProperties(NULL, &instanceExtensionCount, vkExtensionProperties_array);
    if (vkResult != VK_SUCCESS)
    {
        fprintf(gFILE, "FillInstanceExtensionNames(): second call to vkEnumerateInstanceExtensionProperties() failed\n");
        return vkResult;
    }
    else
    {
        fprintf(gFILE, "FillInstanceExtensionNames(): second call succeeded\n");
    }

    char** instanceExtensionNames_array = (char**)malloc(sizeof(char*) * instanceExtensionCount);

    for (uint32_t i = 0; i < instanceExtensionCount; i++)
    {
        instanceExtensionNames_array[i] =
            (char*)malloc(strlen(vkExtensionProperties_array[i].extensionName) + 1);
        strcpy(instanceExtensionNames_array[i], vkExtensionProperties_array[i].extensionName);

        fprintf(gFILE, "FillInstanceExtensionNames(): Vulkan Extension Name = %s\n",
                instanceExtensionNames_array[i]);
    }

    VkBool32 vulkanSurfaceExtensionFound = VK_FALSE;
    VkBool32 vulkanWin32SurfaceExtensionFound = VK_FALSE;

    for (uint32_t i = 0; i < instanceExtensionCount; i++)
    {
        if (strcmp(instanceExtensionNames_array[i], VK_KHR_SURFACE_EXTENSION_NAME) == 0)
        {
            vulkanSurfaceExtensionFound = VK_TRUE;
            enabledInstanceExtensionNames_array[enabledInstanceExtensionsCount++] =
                VK_KHR_SURFACE_EXTENSION_NAME;
        }
        else if (strcmp(instanceExtensionNames_array[i], VK_KHR_WIN32_SURFACE_EXTENSION_NAME) == 0)
        {
            vulkanWin32SurfaceExtensionFound = VK_TRUE;
            enabledInstanceExtensionNames_array[enabledInstanceExtensionsCount++] =
                VK_KHR_WIN32_SURFACE_EXTENSION_NAME;
        }
    }

    for (uint32_t i = 0; i < instanceExtensionCount; i++)
    {
        free(instanceExtensionNames_array[i]);
    }
    free(instanceExtensionNames_array);
    free(vkExtensionProperties_array);

    if (vulkanSurfaceExtensionFound == VK_FALSE)
    {
        fprintf(gFILE, "FillInstanceExtensionNames(): VK_KHR_SURFACE_EXTENSION_NAME not found\n");
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    else
    {
        fprintf(gFILE, "FillInstanceExtensionNames(): VK_KHR_SURFACE_EXTENSION_NAME is found\n");
    }

    if (vulkanWin32SurfaceExtensionFound == VK_FALSE)
    {
        fprintf(gFILE, "FillInstanceExtensionNames(): VK_KHR_WIN32_SURFACE_EXTENSION_NAME not found\n");
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    else
    {
        fprintf(gFILE, "FillInstanceExtensionNames(): VK_KHR_WIN32_SURFACE_EXTENSION_NAME is found\n");
    }

    for (uint32_t i = 0; i < enabledInstanceExtensionsCount; i++)
    {
        fprintf(gFILE, "FillInstanceExtensionNames(): Enabled Vulkan Instance Extension Name = %s\n",
                enabledInstanceExtensionNames_array[i]);
    }

    return vkResult;
}

VkResult GetSupportedSurface(void)
{
    VkResult vkResult = VK_SUCCESS;

    VkWin32SurfaceCreateInfoKHR vkWin32SurfaceCreateInfoKHR;
    memset((void*)&vkWin32SurfaceCreateInfoKHR, 0, sizeof(VkWin32SurfaceCreateInfoKHR));

    vkWin32SurfaceCreateInfoKHR.sType     = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    vkWin32SurfaceCreateInfoKHR.pNext     = NULL;
    vkWin32SurfaceCreateInfoKHR.flags     = 0;
    vkWin32SurfaceCreateInfoKHR.hinstance = (HINSTANCE)GetWindowLongPtr(ghwnd, GWLP_HINSTANCE);
    vkWin32SurfaceCreateInfoKHR.hwnd      = ghwnd;

    vkResult = vkCreateWin32SurfaceKHR(vkInstance, &vkWin32SurfaceCreateInfoKHR, NULL, &vkSurfaceKHR);
    if (vkResult != VK_SUCCESS)
    {
        fprintf(gFILE, "GetSupportedSurface(): vkCreateWin32SurfaceKHR() function failed with error code %d\n", vkResult);
        return vkResult;
    }
    else
    {
        fprintf(gFILE, "GetSupportedSurface(): vkCreateWin32SurfaceKHR() succeeded\n");
    }

    return vkResult;
}

VkResult GetPhysicalDevice(void)
{
    VkResult vkResult = VK_SUCCESS;
    uint32_t physicalDeviceCount = 0;

    vkResult = vkEnumeratePhysicalDevices(vkInstance, &physicalDeviceCount, NULL);
    if (vkResult != VK_SUCCESS)
    {
        fprintf(gFILE, "GetPhysicalDevice(): vkEnumeratePhysicalDevices() first call failed with error code %d\n", vkResult);
        return vkResult;
    }
    else if (physicalDeviceCount == 0)
    {
        fprintf(gFILE, "GetPhysicalDevice(): No physical devices found.\n");
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    else
    {
        fprintf(gFILE, "GetPhysicalDevice(): vkEnumeratePhysicalDevices() first call succedded\n");
    }

    VkPhysicalDevice *vkPhysicalDevice_array =
        (VkPhysicalDevice*)malloc(sizeof(VkPhysicalDevice) * physicalDeviceCount);

    if (!vkPhysicalDevice_array)
    {
        fprintf(gFILE, "GetPhysicalDevice(): failed to allocate memory for vkPhysicalDevice_array.\n");
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    vkResult = vkEnumeratePhysicalDevices(vkInstance, &physicalDeviceCount, vkPhysicalDevice_array);
    if (vkResult != VK_SUCCESS)
    {
        fprintf(gFILE, "GetPhysicalDevice(): vkEnumeratePhysicalDevices() second call failed with error code %d\n", vkResult);
        free(vkPhysicalDevice_array);
        return vkResult;
    }
    else
    {
        fprintf(gFILE, "GetPhysicalDevice(): vkEnumeratePhysicalDevices() second call succedded\n");
    }

    VkBool32 bFound = VK_FALSE;

    for (uint32_t i = 0; i < physicalDeviceCount; i++)
    {
        // Query device properties for name, etc.
        VkPhysicalDeviceProperties deviceProperties;
        memset((void*)&deviceProperties, 0, sizeof(VkPhysicalDeviceProperties));
        vkGetPhysicalDeviceProperties(vkPhysicalDevice_array[i], &deviceProperties);

        // Print each device by index and name
        fprintf(gFILE, "\nPhysical Device %u: %s\n", i, deviceProperties.deviceName);

        // Check queue families for graphics + presentation support
        uint32_t queueCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(vkPhysicalDevice_array[i], &queueCount, NULL);

        VkQueueFamilyProperties *vkQueueFamilyProperties_array =
            (VkQueueFamilyProperties*)malloc(sizeof(VkQueueFamilyProperties) * queueCount);

        vkGetPhysicalDeviceQueueFamilyProperties(vkPhysicalDevice_array[i],
                                                 &queueCount,
                                                 vkQueueFamilyProperties_array);

        VkBool32 *isQuequeSurfaceSupported_array =
            (VkBool32*)malloc(sizeof(VkBool32) * queueCount);

        for (uint32_t j = 0; j < queueCount; j++)
        {
            vkGetPhysicalDeviceSurfaceSupportKHR(vkPhysicalDevice_array[i],
                                                 j,
                                                 vkSurfaceKHR,
                                                 &isQuequeSurfaceSupported_array[j]);
        }

        for (uint32_t j = 0; j < queueCount; j++)
        {
            if ((vkQueueFamilyProperties_array[j].queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
                (isQuequeSurfaceSupported_array[j] == VK_TRUE))
            {
                vkPhysicalDevice_selected         = vkPhysicalDevice_array[i];
                graphicsQuequeFamilyIndex_selected = j;
                bFound = VK_TRUE;

                fprintf(gFILE, "==> SELECTED this Physical Device (Index %u): %s\n",
                        i, deviceProperties.deviceName);
                break;
            }
        }

        free(isQuequeSurfaceSupported_array);
        free(vkQueueFamilyProperties_array);

        if (bFound == VK_TRUE)
            break;
    }

    free(vkPhysicalDevice_array);

    if (bFound == VK_FALSE)
    {
        fprintf(gFILE, "GetPhysicalDevice(): Failed to obtain a graphics-supported physical device\n");
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    else
    {
        fprintf(gFILE, "GetPhysicalDevice(): Successfully chose a physical device with graphics + present support\n");
    }

    // ----- ADDED/UPDATED BELOW FOR PRINTING DEVICE DETAILS -----
    {
        // 1) Device Properties
        VkPhysicalDeviceProperties deviceProps;
        memset(&deviceProps, 0, sizeof(deviceProps));
        vkGetPhysicalDeviceProperties(vkPhysicalDevice_selected, &deviceProps);

        fprintf(gFILE, "\n***** SELECTED DEVICE FULL DETAILS *****\n");
        fprintf(gFILE, "Device Name  : %s\n", deviceProps.deviceName);
        fprintf(gFILE, "API Version  : %u.%u.%u\n",
            VK_VERSION_MAJOR(deviceProps.apiVersion),
            VK_VERSION_MINOR(deviceProps.apiVersion),
            VK_VERSION_PATCH(deviceProps.apiVersion));
        fprintf(gFILE, "Driver Ver.  : %u (Vendor Specific)\n", deviceProps.driverVersion);
        fprintf(gFILE, "VendorID     : 0x%X\n", deviceProps.vendorID);
        fprintf(gFILE, "DeviceID     : 0x%X\n", deviceProps.deviceID);
        fprintf(gFILE, "Device Type  : %d\n", deviceProps.deviceType); // 1=Integrated,2=Discrete,3=Virtual,4=CPU

        // Print pipelineCacheUUID
        fprintf(gFILE, "pipelineCacheUUID: ");
        for (int idx = 0; idx < VK_UUID_SIZE; idx++)
            fprintf(gFILE, "%02X", deviceProps.pipelineCacheUUID[idx]);
        fprintf(gFILE, "\n");

        // 2) Device Limits (just some highlights, not everything)
        VkPhysicalDeviceLimits limits = deviceProps.limits;
        fprintf(gFILE, "\n>> Device Limits <<\n");
        fprintf(gFILE, "maxImageDimension2D            : %u\n", limits.maxImageDimension2D);
        fprintf(gFILE, "maxUniformBufferRange          : %u\n", limits.maxUniformBufferRange);
        fprintf(gFILE, "maxStorageBufferRange          : %u\n", limits.maxStorageBufferRange);
        fprintf(gFILE, "maxPushConstantsSize           : %u\n", limits.maxPushConstantsSize);
        fprintf(gFILE, "maxMemoryAllocationCount       : %u\n", limits.maxMemoryAllocationCount);
        fprintf(gFILE, "maxSamplerAllocationCount      : %u\n", limits.maxSamplerAllocationCount);
        fprintf(gFILE, "bufferImageGranularity         : %llu\n", (unsigned long long)limits.bufferImageGranularity);
        fprintf(gFILE, "maxBoundDescriptorSets         : %u\n", limits.maxBoundDescriptorSets);
        fprintf(gFILE, "maxPerStageDescriptorSamplers  : %u\n", limits.maxPerStageDescriptorSamplers);
        fprintf(gFILE, "maxPerStageDescriptorUniformBuffers : %u\n", limits.maxPerStageDescriptorUniformBuffers);
        // ... (you can print more from limits if needed)

        // 3) Device Features
        VkPhysicalDeviceFeatures deviceFeatures;
        memset(&deviceFeatures, 0, sizeof(deviceFeatures));
        vkGetPhysicalDeviceFeatures(vkPhysicalDevice_selected, &deviceFeatures);

        fprintf(gFILE, "\n>> Device Features <<\n");
        fprintf(gFILE, "geometryShader               : %s\n", deviceFeatures.geometryShader               ? "YES":"NO");
        fprintf(gFILE, "tessellationShader           : %s\n", deviceFeatures.tessellationShader           ? "YES":"NO");
        fprintf(gFILE, "samplerAnisotropy            : %s\n", deviceFeatures.samplerAnisotropy            ? "YES":"NO");
        fprintf(gFILE, "multiDrawIndirect            : %s\n", deviceFeatures.multiDrawIndirect            ? "YES":"NO");
        fprintf(gFILE, "fillModeNonSolid             : %s\n", deviceFeatures.fillModeNonSolid             ? "YES":"NO");
        fprintf(gFILE, "wideLines                    : %s\n", deviceFeatures.wideLines                    ? "YES":"NO");
        fprintf(gFILE, "largePoints                  : %s\n", deviceFeatures.largePoints                  ? "YES":"NO");
        fprintf(gFILE, "alphaToOne                   : %s\n", deviceFeatures.alphaToOne                   ? "YES":"NO");
        // ... (print more features as desired)

        // 4) Memory Properties
        memset((void*)&vkPhysicalDeviceMemoryProperties, 0, sizeof(VkPhysicalDeviceMemoryProperties));
        vkGetPhysicalDeviceMemoryProperties(vkPhysicalDevice_selected, &vkPhysicalDeviceMemoryProperties);
        
        fprintf(gFILE, "\n>> Memory Properties <<\n");
        fprintf(gFILE, "Memory Heap Count: %u\n", vkPhysicalDeviceMemoryProperties.memoryHeapCount);
        for(uint32_t h = 0; h < vkPhysicalDeviceMemoryProperties.memoryHeapCount; h++)
        {
            VkMemoryHeap heap = vkPhysicalDeviceMemoryProperties.memoryHeaps[h];
            fprintf(gFILE, "  Heap[%u]: Size=%llu bytes, Flags=0x%X\n",
                    h, (unsigned long long)heap.size, heap.flags);
        }

        fprintf(gFILE, "Memory Type Count: %u\n", vkPhysicalDeviceMemoryProperties.memoryTypeCount);
        for(uint32_t t = 0; t < vkPhysicalDeviceMemoryProperties.memoryTypeCount; t++)
        {
            VkMemoryType memType = vkPhysicalDeviceMemoryProperties.memoryTypes[t];
            fprintf(gFILE, "  Type[%u]: HeapIndex=%u, PropertyFlags=0x%X\n",
                    t, memType.heapIndex, memType.propertyFlags);
        }

        fprintf(gFILE, "***** END of SELECTED DEVICE DETAILS *****\n\n");
    }
    // ----- END OF ADDED DETAILS -----

    return vkResult;
}
