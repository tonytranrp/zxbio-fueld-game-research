#include <stdexcept>

#include "render/diligent/debug_overlay.hpp"

#include "detail/render_context_impl.hpp"

#include "ImGuiImplDiligent.hpp"

#include <imgui.h>

#include <backends/imgui_impl_glfw.h>

namespace render::diligent {

struct DebugOverlay::Impl {
    RenderContext* context = nullptr;
    std::unique_ptr<Diligent::ImGuiImplDiligent>
        imgui; // owns the ImGui context; created first, destroyed last
    bool glfwBackendInitialized = false;
};

DebugOverlay::DebugOverlay(RenderContext& context, GLFWwindow* window) : impl_(std::make_unique<Impl>()) {
    impl_->context = &context;
    auto& rc = context.impl();

    const Diligent::ImGuiDiligentCreateInfo createInfo{rc.device, rc.swapchain->GetDesc()};
    impl_->imgui = std::make_unique<Diligent::ImGuiImplDiligent>(createInfo); // creates the ImGui context

    // "ForOther": ImGui gets input from GLFW, rendering from Diligent. install_callbacks=true
    // chains any callbacks already registered (engine/input's), so both keep receiving events.
    if (!ImGui_ImplGlfw_InitForOther(window, /*install_callbacks=*/true)) {
        impl_->imgui.reset();
        throw std::runtime_error("ImGui GLFW backend initialization failed");
    }
    impl_->glfwBackendInitialized = true;
}

DebugOverlay::~DebugOverlay() {
    if (impl_ && impl_->glfwBackendInitialized) {
        ImGui_ImplGlfw_Shutdown(); // before ImGuiImplDiligent destroys the ImGui context
    }
}

void DebugOverlay::render(const OverlayStats& stats) {
    auto& rc = impl_->context->impl();
    const Diligent::SwapChainDesc& scDesc = rc.swapchain->GetDesc();

    ImGui_ImplGlfw_NewFrame();
    impl_->imgui->NewFrame(scDesc.Width, scDesc.Height, scDesc.PreTransform);

    constexpr double kMiB = 1024.0 * 1024.0;
    ImGui::SetNextWindowPos(ImVec2(10.0f, 10.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowBgAlpha(0.6f);
    if (ImGui::Begin("voxel_app", nullptr,
                     ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing)) {
        ImGui::Text("%.1f fps (%.2f ms)", static_cast<double>(stats.fps),
                    static_cast<double>(stats.frame_ms));
        ImGui::Separator();
        ImGui::Text("chunks ready: %zu", stats.ready_chunks);
        ImGui::Text("visible after culling: %zu / %zu", stats.visible_chunks, stats.total_chunk_meshes);
        ImGui::Text("objects: %zu (%zu round / %zu conifer / %zu shrub)", stats.objects, stats.objects_round,
                    stats.objects_conifer, stats.objects_shrub);
        if (stats.aim_line[0] != '\0') {
            ImGui::Text("aim: %s", stats.aim_line);
        }
        ImGui::Text("jobs in flight: %zu", stats.jobs_in_flight);
        ImGui::Separator();
        ImGui::Text("chunk GPU memory: %.1f MiB (peak %.1f)",
                    static_cast<double>(stats.gpu_self_bytes) / kMiB,
                    static_cast<double>(stats.gpu_self_peak_bytes) / kMiB);
        if (stats.budget.available) {
            ImGui::Text("VRAM (VK_EXT_memory_budget): %.0f / %.0f MiB",
                        static_cast<double>(stats.budget.device_local_usage_bytes) / kMiB,
                        static_cast<double>(stats.budget.device_local_budget_bytes) / kMiB);
        } else {
            ImGui::TextDisabled("VRAM budget: unavailable on this backend");
        }
    }
    ImGui::End();

    impl_->imgui->Render(rc.context);
}

} // namespace render::diligent
