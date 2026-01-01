# BVH Render 分析与骨骼渲染扩展

## 1. 现有 BVH Render 结构分析

目前的渲染逻辑主要集中在 `CaseBVH.cpp` 和 `CaseBVH.h` 中，用于演示 IK (Inverse Kinematics) 系统。

### 1.1 核心类
*   **`CaseBVH`**: 场景的主控类。
    *   管理相机 (`OrbitCameraManager`)。
    *   管理渲染对象 (`BackGroundRender`, `BoxRenderer` 数组)。
    *   `OnRender` 函数负责每帧的绘制调用。
*   **`BoxRenderer`**: 用于渲染长方体（目前用于表示 IK 系统中的“手臂”段）。
    *   使用 `flat` shader。
    *   包含 `BoxItem` (三角形) 和 `LineItem` (线框)。
    *   `calc_vert_position()` 根据中心点和主轴方向计算 8 个顶点的坐标。
*   **`BackGroundRender`**: 用于渲染背景元素。
    *   绘制坐标轴 (LineItem)。
    *   绘制目标点和历史轨迹点 (PointItem, GTPointItem)。

### 1.2 Shader 分析
项目包含多组 Shader，用途如下：

*   **`flat.vert` / `flat.frag`**:
    *   **用途**: 基础的纯色渲染。
    *   **Vert**: 接收位置 `a_Position`，应用 MVP 变换。
    *   **Frag**: 输出 Uniform 颜色 `u_Color`。
    *   **场景**: 用于绘制线条、点、简单的几何体（如目前的 IK 手臂）。

*   **`character.vert` / `character.frag`**:
    *   **用途**: 蒙皮角色渲染。
    *   **Vert**: 接收位置、法线、纹理坐标。虽然名字叫 `character`，但目前的 shader 代码看起来是标准的静态网格变换（MVP, Model, Normal Matrix），**没有包含骨骼蒙皮计算**（蒙皮计算通常在 CPU 端完成，如 `character.cpp` 中的 `linear_blend_skinning_positions`，或者在 Vertex Shader 中通过骨骼权重和索引进行）。
    *   **Frag**: 半兰伯特 (Half-Lambert) 光照模型，使阴影部分不会太黑。

*   **`three.vert` / `three.geom` / `three.frag`**:
    *   **用途**: 带有几何着色器的渲染管线。
    *   **Vert**: 传递顶点数据。
    *   **Geom**: 几何着色器。如果 `u_Flat` 为 true，它会根据三角形的三个顶点计算面法线（Face Normal），实现 Flat Shading 效果；否则使用顶点法线。
    *   **Frag**: 包含棋盘格纹理生成逻辑（基于 UV 坐标），以及 Gamma 校正。

## 2. 扩展为骨骼渲染 (Skeleton Rendering)

要实现完整的骨骼渲染，我们需要利用骨骼的层级关系 (`bone_parents`) 和每一帧计算出的全局骨骼位置 (`global_bone_positions`)。

### 2.1 数据准备
根据 `Core/Animation/database.hpp` 和 `bone_operations.hpp`，我们有以下关键数据：
*   **`bone_parents`**: 一个整数数组，存储每个骨骼的父骨骼索引。
*   **`global_bone_positions`**: 每一帧计算出的所有骨骼的世界坐标。

### 2.2 渲染方案
我们可以创建一个新的渲染类 `SkeletonRenderer`，或者扩展现有的 `CaseBVH`。

#### 方案 A: 线框渲染 (简单高效)
使用 `GL_LINES` 绘制连接父子骨骼的线段。

1.  **初始化**:
    *   创建一个 `LineItem` (使用 `flat` shader)。
2.  **每帧更新**:
    *   遍历所有骨骼 `i` (从 1 开始，跳过根节点 0，或者根据具体逻辑)。
    *   获取当前骨骼位置 `p_curr = global_bone_positions[i]`。
    *   获取父骨骼索引 `parent_idx = bone_parents[i]`。
    *   如果 `parent_idx != -1`:
        *   获取父骨骼位置 `p_parent = global_bone_positions[parent_idx]`。
        *   将 `p_curr` 和 `p_parent` 添加到顶点缓冲区。
3.  **绘制**:
    *   调用 `LineItem.Draw()`。

#### 方案 B: 实体渲染 (更美观)
使用类似 `BoxRenderer` 的方式，在父子骨骼之间绘制长方体或圆柱体。

1.  **复用 `BoxRenderer`**:
    *   为每一根骨骼（除了根节点）实例化一个 `BoxRenderer`，或者使用 Instanced Rendering（如果骨骼数量很多）。
2.  **每帧更新**:
    *   对于骨骼 `i` 和父骨骼 `p`:
        *   计算向量 `v = pos[i] - pos[p]`。
        *   **长度**: `length = length(v)`。
        *   **中心点**: `center = (pos[i] + pos[p]) / 2`。
        *   **方向**: `axis = normalize(v)`。
    *   将这些参数传递给 `BoxRenderer` 的 `calc_vert_position` 逻辑，更新顶点数据。

### 2.3 具体实现步骤建议

1.  **定义 `SkeletonRenderer` 类**:
    ```cpp
    class SkeletonRenderer {
    public:
        SkeletonRenderer(const std::vector<int>& parents);
        void Update(const std::vector<glm::vec3>& global_positions);
        void Render(Engine::GL::UniqueProgram& program);
    private:
        std::vector<int> _parents;
        Engine::GL::UniqueIndexedRenderItem _lineItem;
    };
    ```

2.  **在 `CaseBVH` 中集成**:
    *   在 `CaseBVH` 中添加 `SkeletonRenderer` 成员。
    *   在加载 BVH 数据后，初始化 `SkeletonRenderer` 的父子关系。
    *   在 `OnRender` 中，获取当前的骨骼姿态，调用 `Update`，然后调用 `Render`。

3.  **Shader 选择**:
    *   骨骼连线通常使用 `flat` shader 即可，颜色可以设置为显眼的颜色（如黄色或白色）。
