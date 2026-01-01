# Shader 层面实现分析

在本项目中，Shader (`character.vs` 和 `character.fs`) 的逻辑非常简单。这是因为**蒙皮计算（Skinning）完全是在 CPU 端完成的**。

对于 GPU 来说，它接收到的已经是一个**每一帧都在变形状的“静态”网格**。Shader 不需要知道骨骼、权重或蒙皮矩阵的存在。

## 1. Vertex Shader (`character.vs`) 分析

这是一个标准的、用于静态物体的顶点着色器。

```glsl
#version 410 core

// 输入属性 (Attributes)
// 这些数据是 CPU 每一帧计算好并上传更新后的数据
in vec3 vertexPosition; // 已经是变形后的位置 (Deformed Position)
in vec2 vertexTexCoord;
in vec3 vertexNormal;   // 已经是变形后的法线 (Deformed Normal)
in vec4 vertexColor;

// Uniforms (全局变量)
uniform mat4 mvp;       // Model-View-Projection 矩阵
uniform mat4 matModel;  // Model 矩阵 (用于世界空间变换)
uniform mat4 matNormal; // Normal 矩阵 (用于法线变换)

// 输出到 Fragment Shader
out vec3 fragPosition;
out vec2 fragTexCoord;
out vec4 fragColor;
out vec3 fragNormal;

void main()
{
    // 1. 计算世界空间坐标 (用于光照计算)
    fragPosition = vec3(matModel * vec4(vertexPosition, 1.0));
    
    // 2. 传递纹理坐标和颜色
    fragTexCoord = vertexTexCoord;
    fragColor = vertexColor;
    
    // 3. 计算世界空间法线
    // 注意：这里的 vertexNormal 已经是 CPU 蒙皮旋转过的法线了
    // matNormal 通常只是 matModel 的逆转置，用于处理非均匀缩放
    fragNormal = normalize(vec3(matNormal * vec4(vertexNormal, 1.0)));

    // 4. 计算裁剪空间坐标 (最终屏幕位置)
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
```

**关键点**：
*   **没有骨骼数据**：你可以看到输入中没有 `boneIndices` 或 `boneWeights`。
*   **没有骨骼矩阵**：Uniform 中没有 `boneTransforms[]` 数组。
*   **纯透视投影**：它所做的仅仅是将输入的顶点位置乘以 MVP 矩阵。

## 2. Fragment Shader (`character.fs`) 分析

这是一个简单的光照着色器，使用了 **Half-Lambert** 光照模型。

```glsl
#version 410 core
precision mediump float;

// 从 Vertex Shader 接收的数据
in vec3 fragPosition;
in vec2 fragTexCoord;
in vec4 fragColor;
in vec3 fragNormal;

uniform vec4 colDiffuse; // 材质漫反射颜色

out vec4 finalColor;     // 最终输出像素颜色

void main()
{
    // 硬编码的光照方向 (从右上方打下来的光)
    vec3 light_dir = normalize(vec3(0.25, -0.8, 0.1));

    // Half-Lambert 光照计算
    // dot(-light_dir, fragNormal) 计算光线入射角余弦值 (-1 到 1)
    // + 1.0 然后 / 2.0 将范围映射到 (0 到 1)
    // 这种光照模型比标准的 Lambert 更柔和，阴影不会死黑，适合卡通或非写实渲染
    float half_lambert = (dot(-light_dir, fragNormal) + 1.0) / 2.0;

    // 应用光照强度到材质颜色，并加上一点环境光 (0.1)
    finalColor = vec4(half_lambert * colDiffuse.xyz + 0.1, 1.0);
}
```

## 3. 调用流程 (Pipeline)

整个渲染流程是这样的：

1.  **CPU 计算 (C++)**:
    *   在 `controller.cpp` 中，`deform_character_mesh` 函数根据骨骼动画计算出新的顶点位置 (`v'`) 和法线 (`n'`)。
    *   这些计算结果被写入内存中的 `Mesh` 结构体。

2.  **数据上传 (CPU -> GPU)**:
    *   调用 `UpdateMeshBuffer`。
    *   这会将 `v'` 和 `n'` 覆盖到 GPU 显存中的 Vertex Buffer Object (VBO)。
    *   对于 Shader 来说，`vertexPosition` 属性的数据源变了。

3.  **Shader 执行 (GPU)**:
    *   Raylib 内部调用 `glDrawElements`。
    *   **Vertex Shader** 读取新的 `vertexPosition`，执行 `gl_Position = mvp * vec4(vertexPosition, 1.0)`。
    *   **Fragment Shader** 上色。

## 4. 总结

在这个项目中，Shader 扮演的是一个**被动**的角色。它不负责复杂的变形逻辑，只负责将 CPU 喂给它的“已经摆好姿势”的网格画在屏幕上。

*   **优点**：Shader 极其简单，兼容性好（甚至可以在不支持 Uniform 数组的老旧硬件上跑）。
*   **缺点**：每一帧都要上传整个网格的顶点数据到 GPU，带宽消耗大，且 CPU 负担重。如果是现代 3A 游戏，通常会将蒙皮逻辑移入 Vertex Shader (GPU Skinning)。
