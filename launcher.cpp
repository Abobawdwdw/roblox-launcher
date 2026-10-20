// ============================================================
// ROBLOX LAUNCHER v1.0
// ============================================================
// Сборка MSVC:
//   cl /O2 /EHsc /std:c++17 launcher.cpp comctl32.lib gdi32.lib user32.lib shlwapi.lib /Fe:launcher.exe /link /SUBSYSTEM:WINDOWS
// ============================================================

#define _WIN32_WINNT 0x0601
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shlwapi.h>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <map>
#include <algorithm>
#include <cstdio>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

// ============================================================
// КОНСТАНТЫ
// ============================================================
#define WINDOW_CLASS    L"RobloxLauncherClass"
#define WINDOW_TITLE    L"Roblox Launcher"
#define WINDOW_W        900
#define WINDOW_H        600

enum {
    ID_TAB_PLAY = 100,
    ID_TAB_SETTINGS,
    ID_TAB_ABOUT,
    ID_BTN_PLAY,
    ID_BTN_FOLDER,
    ID_BTN_SAVE,
    ID_BTN_RESET,
    ID_EDIT_PATH,
    ID_EDIT_ARGS,
    ID_EDIT_FPS,
    ID_COMBO_THEME,
    ID_COMBO_RESOLUTION,
    ID_CHECK_FULLSCREEN,
    ID_CHECK_CONSOLE,
    ID_CHECK_MULTIINSTANCE,
    ID_STATIC_STATUS,
    ID_STATIC_TITLE,
    ID_STATIC_VERSION,
    ID_STATIC_INFO,
};

// ============================================================
// НАСТРОЙКИ
// ============================================================
struct Settings {
    std::wstring robloxPath;
    std::wstring args;
    int fps = 60;
    int theme = 0;
    int resolution = 0;
    bool fullscreen = false;
    bool showConsole = false;
    bool multiInstance = false;
};

Settings g_settings;
std::wstring g_configPath;

// ============================================================
// ТЕМЫ
// ============================================================
struct Theme {
    COLORREF bg;
    COLORREF bg2;
    COLORREF bg3;
    COLORREF text;
    COLORREF text2;
    COLORREF accent;
    COLORREF accent2;
    COLORREF danger;
};

Theme g_themes[3] = {
    { RGB(14,22,33), RGB(23,33,43), RGB(35,46,60),
      RGB(255,255,255), RGB(122,138,153),
      RGB(0,255,136), RGB(0,204,255), RGB(255,85,85) },
    { RGB(245,245,245), RGB(255,255,255), RGB(232,232,232),
      RGB(0,0,0), RGB(102,102,102),
      RGB(0,180,100), RGB(0,150,200), RGB(220,50,50) },
    { RGB(0,0,0), RGB(10,10,10), RGB(20,20,20),
      RGB(255,255,255), RGB(150,150,150),
      RGB(0,255,136), RGB(0,204,255), RGB(255,85,85) },
};

Theme& CurTheme() { return g_themes[g_settings.theme]; }

// ============================================================
// ГЛОБАЛЬНЫЕ HWND
// ============================================================
HWND g_hWnd = nullptr;
HWND g_hTabPlay = nullptr;
HWND g_hTabSettings = nullptr;
HWND g_hTabAbout = nullptr;

HWND g_hTitle = nullptr;
HWND g_hVersion = nullptr;
HWND g_hInfo = nullptr;
HWND g_hBtnPlay = nullptr;
HWND g_hBtnFolder = nullptr;
HWND g_hEditPath = nullptr;
HWND g_hStatus = nullptr;

HWND g_hEditArgs = nullptr;
HWND g_hEditFps = nullptr;
HWND g_hComboTheme = nullptr;
HWND g_hComboRes = nullptr;
HWND g_hCheckFullscreen = nullptr;
HWND g_hCheckConsole = nullptr;
HWND g_hCheckMulti = nullptr;
HWND g_hBtnSave = nullptr;
HWND g_hBtnReset = nullptr;

HBRUSH g_brBg = nullptr;
HBRUSH g_brBg2 = nullptr;
HBRUSH g_brBg3 = nullptr;
HBRUSH g_brAccent = nullptr;
HFONT g_fontTitle = nullptr;
HFONT g_fontNormal = nullptr;
HFONT g_fontSmall = nullptr;
HFONT g_fontButton = nullptr;

