#include "render/diligent/renderdoc_trigger.hpp"

#include "engine/core/log.hpp"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace render::diligent {
namespace {

using engine::core::log;
using engine::core::LogLevel;

// The prefix of RENDERDOC_API_1_6_0 this file uses, in the documented order.
//
// NOT VERIFIED AGAINST THE REAL HEADER -- RenderDoc is not installed on this machine, so the
// ordering below is from the documented v1.6.0 API rather than from renderdoc_app.h. That is why
// `RenderDocTrigger` validates itself at runtime (see the header): a wrong offset disables the
// feature instead of calling an arbitrary pointer. Everything past the entries actually called is
// deliberately omitted -- a shorter struct cannot be wrong about members it does not name, and the
// pointers are only ever read, never the struct copied.
struct ApiPrefix {
    void(__cdecl* GetAPIVersion)(int* major, int* minor, int* patch);
    void* SetCaptureOptionU32;
    void* SetCaptureOptionF32;
    void* GetCaptureOptionU32;
    void* GetCaptureOptionF32;
    void* SetFocusToggleKeys;
    void* SetCaptureKeys;
    void* GetOverlayBits;
    void* MaskOverlayBits;
    void* RemoveHooks;
    void* UnloadCrashHandler;
    void* SetCaptureFilePathTemplate;
    void* GetCaptureFilePathTemplate;
    std::uint32_t(__cdecl* GetNumCaptures)();
    void* GetCapture;
    void* TriggerCapture;
    void* IsTargetControlConnected;
    void* LaunchReplayUI;
    void* SetActiveWindow;
    void(__cdecl* StartFrameCapture)(void* device, void* window);
    std::uint32_t(__cdecl* IsFrameCapturing)();
    std::uint32_t(__cdecl* EndFrameCapture)(void* device, void* window);
};

constexpr int kApiVersion_1_6_0 = 10600;
using GetApiFn = int(__cdecl*)(int version, void** outPointers);

} // namespace

RenderDocTrigger::RenderDocTrigger() {
#if defined(_WIN32)
    // PASSIVE: asks whether the injector already loaded it. Never LoadLibrary.
    HMODULE module = ::GetModuleHandleA("renderdoc.dll");
    if (module == nullptr) {
        status_ = "not injected (run under RenderDoc to enable)";
        return;
    }
    auto getApi =
        reinterpret_cast<GetApiFn>(reinterpret_cast<void*>(::GetProcAddress(module, "RENDERDOC_GetAPI")));
    if (getApi == nullptr) {
        status_ = "renderdoc.dll loaded but RENDERDOC_GetAPI is missing";
        return;
    }
    void* api = nullptr;
    if (getApi(kApiVersion_1_6_0, &api) != 1 || api == nullptr) {
        status_ = "RENDERDOC_GetAPI refused version 1.6.0";
        return;
    }

    // First validation: the very first pointer. If this is wrong nothing else can be right.
    auto* prefix = static_cast<ApiPrefix*>(api);
    int major = 0;
    int minor = 0;
    int patch = 0;
    if (prefix->GetAPIVersion == nullptr) {
        status_ = "API pointer table has no GetAPIVersion";
        return;
    }
    prefix->GetAPIVersion(&major, &minor, &patch);
    if (major != 1 || minor < 6) {
        status_ = "RenderDoc API " + std::to_string(major) + "." + std::to_string(minor) +
                  " is older than the 1.6 layout this expects";
        return;
    }
    api_ = api;
    status_ = "RenderDoc API " + std::to_string(major) + "." + std::to_string(minor) + "." +
              std::to_string(patch) + " ready";
#else
    status_ = "not supported on this platform";
#endif
}

void RenderDocTrigger::begin_capture() {
    if (api_ == nullptr || active_) {
        return;
    }
#if defined(_WIN32)
    auto* prefix = static_cast<ApiPrefix*>(api_);
    if (prefix->StartFrameCapture == nullptr || prefix->IsFrameCapturing == nullptr) {
        api_ = nullptr;
        status_ = "API table is missing StartFrameCapture -- disabled";
        return;
    }
    // Null device and window: capture whatever the next Present belongs to. That is what the
    // documented API does for the common single-swapchain case and it avoids having to reach a
    // native device handle through Diligent.
    prefix->StartFrameCapture(nullptr, nullptr);

    // SECOND VALIDATION, and the reason this feature can be trusted at all on a layout that could
    // not be checked against the real header: if the offsets were wrong, StartFrameCapture called
    // something else and no capture is running. Ask.
    if (prefix->IsFrameCapturing() == 0u) {
        api_ = nullptr;
        status_ = "StartFrameCapture did not start a capture -- API layout rejected, disabled";
        log(LogLevel::Error, "renderdoc: {}", status_);
        return;
    }
    active_ = true;
#endif
}

bool RenderDocTrigger::end_capture() {
    if (api_ == nullptr || !active_) {
        return false;
    }
#if defined(_WIN32)
    auto* prefix = static_cast<ApiPrefix*>(api_);
    active_ = false;
    if (prefix->EndFrameCapture == nullptr) {
        return false;
    }
    const bool ok = prefix->EndFrameCapture(nullptr, nullptr) != 0u;
    if (ok) {
        ++captures_;
    }
    return ok;
#else
    return false;
#endif
}

bool RenderDocTrigger::capturing() const noexcept {
#if defined(_WIN32)
    if (api_ == nullptr) {
        return false;
    }
    const auto* prefix = static_cast<const ApiPrefix*>(api_);
    return prefix->IsFrameCapturing != nullptr && prefix->IsFrameCapturing() != 0u;
#else
    return false;
#endif
}

RenderDocTrigger& renderdoc_trigger() {
    static RenderDocTrigger instance;
    return instance;
}

} // namespace render::diligent
