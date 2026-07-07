#pragma once
#include <string>

class SdlRenderer {
public:
    SdlRenderer(int width, int height, const std::string& fontPath);
    ~SdlRenderer();
    void render(const std::string& status, const std::string& tempo, const std::string& beatPhase, bool windowed);
    void present();
    bool shouldQuit() const;
private:
    // SDL handles (opaque in header for simplicity)
    void* window;
    void* renderer;
    void* font;
    bool quit;
};