// ============================================================
// КОНФИГ
// ============================================================
std::wstring GetExeDir() {
    wchar_t buf[MAX_PATH];
    GetModuleFileNameW(nullptr, buf, MAX_PATH);
    std::wstring p(buf);
    size_t pos = p.find_last_of(L"\\/");
    return pos == std::wstring::npos ? L"." : p.substr(0, pos);
}

std::wstring GetDefaultRobloxPath() {
    wchar_t local[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, local))) {
        std::wstring p = std::wstring(local) + L"\\Roblox\\Versions";
        if (PathFileExistsW(p.c_str())) {
            WIN32_FIND_DATAW fd;
            HANDLE h = FindFirstFileW((p + L"\\*").c_str(), &fd);
            std::wstring best;
            if (h != INVALID_HANDLE_VALUE) {
                do {
                    if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
                        wcscmp(fd.cFileName, L".") && wcscmp(fd.cFileName, L"..")) {
                        std::wstring exe = p + L"\\" + fd.cFileName + L"\\RobloxPlayerBeta.exe";
                        if (PathFileExistsW(exe.c_str())) best = exe;
                    }
                } while (FindNextFileW(h, &fd));
                FindClose(h);
            }
            if (!best.empty()) return best;
        }
    }
    return L"";
}

void LoadSettings() {
    g_configPath = GetExeDir() + L"\\launcher.ini";
    g_settings.robloxPath = GetDefaultRobloxPath();

    std::ifstream f(g_configPath);
    if (!f) return;

    std::string line;
    while (std::getline(f, line)) {
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string k = line.substr(0, eq);
        std::string v = line.substr(eq + 1);

        auto toW = [](const std::string& s) {
            int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
            std::wstring w(len, 0);
            MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &w[0], len);
            if (!w.empty() && w.back() == 0) w.pop_back();
            return w;
        };

        if (k == "robloxPath") g_settings.robloxPath = toW(v);
        else if (k == "args") g_settings.args = toW(v);
        else if (k == "fps") g_settings.fps = std::stoi(v);
        else if (k == "theme") g_settings.theme = std::stoi(v);
        else if (k == "resolution") g_settings.resolution = std::stoi(v);
        else if (k == "fullscreen") g_settings.fullscreen = v == "1";
        else if (k == "showConsole") g_settings.showConsole = v == "1";
        else if (k == "multiInstance") g_settings.multiInstance = v == "1";
    }
}

void SaveSettings() {
    std::ofstream f(g_configPath);
    if (!f) return;

    auto toS = [](const std::wstring& w) {
        int len = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
        std::string s(len, 0);
        WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, &s[0], len, nullptr, nullptr);
        if (!s.empty() && s.back() == 0) s.pop_back();
        return s;
    };

    f << "robloxPath=" << toS(g_settings.robloxPath) << "\n";
    f << "args=" << toS(g_settings.args) << "\n";
    f << "fps=" << g_settings.fps << "\n";
    f << "theme=" << g_settings.theme << "\n";
    f << "resolution=" << g_settings.resolution << "\n";
    f << "fullscreen=" << (g_settings.fullscreen ? 1 : 0) << "\n";
    f << "showConsole=" << (g_settings.showConsole ? 1 : 0) << "\n";
    f << "multiInstance=" << (g_settings.multiInstance ? 1 : 0) << "\n";
}

// ============================================================
// ЗАПУСК
// ============================================================
bool LaunchRoblox() {
    if (g_settings.robloxPath.empty() || !PathFileExistsW(g_settings.robloxPath.c_str())) {
        MessageBoxW(g_hWnd, L"Roblox не найден. Укажи путь в настройках.", L"Ошибка", MB_ICONERROR);
        return false;
    }

    std::wstring cmd = L"\"" + g_settings.robloxPath + L"\"";
    if (!g_settings.args.empty()) cmd += L" " + g_settings.args;

    if (g_settings.multiInstance) {
        CreateMutexW(nullptr, FALSE, L"ROBLOX_singletonEvent");
    }

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_SHOWNORMAL;

    std::vector<wchar_t> cmdBuf(cmd.begin(), cmd.end());
    cmdBuf.push_back(0);

    BOOL ok = CreateProcessW(
        nullptr, cmdBuf.data(),
        nullptr, nullptr, FALSE, 0, nullptr, nullptr,
        &si, &pi
    );

    if (!ok) {
        DWORD err = GetLastError();
        std::wstring msg = L"Не удалось запустить Roblox. Код: " + std::to_wstring(err);
        MessageBoxW(g_hWnd, msg.c_str(), L"Ошибка", MB_ICONERROR);
        return false;
    }

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return true;
}

