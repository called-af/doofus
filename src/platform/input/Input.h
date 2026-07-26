#pragma once
#include <SDL3/SDL.h>

class Input
{
public:
    static bool w, a, s, d;
    static bool b;   
    static bool bPressed;
    static bool oPressed;
    static bool v;
    static bool vPressed;
    static bool c;
    static bool lctrl;
    static bool lshift;

    static bool left_click;
    static bool right_click;
    static bool space;
    static bool escape;
    static bool escapePressed;
    static bool bracketLeftPressed;
    static bool bracketRightPressed;

    static float mouseX;
    static float mouseY; 

    static void beginFrame();
    static void handleEvent(SDL_Event& e);
};