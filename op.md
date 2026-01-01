# 蒙皮渲染策略分析与实施方案

基于 `render.md`, `record.md`, 和 `bone.md` 的分析，针对当前 `CaseBVHSkinned` 的实现目标，制定以下策略。

## 1. 渲染管线选择：CPU Skinning

鉴于 `render.md` 指出当前 Shader (`character.vs`) 仅接受静态顶点数据，且 `record.md` 确认了项目中已存在成熟的 CPU 蒙皮算法 (`linear_blend_skinning_*`)，我们继续沿用 **CPU 蒙皮** 方案。

*   **优势**: 兼容现有 Shader，无需修改 GLSL；可以直接利用 `character.cpp` 中的算法；方便调试骨骼与顶点的对应关系。
*   **流程**: `BVH -> FK -> Global Transforms -> CPU Skinning -> Update VBO -> Draw`。

## 2. 关键技术点：虚拟骨骼 (Simulation Bone) 的实时注入

根据 `bone.md`，BVH 数据通常以 Hips 为根，而我们的蒙皮网格 (Mesh) 包含一个额外的 `Bone_Entity` (Index 0) 作为逻辑根。

**策略**:
在每一帧的 FK 计算之后，蒙皮计算之前，**手动计算并注入 Index 0 的数据**。

1.  **计算源**: 使用 FK 计算出的 `Spine2` (位置参考) 和 `Hips` (方向参考) 的 Global Transform。
2.  **计算逻辑**:
    *   `SimPos`: 投影 `Spine2` 到地面 (y=0)。
    *   `SimRot`: 提取 `Hips` 的前向向量，投影到地面，计算与 Z 轴的旋转偏差。
3.  **数据注入**:
    *   将计算出的 `SimPos` 和 `SimRot` 写入 `_globalPositions[0]` 和 `_globalRotations[0]`。
    *   *注意*: 原始 BVH 的 Hips 数据通常对应 Index 1，需要确保索引映射正确。

## 3. 实现架构 (`CaseBVHSkinned`)

需要在 `CaseBVHSkinned` 类中添加以下组件：

### A. 数据成员
*   `Core::Animation::character _character`: 存储网格数据（顶点、法线、权重、Rest Pose 等）。
*   `Engine::GL::UniqueVertexArray _vaoMesh`: 用于渲染角色的 VAO。
*   `Engine::GL::UniqueArrayBuffer _vboPos`, `_vboNorm`, `_vboTex`: 动态更新的 VBO。

### B. 初始化 (`OnBVHLoaded` / `Setup`)
1.  加载 `.bvh` 文件。
2.  加载对应的 `.character` 文件 (包含蒙皮权重)。
3.  初始化 VBOs，分配显存 (大小为 `_character.positions.size()`)。

### C. 帧更新 (`UpdateFrame`)
1.  **采样**: 根据时间 `t` 从 BVH 采样 Local Transforms。
2.  **FK**: 计算所有骨骼的 Global Transforms。
3.  **重定向 (Retargeting/Injection)**:
    *   计算 Simulation Bone 数据。
    *   修正 `_globalPositions` 和 `_globalRotations` 数组，确保 Index 0 是 Sim Bone，Index 1 是 Hips，以此类推。
4.  **蒙皮 (Skinning)**:
    *   调用 `linear_blend_skinning_positions` 计算 `_character.positions` (变形后)。
    *   调用 `linear_blend_skinning_normals` 计算 `_character.normals` (变形后)。
5.  **上传 (Upload)**:
    *   `glBufferSubData` 将新的 positions 和 normals 上传到 GPU。

### D. 渲染 (`OnRender`)
1.  使用 `character.vert` / `character.frag` Shader。
2.  绑定 `_vaoMesh`。
3.  `glDrawArrays` 或 `glDrawElements` 绘制角色。

## 4. 总结
该策略利用了 CPU 的灵活性来解决骨骼层级不匹配的问题 (`bone.md`)，同时复用了现有的渲染管线 (`render.md`) 和算法库 (`record.md`)，是当前工程环境下最稳健的实现路径。