// ============================================================
// РЕСУРСЫ
// ============================================================
void CreateResources() {
    Theme& t = CurTheme();

    if (g_brBg) DeleteObject(g_brBg);
    if (g_brBg2) DeleteObject(g_brBg2);
    if (g_brBg3) DeleteObject(g_brBg3);
    if (g_brAccent) DeleteObject(g_brAccent);

    g_brBg = CreateSolidBrush(t.bg);
    g_brBg2 = CreateSolidBrush(t.bg2);
    g_brBg3 = CreateSolidBrush(t.bg3);
    g_brAccent = CreateSolidBrush(t.accent);

    if (!g_fontTitle) {
        g_fontTitle = CreateFontW(36, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    }
    if (!g_fontNormal) {
        g_fontNormal = CreateFontW(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    }
    if (!g_fontSmall) {
        g_fontSmall = CreateFontW(13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    }
    if (!g_fontButton) {
        g_fontButton = CreateFontW(18, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    }
}

void ApplyThemeToControls() {
    HWND controls[] = {
        g_hTabPlay, g_hTabSettings, g_hTabAbout,
        g_hTitle, g_hVersion, g_hInfo,
        g_hBtnPlay, g_hBtnFolder, g_hEditPath, g_hStatus,
        g_hEditArgs, g_hEditFps,
        g_hComboTheme, g_hComboRes,
        g_hCheckFullscreen, g_hCheckConsole, g_hCheckMulti,
        g_hBtnSave, g_hBtnReset,
    };
    for (HWND h : controls) {
        if (h) InvalidateRect(h, nullptr, TRUE);
    }
}

// ============================================================
// КОНТРОЛЫ
// ============================================================
void CreateControls(HWND hWnd) {
    HINSTANCE hInst = GetModuleHandleW(nullptr);

    g_hTabPlay = CreateWindowW(L"BUTTON", L"Играть",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        20, 20, 180, 45, hWnd, (HMENU)ID_TAB_PLAY, hInst, nullptr);

    g_hTabSettings = CreateWindowW(L"BUTTON", L"Настройки",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        210, 20, 180, 45, hWnd, (HMENU)ID_TAB_SETTINGS, hInst, nullptr);

    g_hTabAbout = CreateWindowW(L"BUTTON", L"О программе",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        400, 20, 180, 45, hWnd, (HMENU)ID_TAB_ABOUT, hInst, nullptr);

    // ===== ИГРАТЬ =====
    g_hTitle = CreateWindowW(L"STATIC", L"ROBLOX",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        40, 100, 500, 50, hWnd, nullptr, hInst, nullptr);

    g_hVersion = CreateWindowW(L"STATIC", L"Launcher v1.0",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        40, 150, 500, 25, hWnd, nullptr, hInst, nullptr);

    g_hInfo = CreateWindowW(L"STATIC", L"Путь к Roblox:",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        40, 200, 200, 25, hWnd, nullptr, hInst, nullptr);

    g_hEditPath = CreateWindowW(L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
        40, 230, 600, 30, hWnd, (HMENU)ID_EDIT_PATH, hInst, nullptr);

    g_hBtnFolder = CreateWindowW(L"BUTTON", L"...",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        650, 230, 60, 30, hWnd, (HMENU)ID_BTN_FOLDER, hInst, nullptr);

    g_hBtnPlay = CreateWindowW(L"BUTTON", L"> ЗАПУСТИТЬ ROBLOX",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        40, 300, 670, 60, hWnd, (HMENU)ID_BTN_PLAY, hInst, nullptr);

    g_hStatus = CreateWindowW(L"STATIC", L"Готов к запуску",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        40, 380, 670, 25, hWnd, (HMENU)ID_STATIC_STATUS, hInst, nullptr);

    // ===== НАСТРОЙКИ =====
    int y = 100;

    CreateWindowW(L"STATIC", L"Аргументы запуска:",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        40, y, 300, 25, hWnd, nullptr, hInst, nullptr);
    y += 30;
    g_hEditArgs = CreateWindowW(L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
        40, y, 670, 30, hWnd, (HMENU)ID_EDIT_ARGS, hInst, nullptr);
    y += 50;

    CreateWindowW(L"STATIC", L"FPS-лимит:",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        40, y, 300, 25, hWnd, nullptr, hInst, nullptr);
    y += 30;
    g_hEditFps = CreateWindowW(L"EDIT", L"60",
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER,
        40, y, 150, 30, hWnd, (HMENU)ID_EDIT_FPS, hInst, nullptr);
    y += 50;

    CreateWindowW(L"STATIC", L"Тема:",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        40, y, 300, 25, hWnd, nullptr, hInst, nullptr);
    y += 30;
    g_hComboTheme = CreateWindowW(L"COMBOBOX", L"",
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
        40, y, 250, 200, hWnd, (HMENU)ID_COMBO_THEME, hInst, nullptr);
    SendMessageW(g_hComboTheme, CB_ADDSTRING, 0, (LPARAM)L"Тёмная");
    SendMessageW(g_hComboTheme, CB_ADDSTRING, 0, (LPARAM)L"Светлая");
    SendMessageW(g_hComboTheme, CB_ADDSTRING, 0, (LPARAM)L"AMOLED");
    SendMessageW(g_hComboTheme, CB_SETCURSEL, g_settings.theme, 0);
    y += 50;

    CreateWindowW(L"STATIC", L"Разрешение:",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        40, y, 300, 25, hWnd, nullptr, hInst, nullptr);
    y += 30;
    g_hComboRes = CreateWindowW(L"COMBOBOX", L"",
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
        40, y, 250, 200, hWnd, (HMENU)ID_COMBO_RESOLUTION, hInst, nullptr);
    SendMessageW(g_hComboRes, CB_ADDSTRING, 0, (LPARAM)L"Авто");
    SendMessageW(g_hComboRes, CB_ADDSTRING, 0, (LPARAM)L"1280x720");
    SendMessageW(g_hComboRes, CB_ADDSTRING, 0, (LPARAM)L"1920x1080");
    SendMessageW(g_hComboRes, CB_ADDSTRING, 0, (LPARAM)L"2560x1440");
    SendMessageW(g_hComboRes, CB_ADDSTRING, 0, (LPARAM)L"3840x2160");
    SendMessageW(g_hComboRes, CB_SETCURSEL, g_settings.resolution, 0);
    y += 50;

    g_hCheckFullscreen = CreateWindowW(L"BUTTON", L"Полноэкранный режим",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        40, y, 300, 25, hWnd, (HMENU)ID_CHECK_FULLSCREEN, hInst, nullptr);
    if (g_settings.fullscreen) SendMessageW(g_hCheckFullscreen, BM_SETCHECK, BST_CHECKED, 0);
    y += 35;

    g_hCheckConsole = CreateWindowW(L"BUTTON", L"Показывать консоль",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        40, y, 300, 25, hWnd, (HMENU)ID_CHECK_CONSOLE, hInst, nullptr);
    if (g_settings.showConsole) SendMessageW(g_hCheckConsole, BM_SETCHECK, BST_CHECKED, 0);
    y += 35;

    g_hCheckMulti = CreateWindowW(L"BUTTON", L"Мульти-инстанс",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        40, y, 300, 25, hWnd, (HMENU)ID_CHECK_MULTIINSTANCE, hInst, nullptr);
    if (g_settings.multiInstance) SendMessageW(g_hCheckMulti, BM_SETCHECK, BST_CHECKED, 0);
    y += 50;

    g_hBtnSave = CreateWindowW(L"BUTTON", L"Сохранить",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        40, y, 200, 45, hWnd, (HMENU)ID_BTN_SAVE, hInst, nullptr);

    g_hBtnReset = CreateWindowW(L"BUTTON", L"Сбросить",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        250, y, 200, 45, hWnd, (HMENU)ID_BTN_RESET, hInst, nullptr);
}

// ============================================================
// ВКЛАДКИ
// ============================================================
void ShowTab(int tab) {
    ShowWindow(g_hTitle, tab == 0 ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hVersion, tab == 0 ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hInfo, tab == 0 ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hEditPath, tab == 0 ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hBtnFolder, tab == 0 ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hBtnPlay, tab == 0 ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hStatus, tab == 0 ? SW_SHOW : SW_HIDE);

    ShowWindow(g_hEditArgs, tab == 1 ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hEditFps, tab == 1 ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hComboTheme, tab == 1 ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hComboRes, tab == 1 ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hCheckFullscreen, tab == 1 ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hCheckConsole, tab == 1 ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hCheckMulti, tab == 1 ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hBtnSave, tab == 1 ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hBtnReset, tab == 1 ? SW_SHOW : SW_HIDE);
}

// ============================================================
// WNDPROC
// ============================================================
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        LoadSettings();
        CreateResources();
        CreateControls(hWnd);

        SetWindowTextW(g_hEditPath, g_settings.robloxPath.c_str());
        SetWindowTextW(g_hEditArgs, g_settings.args.c_str());
        SetWindowTextW(g_hEditFps, std::to_wstring(g_settings.fps).c_str());

        ShowTab(0);
        ApplyThemeToControls();
        break;
    }

    case WM_COMMAND: {
        int id = LOWORD(wParam);

        if (id == ID_TAB_PLAY) ShowTab(0);
        else if (id == ID_TAB_SETTINGS) ShowTab(1);
        else if (id == ID_TAB_ABOUT) {
            MessageBoxW(hWnd,
                L"Roblox Launcher v1.0\n\n"
                L"Красивый лаунчер для Roblox.\n"
                L"Настройки: тема, путь, FPS, разрешение.\n\n"
                L"C++ + WinAPI. Один EXE.",
                L"О программе", MB_ICONINFORMATION);
        }
        else if (id == ID_BTN_PLAY) {
            wchar_t buf[MAX_PATH];
            GetWindowTextW(g_hEditPath, buf, MAX_PATH);
            g_settings.robloxPath = buf;

            if (LaunchRoblox()) {
                SetWindowTextW(g_hStatus, L"Roblox запущен");
            } else {
                SetWindowTextW(g_hStatus, L"Ошибка запуска");
            }
        }
        else if (id == ID_BTN_FOLDER) {
            OPENFILENAMEW ofn{};
            wchar_t file[MAX_PATH] = {0};
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = hWnd;
            ofn.lpstrFilter = L"RobloxPlayerBeta.exe\0RobloxPlayerBeta.exe\0EXE\0*.exe\0";
            ofn.lpstrFile = file;
            ofn.nMaxFile = MAX_PATH;
            ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

            if (GetOpenFileNameW(&ofn)) {
                g_settings.robloxPath = file;
                SetWindowTextW(g_hEditPath, file);
            }
        }
        else if (id == ID_BTN_SAVE) {
            wchar_t buf[MAX_PATH];
            GetWindowTextW(g_hEditPath, buf, MAX_PATH);
            g_settings.robloxPath = buf;

            GetWindowTextW(g_hEditArgs, buf, MAX_PATH);
            g_settings.args = buf;

            GetWindowTextW(g_hEditFps, buf, MAX_PATH);
            g_settings.fps = _wtoi(buf);
            if (g_settings.fps < 0) g_settings.fps = 0;
            if (g_settings.fps > 500) g_settings.fps = 500;

            g_settings.theme = (int)SendMessageW(g_hComboTheme, CB_GETCURSEL, 0, 0);
            g_settings.resolution = (int)SendMessageW(g_hComboRes, CB_GETCURSEL, 0, 0);

            g_settings.fullscreen = SendMessageW(g_hCheckFullscreen, BM_GETCHECK, 0, 0) == BST_CHECKED;
            g_settings.showConsole = SendMessageW(g_hCheckConsole, BM_GETCHECK, 0, 0) == BST_CHECKED;
            g_settings.multiInstance = SendMessageW(g_hCheckMulti, BM_GETCHECK, 0, 0) == BST_CHECKED;

            SaveSettings();
            CreateResources();
            ApplyThemeToControls();
            InvalidateRect(hWnd, nullptr, TRUE);
            MessageBoxW(hWnd, L"Настройки сохранены!", L"OK", MB_ICONINFORMATION);
        }
        else if (id == ID_BTN_RESET) {
            g_settings.robloxPath = GetDefaultRobloxPath();
            g_settings.args = L"";
            g_settings.fps = 60;
            g_settings.theme = 0;
            g_settings.resolution = 0;
            g_settings.fullscreen = false;
            g_settings.showConsole = false;
            g_settings.multiInstance = false;

            SetWindowTextW(g_hEditPath, g_settings.robloxPath.c_str());
            SetWindowTextW(g_hEditArgs, L"");
            SetWindowTextW(g_hEditFps, L"60");
            SendMessageW(g_hComboTheme, CB_SETCURSEL, 0, 0);
            SendMessageW(g_hComboRes, CB_SETCURSEL, 0, 0);
            SendMessageW(g_hCheckFullscreen, BM_SETCHECK, BST_UNCHECKED, 0);
            SendMessageW(g_hCheckConsole, BM_SETCHECK, BST_UNCHECKED, 0);
            SendMessageW(g_hCheckMulti, BM_SETCHECK, BST_UNCHECKED, 0);

            SaveSettings();
            CreateResources();
            ApplyThemeToControls();
            InvalidateRect(hWnd, nullptr, TRUE);
        }
        break;
    }

    case WM_CTLCOLORSTATIC: {
        HDC hdc = (HDC)wParam;
        Theme& t = CurTheme();
        SetTextColor(hdc, t.text);
        SetBkColor(hdc, t.bg);
        return (LRESULT)g_brBg;
    }
    case WM_CTLCOLOREDIT: {
        HDC hdc = (HDC)wParam;
        Theme& t = CurTheme();
        SetTextColor(hdc, t.text);
        SetBkColor(hdc, t.bg2);
        return (LRESULT)g_brBg2;
    }
    case WM_CTLCOLORBTN: {
        HDC hdc = (HDC)wParam;
        Theme& t = CurTheme();
        SetTextColor(hdc, t.text);
        SetBkColor(hdc, t.bg3);
        return (LRESULT)g_brBg3;
    }
    case WM_ERASEBKGND: {
        HDC hdc = (HDC)wParam;
        RECT rc;
        GetClientRect(hWnd, &rc);
        FillRect(hdc, &rc, g_brBg);
        return 1;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        Theme& t = CurTheme();

        RECT rc;
        GetClientRect(hWnd, &rc);
        FillRect(hdc, &rc, g_brBg);

        RECT topBar = {0, 0, rc.right, 5};
        HBRUSH brAccent = CreateSolidBrush(t.accent);
        FillRect(hdc, &topBar, brAccent);
        DeleteObject(brAccent);

        RECT sep = {0, 80, rc.right, 82};
        HBRUSH brSep = CreateSolidBrush(t.bg3);
        FillRect(hdc, &sep, brSep);
        DeleteObject(brSep);

        EndPaint(hWnd, &ps);
        return 0;
    }
    case WM_DESTROY:
        SaveSettings();
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

// ============================================================
// WINMAIN
// ============================================================
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int nCmdShow) {
    INITCOMMONCONTROLSEX icc{};
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_WIN95_CLASSES;
    InitCommonControlsEx(&icc);

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    wc.lpszClassName = WINDOW_CLASS;
    wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    wc.hIconSm = LoadIconW(nullptr, IDI_APPLICATION);

    if (!RegisterClassExW(&wc)) {
        MessageBoxW(nullptr, L"Не удалось зарегистрировать окно", L"Ошибка", MB_ICONERROR);
        return 1;
    }

    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int x = (screenW - WINDOW_W) / 2;
    int y = (screenH - WINDOW_H) / 2;

    HWND hWnd = CreateWindowExW(
        0, WINDOW_CLASS, WINDOW_TITLE,
        WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX & ~WS_THICKFRAME,
        x, y, WINDOW_W, WINDOW_H,
        nullptr, nullptr, hInstance, nullptr
    );

    if (!hWnd) {
        MessageBoxW(nullptr, L"Не удалось создать окно", L"Ошибка", MB_ICONERROR);
        return 1;
    }

    g_hWnd = hWnd;

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (g_fontTitle) DeleteObject(g_fontTitle);
    if (g_fontNormal) DeleteObject(g_fontNormal);
    if (g_fontSmall) DeleteObject(g_fontSmall);
    if (g_fontButton) DeleteObject(g_fontButton);
    if (g_brBg) DeleteObject(g_brBg);
    if (g_brBg2) DeleteObject(g_brBg2);
    if (g_brBg3) DeleteObject(g_brBg3);
    if (g_brAccent) DeleteObject(g_brAccent);

    return 0;
}
