#include "Labs/MotionMatching/App.h"

namespace VCX::Labs::MotionMatching {

    App::App():
        _ui(Labs::Common::UIOptions {}) {
    }

    void App::OnFrame() {
        _ui.Setup(_cases, _caseId);
    }
}
