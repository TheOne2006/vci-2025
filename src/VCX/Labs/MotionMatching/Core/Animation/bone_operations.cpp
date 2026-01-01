#include "bone_operations.hpp"

#include <string>

namespace VCX::Labs::MotionMatching::Core::Animation {

    // ============================================================================
    // Forward Kinematics Functions
    // ============================================================================

    // Simple recursive forward kinematics
    void forward_kinematics(
        vec3 &              bone_position,
        quat &              bone_rotation,
        const slice1d<vec3> bone_positions,
        const slice1d<quat> bone_rotations,
        const slice1d<int>  bone_parents,
        const int           bone) {
        if (bone_parents(bone) != -1) {
            vec3 parent_position;
            quat parent_rotation;

            forward_kinematics(
                parent_position,
                parent_rotation,
                bone_positions,
                bone_rotations,
                bone_parents,
                bone_parents(bone));

            bone_position = quat_mul_vec3(parent_rotation, bone_positions(bone)) + parent_position;
            bone_rotation = quat_mul(parent_rotation, bone_rotations(bone));
        } else {
            bone_position = bone_positions(bone);
            bone_rotation = bone_rotations(bone);
        }
    }

    // Forward kinematics with velocity
    void forward_kinematics_velocity(
        vec3 &              bone_position,
        vec3 &              bone_velocity,
        quat &              bone_rotation,
        vec3 &              bone_angular_velocity,
        const slice1d<vec3> bone_positions,
        const slice1d<vec3> bone_velocities,
        const slice1d<quat> bone_rotations,
        const slice1d<vec3> bone_angular_velocities,
        const slice1d<int>  bone_parents,
        const int           bone) {
        if (bone_parents(bone) != -1) {
            vec3 parent_position;
            vec3 parent_velocity;
            quat parent_rotation;
            vec3 parent_angular_velocity;

            forward_kinematics_velocity(
                parent_position,
                parent_velocity,
                parent_rotation,
                parent_angular_velocity,
                bone_positions,
                bone_velocities,
                bone_rotations,
                bone_angular_velocities,
                bone_parents,
                bone_parents(bone));

            bone_position = quat_mul_vec3(parent_rotation, bone_positions(bone)) + parent_position;
            bone_velocity =
                parent_velocity + quat_mul_vec3(parent_rotation, bone_velocities(bone)) + cross(parent_angular_velocity, quat_mul_vec3(parent_rotation, bone_positions(bone)));
            bone_rotation         = quat_mul(parent_rotation, bone_rotations(bone));
            bone_angular_velocity = quat_mul_vec3(parent_rotation, bone_angular_velocities(bone)) + parent_angular_velocity;
        } else {
            bone_position         = bone_positions(bone);
            bone_velocity         = bone_velocities(bone);
            bone_rotation         = bone_rotations(bone);
            bone_angular_velocity = bone_angular_velocities(bone);
        }
    }

    // Compute forward kinematics for all joints
    void forward_kinematics_full(
        slice1d<vec3>       global_bone_positions,
        slice1d<quat>       global_bone_rotations,
        const slice1d<vec3> local_bone_positions,
        const slice1d<quat> local_bone_rotations,
        const slice1d<int>  bone_parents) {
        for (int i = 0; i < bone_parents.size; i++) {
            // Assumes bones are always sorted from root onwards
            assert(bone_parents(i) < i);

            if (bone_parents(i) == -1) {
                global_bone_positions(i) = local_bone_positions(i);
                global_bone_rotations(i) = local_bone_rotations(i);
            } else {
                vec3 parent_position     = global_bone_positions(bone_parents(i));
                quat parent_rotation     = global_bone_rotations(bone_parents(i));
                global_bone_positions(i) = quat_mul_vec3(parent_rotation, local_bone_positions(i)) + parent_position;
                global_bone_rotations(i) = quat_mul(parent_rotation, local_bone_rotations(i));
            }
        }
    }

