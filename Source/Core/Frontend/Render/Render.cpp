// Render.cpp
#include "Render.h"
#include <imgui/imgui.h>
#include <imgui/imgui_impl_dx11.h>
#include <imgui/imgui_impl_win32.h>

bool Render::Start()
{
    mDiscordBase = VMMDLL_ProcessGetModuleBaseU(Memory::Instance()->mHandle, Memory::ProcessPid(), "DiscordHook64.dll");
    if (!mDiscordBase)
    {
        DEBUG("Failed to get discord base!\n");
        return false;
    }

    // 48 89 3D ? ? ? ? 48 85 ? 74 ? F0
    mFrameBuffer = Memory::Read<uint64_t>(mDiscordBase + 0x130E98);
    if (!mFrameBuffer)
    {
        DEBUG("Failed to get frame buffer!\n");
        return false;
    }

    mHandle = Memory::CreateScatterHandle();
    if (!mHandle)
    {
        DEBUG("Failed to create scatter handle!\n");
        return false;
    }

    mFrame.Width = 1920;
    mFrame.Height = 1080;
    mFrame.Size = mFrame.Width * mFrame.Height * 4;
    mFrame.Buffer1 = calloc(1, mFrame.Size);
    mFrame.Buffer2 = calloc(1, mFrame.Size);
    mFrame.CurrentBuffer = mFrame.Buffer1;

    D3D_FEATURE_LEVEL featureLevel;
    if (FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
        0, nullptr, 0, D3D11_SDK_VERSION, &mD3DDevice, &featureLevel, &mD3DContext)))
    {
        DEBUG("Failed to create D3D11 device!\n");
        return false;
    }

    D3D11_TEXTURE2D_DESC rtDesc{};
    rtDesc.Width = mFrame.Width;
    rtDesc.Height = mFrame.Height;
    rtDesc.MipLevels = 1;
    rtDesc.ArraySize = 1;
    rtDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    rtDesc.SampleDesc.Count = 1;
    rtDesc.Usage = D3D11_USAGE_DEFAULT;
    rtDesc.BindFlags = D3D11_BIND_RENDER_TARGET;

    if (FAILED(mD3DDevice->CreateTexture2D(&rtDesc, nullptr, &mRenderTarget)))
    {
        DEBUG("Failed to create render target!\n");
        return false;
    }

    D3D11_TEXTURE2D_DESC stagingDesc = rtDesc;
    stagingDesc.Usage = D3D11_USAGE_STAGING;
    stagingDesc.BindFlags = 0;
    stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

    if (FAILED(mD3DDevice->CreateTexture2D(&stagingDesc, nullptr, &mStagingTex)))
    {
        DEBUG("Failed to create staging texture!\n");
        return false;
    }

    if (FAILED(mD3DDevice->CreateRenderTargetView(mRenderTarget, nullptr, &mRTView)))
    {
        DEBUG("Failed to create render target view!\n");
        return false;
    }

    D3D11_VIEWPORT vp{};
    vp.Width = (float)mFrame.Width;
    vp.Height = (float)mFrame.Height;
    vp.MaxDepth = 1.0f;
    mD3DContext->RSSetViewports(1, &vp);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2((float)mFrame.Width, (float)mFrame.Height);
    io.DeltaTime = 1.0f / 60.0f;
    io.IniFilename = nullptr;

    ImGui_ImplDX11_Init(mD3DDevice, mD3DContext);

    return true;
}

void Render::Destroy()
{
    ImGui_ImplDX11_Shutdown();
    ImGui::DestroyContext();

    if (mRTView) { mRTView->Release();       mRTView = nullptr; }
    if (mStagingTex) { mStagingTex->Release();   mStagingTex = nullptr; }
    if (mRenderTarget) { mRenderTarget->Release();  mRenderTarget = nullptr; }
    if (mD3DContext) { mD3DContext->Release();    mD3DContext = nullptr; }
    if (mD3DDevice) { mD3DDevice->Release();     mD3DDevice = nullptr; }

    if (mFrame.Buffer1) { free(mFrame.Buffer1); mFrame.Buffer1 = nullptr; }
    if (mFrame.Buffer2) { free(mFrame.Buffer2); mFrame.Buffer2 = nullptr; }
    if (mHandle) { Memory::CloseScatterHandle(mHandle); mHandle = nullptr; }
}

