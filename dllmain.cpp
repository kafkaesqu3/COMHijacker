// dllmain.cpp : Defines the entry point for the DLL application.
#include <Windows.h>
#include <cstdio>
#include <detours.h>



//typedef functions to hook
typedef HRESULT(_fastcall * _CoCreateInstance)(REFCLSID  rclsid,
	LPUNKNOWN pUnkOuter,
	DWORD     dwClsContext,
	REFIID    riid,
	LPVOID    *ppv);


//GVars
bool runOnce = false;
EXTERN_C IMAGE_DOS_HEADER __ImageBase;

//Func Definitions
VOID OurThread();
_CoCreateInstance o_CoCreateInstance = CoCreateInstance;

//Helper hook function
BOOL hook_function(PVOID & t1, PBYTE t2, const char * s = NULL)
{
	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());
	DetourAttach(&t1, t2);
	if (DetourTransactionCommit() != NO_ERROR) {
		printf("[+] - Failed to hook %s.\n", s);
		return false;
	}
	else {
		printf("[+] - Successfully hooked %s.\n", s);
		return true;
	}
}

BOOL unhook_function(PVOID & t1, PBYTE t2, const char * s = NULL)
{
	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());
	DetourDetach(&(PVOID&)t1, t2);
	if (DetourTransactionCommit() != NO_ERROR) {
		printf("[+] - Failed to unhook %s.\n", s);
		return false;
	}
	else {
		printf("[+] - Successfully unhooked %s.\n", s);
		return true;
	}
}

//define hooked CreateInstance
HRESULT h_CoCreateInstance(REFCLSID  rclsid,
	LPUNKNOWN pUnkOuter,
	DWORD     dwClsContext,
	REFIID    riid,
	LPVOID    *ppv)
{
	if (!runOnce)
	{
		runOnce = !runOnce;
		printf("[+] - Spawning our thread...\n");
		CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)OurThread, 0, 0, nullptr);

		//Clean up the hook for more stealth.
		unhook_function((PVOID &)o_CoCreateInstance, (PBYTE)h_CoCreateInstance, "CoCreateInstance");
	}
	return o_CoCreateInstance(rclsid, pUnkOuter, dwClsContext, riid, ppv);
};

//CreateEvent to check Mutex/existing event
bool CheckOneInstance()
{
	HANDLE  m_hStartEvent = CreateEventW(NULL, FALSE, FALSE, L"Global\\hoangspecial");
	if (m_hStartEvent == NULL)
	{
		CloseHandle(m_hStartEvent);
		return false;
	}
	if (GetLastError() == ERROR_ALREADY_EXISTS) {

		CloseHandle(m_hStartEvent);
		m_hStartEvent = NULL;
		return false;
	}
	return true;
}

//Whatever we want goes in here
VOID OurThread()
{
	int a = 0;
	while (true)
	{
		printf("a = %d\n", a);
		++a;
		Sleep(1000);
	}

}


BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
		if (CheckOneInstance())
		{
			//Spawn Console
			AllocConsole();
			FILE *stream;
			freopen_s(&stream, "CONOUT$", "w+", stdout);


			//Make CoCreateInstance to start our thread for us, since we can't/don't have time to start it ourselves
			hook_function((PVOID &)o_CoCreateInstance, (PBYTE)h_CoCreateInstance, "CoCreateInstance");

			//Our DLL get unload right after this exit, making our hooked_cocreateinstance/OurThread to be invalid, it would then crash
			//due to original_CoCreateInstance trying to jump to our unloaded hooked_cocreateinstance


			//Get path to DLL
			WCHAR   Path2DLL[MAX_PATH] = { 0 };
			GetModuleFileNameW((HINSTANCE)&__ImageBase, Path2DLL, _countof(Path2DLL));

			//Doing this LoadLibraryA will let's us pull in the h_cocreateinstance/OurThread function. 
			//This can be fix if we manually write the function's bytecodes to a new alloc memory region, complicated
			//However, a process loading a DLL into itself is almost invisible because it is not malicious
			//So removing this isn't needed - would just be better
			LoadLibraryW(Path2DLL);
		}
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

