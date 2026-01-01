# Motion Matching Pipeline 对比分析

## 概述

本文档对比分析 `example.cpp` 中的完整 Motion Matching pipeline 与当前项目中的 `CaseBVHMotionMatching` 实现，特别关注数据搜索 pipeline 的实现差异。

## 1. example.cpp 中的完整 Motion Matching Pipeline

### 1.1 整体架构
`example.cpp` 实现了一个完整的、生产级的 Motion Matching 系统，包含以下核心模块：

1. **输入处理** - 手柄输入、相机控制、步态切换
2. **轨迹预测** - 预测未来20、40、60帧的位置和方向
3. **查询向量构建** - 构建包含当前状态和未来轨迹的特征向量
4. **数据库搜索** - 在动画数据库中寻找最佳匹配帧
5. **惯性化过渡** - 平滑过渡到新动画帧
6. **同步与调整** - 同步角色与模拟对象，包含多种策略：
   - 同步（Synchronization）
   - 调整（Adjustment）
   - 钳制（Clamping）
7. **IK与脚部锁定** - 逆向运动学和脚部接触处理

### 1.2 数据搜索 Pipeline 详解

#### 查询向量构建
```cpp
// 特征向量包含：
// 1. 左脚位置 (3个float)
// 2. 右脚位置 (3个float)
// 3. 左脚速度 (3个float)
// 4. 右脚速度 (3个float)
// 5. 髋部速度 (3个float)
// 6. 轨迹位置 (6个float: 20,40,60帧的x,z坐标)
// 7. 轨迹方向 (6个float: 20,40,60帧的x,z坐标)
// 总计：27个特征
```

#### 搜索策略
1. **定期搜索** - 每0.1秒执行一次搜索
2. **强制搜索** - 当输入变化剧烈时立即搜索
3. **动画结束搜索** - 当到达动画片段末尾时搜索

#### 搜索算法
```cpp
database_search(
    best_index,
    best_cost,
    db,
    query);
```

### 1.3 关键特性

#### 轨迹预测
- 使用弹簧阻尼系统预测未来轨迹
- 考虑相机旋转和输入变化
- 预测多个时间点（20, 40, 60帧）

#### 惯性化系统
- 完整的惯性化过渡机制
- 支持位置和旋转的平滑过渡
- 处理根骨骼的特殊情况

#### 同步策略
1. **同步（Synchronization）** - 直接将角色根骨骼与模拟对象对齐
2. **调整（Adjustment）** - 使用弹簧系统缓慢调整角色位置
3. **钳制（Clamping）** - 限制角色与模拟对象的最大偏差

#### IK系统
- 完整的脚部锁定机制
- 两关节IK（髋-膝-踝）
- 脚趾旋转防止穿透地面

## 2. 当前 MotionMatching Pipeline 的问题

### 2.1 主要差异

#### 查询向量构建不完整
当前实现（`update.cpp`）中的查询向量构建存在问题：

```cpp
// 问题1：只复制了前15个特征（脚部位置、速度、髋部速度）
for (int i = 0; i < 15; i++) {
    query(i) = (db.features(current_frame_index, i) * db.features_scale(i)) + db.features_offset(i);
}

// 问题2：轨迹特征计算使用了错误的参考系
// 应该使用角色根骨骼的旋转，但代码中使用了 simulation_rotation
vec3 traj_pos_local = quat_mul_vec3(quat_inv(simulation_rotation), traj_pos - simulation_position);
```

#### 缺少"脚的desired position"
这是用户提到的主要问题。在完整的 Motion Matching 中，脚的期望位置应该：

1. **从当前动画帧提取** - 使用数据库中的脚部位置
2. **考虑未来轨迹** - 脚部位置应与预测轨迹一致
3. **用于查询匹配** - 作为特征向量的一部分

当前实现中，查询向量使用了数据库中的脚部位置，但这些位置：
- 没有根据当前角色状态进行调整
- 没有考虑输入控制的期望方向
- 导致角色无法正确响应输入

### 2.2 搜索策略问题

#### 搜索频率
- `example.cpp`: 每0.1秒搜索一次，或当输入剧烈变化时
- 当前实现: 每帧都搜索（可能效率低下）

#### 过渡逻辑
- `example.cpp`: 使用惯性化过渡，平滑处理动画切换
- 当前实现: 简单的硬切换，可能导致 popping

### 2.3 同步机制缺失，这里使用synchronize状态下的同步策略，而不是 Adjustment 和 clamping 机制

## 3. 修复建议

### 3.1 修复查询向量构建

```cpp
// 正确构建查询向量：
void build_query_vector(slice1d<float> query, 
                       const database& db,
                       int current_frame_index,
                       const vec3& root_position,
                       const quat& root_rotation,
                       const slice1d<vec3>& trajectory_positions,
                       const slice1d<quat>& trajectory_rotations) {
    int offset = 0;
    
    // 1. 脚部位置和速度（从当前动画帧，但可能需要调整）
    query_copy_denormalized_feature(query, offset, 3, db.features(current_frame_index), 
                                   db.features_offset, db.features_scale); // 左脚位置
    query_copy_denormalized_feature(query, offset, 3, db.features(current_frame_index), 
                                   db.features_offset, db.features_scale); // 右脚位置
    query_copy_denormalized_feature(query, offset, 3, db.features(current_frame_index), 
                                   db.features_offset, db.features_scale); // 左脚速度
    query_copy_denormalized_feature(query, offset, 3, db.features(current_frame_index), 
                                   db.features_offset, db.features_scale); // 右脚速度
    query_copy_denormalized_feature(query, offset, 3, db.features(current_frame_index), 
                                   db.features_offset, db.features_scale); // 髋部速度
    
    // 2. 轨迹位置特征（相对于角色根骨骼）
    query_compute_trajectory_position_feature(query, offset, root_position, root_rotation, 
                                             trajectory_positions);
    
    // 3. 轨迹方向特征
    query_compute_trajectory_direction_feature(query, offset, root_rotation, 
                                              trajectory_rotations);
}
```

### 3.2 实现完整的同步策略

这里使用synchronize状态下的同步策略，而不是 Adjustment 和 clamping 机制


### 3.3 改进搜索策略

1. **实现定期搜索** - 每0.1秒搜索一次
2. **添加强制搜索触发** - 当输入变化超过阈值时
3. **优化搜索范围** - 限制搜索范围提高性能

## 4. 关键区别总结

| 特性 | example.cpp | 当前实现 |
|------|-------------|----------|
| 查询向量 | 完整的27个特征 | 不完整的特征构建 |
| 脚部期望位置 | 从动画数据提取并调整 | 缺失或错误 |
| 搜索频率 | 0.1秒 + 触发条件 | 每帧搜索 |
| 过渡平滑性 | 惯性化过渡 | 硬切换 |
| IK系统 | 完整的脚部锁定 | 基本实现但可能有问题 |

## 5. 结论

当前 MotionMatching pipeline 的主要问题是：

1. **查询向量构建不完整** - 缺少正确的轨迹特征计算
2. **缺少脚的desired position** - 脚部位置没有根据输入和轨迹调整
3. **同步策略简单** - 只有强制同步，缺少调整和钳制机制
4. **搜索策略低效** - 每帧搜索，缺少优化

修复方向：
1. 参考 `example.cpp` 实现完整的查询向量构建
2. 添加调整（Adjustment）和钳制（Clamping）机制
3. 优化搜索策略，实现定期搜索和触发搜索
4. 确保脚部位置正确反映输入控制的期望方向

通过修复这些问题，可以使 Motion Matching 系统更准确、更平滑地响应用户输入。
