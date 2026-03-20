#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include "gradient_app.h"
#include <windowsx.h>
#include <commdlg.h>
#include <algorithm>
#include <cmath>

#pragma comment(lib, "Msimg32.lib")  // Dla GradientFill
#pragma comment(lib, "Comdlg32.lib") // Dla ChooseColor

bool GradientApp::register_classes() {
    WNDCLASSEXW desc{};
    desc.cbSize = sizeof(WNDCLASSEXW);
    desc.lpfnWndProc = window_proc_static;
    desc.hInstance = m_instance;
    desc.hCursor = LoadCursorW(nullptr, (LPCWSTR)IDC_ARROW);
    // Tło ustawi się samo w WM_PAINT, więc zostawiamy pusty pędzel
    desc.hbrBackground = nullptr; 
    desc.lpszClassName = L"GradientAppClass";

    return RegisterClassExW(&desc) != 0;
}

GradientApp::GradientApp(HINSTANCE instance)
    : m_instance{ instance }, m_main{ nullptr }, 
      m_btn_color1{ nullptr }, m_btn_color2{ nullptr },
      m_color1{ RGB(255, 0, 0) }, m_color2{ RGB(0, 0, 255) }, // Startowe kolory: Czerwony -> Niebieski
      m_pos1{ 0.1 }, m_pos_mid{ 0.5 }, m_pos2{ 0.9 }, // Ułożenie suwaków z obrazka
      m_drag_target{ 0 }
{
    register_classes();

    // Pełen zestaw flag by umożliwić swobodne rozciąganie okna
    DWORD style = WS_OVERLAPPEDWINDOW;

    m_main = CreateWindowExW(
        0, L"GradientAppClass", L"Zaawansowany Edytor Gradientu", style,
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 600,
        nullptr, nullptr, m_instance, this
    );

    // Przyciski osadzamy jako okna potomne, ich wymiary będziemy modyfikować w WM_SIZE
    m_btn_color1 = CreateWindowExW(
        0, L"BUTTON", L"Zmień Kolor 1",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        0, 0, 0, 0, m_main, reinterpret_cast<HMENU>(ID_BTN_C1), m_instance, nullptr
    );

    m_btn_color2 = CreateWindowExW(
        0, L"BUTTON", L"Zmień Kolor 2",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        0, 0, 0, 0, m_main, reinterpret_cast<HMENU>(ID_BTN_C2), m_instance, nullptr
    );
}

GradientApp::~GradientApp() {}

