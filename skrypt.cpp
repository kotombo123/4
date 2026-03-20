// ==============================================================================
// PLIK: main.cpp
// Aplikacja WinAPI rysująca płynny gradient z własnoręcznie rysowanymi przyciskami.
// Całość w jednym pliku dla łatwiejszej kompilacji i analizy.
// ==============================================================================

#include <windows.h>      // Główne nagłówki systemu Windows
#include <windowsx.h>     // Makra pomocnicze (np. GET_X_LPARAM)
#include <commdlg.h>      // Okna dialogowe systemu (np. ChooseColor)
#include <string>         // Obsługa std::wstring

// Dyrektywy informujące linker (konsolidator) o konieczności dołączenia bibliotek.
// Bez tego otrzymalibyśmy błędy "unresolved external symbol" przy kompilacji.
#pragma comment(lib, "Msimg32.lib")  // Biblioteka wymagana przez funkcję GradientFill
#pragma comment(lib, "Comdlg32.lib") // Biblioteka wymagana przez funkcję ChooseColor

// ==============================================================================
// 1. DEKLARACJA KLASY APLIKACJI
// ==============================================================================
class GradientApp
{
private:
    HINSTANCE m_instance; // Uchwyt instancji (identyfikator naszego programu w systemie)
    HWND m_main;          // Uchwyt (Handle) naszego głównego okna

    // Uchwyty do okien potomnych (naszych przycisków)
    HWND m_btn_color1;
    HWND m_btn_color2;

    // Przechowujemy aktualnie wybrane kolory gradientu w formacie RGB
    COLORREF m_color1;
    COLORREF m_color2;

    // Struktury RECT przechowują współrzędne (lewo, góra, prawo, dół).
    // Dzielimy okno na dwie strefy: górną na gradient i dolną na przyciski.
    RECT m_draw_area;   
    RECT m_bottom_area; 

    // Identyfikatory naszych przycisków (żebyśmy wiedzieli, który został kliknięty)
    static constexpr int ID_BTN_C1 = 101;
    static constexpr int ID_BTN_C2 = 102;

    // Metody wewnętrzne
    bool register_classes();
    COLORREF open_color_chooser(HWND owner, COLORREF initial_color);
    void on_paint(HWND window);
    void on_draw_item(DRAWITEMSTRUCT* dis);

    // Procedury przetwarzające komunikaty od systemu operacyjnego
    static LRESULT CALLBACK window_proc_static(HWND window, UINT message, WPARAM wparam, LPARAM lparam);
    LRESULT window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam);

public:
    GradientApp(HINSTANCE instance);
    ~GradientApp();
    int run(int show_command);
};

// ==============================================================================
// 2. IMPLEMENTACJA METOD KLASY
// ==============================================================================

// Konstruktor - ustawia wartości początkowe i tworzy interfejs użytkownika
GradientApp::GradientApp(HINSTANCE instance)
    : m_instance{ instance }, m_main{ nullptr },
    m_btn_color1{ nullptr }, m_btn_color2{ nullptr },
    m_color1{ RGB(0, 200, 150) }, m_color2{ RGB(70, 0, 130) } // Domyślne kolory startowe
{
    // Krok 1: Zanim stworzymy okno, musimy zarejestrować jego "szablon" w systemie
    register_classes();

    // WS_CLIPCHILDREN mówi systemowi: "Jeśli rysuję tło głównego okna, 
    // to omijaj miejsca, w których leżą inne kontrolki (np. przyciski)". 
    // Zapobiega to nakładaniu się grafiki i migotaniu przycisków.
    DWORD style = WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN;

    // Krok 2: Tworzymy główne okno aplikacji
    m_main = CreateWindowExW(
        0,                             // Rozszerzone style (brak)
        L"SimpleGradientClass",        // Nazwa zarejestrowanej klasy (musi się zgadzać z tą w register_classes!)
        L"Edytor Gradientu (Single File)", // Tytuł okna na pasku
        style,                         // Style okna (ramki, minimalizacja, itp.)
        CW_USEDEFAULT, CW_USEDEFAULT,  // Pozycja X i Y (niech Windows sam zdecyduje)
        800, 500,                      // Szerokość i wysokość startowa
        nullptr,                       // Uchwyt okna rodzica (nie mamy)
        nullptr,                       // Uchwyt menu (brak)
        m_instance,                    // Instancja naszej aplikacji
        this                           // Przekazujemy wskaźnik na NASZ obiekt (użyjemy tego w WM_NCCREATE)
    );

    // Krok 3: Tworzymy przyciski wewnątrz głównego okna
    // Styl BS_OWNERDRAW oznacza, że całkowicie przejmujemy kontrolę nad wyglądem przycisku.
    m_btn_color1 = CreateWindowExW(
        0, L"BUTTON", L"Kolor 1",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        0, 0, 0, 0, // Pozycję i rozmiar ustawimy dynamicznie w WM_SIZE
        m_main, reinterpret_cast<HMENU>(ID_BTN_C1), m_instance, nullptr
    );

    m_btn_color2 = CreateWindowExW(
        0, L"BUTTON", L"Kolor 2",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        0, 0, 0, 0, 
        m_main, reinterpret_cast<HMENU>(ID_BTN_C2), m_instance, nullptr
    );
}

