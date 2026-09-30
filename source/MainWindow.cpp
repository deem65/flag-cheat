#include "MainWindow.h"
#include "Clipboard.h"
#include "WindowsSupport.h"

#include <CommCtrl.h>
#include <Uxtheme.h>

#include <algorithm>
#include <iomanip>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <utility>

#include "DiscordDrop.h"

#include <ole2.h>

#pragma comment(lib, "UxTheme.lib")

namespace drek_flag_cheat {
    namespace {
        constexpr int pasteCommand = 100;
        constexpr int copyCommand = 101;
        constexpr int clearCommand = 102;
        constexpr int candidateCommand = 103;

        constexpr wchar_t windowClassName[] = L"drek_flag_cheat.MainWindow";

        constexpr COLORREF backgroundColor =
            RGB(18, 18, 18);

        constexpr COLORREF surfaceColor =
            RGB(28, 28, 28);

        constexpr COLORREF borderColor =
            RGB(58, 58, 58);

        constexpr COLORREF textColor =
            RGB(235, 235, 235);

        constexpr COLORREF mutedTextColor =
            RGB(145, 145, 145);

        HBRUSH BackgroundBrush() {
            static HBRUSH brush =
                CreateSolidBrush(backgroundColor);

            return brush;
        }

        HBRUSH SurfaceBrush() {
            static HBRUSH brush =
                CreateSolidBrush(surfaceColor);

            return brush;
        }

        HBRUSH BorderBrush() {
            static HBRUSH brush =
                CreateSolidBrush(borderColor);

            return brush;
        }

        HFONT CreateUiFont(UINT dpi, int points, int weight) {
            return CreateFontW(
                -MulDiv(points, static_cast<int>(dpi), 72),
                0,
                0,
                0,
                weight,
                FALSE,
                FALSE,
                FALSE,
                DEFAULT_CHARSET,
                OUT_DEFAULT_PRECIS,
                CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY,
                DEFAULT_PITCH | FF_DONTCARE,
                L"Consolas"
            );
        }
        

        void ApplyDarkTheme(HWND control) {
            if (!control) {
                return;
            }

            SetWindowTheme(
                control,
                L"DarkMode_Explorer",
                nullptr
            );
        }

    } 


    MainWindow::MainWindow(HINSTANCE instance)
        : instance_(instance) {
    }


    MainWindow::~MainWindow() {
        worker_.Stop();

        if (accelerators_) {
            DestroyAcceleratorTable(accelerators_);
        }

        if (bodyFont_) {
            DeleteObject(bodyFont_);
        }

        if (titleFont_) {
            DeleteObject(titleFont_);
        }

        if (resultFont_) {
            DeleteObject(resultFont_);
        }
    }


    int MainWindow::Run(int showCommand) {
        INITCOMMONCONTROLSEX controls{
            sizeof(controls),
            ICC_STANDARD_CLASSES
        };

        InitCommonControlsEx(&controls);

        WNDCLASSEXW windowClass{};

        windowClass.cbSize = sizeof(windowClass);
        windowClass.lpfnWndProc = WindowProcedure;
        windowClass.hInstance = instance_;
        windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        windowClass.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
        windowClass.hIconSm = windowClass.hIcon;

        windowClass.hbrBackground = BackgroundBrush();

        windowClass.lpszClassName = windowClassName;

        if (!RegisterClassExW(&windowClass)) {
            throw std::runtime_error(
                "Could not register the application window."
            );
        }

        dpi_ = GetDpiForSystem();

        RECT rectangle{
            0,
            0,
            Scale(720),
            Scale(700)
        };

        AdjustWindowRectExForDpi(
            &rectangle,
            WS_OVERLAPPEDWINDOW,
            FALSE,
            WS_EX_CONTROLPARENT,
            dpi_
        );

        window_ = CreateWindowExW(
            WS_EX_CONTROLPARENT,
            windowClassName,
            L"FlagCheat // by deem",
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            rectangle.right - rectangle.left,
            rectangle.bottom - rectangle.top,
            nullptr,
            nullptr,
            instance_,
            this
        );

        if (!window_) {
            throw std::runtime_error(
                "Could not create the application window."
            );
        }

        ACCEL shortcuts[] = {
            { FVIRTKEY | FCONTROL, 'V', pasteCommand },
            { FVIRTKEY | FCONTROL, 'C', copyCommand },
            { FVIRTKEY, VK_ESCAPE, clearCommand }
        };

        accelerators_ = CreateAcceleratorTableW(
            shortcuts,
            static_cast<int>(std::size(shortcuts))
        );

        ShowWindow(window_, showCommand);
        UpdateWindow(window_);

        MSG message{};
        int status = 0;

        while (
            (status = GetMessageW(
                &message,
                nullptr,
                0,
                0
            )) > 0
            ) {
            if (
                TranslateAcceleratorW(
                    window_,
                    accelerators_,
                    &message
                )
                ||
                IsDialogMessageW(
                    window_,
                    &message
                )
                ) {
                continue;
            }

            TranslateMessage(&message);
            DispatchMessageW(&message);
        }

        if (status == -1) {
            throw std::runtime_error(
                "Windows could not read the next application message."
            );
        }

        return static_cast<int>(message.wParam);
    }


