#include "Assets/bundled.h"
#include "Engine/prelude.hpp"
#include "Labs/MotionMatching/App.h"
#include "Labs/MotionMatching/Core/Animation/character.hpp"
#include <iostream>

using namespace VCX::Labs::MotionMatching::Core::Animation;

int main() {
    std::cout << "Hello from Motion-Matching-Test!" << std::endl;

    character c;
    // Use the first character path from bundled assets
    std::string characterPath = std::string(VCX::Assets::CharacterPath[0]);
    std::cout << "Loading character from: " << characterPath << std::endl;

    // Check if file exists first to avoid assertion failure if possible,
    // but character_load uses fopen/assert so we just call it.
    // Ensure we are running from the project root so assets/ is found.
    character_load(c, characterPath.c_str());

    std::cout << "Character loaded successfully." << std::endl;
    std::cout << "Positions size: " << c.positions.size << std::endl;
    std::cout << "Normals size: " << c.normals.size << std::endl;
    std::cout << "Texcoords size: " << c.texcoords.size << std::endl;
    std::cout << "Triangles size: " << c.triangles.size << std::endl;
    std::cout << "Bone weights size: " << c.bone_weights.rows << "x" << c.bone_weights.cols << std::endl;
    std::cout << "Bone indices size: " << c.bone_indices.rows << "x" << c.bone_indices.cols << std::endl;
    std::cout << "Bone rest positions size: " << c.bone_rest_positions.size << std::endl;
    std::cout << "Bone rest rotations size: " << c.bone_rest_rotations.size << std::endl;

    // Inspect Entity Bone (Index 0)
    int entityBoneIndex = 0;
    if (c.bone_rest_positions.size > entityBoneIndex) {
        std::cout << "\n--- Entity Bone (Index 0) ---" << std::endl;
        auto pos = c.bone_rest_positions(entityBoneIndex);
        auto rot = c.bone_rest_rotations(entityBoneIndex);
        std::cout << "Rest Position: (" << pos.x << ", " << pos.y << ", " << pos.z << ")" << std::endl;
        std::cout << "Rest Rotation: (" << rot.w << ", " << rot.x << ", " << rot.y << ", " << rot.z << ")" << std::endl;
    }

    // Inspect Hips Bone (Index 1) for comparison
    int hipsBoneIndex = 1;
    if (c.bone_rest_positions.size > hipsBoneIndex) {
        std::cout << "\n--- Hips Bone (Index 1) ---" << std::endl;
        auto pos = c.bone_rest_positions(hipsBoneIndex);
        auto rot = c.bone_rest_rotations(hipsBoneIndex);
        std::cout << "Rest Position: (" << pos.x << ", " << pos.y << ", " << pos.z << ")" << std::endl;
        std::cout << "Rest Rotation: (" << rot.w << ", " << rot.x << ", " << rot.y << ", " << rot.z << ")" << std::endl;
    }

    return 0;
}
