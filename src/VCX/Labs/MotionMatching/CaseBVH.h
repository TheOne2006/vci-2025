#pragma once

#include "Labs/Common/ICase.h"

namespace VCX::Labs::MotionMatching {
    class CaseBVH : public Common::ICase {
    public:
        CaseBVH();

        virtual std::string_view const GetName() override { return "Image BVH"; }

        virtual Common::CaseRenderResult OnRender(std::pair<std::uint32_t, std::uint32_t> const desiredSize) override;

    private:
    };
} // namespace VCX::Labs::MotionMatching