int GradientApp::run(int show_command) {
    ShowWindow(m_main, show_command);
    MSG msg{};
    BOOL result = TRUE;
    while ((result = GetMessageW(&msg, nullptr, 0, 0)) != 0) {
        if (result == -1) return EXIT_FAILURE;
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return EXIT_SUCCESS;
}

COLORREF GradientApp::open_color_chooser(HWND owner, COLORREF initial_color) {
    CHOOSECOLOR cc{};
    static COLORREF custom_colors[16]{}; 

    cc.lStructSize = sizeof(cc);
    cc.hwndOwner = owner;
    cc.lpCustColors = (LPDWORD)custom_colors;
    cc.rgbResult = initial_color;
    cc.Flags = CC_FULLOPEN | CC_RGBINIT; 

    if (ChooseColor(&cc)) {
        return cc.rgbResult;
    }
    return initial_color;
}

// Funkcja pomocnicza - upraszcza rysowanie jednego płynnego przejścia na wskazanym prostokącie
void GradientApp::draw_gradient_segment(HDC dc, RECT rc, COLORREF c_left, COLORREF c_right) {
    if (rc.right <= rc.left) return; // Zabezpieczenie przed zerową szerokością

    TRIVERTEX vertex[2];
    vertex[0] = { rc.left, rc.top, 
                 (COLOR16)(GetRValue(c_left) << 8), 
                 (COLOR16)(GetGValue(c_left) << 8), 
                 (COLOR16)(GetBValue(c_left) << 8), 0 };
                 
    vertex[1] = { rc.right, rc.bottom, 
                 (COLOR16)(GetRValue(c_right) << 8), 
                 (COLOR16)(GetGValue(c_right) << 8), 
                 (COLOR16)(GetBValue(c_right) << 8), 0 };

    GRADIENT_RECT gRect = { 0, 1 };
    GradientFill(dc, vertex, 2, &gRect, 1, GRADIENT_FILL_RECT_H);
}

void GradientApp::draw_ui(HDC dc) {
    // Rysowanie tła dolnego paska w natywnym kolorze systemu
    HBRUSH bgBrush = GetSysColorBrush(COLOR_BTNFACE);
    RECT bottom_bar = { 0, m_draw_area.bottom, m_draw_area.right, m_draw_area.bottom + 80 };
    FillRect(dc, &bottom_bar, bgBrush);

    // --- TŁO ŚCIEŻKI SUWAKA (Podgląd Gradientu) ---
    COLORREF cmid = RGB(
        (GetRValue(m_color1) + GetRValue(m_color2)) / 2,
        (GetGValue(m_color1) + GetGValue(m_color2)) / 2,
        (GetBValue(m_color1) + GetBValue(m_color2)) / 2
    );

    int w = m_track_area.right - m_track_area.left;
    int x1 = m_track_area.left + static_cast<int>(m_pos1 * w);
    int xmid = m_track_area.left + static_cast<int>(m_pos_mid * w);
    int x2 = m_track_area.left + static_cast<int>(m_pos2 * w);

    // Segmenty gradientu wewnątrz samego suwaka
    draw_gradient_segment(dc, { m_track_area.left, m_track_area.top, x1, m_track_area.bottom }, m_color1, m_color1);
    draw_gradient_segment(dc, { x1, m_track_area.top, xmid, m_track_area.bottom }, m_color1, cmid);
    draw_gradient_segment(dc, { xmid, m_track_area.top, x2, m_track_area.bottom }, cmid, m_color2);
    draw_gradient_segment(dc, { x2, m_track_area.top, m_track_area.right, m_track_area.bottom }, m_color2, m_color2);

    // Obramowanie paska
    FrameRect(dc, &m_track_area, (HBRUSH)GetStockObject(BLACK_BRUSH));

    // --- RYSOWANIE KCIUKÓW (THUMBS) ---
    HBRUSH whiteBrush = (HBRUSH)GetStockObject(WHITE_BRUSH);
    HPEN blackPen = (HPEN)GetStockObject(BLACK_PEN);
    HGDIOBJ oldBrush = SelectObject(dc, whiteBrush);
    HGDIOBJ oldPen = SelectObject(dc, blackPen);

    // Rysowanie zaleznie od typu: Krawędzie to prostokąty, środek to kółko/elipsa
    Rectangle(dc, x1 - 6, m_track_area.top - 5, x1 + 6, m_track_area.bottom + 5);
    Ellipse(dc, xmid - 6, m_track_area.top - 2, xmid + 6, m_track_area.bottom + 2);
    Rectangle(dc, x2 - 6, m_track_area.top - 5, x2 + 6, m_track_area.bottom + 5);

    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
}

void GradientApp::on_paint(HWND window) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(window, &ps);

    // --- DOUBLE BUFFERING --- 
    // Zapobiega migotaniu przy ciągłym przesuwaniu suwaków myszą
    RECT rc;
    GetClientRect(window, &rc);
    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBM = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
    HBITMAP oldBM = (HBITMAP)SelectObject(memDC, memBM);

    // Obliczanie koloru środkowego
    COLORREF cmid = RGB(
        (GetRValue(m_color1) + GetRValue(m_color2)) / 2,
        (GetGValue(m_color1) + GetGValue(m_color2)) / 2,
        (GetBValue(m_color1) + GetBValue(m_color2)) / 2
    );

    int w = m_draw_area.right - m_draw_area.left;
    int x1 = static_cast<int>(m_pos1 * w);
    int xmid = static_cast<int>(m_pos_mid * w);
    int x2 = static_cast<int>(m_pos2 * w);

    // Rysowanie 4 segmentów na głównym ekranie
    draw_gradient_segment(memDC, { 0, 0, x1, m_draw_area.bottom }, m_color1, m_color1);
    draw_gradient_segment(memDC, { x1, 0, xmid, m_draw_area.bottom }, m_color1, cmid);
    draw_gradient_segment(memDC, { xmid, 0, x2, m_draw_area.bottom }, cmid, m_color2);
    draw_gradient_segment(memDC, { x2, 0, w, m_draw_area.bottom }, m_color2, m_color2);

    // Rysowanie dolnego paska z narzędziami
    draw_ui(memDC);

    // Skopiowanie gotowej, płynnej klatki na ekran
    BitBlt(hdc, 0, 0, rc.right, rc.bottom, memDC, 0, 0, SRCCOPY);

    // Sprzątanie pamięci GDI
    SelectObject(memDC, oldBM);
    DeleteObject(memBM);
    DeleteDC(memDC);
    EndPaint(window, &ps);
}

