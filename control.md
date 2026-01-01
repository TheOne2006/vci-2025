# 控制策略设计 (Control Strategy)

本文档详细描述了 Motion Matching 系统中的角色控制策略，包括输入处理、相机融合、平滑算法（Spring/Damper）以及轨迹预测。

## 1. 控制模式架构 (Control Architecture)

为了满足“相机控制”与“角色控制”的切换需求，我们将引入一个控制状态标志。

### 1.1 模式切换 (Mode Switching)
在 UI 面板中添加一个 Checkbox（复选框）：
- **`[ ] Control Character` (控制角色)**
    - **选中 (True)**: 键盘 WASD 和 Shift 控制角色移动/冲刺。鼠标控制相机旋转（Orbit）。相机跟随角色。
    - **未选中 (False)**: 键盘 WASD 控制相机平移（Pan）。鼠标控制相机旋转/缩放。相机与角色解耦（用于自由观察）。

### 1.2 数据流 (Data Flow)
```mermaid
graph TD
    Input[Input (WASD + Shift)] --> Switch{Control Character?}
    Switch -- Yes --> CharCtrl[Character Controller]
    Switch -- No --> CamCtrl[Camera Controller]
    
    CamInfo[Camera Forward/Right] --> CharCtrl
    
    CharCtrl --> Smooth[Spring/Damper Smoothing]
    Smooth --> Traj[Trajectory Prediction]
    Traj --> MM[Motion Matching Search]
```

## 2. 输入映射与处理 (Input Mapping)

### 2.1 基础输入
- **W / S**: 前后移动 (Z轴)
- **A / D**: 左右移动 (X轴)
- **Shift**: 冲刺 (Sprint) / 切换步态 (Gait)
- **无输入**: 待机 (Idle)

### 2.2 相机相对控制 (Camera-Relative Movement)
角色的移动方向必须相对于当前的相机视角。
1. 获取相机的前方向量 (`CamFwd`) 和右方向量 (`CamRight`)。
2. 将向量投影到水平地面（去除 Y 分量）并归一化。
3. 合成目标方向：
   $$ \vec{Dir}_{target} = \text{Normalize}(Input.y \times \vec{CamFwd} + Input.x \times \vec{CamRight}) $$

### 2.3 目标速度计算 (Target Velocity)
根据输入状态设定目标速度大小 (`TargetSpeed`)：
- **Idle**: $0.0 m/s$
- **Walk**: $\approx 1.75 m/s$ (无 Shift)
- **Run**: $\approx 4.0 m/s$ (按住 Shift)

$$ \vec{Vel}_{target} = \vec{Dir}_{target} \times TargetSpeed $$

## 3. 平滑与弹簧阻尼 (Spring/Damper Smoothing)

为了避免角色动作生硬（如瞬间转身、瞬间加速），我们不直接使用 `TargetVelocity`，而是通过弹簧阻尼系统计算 `CurrentVelocity`。

### 3.1 核心公式 (Halflife Spring)
使用基于半衰期 (Halflife) 的弹簧插值公式。这种方法比简单的 `Lerp` 更符合物理直觉，且与帧率无关。

```cpp
// 简化的阻尼函数伪代码
// y: 当前值, g: 目标值, halflife: 半衰期, dt: 时间步长
void simple_spring_damper_exact(
    Vec3& y, Vec3& dy, const Vec3& g, 
    float halflife, float dt) 
{
    // 计算阻尼系数和频率
    float y_freq = 0.6931472f / (halflife + 1e-5f); // ln(2) / halflife
    // ... 执行临界阻尼弹簧更新 ...
    // 更新 y (位置/速度) 和 dy (加速度/变化率)
}
```

### 3.2 应用场景
1.  **速度平滑 (Velocity Smoothing)**:
    -   输入: 用户输入的突变目标速度。
    -   参数: `simulation_velocity_halflife` (约 0.27s)。
    -   效果: 模拟角色的惯性，起步和急停有“滑步”感。

2.  **朝向平滑 (Rotation Smoothing)**:
    -   输入: 目标朝向。
    -   参数: `simulation_rotation_halflife` (约 0.27s)。
    -   效果: 角色转身时会有自然的角速度，而不是瞬间瞬移。

3.  **步态平滑 (Gait Smoothing)**:
    -   输入: 0 (Walk) 或 1 (Run)。
    -   参数: `gait_change_halflife` (约 0.1s)。
    -   效果: 在走和跑之间平滑过渡。

## 4. 轨迹预测 (Trajectory Prediction)

Motion Matching 的核心是“匹配未来”。我们需要预测未来一段时间（如 1秒）内的角色轨迹，用于在数据库中搜索最相似的片段。

### 4.1 预测逻辑
利用上述的 **Spring/Damper** 逻辑进行迭代预测。
假设当前时刻为 $t_0$，我们需要预测 $t_{20}, t_{40}, t_{60} ...$ (未来 20ms, 40ms...) 的状态。

**算法步骤**:
1.  复制当前的模拟状态（位置、速度、朝向）。
2.  设定目标速度（基于当前用户输入，假设用户在未来1秒内保持按键不变）。
3.  以固定的时间步长（如 20ms）迭代运行 Spring 更新函数。
4.  记录关键时间点（如 0.33s, 0.66s, 1.0s）的位置和朝向。
5.  这些点构成了 **Query Trajectory** (查询轨迹)。