    LRESULT CALLBACK MainWindow::WindowProcedure(
        HWND window,
        UINT message,
        WPARAM wParam,
        LPARAM lParam
    ) {
        auto* self =
            reinterpret_cast<MainWindow*>(
                GetWindowLongPtrW(
                    window,
                    GWLP_USERDATA
                )
                );

        if (message == WM_NCCREATE) {
            const auto* creation =
                reinterpret_cast<CREATESTRUCTW*>(
                    lParam
                    );

            self =
                static_cast<MainWindow*>(
                    creation->lpCreateParams
                    );

            self->window_ = window;

            SetWindowLongPtrW(
                window,
                GWLP_USERDATA,
                reinterpret_cast<LONG_PTR>(self)
            );
        }

        if (!self) {
            return DefWindowProcW(
                window,
                message,
                wParam,
                lParam
            );
        }

        try {
            return self->HandleMessage(
                message,
                wParam,
                lParam
            );
        }
        catch (const std::exception& error) {
            if (message == WM_CREATE) {
                MessageBoxW(
                    window,
                    Utf8ToWide(error.what()).c_str(),
                    L"drek_flag_cheat",
                    MB_OK | MB_ICONERROR
                );

                return -1;
            }

            self->SetStatus(
                Utf8ToWide(error.what())
            );

            return 0;
        }
    }


    LRESULT MainWindow::HandleMessage(
        UINT message,
        WPARAM wParam,
        LPARAM lParam
    ) {
        switch (message) {

        case WM_CREATE:
            dpi_ = GetDpiForWindow(window_);

            CreateControls();
            UpdateFonts();
            Layout();

            RegisterDiscordDrop(
                window_,
                [this](ClipboardImage image) {
                    if (!ready_) {
                        throw std::runtime_error(
                            "Reference flags are still loading."
                        );
                    }

                    Clear();

                    SetStatus(
                        image.imageUrl.empty()
                        ? L"Recognizing..."
                        : L"Downloading flag..."
                    );

                    worker_.Submit(
                        requestId_,
                        std::move(image)
                    );
                },
                [this](const std::string& error) {
                    Clear();

                    SetStatus(
                        Utf8ToWide(error)
                    );
                }
            );

            worker_.Start(
                window_,
                ExecutableDirectory()
            );

            return 0;


        case WM_COMMAND:
            switch (LOWORD(wParam)) {

            case pasteCommand:
                Paste();
                return 0;

            case copyCommand:
                Copy();
                return 0;

            case clearCommand:
                Clear();
                return 0;

            case candidateCommand:
                if (HIWORD(wParam) == LBN_SELCHANGE) {
                    SelectCandidate();
                }

                return 0;
            }

            break;


        case recognitionEventMessage:
            ReceiveWorkerEvents();
            return 0;


        case WM_SIZE:
            if (wParam != SIZE_MINIMIZED) {
                Layout();
            }

            return 0;


        case WM_PAINT:
            Paint();
            return 0;


        case WM_DPICHANGED: {
            dpi_ = HIWORD(wParam);

            UpdateFonts();

            const auto* suggested =
                reinterpret_cast<RECT*>(lParam);

            SetWindowPos(
                window_,
                nullptr,
                suggested->left,
                suggested->top,
                suggested->right - suggested->left,
                suggested->bottom - suggested->top,
                SWP_NOZORDER | SWP_NOACTIVATE
            );

            Layout();

            return 0;
        }


        case WM_GETMINMAXINFO: {
            RECT minimum{
                0,
                0,
                Scale(600),
                Scale(640)
            };

            AdjustWindowRectExForDpi(
                &minimum,
                WS_OVERLAPPEDWINDOW,
                FALSE,
                WS_EX_CONTROLPARENT,
                dpi_
            );

            auto* information =
                reinterpret_cast<MINMAXINFO*>(
                    lParam
                    );

            information->ptMinTrackSize = {
                minimum.right - minimum.left,
                minimum.bottom - minimum.top
            };

            return 0;
        }


        case WM_CTLCOLORSTATIC: {
            HDC dc =
                reinterpret_cast<HDC>(
                    wParam
                    );

            SetTextColor(
                dc,
                textColor
            );

            SetBkColor(
                dc,
                backgroundColor
            );

            SetBkMode(
                dc,
                TRANSPARENT
            );

            return reinterpret_cast<LRESULT>(
                BackgroundBrush()
                );
        }


        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORLISTBOX: {
            HDC dc =
                reinterpret_cast<HDC>(
                    wParam
                    );

            SetTextColor(
                dc,
                textColor
            );

            SetBkColor(
                dc,
                surfaceColor
            );

            return reinterpret_cast<LRESULT>(
                SurfaceBrush()
                );
        }


        case WM_ERASEBKGND: {
            HDC dc =
                reinterpret_cast<HDC>(
                    wParam
                    );

            RECT client{};
            GetClientRect(
                window_,
                &client
            );

            FillRect(
                dc,
                &client,
                BackgroundBrush()
            );

            return 1;
        }


        case WM_DESTROY:
            RevokeDragDrop(window_);

            worker_.Stop();

            PostQuitMessage(0);

            return 0;
        }

        return DefWindowProcW(
            window_,
            message,
            wParam,
            lParam
        );
    }


