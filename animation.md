# 动画系统统一设计范式

## 当前问题分析

通过分析现有代码，发现以下主要问题：

### 1. API设计不一致
- **bvh_loader.hpp** 提供了 `bvh_motion` 结构体，包含自己的前向运动学方法 `batch_forward_kinematics`
- **bone_operations.hpp** 提供了通用的前向运动学函数 `forward_kinematics`、`forward_kinematics_full` 等
- 两套API功能重叠但接口不同，形成了独立的系统

### 2. 数据结构不统一
- `bvh_motion`: 使用 `array2d<vec3>` 和 `array2d<quat>` 存储每帧每个关节的位置和旋转
- `database`: 使用 `array2d<vec3>` 和 `array2d<quat>` 存储每帧每个关节的位置和旋转
- `character`: 使用不同的数据结构存储角色网格和骨骼权重
- 缺乏统一的动画数据表示

### 3. 缺乏统一的骨骼层次结构
- `bvh_motion`: 有 `joint_parents`、`joint_names`、`joint_offsets`
- `database`: 只有 `bone_parents`
- `character`: 有固定的骨骼枚举但没有层次结构信息

### 4. 运动学计算重复
- `bvh_motion::batch_forward_kinematics`: 实现了前向运动学
- `bone_operations` 中的函数也实现了前向运动学
- 两者实现相似但接口不同，导致代码重复和维护困难

## 统一设计目标

1. **单一数据源**: 统一的动画数据结构
2. **统一API**: 一致的运动学计算接口
3. **模块化设计**: 清晰的职责分离
4. **可扩展性**: 支持多种动画格式和算法
5. **性能优化**: 高效的内存布局和计算

## 设计方案（修订版）

### 设计原则

1. **保持array1d/array2d的简单性**：只用于存储POD（Plain Old Data）类型，如`float`、`int`、`vec3`、`quat`等
2. **使用枚举标识骨骼**：避免使用`std::string`存储骨骼名称，使用枚举或整数ID
3. **分离复杂数据**：将复杂数据（如名称映射）放在单独的`std::vector`或`std::unordered_map`中
4. **保持向后兼容**：尽可能与现有代码兼容

### 1. 简化的骨骼层次结构

```cpp
// 骨骼信息（全部为POD类型，适合array1d存储）
struct BoneInfo {
    int parent;          // 父骨骼索引，-1表示根骨骼
    vec3 local_offset;   // 局部偏移量
    vec3 rest_position;  // 静止位置
    quat rest_rotation;  // 静止旋转
    int channel_count;   // 通道数量（0, 3, 6）
};

// 骨骼层次结构
struct Skeleton {
    array1d<BoneInfo> bones;           // 骨骼信息数组
    int num_bones;                     // 骨骼数量
    
    // 可选：名称映射（独立于array1d）
    std::vector<std::string> bone_names;                    // 骨骼名称
    std::unordered_map<std::string, int> name_to_index;     // 名称到索引映射
    
    // 工具方法
    int find_bone(const std::string& name) const;  // 通过名称查找
    bool validate() const;                         // 验证层次结构
    void compute_rest_pose();                      // 计算静止姿态
    
    // 从现有枚举构建
    static Skeleton from_standard_humanoid();
};
```

### 2. 统一的动画数据结构

```cpp
// 动画帧数据（全部为POD类型，适合array1d存储）
struct AnimationFrame {
    array1d<vec3> local_positions;      // 局部位置
    array1d<quat> local_rotations;      // 局部旋转
    // 可选：速度信息
    array1d<vec3> local_velocities;     // 局部速度
    array1d<vec3> local_angular_velocities; // 局部角速度
};

// 动画剪辑
struct AnimationClip {
    Skeleton skeleton;                  // 骨骼层次结构
    array1d<AnimationFrame> frames;     // 动画帧序列
    float frame_rate;                   // 帧率
    float duration;                     // 持续时间（秒）
    int num_frames;                     // 帧数
    
    // 元数据（独立存储，不放在array1d中）
    std::string name;
    std::string source_file;
    
    // 工具方法
    AnimationClip sub_sequence(int start_frame, int end_frame) const;
    void append(const AnimationClip& other);
    void resample(float new_frame_rate);
    
    // 与现有bvh_motion的转换
    static AnimationClip from_bvh_motion(const bvh_motion& motion);
    bvh_motion to_bvh_motion() const;
};
```

### 3. 统一的运动学API（保持与现有代码兼容）