    // Compute forward kinematics of just some joints using a mask
    void forward_kinematics_partial(
        slice1d<vec3>       global_bone_positions,
        slice1d<quat>       global_bone_rotations,
        slice1d<bool>       global_bone_computed,
        const slice1d<vec3> local_bone_positions,
        const slice1d<quat> local_bone_rotations,
        const slice1d<int>  bone_parents,
        int                 bone) {
        if (bone_parents(bone) == -1) {
            global_bone_positions(bone) = local_bone_positions(bone);
            global_bone_rotations(bone) = local_bone_rotations(bone);
            global_bone_computed(bone)  = true;
            return;
        }

        if (! global_bone_computed(bone_parents(bone))) {
            forward_kinematics_partial(
                global_bone_positions,
                global_bone_rotations,
                global_bone_computed,
                local_bone_positions,
                local_bone_rotations,
                bone_parents,
                bone_parents(bone));
        }

        vec3 parent_position        = global_bone_positions(bone_parents(bone));
        quat parent_rotation        = global_bone_rotations(bone_parents(bone));
        global_bone_positions(bone) = quat_mul_vec3(parent_rotation, local_bone_positions(bone)) + parent_position;
        global_bone_rotations(bone) = quat_mul(parent_rotation, local_bone_rotations(bone));
        global_bone_computed(bone)  = true;
    }

    // Same but including velocity
    void forward_kinematics_velocity_partial(
        slice1d<vec3>       global_bone_positions,
        slice1d<vec3>       global_bone_velocities,
        slice1d<quat>       global_bone_rotations,
        slice1d<vec3>       global_bone_angular_velocities,
        slice1d<bool>       global_bone_computed,
        const slice1d<vec3> local_bone_positions,
        const slice1d<vec3> local_bone_velocities,
        const slice1d<quat> local_bone_rotations,
        const slice1d<vec3> local_bone_angular_velocities,
        const slice1d<int>  bone_parents,
        int                 bone) {
        if (bone_parents(bone) == -1) {
            global_bone_positions(bone)          = local_bone_positions(bone);
            global_bone_velocities(bone)         = local_bone_velocities(bone);
            global_bone_rotations(bone)          = local_bone_rotations(bone);
            global_bone_angular_velocities(bone) = local_bone_angular_velocities(bone);
            global_bone_computed(bone)           = true;
            return;
        }

        if (! global_bone_computed(bone_parents(bone))) {
            forward_kinematics_velocity_partial(
                global_bone_positions,
                global_bone_velocities,
                global_bone_rotations,
                global_bone_angular_velocities,
                global_bone_computed,
                local_bone_positions,
                local_bone_velocities,
                local_bone_rotations,
                local_bone_angular_velocities,
                bone_parents,
                bone_parents(bone));
        }

        vec3 parent_position         = global_bone_positions(bone_parents(bone));
        vec3 parent_velocity         = global_bone_velocities(bone_parents(bone));
        quat parent_rotation         = global_bone_rotations(bone_parents(bone));
        vec3 parent_angular_velocity = global_bone_angular_velocities(bone_parents(bone));

        global_bone_positions(bone) = quat_mul_vec3(parent_rotation, local_bone_positions(bone)) + parent_position;
        global_bone_velocities(bone) =
            parent_velocity + quat_mul_vec3(parent_rotation, local_bone_velocities(bone)) + cross(parent_angular_velocity, quat_mul_vec3(parent_rotation, local_bone_positions(bone)));
        global_bone_rotations(bone)          = quat_mul(parent_rotation, local_bone_rotations(bone));
        global_bone_angular_velocities(bone) = quat_mul_vec3(parent_rotation, local_bone_angular_velocities(bone)) + parent_angular_velocity;
        global_bone_computed(bone)           = true;
    }

    // ============================================================================
    // Inverse Kinematics Functions
    // ============================================================================

    // ============================================================================
    // Contact and Foot Locking Functions
    // ============================================================================

    // ============================================================================
    // Inertialization Functions
    // ============================================================================

