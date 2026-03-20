#include <windows.h>
#include "gradient.h"

int WINAPI wWinMain(HINSTANCE instance,
    HINSTANCE /*prevInstance*/,
    LPWSTR /*command_line*/,
    int show_command) {

    GradientApp app{ instance };
    return app.run(show_command);
}