#include "Assets/bundled.h"
#include "Labs/MotionMatching/App.h"

int main() {
    using namespace VCX;
    return Engine::RunApp<Labs::MotionMatching::App>(Engine::AppContextOptions {
        .Title      = "VCX Labs: Motion Matching",
        .WindowSize = { 1200, 800 },
        .FontSize   = 16,

        .IconFileNames = Assets::DefaultIcons,
        .FontFileNames = Assets::DefaultFonts,
    });
}
