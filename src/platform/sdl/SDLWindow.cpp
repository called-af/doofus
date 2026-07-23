#include "SDLWindow.h"
#include "../../platform/input/Input.h"
#include "../../core/Setting.h"

#include <glad/gl.h>

#include <iostream>

SDLWindow::SDLWindow()
{
}

SDLWindow::~SDLWindow()
{
    if (glContext)
        SDL_GL_DestroyContext(glContext);

    if (window)
        SDL_DestroyWindow(window);

    SDL_Quit();
}

bool SDLWindow::init(
    int width,
    int height,
    const char *title)
{
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        std::cout
            << "SDL init failed\n";

        return false;
    }

    SDL_GL_SetAttribute(
        SDL_GL_CONTEXT_MAJOR_VERSION,
        4);

    SDL_GL_SetAttribute(
        SDL_GL_CONTEXT_MINOR_VERSION,
        6);

    SDL_GL_SetAttribute(
        SDL_GL_CONTEXT_PROFILE_MASK,
        SDL_GL_CONTEXT_PROFILE_CORE);

    Uint32 flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    if (Setting::fullscreen)
        flags |= SDL_WINDOW_FULLSCREEN;

    window = SDL_CreateWindow(
        title,
        width,
        height,
        flags);

    if (!window)
    {
        std::cout
            << "Window failed\n";

        return false;
    }

    glContext =
        SDL_GL_CreateContext(window);

    if (!glContext)
    {
        std::cout
            << "GL Context failed\n";

        return false;
    }

    SDL_GL_MakeCurrent(
        window,
        glContext);

    // VSYNC
    SDL_GL_SetSwapInterval(
        Setting::vsync ? 1 : 0);
    int interval = 0;
    SDL_GL_GetSwapInterval(&interval);

    std::cout
        << "Swap Interval = "
        << interval
        << std::endl;

    int pxW, pxH;
    SDL_GetWindowSizeInPixels(window, &pxW, &pxH);
    Setting::windowWidth = pxW;
    Setting::windowHeight = pxH;

    return true;
}

void SDLWindow::pollEvents()
{
    SDL_Event e;

    while (SDL_PollEvent(&e))
    {
        Input::handleEvent(e);

        if (e.type == SDL_EVENT_QUIT)
            closed = true;

        if (e.type == SDL_EVENT_WINDOW_RESIZED)
        {
            int pxW, pxH;
            SDL_GetWindowSizeInPixels(window, &pxW, &pxH);

            glViewport(0, 0, pxW, pxH);

            Setting::windowWidth = pxW;
            Setting::windowHeight = pxH;
        }
    }
}

void SDLWindow::swapBuffers()
{
    SDL_GL_SwapWindow(window);
}

bool SDLWindow::shouldClose()
{
    return closed;
}

int SDLWindow::getWidth()
{
    int width;

    SDL_GetWindowSize(
        window,
        &width,
        nullptr);

    return width;
}

int SDLWindow::getHeight()
{
    int height;

    SDL_GetWindowSize(
        window,
        nullptr,
        &height);

    return height;
}

SDL_Window *SDLWindow::getWindow()
{
    return window;
}