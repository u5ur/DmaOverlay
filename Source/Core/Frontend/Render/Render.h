// Render.h
#pragma once
#include <Core/Common/Memory/Memory.h>

#include <d3d11.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

class Render
{
private:
    struct Frame
    {
        void* Buffer1 = nullptr;
        void* Buffer2 = nullptr;
        void* CurrentBuffer = nullptr;
        uint32_t Width = 0;
        uint32_t Height = 0;
        size_t Size = 0;
    };

private:
    uint64_t mDiscordBase = 0;
    uint64_t mFrameBuffer = 0;
    VMMDLL_SCATTER_HANDLE mHandle = nullptr;
    Frame mFrame{};

    ID3D11Device* mD3DDevice = nullptr;
    ID3D11DeviceContext* mD3DContext = nullptr;
    ID3D11Texture2D* mRenderTarget = nullptr;
    ID3D11Texture2D* mStagingTex = nullptr;
    ID3D11RenderTargetView* mRTView = nullptr;

public:
    static Render* Instance()
    {
        static Render* memory = new Render();
        return memory;
    }

    bool Start();
    void Destroy();

public:
    void BeginFrame();
    void RenderFrame();
    void EndFrame();
};