    HWND MainWindow::AddControl(
        const wchar_t* type,
        const wchar_t* text,
        DWORD style,
        int id
    ) {
        HWND control =
            CreateWindowExW(
                0,
                type,
                text,
                WS_CHILD |
                WS_VISIBLE |
                style,
                0,
                0,
                1,
                1,
                window_,
                reinterpret_cast<HMENU>(
                    static_cast<INT_PTR>(id)
                    ),
                instance_,
                nullptr
            );

        if (!control) {
            throw std::runtime_error(
                "Could not create an interface control."
            );
        }

        return control;
    }


    void MainWindow::CreateControls() {
        title_ = AddControl(
            L"STATIC",
            L"Flag Cheat",
            SS_CENTER
        );


        resultLabel_ = AddControl(
            L"STATIC",
            L"country / territory",
            SS_LEFT
        );

        resultField_ = AddControl(
            L"EDIT",
            L"",
            WS_TABSTOP |
            WS_BORDER |
            ES_READONLY |
            ES_AUTOHSCROLL
        );

        copyButton_ = AddControl(
            L"BUTTON",
            L"Copy",
            WS_TABSTOP | BS_PUSHBUTTON,
            copyCommand
        );

        statusField_ = AddControl(
            L"STATIC",
            L"Loading reference flags...",
            SS_LEFT
        );

        candidateLabel_ = AddControl(
            L"STATIC",
            L"Possible matches",
            SS_LEFT
        );

        candidateList_ = AddControl(
            L"LISTBOX",
            L"",
            WS_TABSTOP |
            WS_BORDER |
            WS_VSCROLL |
            LBS_NOTIFY |
            LBS_NOINTEGRALHEIGHT |
            LBS_HASSTRINGS,
            candidateCommand
        );

        shortcutLabel_ = AddControl(
            L"STATIC",
            L"Ctrl+V  paste     Ctrl+C  copy     Esc  clear",
            SS_LEFT
        );

        // Apply Windows' dark control theme.
        ApplyDarkTheme(pasteButton_);
        ApplyDarkTheme(copyButton_);
        ApplyDarkTheme(resultField_);
        ApplyDarkTheme(candidateList_);

        SendMessageW(
            resultField_,
            EM_SETMARGINS,
            EC_LEFTMARGIN | EC_RIGHTMARGIN,
            MAKELPARAM(8, 8)
        );

        EnableWindow(
            pasteButton_,
            FALSE
        );

        EnableWindow(
            copyButton_,
            FALSE
        );

        ShowWindow(
            candidateList_,
            SW_HIDE
        );

        ShowWindow(
            candidateLabel_,
            SW_HIDE
        );
    }


