// Memory.h
#pragma once
#include <Core/Common/Common.h>

#include <vmmdll.h>
#include <leechcore.h>

#pragma comment(lib, "leechcore.lib")
#pragma comment(lib, "vmm.lib")

class Memory
{
public:
	VMM_HANDLE mHandle;

private:

	/* target process */
	int		 mProcessPid;
	uint64_t mProcessBase;
	uint64_t mProcessSize;
	uint64_t mProcessCr3;

	/* input stuff */
	uint64_t mAsyncKeyState = 0;
	uint8_t  mKeyStateBitmap[64]{};
	uint8_t  mPrevKeyStateBitmap[256 / 8]{};
	int      mWinLogonPid = 0;

	uint64_t mGptCursorAsync = 0;
	std::pair<float, float> mMousePos = { 0.f, 0.f };

public:
	static Memory* Instance()
	{
		static Memory* memory = new Memory();
		return memory;
	}

	bool Start();
	bool Destroy();

	bool SetTargetProcess(const char* process_name);

	static int ProcessPid();
	static VMM_HANDLE Handle();
	static uint64_t ProcessBase();
	static uint64_t ProcessSize();
	static uint64_t ProcessCr3();

	uint32_t GetPidByName(const char* process_name);
	uint64_t GetBaseByName(const char* process_name);
	uint64_t GetSizeByName(const char* process_name);

	static VMMDLL_SCATTER_HANDLE CreateScatterHandle();

	void UpdateInput();
	static bool IsKeyDown(uint32_t vkey);
	static std::pair<float, float> GetMousePos();

	static void AddScatterRead(VMMDLL_SCATTER_HANDLE handle, uint64_t address, void* buffer, size_t size);

	template <typename T>
	static void AddScatterRead(VMMDLL_SCATTER_HANDLE handle, uintptr_t address, T* out)
	{
		AddScatterRead(handle, address, out, sizeof(T));
	}

	static void AddScatterWrite(VMMDLL_SCATTER_HANDLE handle, uint64_t address, void* buffer, size_t size);

	template <typename T>
	static void AddScatterWrite(VMMDLL_SCATTER_HANDLE handle, uint64_t address, T* value)
	{
		AddScatterWrite(handle, address, value, sizeof(T));
	}

	static void ExecuteScatterWrite(VMMDLL_SCATTER_HANDLE handle);

	static void ExecuteScatterRead(VMMDLL_SCATTER_HANDLE handle);

	static void CloseScatterHandle(VMMDLL_SCATTER_HANDLE handle);


	static bool Read(uintptr_t address, void* buffer, size_t size);

	template <typename T>
	__forceinline static T Read(void* address)
	{
		T buffer{ };
		memset(&buffer, 0, sizeof(T));
		Read(reinterpret_cast<uint64_t>(address), reinterpret_cast<void*>(&buffer), sizeof(T));

		return buffer;
	}

	template <typename T>
	__forceinline static T Read(uint64_t address)
	{
		return Read<T>(reinterpret_cast<void*>(address));
	}

	static bool Write(uintptr_t address, void* buffer, size_t size);

	template <typename T>
	__forceinline static void Write(void* address, T value)
	{
		Write(address, &value, sizeof(T));
	}

	template <typename T>
	__forceinline static void Write(uintptr_t address, T value)
	{
		Write(address, &value, sizeof(T));
	}
};