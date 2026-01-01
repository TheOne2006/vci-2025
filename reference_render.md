## 1. 模拟对象 (Simulation Object)
这部分渲染了 Spring 系统计算出的模拟目标状态及其预测的未来轨迹。

*   **位置圆环 (Spring Ring)**:
    *   **逻辑**: 在 `simulation_position` 处渲染一个线框圆柱体（高度极低，类似圆环）。
    *   **参数**: 半径 `0.6f`。
    *   **颜色**: `ORANGE` (橙色)。
    *   **用途**: 表示模拟系统的当前位置，即“底下的 spring 圆环”。

*   **中心点**:
    *   **逻辑**: 在 `simulation_position` 处渲染一个线框小球。
    *   **参数**: 半径 `0.05f`。
    *   **颜色**: `ORANGE` (橙色)。

*   **方向指示箭头**:
    *   **逻辑**: 从 `simulation_position` 出发，沿着 `simulation_rotation` 的 Z 轴（前方）方向渲染一条线段。
    *   **参数**: 长度 `0.6f`。
    *   **颜色**: `ORANGE` (橙色)。

*   **未来轨迹 (Future Trajectory)**:
    *   **逻辑**: 渲染预测的未来轨迹点 `trajectory_positions`。
        *   **轨迹点**: 遍历预测的轨迹点，在每个点渲染线框小球 (半径 `0.05f`)。
        *   **连线**: 用线段连接相邻的轨迹点。
        *   **方向指示**: 在每个轨迹点处，沿着该点的预测旋转 `trajectory_rotations` 的 Z 轴方向渲染一条线段 (长度 `0.6f`)。
    *   **颜色**: `ORANGE` (橙色)。