### 4.2 轨迹可视化 (Trajectory Visualization)
在 Scene 中绘制预测的轨迹点（通常是3-4个点）和连线，以及目标方向箭头。这有助于调试控制手感。

## 5. 实现细节 (Implementation Details)

### 5.1 Controller 类定义
```cpp
struct Controller {
    // 状态
    Vec3 position;
    Vec3 velocity;
    Vec3 acceleration; // 用于弹簧计算的内部状态
    Quat rotation;
    float gait;        // 0.0 ~ 1.0
    
    // 参数
    float velocity_halflife = 0.27f;
    float rotation_halflife = 0.27f;
    float gait_halflife     = 0.1f;
    
    // 方法
    void Update(const Input& input, float dt);
    Trajectory Predict(float duration, float dt);
};
```

### 5.2 与 Camera 的融合
在 `OnProcessInput` 或 `Update` 中：

```cpp
if (control_character) {
    // 1. 计算相机相对方向
    Vec3 cam_fwd = camera.GetForward();
    cam_fwd.y = 0; normalize(cam_fwd);
    Vec3 cam_right = camera.GetRight();
    cam_right.y = 0; normalize(cam_right);
    
    Vec3 move_dir = cam_fwd * input.y + cam_right * input.x;
    
    // 2. 更新控制器
    controller.Update(move_dir, input.shift, dt);
    
    // 3. 相机跟随 (可选)
    // camera.SetTarget(controller.position);
} else {
    // 标准相机控制
    cameraManager.ProcessInput(input);
}
```

## 6. 总结 (Summary)
本策略通过 **Spring/Damper** 系统将生硬的键盘输入转化为平滑的物理运动轨迹，并利用 **轨迹预测** 生成 Motion Matching 所需的查询特征。通过 **控制模式切换**，实现了开发调试便利性与游戏操控体验的平衡。

这是一个为您准备的 Prompt，您可以直接发送给另一个 AI（例如能够读取那个参考项目代码库的 AI）。

这个 Prompt 旨在提取核心的数学算法和控制逻辑，以便我们将其移植到当前的键盘/鼠标控制方案中。

***

**Prompt for the other AI:**

> **任务目标**：请分析参考项目（Motion Matching 相关代码库），提取关于角色控制器、弹簧阻尼系统、轨迹预测以及惯性化处理的核心 C++ 代码和逻辑，并整理为一个名为 `reference_controller_code.md` 的文件。
>
> **具体需求**：
>
> 请在项目中查找类似 `controller.cpp`, `controller.h`, `spring.h`, `spring.cpp`, `math.h`, `character.cpp` 等文件，并提取以下内容：
>
> 1.  **弹簧阻尼数学公式 (Spring & Damper Math)**:
>     *   请提取核心的弹簧更新函数实现代码（例如 `simple_spring_damper_exact`, `decay_spring_damper_exact` 或类似的 Halflife 相关函数）。
>     *   包括这些函数依赖的任何数学辅助函数或结构体。
>
> 2.  **角色模拟更新逻辑 (Character Simulation Update)**:
>     *   提取更新角色当前位置、速度和旋转的函数（例如 `simulation_positions_update`, `simulation_rotations_update`）。
>     *   **关键点**：我需要看到如何将“目标速度”（desired velocity）与“当前速度”通过弹簧系统进行混合的代码。
>
> 3.  **轨迹预测逻辑 (Trajectory Prediction)**:
>     *   提取用于 Motion Matching 查询的未来轨迹预测代码（例如 `trajectory_positions_predict`, `trajectory_rotations_predict`）。
>     *   请展示它是如何复用上述弹簧逻辑来预测未来 20ms, 40ms, ... 1s 的状态的。
>
> 4.  **惯性化处理 (Inertialization)**:
>     *   提取用于平滑动画过渡的惯性化逻辑（例如 `inertialize_transition`, `inertialize_update`）。
>     *   包括如何计算和衰减 offset (位置/旋转偏移) 的代码。
>
> 5.  **输入处理与相机相对移动 (Input & Camera Relative)**:
>     *   虽然参考项目是手柄输入，但请提取它如何将“摇杆输入”转换为“世界空间移动方向”的代码。
>     *   特别是涉及相机朝向（Camera Forward/Right）与输入向量混合的部分。
>
> **输出格式**：
> 请将所有提取的代码放入 Markdown 代码块中，并附带简短的注释说明该函数的作用。如果涉及关键常量（如 `halflife` 的具体数值），请务必保留。

***

**为什么需要这些信息？**

1.  **Spring Math**: 这是整个手感的灵魂。我们需要完全一样的数学公式来保证移动的“重量感”和“惯性”。
2.  **Simulation Update**: 我们需要知道它是如何更新 `_currentVelocity` 和 `_currentPosition` 的，以便在我们的 `UpdateFrame` 中复现。
3.  **Trajectory Prediction**: Motion Matching 的搜索依赖于准确的未来轨迹预测。如果预测算法和数据库生成的算法不一致，匹配就会出错（滑步或抽搐）。
4.  **Inertialization**: 这是处理动画切换（如从 Idle 突然切换到 Run）时不发生跳变的关键技术。