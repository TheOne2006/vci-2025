# Simulation Bone 添加算法数学推导与正确性分析

本文档详细阐述了在骨骼动画层级中添加 `Simulation` 骨骼（Entity Bone）的数学原理，并验证了坐标变换的正确性。

## 1. 问题定义

原始 BVH 骨骼层级通常以 `Hips`（骨盆）作为根节点（Root）。
目标是在 `Hips` 之上添加一个新的根节点 `Simulation`，使得：
$$ \text{Simulation} \to \text{Hips} \to \dots $$

`Simulation` 骨骼通常用于表示角色在游戏世界中的逻辑位置和朝向（Trajectory），它通常被约束在地面上，且只包含 Y 轴旋转（Heading）。

## 2. 符号定义

*   $P_{hips}^{world}, R_{hips}^{world}$: Hips 骨骼在世界空间中的位置和旋转（四元数）。
*   $P_{sim}^{world}, R_{sim}^{world}$: Simulation 骨骼在世界空间中的位置和旋转。
*   $P_{hips}^{local}, R_{hips}^{local}$: Hips 骨骼相对于父节点（即 Simulation）的局部位置和旋转。

## 3. Simulation 骨骼的计算

### 3.1 位置 ($P_{sim}^{world}$)

Simulation 骨骼的位置通常跟随角色的躯干，但被投影到地面上。我们选择 `Spine2`（脊柱）骨骼的水平位置作为参考，以避免 Hips 摆动带来的高频噪声，同时忽略高度变化。

$$ P_{sim}^{world} = \begin{bmatrix} 1 & 0 & 0 \\ 0 & 0 & 0 \\ 0 & 0 & 1 \end{bmatrix} P_{spine2}^{world} $$

即：
$$ P_{sim}^{world}.x = P_{spine2}^{world}.x $$
$$ P_{sim}^{world}.y = 0 $$
$$ P_{sim}^{world}.z = P_{spine2}^{world}.z $$

**注意**：根据用户要求，此处**不进行**平滑滤波（如 Savitzky-Golay filter），保持数据的原始采样特征。

### 3.2 旋转 ($R_{sim}^{world}$)

Simulation 骨骼的朝向由角色的整体朝向决定。我们使用 `Hips` 的前方向向量，将其投影到水平面上，计算其与世界坐标系 Z 轴（或由数据定义的“前”方向）的夹角。

1.  获取 Hips 的前方向向量（假设局部前方向为 $V_{fwd} = (0, 1, 0)$ 或 $(0, 0, 1)$，具体取决于骨骼定义的轴向，代码中使用了 Y 轴作为前方向进行推导，需根据实际 BVH 轴向调整，这里假设 Y 轴为前向）：
    $$ D_{hips} = R_{hips}^{world} \times \begin{bmatrix} 0 \\ 1 \\ 0 \end{bmatrix} $$

2.  投影到水平面并归一化：
    $$ D_{sim} = \text{normalize}(\begin{bmatrix} D_{hips}.x \\ 0 \\ D_{hips}.z \end{bmatrix}) $$

3.  计算旋转 $R_{sim}^{world}$：
    计算从参考向量 $V_{ref} = (0, 0, 1)$ 到 $D_{sim}$ 的旋转四元数。由于两者都在 XZ 平面上，该旋转轴必定是 Y 轴。

    $$ R_{sim}^{world} = \text{quat\_between}((0, 0, 1), D_{sim}) $$

同样，此处**不进行**平滑处理。

## 4. 坐标变换推导

在引入 Simulation 骨骼后，Hips 不再是世界空间的根，而是 Simulation 的子节点。我们需要计算 Hips 在 Simulation 局部空间下的变换。

### 4.1 变换公式

世界变换可以分解为父节点变换与局部变换的组合：
$$ T_{hips}^{world} = T_{sim}^{world} \times T_{hips}^{local} $$

