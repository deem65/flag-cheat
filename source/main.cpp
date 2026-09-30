#include "MainWindow.h"
#include "WindowsSupport.h"

#include <Windows.h>
#include <ole2.h>

#include <exception>

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand) {
    const HRESULT initialized = OleInitialize(nullptr);
    if (FAILED(initialized)) {
        MessageBoxW(nullptr, L"Could not initialize drag-and-drop.",
            L"drek_flag_cheat", MB_OK | MB_ICONERROR);
        return 1;
    }

    int exitCode = 1;
    try {
        drek_flag_cheat::MainWindow window(instance);
        exitCode = window.Run(showCommand);
    }
    catch (const std::exception& error) {
        MessageBoxW(nullptr, drek_flag_cheat::Utf8ToWide(error.what()).c_str(),
            L"drek_flag_cheat", MB_OK | MB_ICONERROR);
    }

    OleUninitialize();
    return exitCode;
}
