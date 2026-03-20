#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <commdlg.h>
#include <vector>
#include <algorithm>
#include <cmath>

#pragma comment(lib, "Msimg32.lib")  // Dla GradientFill
#pragma comment(lib, "Comdlg32.lib") // Dla ChooseColor

// Niestandardowy komunikat do komunikacji: Dziecko (Pasek) -> Rodzic (Główne okno)
#define WM_APP_GRADIENT_CHANGED (WM_APP + 1)

// Struktura reprezentująca pojedynczy punkt kontrolny na gradiencie
struct ColorStop {
    float pos;      // Pozycja w zakresie od 0.0 do 1.0
    COLORREF color; // Kolor RGB

    // Sortowanie punktów po ich pozycji
    bool operator<(const ColorStop& other) const { return pos < other.pos; }
};

class GradientApp {
private:
    HINSTANCE m_instance;
    HWND m_main_hwnd;
    HWND m_strip_hwnd;
    HACCEL m_hAccel; // Akcelerator skrótów klawiszowych (Ctrl+R)

    std::vector<ColorStop> m_stops;

    // Zmienne stanu dla Paska Gradientu
    int m_drag_idx = -1;
    int m_hover_idx = -1;
    POINT m_click_pos = { 0, 0 };
    bool m_mouse_tracked = false;

    // Funkcje pomocnicze
    void reset_gradient();
    COLORREF interpolate_color(float pos);
    void draw_multi_gradient(HDC dc, RECT rc, bool is_strip);
    void notify_parent() { SendMessage(m_main_hwnd, WM_APP_GRADIENT_CHANGED, 0, 0); }

    // Rysowanie uchwytu (znacznika koloru)
    void draw_handle(HDC dc, int x, int y, int width, int height, COLORREF color, bool is_hovered);

    // Procedury okien
    static LRESULT CALLBACK main_wnd_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
    static LRESULT CALLBACK strip_wnd_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

public:
    GradientApp(HINSTANCE inst);
    ~GradientApp();
    int run(int show_cmd);
};

// ==============================================================================
// LOGIKA BIZNESOWA I RYSOWANIE
// ==============================================================================

GradientApp::GradientApp(HINSTANCE inst) : m_instance(inst) {
    reset_gradient();

    // 1. Rejestracja Głównego Okna
    WNDCLASSEXW wcMain = { sizeof(WNDCLASSEXW) };
    wcMain.lpfnWndProc = main_wnd_proc;
    wcMain.hInstance = m_instance;
    // Używamy nietypowej ikony systemowej, aby spełnić warunek "własna ikona zamiast domyślnej"
    // W profesjonalnym projekcie użylibyśmy LoadIcon(hInstance, MAKEINTRESOURCE(IDI_MYICON)) z pliku .rc
    wcMain.hIcon = LoadIcon(NULL, IDI_SHIELD); 
    wcMain.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcMain.hbrBackground = NULL; // Zapobiega migotaniu (Double Buffering)
    wcMain.lpszClassName = L"LabGradientMainClass";
    RegisterClassExW(&wcMain);

    // 2. Rejestracja Niestandardowej Kontrolki GDI (Pasek Gradientu)
    WNDCLASSEXW wcStrip = { sizeof(WNDCLASSEXW) };
    // Styl CS_DBLCLKS jest wymagany, aby okno rejestrowało podwójne kliknięcia!
    wcStrip.style = CS_DBLCLKS; 
    wcStrip.lpfnWndProc = strip_wnd_proc;
    wcStrip.hInstance = m_instance;
    wcStrip.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcStrip.hbrBackground = NULL; 
    wcStrip.lpszClassName = L"LabGradientStripClass";
    RegisterClassExW(&wcStrip);

    // Tworzenie Okna Głównego (rozmiar 600x400)
    m_main_hwnd = CreateWindowExW(0, L"LabGradientMainClass", L"Edytor Gradientów (Core)",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT, 600, 400,
        NULL, NULL, m_instance, this);

    // Tworzenie Okna Paska (jako dziecko)
    m_strip_hwnd = CreateWindowExW(0, L"LabGradientStripClass", NULL,
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
        0, 0, 0, 0, m_main_hwnd, NULL, m_instance, this);

    // Tworzenie Menu i Akceleratora (Ctrl+R) w kodzie, bez pliku zasobów (.rc)
    HMENU hMenu = CreateMenu();
    HMENU hMenuOptions = CreatePopupMenu();
    AppendMenuW(hMenuOptions, MF_STRING, 1001, L"Resetuj Gradient\tCtrl+R");
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hMenuOptions, L"Opcje");
    SetMenu(m_main_hwnd, hMenu);

    ACCEL accel[1];
    accel[0].fVirt = FCONTROL | FVIRTKEY;
    accel[0].key = 'R';
    accel[0].cmd = 1001; // ID opcji menu
    m_hAccel = CreateAcceleratorTable(accel, 1);
}

