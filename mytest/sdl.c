_Nameprefix sdl = "SDL_";

_Capture _Nameprefix sdl {
    #include <SDL3/SDL.h>
    #include <stdio.h>
}

int main(int argc, char *argv[])
{
    if (!sdl::Init(SDL_INIT_VIDEO)) {
        printf("SDL_Init Error: %s\n", sdl::GetError());
        return 1;
    }

    sdl::Window *window = sdl::CreateWindow(
        "SDL Window",
        800,
        600,
        0
    );

    if (!window) {
        printf("SDL_CreateWindow Error: %s\n", sdl::GetError());
        sdl::Quit();
        return 1;
    }

    sdl::Renderer *renderer = sdl::CreateRenderer(window, 0);
    if (!renderer) {
        printf("SDL_CreateRenderer Error: %s\n", sdl::GetError());
        sdl::DestroyWindow(window);
        sdl::Quit();
        return 1;
    }

    // Set background color (static)
    sdl::SetRenderDrawColor(renderer, 30, 30, 120, 255);
    sdl::RenderClear(renderer);
    sdl::RenderPresent(renderer);

    // Wait until user closes window
    sdl::Event e;
    int running = 1;
    while (running) {
        while (sdl::PollEvent(&e)) {
            if (e.type == sdl::EVENT_QUIT) {
                running = 0;
            }
        }
    }

    sdl::DestroyRenderer(renderer);
    sdl::DestroyWindow(window);
    sdl::Quit();
    return 0;
}