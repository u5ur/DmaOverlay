#include "Memory.h"

static bool LoadLibraries()
{
    if (!LoadLibraryA("leechcore.dll")) { return false; }
    if (!LoadLibraryA("FTD3XX.dll")) { return false; }
    if (!LoadLibraryA("vmm.dll")) { return false; }
    return true;
}

static bool SetFpga()
{
    ULONG64 verMajor = 0, verMinor = 0;
    VMMDLL_ConfigGet(Memory::Instance()->mHandle, LC_OPT_FPGA_VERSION_MAJOR, &verMajor);
    VMMDLL_ConfigGet(Memory::Instance()->mHandle, LC_OPT_FPGA_VERSION_MINOR, &verMinor);

    if (verMajor >= 4 && (verMajor > 4 || verMinor >= 7))
    {
        LC_CONFIG cfg{ .dwVersion = LC_CONFIG_VERSION, .szDevice = "existing" };
        HANDLE h = LcCreate(&cfg);
        if (!h) return false;

        static unsigned char abort2[4] = { 0x10, 0x00, 0x10, 0x00 };
        LcCommand(h, LC_CMD_FPGA_CFGREGPCIE_MARKWR | 0x002, 4, abort2, nullptr, nullptr);
        LcClose(h);
    }

    return true;
}

void Memory::UpdateInput()
{
    mWinLogonPid = GetPidByName("winlogon.exe");

    PVMMDLL_MAP_EAT eatMap = nullptr;
    if (!VMMDLL_Map_GetEATU(mHandle, mWinLogonPid | VMMDLL_PID_PROCESS_WITH_KERNELMEMORY,
        const_cast<LPSTR>("win32kbase.sys"), &eatMap))
        return;

    if (eatMap->dwVersion != VMMDLL_MAP_EAT_VERSION)
    {
        VMMDLL_MemFree(eatMap);
        return;
    }

    for (DWORD i = 0; i < eatMap->cMap; i++)
    {
        if (strcmp(eatMap->pMap[i].uszFunction, "gafAsyncKeyState") == 0)
        {
            mAsyncKeyState = eatMap->pMap[i].vaFunction;
            break;
        }
    }

    VMMDLL_MemFree(eatMap);

    mGptCursorAsync = VMMDLL_ProcessGetProcAddressU(
        mHandle,
        mWinLogonPid | VMMDLL_PID_PROCESS_WITH_KERNELMEMORY,
        "win32kbase.sys",
        "gptCursorAsync");

    while (true)
    {
        uint8_t prev[64]{};
        memcpy(prev, mKeyStateBitmap, 64);

        VMMDLL_MemReadEx(mHandle, mWinLogonPid | VMMDLL_PID_PROCESS_WITH_KERNELMEMORY,
            mAsyncKeyState, reinterpret_cast<PBYTE>(mKeyStateBitmap), 64,
            nullptr, VMMDLL_FLAG_NOCACHE);

        for (int vk = 0; vk < 256; vk++)
        {
            if ((mKeyStateBitmap[(vk * 2 / 8)] & 1 << vk % 4 * 2) &&
                !(prev[(vk * 2 / 8)] & 1 << vk % 4 * 2))
                mPrevKeyStateBitmap[vk / 8] |= 1 << vk % 8;
        }

        if (mGptCursorAsync)
        {
            POINT pt{};
            VMMDLL_MemReadEx(mHandle, mWinLogonPid | VMMDLL_PID_PROCESS_WITH_KERNELMEMORY,
                mGptCursorAsync, reinterpret_cast<PBYTE>(&pt), sizeof(POINT),
                nullptr, VMMDLL_FLAG_NOCACHE);

            mMousePos = { (float)pt.x, (float)pt.y };
        }

        Sleep(10);
    }
}

bool Memory::Start()
{
    if (!LoadLibraries())
        return false;

    LPCSTR args[] = { "", "-device", "fpga://algo=0" };
    mHandle = VMMDLL_Initialize(3, args);
    if (!mHandle)
        return false;

    SetFpga();

    std::thread(&Memory::UpdateInput, this).detach();
    return true;
}

bool Memory::Destroy()
{
    if (mHandle)
    {
        VMMDLL_CloseAll();
        mHandle = nullptr;
    }
    return true;
}

bool Memory::SetTargetProcess(const char* process_name)
{
    if (!mHandle || !process_name)
        return false;

    mProcessPid = static_cast<int>(GetPidByName(process_name));
    if (!mProcessPid)
        return false;

    mProcessBase = GetBaseByName(process_name);
    mProcessSize = GetSizeByName(process_name);

    VMMDLL_PROCESS_INFORMATION info{};
    SIZE_T infoSize = sizeof(info);
    info.magic = VMMDLL_PROCESS_INFORMATION_MAGIC;
    info.wVersion = VMMDLL_PROCESS_INFORMATION_VERSION;

    if (!VMMDLL_ProcessGetInformation(mHandle, mProcessPid, &info, &infoSize))
        return false;

    mProcessCr3 = info.paDTB;
    return true;
}

