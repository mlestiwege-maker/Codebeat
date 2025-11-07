#include <SDL2/SDL.h>
#include <GL/glew.h>
#include "imgui.h"
#include "imgui_impl_sdl.h"
#include "imgui_impl_opengl3.h"
#include "main_window.h"
#include <iostream>

int main(int argc, char* argv[]) {
    // --- SDL2 Initialization ---
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        std::cerr << "SDL Init Error: " << SDL_GetError() << std::endl;
        return -1;
    }

    // --- OpenGL Attributes ---
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

    SDL_Window* window = SDL_CreateWindow("Codebeat",
                                          SDL_WINDOWPOS_CENTERED,
                                          SDL_WINDOWPOS_CENTERED,
                                          800, 600,
                                          SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);

    if (!window) {
        std::cerr << "Failed to create SDL Window: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return -1;
    }

    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, gl_context);
    SDL_GL_SetSwapInterval(1); // Enable VSync

    if (glewInit() != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW\n";
        SDL_GL_DeleteContext(gl_context);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }

    // --- ImGui Setup ---
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();
    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init("#version 130");

    // --- Main Window ---
    MainWindow mainWindow;

    bool running = true;
    Uint64 lastTime = SDL_GetPerformanceCounter();

    // --- Main Loop ---
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT)
                running = false;
        }

        // --- Delta Time Calculation ---
        Uint64 now = SDL_GetPerformanceCounter();
        float deltaTime = static_cast<float>((now - lastTime) / static_cast<double>(SDL_GetPerformanceFrequency()));
        lastTime = now;

        // --- ImGui Frame Start ---
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame(window);
        ImGui::NewFrame();

        // --- Render GUI ---
        mainWindow.Render(deltaTime);

        // --- Render to Screen ---
        ImGui::Render();
        int display_w, display_h;
        SDL_GetWindowSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);
    }

    // --- Cleanup ---
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
