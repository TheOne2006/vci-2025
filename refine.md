# 场景渲染 Pipeline 解析

本项目使用 **Raylib** 进行渲染，场景非常简洁。渲染管线主要由“天空”（背景清除）、“地面”（棋盘格 Shader）和“角色”（蒙皮网格）组成。

## 1. 渲染顺序 Pipeline

每一帧的渲染流程在 `controller.cpp` 的 `update_func` (或 `main` 循环) 中执行，顺序如下：

1.  **准备阶段**:
    *   `BeginDrawing()`: 开始绘制新的一帧。
    *   `ClearBackground(RAYWHITE)`: **绘制天空**。实际上并没有真正的天空盒或天空 Shader，而是直接用纯色 (RayWhite) 清除屏幕缓冲区。

2.  **3D 场景绘制 (`BeginMode3D(camera)`)**:
    *   **绘制地面 (Ground Plane)**:
        *   调用 `DrawModel(ground_plane_model, ...)`。
        *   使用 `checkerboard.vert` 和 `checkerboard.frag` 进行程序化纹理生成。
        希望绘制的无穷大的棋盘
    *   **绘制网格辅助线**:
        *   调用 `DrawGrid(20, 1.0f)` 绘制覆盖在地面上的黑色线框网格。
    *   **绘制坐标轴**(需要绘制一个坐标轴, 下面有一个参考，不过你可以自己实现):
        *   调用 `draw_axis(...)` 绘制原点坐标轴。
        void draw_axis(const vec3 pos, const quat rot, const float scale = 1.0f)
{
    vec3 axis0 = pos + quat_mul_vec3(rot, scale * vec3(1.0f, 0.0f, 0.0f));
    vec3 axis1 = pos + quat_mul_vec3(rot, scale * vec3(0.0f, 1.0f, 0.0f));
    vec3 axis2 = pos + quat_mul_vec3(rot, scale * vec3(0.0f, 0.0f, 1.0f));
    
    DrawLine3D(to_Vector3(pos), to_Vector3(axis0), RED);
    DrawLine3D(to_Vector3(pos), to_Vector3(axis1), GREEN);
    DrawLine3D(to_Vector3(pos), to_Vector3(axis2), BLUE);
}
    *   **绘制角色 (Character)**(这一步你已经完成了):
        *   先调用 `deform_character_mesh` 更新 CPU 蒙皮数据。
        *   调用 `DrawModel(character_model, ...)` 绘制角色。
    *   `EndMode3D()`: 结束 3D 投影，切换回 2D 屏幕空间。

3.  **UI 绘制**:
    *   使用 `raygui` 绘制各种滑块、按钮和文本 (`GuiSliderBar`, `GuiLabel` 等)。

4.  **结束**:
    *   `EndDrawing()`: 交换缓冲区，显示画面。

---

## 2. 详细实现解析

### A. 天空 (Sky)
*   **实现方式**: `ClearBackground(RAYWHITE)`
*   **原理**: 每一帧开始时，将颜色缓冲区的所有像素重置为 `RAYWHITE` (一种浅灰色/白色)。
*   **改进建议**: 如果想要更真实的天空，可以加载一个 Skybox (立方体贴图) 或者编写一个基于梯度的 Sky Shader，在 `BeginMode3D` 之后首先绘制一个巨大的反向球体或立方体。

### B. 地面 (Checkerboard Floor)
地面不是一张贴图，而是通过 **Fragment Shader 程序化生成** 的棋盘格图案。

*   **Mesh**: 一个简单的平面 (`GenMeshPlane(20.0f, 20.0f, ...)`).
*   **Vertex Shader (`checkerboard.vs`)**:
    *   标准的 MVP 变换。
    *   关键是将 **世界坐标** (`fragPosition`) 传递给 Fragment Shader。
*   **Fragment Shader (`checkerboard.fs`)**:
    ```glsl
    void main()
    {
        // 根据世界坐标的 X 和 Z 计算棋盘格索引
        // floor(x * 2.0) 意味着每 0.5 个单位变换一次颜色
        float total = floor(fragPosition.x * 2.0f) +
                      floor(fragPosition.z * 2.0f);
        
        // 使用模运算判断奇偶，选择两种不同的灰色
        finalColor = mod(total, 2.0f) == 0.0f ? 
            vec4(0.8f, 0.8f, 0.8f, 1.0f) : 
            vec4(0.85f, 0.85f, 0.85f, 1.0f);
    }
    ```
*   **优点**: 无限分辨率，不会模糊，不需要 UV 映射，不需要加载纹理图片。

### C. 阴影 (Shadows)
*   **当前状态**: **没有阴影**。
*   **原因**: Raylib 的默认材质系统不包含阴影贴图 (Shadow Mapping)。
*   **如何添加**: 需要实现 Shadow Mapping 技术：
    1.  从光源视角渲染场景深度到 Framebuffer (Shadow Map)。
    2.  在渲染地面和角色时，采样 Shadow Map 判断像素是否被遮挡。
    3.  这需要编写自定义的 Shader 并管理额外的渲染通道 (Render Pass)。

## 3. 总结

这个项目的渲染管线非常基础，主要服务于**算法演示**而非图形效果。
*   **天空** = `ClearBackground`
*   **地面** = 程序化 Shader
*   **光照** = 简单的 Half-Lambert (无阴影)

这种极简设计使得代码易于理解，且能让观察者专注于角色的动作质量，而不是被花哨的图形效果分散注意力。
