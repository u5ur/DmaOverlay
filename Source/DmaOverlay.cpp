// DmaOverlay.cpp
#include <Core/Common/Memory/Memory.h>
#include <Core/Frontend/Render/Render.h>

#include <chrono>
#include <imgui/imgui.h>

/* make sure you are using legacy overlay and it has fully started */

int main()
{
    if (!Memory::Instance()->Start())
    {
        DEBUG("Failed to start DMA!\n");
        return 0;
    }

    if (!Memory::Instance()->SetTargetProcess("YourFavGame.exe"))
    {
        DEBUG("Failed to set target process!\n");
        return 0;
    }

    if (!Render::Instance()->Start())
    {
        DEBUG("Failed to start render!\n");
        return 0;
    }

    bool showDemo = true;

    int   frameCount = 0;
    float fps = 0.f;
    auto  lastTime = std::chrono::high_resolution_clock::now();

    while (true)
    {
        Render::Instance()->BeginFrame();

        ImGui::ShowDemoWindow(&showDemo);

        ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.4f);
        ImGui::Begin("##fps", nullptr,
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoInputs |
            ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::Text("FPS: %.1f", fps);
        ImGui::End();

        Render::Instance()->RenderFrame();
        Render::Instance()->EndFrame();

        frameCount++;

        auto  now = std::chrono::high_resolution_clock::now();
        float elapsed = std::chrono::duration<float>(now - lastTime).count();

        if (elapsed >= 1.0f)
        {
            fps = frameCount / elapsed;
            frameCount = 0;
            lastTime = now;
        }
    }

    Render::Instance()->Destroy();
    Memory::Instance()->Destroy();
    return 1;
}