# VCL Motion Matching Project

**Author**: 王唐欣宇 (Wang Tangxinyu)  
**Course**: Visual Computing and Learning (VCL)

## Introduction

This project implements advanced character animation techniques including a BVH Motion Player, Linear Blend Skinning (LBS), and a complete Motion Matching system. These techniques go beyond standard forward/inverse kinematics to enable realistic, data-driven character animation.

## Features

The project consists of three main tasks:

1.  **BVH Motion Player**: 
    -   Loads and plays motion capture data from `.bvh` files.
    -   Implements Forward Kinematics (FK) to compute global bone transformations.
    -   Visualizes the skeleton using 3D primitives.

2.  **Skinned BVH Motion Player**:
    -   Implements Linear Blend Skinning (LBS) to deform a character mesh based on the skeletal pose.
    -   Includes retargeting logic to map BVH motion data (starting at Hips) to the character skeleton (Simulation Bone).
    -   Rendered with Half-Lambert shading.

3.  **Motion Matching System**:
    -   A real-time animation system that selects and blends motion data based on user input and character trajectory.
    -   **Locomotion Modes**: Walking, Running (Shift key), and Strafing.
    -   **Pipeline**:
        1.  **Input Processing**: Maps WASD to desired velocity relative to the camera.
        2.  **Trajectory Prediction**: Uses spring-damper systems to predict future path.
        3.  **Database Search**: Finds the best matching animation frame using a high-dimensional feature vector (Trajectory + Pose features).
        4.  **Inertialization**: Smooths transitions between animation clips without ghosting artifacts.
        5.  **IK Adjustment**: Two-bone IK and foot locking to prevent foot sliding.

## Project Structure

The codebase is organized into modular components:

-   `assets/`: Resources (BVH files, 3D models, shaders).
-   `src/VCX/Labs/MotionMatching/`: Main application logic.
    -   `Core/Animation/`: Animation logic (Bone ops, Kinematics, Database search).
    -   `Core/Math/`: Math utilities (Vectors, Quaternions).
    -   `SceneEnvironment.cpp`: Rendering logic.
    -   `CaseBVH.cpp`, `CaseBVHSkinned.cpp`, `CaseMotionMatching.cpp`: Entry points for the three tasks.

## Setup and Usage

### Prerequisites
-   **xmake**: The project uses xmake as the build system.

### Build
To compile the project:
```bash
xmake build -v
```

### Run
To run the motion matching demo:
```bash
xmake run motion-matching
```
You can also find the executable in the `build/` directory corresponding to your platform.

## Controls

For the **Motion Matching** case:
-   **W, A, S, D**: Move character.
-   **Shift**: Hold to Run.
-   **Mouse**: Control Camera.

## Implementation Details

For detailed technical implementation, including the search algorithm, inertialization blending mathematics, and architecture diagrams, please refer to `/report/report.pdf` (generated from `report.tex`).
