#include "Labs/MotionMatching/CaseBVH.h"
#include "Labs/Common/ImageRGB.h"

namespace VCX::Labs::MotionMatching {
    CaseBVH::CaseBVH() {
    }

    Common::CaseRenderResult CaseBVH::OnRender(std::pair<std::uint32_t, std::uint32_t> const desiredSize) {
        // Create a checkerboard texture as placeholder
        auto const width  = desiredSize.first;
        auto const height = desiredSize.second;

        return Common::CaseRenderResult {
            .Fixed     = false,
            .Flipped   = false,
            .Image     = Common::ImageRGB(width, height),
            .ImageSize = { width, height }
        };
    }
}