uint32_t Memory::GetPidByName(const char* process_name)
{
    if (!mHandle || !process_name)
        return 0;

    DWORD pid = 0;
    VMMDLL_PidGetFromName(mHandle, const_cast<LPSTR>(process_name), &pid);
    return static_cast<uint32_t>(pid);
}

uint64_t Memory::GetBaseByName(const char* process_name)
{
    if (!mHandle || !process_name)
        return 0;

    std::wstring wname(process_name, process_name + strlen(process_name));
    PVMMDLL_MAP_MODULEENTRY modInfo = nullptr;

    if (!VMMDLL_Map_GetModuleFromNameW(mHandle, mProcessPid,
        const_cast<LPWSTR>(wname.c_str()), &modInfo, VMMDLL_MODULE_FLAG_NORMAL))
        return 0;

    return modInfo->vaBase;
}

uint64_t Memory::GetSizeByName(const char* process_name)
{
    if (!mHandle || !process_name)
        return 0;

    std::wstring wname(process_name, process_name + strlen(process_name));
    PVMMDLL_MAP_MODULEENTRY modInfo = nullptr;

    if (!VMMDLL_Map_GetModuleFromNameW(mHandle, mProcessPid,
        const_cast<LPWSTR>(wname.c_str()), &modInfo, VMMDLL_MODULE_FLAG_NORMAL))
        return 0;

    return modInfo->cbImageSize;
}

VMM_HANDLE Memory::Handle()
{
    return Memory::Instance()->mHandle;
}

int Memory::ProcessPid() 
{
    return Memory::Instance()->mProcessPid; 
}

uint64_t Memory::ProcessBase() 
{ 
    return Memory::Instance()->mProcessBase; 
}

uint64_t Memory::ProcessSize() 
{ 
    return Memory::Instance()->mProcessSize; 
}

uint64_t Memory::ProcessCr3() 
{ 
    return Memory::Instance()->mProcessCr3; 
}

VMMDLL_SCATTER_HANDLE Memory::CreateScatterHandle()
{
    return VMMDLL_Scatter_Initialize(Memory::Handle(), Memory::ProcessPid(),
        VMMDLL_FLAG_NOCACHE | VMMDLL_FLAG_ZEROPAD_ON_FAIL);
}

void Memory::AddScatterRead(VMMDLL_SCATTER_HANDLE handle,
    uint64_t address, void* buffer, size_t size)
{
    VMMDLL_Scatter_PrepareEx(handle, address, static_cast<DWORD>(size),
        static_cast<PBYTE>(buffer), nullptr);
}

void Memory::ExecuteScatterRead(VMMDLL_SCATTER_HANDLE handle)
{
    VMMDLL_Scatter_ExecuteRead(handle);
    VMMDLL_Scatter_Clear(handle, Memory::ProcessPid(),
        VMMDLL_FLAG_NOCACHE | VMMDLL_FLAG_ZEROPAD_ON_FAIL);
}

void Memory::AddScatterWrite(VMMDLL_SCATTER_HANDLE handle, uint64_t address, void* buffer, size_t size)
{
    VMMDLL_Scatter_PrepareWrite(handle, address, static_cast<PBYTE>(buffer), static_cast<DWORD>(size));
}

void Memory::ExecuteScatterWrite(VMMDLL_SCATTER_HANDLE handle)
{
    VMMDLL_Scatter_Execute(handle);
    VMMDLL_Scatter_Clear(handle, Memory::ProcessPid(),
        VMMDLL_FLAG_NOCACHE | VMMDLL_FLAG_ZEROPAD_ON_FAIL);
}

void Memory::CloseScatterHandle(VMMDLL_SCATTER_HANDLE handle)
{
    VMMDLL_Scatter_CloseHandle(handle);
}

bool Memory::Read(uintptr_t address, void* buffer, size_t size)
{
    if (!buffer || !size || !Memory::Handle())
        return false;

    DWORD bytesRead = 0;
    return VMMDLL_MemReadEx(Memory::Handle(), Memory::ProcessPid(),
        address, static_cast<PBYTE>(buffer),
        static_cast<DWORD>(size), &bytesRead,
        VMMDLL_FLAG_NOCACHE | VMMDLL_FLAG_ZEROPAD_ON_FAIL)
        && bytesRead == size;
}

bool Memory::Write(uintptr_t address, void* buffer, size_t size)
{
    if (!buffer || !size || !Memory::Handle())
        return false;

    return VMMDLL_MemWrite(Memory::Handle(), Memory::ProcessPid(),
        address, static_cast<PBYTE>(buffer),
        static_cast<DWORD>(size));
}

bool Memory::IsKeyDown(uint32_t vkey)
{
    if (!Memory::Instance()->mAsyncKeyState)
        return false;

    return Memory::Instance()->mKeyStateBitmap[(vkey * 2 / 8)] & 1 << vkey % 4 * 2;
}

std::pair<float, float> Memory::GetMousePos()
{
    return Memory::Instance()->mMousePos;
}