LRESULT GradientApp::window_proc_static(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    GradientApp* app = nullptr;
    if (message == WM_NCCREATE) {
        auto p = reinterpret_cast<LPCREATESTRUCTW>(lparam);
        app = static_cast<GradientApp*>(p->lpCreateParams);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
    } else {
        app = reinterpret_cast<GradientApp*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    }
    
    if (app != nullptr) return app->window_proc(window, message, wparam, lparam);
    return DefWindowProcW(window, message, wparam, lparam);
}

LRESULT GradientApp::window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
    case WM_CLOSE: DestroyWindow(window); return 0;
    case WM_DESTROY: PostQuitMessage(EXIT_SUCCESS); return 0;

    // --- Odbieramy tłu uprawnienia do samodzielnego czyszczenia (brak migotania!) ---
    case WM_ERASEBKGND: return 1; 

    case WM_SIZE: {
        int width = LOWORD(lparam);
        int height = HIWORD(lparam);
        
        // Zostawiamy 80 pikseli na dole okna dla interfejsu
        m_draw_area = { 0, 0, width, height - 80 };
        
        // Przemieszczenie przycisków w obrębie panelu UI
        SetWindowPos(m_btn_color1, nullptr, 10, height - 55, 120, 30, SWP_NOZORDER);
        SetWindowPos(m_btn_color2, nullptr, width - 130, height - 55, 120, 30, SWP_NOZORDER);

        // Zapisanie wymiarów samej ścieżki "suwaka"
        m_track_area = { 150, height - 50, width - 150, height - 30 };
        
        InvalidateRect(window, nullptr, FALSE);
        return 0;
    }

    case WM_PAINT: {
        on_paint(window);
        return 0;
    }

    case WM_COMMAND: {
        int id = LOWORD(wparam);
        if (id == ID_BTN_C1) {
            m_color1 = open_color_chooser(window, m_color1);
            InvalidateRect(window, nullptr, FALSE);
        } else if (id == ID_BTN_C2) {
            m_color2 = open_color_chooser(window, m_color2);
            InvalidateRect(window, nullptr, FALSE);
        }
        return 0;
    }

    case WM_LBUTTONDOWN: {
        // --- LOGIKA CHWYTANIA CUSTOMOWYCH KCIUKÓW ---
        int mx = GET_X_LPARAM(lparam);
        int my = GET_Y_LPARAM(lparam);

        // Upewniamy się, że użytkownik klika w obrębie wysokości paska
        if (my >= m_track_area.top - 10 && my <= m_track_area.bottom + 10) {
            int w = m_track_area.right - m_track_area.left;
            int x1 = m_track_area.left + static_cast<int>(m_pos1 * w);
            int xmid = m_track_area.left + static_cast<int>(m_pos_mid * w);
            int x2 = m_track_area.left + static_cast<int>(m_pos2 * w);

            // Sprawdzamy co użytkownik kliknął (tolerancja promienia 10px)
            if (std::abs(mx - xmid) <= 10) m_drag_target = 2; // Środkowy ma najwyższy priorytet
            else if (std::abs(mx - x1) <= 10) m_drag_target = 1;
            else if (std::abs(mx - x2) <= 10) m_drag_target = 3;

            if (m_drag_target > 0) SetCapture(window);
        }
        return 0;
    }

    case WM_MOUSEMOVE: {
        // --- LOGIKA PŁYNNEGO PRZESUWANIA (DRAG & DROP) ---
        if (m_drag_target > 0) {
            int mx = GET_X_LPARAM(lparam);
            int w = m_track_area.right - m_track_area.left;
            
            // Konwersja pikseli z powrotem na procentowy ułamek (0.0 do 1.0)
            double new_pos = static_cast<double>(mx - m_track_area.left) / static_cast<double>(w);
            new_pos = std::max(0.0, std::min(new_pos, 1.0)); // Ogranicznik "twardy" ekranu

            // Ograniczniki "miękkie" - zachowanie logicznej kolejności suwaków
            if (m_drag_target == 1) m_pos1 = std::min(new_pos, m_pos_mid);
            if (m_drag_target == 2) m_pos_mid = std::max(m_pos1, std::min(new_pos, m_pos2));
            if (m_drag_target == 3) m_pos2 = std::max(new_pos, m_pos_mid);

            // Odświeżenie okna bez migotania
            InvalidateRect(window, nullptr, FALSE);
        }
        return 0;
    }

    case WM_LBUTTONUP: {
        if (m_drag_target > 0) {
            m_drag_target = 0;
            ReleaseCapture();
        }
        return 0;
    }
    }
    return DefWindowProcW(window, message, wparam, lparam);
}