我们需要求解 $T_{hips}^{local}$：
$$ T_{hips}^{local} = (T_{sim}^{world})^{-1} \times T_{hips}^{world} $$

### 4.2 旋转变换 ($R_{hips}^{local}$)

$$ R_{hips}^{local} = (R_{sim}^{world})^{-1} \times R_{hips}^{world} $$

在四元数运算中：
$$ R_{hips}^{local} = \text{quat\_mul}(\text{quat\_inv}(R_{sim}^{world}), R_{hips}^{world}) $$

### 4.3 位置变换 ($P_{hips}^{local}$)

位置变换受到旋转的影响。
$$ P_{hips}^{world} = P_{sim}^{world} + R_{sim}^{world} \times P_{hips}^{local} $$

求解 $P_{hips}^{local}$：
$$ R_{sim}^{world} \times P_{hips}^{local} = P_{hips}^{world} - P_{sim}^{world} $$
$$ P_{hips}^{local} = (R_{sim}^{world})^{-1} \times (P_{hips}^{world} - P_{sim}^{world}) $$

在代码中的实现：
```python
# 伪代码
inv_sim_rot = quat.inv(sim_rotation)
# 旋转 Hips 的相对位移到 Simulation 的局部空间
positions[:, 0:1] = quat.mul_vec(inv_sim_rot, positions[:, 0:1] - sim_position)
# 计算 Hips 的局部旋转
rotations[:, 0:1] = quat.mul(inv_sim_rot, rotations[:, 0:1])
```

## 5. 数据结构更新

1.  **Positions**:
    *   原 `positions` 数组大小为 $(N, J, 3)$。
    *   新 `positions` 数组大小为 $(N, J+1, 3)$。
    *   新数组第 0 列为 $P_{sim}^{world}$。
    *   新数组第 1 列（原 Hips）更新为 $P_{hips}^{local}$。
    *   其他骨骼位置不变（相对于父节点的位置不变）。

2.  **Rotations**:
    *   原 `rotations` 数组大小为 $(N, J, 4)$。
    *   新 `rotations` 数组大小为 $(N, J+1, 4)$。
    *   新数组第 0 列为 $R_{sim}^{world}$。
    *   新数组第 1 列（原 Hips）更新为 $R_{hips}^{local}$。
    *   其他骨骼旋转不变。

3.  **Hierarchy (Parents)**:
    *   原 `parents` 索引需 +1。
    *   新 `parents` 数组头部插入 -1（Simulation 为根）。
    *   原 Hips 的父节点由 -1 变为 0（Simulation）。

4.  **Offsets**:
    *   Simulation 骨骼的 Offset 设置为 $(0, 0, 0)$。
    *   Hips 骨骼的 Offset 保持原值（通常为 0，因为其位置由 Motion 数据驱动）。

## 6. 正确性验证

为了验证变换的正确性，我们可以尝试还原 Hips 的世界坐标：

$$ P_{hips}^{restored} = P_{sim}^{world} + R_{sim}^{world} \times P_{hips}^{local} $$
$$ = P_{sim}^{world} + R_{sim}^{world} \times ((R_{sim}^{world})^{-1} \times (P_{hips}^{world} - P_{sim}^{world})) $$
$$ = P_{sim}^{world} + (R_{sim}^{world} \times (R_{sim}^{world})^{-1}) \times (P_{hips}^{world} - P_{sim}^{world}) $$
$$ = P_{sim}^{world} + I \times (P_{hips}^{world} - P_{sim}^{world}) $$
$$ = P_{hips}^{world} $$

同理验证旋转：
$$ R_{hips}^{restored} = R_{sim}^{world} \times R_{hips}^{local} $$
$$ = R_{sim}^{world} \times ((R_{sim}^{world})^{-1} \times R_{hips}^{world}) $$
$$ = R_{hips}^{world} $$

推导表明，该变换是数学上可逆且正确的，能够无损地保留原始动画信息，仅改变了层级描述方式。