GradientApp::~GradientApp() {}

// Rejestruje "Klasę Okna" (Window Class) - to taki wzorzec, z którego Windows tworzy okna
bool GradientApp::register_classes() {
    WNDCLASSEXW desc{}; // Inicjalizacja zerami
    desc.cbSize = sizeof(WNDCLASSEXW);
    desc.lpfnWndProc = window_proc_static; // Wskazanie na funkcję obsługującą zdarzenia!
    desc.hInstance = m_instance;
    desc.hCursor = LoadCursorW(nullptr, (LPCWSTR)IDC_ARROW); // Standardowy kursor myszy
    desc.hbrBackground = nullptr; // BARDZO WAŻNE: nullptr oznacza, że sami rysujemy tło (brak migotania)
    desc.lpszClassName = L"SimpleGradientClass"; // Nazwa identyfikująca nasz wzorzec

    return RegisterClassExW(&desc) != 0;
}

// Główna pętla programu - utrzymuje program przy życiu i nasłuchuje zdarzeń
int GradientApp::run(int show_command) {
    ShowWindow(m_main, show_command); // Pokazujemy ukryte dotąd okno

    MSG msg{};
    BOOL result = TRUE;
    // GetMessageW pobiera komunikaty (kliknięcia, ruchy myszą) z kolejki systemu.
    // Zwraca 0, gdy program dostanie sygnał do zamknięcia (WM_QUIT).
    while ((result = GetMessageW(&msg, nullptr, 0, 0)) != 0) {
        if (result == -1) return EXIT_FAILURE; // Błąd krytyczny
        TranslateMessage(&msg); // Przekształca wciśnięcia klawiszy na znaki (jeśli potrzebne)
        DispatchMessageW(&msg); // Wysyła komunikat do naszej funkcji window_proc_static
    }
    return EXIT_SUCCESS;
}

// Wywołuje systemowe okienko do wyboru koloru
COLORREF GradientApp::open_color_chooser(HWND owner, COLORREF initial_color) {
    CHOOSECOLOR cc{};
    static COLORREF custom_colors[16]{}; // Tablica na "własne kolory" użytkownika (musi być static!)

    cc.lStructSize = sizeof(cc);
    cc.hwndOwner = owner; // Okno rodzica (blokuje je dopóki nie zamkniemy dialogu)
    cc.lpCustColors = (LPDWORD)custom_colors;
    cc.rgbResult = initial_color; // Kolor zaznaczony po otwarciu
    cc.Flags = CC_FULLOPEN | CC_RGBINIT; // CC_FULLOPEN rozwija od razu paletę niestandardową

    if (ChooseColor(&cc)) {
        return cc.rgbResult; // Użytkownik kliknął OK
    }
    return initial_color; // Użytkownik kliknął Anuluj
}

