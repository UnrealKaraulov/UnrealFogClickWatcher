#define _WIN32_WINNT 0x0501 
#define WINVER 0x0501 
#define NTDDI_VERSION 0x05010000
//#define BOTDEBUG
#define WIN32_LEAN_AND_MEAN
#define PSAPI_VERSION 1

#include <Windows.h>
#include <iostream>

typedef LRESULT(CALLBACK *ExternalLoader)(int nCode, WPARAM wParam, LPARAM lParam);

int main(void) {
	std::cout << "Close this windows only after close Warcraft III" << std::endl;
	HINSTANCE meowDll;
	ExternalLoader meowFunc;
	meowDll = LoadLibraryA("FogWatcherLibrary.dll");
	if (!meowDll)
	{
		MessageBoxA(0, "FogWatcherLibrary.dll not found", "Error", 0);
		return 0;
	}
	meowFunc = (ExternalLoader)GetProcAddress(meowDll, (const char *)1);
	if (!meowFunc)
	{
		MessageBoxA(0, "Invalid FogWatcherLibrary.dll", "Error", 0);
		return 0;
	}
	HHOOK hook = SetWindowsHookExW(WH_GETMESSAGE, meowFunc, meowDll, 0);

	std::cout << "Press any key to close Warcraft III and FogClick launcher" << std::endl;
	system("pause");
	// Uninstall the hook
	std::cout << "WARNING! Press any key to close Warcraft III and FogClick launcher" << std::endl;
	system("pause");
	UnhookWindowsHookEx(hook);
	return 0;
}