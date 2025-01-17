#include "PlatformBase.h"
#include "PixelsForGlory/RayTracerAPI.h"

#include <assert.h>
#include <math.h>
#include <map>
#include <vector>

#include "PixelsForGlory/Debug.h"

#define PLUGIN_CHECK()  if (s_CurrentAPI == nullptr) { return; }
#define PLUGIN_CHECK_RETURN(returnValue)  if (s_CurrentAPI == nullptr) { return returnValue; }

// --------------------------------------------------------------------------
// SetTimeFromUnity, an example function we export which is called by one of the scripts.

static float g_Time;
extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API SetTimeFromUnity (float t) { g_Time = t; }

static void* g_TextureHandle = nullptr;
static int   g_TextureWidth = 0;
static int   g_TextureHeight = 0;
extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API SetTargetTexture(void* textureHandle, int width, int height)
{
    // A script calls this at initialization time; just remember the texture pointer here.
    // Will update texture pixels each frame from the plugin rendering event (texture update
    // needs to happen on the rendering thread).
    g_TextureHandle = textureHandle;
    g_TextureWidth = width;
    g_TextureHeight = height;
}

// --------------------------------------------------------------------------
// UnitySetInterfaces
static void UNITY_INTERFACE_API OnGraphicsDeviceEvent(UnityGfxDeviceEventType eventType);

static IUnityInterfaces* s_UnityInterfaces = NULL;
static IUnityGraphics* s_Graphics = NULL;

extern "C" void    UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UnityPluginLoad(IUnityInterfaces* unityInterfaces)
{
    s_UnityInterfaces = unityInterfaces;
    s_Graphics = s_UnityInterfaces->Get<IUnityGraphics>();
    s_Graphics->RegisterDeviceEventCallback(OnGraphicsDeviceEvent);
    
#if SUPPORT_VULKAN
    if (s_Graphics->GetRenderer() == kUnityGfxRendererNull)
    {
        extern void RenderAPI_Vulkan_OnPluginLoad(IUnityInterfaces*);
        RenderAPI_Vulkan_OnPluginLoad(unityInterfaces);
    }
#endif // SUPPORT_VULKAN

    // Run OnGraphicsDeviceEvent(initialize) manually on plugin load
    OnGraphicsDeviceEvent(kUnityGfxDeviceEventInitialize);
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UnityPluginUnload()
{
    s_Graphics->UnregisterDeviceEventCallback(OnGraphicsDeviceEvent);

    // Clear debug callback because it will freeze Application.Reload
    //ClearDebugCallback();
}

// --------------------------------------------------------------------------
// GraphicsDeviceEvent


static PixelsForGlory::RayTracerAPI* s_CurrentAPI = nullptr;
static UnityGfxRenderer s_DeviceType = kUnityGfxRendererNull;
static void UNITY_INTERFACE_API OnGraphicsDeviceEvent(UnityGfxDeviceEventType eventType)
{
    // Create graphics API implementation upon initialization
    if (eventType == kUnityGfxDeviceEventInitialize)
    {
        assert(s_CurrentAPI == nullptr);
        s_DeviceType = s_Graphics->GetRenderer();
        s_CurrentAPI = PixelsForGlory::CreateRayTracerAPI(s_DeviceType);
    }

    // Let the implementation process the device related events
    bool success = false;
    if (s_CurrentAPI)
    {
        success = s_CurrentAPI->ProcessDeviceEvent(eventType, s_UnityInterfaces);
    }

    // If kUnityGfxDeviceEventInitialize was not successful, we don't want the plugin to run
    if (eventType == kUnityGfxDeviceEventInitialize && success == false)
    {
        s_CurrentAPI = nullptr;
        s_DeviceType = kUnityGfxRendererNull;
    }

    // Cleanup graphics API implementation upon shutdown
    if (eventType == kUnityGfxDeviceEventShutdown)
    {
        s_CurrentAPI = nullptr;
        s_DeviceType = kUnityGfxRendererNull;
    }
}

enum class Events
{
    None        = 0,
    TraceRays   = 1
};

static void UNITY_INTERFACE_API OnEvent(int eventId)
{
    // Unknown / unsupported graphics device type? Do nothing
    PLUGIN_CHECK();
}



static void UNITY_INTERFACE_API OnEventAndData(int eventId, void* data)
{
    // Unknown / unsupported graphics device type? Do nothing
    PLUGIN_CHECK();

    auto event = static_cast<Events>(eventId);

    switch (event)
    {
    case Events::TraceRays:
        int cameraInstanceId = *static_cast<int*>(data);
        s_CurrentAPI->TraceRays(cameraInstanceId);
        break;
    }
    
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API SetShaderFolder(const char* shaderFolder)
{
    PLUGIN_CHECK();

    s_CurrentAPI->SetShaderFolder(std::string(shaderFolder));
}

extern "C" int UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API SetRenderTarget(int cameraInstanceId, int unityTextureFormat, int width, int height, void* textureHandle)
{
    PLUGIN_CHECK_RETURN(0);

    return s_CurrentAPI->SetRenderTarget(cameraInstanceId, unityTextureFormat, width, height, textureHandle);
}

extern "C" int UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API GetSharedMeshIndex(int sharedMeshInstanceId)
{
    PLUGIN_CHECK_RETURN(-1);

    return s_CurrentAPI->GetSharedMeshIndex(sharedMeshInstanceId);
}


extern "C" int UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API AddSharedMesh(int instanceId, float* verticesArray, float* normalsArray, float* uvsArray, int vertexCount, int* indicesArray, int indexCount)
{
    PLUGIN_CHECK_RETURN(-1);

    return s_CurrentAPI->AddSharedMesh(instanceId, verticesArray, normalsArray, uvsArray, vertexCount, indicesArray, indexCount);
}

extern "C" int UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API GetTlasInstanceIndex(int gameObjectInstanceId)
{
    PLUGIN_CHECK_RETURN(-1);

    return s_CurrentAPI->GetTlasInstanceIndex(gameObjectInstanceId);
}

extern "C" int UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API AddTlasInstance(int gameObjectInstanceId, int sharedMeshIndex, float* l2wMatrix)
{
    PLUGIN_CHECK_RETURN(-1);

    return s_CurrentAPI->AddTlasInstance(gameObjectInstanceId, sharedMeshIndex, l2wMatrix);
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API RemoveTlasInstance(int meshInstanceIndex)
{
    PLUGIN_CHECK();

    s_CurrentAPI->RemoveTlasInstance(meshInstanceIndex);
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API BuildTlas()
{
    PLUGIN_CHECK();

    s_CurrentAPI->BuildTlas();
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API Prepare()
{
    PLUGIN_CHECK();

    s_CurrentAPI->Prepare();
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API ResetPipeline()
{
    PLUGIN_CHECK();

    s_CurrentAPI->ResetPipeline();
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UpdateCamera(int cameraInstanceId, float* camPos, float* camDir, float* camUp, float* camSide, float* camNearFarFov)
{
    PLUGIN_CHECK();

    s_CurrentAPI->UpdateCamera(cameraInstanceId, camPos, camDir, camUp, camSide, camNearFarFov);
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UpdateSceneData(float* color)
{
    PLUGIN_CHECK();

    s_CurrentAPI->UpdateSceneData(color);
}

extern "C" UnityRenderingEvent UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API GetEventFunc()
{
    return OnEvent;
}

extern "C" UnityRenderingEventAndData UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API GetEventAndDataFunc()
{
    return OnEventAndData;
}