    void MainWindow::UpdateFonts() {
        const HFONT oldBody = bodyFont_;
        const HFONT oldTitle = titleFont_;
        const HFONT oldResult = resultFont_;

        bodyFont_ =
            CreateUiFont(
                dpi_,
                11,
                FW_NORMAL
            );

        titleFont_ =
            CreateUiFont(
                dpi_,
                22,
                FW_SEMIBOLD
            );

        resultFont_ =
            CreateUiFont(
                dpi_,
                24,
                FW_SEMIBOLD
            );

        for (
            HWND control :
        {
            hint_,
                pasteButton_,
                resultLabel_,
                copyButton_,
                statusField_,
                candidateLabel_,
                candidateList_,
                shortcutLabel_
        }
            ) {
            if (control) {
                SendMessageW(
                    control,
                    WM_SETFONT,
                    reinterpret_cast<WPARAM>(
                        bodyFont_
                        ),
                    TRUE
                );
            }
        }

        SendMessageW(
            title_,
            WM_SETFONT,
            reinterpret_cast<WPARAM>(
                titleFont_
                ),
            TRUE
        );

        SendMessageW(
            resultField_,
            WM_SETFONT,
            reinterpret_cast<WPARAM>(
                resultFont_
                ),
            TRUE
        );

        if (oldBody) {
            DeleteObject(oldBody);
        }

        if (oldTitle) {
            DeleteObject(oldTitle);
        }

        if (oldResult) {
            DeleteObject(oldResult);
        }
    }


    int MainWindow::Scale(int value) const {
        return MulDiv(
            value,
            static_cast<int>(dpi_),
            96
        );
    }


    void MainWindow::Layout() {
        if (!title_) {
            return;
        }

        RECT client{};

        GetClientRect(
            window_,
            &client
        );

        const int width =
            client.right;

        const int height =
            client.bottom;

        const int margin =
            Scale(24);

        const auto place =
            [](
                HWND control,
                int x,
                int y,
                int w,
                int h
                ) {
                    if (!control) {
                        return;
                    }

                    MoveWindow(
                        control,
                        x,
                        y,
                        std::max(1, w),
                        std::max(1, h),
                        TRUE
                    );
            };

        place(title_, margin, Scale(18), width - margin * 2, Scale(40));

        place(
            hint_,
            margin,
            Scale(62),
            width - margin * 2,
            Scale(25)
        );

        place(
            pasteButton_,
            width - margin - Scale(140),
            Scale(25),
            Scale(140),
            Scale(38)
        );

        const int previewHeight =
            std::clamp(
                height - Scale(470),
                Scale(170),
                Scale(270)
            );

        previewRectangle_ = {
            margin,
            Scale(62),
            width - margin,
            Scale(62) + previewHeight
        };

        const int below =
            previewRectangle_.bottom;

        place(
            resultLabel_,
            margin,
            below + Scale(17),
            width - margin * 2,
            Scale(22)
        );

        place(
            resultField_,
            margin,
            below + Scale(43),
            width - margin * 2 - Scale(122),
            Scale(50)
        );

        place(
            copyButton_,
            width - margin - Scale(110),
            below + Scale(43),
            Scale(110),
            Scale(50)
        );

        place(
            statusField_,
            margin,
            below + Scale(107),
            width - margin * 2,
            Scale(48)
        );

        place(
            candidateLabel_,
            margin,
            below + Scale(160),
            width - margin * 2,
            Scale(22)
        );

        place(
            candidateList_,
            margin,
            below + Scale(187),
            width - margin * 2,
            height - below - Scale(239)
        );

        place(
            shortcutLabel_,
            margin,
            height - Scale(32),
            width - margin * 2,
            Scale(22)
        );

        InvalidateRect(
            window_,
            nullptr,
            TRUE
        );
    }


