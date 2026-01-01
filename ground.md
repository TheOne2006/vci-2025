# Ground Rendering Pipeline Analysis

## 渲染管线概述 (Rendering Pipeline Overview)

地面（Ground）的渲染过程遵循标准的 OpenGL 渲染管线：

1.  **数据准备 (Data Preparation)**:
    *   在 C++ (`CaseBVH.cpp`) 中定义了地面的顶点数据 (`VertexCheckerboard`)，包含位置、纹理坐标、法线和颜色。
    *   创建了 VAO (Vertex Array Object) 和 VBO (Vertex Buffer Object) 以及 EBO (Element Buffer Object) 来存储这些数据。
    *   地面由两个三角形组成一个大的四边形平面。

2.  **顶点着色器 (Vertex Shader - `checkerboard.vert`)**:
    *   接收顶点数据。
    *   使用 `mvp` (Model-View-Projection) 矩阵将顶点从模型空间转换到裁剪空间 (`gl_Position`)。
    *   计算世界空间坐标 (`fragPosition`) 和法线 (`fragNormal`) 传递给片段着色器。

3.  **光栅化 (Rasterization)**:
    *   GPU 将裁剪空间中的三角形转换为屏幕上的像素（片段）。
    *   插值计算每个片段的 `fragPosition` 等属性。

4.  **片段着色器 (Fragment Shader - `checkerboard.frag`)**:
    *   接收插值后的世界空间坐标。
    *   根据 X 和 Z 坐标计算棋盘格图案。
    *   输出最终颜色。

---

## Shader 代码检查

### 1. Vertex Shader (`assets/shaders/checkerboard.vert`)

```glsl
#version 410 core

layout(location = 0) in vec3 vertexPosition;
layout(location = 1) in vec2 vertexTexCoord;
layout(location = 2) in vec3 vertexNormal;
layout(location = 3) in vec4 vertexColor;

uniform mat4 mvp;
uniform mat4 matModel;
uniform mat4 matNormal;

out vec3 fragPosition;
out vec2 fragTexCoord;
out vec4 fragColor;
out vec3 fragNormal;

void main()
{
    // 计算世界空间坐标，用于在 Fragment Shader 中生成棋盘格
    fragPosition = vec3(matModel * vec4(vertexPosition, 1.0f));
    fragTexCoord = vertexTexCoord;
    fragColor = vertexColor;
    // 计算变换后的法线
    fragNormal = normalize(vec3(matNormal * vec4(vertexNormal, 1.0f)));

    // 标准 MVP 变换
    gl_Position = mvp * vec4(vertexPosition, 1.0f);
}
```

**分析**:
*   输入属性 (`layout location`) 与 C++ 代码中的 `glVertexAttribPointer` 设置一致。
*   `mvp` 用于最终位置计算，`matModel` 用于世界坐标计算（用于纹理生成）。逻辑正确。

### 2. Fragment Shader (`assets/shaders/checkerboard.frag`)

```glsl
#version 410 core
precision mediump float;

in vec3 fragPosition;
in vec2 fragTexCoord;
in vec4 fragColor;
in vec3 fragNormal;

uniform vec4 colDiffuse;

out vec4 finalColor;

void main()
{
    // 程序化生成棋盘格纹理
    // 基于世界坐标的 X 和 Z 分量
    float total = floor(fragPosition.x * 2.0f) +
            floor(fragPosition.z * 2.0f);

    // 使用 mod 判断奇偶，选择两种灰色
    finalColor = mod(total, 2.0f) == 0.0f ?
        vec4(0.8f, 0.8f, 0.8f, 1.0f) :
        vec4(0.85f, 0.85f, 0.85f, 1.0f);
}
```

**分析**:
*   逻辑是基于世界坐标 `fragPosition` 生成棋盘格。
*   只要 `fragPosition` 变化（即地面足够大且相机在移动），就能看到格子。
*   颜色是两种非常接近的灰色 (`0.8` vs `0.85`)，对比度较低，可能在某些光照或显示器下看起来像纯色，但应该能看见。

---

## 潜在问题排查 (Troubleshooting)

如果地面依然无法渲染，可能的原因及排查点：

1.  **面剔除 (Face Culling)**:
    *   **已修复**: 之前代码中 `glEnable(GL_CULL_FACE)` 导致地面可能被剔除。已在 `OnRender` 开头添加 `glDisable(GL_CULL_FACE)`。

2.  **深度测试 (Depth Test)**:
    *   如果深度测试未开启或深度函数设置错误，地面可能被背景覆盖。
    *   **检查**: 代码中已调用 `glEnable(GL_DEPTH_TEST)` 和 `glDepthMask(GL_TRUE)`。

3.  **相机矩阵 (Camera Matrices)**:
    *   如果 `mvp` 矩阵全为 0 或错误，几何体将不可见。
    *   **检查**: `_camera.GetProjectionMatrix` 和 `_camera.GetViewMatrix` 被正确传入。

4.  **Shader 编译**:
    *   如果 Shader 编译失败，程序可能静默失败（取决于引擎的错误处理）。
    *   **建议**: 检查控制台输出是否有 Shader 编译错误。

5.  **VAO/VBO 绑定**:
    *   如果在绘制时 VAO 没有正确绑定，或者 VBO 数据为空，则画不出东西。
    *   **检查**: C++ 代码中 `glBindVertexArray(_vaoGround.Get())` 和 `glDrawElements` 看起来是正确的。

6.  **颜色对比度**:
    *   `0.8` 和 `0.85` 的灰色非常接近。如果光照（虽然这个 Shader 没用光照计算）或显示器原因，可能看不清格子。
    *   **建议**: 可以尝试将颜色改为红绿对比 (`vec4(1,0,0,1)` vs `vec4(0,1,0,1)`) 来测试是否是颜色问题。