```cpp
namespace Kinematics {
    // 现有API的增强版本
    void forward_kinematics(
        slice1d<vec3> global_positions,
        slice1d<quat> global_rotations,
        const slice1d<vec3> local_positions,
        const slice1d<quat> local_rotations,
        const slice1d<int> bone_parents);
    
    // 带速度的前向运动学
    void forward_kinematics_with_velocity(
        slice1d<vec3> global_positions,
        slice1d<vec3> global_velocities,
        slice1d<quat> global_rotations,
        slice1d<vec3> global_angular_velocities,
        const slice1d<vec3> local_positions,
        const slice1d<vec3> local_velocities,
        const slice1d<quat> local_rotations,
        const slice1d<vec3> local_angular_velocities,
        const slice1d<int> bone_parents);
    
    // 批量前向运动学（针对AnimationClip）
    void batch_forward_kinematics(
        array2d<vec3> global_positions,
        array2d<quat> global_rotations,
        const AnimationClip& clip);
    
    // 与现有bvh_motion兼容的版本
    void batch_forward_kinematics(
        array2d<vec3> global_positions,
        array2d<quat> global_rotations,
        const struct bvh_motion& motion);
}
```

### 4. 统一的BVH加载器（增强现有功能）

```cpp
// 增强现有的bvh_motion结构
struct bvh_motion {
    // 现有字段保持不变
    array1d<std::string> joint_names;    // 关节名称（独立存储）
    array1d<int>         joint_parents;  // 父关节索引
    array1d<int>         joint_channels; // 通道数量
    array1d<vec3>        joint_offsets;  // 局部偏移
    
    array2d<vec3> joint_positions;       // 局部位置
    array2d<quat> joint_rotations;       // 局部旋转
    
    int num_frames = 0;
    int num_joints = 0;
    
    // 新增：转换为AnimationClip
    AnimationClip to_animation_clip() const;
    
    // 新增：从AnimationClip构建
    static bvh_motion from_animation_clip(const AnimationClip& clip);
    
    // 现有方法保持不变
    void load(const char* filename);
    void batch_forward_kinematics(...) const;
    // ... 其他现有方法
};

// BVH加载工具
namespace BVHLoader {
    // 加载BVH文件到AnimationClip
    AnimationClip load_to_clip(const std::string& filename);
    
    // 保存AnimationClip为BVH格式
    bool save_from_clip(const AnimationClip& clip, const std::string& filename);
    
    // 调整骨骼映射
    void remap_to_standard_skeleton(bvh_motion& motion, const Skeleton& target_skeleton);
}
```

### 5. 统一的数据库设计（最小化修改）

```cpp
// 增强现有的database结构
struct database {
    // 现有字段保持不变
    array2d<vec3> bone_positions;
    array2d<vec3> bone_velocities;
    array2d<quat> bone_rotations;
    array2d<vec3> bone_angular_velocities;
    array1d<int>  bone_parents;
    
    array1d<int> range_starts;
    array1d<int> range_stops;
    
    array2d<float> features;
    array1d<float> features_offset;
    array1d<float> features_scale;
    
    array2d<bool> contact_states;
    
    array2d<float> bound_sm_min;
    array2d<float> bound_sm_max;
    array2d<float> bound_lr_min;
    array2d<float> bound_lr_max;
    
    // 新增：骨骼信息
    Skeleton skeleton;  // 骨骼层次结构
    
    // 新增：从AnimationClip构建
    void build_from_clip(const AnimationClip& clip);
    
    // 新增：转换为AnimationClip
    AnimationClip to_animation_clip() const;
    
    // 现有方法保持不变
    int nframes() const { return bone_positions.rows; }
    int nbones() const { return bone_positions.cols; }
    // ... 其他现有方法
};
```

### 6. 统一的角色系统（增强现有结构）

```cpp
// 增强现有的character结构
struct character {
    // 现有字段保持不变
    array1d<vec3>           positions;
    array1d<vec3>           normals;
    array1d<vec2>           texcoords;
    array1d<unsigned short> triangles;
    
    array2d<float>          bone_weights;
    array2d<unsigned short> bone_indices;
    
    array1d<vec3> bone_rest_positions;
    array1d<quat> bone_rest_rotations;
    
    // 新增：骨骼信息
    Skeleton skeleton;  // 骨骼层次结构
    
    // 新增：当前动画状态
    AnimationClip current_animation;
    float current_time = 0.0f;
    float playback_speed = 1.0f;
    
    // 新增：动画混合
    struct AnimationBlend {
        AnimationClip clip;
        float weight;
        float time;
    };
    std::vector<AnimationBlend> active_blends;
    
    // 工具方法
    void set_animation(const AnimationClip& clip);
    void blend_animation(const AnimationClip& clip, float blend_weight, float blend_time);
    void update(float delta_time);
    void compute_pose(array1d<vec3>& bone_positions, array1d<quat>& bone_rotations) const;
    
    // 蒙皮计算（现有函数增强）
    void skin_pose(
        array1d<vec3>& animated_positions,
        array1d<vec3>& animated_normals) const;
};
```

## 核心重构方案

### 重点：统一bvh_loader.hpp、Character.hpp和bone_operations.hpp的API

#### 1. 创建统一的骨骼映射系统

