# 调用 Pipeline 分析

## 1. 整体架构

项目采用基于案例（Case）的架构，每个实验室（Lab）包含多个案例，每个案例实现特定的图形学算法。

## 2. 主要文件及其作用

### 2.1 main.cpp
- **位置**: `src/VCX/Labs/1-Drawing2D/main.cpp`
- **作用**: 程序入口点
- **关键代码**:
```cpp
int main() {
    using namespace VCX;
    return Engine::RunApp<Labs::Drawing2D::App>(Engine::AppContextOptions {
        .Title      = "VCX Labs 1: Drawing 2D",
        .WindowSize = { 1024, 768 },
        .FontSize   = 16,
        .IconFileNames = Assets::DefaultIcons,
        .FontFileNames = Assets::DefaultFonts,
    });
}
```

### 2.2 app.cpp (引擎部分)
- **位置**: `src/VCX/Engine/app.cpp`
- **作用**: 提供应用程序框架，管理 GLFW、OpenGL、ImGui 的初始化和主循环
- **关键流程**:
  1. `RunApp_Init()`: 初始化 GLFW、OpenGL、ImGui
  2. `RunApp_Main()`: 主循环，每帧调用 `RunApp_Frame()`
  3. `RunApp_Frame()`: 每帧执行，调用 `app.OnFrame()`

### 2.3 App.h / App.cpp (实验室应用)
- **位置**: `src/VCX/Labs/1-Drawing2D/App.h`, `src/VCX/Labs/1-Drawing2D/App.cpp`
- **作用**: 实验室特定的应用程序，管理多个案例
- **关键代码**:
```cpp
class App : public VCX::Engine::IApp {
private:
    Common::UI _ui;
    // 各个案例实例
    CaseDithering  _caseDithering;
    CaseFiltering  _caseFiltering;
    // ...
    std::vector<std::reference_wrapper<Common::ICase>> _cases;
    
public:
    void OnFrame() override;
};

void App::OnFrame() {
    _ui.Setup(_cases, _caseId);  // 设置 UI 并渲染当前案例
}
```

### 2.4 UI.h / UI.cpp
- **位置**: `src/VCX/Labs/Common/UI.h`, `src/VCX/Labs/Common/UI.cpp`
- **作用**: 管理用户界面，包括侧边栏和主窗口
- **关键方法**:
  - `Setup()`: 设置整个 UI 布局
  - `setupSideWindow()`: 渲染侧边栏（案例列表和属性）
  - `setupMainWindow()`: 渲染主窗口（案例内容）

### 2.5 ICase.h
- **位置**: `src/VCX/Labs/Common/ICase.h`
- **作用**: 定义案例接口，所有案例必须实现此接口

## 3. 调用 Pipeline

```
main.cpp
    ↓
Engine::RunApp<Labs::Drawing2D::App>()
    ↓
app.cpp: RunApp_Main() 主循环
    ↓ 每帧调用
app.cpp: RunApp_Frame()
    ↓
app.OnFrame()  // Labs::Drawing2D::App::OnFrame()
    ↓
UI::Setup(_cases, _caseId)
    ├─→ setupSideWindow()  // 侧边栏
    │      ├─→ cases[caseId].get().GetName()  // 获取案例名称
    │      └─→ cases[caseId].get().OnSetupPropsUI()  // 设置属性 UI
    │
    └─→ setupMainWindow(cases[caseId])  // 主窗口
           └─→ casei.OnRender(desiredSize)  // 渲染案例内容
           └─→ casei.OnProcessInput(pos)  // 处理输入事件
```

## 4. 需要覆盖的函数

所有案例类必须继承 `Common::ICase` 并实现以下纯虚函数：

### 4.1 必须实现的函数

1. **`virtual std::string_view const GetName() override`**
   - **作用**: 返回案例的名称，显示在侧边栏案例列表中
   - **示例**: `return "Image Dithering";`

