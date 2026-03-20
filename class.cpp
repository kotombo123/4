#include "gradient.h"
#include <windowsx.h>
#include <commdlg.h>

#pragma comment(lib, "Msimg32.lib")  // Dla GradientFill
#pragma comment(lib, "Comdlg32.lib") // Dla ChooseColor

bool GradientApp::register_classes() {
    WNDCLASSEXW desc{};
    desc.cbSize = sizeof(WNDCLASSEXW);
    desc.lpfnWndProc = window_proc_static;
    desc.hInstance = m_instance;
    desc.hCursor = LoadCursorW(nullptr, (LPCWSTR)IDC_ARROW);
    desc.hbrBackground = nullptr; // Rysujemy t³o rêcznie
    desc.lpszClassName = L"SimpleGradientClass";

    return RegisterClassExW(&desc) != 0;
}

GradientApp::GradientApp(HINSTANCE instance)
    : m_instance{ instance }, m_main{ nullptr },
    m_btn_color1{ nullptr }, m_btn_color2{ nullptr },
    m_color1{ RGB(0, 200, 150) }, m_color2{ RGB(70, 0, 130) } // Domyœlne kolory
{
    register_classes();

    // WS_CLIPCHILDREN jest tu kluczowe - zapobiega zamalowywaniu przycisków przez Double Buffering
    DWORD style = WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN;

    m_main = CreateWindowExW(
        0, L"SimpleGradientClass", L"Edytor Gradientu", style,
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 500,
        nullptr, nullptr, m_instance, this
    );

    // Tworzymy przyciski ze stylem BS_OWNERDRAW
    m_btn_color1 = CreateWindowExW(
        0, L"BUTTON", L"Kolor 1",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        0, 0, 0, 0, m_main, reinterpret_cast<HMENU>(ID_BTN_C1), m_instance, nullptr
    );

    m_btn_color2 = CreateWindowExW(
        0, L"BUTTON", L"Kolor 2",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
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

// Ta funkcja obs³uguje w³asnorêczne malowanie przycisków
void GradientApp::on_draw_item(DRAWITEMSTRUCT* dis) {
    if (dis->CtlID == ID_BTN_C1 || dis->CtlID == ID_BTN_C2) {
        HDC dc = dis->hDC;
        RECT rc = dis->rcItem;
        bool is_pressed = (dis->itemState & ODS_SELECTED);

        // 1. T³o przycisku (szary systemowy)
        FillRect(dc, &rc, GetSysColorBrush(COLOR_BTNFACE));

        // 2. Wypuk³a/wklês³a ramka 3D przycisku
        DrawEdge(dc, &rc, is_pressed ? EDGE_SUNKEN : EDGE_RAISED, BF_RECT);

        // Lekkie przesuniêcie pikseli w dó³ i prawo symuluj¹ce wciœniêcie fizycznego przycisku
        int offset = is_pressed ? 1 : 0;

        // 3. Okienko z kolorem ("swatch")
        RECT swatch_rc = { rc.left + 8 + offset, rc.top + 8 + offset, rc.left + 35 + offset, rc.bottom - 8 + offset };
        COLORREF btn_color = (dis->CtlID == ID_BTN_C1) ? m_color1 : m_color2;

        HBRUSH color_brush = CreateSolidBrush(btn_color);
        FillRect(dc, &swatch_rc, color_brush);
        DeleteObject(color_brush); // Wa¿ne: zwalniamy pêdzel po u¿yciu

        // Czarna ramka wokó³ kwadracika z kolorem
        FrameRect(dc, &swatch_rc, (HBRUSH)GetStockObject(BLACK_BRUSH));

        // 4. Tekst przycisku
        std::wstring text = (dis->CtlID == ID_BTN_C1) ? L"Wybierz Kolor 1" : L"Wybierz Kolor 2";
        SetBkMode(dc, TRANSPARENT); // ¯eby t³o tekstu nie maza³o na bia³o

        RECT text_rc = rc;
        text_rc.left = swatch_rc.right + 10; // Tekst zaczyna siê za kwadracikiem
        text_rc.top += offset;
        text_rc.bottom += offset;

        // Rysujemy tekst wyœrodkowany w pionie
        DrawTextW(dc, text.c_str(), -1, &text_rc, DT_SINGLELINE | DT_VCENTER | DT_LEFT);

        // 5. Opcjonalna przerywana ramka Focusu (gdy u¿ywamy klawiatury)
        if (dis->itemState & ODS_FOCUS) {
            RECT focus_rc = rc;
            InflateRect(&focus_rc, -4, -4);
            DrawFocusRect(dc, &focus_rc);
        }
    }
}

void GradientApp::on_paint(HWND window) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(window, &ps);

    // --- Double Buffering (brak migotania) ---
    RECT rc;
    GetClientRect(window, &rc);
    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBM = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
    HBITMAP oldBM = (HBITMAP)SelectObject(memDC, memBM);

    // 1. Wype³niamy dolny pasek szarym kolorem
    FillRect(memDC, &m_bottom_area, GetSysColorBrush(COLOR_BTNFACE));

    // 2. Rysujemy prosty gradient 2-kolorowy na górze
    if (m_draw_area.right > 0 && m_draw_area.bottom > 0) {
        TRIVERTEX vertex[2];
        vertex[0] = { m_draw_area.left, m_draw_area.top,
                     (COLOR16)(GetRValue(m_color1) << 8),
                     (COLOR16)(GetGValue(m_color1) << 8),
                     (COLOR16)(GetBValue(m_color1) << 8), 0 };

        vertex[1] = { m_draw_area.right, m_draw_area.bottom,
                     (COLOR16)(GetRValue(m_color2) << 8),
                     (COLOR16)(GetGValue(m_color2) << 8),
                     (COLOR16)(GetBValue(m_color2) << 8), 0 };

        GRADIENT_RECT gRect = { 0, 1 };
        GradientFill(memDC, vertex, 2, &gRect, 1, GRADIENT_FILL_RECT_H);
    }

    // Przerzucenie ca³oœci na ekran. Zwróæ uwagê na flagê WS_CLIPCHILDREN 
    // ustawion¹ w konstruktorze – dziêki niej przyciski nie s¹ nadpisywane!
    BitBlt(hdc, 0, 0, rc.right, rc.bottom, memDC, 0, 0, SRCCOPY);

    // Sprz¹tanie po buforze
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
    }
    else {
        app = reinterpret_cast<GradientApp*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    }

    if (app != nullptr) return app->window_proc(window, message, wparam, lparam);
    return DefWindowProcW(window, message, wparam, lparam);
}