    void MainWindow::Paint() {
        PAINTSTRUCT paint{};

        HDC dc =
            BeginPaint(
                window_,
                &paint
            );

        // Ensure the window itself is dark.
        RECT client{};

        GetClientRect(
            window_,
            &client
        );

        FillRect(
            dc,
            &client,
            BackgroundBrush()
        );

        // Dark preview area.
        FillRect(
            dc,
            &previewRectangle_,
            SurfaceBrush()
        );

        FrameRect(
            dc,
            &previewRectangle_,
            BorderBrush()
        );

        if (preview_.pixels.empty()) {
            RECT label =
                previewRectangle_;

            const auto previousFont =
                SelectObject(
                    dc,
                    bodyFont_
                );

            SetTextColor(
                dc,
                mutedTextColor
            );

            SetBkMode(
                dc,
                TRANSPARENT
            );

            DrawTextW(
                dc,
                L"paste or drop, cheater",
                -1,
                &label,
                DT_CENTER |
                DT_VCENTER |
                DT_SINGLELINE
            );

            SelectObject(
                dc,
                previousFont
            );

            EndPaint(
                window_,
                &paint
            );

            return;
        }

        const int availableWidth =
            previewRectangle_.right -
            previewRectangle_.left -
            Scale(20);

        const int availableHeight =
            previewRectangle_.bottom -
            previewRectangle_.top -
            Scale(20);

        const double scale =
            std::min(
                static_cast<double>(
                    availableWidth
                    ) / preview_.width,
                static_cast<double>(
                    availableHeight
                    ) / preview_.height
            );

        const int drawWidth =
            std::max(
                1,
                static_cast<int>(
                    preview_.width * scale
                    )
            );

        const int drawHeight =
            std::max(
                1,
                static_cast<int>(
                    preview_.height * scale
                    )
            );

        const int left =
            (
                previewRectangle_.left +
                previewRectangle_.right -
                drawWidth
                ) / 2;

        const int top =
            (
                previewRectangle_.top +
                previewRectangle_.bottom -
                drawHeight
                ) / 2;

        BITMAPINFO format{};

        format.bmiHeader.biSize =
            sizeof(BITMAPINFOHEADER);

        format.bmiHeader.biWidth =
            preview_.width;

        format.bmiHeader.biHeight =
            -preview_.height;

        format.bmiHeader.biPlanes =
            1;

        format.bmiHeader.biBitCount =
            32;

        format.bmiHeader.biCompression =
            BI_RGB;

        SetStretchBltMode(
            dc,
            HALFTONE
        );

        SetBrushOrgEx(
            dc,
            0,
            0,
            nullptr
        );

        StretchDIBits(
            dc,
            left,
            top,
            drawWidth,
            drawHeight,
            0,
            0,
            preview_.width,
            preview_.height,
            preview_.pixels.data(),
            &format,
            DIB_RGB_COLORS,
            SRCCOPY
        );

        EndPaint(
            window_,
            &paint
        );
    }


    void MainWindow::SetStatus(
        const std::wstring& text
    ) {
        if (statusField_) {
            SetWindowTextW(
                statusField_,
                text.c_str()
            );
        }
    }


    void MainWindow::Clear() {
        // Invalidates a result still in flight,
        // even if no new paste follows.
        ++requestId_;

        answer_.clear();

        preview_ = {};
        recognition_ = {};

        SetWindowTextW(
            resultField_,
            L""
        );

        EnableWindow(
            copyButton_,
            FALSE
        );

        SendMessageW(
            candidateList_,
            LB_RESETCONTENT,
            0,
            0
        );

        ShowWindow(
            candidateList_,
            SW_HIDE
        );

        ShowWindow(
            candidateLabel_,
            SW_HIDE
        );

        SetStatus(
            ready_
            ? L"paste a flag image."
            : L"reference flags are not ready yet."
        );

        InvalidateRect(
            window_,
            &previewRectangle_,
            TRUE
        );
    }


    void MainWindow::Paste() {
        if (!ready_) {
            SetStatus(
                L"reference flags are not ready yet. Check the startup message."
            );

            return;
        }

        // Clear first so a failed paste cannot
        // leave an old answer available to copy.
        Clear();

        auto image =
            ReadClipboardImage(
                window_
            );

        SetStatus(
            L"recognizing..."
        );

        worker_.Submit(
            requestId_,
            std::move(image)
        );
    }


    void MainWindow::Copy() {
        if (answer_.empty()) {
            return;
        }

        CopyText(
            window_,
            answer_
        );
    }


