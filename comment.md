# 足部IK锁定问题分析与修复

## 问题分析

通过对比`example.cpp`和`update.cpp`中的足部IK实现，发现了以下问题：

### 1. 同步策略过于复杂
- 原代码使用了adjustment和clamping策略，导致同步过程复杂
- 根据用户要求，实现了最简单的Synchronization（synchronization=1，直接同步）

### 2. 查询向量构建不完整
- 原代码只复制了前15个特征，但完整的查询向量应该包含27个特征
- 添加了辅助函数来正确构建查询向量：
  - `query_copy_denormalized_feature`
  - `query_compute_trajectory_position_feature`
  - `query_compute_trajectory_direction_feature`

### 3. 足部IK实现问题
- **问题1**：`character_position_flat.y = 0.0f`假设不正确
  - 原代码假设角色位置在y=0的高度，但实际角色可能在不同高度
  - 修复：移除了这个错误的假设

- **问题2**：接触点计算错误
  - 在`contact_update`函数中，当接触激活时，代码设置`contact_point.y = foot_height`
  - 但`contact_point`是惯性化系统的输出，而不是当前的骨骼位置
  - 修复：使用当前的全局骨骼位置作为输入接触位置

- **问题3**：IK目标位置计算错误
  - 原代码：`contact_positions(i) + (out_bone_positions(heel_bone) - out_bone_positions(toe_bone))`
  - 这个计算可能不正确，因为`contact_positions(i)`是惯性化系统的输出
  - 修复：明确计算heel_to_toe_offset，然后加到接触位置上

- **问题4**：缺少IK启用开关
  - 原代码中IK总是启用，无法关闭
  - 修复：添加了`_enableIK`变量和UI复选框

## 修改内容

### 1. `update.cpp`修改
- 简化同步策略：直接同步character和simulation状态
- 修复查询向量构建：使用完整的27个特征
- 修复足部IK实现：
  - 移除错误的`character_position_flat.y = 0.0f`假设
  - 使用正确的全局骨骼位置作为输入接触位置
  - 修复IK目标位置计算
  - 添加清晰的注释说明

### 2. `CaseBVHMotionMatching.h`修改
- 添加`_enableIK`成员变量（默认启用）

### 3. `CaseBVHMotionMatching.cpp`修改
- 在UI中添加"Enable IK"复选框
- 将`_enableIK`传递给`MotionMatchingUpdate`函数

## 测试建议

1. **编译测试**：已通过编译，无错误
2. **功能测试**：
   - 运行程序，检查IK复选框是否正常工作
   - 启用IK时，观察足部是否与地面正确接触
   - 禁用IK时，观察动画是否正常播放
3. **异常测试**：
   - 测试各种运动状态（行走、奔跑、转向）
   - 观察骨骼是否出现异常变形

## 注意事项

1. IK参数可能需要根据实际场景调整：
   - `ik_foot_height`：足部高度
   - `ik_unlock_radius`：解锁半径
   - `ik_blending_halflife`：混合半衰期

2. 如果仍然出现骨骼异常，可能需要进一步检查：
   - 骨骼层级结构是否正确
   - 接触状态检测是否准确
   - IK算法的限制条件

## 总结

通过对比`example.cpp`的参考实现，修复了`update.cpp`中的足部IK问题。主要修复了接触点计算、IK目标位置计算和错误的假设。同时添加了IK启用开关，方便调试和测试。

这些修改应该解决足部IK锁定导致的骨骼异常问题，并使足部与地面接触更加一致。
