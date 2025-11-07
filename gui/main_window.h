#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H

#include <string>
#include "voice.h"
#include "brain.h"
#include "response.h"

class MainWindow {
public:
    MainWindow();

    // Render the GUI every frame
    // deltaTime = time elapsed since last frame (used for animations)
    void Render(float deltaTime);

private:
    char inputBuffer[256] = {};   // User text input
    std::string chatLog;          // Chat history
    bool listening = false;       // Mic state
    bool devMode = false;         // Dev Mode toggle
    float time = 0.0f;            // For mic glow animation

    // Backend AI objects
    Voice voice;
    Brain brain;
    Response response;

    // Helper function to center text in ImGui windows
    void TextCentered(const char* text);
};

#endif // MAIN_WINDOW_H