// Ręczne rysowanie przycisków (wywoływane przez komunikat WM_DRAWITEM)
void GradientApp::on_draw_item(DRAWITEMSTRUCT* dis) {
    // Sprawdzamy, czy komunikat dotyczy jednego z naszych dwóch przycisków
    if (dis->CtlID == ID_BTN_C1 || dis->CtlID == ID_BTN_C2) {
        HDC dc = dis->hDC;           // Kontekst urządzenia (nasze "płótno" dla tego przycisku)
        RECT rc = dis->rcItem;       // Wymiary przycisku
        bool is_pressed = (dis->itemState & ODS_SELECTED); // Stan wciśnięcia lewego przycisku myszy

        // 1. Wypełniamy całe tło przycisku szarym kolorem systemowym
        FillRect(dc, &rc, GetSysColorBrush(COLOR_BTNFACE));

        // 2. Rysujemy systemową ramkę 3D. Zmieniamy jej wygląd zależnie od stanu kliknięcia
        DrawEdge(dc, &rc, is_pressed ? EDGE_SUNKEN : EDGE_RAISED, BF_RECT);

        // Tworzymy iluzję fizycznego przycisku - jeśli kliknięty, przesuwamy zawartość nieco w prawo i w dół
        int offset = is_pressed ? 1 : 0;

        // 3. Rysujemy kwadracik z podglądem wybranego koloru
        RECT swatch_rc = { rc.left + 8 + offset, rc.top + 8 + offset, rc.left + 35 + offset, rc.bottom - 8 + offset };
        COLORREF btn_color = (dis->CtlID == ID_BTN_C1) ? m_color1 : m_color2;

        HBRUSH color_brush = CreateSolidBrush(btn_color); // Tworzymy pędzel w danym kolorze
        FillRect(dc, &swatch_rc, color_brush);            // Malujemy kwadrat
        DeleteObject(color_brush);                        // ZASADA WinAPI: zawsze zwalniaj utworzone obiekty GDI!

        // Czarna ramka dla lepszego kontrastu wokół kwadracika z kolorem
        FrameRect(dc, &swatch_rc, (HBRUSH)GetStockObject(BLACK_BRUSH));

        // 4. Rysujemy tekst na przycisku
        std::wstring text = (dis->CtlID == ID_BTN_C1) ? L"Wybierz Kolor 1" : L"Wybierz Kolor 2";
        SetBkMode(dc, TRANSPARENT); // Ustawiamy tło tekstu na przezroczyste (inaczej byłoby białe pole)

        RECT text_rc = rc;
        text_rc.left = swatch_rc.right + 10; // Przesuwamy tekst, aby nie nachodził na kolorowy kwadracik
        text_rc.top += offset;
        text_rc.bottom += offset;

        // Rysowanie tekstu wyrównanego do lewej (DT_LEFT) i wyśrodkowanego w pionie (DT_VCENTER)
        DrawTextW(dc, text.c_str(), -1, &text_rc, DT_SINGLELINE | DT_VCENTER | DT_LEFT);

        // 5. Rysowanie systemowej przerywanej ramki wokół tekstu, gdy używamy nawigacji z klawiatury (Tab)
        if (dis->itemState & ODS_FOCUS) {
            RECT focus_rc = rc;
            InflateRect(&focus_rc, -4, -4); // Pomniejszamy obszar ramki o 4 piksele z każdej strony
            DrawFocusRect(dc, &focus_rc);
        }
    }
}