LRESULT GradientApp::window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
    case WM_CLOSE: DestroyWindow(window); return 0;
    case WM_DESTROY: PostQuitMessage(EXIT_SUCCESS); return 0;

        // Zapobiega migotaniu przy zmianie rozmiaru okna
    case WM_ERASEBKGND: return 1;

    case WM_SIZE: {
        int width = LOWORD(lparam);
        int height = HIWORD(lparam);

        int bar_height = 60;
        int btn_width = 160;
        int btn_height = 40;

        m_draw_area = { 0, 0, width, height - bar_height };
        m_bottom_area = { 0, height - bar_height, width, height };

        // Uk³adanie przycisków na œrodku paska w pionie i po bokach w poziomie
        int btn_y = height - bar_height + (bar_height - btn_height) / 2;

        SetWindowPos(m_btn_color1, nullptr, 20, btn_y, btn_width, btn_height, SWP_NOZORDER);
        SetWindowPos(m_btn_color2, nullptr, width - btn_width - 20, btn_y, btn_width, btn_height, SWP_NOZORDER);

        InvalidateRect(window, nullptr, FALSE);
        return 0;
    }

    case WM_PAINT: {
        on_paint(window);
        return 0;
    }

                 // Ten komunikat pozwala nam rêcznie narysowaæ przyciski!
    case WM_DRAWITEM: {
        DRAWITEMSTRUCT* dis = reinterpret_cast<DRAWITEMSTRUCT*>(lparam);
        on_draw_item(dis);
        return TRUE;
    }

    case WM_COMMAND: {
        int id = LOWORD(wparam);
        if (id == ID_BTN_C1) {
            m_color1 = open_color_chooser(window, m_color1);
            InvalidateRect(window, nullptr, FALSE);
        }
        else if (id == ID_BTN_C2) {
            m_color2 = open_color_chooser(window, m_color2);
            InvalidateRect(window, nullptr, FALSE);
        }
        return 0;
    }
    }
    return DefWindowProcW(window, message, wparam, lparam);
}