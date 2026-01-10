#pragma once

#include <vector>

#include "Engine/app.h"
#include "Labs/Common/UI.h"
#include "Labs/MotionMatching/CaseBVH.h"
#include "Labs/MotionMatching/CaseBVHSkinned.h"
#include "Labs/MotionMatching/CaseMotionMatching.h"

namespace VCX::Labs::MotionMatching {
    class App : public VCX::Engine::IApp {
    private:
        Common::UI _ui;

        CaseBVH            _caseBVH;
        CaseBVHSkinned     _caseBVHSkinned;
        CaseMotionMatching _caseBVHMotionMatching;

        std::size_t _caseId = 0;

        std::vector<std::reference_wrapper<Common::ICase>> _cases = {
            _caseBVH,
            _caseBVHSkinned,
            _caseBVHMotionMatching,
        };

    public:
        App();

        void OnFrame() override;
    };
}