void Render::BeginFrame()
{
    ImGuiIO& io = ImGui::GetIO();

    /* imgui, why is there not a func for vk to imgui key?!*/
    auto [mx, my] = Memory::GetMousePos();
    io.AddMousePosEvent(mx, my);
    io.AddMouseButtonEvent(0, Memory::IsKeyDown(VK_LBUTTON));
    io.AddMouseButtonEvent(1, Memory::IsKeyDown(VK_RBUTTON));
    io.AddMouseButtonEvent(2, Memory::IsKeyDown(VK_MBUTTON));

    io.AddKeyEvent(ImGuiMod_Ctrl, Memory::IsKeyDown(VK_CONTROL));
    io.AddKeyEvent(ImGuiMod_Shift, Memory::IsKeyDown(VK_SHIFT));
    io.AddKeyEvent(ImGuiMod_Alt, Memory::IsKeyDown(VK_MENU));
    io.AddKeyEvent(ImGuiMod_Super, Memory::IsKeyDown(VK_LWIN) || Memory::IsKeyDown(VK_RWIN));

    io.AddKeyEvent(ImGuiKey_Tab, Memory::IsKeyDown(VK_TAB));
    io.AddKeyEvent(ImGuiKey_LeftArrow, Memory::IsKeyDown(VK_LEFT));
    io.AddKeyEvent(ImGuiKey_RightArrow, Memory::IsKeyDown(VK_RIGHT));
    io.AddKeyEvent(ImGuiKey_UpArrow, Memory::IsKeyDown(VK_UP));
    io.AddKeyEvent(ImGuiKey_DownArrow, Memory::IsKeyDown(VK_DOWN));
    io.AddKeyEvent(ImGuiKey_PageUp, Memory::IsKeyDown(VK_PRIOR));
    io.AddKeyEvent(ImGuiKey_PageDown, Memory::IsKeyDown(VK_NEXT));
    io.AddKeyEvent(ImGuiKey_Home, Memory::IsKeyDown(VK_HOME));
    io.AddKeyEvent(ImGuiKey_End, Memory::IsKeyDown(VK_END));
    io.AddKeyEvent(ImGuiKey_Insert, Memory::IsKeyDown(VK_INSERT));
    io.AddKeyEvent(ImGuiKey_Delete, Memory::IsKeyDown(VK_DELETE));
    io.AddKeyEvent(ImGuiKey_Backspace, Memory::IsKeyDown(VK_BACK));
    io.AddKeyEvent(ImGuiKey_Space, Memory::IsKeyDown(VK_SPACE));
    io.AddKeyEvent(ImGuiKey_Enter, Memory::IsKeyDown(VK_RETURN));
    io.AddKeyEvent(ImGuiKey_Escape, Memory::IsKeyDown(VK_ESCAPE));
    io.AddKeyEvent(ImGuiKey_Apostrophe, Memory::IsKeyDown(VK_OEM_7));
    io.AddKeyEvent(ImGuiKey_Comma, Memory::IsKeyDown(VK_OEM_COMMA));
    io.AddKeyEvent(ImGuiKey_Minus, Memory::IsKeyDown(VK_OEM_MINUS));
    io.AddKeyEvent(ImGuiKey_Period, Memory::IsKeyDown(VK_OEM_PERIOD));
    io.AddKeyEvent(ImGuiKey_Slash, Memory::IsKeyDown(VK_OEM_2));
    io.AddKeyEvent(ImGuiKey_Semicolon, Memory::IsKeyDown(VK_OEM_1));
    io.AddKeyEvent(ImGuiKey_Equal, Memory::IsKeyDown(VK_OEM_PLUS));
    io.AddKeyEvent(ImGuiKey_LeftBracket, Memory::IsKeyDown(VK_OEM_4));
    io.AddKeyEvent(ImGuiKey_Backslash, Memory::IsKeyDown(VK_OEM_5));
    io.AddKeyEvent(ImGuiKey_RightBracket, Memory::IsKeyDown(VK_OEM_6));
    io.AddKeyEvent(ImGuiKey_GraveAccent, Memory::IsKeyDown(VK_OEM_3));
    io.AddKeyEvent(ImGuiKey_CapsLock, Memory::IsKeyDown(VK_CAPITAL));
    io.AddKeyEvent(ImGuiKey_ScrollLock, Memory::IsKeyDown(VK_SCROLL));
    io.AddKeyEvent(ImGuiKey_NumLock, Memory::IsKeyDown(VK_NUMLOCK));
    io.AddKeyEvent(ImGuiKey_PrintScreen, Memory::IsKeyDown(VK_SNAPSHOT));
    io.AddKeyEvent(ImGuiKey_Pause, Memory::IsKeyDown(VK_PAUSE));
    io.AddKeyEvent(ImGuiKey_Keypad0, Memory::IsKeyDown(VK_NUMPAD0));
    io.AddKeyEvent(ImGuiKey_Keypad1, Memory::IsKeyDown(VK_NUMPAD1));
    io.AddKeyEvent(ImGuiKey_Keypad2, Memory::IsKeyDown(VK_NUMPAD2));
    io.AddKeyEvent(ImGuiKey_Keypad3, Memory::IsKeyDown(VK_NUMPAD3));
    io.AddKeyEvent(ImGuiKey_Keypad4, Memory::IsKeyDown(VK_NUMPAD4));
    io.AddKeyEvent(ImGuiKey_Keypad5, Memory::IsKeyDown(VK_NUMPAD5));
    io.AddKeyEvent(ImGuiKey_Keypad6, Memory::IsKeyDown(VK_NUMPAD6));
    io.AddKeyEvent(ImGuiKey_Keypad7, Memory::IsKeyDown(VK_NUMPAD7));
    io.AddKeyEvent(ImGuiKey_Keypad8, Memory::IsKeyDown(VK_NUMPAD8));
    io.AddKeyEvent(ImGuiKey_Keypad9, Memory::IsKeyDown(VK_NUMPAD9));
    io.AddKeyEvent(ImGuiKey_KeypadDecimal, Memory::IsKeyDown(VK_DECIMAL));
    io.AddKeyEvent(ImGuiKey_KeypadDivide, Memory::IsKeyDown(VK_DIVIDE));
    io.AddKeyEvent(ImGuiKey_KeypadMultiply, Memory::IsKeyDown(VK_MULTIPLY));
    io.AddKeyEvent(ImGuiKey_KeypadSubtract, Memory::IsKeyDown(VK_SUBTRACT));
    io.AddKeyEvent(ImGuiKey_KeypadAdd, Memory::IsKeyDown(VK_ADD));
    io.AddKeyEvent(ImGuiKey_LeftShift, Memory::IsKeyDown(VK_LSHIFT));
    io.AddKeyEvent(ImGuiKey_LeftCtrl, Memory::IsKeyDown(VK_LCONTROL));
    io.AddKeyEvent(ImGuiKey_LeftAlt, Memory::IsKeyDown(VK_LMENU));
    io.AddKeyEvent(ImGuiKey_LeftSuper, Memory::IsKeyDown(VK_LWIN));
    io.AddKeyEvent(ImGuiKey_RightShift, Memory::IsKeyDown(VK_RSHIFT));
    io.AddKeyEvent(ImGuiKey_RightCtrl, Memory::IsKeyDown(VK_RCONTROL));
    io.AddKeyEvent(ImGuiKey_RightAlt, Memory::IsKeyDown(VK_RMENU));
    io.AddKeyEvent(ImGuiKey_RightSuper, Memory::IsKeyDown(VK_RWIN));
    io.AddKeyEvent(ImGuiKey_Menu, Memory::IsKeyDown(VK_APPS));
    io.AddKeyEvent(ImGuiKey_0, Memory::IsKeyDown('0'));
    io.AddKeyEvent(ImGuiKey_1, Memory::IsKeyDown('1'));
    io.AddKeyEvent(ImGuiKey_2, Memory::IsKeyDown('2'));
    io.AddKeyEvent(ImGuiKey_3, Memory::IsKeyDown('3'));
    io.AddKeyEvent(ImGuiKey_4, Memory::IsKeyDown('4'));
    io.AddKeyEvent(ImGuiKey_5, Memory::IsKeyDown('5'));
    io.AddKeyEvent(ImGuiKey_6, Memory::IsKeyDown('6'));
    io.AddKeyEvent(ImGuiKey_7, Memory::IsKeyDown('7'));
    io.AddKeyEvent(ImGuiKey_8, Memory::IsKeyDown('8'));
    io.AddKeyEvent(ImGuiKey_9, Memory::IsKeyDown('9'));
    io.AddKeyEvent(ImGuiKey_A, Memory::IsKeyDown('A'));
    io.AddKeyEvent(ImGuiKey_B, Memory::IsKeyDown('B'));
    io.AddKeyEvent(ImGuiKey_C, Memory::IsKeyDown('C'));
    io.AddKeyEvent(ImGuiKey_D, Memory::IsKeyDown('D'));
    io.AddKeyEvent(ImGuiKey_E, Memory::IsKeyDown('E'));
    io.AddKeyEvent(ImGuiKey_F, Memory::IsKeyDown('F'));
    io.AddKeyEvent(ImGuiKey_G, Memory::IsKeyDown('G'));
    io.AddKeyEvent(ImGuiKey_H, Memory::IsKeyDown('H'));
    io.AddKeyEvent(ImGuiKey_I, Memory::IsKeyDown('I'));
    io.AddKeyEvent(ImGuiKey_J, Memory::IsKeyDown('J'));
    io.AddKeyEvent(ImGuiKey_K, Memory::IsKeyDown('K'));
    io.AddKeyEvent(ImGuiKey_L, Memory::IsKeyDown('L'));
    io.AddKeyEvent(ImGuiKey_M, Memory::IsKeyDown('M'));
    io.AddKeyEvent(ImGuiKey_N, Memory::IsKeyDown('N'));
    io.AddKeyEvent(ImGuiKey_O, Memory::IsKeyDown('O'));
    io.AddKeyEvent(ImGuiKey_P, Memory::IsKeyDown('P'));
    io.AddKeyEvent(ImGuiKey_Q, Memory::IsKeyDown('Q'));
    io.AddKeyEvent(ImGuiKey_R, Memory::IsKeyDown('R'));
    io.AddKeyEvent(ImGuiKey_S, Memory::IsKeyDown('S'));
    io.AddKeyEvent(ImGuiKey_T, Memory::IsKeyDown('T'));
    io.AddKeyEvent(ImGuiKey_U, Memory::IsKeyDown('U'));
    io.AddKeyEvent(ImGuiKey_V, Memory::IsKeyDown('V'));
    io.AddKeyEvent(ImGuiKey_W, Memory::IsKeyDown('W'));
    io.AddKeyEvent(ImGuiKey_X, Memory::IsKeyDown('X'));
    io.AddKeyEvent(ImGuiKey_Y, Memory::IsKeyDown('Y'));
    io.AddKeyEvent(ImGuiKey_Z, Memory::IsKeyDown('Z'));
    io.AddKeyEvent(ImGuiKey_F1, Memory::IsKeyDown(VK_F1));
    io.AddKeyEvent(ImGuiKey_F2, Memory::IsKeyDown(VK_F2));
    io.AddKeyEvent(ImGuiKey_F3, Memory::IsKeyDown(VK_F3));
    io.AddKeyEvent(ImGuiKey_F4, Memory::IsKeyDown(VK_F4));
    io.AddKeyEvent(ImGuiKey_F5, Memory::IsKeyDown(VK_F5));
    io.AddKeyEvent(ImGuiKey_F6, Memory::IsKeyDown(VK_F6));
    io.AddKeyEvent(ImGuiKey_F7, Memory::IsKeyDown(VK_F7));
    io.AddKeyEvent(ImGuiKey_F8, Memory::IsKeyDown(VK_F8));
    io.AddKeyEvent(ImGuiKey_F9, Memory::IsKeyDown(VK_F9));
    io.AddKeyEvent(ImGuiKey_F10, Memory::IsKeyDown(VK_F10));
    io.AddKeyEvent(ImGuiKey_F11, Memory::IsKeyDown(VK_F11));
    io.AddKeyEvent(ImGuiKey_F12, Memory::IsKeyDown(VK_F12));

    ImGui_ImplDX11_NewFrame();
    ImGui::NewFrame();
}

