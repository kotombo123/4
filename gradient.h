#pragma once
#include <windows.h>
#include <string>

class GradientApp
{
private:
    bool register_classes();

    static LRESULT CALLBACK window_proc_static(HWND window, UINT message, WPARAM wparam, LPARAM lparam);
    LRESULT window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam);

    // Wywo³anie systemowego okna dialogowego
    COLORREF open_color_chooser(HWND owner, COLORREF initial_color);

    // Obs³uga rysowania
    void on_paint(HWND window);
    void on_draw_item(DRAWITEMSTRUCT* dis); // Nowa funkcja do rysowania w³asnych przycisków!

    HINSTANCE m_instance;
    HWND m_main;

    // Uchwyty przycisków
    HWND m_btn_color1;
    HWND m_btn_color2;

    // Stan aplikacji
    COLORREF m_color1;
    COLORREF m_color2;

    // Obszary robocze
    RECT m_draw_area;   // Obszar z gradientem
    RECT m_bottom_area; // Obszar dolnego paska

    static constexpr int ID_BTN_C1 = 101;
    static constexpr int ID_BTN_C2 = 102;

public:
    GradientApp(HINSTANCE instance);
    ~GradientApp();
    int run(int show_command);
};