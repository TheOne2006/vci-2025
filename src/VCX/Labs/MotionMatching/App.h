#pragma once

#include <vector>

#include "Engine/app.h"
#include "Labs/Common/UI.h"
#include "Labs/MotionMatching/CaseBVH.h"

namespace VCX::Labs::MotionMatching {
    class App : public VCX::Engine::IApp {
    private:
        Common::UI _ui;

        CaseBVH _caseBVH;

        std::size_t _caseId = 0;

        std::vector<std::reference_wrapper<Common::ICase>> _cases = {
            _caseBVH,
        };

    public:
        App();

        void OnFrame() override;
    };
}