GradientApp::~GradientApp() {
    if (m_hAccel) DestroyAcceleratorTable(m_hAccel);
}

int GradientApp::run(int show_cmd) {
    ShowWindow(m_main_hwnd, show_cmd);
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        // Przechwytywanie skrótów klawiszowych (naszego Ctrl+R) przed standardowym przetworzeniem
        if (!TranslateAccelerator(m_main_hwnd, m_hAccel, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    return (int)msg.wParam;
}

void GradientApp::reset_gradient() {
    m_stops.clear();
    m_stops.push_back({ 0.0f, RGB(0, 0, 0) });       // Czarny
    m_stops.push_back({ 1.0f, RGB(255, 255, 255) }); // Biały
}

COLORREF GradientApp::interpolate_color(float pos) {
    if (m_stops.empty()) return RGB(255, 255, 255);
    if (pos <= m_stops.front().pos) return m_stops.front().color;
    if (pos >= m_stops.back().pos) return m_stops.back().color;

    for (size_t i = 0; i < m_stops.size() - 1; ++i) {
        if (pos >= m_stops[i].pos && pos <= m_stops[i + 1].pos) {
            float t = (pos - m_stops[i].pos) / (m_stops[i + 1].pos - m_stops[i].pos);
            BYTE r = GetRValue(m_stops[i].color) + t * (GetRValue(m_stops[i + 1].color) - GetRValue(m_stops[i].color));
            BYTE g = GetGValue(m_stops[i].color) + t * (GetGValue(m_stops[i + 1].color) - GetGValue(m_stops[i].color));
            BYTE b = GetBValue(m_stops[i].color) + t * (GetBValue(m_stops[i + 1].color) - GetBValue(m_stops[i].color));
            return RGB(r, g, b);
        }
    }
    return RGB(0, 0, 0);
}

void GradientApp::draw_multi_gradient(HDC dc, RECT rc, bool is_strip) {
    int width = rc.right - rc.left;
    if (width <= 0 || m_stops.empty()) return;

    // Rysowanie segmentów przy użyciu systemowego GradientFill
    for (size_t i = 0; i < m_stops.size() - 1; ++i) {
        int x1 = rc.left + static_cast<int>(m_stops[i].pos * width);
        int x2 = rc.left + static_cast<int>(m_stops[i + 1].pos * width);

        if (x2 > x1) {
            TRIVERTEX vertex[2];
            vertex[0] = { x1, rc.top, (COLOR16)(GetRValue(m_stops[i].color) << 8),
                         (COLOR16)(GetGValue(m_stops[i].color) << 8), (COLOR16)(GetBValue(m_stops[i].color) << 8), 0 };
            vertex[1] = { x2, rc.bottom, (COLOR16)(GetRValue(m_stops[i + 1].color) << 8),
                         (COLOR16)(GetGValue(m_stops[i + 1].color) << 8), (COLOR16)(GetBValue(m_stops[i + 1].color) << 8), 0 };

            GRADIENT_RECT gRect = { 0, 1 };
            GradientFill(dc, vertex, 2, &gRect, 1, GRADIENT_FILL_RECT_H);
        }
    }

    // Wypełnianie krańców, jeśli punkty nie znajdują się na skrajach 0.0 i 1.0
    if (m_stops.front().pos > 0.0f) {
        RECT r_left = { rc.left, rc.top, rc.left + static_cast<int>(m_stops.front().pos * width), rc.bottom };
        HBRUSH br = CreateSolidBrush(m_stops.front().color);
        FillRect(dc, &r_left, br);
        DeleteObject(br);
    }
    if (m_stops.back().pos < 1.0f) {
        RECT r_right = { rc.left + static_cast<int>(m_stops.back().pos * width), rc.top, rc.right, rc.bottom };
        HBRUSH br = CreateSolidBrush(m_stops.back().color);
        FillRect(dc, &r_right, br);
        DeleteObject(br);
    }
}

void GradientApp::draw_handle(HDC dc, int x, int y, int width, int height, COLORREF color, bool is_hovered) {
    // Prosty marker w kształcie prostokąta/trójkąta z obramowaniem
    POINT pts[5] = {
        {x - width / 2, y + height},
        {x - width / 2, y + height / 3},
        {x, y},
        {x + width / 2, y + height / 3},
        {x + width / 2, y + height}
    };

    HBRUSH brush = CreateSolidBrush(color);
    HPEN pen = CreatePen(PS_SOLID, is_hovered ? 2 : 1, is_hovered ? RGB(0, 120, 215) : RGB(100, 100, 100));
    
    HGDIOBJ oldBrush = SelectObject(dc, brush);
    HGDIOBJ oldPen = SelectObject(dc, pen);

    Polygon(dc, pts, 5);

    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(brush);
    DeleteObject(pen);
}

// ==============================================================================
// GŁÓWNE OKNO - KANWA
// ==============================================================================
LRESULT CALLBACK GradientApp::main_wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    GradientApp* app = nullptr;
    if (msg == WM_NCCREATE) {
        app = static_cast<GradientApp*>(((LPCREATESTRUCT)lp)->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)app);
    } else {
        app = reinterpret_cast<GradientApp*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    if (!app) return DefWindowProc(hwnd, msg, wp, lp);

    switch (msg) {
    case WM_DESTROY: PostQuitMessage(0); return 0;
    
    // Zapobiega migotaniu całego obszaru roboczego
    case WM_ERASEBKGND: return 1; 

    // Ograniczenie minimalnego rozmiaru okna do 400x300 (Wymóg z zadania)
    case WM_GETMINMAXINFO: {
        LPMINMAXINFO mmi = (LPMINMAXINFO)lp;
        mmi->ptMinTrackSize.x = 400;
        mmi->ptMinTrackSize.y = 300;
        return 0;
    }

    case WM_SIZE: {
        int width = LOWORD(lp);
        int height = HIWORD(lp);
        
        // Responsywny układ z 5-pikselowym marginesem wokół komponentów
        int margin = 5;
        int strip_height = 60;

        // Ustawienie wymiarów dla paska poniżej głównej kanwy
        SetWindowPos(app->m_strip_hwnd, NULL, 
            margin, height - strip_height - margin, 
            width - (2 * margin), strip_height, 
            SWP_NOZORDER);

        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc; GetClientRect(hwnd, &rc);

        // Wyliczenie obszaru samej kanwy (z uwzględnieniem dolnego paska i marginesów)
        int strip_height = 60;
        RECT canvas_rc = { 5, 5, rc.right - 5, rc.bottom - strip_height - 10 };

        // Double Buffering
        HDC memDC = CreateCompatibleDC(hdc);
        HBITMAP memBM = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
        HBITMAP oldBM = (HBITMAP)SelectObject(memDC, memBM);

        // Tło poza kanwą
        FillRect(memDC, &rc, GetSysColorBrush(COLOR_BTNFACE));

        // Rysowanie gradientu na głównej kanwie
        app->draw_multi_gradient(memDC, canvas_rc, false);

        BitBlt(hdc, 0, 0, rc.right, rc.bottom, memDC, 0, 0, SRCCOPY);

        SelectObject(memDC, oldBM);
        DeleteObject(memBM);
        DeleteDC(memDC);
        EndPaint(hwnd, &ps);
        return 0;
    }

    // Odbiór niestandardowego komunikatu od Paska Gradientu
    case WM_APP_GRADIENT_CHANGED: {
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    }

    // Obsługa Menu i Skrótu Ctrl+R
    case WM_COMMAND: {
        if (LOWORD(wp) == 1001) {
            app->reset_gradient();
            InvalidateRect(hwnd, NULL, FALSE);
            InvalidateRect(app->m_strip_hwnd, NULL, FALSE);
        }
        return 0;
    }
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

// ==============================================================================
// NIESTANDARDOWE OKNO GDI - PASEK GRADIENTU
// ==============================================================================
LRESULT CALLBACK GradientApp::strip_wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    // Ponieważ to dziecko, pobieramy wskaźnik aplikacji przez GetParent
    GradientApp* app = reinterpret_cast<GradientApp*>(GetWindowLongPtr(GetParent(hwnd), GWLP_USERDATA));
    if (!app) return DefWindowProc(hwnd, msg, wp, lp);

    switch (msg) {
    case WM_ERASEBKGND: return 1; // Podwójne buforowanie, unikamy migania

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc; GetClientRect(hwnd, &rc);

        HDC memDC = CreateCompatibleDC(hdc);
        HBITMAP memBM = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
        HBITMAP oldBM = (HBITMAP)SelectObject(memDC, memBM);

        // 1. Tło całego paska (zgodnie ze specyfikacją - COLOR_BTNFACE)
        FillRect(memDC, &rc, GetSysColorBrush(COLOR_BTNFACE));

        // 2. Obszar samego "wizualnego" gradientu (np. margines 20px góra/dół dla markerów)
        RECT grad_rc = { 10, 20, rc.right - 10, rc.bottom - 20 };
        app->draw_multi_gradient(memDC, grad_rc, true);
        FrameRect(memDC, &grad_rc, (HBRUSH)GetStockObject(BLACK_BRUSH)); // Opcjonalna ramka

        // 3. Rysowanie uchwytów (Color Stops) na górze i na dole
        int w = grad_rc.right - grad_rc.left;
        for (size_t i = 0; i < app->m_stops.size(); ++i) {
            int x = grad_rc.left + static_cast<int>(app->m_stops[i].pos * w);
            bool is_hovered = (i == app->m_hover_idx);
            
            // Rysujemy marker poniżej gradientu
            app->draw_handle(memDC, x, grad_rc.bottom, 12, 15, app->m_stops[i].color, is_hovered);
        }

        BitBlt(hdc, 0, 0, rc.right, rc.bottom, memDC, 0, 0, SRCCOPY);
        SelectObject(memDC, oldBM);
        DeleteObject(memBM);
        DeleteDC(memDC);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_MOUSEMOVE: {
        int mx = GET_X_LPARAM(lp);
        RECT rc; GetClientRect(hwnd, &rc);
        int w = rc.right - 20; // 10px marginesu bocznego
        int offset_x = 10;

        // Włączanie śledzenia opuszczenia okna (WM_MOUSELEAVE)
        if (!app->m_mouse_tracked) {
            TRACKMOUSEEVENT tme = { sizeof(TRACKMOUSEEVENT), TME_LEAVE, hwnd, 0 };
            TrackMouseEvent(&tme);
            app->m_mouse_tracked = true;
        }

        // Obsługa Przeciągania (Drag)
        if (app->m_drag_idx != -1 && (wp & MK_LBUTTON)) {
            float new_pos = (float)(mx - offset_x) / w;
            
            // Logika "miękkiego" blokowania: nie pozwalamy minąć sąsiednich punktów
            float min_pos = (app->m_drag_idx > 0) ? app->m_stops[app->m_drag_idx - 1].pos + 0.01f : 0.0f;
            float max_pos = (app->m_drag_idx < app->m_stops.size() - 1) ? app->m_stops[app->m_drag_idx + 1].pos - 0.01f : 1.0f;
            
            new_pos = std::max(min_pos, std::min(new_pos, max_pos));
            app->m_stops[app->m_drag_idx].pos = new_pos;

            app->notify_parent();
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }

        // Obsługa efektu Hover
        int old_hover = app->m_hover_idx;
        app->m_hover_idx = -1;
        for (size_t i = 0; i < app->m_stops.size(); ++i) {
            int hx = offset_x + static_cast<int>(app->m_stops[i].pos * w);
            if (std::abs(mx - hx) < 8) { // Promień 8 pikseli na trafienie
                app->m_hover_idx = static_cast<int>(i);
                break;
            }
        }

        if (old_hover != app->m_hover_idx) InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    }

    case WM_MOUSELEAVE: {
        app->m_mouse_tracked = false;
        if (app->m_hover_idx != -1) {
            app->m_hover_idx = -1;
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    }

    case WM_LBUTTONDOWN: {
        app->m_click_pos.x = GET_X_LPARAM(lp);
        app->m_click_pos.y = GET_Y_LPARAM(lp);
        
        if (app->m_hover_idx != -1) {
            app->m_drag_idx = app->m_hover_idx;
            SetCapture(hwnd); // Zatrzymuje mysz "w oknie" podczas przeciągania
        }
        return 0;
    }

    case WM_LBUTTONUP: {
        if (app->m_drag_idx != -1) {
            ReleaseCapture();
            
            // Jeśli puszczono przycisk myszy bardzo blisko punktu startowego, traktujemy to jako zwykłe "Kliknięcie"
            int mx = GET_X_LPARAM(lp);
            if (std::abs(mx - app->m_click_pos.x) < 3) {
                CHOOSECOLOR cc = { sizeof(CHOOSECOLOR) };
                static COLORREF custom_colors[16] = { 0 };
                cc.hwndOwner = hwnd;
                cc.lpCustColors = (LPDWORD)custom_colors;
                cc.rgbResult = app->m_stops[app->m_drag_idx].color;
                cc.Flags = CC_FULLOPEN | CC_RGBINIT;

                if (ChooseColor(&cc)) {
                    app->m_stops[app->m_drag_idx].color = cc.rgbResult;
                    app->notify_parent();
                }
            }
            app->m_drag_idx = -1;
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    }

    case WM_LBUTTONDBLCLK: {
        int mx = GET_X_LPARAM(lp);
        RECT rc; GetClientRect(hwnd, &rc);
        float new_pos = (float)(mx - 10) / (rc.right - 20);
        new_pos = std::max(0.0f, std::min(new_pos, 1.0f));

        COLORREF new_color = app->interpolate_color(new_pos);
        app->m_stops.push_back({ new_pos, new_color });
        std::sort(app->m_stops.begin(), app->m_stops.end()); // Wymaga <algorithm>
        
        app->notify_parent();
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    }

    case WM_RBUTTONDOWN: {
        if (app->m_hover_idx != -1 && app->m_stops.size() > 2) {
            app->m_stops.erase(app->m_stops.begin() + app->m_hover_idx);
            app->m_hover_idx = -1;
            app->notify_parent();
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    }

    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

// Punkt startowy
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int nCmdShow) {
    GradientApp app(hInstance);
    return app.run(nCmdShow);
}