// Główna funkcja rysująca interfejs okna (wywoływana przez WM_PAINT)
void GradientApp::on_paint(HWND window) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(window, &ps); // Rozpoczęcie malowania

    // ==============================================================================
    // TECHNIKA: DOUBLE BUFFERING (PODWÓJNE BUFOROWANIE)
    // Zamiast rysować piksel po pikselu bezpośrednio na ekranie (co powoduje migotanie),
    // tworzymy wirtualną bitmapę w pamięci RAM, malujemy po niej, a na koniec
    // "kopiujemy" ją w ułamku sekundy na ekran.
    // ==============================================================================
    
    RECT rc;
    GetClientRect(window, &rc); // Pobieramy aktualne wymiary okna

    HDC memDC = CreateCompatibleDC(hdc); // Tworzymy kontekst pamięci kompatybilny z naszym ekranem
    HBITMAP memBM = CreateCompatibleBitmap(hdc, rc.right, rc.bottom); // Wirtualna bitmapa wielkości okna
    HBITMAP oldBM = (HBITMAP)SelectObject(memDC, memBM); // "Wkładamy" bitmapę do wirtualnego kontekstu

    // 1. Wypełnienie dolnego obszaru narzędziowego szarym kolorem systemowym
    FillRect(memDC, &m_bottom_area, GetSysColorBrush(COLOR_BTNFACE));

    // 2. Rysowanie przejścia tonalnego w obszarze górnym (m_draw_area)
    if (m_draw_area.right > 0 && m_draw_area.bottom > 0) {
        
        TRIVERTEX vertex[2]; // Tablica dwóch wierzchołków definiujących gradient

        // Lewy górny róg - Kolor 1 (formatowanie rzutuje na COLOR16 przez przesunięcie bitowe << 8)
        vertex[0] = { m_draw_area.left, m_draw_area.top,
                     (COLOR16)(GetRValue(m_color1) << 8),
                     (COLOR16)(GetGValue(m_color1) << 8),
                     (COLOR16)(GetBValue(m_color1) << 8), 0 };

        // Prawy dolny róg - Kolor 2
        vertex[1] = { m_draw_area.right, m_draw_area.bottom,
                     (COLOR16)(GetRValue(m_color2) << 8),
                     (COLOR16)(GetGValue(m_color2) << 8),
                     (COLOR16)(GetBValue(m_color2) << 8), 0 };

        GRADIENT_RECT gRect = { 0, 1 }; // Które indeksy wierzchołków łączymy ze sobą
        
        // Funkcja WinAPI realizująca sam gradient (Kierunek poziomy: GRADIENT_FILL_RECT_H)
        GradientFill(memDC, vertex, 2, &gRect, 1, GRADIENT_FILL_RECT_H);
    }

    // 3. PRZERZUT NA EKRAN: Skopiowanie całej wirtualnej pamięci wprost na okno fizyczne
    BitBlt(hdc, 0, 0, rc.right, rc.bottom, memDC, 0, 0, SRCCOPY);

    // ==============================================================================
    // SPRZĄTANIE PAMIĘCI - Absolutnie konieczne w WinAPI!
    // ==============================================================================
    SelectObject(memDC, oldBM); // Przywracamy stary obiekt
    DeleteObject(memBM);        // Usuwamy naszą bitmapę z RAMu
    DeleteDC(memDC);            // Usuwamy wirtualny kontekst

    EndPaint(window, &ps); // Zakończenie malowania
}

// ==============================================================================
// 3. PRZETWARZANIE KOMUNIKATÓW (MÓZG PROGRAMU)
// ==============================================================================

// Most pomiędzy "czystym językiem C" jakiego wymaga WinAPI, a programowaniem zorientowanym obiektowo (C++)
LRESULT GradientApp::window_proc_static(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    GradientApp* app = nullptr;

    // Komunikat WM_NCCREATE jest wysyłany ZANIM okno się w ogóle pojawi.
    if (message == WM_NCCREATE) {
        // Z parametru lparam wyciągamy wskaźnik "this", który podaliśmy przy CreateWindowExW
        auto p = reinterpret_cast<LPCREATESTRUCTW>(lparam);
        app = static_cast<GradientApp*>(p->lpCreateParams);
        
        // Zapisujemy ten wskaźnik w "pamięci" samego okna
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
    }
    else {
        // Dla wszystkich późniejszych komunikatów odczytujemy zapisany wskaźnik z okna
        app = reinterpret_cast<GradientApp*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    }

    // Jeśli udało się przypiąć wskaźnik, kierujemy ruch do "prawdziwej" metody klasy
    if (app != nullptr) return app->window_proc(window, message, wparam, lparam);
    
    // Fallback dla systemowych rzeczy
    return DefWindowProcW(window, message, wparam, lparam);
}

