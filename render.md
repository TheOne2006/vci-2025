# 渲染逻辑分析与问题排查

## 1. 渲染逻辑概述

目前的渲染流程主要由 `CaseBVH` 类和 `BoxRenderer` 类协同完成，具体步骤如下：

### 1.1 初始化 (`CaseBVH::CaseBVH`)
- **加载 Shader**: 使用 `assets/shaders/flat.vert` 和 `assets/shaders/flat.frag` 创建 Shader 程序。这是一个简单的纯色 Shader。
- **加载 BVH**: 调用 `LoadBVH` 解析 BVH 文件，构建骨骼层级结构 (`_boneParents`) 和初始数据。
- **创建渲染器**: 为每一个有父节点的骨骼创建一个 `BoxRenderer` 实例。

### 1.2 数据更新 (`CaseBVH::UpdateFrame`)
- **时间步进**: 根据 `dt` 更新当前动画时间 `_currentTime`。
- **获取局部变换**: 从 BVH 数据中获取当前帧每个关节的局部位移和旋转。
- **正向运动学 (FK)**: 调用 `Core::Animation::forward_kinematics_full` 计算所有关节的**全局位置** (`_globalPositions`) 和旋转。
- **更新骨骼几何**: 遍历所有 `BoxRenderer`：
    - 计算父关节到子关节的向量作为骨骼的主轴 (`MainAxis`)。
    - 计算骨骼长度 (`length`)。
    - 调用 `BoxRenderer::calc_vert_position`，根据中心点、主轴方向、长度和宽度 (`width`) 计算长方体（骨骼）的 8 个顶点坐标。

### 1.3 渲染循环 (`CaseBVH::OnRender`)
- **设置相机**: 更新相机矩阵 (`u_Projection`, `u_View`) 并传递给 Shader。
- **绘制**: 遍历所有 `BoxRenderer`，调用其 `render` 函数。
    - `render` 函数会将计算好的顶点数据 (`VertsPosition`) 上传到 GPU。
    - 使用 `BoxItem` 绘制实心长方体（绿色）。
    - 使用 `LineItem` 绘制线框（白色）。

## 2. 为什么什么都渲染不出来？

经过分析，代码逻辑本身没有明显的语法或流程错误，导致“什么都看不见”的主要原因是**数据单位与相机视角的尺度不匹配**。

### 2.1 尺度单位差异 (Scale Mismatch)
- **BVH 数据单位**: 查看 `assets/bvh/walk.bvh` 文件，可以看到 `OFFSET` 的数值在 10 到 100 之间（例如 `OFFSET -9.91 91.97 100.90`）。这说明 BVH 数据的单位是 **厘米 (cm)**。角色的高度大约在 170-180 单位左右。
- **骨骼宽度设置**: 在 `CaseBVH.h` 中，`BoxRenderer` 的默认宽度设置为：
  ```cpp
  float width = 0.05f;
  ```
  在厘米单位下，这意味着骨骼的宽度只有 **0.05 厘米 (0.5 毫米)**。这对于一个 1.8 米高的角色来说，细得像一根头发，在屏幕上几乎不可见。

### 2.2 相机位置不当
- **初始位置**: 在 `CaseBVH.h` 中，相机初始化为：
  ```cpp
  Engine::Camera _camera { .Eye = glm::vec3(-3, 3, 3) };
  ```
  这意味着相机位于世界坐标系原点附近 **3 厘米** 处。
- **后果**: 
    - 角色通常位于原点附近或上方（Y轴 90-100cm 处）。
    - 相机距离角色非常近，甚至可能位于角色的脚底模型内部。
    - 结合极细的骨骼宽度，相机视野内可能什么都捕捉不到，或者因为 Near Plane (近裁剪面) 的原因被裁剪掉了。

## 3. 解决方案

为了修复渲染问题，需要调整相机位置和骨骼宽度以适应 BVH 的厘米单位。

### 建议修改

1.  **调整相机位置**: 将相机移远，以便能看到整个角色（例如距离 300 厘米）。
2.  **增加骨骼宽度**: 将骨骼宽度增加到可见的程度（例如 5 厘米）。

**修改 `src/VCX/Labs/MotionMatching/CaseBVH.h`:**

```cpp
// 修改前
Engine::Camera _camera { .Eye = glm::vec3(-3, 3, 3) };
// ...
float width = 0.05f;

// 建议修改后
Engine::Camera _camera { .Eye = glm::vec3(0, 100, 300) }; // 抬高并拉远相机
// ...
float width = 5.0f; // 增加宽度到 5cm
```
