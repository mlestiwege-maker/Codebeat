#include "main_window.h"
#include "imgui.h"
#include <sstream>
#include <cmath> // for sin()

// Include Brain/Response if you want the GUI to reply
#include "brain.h"
#include "response.h"

MainWindow::MainWindow() : listening(false), devMode(false), time(0.0f) {
    inputBuffer[0] = '\0';
}

// --- Render function called every frame ---
void MainWindow::Render(float deltaTime) {
    time += deltaTime;

    ImGui::Begin("Codebeat Assistant");

    // Logo
    TextCentered("🎵 Codebeat AI Assistant");
    ImGui::Separator();

    // Chat log window
    ImGui::BeginChild("ChatLog", ImVec2(0, 300), true);
    ImGui::TextUnformatted(chatLog.c_str());
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
        ImGui::SetScrollHereY(1.0f); // Auto-scroll
    ImGui::EndChild();

    // Input field
    ImGui::PushItemWidth(-100);
    if (ImGui::InputText("##input", inputBuffer, IM_ARRAYSIZE(inputBuffer),
                         ImGuiInputTextFlags_EnterReturnsTrue)) {

        // Append user message
        std::stringstream ss;
        ss << "You: " << inputBuffer << "\n";
        chatLog += ss.str();

        // Process input through Brain
        Brain brain;
        std::string reply = brain.process(inputBuffer);

        // Append AI reply
        ss.str(""); // Clear stringstream
        ss << "Codebeat: " << reply << "\n";
        chatLog += ss.str();

        inputBuffer[0] = '\0';
    }
    ImGui::PopItemWidth();

    ImGui::SameLine();

    // Glowing mic button
    float glow = (std::sin(time * 6.0f) * 0.5f + 0.5f);
    ImVec4 micColor = listening ? ImVec4(0.2f, 1.0f * glow, 0.2f, 1.0f)
                                : ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_Button, micColor);
    if (ImGui::Button(listening ? "🟢 Stop" : "🎤 Listen", ImVec2(80, 0))) {
        listening = !listening;
        chatLog += listening ? "🟢 Listening...\n" : "🔴 Stopped listening.\n";
    }
    ImGui::PopStyleColor();

    ImGui::Separator();

    // Dev Mode toggle
    ImGui::Checkbox("Dev Mode", &devMode);

    ImGui::End();
}

// --- Helper function to center text ---
void MainWindow::TextCentered(const char* text) {
    ImVec2 size = ImGui::CalcTextSize(text);
    float avail = ImGui::GetContentRegionAvail().x;
    ImGui::SetCursorPosX((avail - size.x) * 0.5f);
    ImGui::TextUnformatted(text);
}