```cpp
// 在character.hpp中扩展骨骼枚举，使其成为标准
namespace Bones {
    enum StandardBones {
        Hips = 0,
        LeftUpLeg, LeftLeg, LeftFoot, LeftToe,
        RightUpLeg, RightLeg, RightFoot, RightToe,
        Spine, Spine1, Spine2, Neck, Head,
        LeftShoulder, LeftArm, LeftForeArm, LeftHand,
        RightShoulder, RightArm, RightForeArm, RightHand,
        Count  // 骨骼总数
    };
    
    // 骨骼名称映射（用于BVH加载时的名称解析）
    extern const std::unordered_map<std::string, int> NameToBone;
    
    // 获取骨骼名称
    const char* get_bone_name(int bone_index);
}
```

#### 2. 增强bvh_motion以支持标准骨骼映射

```cpp
// 在bvh_loader.hpp中增强bvh_motion
struct bvh_motion {
    // 现有字段...
    
    // 新增：映射到标准骨骼
    array1d<int> joint_to_standard_bone;  // 每个关节对应的标准骨骼索引（-1表示无对应）
    
    // 新增：转换为标准骨骼顺序
    void remap_to_standard_skeleton();
    
    // 新增：使用统一的运动学API
    void forward_kinematics(
        array2d<vec3>& global_positions,
        array2d<quat>& global_rotations) const {
        // 调用bone_operations中的函数
        // ...
    }
};
```

#### 3. 统一运动学API入口点

```cpp
// 创建新的头文件：animation_core.hpp
#pragma once
#include "bone_operations.hpp"
#include "bvh_loader.hpp"
#include "character.hpp"

namespace AnimationCore {
    // 统一的运动学计算
    void compute_forward_kinematics(
        slice1d<vec3> global_positions,
        slice1d<quat> global_rotations,
        const slice1d<vec3> local_positions,
        const slice1d<quat> local_rotations,
        const slice1d<int> bone_parents);
    
    // 批量版本（兼容bvh_motion）
    void batch_compute_forward_kinematics(
        array2d<vec3> global_positions,
        array2d<quat> global_rotations,
        const bvh_motion& motion);
    
    // 角色姿态计算
    void compute_character_pose(
        character& character,
        const slice1d<vec3> bone_positions,
        const slice1d<quat> bone_rotations);
}
```

#### 4. 创建适配层函数

```cpp
// 在现有bone_operations.hpp中添加适配函数
namespace VCX::Labs::MotionMatching::Core::Animation {
    // 为bvh_motion提供适配接口
    void forward_kinematics_for_bvh(
        array2d<vec3>& global_positions,
        array2d<quat>& global_rotations,
        const bvh_motion& motion,
        const slice1d<int> frame_ids = slice1d<int>(0, nullptr));
    
    // 为character提供适配接口
    void apply_pose_to_character(
        character& character,
        const slice1d<vec3> bone_positions,
        const slice1d<quat> bone_rotations);
}
```

## 实施步骤（简化版）

### 第1步：扩展骨骼枚举和映射
1. 在character.hpp中扩展骨骼枚举
2. 创建骨骼名称到索引的映射表
3. 提供工具函数进行名称解析

### 第2步：增强bvh_loader
1. 添加`joint_to_standard_bone`字段到bvh_motion
2. 实现BVH关节到标准骨骼的自动映射
3. 添加使用统一运动学API的方法

### 第3步：创建统一API层
1. 创建animation_core.hpp头文件
2. 实现统一的运动学函数
3. 提供适配现有数据结构的接口

### 第4步：更新使用代码
1. 修改现有代码使用统一API
2. 保持向后兼容性
3. 提供迁移示例

## 优势

### 1. 最小化修改
- 保持现有数据结构基本不变
- 通过适配层连接现有组件
- 逐步迁移，风险可控

### 2. 统一API入口
- 单一的运动学计算入口点
- 一致的函数命名和参数顺序
- 减少API学习成本

### 3. 保持性能
- 不增加额外数据复制
- 重用现有高效实现
- 适配层开销极小

### 4. 易于扩展
- 新动画格式可以轻松集成
- 支持多种骨骼映射方案
- 模块化设计便于维护

## 优势

### 1. 代码一致性
- 统一的数据结构减少转换开销
- 一致的API降低学习成本
- 减少代码重复和维护负担

### 2. 性能优化
- 连续内存布局提高缓存效率
- 批量操作支持SIMD优化
- 减少数据复制和转换

### 3. 可扩展性
- 易于添加新的动画格式
- 支持多种运动学算法
- 模块化设计便于测试和维护

### 4. 开发效率
- 清晰的接口定义
- 更好的错误处理和数据验证
- 丰富的工具函数

## 迁移策略

### 渐进式迁移
1. 首先实现新数据结构，保持向后兼容
2. 逐步重构各个模块
3. 提供数据转换工具

### 兼容性保证
1. 保持现有API在过渡期间可用
2. 提供详细的迁移指南
3. 逐步弃用旧API

## 结论

通过统一的设计范式，我们可以解决当前动画系统中存在的API不一致、数据重复和代码冗余问题。新设计提供了清晰的模块划分、一致的数据结构和统一的API，将显著提高代码的可维护性、性能和可扩展性。

建议按照上述方案逐步实施重构，首先从基础数据结构开始，然后逐步迁移各个模块，最终实现完全统一的动画系统。