    // Moving the root is a bit difficult because of the way we set up the inertializer.
    // Essentially we also need to make sure to adjust all the positions we are transitioning
    // data from as well as the offsets we are blending
    void inertialize_root_adjust(
        vec3 &     offset_position,
        vec3 &     transition_src_position,
        quat &     transition_src_rotation,
        vec3 &     transition_dst_position,
        quat &     transition_dst_rotation,
        vec3 &     position,
        quat &     rotation,
        const vec3 input_position,
        const quat input_rotation) {
        // Find the position difference and add it to the state and transition location
        vec3 position_difference = input_position - position;
        position                 = position_difference + position;
        transition_dst_position  = position_difference + transition_dst_position;

        // Find the point at which we want to now transition from in the src data
        transition_src_position = transition_src_position + quat_mul_vec3(transition_src_rotation, quat_inv_mul_vec3(transition_dst_rotation, position - offset_position - transition_dst_position));
        transition_dst_position = position;
        offset_position         = vec3();

        // Find the rotation difference. We need to normalize here or some error can accumulate
        // over time during adjustment.
        quat rotation_difference = quat_normalize(quat_mul_inv(input_rotation, rotation));

        // Apply the rotation difference to the current rotation and transition location
        rotation                = quat_mul(rotation_difference, rotation);
        transition_dst_rotation = quat_mul(rotation_difference, transition_dst_rotation);
    }

    void inertialize_pose_reset(
        slice1d<vec3> bone_offset_positions,
        slice1d<vec3> bone_offset_velocities,
        slice1d<quat> bone_offset_rotations,
        slice1d<vec3> bone_offset_angular_velocities,
        vec3 &        transition_src_position,
        quat &        transition_src_rotation,
        vec3 &        transition_dst_position,
        quat &        transition_dst_rotation,
        const vec3    root_position,
        const quat    root_rotation) {
        bone_offset_positions.zero();
        bone_offset_velocities.zero();
        bone_offset_rotations.set(quat());
        bone_offset_angular_velocities.zero();

        transition_src_position = root_position;
        transition_src_rotation = root_rotation;
        transition_dst_position = vec3();
        transition_dst_rotation = quat();
    }

    void inertialize_pose_transition(
        slice1d<vec3>       bone_offset_positions,
        slice1d<vec3>       bone_offset_velocities,
        slice1d<quat>       bone_offset_rotations,
        slice1d<vec3>       bone_offset_angular_velocities,
        vec3 &              transition_src_position,
        quat &              transition_src_rotation,
        vec3 &              transition_dst_position,
        quat &              transition_dst_rotation,
        const vec3          root_position,
        const vec3          root_velocity,
        const quat          root_rotation,
        const vec3          root_angular_velocity,
        const slice1d<vec3> bone_src_positions,
        const slice1d<vec3> bone_src_velocities,
        const slice1d<quat> bone_src_rotations,
        const slice1d<vec3> bone_src_angular_velocities,
        const slice1d<vec3> bone_dst_positions,
        const slice1d<vec3> bone_dst_velocities,
        const slice1d<quat> bone_dst_rotations,
        const slice1d<vec3> bone_dst_angular_velocities) {
        // First we record the root position and rotation
        // in the animation data for the source and destination
        // animation
        transition_dst_position = root_position;
        transition_dst_rotation = root_rotation;
        transition_src_position = bone_dst_positions(0);
        transition_src_rotation = bone_dst_rotations(0);

        // We then find the velocities so we can transition the
        // root inertiaizers
        vec3 world_space_dst_velocity = quat_mul_vec3(transition_dst_rotation, quat_inv_mul_vec3(transition_src_rotation, bone_dst_velocities(0)));

        vec3 world_space_dst_angular_velocity = quat_mul_vec3(transition_dst_rotation, quat_inv_mul_vec3(transition_src_rotation, bone_dst_angular_velocities(0)));

        // Transition inertializers recording the offsets for
        // the root joint
        inertialize_transition(
            bone_offset_positions(0),
            bone_offset_velocities(0),
            root_position,
            root_velocity,
            root_position,
            world_space_dst_velocity);

        inertialize_transition(
            bone_offset_rotations(0),
            bone_offset_angular_velocities(0),
            root_rotation,
            root_angular_velocity,
            root_rotation,
            world_space_dst_angular_velocity);

        // Transition all the inertializers for each other bone
        for (int i = 1; i < bone_offset_positions.size; i++) {
            inertialize_transition(
                bone_offset_positions(i),
                bone_offset_velocities(i),
                bone_src_positions(i),
                bone_src_velocities(i),
                bone_dst_positions(i),
                bone_dst_velocities(i));

            inertialize_transition(
                bone_offset_rotations(i),
                bone_offset_angular_velocities(i),
                bone_src_rotations(i),
                bone_src_angular_velocities(i),
                bone_dst_rotations(i),
                bone_dst_angular_velocities(i));
        }
    }

