#pragma once

#include "../Animation/character.h"
#include "../DataStructures/array.h"
#include "../MotionMatching/database.h"

#include <cassert>
#include <cstdio>

namespace VCX::Labs::MotionMatching::Core::IO {

    using namespace DataStructures;
    using namespace Animation;
    using namespace MotionMatching;

    // Load character data from binary file
    inline bool load_character(character & c, const char * filename) {
        FILE * f = std::fopen(filename, "rb");
        if (f == nullptr) {
            return false;
        }

        array1d_read(c.positions, f);
        array1d_read(c.normals, f);
        array1d_read(c.texcoords, f);
        array1d_read(c.triangles, f);

        array2d_read(c.bone_weights, f);
        array2d_read(c.bone_indices, f);

        array1d_read(c.bone_rest_positions, f);
        array1d_read(c.bone_rest_rotations, f);

        std::fclose(f);
        return true;
    }

    // Load motion matching database from binary file
    inline bool load_database(database & db, const char * filename) {
        FILE * f = std::fopen(filename, "rb");
        if (f == nullptr) {
            return false;
        }

        array2d_read(db.bone_positions, f);
        array2d_read(db.bone_velocities, f);
        array2d_read(db.bone_rotations, f);
        array2d_read(db.bone_angular_velocities, f);
        array1d_read(db.bone_parents, f);

        array1d_read(db.range_starts, f);
        array1d_read(db.range_stops, f);

        array2d_read(db.contact_states, f);

        std::fclose(f);
        return true;
    }

    // Save motion matching features to binary file
    inline bool save_matching_features(const database & db, const char * filename) {
        FILE * f = std::fopen(filename, "wb");
        if (f == nullptr) {
            return false;
        }

        array2d_write(db.features, f);
        array1d_write(db.features_offset, f);
        array1d_write(db.features_scale, f);

        std::fclose(f);
        return true;
    }

    // Load motion matching features from binary file
    inline bool load_matching_features(database & db, const char * filename) {
        FILE * f = std::fopen(filename, "rb");
        if (f == nullptr) {
            return false;
        }

        array2d_read(db.features, f);
        array1d_read(db.features_offset, f);
        array1d_read(db.features_scale, f);

        std::fclose(f);
        return true;
    }

} // namespace VCX::Labs::MotionMatching::Core::IO
