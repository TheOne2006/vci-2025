#pragma once

#include <vector>

#include "Engine/app.h"
#include "Labs/Common/UI.h"
#include "Labs/MotionMatching/CaseMotionMatching.h"

namespace VCX::Labs::MotionMatching {
    class App : public Engine::IApp {
    private:
        Common::UI _ui;

        CaseMotionMatching _caseMotionMatching;

        std::size_t _caseId = 0;

        std::vector<std::reference_wrapper<Common::ICase>> _cases = { _caseMotionMatching };

    public:
        App();

        void OnFrame() override;
    };
}