    void inertialize_pose_update(
        slice1d<vec3>       bone_positions,
        slice1d<vec3>       bone_velocities,
        slice1d<quat>       bone_rotations,
        slice1d<vec3>       bone_angular_velocities,
        slice1d<vec3>       bone_offset_positions,
        slice1d<vec3>       bone_offset_velocities,
        slice1d<quat>       bone_offset_rotations,
        slice1d<vec3>       bone_offset_angular_velocities,
        const slice1d<vec3> bone_input_positions,
        const slice1d<vec3> bone_input_velocities,
        const slice1d<quat> bone_input_rotations,
        const slice1d<vec3> bone_input_angular_velocities,
        const vec3          transition_src_position,
        const quat          transition_src_rotation,
        const vec3          transition_dst_position,
        const quat          transition_dst_rotation,
        const float         halflife,
        const float         dt) {
        // First we find the next root position, velocity, rotation
        // and rotational velocity in the world space by transforming
        // the input animation from it's animation space into the
        // space of the currently playing animation.
        vec3 world_space_position = quat_mul_vec3(transition_dst_rotation, quat_inv_mul_vec3(transition_src_rotation, bone_input_positions(0) - transition_src_position)) + transition_dst_position;

        vec3 world_space_velocity = quat_mul_vec3(transition_dst_rotation, quat_inv_mul_vec3(transition_src_rotation, bone_input_velocities(0)));

        // Normalize here because quat inv mul can sometimes produce
        // unstable returns when the two rotations are very close.
        quat world_space_rotation = quat_normalize(quat_mul(transition_dst_rotation, quat_inv_mul(transition_src_rotation, bone_input_rotations(0))));

        vec3 world_space_angular_velocity = quat_mul_vec3(transition_dst_rotation, quat_inv_mul_vec3(transition_src_rotation, bone_input_angular_velocities(0)));

        // Then we update these two inertializers with these new world space inputs
        inertialize_update(
            bone_positions(0),
            bone_velocities(0),
            bone_offset_positions(0),
            bone_offset_velocities(0),
            world_space_position,
            world_space_velocity,
            halflife,
            dt);

        inertialize_update(
            bone_rotations(0),
            bone_angular_velocities(0),
            bone_offset_rotations(0),
            bone_offset_angular_velocities(0),
            world_space_rotation,
            world_space_angular_velocity,
            halflife,
            dt);

        // Then we update the inertializers for the rest of the bones
        for (int i = 1; i < bone_positions.size; i++) {
            inertialize_update(
                bone_positions(i),
                bone_velocities(i),
                bone_offset_positions(i),
                bone_offset_velocities(i),
                bone_input_positions(i),
                bone_input_velocities(i),
                halflife,
                dt);

            inertialize_update(
                bone_rotations(i),
                bone_angular_velocities(i),
                bone_offset_rotations(i),
                bone_offset_angular_velocities(i),
                bone_input_rotations(i),
                bone_input_angular_velocities(i),
                halflife,
                dt);
        }
    }
} // namespace VCX::Labs::MotionMatching::Core::Animation
