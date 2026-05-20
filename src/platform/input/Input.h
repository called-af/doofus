#pragma once

class Input
{
public:
    static bool w;
    static bool a;
    static bool s;
    static bool d;

    static bool space;
    static bool escape;

    static float mouseX;
    static float mouseY;

    static void update();
};