    void MainWindow::ReceiveWorkerEvents() {
        for (
            auto& event :
            worker_.TakeEvents()
            ) {
            if (
                event.kind ==
                WorkerEvent::Kind::Ready
                ) {
                ready_ = true;

                referenceCount_ =
                    event.referenceCount;

                EnableWindow(
                    pasteButton_,
                    TRUE
                );

                SetStatus(
                    L"Ready. " +
                    std::to_wstring(
                        referenceCount_
                    ) +
                    L" reference flags loaded."
                );

                continue;
            }

            if (
                event.requestId != 0 &&
                event.requestId != requestId_
                ) {
                continue;
            }

            if (
                event.kind ==
                WorkerEvent::Kind::Error
                ) {
                if (event.requestId == 0) {
                    ready_ = false;

                    EnableWindow(
                        pasteButton_,
                        FALSE
                    );
                }

                SetStatus(
                    Utf8ToWide(
                        event.error
                    )
                );

                continue;
            }

            ShowResult(
                std::move(event)
            );
        }
    }


    void MainWindow::ShowResult(
        WorkerEvent event
    ) {
        preview_ =
            std::move(event.preview);

        recognition_ =
            std::move(event.recognition);

        std::wostringstream timing;

        timing
            << std::fixed
            << std::setprecision(1)
            << event.elapsedMilliseconds
            << L" ms";

        const bool uncertain =
            recognition_.status ==
            MatchStatus::Uncertain;

        if (!uncertain) {
            answer_ =
                LowercaseInvariant(
                    Utf8ToWide(
                        recognition_
                        .candidates
                        .front()
                        .name
                    )
                );

            SetWindowTextW(
                resultField_,
                answer_.c_str()
            );

            EnableWindow(
                copyButton_,
                TRUE
            );
        }

        if (
            recognition_.status ==
            MatchStatus::Match
            ) {
            SetStatus(
                L"Match in " +
                timing.str() +
                L"."
            );
        }
        else if (
            recognition_.status ==
            MatchStatus::Ambiguous
            ) {
            SetStatus(
                L"Ambiguous in " +
                timing.str()
            );
        }
        else {
            SetStatus(
                L"Uncertain in " +
                timing.str()
            );
        }

        const bool showCandidates =
            recognition_.status !=
            MatchStatus::Match;

        ShowWindow(
            candidateLabel_,
            showCandidates
            ? SW_SHOW
            : SW_HIDE
        );

        ShowWindow(
            candidateList_,
            showCandidates
            ? SW_SHOW
            : SW_HIDE
        );

        if (showCandidates) {
            SetWindowTextW(
                candidateLabel_,
                uncertain
                ? L"Suggestions"
                : L"Possible matches"
            );

            for (
                const auto& candidate :
                recognition_.candidates
                ) {
                const auto label =
                    LowercaseInvariant(
                        Utf8ToWide(
                            candidate.name
                        )
                    )
                    +
                    L"  ["
                    +
                    Utf8ToWide(
                        candidate.code
                    )
                    +
                    L"]";

                SendMessageW(
                    candidateList_,
                    LB_ADDSTRING,
                    0,
                    reinterpret_cast<LPARAM>(
                        label.c_str()
                        )
                );
            }

            if (!uncertain) {
                SendMessageW(
                    candidateList_,
                    LB_SETCURSEL,
                    0,
                    0
                );
            }
        }

        InvalidateRect(
            window_,
            &previewRectangle_,
            TRUE
        );
    }


    void MainWindow::SelectCandidate() {
        const auto selected =
            SendMessageW(
                candidateList_,
                LB_GETCURSEL,
                0,
                0
            );

        if (
            selected == LB_ERR ||
            static_cast<std::size_t>(
                selected
                ) >=
            recognition_.candidates.size()
            ) {
            return;
        }

        answer_ =
            LowercaseInvariant(
                Utf8ToWide(
                    recognition_
                    .candidates[
                        static_cast<std::size_t>(
                            selected
                            )
                    ]
                    .name
                )
            );

        SetWindowTextW(
            resultField_,
            answer_.c_str()
        );

        EnableWindow(
            copyButton_,
            TRUE
        );

        SetStatus(
            L"Possible match selected. The image alone may not distinguish these flags."
        );
    }

} 
