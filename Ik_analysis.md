# 足部IK锁定逻辑深度分析

## 问题概述

用户反馈足部IK锁定导致骨骼完全异常，主要问题是"锁定的高度不对"。通过对比`example.cpp`和`update.cpp`的实现，发现了足部IK锁定逻辑中的关键差异。

## 1. example.cpp中的足部IK锁定逻辑

### 1.1 contact_update函数分析

在`example.cpp`中，`contact_update`函数负责管理接触状态和锁定逻辑：

```cpp
void contact_update(
    bool& contact_state,
    bool& contact_lock,
    vec3& contact_position,
    vec3& contact_velocity,
    vec3& contact_point,
    vec3& contact_target,
    vec3& contact_offset_position,
    vec3& contact_offset_velocity,
    const vec3 input_contact_position,
    const bool input_contact_state,
    const float unlock_radius,
    const float foot_height,
    const float halflife,
    const float dt,
    const float eps=1e-8)
```

关键逻辑：

1. **接触激活时**（`!contact_state && input_contact_state`）：
   ```cpp
   contact_lock = true;
   contact_point = contact_position;  // 使用惯性化系统的输出
   contact_point.y = foot_height;     // 将y坐标设置为foot_height
   ```

2. **接触点计算**：
   - `contact_position`是惯性化系统的输出，不是当前的骨骼位置
   - `foot_height`是固定值（0.02f），表示地面以上的高度
   - 接触点被固定在y=`foot_height`的高度

3. **接触位置钳制**：
   ```cpp
   vec3 contact_position_clamp = contact_positions(i);
   contact_position_clamp.y = maxf(contact_position_clamp.y, ik_foot_height);
   ```
   - 确保接触位置不会低于`ik_foot_height`

### 1.2 IK应用逻辑

在`example.cpp`中，IK的应用逻辑：

1. 使用`global_bone_positions`（全局骨骼位置）作为输入
2. 计算目标位置：`contact_position_clamp + (global_bone_positions(heel_bone) - global_bone_positions(toe_bone))`
3. 应用`ik_two_bone`和`ik_look_at`

## 2. update.cpp中的问题分析

### 2.1 原始实现的问题

在原始的`update.cpp`中，存在以下问题：

1. **错误的假设**：
   ```cpp
   vec3 character_position_flat = character_position;
   character_position_flat.y = 0.0f; // Assuming flat ground for now
   ```
   - 假设地面在y=0，但角色可能在不同高度
   - 这个假设导致接触点计算错误

2. **接触点计算问题**：
   - 使用`out_bone_positions(toe_bone)`作为输入接触位置，这是正确的
   - 但是，当接触激活时，`contact_point`被设置为`contact_position`（惯性化系统的输出）
   - 然后`contact_point.y`被设置为`foot_height`，这可能不正确

3. **IK目标位置计算**：
   ```cpp
   contact_positions(i) + (out_bone_positions(heel_bone) - out_bone_positions(toe_bone))
   ```
   - 这个计算可能有问题，因为`contact_positions(i)`是惯性化系统的输出

### 2.2 修复后的问题

在修复后的`update.cpp`中：

1. 移除了`character_position_flat.y = 0.0f`的错误假设
2. 明确计算`heel_to_toe_offset`
3. 使用`contact_positions(i) + heel_to_toe_offset`作为目标位置

但是，仍然可能存在高度问题。

## 3. 锁定高度问题的根本原因

### 3.1 问题分析

锁定高度问题的根本原因可能在于：

1. **地面高度假设**：
   - 代码假设地面在y=0，但实际场景中地面可能在不同高度
   - `foot_height`（0.02f）是相对于y=0的高度

2. **接触点y坐标设置**：
   ```cpp
   contact_point.y = foot_height;
   ```
   - 这强制将接触点的y坐标设置为`foot_height`
   - 如果角色不在y=0的高度，这个计算就是错误的

3. **惯性化系统的影响**：
   - `contact_position`是惯性化系统的输出，不是当前的骨骼位置
   - 当接触激活时，使用`contact_position`作为接触点的基础可能不正确

### 3.2 解决方案

1. **正确的地面高度计算**：
   - 需要根据实际场景计算地面高度
   - 不能假设地面在y=0

2. **接触点计算修正**：
   - 应该使用当前的骨骼位置，而不是惯性化系统的输出
   - 接触点的y坐标应该是：`当前骨骼位置.y - 角色高度 + foot_height`

3. **接触位置钳制**：
   - 需要确保接触位置不会低于实际地面

## 4. 建议的修复方案

### 4.1 修改contact_update逻辑

在`contact_update`函数中，当接触激活时：

```cpp
if (!contact_state && input_contact_state)
{
    // 使用输入接触位置作为基础，而不是contact_position
    contact_lock = true;
    contact_point = input_contact_position;  // 使用当前的骨骼位置
    contact_point.y = ground_height + foot_height;  // 根据实际地面高度计算
    
    inertialize_transition(
        contact_offset_position,
        contact_offset_velocity,
        input_contact_position,
        input_contact_velocity,
        contact_point,
        vec3());
}
```

### 4.2 添加地面高度计算

需要添加地面高度计算逻辑：
- 射线检测或碰撞检测来确定地面高度
- 或者根据场景设置地面高度

### 4.3 修改update.cpp中的IK逻辑

在update.cpp中：

1. 移除所有对y=0的假设
2. 使用正确的地面高度计算接触点
3. 确保接触位置不会低于地面

## 5. 测试建议

1. **地面高度测试**：
   - 测试角色在不同高度的地面上
   - 观察足部是否正确地与地面接触

2. **接触锁定测试**：
   - 测试接触激活和解除的逻辑
   - 观察接触点是否正确计算

3. **IK效果测试**：
   - 测试IK是否正确地调整骨骼位置
   - 观察足部是否平滑地移动

## 6. 总结

足部IK锁定高度问题的根本原因是对地面高度的错误假设和接触点计算逻辑的问题。需要：

1. 正确计算地面高度
2. 修正接触点计算逻辑
3. 确保接触位置不会低于地面
4. 使用当前的骨骼位置而不是惯性化系统的输出作为接触点的基础

这些修改应该解决足部IK锁定导致的骨骼异常问题。
