#include "gradient.h"

#pragma comment(lib, "Msimg32.lib")

GradientApp::GradientApp(HINSTANCE inst) : m_instance(inst) {
    WNDCLASSEXW wcMain = { sizeof(WNDCLASSEXW) };
    wcMain.lpfnWndProc = main_wnd_proc;
    wcMain.hInstance = m_instance;
    wcMain.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wcMain.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcMain.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcMain.lpszClassName = L"LabMainClass";
    RegisterClassExW(&wcMain);

    WNDCLASSEXW wcStrip = { sizeof(WNDCLASSEXW) };
    wcStrip.lpfnWndProc = strip_wnd_proc;
    wcStrip.hInstance = m_instance;
    wcStrip.hCursor = LoadCursor(NULL, IDC_CROSS);
    wcStrip.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wcStrip.lpszClassName = L"LabStripClass";
    RegisterClassExW(&wcStrip);

    m_main_hwnd = CreateWindowExW(0, L"LabMainClass", L"Edytor Gradientow - Etap 1",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT, 600, 400,
        NULL, NULL, m_instance, this);

    m_strip_hwnd = CreateWindowExW(0, L"LabStripClass", NULL,
        WS_CHILD | WS_VISIBLE,
        50, 250, 480, 60, m_main_hwnd, NULL, m_instance, this);

    HMENU hMenu = CreateMenu();
    HMENU hMenuOptions = CreatePopupMenu();
    AppendMenuW(hMenuOptions, MF_STRING, 1001, L"Resetuj Gradient\tCtrl+R");
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hMenuOptions, L"Opcje");
    SetMenu(m_main_hwnd, hMenu);

    ACCEL accel[1];
    accel[0].fVirt = FCONTROL | FVIRTKEY;
    accel[0].key = 'R';
    accel[0].cmd = 1001; 
    m_hAccel = CreateAcceleratorTable(accel, 1);
}

GradientApp::~GradientApp() {
    if (m_hAccel) DestroyAcceleratorTable(m_hAccel);
}

int GradientApp::run(int show_cmd) {
    ShowWindow(m_main_hwnd, show_cmd);
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        if (!TranslateAccelerator(m_main_hwnd, m_hAccel, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    return (int)msg.wParam;
}

LRESULT CALLBACK GradientApp::main_wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_NCCREATE) {
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)(((LPCREATESTRUCT)lp)->lpCreateParams));
    }
    GradientApp* app = reinterpret_cast<GradientApp*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    if (!app) return DefWindowProc(hwnd, msg, wp, lp);

    switch (msg) {
    case WM_DESTROY: 
        PostQuitMessage(0); 
        return 0;

    case WM_GETMINMAXINFO: {
        LPMINMAXINFO mmi = (LPMINMAXINFO)lp;
        mmi->ptMinTrackSize.x = 400;
        mmi->ptMinTrackSize.y = 300;
        return 0;
    }

    case WM_COMMAND: {
        if (LOWORD(wp) == 1001) {
            MessageBoxW(hwnd, L"Wywołano Reset Gradientu (Ctrl+R)!", L"Akcja", MB_OK | MB_ICONINFORMATION);
        }
        return 0;
    }

    case WM_APP_GRADIENT_CHANGED: {
        MessageBoxW(hwnd, L"Główne okno odebrało sygnał!", L"Komunikacja", MB_OK);
        return 0;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        
        RECT rc;
        GetClientRect(hwnd, &rc);

        TRIVERTEX vertex[2];
        
        vertex[0].x     = rc.left;
        vertex[0].y     = rc.top;
        vertex[0].Red   = 0x0000;
        vertex[0].Green = 0x0000;
        vertex[0].Blue  = 0x0000;
        vertex[0].Alpha = 0x0000;

        vertex[1].x     = rc.right;
        vertex[1].y     = rc.bottom; 
        vertex[1].Red   = 0xff00; 
        vertex[1].Green = 0xff00;
        vertex[1].Blue  = 0xff00;
        vertex[1].Alpha = 0x0000;

        GRADIENT_RECT gRect;
        gRect.UpperLeft  = 0;
        gRect.LowerRight = 1;

        GradientFill(hdc, vertex, 2, &gRect, 1, GRADIENT_FILL_RECT_H);

        EndPaint(hwnd, &ps);
        return 0;
    }
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

LRESULT CALLBACK GradientApp::strip_wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_LBUTTONDOWN: {
        HWND parent = GetParent(hwnd);
        SendMessage(parent, WM_APP_GRADIENT_CHANGED, 0, 0);
        return 0;
    }
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}