2. **`virtual Common::CaseRenderResult OnRender(std::pair<std::uint32_t, std::uint32_t> const desiredSize) override`**
   - **作用**: 渲染案例内容，返回渲染结果
   - **参数**: `desiredSize` - 期望的渲染尺寸
   - **返回值**: `CaseRenderResult` 结构体，包含：
     - `Fixed`: 是否固定尺寸（true 时显示滚动条）
     - `Flipped`: 是否翻转图像（Y 轴）
     - `Image`: 纹理引用
     - `ImageSize`: 图像尺寸
   - **调用时机**: 每帧调用，用于更新显示内容

### 4.2 可选实现的函数

3. **`virtual void OnSetupPropsUI() override`**
   - **作用**: 设置案例的属性 UI（在侧边栏的 "PROPERTIES" 部分）
   - **典型用途**: 添加 ImGui 控件（复选框、单选按钮、滑块等）
   - **调用时机**: 每帧调用，当案例被选中时

4. **`virtual void OnProcessInput(ImVec2 const & pos) override`**
   - **作用**: 处理输入事件（鼠标位置）
   - **参数**: `pos` - 鼠标在案例画布上的相对位置
   - **典型用途**: 处理鼠标交互、拖拽、点击等
   - **调用时机**: 每帧调用，当鼠标在案例区域时

## 5. 案例实现示例

以 `CaseDithering` 为例：

```cpp
class CaseDithering : public Common::ICase {
public:
    // 必须实现的函数
    virtual std::string_view const GetName() override { 
        return "Image Dithering"; 
    }
    
    virtual Common::CaseRenderResult OnRender(
        std::pair<std::uint32_t, std::uint32_t> const desiredSize) override {
        // 1. 检查是否需要重新计算
        // 2. 启动异步任务进行计算
        // 3. 更新纹理
        // 4. 返回渲染结果
        return Common::CaseRenderResult {
            .Fixed     = true,
            .Image     = _texture,
            .ImageSize = c_Size,
        };
    }
    
    // 可选实现的函数
    virtual void OnSetupPropsUI() override {
        // 添加 ImGui 控件
        ImGui::Checkbox("Zoom Tooltip", &_enableZoom);
        ImGui::Text("Algorithm Type");
        // ... 更多控件
    }
    
    virtual void OnProcessInput(ImVec2 const & pos) override {
        // 处理鼠标交互
        if (_enableZoom && ImGui::IsItemHovered()) {
            Common::ImGuiHelper::ZoomTooltip(_texture, c_Size, pos);
        }
    }
    
private:
    // 成员变量
    Engine::GL::UniqueTexture2D _texture;
    Common::ImageRGB _input;
    bool _recompute = true;
    // ...
};
```

## 6. 异步任务处理

案例中常用 `Engine::Async<T>` 来处理耗时的计算任务，避免阻塞主线程：

```cpp
Engine::Async<Common::ImageRGB> _task;

// 在 OnRender 中启动任务
_task.Emplace([&input = _input]() {
    Common::ImageRGB tex(size.first, size.second);
    // 执行耗时的计算
    SomeAlgorithm(tex, input);
    return tex;
});

// 获取任务结果（如果已完成）
_texture.Update(_task.ValueOr(_empty));
```

## 7. 纹理管理

- 使用 `Engine::GL::UniqueTexture2D` 管理 OpenGL 纹理
- 在 `OnRender` 中更新纹理：`_texture.Update(imageData)`
- 纹理参数在构造函数中设置

## 8. 最佳实践

1. **性能优化**:
   - 使用异步任务处理耗时计算
   - 只在需要时重新计算（通过 `_recompute` 标志控制）
   - 缓存计算结果

2. **内存管理**:
   - 合理管理纹理内存
   - 及时释放不再需要的资源

3. **UI/UX**:
   - 提供清晰的属性控制
   - 添加适当的工具提示
   - 处理用户输入反馈

4. **代码组织**:
   - 将算法实现放在单独的 `tasks.h`/`tasks.cpp` 中
   - 保持案例类专注于 UI 和状态管理