// Główna funkcja logiki - odpowiada na kliknięcia, ruchy oknem, etc.
LRESULT GradientApp::window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
    
    // Gdy użytkownik wciśnie "X" w rogu okna
    case WM_CLOSE: 
        DestroyWindow(window); // Niszczy okno i emituje komunikat WM_DESTROY
        return 0; 
        
    // Gdy okno jest niszczone w pamięci
    case WM_DESTROY: 
        PostQuitMessage(EXIT_SUCCESS); // Przerywa pętlę GetMessageW w funkcji run()
        return 0;

    // ==============================================================================
    // WAŻNE: Odpowiedź `return 1` oznacza "Ja już wyczyściłem tło, Windowsie nie rób tego".
    // Zapobiega to mignięciu całego ekranu na czysto biało podczas rozszerzania okna.
    // ==============================================================================
    case WM_ERASEBKGND: 
        return 1;

    // Wywoływane zawsze, gdy wielkość okna ulegnie zmianie
    case WM_SIZE: {
        int width = LOWORD(lparam);  // Szerokość wyciągnięta z młodszego słowa lparam
        int height = HIWORD(lparam); // Wysokość wyciągnięta ze starszego słowa lparam

        int bar_height = 60; // Stała wysokość dolnego szarego paska
        int btn_width = 160; // Szerokość przycisku
        int btn_height = 40; // Wysokość przycisku

        // Aktualizujemy prostokąty z obszarami
        m_draw_area = { 0, 0, width, height - bar_height };
        m_bottom_area = { 0, height - bar_height, width, height };

        // Środek w pionie na dolnym pasku
        int btn_y = height - bar_height + (bar_height - btn_height) / 2;

        // Przesuwamy fizycznie okienka przycisków
        SetWindowPos(m_btn_color1, nullptr, 20, btn_y, btn_width, btn_height, SWP_NOZORDER);
        SetWindowPos(m_btn_color2, nullptr, width - btn_width - 20, btn_y, btn_width, btn_height, SWP_NOZORDER);

        // Informujemy system, że okno "straciło ważność" i trzeba je narysować od nowa
        InvalidateRect(window, nullptr, FALSE);
        return 0;
    }

    // Windows informuje nas, że obszar okna jest "brudny" i trzeba odświeżyć grafikę
    case WM_PAINT: {
        on_paint(window); // Odpalamy naszą procedurę z Double Bufferingiem
        return 0;
    }

    // Windows pyta nas jak narysować kontrolkę z flagą BS_OWNERDRAW
    case WM_DRAWITEM: {
        DRAWITEMSTRUCT* dis = reinterpret_cast<DRAWITEMSTRUCT*>(lparam);
        on_draw_item(dis);
        return TRUE; // Wymagany typ zwrotu, by potwierdzić przetworzenie
    }

    // Obsługa zdarzeń interfejsu (np. wciśnięcie przycisku przez użytkownika)
    case WM_COMMAND: {
        int id = LOWORD(wparam); // ID kontrolki, z którą wszedł w interakcję użytkownik
        
        if (id == ID_BTN_C1) { // Kliknięto przycisk lewy
            m_color1 = open_color_chooser(window, m_color1);
            InvalidateRect(window, nullptr, FALSE); // Po zmianie koloru, przerysuj gradient
        }
        else if (id == ID_BTN_C2) { // Kliknięto przycisk prawy
            m_color2 = open_color_chooser(window, m_color2);
            InvalidateRect(window, nullptr, FALSE);
        }
        return 0;
    }
    } // Koniec switch

    // Wszystkie komunikaty, których nie obsłużyliśmy ręcznie (a są ich setki!),
    // odsyłamy do standardowej, domyślnej procedury Windowsa.
    return DefWindowProcW(window, message, wparam, lparam);
}

// ==============================================================================
// 4. FUNKCJA STARTOWA PROGRAMU (EntryPoint)
// Zamiast standardowego int main() używamy wWinMain dla aplikacji graficznych.
// ==============================================================================
int WINAPI wWinMain(HINSTANCE instance,
    HINSTANCE /*prevInstance*/,  // Przestarzały parametr z czasów Windowsa 3.1, dziś nieużywany
    LPWSTR /*command_line*/,     // Argumenty z wiersza poleceń (np. jak ktoś przeciągnie plik na naszą apkę)
    int show_command) {          // Domyślny stan okna (np. zminimalizowane na starcie)

    // Powołujemy do życia naszą klasę
    GradientApp app{ instance };
    
    // Zaczynamy główną pętlę i blokujemy program w tym miejscu, dopóki się nie zamknie
    return app.run(show_command);
}