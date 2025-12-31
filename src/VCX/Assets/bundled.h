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
            "assets/bvh/turn_right.bvh",
            "assets/bvh/walk_forward_resampled.bvh",
            "assets/bvh/turn_left.bvh",
            "assets/bvh/spin_counter_clockwise.bvh",
            "assets/bvh/walk.bvh",
            "assets/bvh/spin_clockwise.bvh",
            "assets/bvh/run_forward_resampled.bvh",
        })
    };

    enum class ExampleBVH {
        TurnRight,
        WalkForwardResampled,
        TurnLeft,
        SpinCounterClockwise,
        Walk,
        SpinClockwise,
        RunForwardResampled,
    };

    inline constexpr auto DatabasePath {
        std::to_array<std::string_view>({
            "assets/data/database.bin",
        })
    };

    inline constexpr auto FeaturePath {
        std::to_array<std::string_view>({
            "assets/data/features.bin",
        })
    };

    inline constexpr auto CharacterPath {
        std::to_array<std::string_view>({
            "assets/data/character.bin",
        })
    };

}
