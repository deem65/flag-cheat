#pragma once

#include "RecognitionWorker.h"

#include <Windows.h>

#include <cstdint>
#include <string>

namespace drek_flag_cheat {

class MainWindow {
public:
    explicit MainWindow(HINSTANCE instance);
    ~MainWindow();
    int Run(int showCommand);

private:
    static LRESULT CALLBACK WindowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);
    HWND AddControl(const wchar_t* type, const wchar_t* text, DWORD style, int id = 0);
    void CreateControls();
    void UpdateFonts();
    void Layout();
    void Paint();
    void Paste();
    void Copy();
    void Clear();
    void ReceiveWorkerEvents();
    void ShowResult(WorkerEvent event);
    void SelectCandidate();
    void SetStatus(const std::wstring& text);
    int Scale(int value) const;

    HINSTANCE instance_;
    HWND window_ = nullptr;
    HWND title_ = nullptr;
    HWND hint_ = nullptr;
    HWND pasteButton_ = nullptr;
    HWND resultLabel_ = nullptr;
    HWND resultField_ = nullptr;
    HWND copyButton_ = nullptr;
    HWND statusField_ = nullptr;
    HWND candidateLabel_ = nullptr;
    HWND candidateList_ = nullptr;
    HWND shortcutLabel_ = nullptr;
    HFONT bodyFont_ = nullptr;
    HFONT titleFont_ = nullptr;
    HFONT resultFont_ = nullptr;
    HACCEL accelerators_ = nullptr;
    UINT dpi_ = 96;
    RECT previewRectangle_{};
    RecognitionWorker worker_;
    bool ready_ = false;
    std::size_t referenceCount_ = 0;
    std::uint64_t requestId_ = 0;
    Image preview_;
    Recognition recognition_;
    std::wstring answer_;
};

} 