void Render::RenderFrame()
{
    ImGui::Render();

    float clear[4] = { 0.f, 0.f, 0.f, 0.f };
    mD3DContext->ClearRenderTargetView(mRTView, clear);
    mD3DContext->OMSetRenderTargets(1, &mRTView, nullptr);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    mD3DContext->CopyResource(mStagingTex, mRenderTarget);

    D3D11_MAPPED_SUBRESOURCE mapped{};
    mD3DContext->Map(mStagingTex, 0, D3D11_MAP_READ, 0, &mapped);

    const char* src = static_cast<const char*>(mapped.pData);
    char* dst = static_cast<char*>(mFrame.CurrentBuffer);
    const UINT  rowSize = mFrame.Width * 4;

    for (uint32_t row = 0; row < mFrame.Height; row++)
        memcpy(dst + row * rowSize, src + row * mapped.RowPitch, rowSize);

    mD3DContext->Unmap(mStagingTex, 0);
}

void Render::EndFrame()
{
    /* dma sooooo slow :( */

    constexpr size_t kChunkSize = 32;

    const char* curr = static_cast<const char*>(mFrame.CurrentBuffer);
    const char* last = static_cast<const char*>(mFrame.CurrentBuffer == mFrame.Buffer1 ? mFrame.Buffer2 : mFrame.Buffer1);
    uintptr_t currPtr = reinterpret_cast<uintptr_t>(mFrame.CurrentBuffer);

    const size_t totalChunks = mFrame.Size / kChunkSize;

    static size_t dirtyOffsets[1920 * 1080 * 4 / 32];
    size_t dirtyCount = 0;

    for (size_t i = 0; i < totalChunks; i++)
    {
        const size_t offset = i * kChunkSize;

        if (memcmp(curr + offset, last + offset, kChunkSize) != 0)
            dirtyOffsets[dirtyCount++] = offset;
    }

    if (dirtyCount > 0)
    {
        size_t start = dirtyOffsets[0];
        size_t end = start + kChunkSize;

        for (size_t i = 1; i < dirtyCount; i++)
        {
            if (dirtyOffsets[i] == end)
            {
                end += kChunkSize;
            }
            else
            {
                Memory::AddScatterWrite(mHandle, mFrameBuffer + 20 + start, const_cast<char*>(curr) + start, end - start);
                start = dirtyOffsets[i];
                end = start + kChunkSize;
            }
        }

        Memory::AddScatterWrite(mHandle, mFrameBuffer + 20 + start, const_cast<char*>(curr) + start, end - start);

        int frameCount = Memory::Read<int>(mFrameBuffer + 4) + 1;
        Memory::AddScatterWrite(mHandle, mFrameBuffer + 12, &mFrame.Width, 4);
        Memory::AddScatterWrite(mHandle, mFrameBuffer + 16, &mFrame.Height, 4);
        Memory::AddScatterWrite(mHandle, mFrameBuffer + 4, &frameCount, 4);
        Memory::ExecuteScatterWrite(mHandle);
    }

    mFrame.CurrentBuffer = mFrame.CurrentBuffer == mFrame.Buffer1 ? mFrame.Buffer2 : mFrame.Buffer1;
    memset(mFrame.CurrentBuffer, 0, mFrame.Size);
}