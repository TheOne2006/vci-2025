#pragma once

#include <array>
#include <string_view>

namespace VCX::Assets {
    inline constexpr auto DefaultIcons {
        std::to_array<std::string_view>({
            "assets/images/vcl-logo-32x32.png",
            "assets/images/vcl-logo-48x48.png",
        })
    };

    inline constexpr auto DefaultFonts {
        std::to_array<std::string_view>({
            "assets/fonts/Ubuntu.ttf",
            "assets/fonts/UbuntuMono.ttf",
        })
    };

    inline constexpr auto BVHFiles {
        std::to_array<std::string_view>({
            "assets/bvh/aiming.bvh",
            "assets/bvh/dance.bvh",
            "assets/bvh/fallAndGetUp.bvh",
            "assets/bvh/fight.bvh",
            "assets/bvh/ground.bvh",
            "assets/bvh/multipleActions.bvh",
            "assets/bvh/obstacles.bvh",
            "assets/bvh/pushAndStumble.bvh",
            "assets/bvh/run.bvh",
            "assets/bvh/walk.bvh",
        })
    };

    inline constexpr static const char * bvhNames[] = {
        "Aiming", "Dance", "FallAndGetUp", "Fight", "Ground", "MultipleActions", "Obstacles", "PushAndStumble", "Run", "Walk"
    };

    enum class ExampleBVH {
        Aiming,
        Dance,
        FallAndGetUp,
        Fight,
        Ground,
        MultipleActions,
        Obstacles,
        PushAndStumble,
        Run,
        Walk,
    };

    inline constexpr auto DatabasePath {
        std::to_array<std::string_view>({
            "assets/data/database.bin",
        })
    };

    inline constexpr auto CharacterPath {
        std::to_array<std::string_view>({
            "assets/data/character.bin",
        })
    };

}
