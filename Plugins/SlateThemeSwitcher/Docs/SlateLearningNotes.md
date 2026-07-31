# UE Slate 学习笔记

> 这是一份持续更新的学习文档。后续有关 UE Slate、编辑器 UI、样式、主题、输入、性能等内容都追加在这里。

## 学习目录

1. [Slate 的基本逻辑](#1-slate-的基本逻辑)
2. [常用控件、容器与布局](#2-常用控件容器与布局)
3. [样式系统与主题切换](#3-样式系统与主题切换)
4. [编辑器插件、菜单与 Nomad Tab](#4-编辑器插件菜单与-nomad-tab)
5. [自定义控件与绘制](#5-自定义控件与绘制)
6. [输入、焦点与拖拽](#6-输入焦点与拖拽)
7. [性能、失效与调试工具](#7-性能失效与调试工具)
8. [虚拟化列表、树与多列表格](#8-虚拟化列表树与多列表格)
9. [命令、快捷键与 ToolMenus](#9-命令快捷键与-toolmenus)
10. [Details Panel 属性定制](#10-details-panel-属性定制)
11. [动画、Active Timer 与异步任务](#11-动画active-timer-与异步任务)
12. [DPI、本地化、无障碍与可测试性](#12-dpi本地化无障碍与可测试性)
13. [SWebBrowser 与 Slate/网页混合界面](#13-swebbrowser-与-slate网页混合界面)

---

## 1. Slate 的基本逻辑

### 1.1 Slate 是什么

Slate 是 Unreal Engine 的底层 C++ UI 框架。Unreal Editor 的大部分界面，以及运行时的一些原生 UI，都是由 Slate 构建的。

可以先用一句话理解它：

> Slate 把 UI 表达成一棵控件树，然后反复对这棵树执行布局、绘制和输入路由。

Slate 的几个核心特点：

- **即时描述、保留对象**：界面通过类似声明式 DSL 的 C++ 语法构建，但创建出来的 `SWidget` 会作为对象长期存在。
- **轻量**：`SWidget` 不是 `UObject`，不参与垃圾回收和反射，通常由共享指针管理。
- **数据驱动**：属性可以绑定值、成员函数或 Lambda；数据变化后，界面在需要时重新取值。
- **控件树驱动**：父控件负责组织子控件，容器决定子控件如何排列。
- **绘制与布局分离**：先计算控件需要多大、应该放在哪里，再生成绘制元素。

### 1.2 Slate、UMG 和 UObject 的关系

UMG 是面向设计师和蓝图的上层 UI 系统，Slate 是更底层的实际渲染与交互框架。

```text
UUserWidget / UWidget（UMG、UObject、蓝图友好）
                    ↓
          RebuildWidget / TakeWidget
                    ↓
           SWidget（Slate、C++ UI）
                    ↓
          布局、绘制、输入、窗口系统
```

简单选择原则：

- 游戏 HUD、背包、主菜单，并且需要设计师在编辑器里搭建：优先 UMG。
- 编辑器插件、工具面板、属性定制、资源编辑器：通常直接使用 Slate。
- UMG 无法满足的底层自定义绘制或复杂控件：可以写 Slate 控件，再用 `UWidget` 包装给 UMG。

### 1.3 最重要的心智模型：一棵控件树

一个 Slate 界面不是一张画布，而是一棵树。例如：

```text
SBorder
└─ SVerticalBox
   ├─ STextBlock
   ├─ SButton
   │  └─ STextBlock
   └─ SHorizontalBox
      ├─ SCheckBox
      └─ STextBlock
```

树中的节点大致分为两类：

- **叶子控件**：显示文字、图片或自定义图形，例如 `STextBlock`、`SImage`。
- **容器控件（Panel）**：拥有并排列子控件，例如 `SVerticalBox`、`SHorizontalBox`、`SGridPanel`、`SOverlay`。

父控件决定子控件的可用空间，子控件报告自己期望的尺寸。布局结果会沿树逐层计算。

### 1.4 Slate 控件的基本类型

所有 Slate 控件最终都继承自 `SWidget`。常见基类如下：

| 类型 | 用途 | 子控件数量 |
| --- | --- | --- |
| `SLeafWidget` | 自己绘制内容的叶子控件 | 0 |
| `SCompoundWidget` | 最常用的自定义组合控件 | 1 个直接子控件 |
| `SPanel` | 自定义布局容器 | 多个 |
| `SBoxPanel` | 横向或纵向排列的容器基础 | 多个 |

`SCompoundWidget` 虽然只有一个直接子控件，但这个子控件可以是 `SVerticalBox` 等容器，所以实际界面可以很复杂。

### 1.5 声明式语法在做什么

典型 Slate 代码如下：

```cpp
ChildSlot
[
    SNew(SBorder)
    .Padding(12.0f)
    [
        SNew(SVerticalBox)

        + SVerticalBox::Slot()
        .AutoHeight()
        [
            SNew(STextBlock)
            .Text(FText::FromString(TEXT("Hello Slate")))
        ]

        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(0.0f, 8.0f)
        [
            SNew(SButton)
            .Text(FText::FromString(TEXT("Click Me")))
            .OnClicked(this, &SMyPanel::HandleButtonClicked)
        ]
    ]
];
```

这段语法看起来像特殊语言，本质上仍是 C++：

- `SNew(SButton)`：创建 `SButton`，结果通常是 `TSharedRef<SButton>`。
- `.Text(...)`、`.Padding(...)`：给控件的构造参数赋值。
- `[ ChildWidget ]`：设置控件的子节点。
- `+ SVerticalBox::Slot()`：向多子节点容器添加一个插槽。
- `.AutoHeight()`：设置的是插槽布局规则，不是子控件本身。
- `ChildSlot`：`SCompoundWidget` 唯一的直接子插槽。

这里要区分 **Widget** 和 **Slot**：

- Widget 描述“它是什么”，例如按钮、文字、图片。
- Slot 描述“它在父容器里怎么摆”，例如自动高度、填充比例、内边距和对齐。

同一个 `SButton` 放进不同容器时，会使用不同类型的 Slot。

### 1.6 控件是如何构造的

Slate 控件通常不把初始化逻辑写在 C++ 构造函数中，而是使用 `Construct`：

```cpp
class SMyPanel : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SMyPanel)
        : _Title(FText::FromString(TEXT("Default Title")))
    {}

        SLATE_ARGUMENT(FText, Title)
        SLATE_EVENT(FSimpleDelegate, OnConfirmed)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs)
    {
        OnConfirmed = InArgs._OnConfirmed;

        ChildSlot
        [
            SNew(SButton)
            .Text(InArgs._Title)
            .OnClicked(this, &SMyPanel::HandleClicked)
        ];
    }

private:
    FReply HandleClicked()
    {
        OnConfirmed.ExecuteIfBound();
        return FReply::Handled();
    }

    FSimpleDelegate OnConfirmed;
};
```

使用时：

```cpp
SNew(SMyPanel)
.Title(FText::FromString(TEXT("Confirm")))
.OnConfirmed_Lambda([]()
{
    UE_LOG(LogTemp, Log, TEXT("Confirmed"));
});
```

常用参数宏：

- `SLATE_ARGUMENT(Type, Name)`：传入一个普通构造值。
- `SLATE_ATTRIBUTE(Type, Name)`：传入固定值或动态绑定。
- `SLATE_EVENT(DelegateType, Name)`：传入事件委托。
- `SLATE_DEFAULT_SLOT(...)` / `SLATE_NAMED_SLOT(...)`：允许调用方传入子控件。

### 1.7 所有权与指针

Slate 控件不由 Unreal GC 管理，常见指针类型是：

- `TSharedRef<SWidget>`：一定有效，不能为 null；创建和传递控件时最常见。
- `TSharedPtr<SWidget>`：可以为空；需要在以后访问或可选持有时使用。
- `TWeakPtr<SWidget>`：不增加引用计数，避免循环引用或用于观察控件。

常见写法：

```cpp
TSharedPtr<STextBlock> StatusText;

SAssignNew(StatusText, STextBlock)
.Text(FText::FromString(TEXT("Ready")));
```

`SAssignNew` 与 `SNew` 类似，但会同时把新控件保存到指定的 `TSharedPtr`。

实践原则：如果只在控件树里使用一个子控件，就不必额外保存指针；只有后续确实需要直接调用它时才保存。

### 1.8 属性绑定：界面怎样读取数据

Slate 属性既可以是固定值，也可以动态绑定：

```cpp
// 固定值
SNew(STextBlock)
.Text(FText::FromString(TEXT("Ready")))
```

```cpp
// 绑定成员函数
SNew(STextBlock)
.Text(this, &SMyPanel::GetStatusText)
```

```cpp
// 绑定 Lambda
SNew(STextBlock)
.Text_Lambda([this]()
{
    return bIsRunning
        ? FText::FromString(TEXT("Running"))
        : FText::FromString(TEXT("Stopped"));
})
```

初学时可以把 Attribute 理解为一个“取值方法”：Slate 需要该值时，既可能直接拿到常量，也可能调用绑定函数。

但不要把所有属性都无脑绑定成 Lambda。大量绑定可能增加每帧或每次失效后的求值成本，也容易产生生命周期问题。数据不经常变化时，保存控件指针并在事件发生时主动更新，通常更直观。

### 1.9 一帧中的核心流程

Slate 最重要的运行流程可以分成四部分：

```text
数据或输入发生变化
        ↓
属性求值 / 控件失效（Invalidate）
        ↓
布局：期望尺寸 → 分配几何区域
        ↓
绘制：生成 Draw Elements
        ↓
Slate Renderer 批处理并提交给渲染系统
```

#### 第一步：预布局与期望尺寸

控件通过 `ComputeDesiredSize` 表达自己理想的尺寸。父容器收集子控件的期望尺寸，计算自身需要的空间。

例如：

- `STextBlock` 的期望尺寸与字体和文本有关。
- `SVerticalBox` 的期望高度通常是可见子项高度之和。
- `SBox` 可以用 `WidthOverride`、`HeightOverride` 等规则影响期望尺寸。

#### 第二步：排列子控件

父容器在得到实际可用空间后，为每个子控件分配位置和尺寸。最终结果通过 `FGeometry` 表示。

因此：

- **Desired Size** 是控件“想要多大”。
- **Allotted Geometry** 是父控件最后“实际给了多大、放在哪里”。

#### 第三步：绘制

控件的 `OnPaint` 不会立即直接画到屏幕上，而是向绘制列表添加元素，例如矩形、图片、文字和线条。Slate Renderer 随后将这些元素排序、批处理并提交渲染。

`LayerId` 用于控制前后覆盖关系。自定义绘制时，子内容和额外装饰必须正确递增层级。

#### 第四步：失效与缓存

当文本、可见性、尺寸、样式或绘制内容变化时，Slate 会使相应缓存失效。失效类型可能影响布局、绘制或子控件顺序。

性能优化的核心不是“永远不更新”，而是让 Slate 明确知道：

- 哪些内容没变，可以复用。
- 哪些内容只需要重新绘制。
- 哪些变化会影响尺寸，必须重新布局。

这也是 `SInvalidationPanel`、全局失效机制以及属性系统存在的原因。

### 1.10 输入是怎样传递的

Slate 接收鼠标、键盘、触摸和导航事件，并根据命中测试结果、控件层级和焦点状态路由事件。

常见事件函数：

- `OnMouseButtonDown`
- `OnMouseButtonUp`
- `OnMouseMove`
- `OnMouseWheel`
- `OnKeyDown`
- `OnDragDetected`
- `OnDrop`

事件通常返回 `FReply`：

```cpp
FReply SMyWidget::OnMouseButtonDown(
    const FGeometry& MyGeometry,
    const FPointerEvent& MouseEvent)
{
    if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
    {
        return FReply::Handled().SetKeyboardFocus(SharedThis(this));
    }

    return FReply::Unhandled();
}
```

- `FReply::Handled()`：我处理了，通常停止继续寻找处理者。
- `FReply::Unhandled()`：我没处理，让路由系统继续尝试。

一个常见错误是无条件返回 `Handled`，这会吞掉父控件、快捷键系统或其他控件原本应该收到的输入。

输入相关的三个关键概念：

- **Hit Test**：指针位置命中了哪个可交互控件。
- **Focus**：哪个控件接收键盘输入。
- **Capture**：拖动等连续操作期间，哪个控件持续接收指针事件。

### 1.11 可见性不只是“显示或隐藏”

`EVisibility` 同时影响绘制、布局和命中测试：

| 值 | 绘制 | 占据布局 | 自身命中测试 | 子控件命中测试 |
| --- | --- | --- | --- | --- |
| `Visible` | 是 | 是 | 是 | 是 |
| `Collapsed` | 否 | 否 | 否 | 否 |
| `Hidden` | 否 | 是 | 否 | 否 |
| `HitTestInvisible` | 是 | 是 | 否 | 否 |
| `SelfHitTestInvisible` | 是 | 是 | 否 | 是 |

制作纯视觉背景或覆盖层时，`SelfHitTestInvisible` 很常用：它自己不挡鼠标，但内部按钮仍可交互。

### 1.12 Tick 与生命周期

Slate 控件一般经历：

```text
创建共享对象
  → Construct
  → 加入控件树
  → 布局 / 绘制 / 输入 / 可选 Tick
  → 从控件树移除
  → 最后一个共享引用释放
  → 析构
```

注意：

- Slate 控件不是 `UObject`，不要依赖 `BeginPlay`、`EndPlay` 或 GC。
- `Tick` 并不是所有界面逻辑的首选。能通过事件更新的内容就使用事件。
- 委托或 Lambda 捕获外部对象时，要确保对象生命周期足够长；捕获 `UObject` 时尤其要考虑弱引用和失效检查。
- 控件被窗口移除后，如果其他 `TSharedPtr` 仍然持有它，它不会立即析构。

### 1.13 样式与内容的分离

Slate 样式一般由 `FSlateStyleSet` 管理，并通过名字查找 Brush、颜色、字体或完整 Widget Style。

```text
业务控件
  └─ 请求语义样式名，例如 "Theme.Panel.Background"
       └─ FSlateStyleSet
            └─ FSlateBrush / FTextBlockStyle / FButtonStyle / Color / Font
```

一个可维护的主题系统不应该让控件到处硬编码颜色。更好的做法是使用语义名称：

- `Theme.Background`
- `Theme.Panel`
- `Theme.Text.Primary`
- `Theme.Text.Secondary`
- `Theme.Accent`
- `Theme.Border`

这样“日间主题”和“夜间主题”只需要给相同语义键提供不同资源，控件结构本身无需改变。

### 1.14 编辑器插件里使用 Slate 的模块关系

插件模块的 `Build.cs` 通常至少需要：

```csharp
PrivateDependencyModuleNames.AddRange(new[]
{
    "Core",
    "CoreUObject",
    "Engine",
    "Slate",
    "SlateCore"
});
```

常见职责：

- `SlateCore`：底层控件、样式、几何、绘制、输入类型。
- `Slate`：常用控件与应用层功能。
- `ToolMenus`：编辑器菜单和工具栏扩展。
- `UnrealEd`、`LevelEditor`：编辑器专用能力；只应放在 Editor 模块中。

插件模块启动时一般注册样式、命令、菜单和 Tab；关闭时按相反顺序注销，避免热重载或关闭编辑器时留下无效注册项。

### 1.15 一个最小可用控件

下面的例子把参数、布局、事件和状态更新串在一起：

```cpp
class SCounterPanel : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCounterPanel) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs)
    {
        ChildSlot
        [
            SNew(SHorizontalBox)

            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            [
                SAssignNew(CountText, STextBlock)
                .Text(GetCountText())
            ]

            + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(8.0f, 0.0f)
            [
                SNew(SButton)
                .Text(FText::FromString(TEXT("+1")))
                .OnClicked(this, &SCounterPanel::Increment)
            ]
        ];
    }

private:
    FReply Increment()
    {
        ++Count;

        if (CountText.IsValid())
        {
            CountText->SetText(GetCountText());
        }

        return FReply::Handled();
    }

    FText GetCountText() const
    {
        return FText::AsNumber(Count);
    }

    int32 Count = 0;
    TSharedPtr<STextBlock> CountText;
};
```

这个例子体现了 Slate 的基本闭环：

1. `Construct` 创建控件树。
2. `SHorizontalBox` 负责排列子控件。
3. 按钮将点击事件交给 `Increment`。
4. 业务状态 `Count` 改变。
5. `STextBlock::SetText` 更新展示并触发必要的失效。
6. Slate 在后续布局/绘制流程中显示新内容。

### 1.16 初学时最容易混淆的地方

1. **把 Slate 当成 UObject 系统**  
   Slate 控件主要使用共享指针，不使用 `UPROPERTY` 管理，也不参与 GC。

2. **分不清 Widget 和 Slot**  
   Widget 是内容，Slot 是父容器赋予它的布局规则。

3. **认为声明式代码每帧都会重新创建控件**  
   `Construct` 通常只在创建控件时执行一次；之后是已有控件在布局和绘制。

4. **滥用属性绑定或 Tick**  
   高频绑定和 Tick 会让数据流难以追踪。优先使用明确的事件更新。

5. **保存所有子控件指针**  
   控件树已经持有子控件。只有确实需要后续访问时才使用 `SAssignNew`。

6. **硬编码视觉值**  
   颜色、字体、Brush 和 Widget Style 应逐步集中到样式系统，尤其是需要主题切换时。

7. **忘记注销编辑器注册项**  
   Tab、菜单、命令、样式等注册行为都应该在模块关闭时正确清理。

### 1.17 本章总结

只要先记住下面五句话，后续学习就有了主线：

1. Slate UI 是一棵长期存在的 `SWidget` 控件树。
2. 容器通过 Slot 决定子控件的排列方式。
3. Slate 先计算期望尺寸和实际几何，再生成绘制元素。
4. 输入依靠命中测试、焦点、捕获和 `FReply` 在控件树中路由。
5. 数据变化后，通过属性绑定或主动 Setter 使界面失效并更新。

后面无论学习按钮、列表、主题、Tab、自定义绘制还是性能优化，本质上都是在这套流程的某一个环节上继续深入。

---

## 2. 常用控件、容器与布局

### 2.1 本章目标

学完这一章，应当能够只看需求草图就选出合适的 Slate 控件，并能解释每个尺寸究竟由 Widget 还是 Slot 决定。

先记住布局的基本对话：

```text
子控件：这是我的 Desired Size（期望尺寸）
父容器：这是我最终分给你的 Allotted Geometry（实际区域）
Slot：这是你在父容器中的排列规则
```

### 2.2 常用内容控件

| 控件 | 典型用途 | 常用属性或事件 |
| --- | --- | --- |
| `STextBlock` | 显示只读文本 | `Text`、`Font`、`ColorAndOpacity`、`AutoWrapText` |
| `SImage` | 显示 Brush、图标或纯色块 | `Image`、`ColorAndOpacity` |
| `SButton` | 点击操作 | `Text`、`ContentPadding`、`OnClicked`、`IsEnabled` |
| `SCheckBox` | 复选框、单选按钮、开关 | `IsChecked`、`OnCheckStateChanged`、`Style` |
| `SEditableTextBox` | 单行文本输入 | `Text`、`HintText`、`OnTextChanged`、`OnTextCommitted` |
| `SMultiLineEditableTextBox` | 多行文本输入 | 文本编辑、换行、滚动 |
| `SSpinBox<T>` | 数字输入与拖拽调整 | `Value`、`MinValue`、`MaxValue`、`OnValueChanged` |
| `SProgressBar` | 进度展示 | `Percent`、`FillColorAndOpacity` |
| `SSlider` | 连续值调整 | `Value`、`OnValueChanged` |
| `SComboBox<T>` | 下拉选项 | `OptionsSource`、`OnGenerateWidget`、`OnSelectionChanged` |

事件名通常能提示数据流：

- `OnTextChanged`：用户每次修改文本都会触发，适合即时预览。
- `OnTextCommitted`：用户回车或输入框失去焦点时触发，适合真正保存。
- `OnValueChanged`：数值拖动过程中持续触发。
- `OnValueCommitted`：一次编辑动作完成后触发。

搜索框如果每次按键都执行昂贵查询，应当增加防抖或把真正查询放到提交事件中。

### 2.3 最常用的容器

| 容器 | 布局逻辑 | 适用场景 |
| --- | --- | --- |
| `SVerticalBox` | 从上到下排列 | 表单、设置页、工具面板 |
| `SHorizontalBox` | 从左到右排列 | 工具栏、单行字段、按钮组 |
| `SOverlay` | 子控件叠放 | 背景 + 内容 + 角标/遮罩 |
| `SGridPanel` | 行列网格 | 标签与输入框对齐 |
| `SUniformGridPanel` | 等尺寸网格 | 等宽按钮、图标选择器 |
| `SScrollBox` | 单方向滚动 | 长设置页、日志、列表外壳 |
| `SSplitter` | 用户可拖动分区比例 | 左右详情面板、上下预览区 |
| `SWidgetSwitcher` | 同时只显示一个子页 | 标签页内容、状态页面 |
| `SBox` | 限制一个子控件的尺寸 | 固定宽高、最小/最大尺寸 |
| `SBorder` | 背景、边框、Padding | 卡片、分区、面板底色 |

不要因为名字里有 `Box` 就认为它们相同：

- `SVerticalBox` / `SHorizontalBox` 是多子项排列容器。
- `SBox` 是单子项尺寸约束容器。
- `SBorder` 是带 Brush 与 Padding 的单子项装饰容器。

### 2.4 Auto、Fill 与对齐

在 `SVerticalBox` 和 `SHorizontalBox` 中，最常用的尺寸规则是：

- `AutoHeight()` / `AutoWidth()`：沿排列方向使用子控件的期望尺寸。
- `FillHeight(Value)` / `FillWidth(Value)`：瓜分剩余空间，`Value` 是权重。

例如一个工具栏：标题占满剩余宽度，按钮保持自身宽度。

```cpp
SNew(SHorizontalBox)

+ SHorizontalBox::Slot()
.FillWidth(1.0f)
.VAlign(VAlign_Center)
[
    SNew(STextBlock)
    .Text(FText::FromString(TEXT("Theme Editor")))
]

+ SHorizontalBox::Slot()
.AutoWidth()
.Padding(8.0f, 0.0f)
[
    SNew(SButton)
    .Text(FText::FromString(TEXT("Apply")))
]
```

如果两个 Slot 分别是 `FillWidth(1.0f)` 和 `FillWidth(2.0f)`，它们会按 1:2 瓜分剩余宽度，而不是得到固定的 1 或 2 像素。

对齐只在父容器分配的空间大于子控件实际使用空间时才明显：

- 水平：`HAlign_Left`、`HAlign_Center`、`HAlign_Right`、`HAlign_Fill`
- 垂直：`VAlign_Top`、`VAlign_Center`、`VAlign_Bottom`、`VAlign_Fill`

### 2.5 Padding、Margin 和 ContentPadding

Slate 中最常看到的是 `FMargin`：

```cpp
FMargin(8.0f)                    // 四边都是 8
FMargin(8.0f, 4.0f)              // 水平 8，垂直 4
FMargin(8.0f, 4.0f, 12.0f, 6.0f) // 左、上、右、下
```

位置不同，语义也不同：

- Slot 的 `.Padding(...)`：当前子控件与 Slot 边界之间的空白。
- `SBorder::Padding(...)`：边框与内部内容之间的空白。
- `SButton::ContentPadding(...)`：按钮视觉边缘与按钮内容之间的空白。

当界面出现“间距叠加”时，应沿控件树逐层检查，而不是继续增加负边距抵消。

### 2.6 用 SBox 控制尺寸

```cpp
SNew(SBox)
.MinDesiredWidth(240.0f)
.MaxDesiredWidth(480.0f)
.HeightOverride(32.0f)
[
    SNew(SEditableTextBox)
    .HintText(FText::FromString(TEXT("Theme name")))
]
```

尺寸约束是工具，不应成为默认做法。大量硬编码宽高会导致：

- 本地化后的长文本被截断。
- 高 DPI 和字体缩放下比例失衡。
- 面板缩放时无法自适应。

优先让 Desired Size、Fill 和最小尺寸共同工作，只在真正固定的图标、行高或预览区域上使用 Override。

### 2.7 列表控件的基本模式

数据量较大时，不要在 `SScrollBox` 中手动创建几百个子控件，应使用虚拟化列表，例如 `SListView<T>`、`STreeView<T>` 或 `STileView<T>`。

```cpp
using FThemeItem = TSharedPtr<FString>;

TArray<FThemeItem> ThemeItems;
TSharedPtr<SListView<FThemeItem>> ThemeList;

SAssignNew(ThemeList, SListView<FThemeItem>)
.ListItemsSource(&ThemeItems)
.OnGenerateRow(this, &SThemePanel::GenerateThemeRow)
.OnSelectionChanged(this, &SThemePanel::HandleThemeSelected);
```

```cpp
TSharedRef<ITableRow> SThemePanel::GenerateThemeRow(
    FThemeItem Item,
    const TSharedRef<STableViewBase>& OwnerTable)
{
    return SNew(STableRow<FThemeItem>, OwnerTable)
    [
        SNew(STextBlock)
        .Text(FText::FromString(Item.IsValid() ? *Item : TEXT("Invalid")))
    ];
}
```

关键点：

- `ListItemsSource` 指向的数据数组必须比列表活得更久。
- 数组内容变化后调用 `RequestListRefresh()`。
- 行控件由 `OnGenerateRow` 按需创建和复用，不要假设每个数据项永远对应同一个 Widget。

### 2.8 一个设置卡片示例

```cpp
SNew(SBorder)
.Padding(12.0f)
[
    SNew(SVerticalBox)

    + SVerticalBox::Slot()
    .AutoHeight()
    [
        SNew(STextBlock)
        .Text(FText::FromString(TEXT("Appearance")))
    ]

    + SVerticalBox::Slot()
    .AutoHeight()
    .Padding(0.0f, 10.0f, 0.0f, 0.0f)
    [
        SNew(SGridPanel)
        .FillColumn(1, 1.0f)

        + SGridPanel::Slot(0, 0)
        .VAlign(VAlign_Center)
        [
            SNew(STextBlock)
            .Text(FText::FromString(TEXT("Dark Mode")))
        ]

        + SGridPanel::Slot(1, 0)
        .HAlign(HAlign_Right)
        [
            SNew(SCheckBox)
            .IsChecked(this, &SThemePanel::GetDarkModeCheckState)
            .OnCheckStateChanged(this, &SThemePanel::SetDarkMode)
        ]
    ]
]
```

拆解这段代码：`SBorder` 提供卡片外观，`SVerticalBox` 组织标题和内容，`SGridPanel` 保证标签列与控件列对齐，第二列 Fill，开关在该列右对齐。

### 2.9 本章练习

1. 创建一个“主题设置卡片”，包含主题名、主色、字体缩放和应用按钮。
2. 把三个主题放进 `SListView`，选中时更新右侧详情区。
3. 分别用 `Hidden` 与 `Collapsed` 隐藏一行，观察其他行位置的差异。
4. 将固定宽度改为 `FillWidth` 与 `MinDesiredWidth`，测试窄窗口和宽窗口。

---

## 3. 样式系统与主题切换

### 3.1 为什么要有样式系统

如果每个控件都直接写 `FLinearColor`、字体和 Brush，界面很快会出现大量重复值。主题切换时还要遍历所有控件逐个修改。

更合理的依赖方向是：

```text
业务控件 → 语义样式键 → 当前主题的 StyleSet → Brush / Color / Font / WidgetStyle
```

控件只知道“这是主文本”，不知道它在夜间主题中具体是哪种灰色。

### 3.2 FSlateBrush 是什么

`FSlateBrush` 描述“如何画一块视觉元素”，其中可以包含：

- 图片资源或动态资源名
- 绘制类型
- 边距（九宫格拉伸）
- 图像尺寸
- Tint
- Tiling

常见 Brush 类型：

- `FSlateImageBrush`：普通图片。
- `FSlateBoxBrush`：九宫格拉伸，适合面板和按钮。
- `FSlateBorderBrush`：绘制边框。
- `FSlateColorBrush`：纯色矩形。
- `FSlateRoundedBoxBrush`：圆角矩形，可配置边框。
- `FSlateNoResource`：不绘制资源。

Brush 经常通过指针传给 `SImage` 或 `SBorder`，所以 Brush 的所有者必须比使用它的控件活得更久。把局部变量地址传给 `.Image(&LocalBrush)` 是典型生命周期错误。

### 3.3 FSlateStyleSet 与样式注册

`FSlateStyleSet` 是“名字到样式资源”的映射。一个插件通常拥有自己的 StyleSet：

```cpp
class FSlateThemeStyle
{
public:
    static void Initialize();
    static void Shutdown();
    static const ISlateStyle& Get();

private:
    static TSharedPtr<FSlateStyleSet> StyleInstance;
};
```

```cpp
void FSlateThemeStyle::Initialize()
{
    if (StyleInstance.IsValid())
    {
        return;
    }

    StyleInstance = MakeShared<FSlateStyleSet>(TEXT("SlateThemeSwitcherStyle"));
    StyleInstance->Set(
        TEXT("Theme.Background"),
        new FSlateColorBrush(FLinearColor(0.03f, 0.04f, 0.06f, 1.0f)));
    StyleInstance->Set(
        TEXT("Theme.Text.Primary"),
        FSlateColor(FLinearColor(0.92f, 0.94f, 0.98f, 1.0f)));

    FSlateStyleRegistry::RegisterSlateStyle(*StyleInstance);
}

void FSlateThemeStyle::Shutdown()
{
    if (StyleInstance.IsValid())
    {
        FSlateStyleRegistry::UnRegisterSlateStyle(*StyleInstance);
        ensure(StyleInstance.IsUnique());
        StyleInstance.Reset();
    }
}
```

`FSlateStyleSet::Set` 会接管通过 `new` 传入的 Brush；不要再手动删除这些 Brush。

### 3.4 使用样式资源

```cpp
SNew(SBorder)
.BorderImage(FSlateThemeStyle::Get().GetBrush(TEXT("Theme.Background")))
[
    SNew(STextBlock)
    .Text(FText::FromString(TEXT("Theme Preview")))
    .ColorAndOpacity(
        FSlateThemeStyle::Get().GetSlateColor(TEXT("Theme.Text.Primary")))
]
```

常用读取函数包括：

- `GetBrush(Name)`
- `GetColor(Name)` / `GetSlateColor(Name)`
- `GetFontStyle(Name)`
- `GetWidgetStyle<FButtonStyle>(Name)`

如果样式名来自多个模块，使用统一命名约定可以避免碰撞：

```text
SlateThemeSwitcher.Panel.Background
SlateThemeSwitcher.Text.Primary
SlateThemeSwitcher.Button.Primary
SlateThemeSwitcher.Icon.Refresh
```

### 3.5 Widget Style

按钮并不是一个颜色，而是一组状态：Normal、Hovered、Pressed、Disabled，再加上 Padding、文字颜色等。因此常用 `FButtonStyle` 表达完整按钮样式。

```cpp
FButtonStyle PrimaryButton = FAppStyle::Get().GetWidgetStyle<FButtonStyle>(
    TEXT("Button"));

PrimaryButton
    .SetNormal(FSlateRoundedBoxBrush(NormalColor, 4.0f))
    .SetHovered(FSlateRoundedBoxBrush(HoveredColor, 4.0f))
    .SetPressed(FSlateRoundedBoxBrush(PressedColor, 4.0f));

StyleInstance->Set(TEXT("SlateThemeSwitcher.Button.Primary"), PrimaryButton);
```

复制 `FAppStyle` 的基础样式再定制，通常比从零填写所有状态安全。自己的插件只读取 `FAppStyle`，不要修改编辑器的全局 `FAppStyle`。

### 3.6 语义色板

推荐先定义少量语义，再映射到主题颜色：

| 语义 | 日间主题 | 夜间主题 |
| --- | --- | --- |
| `Background` | 浅灰白 | 深蓝黑 |
| `Panel` | 白色 | 深灰蓝 |
| `Text.Primary` | 接近黑色 | 接近白色 |
| `Text.Secondary` | 中灰 | 浅灰 |
| `Accent` | 高饱和蓝 | 明亮青蓝 |
| `Border` | 浅灰 | 深灰 |
| `Danger` | 深红 | 明亮红 |

语义键应该描述用途，不应该叫 `DarkGray300`。用途稳定，实际色值才可以自由切换。

### 3.7 主题注册表的设计

一个可扩展主题系统可以分成三层：

```text
FThemeDefinition
  └─ 主题 id、显示名、色板、Brush 和 WidgetStyle

FThemeRegistry
  └─ 注册、注销、查找、枚举主题

FThemeManager
  └─ 当前主题、切换主题、广播 OnThemeChanged
```

控件不应该写死“如果夜间模式就用黑色，否则用白色”。它只读取当前主题的语义资源，并监听主题变化。

建议的最小接口：

```cpp
class FThemeRegistry
{
public:
    bool RegisterTheme(TSharedRef<const FThemeDefinition> Theme);
    void UnregisterTheme(FName ThemeId);
    TSharedPtr<const FThemeDefinition> FindTheme(FName ThemeId) const;
    void GetThemeIds(TArray<FName>& OutThemeIds) const;
};
```

### 3.8 切换主题后为什么可能不刷新

常见原因有三种：

1. 控件在 `Construct` 中取得了一次颜色常量，之后再也没有更新。
2. StyleSet 被整体替换，但控件仍持有旧 Widget Style 或 Brush 指针。
3. 主题数据改变了，但没有通知 Slate 哪一部分需要失效。

可选解决策略：

- 颜色等轻量属性使用 Attribute 读取当前主题。
- 在 `OnThemeChanged` 中调用控件 Setter 主动更新。
- 必要时重建主题预览子树。
- 保持 StyleSet/Brush 地址稳定，只修改其数据并请求适当失效。

主题切换不是简单替换一个全局变量；它还包含资源生命周期和 UI 失效策略。

### 3.9 样式生命周期顺序

推荐模块生命周期：

```text
StartupModule
  1. 创建并注册 StyleSet
  2. 注册命令
  3. 注册菜单和 Tab

ShutdownModule
  1. 注销菜单和 Tab
  2. 注销命令
  3. 注销并销毁 StyleSet
```

先注册样式，后创建控件；先销毁控件入口，最后注销样式。这样可以减少控件访问已释放 Brush 的风险。

### 3.10 本章练习

1. 为背景、面板、主文本、次文本、强调色和边框各定义一个语义键。
2. 分别创建 Day 与 Night 两套色板，但保持语义键完全一致。
3. 做一个预览卡片，同时显示普通、悬停、按下和禁用按钮状态。
4. 切换主题后检查现有控件是否立即刷新，并说明它使用的是绑定、Setter 还是重建策略。

---

## 4. 编辑器插件、菜单与 Nomad Tab

### 4.1 一个编辑器 Slate 插件包含什么

最小目录结构通常是：

```text
SlateThemeSwitcher/
├─ SlateThemeSwitcher.uplugin
├─ Resources/
│  └─ Icon128.png
├─ Source/
│  └─ SlateThemeSwitcher/
│     ├─ SlateThemeSwitcher.Build.cs
│     ├─ Public/
│     │  └─ SlateThemeSwitcherModule.h
│     └─ Private/
│        ├─ SlateThemeSwitcherModule.cpp
│        ├─ SThemeSwitcherPanel.h
│        └─ SThemeSwitcherPanel.cpp
└─ Docs/
   └─ SlateLearningNotes.md
```

学习文档不会参与编译；真正的插件发现依赖 `.uplugin`。

### 4.2 uplugin 的关键字段

```json
{
  "FileVersion": 3,
  "Version": 1,
  "VersionName": "1.0.0",
  "FriendlyName": "Slate Theme Switcher",
  "Category": "Editor",
  "CanContainContent": false,
  "Modules": [
    {
      "Name": "SlateThemeSwitcher",
      "Type": "Editor",
      "LoadingPhase": "Default"
    }
  ]
}
```

这是编辑器工具，所以模块类型应为 `Editor`。如果写成 `Runtime`，就必须避免依赖 `UnrealEd`、`LevelEditor` 等仅编辑器模块。

### 4.3 Build.cs 依赖

```csharp
public class SlateThemeSwitcher : ModuleRules
{
    public SlateThemeSwitcher(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new[]
        {
            "Core",
            "SlateCore"
        });

        PrivateDependencyModuleNames.AddRange(new[]
        {
            "CoreUObject",
            "Engine",
            "Slate",
            "ToolMenus",
            "UnrealEd"
        });
    }
}
```

只有公共头文件暴露的类型才需要放进 `PublicDependencyModuleNames`。依赖全放 Public 会增加模块耦合和编译成本。

### 4.4 模块启动与关闭

```cpp
class FSlateThemeSwitcherModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

private:
    void RegisterMenus();
    void OpenThemeTab();
    TSharedRef<SDockTab> SpawnThemeTab(const FSpawnTabArgs& Args);

    static const FName ThemeTabName;
};
```

```cpp
const FName FSlateThemeSwitcherModule::ThemeTabName(
    TEXT("SlateThemeSwitcher.MainTab"));

void FSlateThemeSwitcherModule::StartupModule()
{
    FSlateThemeStyle::Initialize();

    FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
        ThemeTabName,
        FOnSpawnTab::CreateRaw(this, &FSlateThemeSwitcherModule::SpawnThemeTab))
        .SetDisplayName(FText::FromString(TEXT("Slate Theme Switcher")))
        .SetMenuType(ETabSpawnerMenuType::Hidden);

    UToolMenus::RegisterStartupCallback(
        FSimpleMulticastDelegate::FDelegate::CreateRaw(
            this,
            &FSlateThemeSwitcherModule::RegisterMenus));
}
```

```cpp
void FSlateThemeSwitcherModule::ShutdownModule()
{
    UToolMenus::UnRegisterStartupCallback(this);
    UToolMenus::UnregisterOwner(this);
    FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(ThemeTabName);

    FSlateThemeStyle::Shutdown();
}
```

编辑器关闭阶段有些模块可能已经卸载。实际项目中还应根据使用的系统检查模块可用性，避免 Shutdown 顺序问题。

### 4.5 注册 Window 菜单入口

```cpp
void FSlateThemeSwitcherModule::RegisterMenus()
{
    FToolMenuOwnerScoped OwnerScoped(this);

    UToolMenu* Menu = UToolMenus::Get()->ExtendMenu(
        TEXT("LevelEditor.MainMenu.Window"));

    FToolMenuSection& Section = Menu->FindOrAddSection(TEXT("WindowLayout"));
    Section.AddMenuEntry(
        TEXT("OpenSlateThemeSwitcher"),
        FText::FromString(TEXT("Slate Theme Switcher")),
        FText::FromString(TEXT("Open the Slate theme preview and switcher.")),
        FSlateIcon(),
        FUIAction(FExecuteAction::CreateRaw(
            this,
            &FSlateThemeSwitcherModule::OpenThemeTab)));
}

void FSlateThemeSwitcherModule::OpenThemeTab()
{
    FGlobalTabmanager::Get()->TryInvokeTab(ThemeTabName);
}
```

不同 UE 版本或编辑器区域的菜单路径可能不同，可使用 ToolMenus 的调试功能或查看引擎现有注册代码确认准确路径。

### 4.6 创建 Nomad Tab

```cpp
TSharedRef<SDockTab> FSlateThemeSwitcherModule::SpawnThemeTab(
    const FSpawnTabArgs& Args)
{
    return SNew(SDockTab)
        .TabRole(ETabRole::NomadTab)
        [
            SNew(SThemeSwitcherPanel)
        ];
}
```

Nomad Tab 是可停靠、可独立浮动的编辑器工具页，适合插件主面板。`TryInvokeTab` 会在 Tab 不存在时调用 Spawner；已存在时通常激活现有 Tab。

### 4.7 Commands 与直接 FUIAction 的区别

只有一个简单菜单入口时，直接使用 `FUIAction` 就足够。需要以下能力时，更适合定义 `TCommands`：

- 快捷键绑定
- 菜单和工具栏复用同一动作
- Checked、Enabled、Visible 状态统一管理
- 在多个编辑器区域映射同一命令

典型结构：

```text
TCommands 定义动作元数据
       ↓
FUICommandList 映射 Execute / CanExecute / IsChecked
       ↓
菜单、工具栏、快捷键引用同一 CommandInfo
```

### 4.8 注册与注销必须成对

| Startup 注册 | Shutdown 注销 |
| --- | --- |
| `RegisterSlateStyle` | `UnRegisterSlateStyle` |
| `RegisterNomadTabSpawner` | `UnregisterNomadTabSpawner` |
| `RegisterStartupCallback` | `UnRegisterStartupCallback` |
| ToolMenus Owner | `UnregisterOwner` |
| `TCommands::Register` | `TCommands::Unregister` |
| Delegate `AddRaw/AddSP` | 保存 Handle 并 `Remove`，或确保所有者关系安全 |

热重载和动态启停插件最容易暴露注册残留问题。每次注册时都问自己：它的注销写在哪里？

### 4.9 本章练习

1. 写出一个 Editor 类型 `.uplugin` 和对应的 `Build.cs`。
2. 在 Window 菜单添加入口，点击后打开 Nomad Tab。
3. Tab 中先只放标题、说明文本和关闭按钮。
4. 连续关闭并重开 Tab，确认不会生成重复菜单项。
5. 禁用再启用插件或重新启动编辑器，确认没有样式和 Tab 注册冲突。

---

## 5. 自定义控件与绘制

### 5.1 先判断是否真的需要自定义绘制

实现一个新视觉控件时，按下面顺序选择：

1. 现有控件加样式能否完成？
2. 用 `SCompoundWidget` 组合现有控件能否完成？
3. 是否只差一层额外装饰，可以覆写组合控件的绘制？
4. 最后才考虑 `SLeafWidget`、自定义 `SPanel` 或完整自定义绘制。

组合控件能自动获得布局、无障碍、焦点、输入状态和成熟样式行为。自定义绘制自由度更高，但这些能力需要自己负责。

### 5.2 SLeafWidget 的最小结构

一个没有 Slate 子控件、完全自己绘制的控件可以继承 `SLeafWidget`：

```cpp
class SColorSwatch : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SColorSwatch)
        : _Color(FLinearColor::White)
        , _DesiredSize(FVector2D(80.0f, 28.0f))
    {}

        SLATE_ATTRIBUTE(FSlateColor, Color)
        SLATE_ARGUMENT(FVector2D, DesiredSize)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs)
    {
        Color = InArgs._Color;
        DesiredSize = InArgs._DesiredSize;
    }

    virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override
    {
        return DesiredSize;
    }

    virtual int32 OnPaint(
        const FPaintArgs& Args,
        const FGeometry& AllottedGeometry,
        const FSlateRect& MyCullingRect,
        FSlateWindowElementList& OutDrawElements,
        int32 LayerId,
        const FWidgetStyle& InWidgetStyle,
        bool bParentEnabled) const override;

private:
    TAttribute<FSlateColor> Color;
    FVector2D DesiredSize;
};
```

`ComputeDesiredSize` 只表达期望尺寸，不应该依赖最终屏幕位置。真正获得的大小以 `OnPaint` 中的 `AllottedGeometry` 为准。

### 5.3 OnPaint 的职责

```cpp
int32 SColorSwatch::OnPaint(
    const FPaintArgs& Args,
    const FGeometry& AllottedGeometry,
    const FSlateRect& MyCullingRect,
    FSlateWindowElementList& OutDrawElements,
    int32 LayerId,
    const FWidgetStyle& InWidgetStyle,
    bool bParentEnabled) const
{
    const FLinearColor FinalColor =
        Color.Get(FSlateColor::UseForeground()).GetColor(InWidgetStyle)
        * InWidgetStyle.GetColorAndOpacityTint();

    FSlateDrawElement::MakeBox(
        OutDrawElements,
        LayerId,
        AllottedGeometry.ToPaintGeometry(),
        FAppStyle::Get().GetBrush(TEXT("WhiteBrush")),
        ESlateDrawEffect::None,
        FinalColor);

    return LayerId;
}
```

`OnPaint` 的关键规则：

- 向 `OutDrawElements` 添加绘制命令，而不是直接操作 RHI。
- 尊重 `AllottedGeometry`，不要假设控件位于屏幕原点。
- 尊重父控件传入的 Tint、Enabled 状态和裁剪区域。
- 返回自己和子控件实际使用到的最大 Layer。
- `OnPaint` 可能频繁发生，避免分配大对象、加载资源或执行昂贵业务逻辑。

### 5.4 常用绘制元素

| API | 用途 |
| --- | --- |
| `FSlateDrawElement::MakeBox` | 图片、纯色块、九宫格背景 |
| `MakeText` | 文本 |
| `MakeLines` | 折线、曲线或图表线段 |
| `MakeSpline` | 样条线 |
| `MakeGradient` | 渐变 |
| `MakeCustomVerts` | 自定义顶点与 UV |

能用 Brush 完成的背景、边框和图标，优先用 `MakeBox`。只有形状或 UV 确实特殊时才进入 `MakeCustomVerts`。

### 5.5 Layer 的正确使用

```text
Layer 0：背景
Layer 1：主体填充
Layer 2：文字或图标
Layer 3：悬停描边
Layer 4：焦点框
```

Layer 只需要在当前窗口绘制列表中维持正确先后关系，不是全局永久层级。

当组合控件既要绘制自己的背景又要绘制子控件时，常见思路是：

1. 当前 `LayerId` 画背景。
2. 调用基类或子控件绘制。
3. 在基类返回的最大层级之上画前景装饰。

不要让所有元素都占用同一个 Layer，否则批次或覆盖结果可能与预期不同。

### 5.6 Geometry 与坐标空间

自定义绘制和输入处理经常要在不同坐标空间转换：

- Local：相对于当前控件左上角。
- Absolute：Slate 桌面空间中的坐标。
- Paint Geometry：包含绘制位置、尺寸和变换的信息。

输入事件通常提供屏幕位置，可以转换到本地：

```cpp
const FVector2D LocalPosition =
    MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
```

绘制时尽量通过 `AllottedGeometry` 生成 Paint Geometry，避免自己拼接绝对坐标。这样控件在缩放、DPI 和父级变换下仍然正确。

### 5.7 一个自定义进度条思路

```cpp
const FVector2D LocalSize = AllottedGeometry.GetLocalSize();
const float ClampedProgress = FMath::Clamp(Progress.Get(), 0.0f, 1.0f);

FSlateDrawElement::MakeBox(
    OutDrawElements,
    LayerId,
    AllottedGeometry.ToPaintGeometry(),
    BackgroundBrush,
    ESlateDrawEffect::None,
    BackgroundColor);

const FVector2D FillSize(LocalSize.X * ClampedProgress, LocalSize.Y);

FSlateDrawElement::MakeBox(
    OutDrawElements,
    LayerId + 1,
    AllottedGeometry.ToPaintGeometry(FillSize, FSlateLayoutTransform()),
    FillBrush,
    ESlateDrawEffect::None,
    FillColor);

return LayerId + 1;
```

完整实现还应考虑从右向左布局、裁剪、0%/100% 边界、圆角被截断以及可访问性文本。

### 5.8 自定义 SPanel 何时使用

只有父容器需要特殊排列算法时才继承 `SPanel`，例如：

- 环形布局
- 节点图自由布局
- 流式换行布局
- 时间轴轨道布局

自定义 Panel 至少要负责：

- 保存子控件与自定义 Slot。
- 根据子控件计算 `ComputeDesiredSize`。
- 在 `OnArrangeChildren` 中分配几何区域。
- 正确处理 Visibility、Padding、对齐和流向。

这比自定义叶子绘制复杂得多，能用现有 Panel 组合时不要先造布局系统。

### 5.9 更新属性与失效

自定义 Setter 应告诉 Slate 发生了什么变化：

```cpp
void SColorSwatch::SetColor(const FSlateColor& InColor)
{
    Color.Set(InColor);
    Invalidate(EInvalidateWidgetReason::Paint);
}
```

如果变化影响期望尺寸，例如字体或文字发生变化，应当触发布局相关失效，而不只是 Paint。失效范围给得过小会导致界面不更新；范围无脑给得过大则会损害缓存效果。

### 5.10 本章练习

1. 实现 `SColorSwatch`，支持颜色绑定与固定期望尺寸。
2. 为它增加悬停描边，确保描边 Layer 在填充之上。
3. 实现一个带百分比文字的主题加载进度条。
4. 用 Widget Reflector 检查控件实际 Geometry 是否符合预期。

---

## 6. 输入、焦点与拖拽

### 6.1 输入处理的整体路径

可以把一次鼠标点击理解成：

```text
平台窗口收到输入
      ↓
FSlateApplication
      ↓
命中测试得到 Widget Path
      ↓
预览阶段（根 → 目标）
      ↓
常规事件阶段（目标 → 父级）
      ↓
某个控件返回 Handled，或最终无人处理
```

不是每一种输入事件都同时拥有完整的预览与冒泡形式，但“沿 Widget Path 路由”是理解 Slate 输入的主线。

### 6.2 FReply 不只是一个布尔值

`FReply` 除了表示是否处理，还可以携带后续操作：

```cpp
return FReply::Handled()
    .SetKeyboardFocus(SharedThis(this))
    .CaptureMouse(SharedThis(this));
```

常见操作：

- `SetKeyboardFocus`：请求键盘焦点。
- `CaptureMouse`：捕获鼠标，拖出控件范围后仍继续接收事件。
- `ReleaseMouseCapture`：释放捕获。
- `DetectDrag`：移动超过阈值后触发拖拽检测。
- `BeginDragDrop`：开始 Slate 拖放操作。
- `EndDragDrop`：结束拖放。

只有真正承担该输入语义的控件才返回 `Handled`。例如纯展示用的背景不应该吞掉鼠标点击。

### 6.3 鼠标捕获的典型模式

制作滑块或自定义拖动控件时：

```text
MouseDown
  → 记录初始状态
  → CaptureMouse

MouseMove
  → 如果仍持有 Capture，持续更新数值

MouseUp
  → 提交最终数值
  → ReleaseMouseCapture
```

还应处理 Capture 意外丢失的情况，及时恢复内部拖动状态，避免控件一直认为自己正在拖拽。

### 6.4 键盘焦点

想接收键盘事件的自定义控件通常需要允许焦点：

```cpp
virtual bool SupportsKeyboardFocus() const override
{
    return true;
}
```

然后处理 `OnKeyDown`：

```cpp
FReply SThemePreview::OnKeyDown(
    const FGeometry& MyGeometry,
    const FKeyEvent& InKeyEvent)
{
    if (InKeyEvent.GetKey() == EKeys::SpaceBar)
    {
        ToggleTheme();
        return FReply::Handled();
    }

    return FReply::Unhandled();
}
```

焦点变化时应提供可见反馈，例如焦点框或状态颜色。只有鼠标能操作、键盘无法操作的工具会损害效率和可访问性。

### 6.5 导航与 Tab 顺序

键盘和手柄导航依赖控件是否可聚焦、是否启用、是否可见以及导航规则。复杂表单要检查：

- Tab 是否按视觉顺序移动。
- 禁用或隐藏控件是否被跳过。
- 弹窗打开后焦点是否进入弹窗。
- 弹窗关闭后焦点是否回到合理位置。
- Enter、Space、Escape 是否符合平台习惯。

不要在每帧强制设置焦点；这会让用户无法离开当前控件。只在窗口首次打开、页面切换或明确操作后设置一次。

### 6.6 Slate 拖放操作

拖放数据通常放在继承自 `FDragDropOperation` 的对象中：

```cpp
class FThemeDragDropOp : public FDragDropOperation
{
public:
    DRAG_DROP_OPERATOR_TYPE(FThemeDragDropOp, FDragDropOperation)

    FName ThemeId;

    static TSharedRef<FThemeDragDropOp> New(FName InThemeId)
    {
        TSharedRef<FThemeDragDropOp> Operation = MakeShared<FThemeDragDropOp>();
        Operation->ThemeId = InThemeId;
        Operation->DefaultHoverText = FText::FromName(InThemeId);
        Operation->Construct();
        return Operation;
    }
};
```

源控件在按下时请求检测拖拽：

```cpp
return FReply::Handled().DetectDrag(SharedThis(this), EKeys::LeftMouseButton);
```

超过系统拖拽阈值后：

```cpp
FReply SThemeItem::OnDragDetected(
    const FGeometry& MyGeometry,
    const FPointerEvent& MouseEvent)
{
    return FReply::Handled().BeginDragDrop(FThemeDragDropOp::New(ThemeId));
}
```

目标控件在 `OnDragEnter` / `OnDragOver` 中提供反馈，在 `OnDrop` 中验证类型并处理数据：

```cpp
FReply SThemeDropTarget::OnDrop(
    const FGeometry& MyGeometry,
    const FDragDropEvent& DragDropEvent)
{
    TSharedPtr<FThemeDragDropOp> Operation =
        DragDropEvent.GetOperationAs<FThemeDragDropOp>();

    if (!Operation.IsValid())
    {
        return FReply::Unhandled();
    }

    ApplyTheme(Operation->ThemeId);
    return FReply::Handled();
}
```

目标必须验证 Operation 类型和数据，不要假设所有拖入对象都来自自己的控件。

### 6.7 命令与快捷键

编辑器工具中的快捷键应优先使用 `TCommands` 与 `FUICommandList`，而不是在根控件里硬编码所有 `OnKeyDown`。

命令系统的好处：

- 菜单会自动显示快捷键。
- 工具栏、菜单和键盘共享同一执行逻辑。
- `CanExecute` 可以统一控制可用状态。
- `IsChecked` 可以统一表达切换状态。

控件内部、只在控件获得焦点时有意义的键盘操作，仍适合用 `OnKeyDown`。

### 6.8 输入相关常见问题

| 现象 | 常见原因 | 排查方向 |
| --- | --- | --- |
| 按钮点不到 | 上层 Overlay 命中测试挡住 | 检查 `EVisibility` 和 Widget Reflector 命中路径 |
| 收不到键盘事件 | 控件不支持焦点或没有焦点 | `SupportsKeyboardFocus`、焦点设置 |
| 拖出控件后停止更新 | 没有鼠标捕获 | MouseDown 时 `CaptureMouse` |
| 父控件快捷键失效 | 子控件无条件 `Handled` | 未处理按键返回 `Unhandled` |
| 拖放没有触发 | 未请求 `DetectDrag` 或移动未过阈值 | 检查 MouseDown Reply 与鼠标键 |
| 弹窗后无法操作主窗口 | 焦点/模态窗口没有正确结束 | 检查窗口关闭和焦点恢复 |

### 6.9 本章练习

1. 让主题预览控件获得焦点后，可以用 Space 切换主题。
2. 实现鼠标拖动调整主题预览的明度，并正确处理鼠标捕获。
3. 把主题条目拖到预览区即可应用主题。
4. 为“切换主题”定义一个编辑器命令，并同时接入菜单与快捷键。

---

## 7. 性能、失效与调试工具

### 7.1 Slate 性能消耗来自哪里

常见成本可以分成：

```text
属性求值
  + Prepass / Desired Size 计算
  + Arrange Children
  + OnPaint 生成绘制元素
  + Draw Element 批处理
  + GPU 绘制
  + Tick / Active Timer / 业务回调
```

性能问题不一定是“控件太多”。一小棵控件树如果每帧重建、反复布局或执行昂贵绑定，也可能比虚拟化的大列表更慢。

### 7.2 失效原因的基本分类

不同变化影响范围不同：

| 变化 | 常见影响 |
| --- | --- |
| 颜色、Tint、悬停视觉 | Paint |
| 文字、字体、Padding、尺寸规则 | Layout，通常也需要重新 Paint |
| Visibility | Layout / Paint / Hit Test |
| 子控件增删、顺序变化 | Child Order / Layout / Paint |
| Render Transform | Paint 或变换相关更新 |
| 是否可交互 | Hit Test / Attribute 状态 |

自定义控件中要给出足够准确的 `EInvalidateWidgetReason`。如果不确定变化是否影响尺寸，先从正确性出发验证，再缩小失效范围。

### 7.3 绑定、主动更新和 Tick 的选择

| 更新方式 | 适用场景 | 风险 |
| --- | --- | --- |
| Attribute 绑定 | 值天然派生、读取便宜、需要自动同步 | 绑定过多或 Getter 很昂贵 |
| Setter 主动更新 | 明确事件驱动、变化频率低 | 容易漏掉某个更新入口 |
| Tick | 连续动画、跟随实时仿真 | 每帧成本、隐藏状态下浪费 |
| Active Timer | 需要定时更新但不一定永久每帧 | 要正确停止和管理生命周期 |

一个主题名称只在切换主题时变化，适合事件 + Setter；一个实时动画进度可能适合 Active Timer 或动画序列；磁盘扫描绝不应该塞进属性 Getter。

### 7.4 避免每次刷新都重建控件树

错误倾向：数据改变后删除整个面板，再次 `SNew` 全部控件。

更好的层级选择：

1. 只更新一个属性。
2. 刷新一个列表数据源。
3. 只替换 `SWidgetSwitcher` 的当前页面。
4. 只有结构确实变化时才重建局部子树。
5. 最后才重建整个 Tab。

控件树重建会产生共享对象分配、布局缓存丢失、焦点丢失和列表滚动位置变化。

### 7.5 大数据列表使用虚拟化

`SListView` 只为当前可见区域和少量缓冲区生成 Row，适合大量数据。优化要点：

- Row 生成函数只负责轻量显示，不执行同步 IO。
- 数据模型与 Row 控件分离。
- 数据变化后 `RequestListRefresh`，不要销毁整个列表。
- 稳定的选择键不要依赖临时行控件地址。
- 过滤和排序结果先在数据层计算，再提供给列表。

### 7.6 Widget Reflector

Widget Reflector 是学习和调试 Slate 最重要的工具之一。它可以帮助查看：

- 鼠标当前位置的 Widget Path。
- 控件类型、可见性与地址。
- Desired Size 和实际 Geometry。
- 控件层级与源代码位置。
- 焦点路径和命中测试问题。
- 失效与更新情况（具体功能随 UE 版本变化）。

排查“为什么这个按钮点不到”时，不要只盯着代码猜。先用 Pick Widget 定位鼠标真正命中的上层控件。

### 7.7 性能统计与排查顺序

常用入口包括：

- Widget Reflector 的统计、快照和失效观察功能。
- 控制台命令 `stat Slate`。
- 控制台命令 `stat SlateRendering`。
- Unreal Insights 中与 Slate、CPU、加载和帧时间相关的轨道。

不同 UE 版本提供的细分统计项和调试开关可能不同，应以当前 UE 5.8 编辑器中可见命令与源码为准。

推荐排查顺序：

1. 确认问题是 CPU 布局/绘制，还是 GPU 绘制。
2. 找出异常频繁 Tick、属性 Getter 或 Active Timer。
3. 检查是否每帧重建控件或刷新大列表。
4. 检查局部变化是否引发了整棵树布局。
5. 检查 Draw Elements、裁剪、复杂材质和批次数量。
6. 修改后在相同界面状态下重新测量。

### 7.8 常见卡顿模式

#### 模式一：绑定 Getter 中做昂贵工作

```cpp
// 不推荐：UI 取文本时扫描磁盘
FText GetStatusText() const
{
    ScanAllThemeFiles();
    return BuildStatusText();
}
```

正确方向是后台或明确事件中扫描，把结果保存成状态，Getter 只读取已经准备好的值。

#### 模式二：Tick 中无条件更新文本

即使内容没有变化，持续 `SetText` 也可能造成多余失效。先比较状态，只在实际变化时设置。

#### 模式三：ScrollBox 塞入大量复杂子控件

改用 `SListView` / `STileView` 虚拟化，或分页展示。

#### 模式四：隐藏页面仍在工作

不可见页面的业务任务、Active Timer 或外部委托可能仍然活跃。页面关闭或切换时，应暂停不必要的更新。

### 7.9 生命周期与委托安全

性能问题有时会伪装成生命周期问题：旧控件虽然不再显示，却因为 Delegate 或共享引用没有释放而继续执行。

检查：

- 外部全局 Delegate 是否保存并移除了 `FDelegateHandle`。
- Lambda 是否强引用了本应销毁的对象。
- 是否形成 `TSharedPtr` 循环引用。
- Tab 关闭后 Active Timer 是否停止。
- Style Brush 是否在使用者之前被销毁。

观察析构日志和共享引用关系，往往能找到“面板关了仍然 Tick”的原因。

### 7.10 调试问题速查表

| 问题 | 第一检查点 |
| --- | --- |
| UI 没更新 | 属性是否重新求值、Setter 是否调用、失效原因是否足够 |
| 布局突然跳动 | Desired Size、Visibility、字体或 Padding 是否变化 |
| 文本被截断 | 固定宽度、AutoWrap、Clipping、本地化长度 |
| 图标不显示 | Brush 生命周期、资源路径、Style Key、图片尺寸 |
| 点击错位 | Geometry 变换、DPI、Overlay 命中、Render Transform |
| 关闭 Tab 后仍有回调 | Delegate、Active Timer、共享引用和注销逻辑 |
| 热重载后重复菜单 | ToolMenus Owner 或 Startup Callback 未注销 |
| 切换主题后部分控件还是旧颜色 | 控件缓存了旧常量、Brush 指针或未收到主题事件 |

### 7.11 发布前检查清单

- 插件模块只在适当目标中加载。
- 注册和注销成对，重复启动不会产生重复项。
- 所有 Brush 和 Widget Style 生命周期正确。
- 窄窗口、宽窗口、高 DPI 下布局可用。
- 长文本和中英文切换不会严重截断。
- 鼠标、键盘焦点和快捷键都可操作。
- 大列表使用虚拟化，昂贵任务不在 Tick 或 Getter 中。
- Widget Reflector 中没有意外遮挡输入的控件。
- `stat Slate` 与实际交互测试没有明显异常峰值。

### 7.12 推荐学习顺序

```text
第 1 章：建立控件树、布局、绘制、输入的全局模型
   ↓
第 2 章：能够搭建普通工具面板
   ↓
第 4 章：把面板真正放进编辑器插件和 Tab
   ↓
第 3 章：将视觉资源抽成可切换主题
   ↓
第 6 章：完善焦点、快捷键和拖放交互
   ↓
第 5 章：确有必要时再做自定义绘制
   ↓
第 7 章：使用工具验证性能和生命周期
```

不要一开始就背所有控件 API。先完成一个小型编辑器面板，每遇到一个问题，再回到对应章节验证它属于控件树、布局、绘制、输入、样式还是生命周期。

---

## 8. 虚拟化列表、树与多列表格

### 8.1 为什么不能把大量条目直接塞进 SScrollBox

`SScrollBox` 会长期持有其中的所有子控件。几十项通常没问题，但数百或数千项会带来：

- 大量 Widget 创建和共享指针分配。
- 更重的 Prepass、布局与绘制遍历。
- 数据更新时难以只刷新必要行。
- 选择、排序、键盘导航需要自己实现。

`SListView`、`STreeView` 和 `STileView` 属于 TableView 家族，会根据可见区域按需生成 Row。数据有一万条，并不意味着同时存在一万个 Row Widget。

```text
数据源 TArray<ItemType>
        ↓
TableView 计算当前可见索引
        ↓
OnGenerateRow 只生成可见 Row
        ↓
滚动后回收/生成新的可见 Row
```

### 8.2 ItemType 的常见选择

Slate 列表的 `ItemType` 通常使用共享指针：

```cpp
struct FThemeListItem
{
    FName Id;
    FText DisplayName;
    FText Description;
    bool bBuiltIn = false;
};

using FThemeListItemPtr = TSharedPtr<FThemeListItem>;
```

```cpp
TArray<FThemeListItemPtr> ThemeItems;
TSharedPtr<SListView<FThemeListItemPtr>> ThemeListView;
```

共享指针让条目在选择、菜单弹出和 Row 生成期间保持稳定身份。不要用 Row Widget 地址代表业务对象；Row 会随着虚拟化而更换。

### 8.3 SListView 的完整闭环

```cpp
SAssignNew(ThemeListView, SListView<FThemeListItemPtr>)
.ListItemsSource(&ThemeItems)
.SelectionMode(ESelectionMode::Single)
.OnGenerateRow(this, &SThemeLibrary::GenerateThemeRow)
.OnSelectionChanged(this, &SThemeLibrary::HandleThemeSelected)
.OnContextMenuOpening(this, &SThemeLibrary::BuildContextMenu);
```

```cpp
TSharedRef<ITableRow> SThemeLibrary::GenerateThemeRow(
    FThemeListItemPtr Item,
    const TSharedRef<STableViewBase>& OwnerTable)
{
    return SNew(STableRow<FThemeListItemPtr>, OwnerTable)
    .Padding(FMargin(8.0f, 4.0f))
    [
        SNew(STextBlock)
        .Text(Item.IsValid() ? Item->DisplayName : FText::GetEmpty())
    ];
}
```

数据更新步骤：

```cpp
ThemeItems = BuildFilteredItems();
ThemeListView->RequestListRefresh();
```

`RequestListRefresh()` 告诉列表在合适时机刷新，不要求调用者立即重建全部行。若只想滚动并选中某项，可使用 `RequestScrollIntoView(Item)` 和 `SetSelection(Item)`。

### 8.4 OptionsSource 与 ListItemsSource 的生命周期

列表只保存数据源数组的地址，不复制整个数组：

```cpp
.ListItemsSource(&ThemeItems)
```

所以 `ThemeItems` 必须是拥有该列表的对象成员，不能是 `Construct` 里的局部变量。这与当前插件中 `SComboBox` 的 `ThemeOptions` 成员是同一个原则。

安全更新顺序通常是：

1. 暂时保存当前选择的业务 Id。
2. 更新成员数组。
3. 调用 `RequestListRefresh()`。
4. 根据 Id 查找新条目并恢复选择。

### 8.5 多列表格：SMultiColumnTableRow

当每项需要多列时，给 ListView 设置 HeaderRow，并让行继承 `SMultiColumnTableRow`：

```cpp
SAssignNew(ThemeListView, SListView<FThemeListItemPtr>)
.ListItemsSource(&ThemeItems)
.OnGenerateRow(this, &SThemeLibrary::GenerateThemeRow)
.HeaderRow
(
    SNew(SHeaderRow)
    + SHeaderRow::Column(TEXT("Name"))
      .DefaultLabel(FText::FromString(TEXT("主题")))
      .FillWidth(0.35f)
    + SHeaderRow::Column(TEXT("Description"))
      .DefaultLabel(FText::FromString(TEXT("说明")))
      .FillWidth(0.55f)
    + SHeaderRow::Column(TEXT("Type"))
      .DefaultLabel(FText::FromString(TEXT("类型")))
      .FillWidth(0.10f)
);
```

```cpp
class SThemeTableRow : public SMultiColumnTableRow<FThemeListItemPtr>
{
public:
    SLATE_BEGIN_ARGS(SThemeTableRow) {}
        SLATE_ARGUMENT(FThemeListItemPtr, Item)
    SLATE_END_ARGS()

    void Construct(
        const FArguments& InArgs,
        const TSharedRef<STableViewBase>& OwnerTable)
    {
        Item = InArgs._Item;
        SMultiColumnTableRow::Construct(
            FSuperRowType::FArguments().Padding(FMargin(6.0f, 3.0f)),
            OwnerTable);
    }

    virtual TSharedRef<SWidget> GenerateWidgetForColumn(
        const FName& ColumnName) override;

private:
    FThemeListItemPtr Item;
};
```

`GenerateWidgetForColumn` 根据稳定的 Column Id 返回内容。表头显示文本可以本地化，但 Column Id 不应随语言变化。

### 8.6 STreeView 的数据模型

树不是让 Row 自己寻找子控件，而是通过 `OnGetChildren` 从业务模型取子项：

```cpp
struct FThemeTreeNode
{
    FText Label;
    TArray<TSharedPtr<FThemeTreeNode>> Children;
};
```

```cpp
SAssignNew(ThemeTree, STreeView<TSharedPtr<FThemeTreeNode>>)
.TreeItemsSource(&RootNodes)
.OnGenerateRow(this, &SThemeTreePanel::GenerateRow)
.OnGetChildren(this, &SThemeTreePanel::GetChildren);
```

```cpp
void SThemeTreePanel::GetChildren(
    TSharedPtr<FThemeTreeNode> Parent,
    TArray<TSharedPtr<FThemeTreeNode>>& OutChildren) const
{
    if (Parent.IsValid())
    {
        OutChildren.Append(Parent->Children);
    }
}
```

展开状态由 TreeView 管理，但如果重建了全部节点对象，旧指针身份会消失，展开和选择状态也可能丢失。稳定 Id 或复用模型对象有助于恢复状态。

### 8.7 搜索、过滤和排序应该放在哪里

推荐分成两层数组：

```text
AllThemeItems：完整数据
       ↓ 过滤 + 排序
VisibleThemeItems：交给 ListView 的视图数据
```

过滤时先把搜索文本标准化；数据量大或搜索昂贵时使用防抖。排序只调整数据源顺序，然后 `RequestListRefresh()`，不要在每个 Row 的 Getter 中重复排序。

### 8.8 本章练习

1. 将主题下拉框扩展成带搜索框的 `SListView`。
2. 加入“名称、描述、来源”三列，并实现表头排序。
3. 用 `STreeView` 表示“内置主题 / 用户主题 / 主题变体”。
4. 刷新数据后恢复选中项、滚动位置和展开状态。

---

## 9. 命令、快捷键与 ToolMenus

### 9.1 命令系统解决什么问题

直接给菜单项绑定 `FUIAction` 很适合单个入口。但当同一操作同时出现在菜单、工具栏、右键菜单和快捷键中时，应使用命令系统：

```text
TCommands：定义名称、说明、图标、默认快捷键
       ↓
FUICommandList：映射 Execute / CanExecute / IsChecked
       ↓
ToolMenus、工具栏和键盘复用同一个 CommandInfo
```

命令描述“用户可以做什么”，命令列表描述“当前上下文中怎么做、能不能做”。

### 9.2 定义 TCommands

```cpp
class FSlateThemeCommands final : public TCommands<FSlateThemeCommands>
{
public:
    FSlateThemeCommands()
        : TCommands<FSlateThemeCommands>(
            TEXT("SlateThemeSwitcher"),
            NSLOCTEXT("Contexts", "SlateThemeSwitcher", "Slate Theme Switcher"),
            NAME_None,
            FAppStyle::GetAppStyleSetName())
    {
    }

    virtual void RegisterCommands() override;

    TSharedPtr<FUICommandInfo> OpenPanel;
    TSharedPtr<FUICommandInfo> ToggleTheme;
};
```

```cpp
#define LOCTEXT_NAMESPACE "SlateThemeCommands"

void FSlateThemeCommands::RegisterCommands()
{
    UI_COMMAND(
        OpenPanel,
        "打开主题面板",
        "打开 Slate Theme Switcher。",
        EUserInterfaceActionType::Button,
        FInputChord(EModifierKey::Control | EModifierKey::Alt, EKeys::T));

    UI_COMMAND(
        ToggleTheme,
        "切换主题",
        "切换到下一个已注册主题。",
        EUserInterfaceActionType::Button,
        FInputChord());
}

#undef LOCTEXT_NAMESPACE
```

默认快捷键只是初始值；编辑器可以允许用户在 Keyboard Shortcuts 中重新绑定。

### 9.3 映射 FUICommandList

```cpp
CommandList = MakeShared<FUICommandList>();

CommandList->MapAction(
    FSlateThemeCommands::Get().OpenPanel,
    FExecuteAction::CreateRaw(this, &FSlateThemeSwitcherModule::OpenThemeSwitcher));

CommandList->MapAction(
    FSlateThemeCommands::Get().ToggleTheme,
    FExecuteAction::CreateRaw(this, &FSlateThemeSwitcherModule::ToggleTheme),
    FCanExecuteAction::CreateRaw(this, &FSlateThemeSwitcherModule::CanToggleTheme));
```

切换类命令还可以提供 `FIsActionChecked`，同时将动作类型声明为 `ToggleButton`。UI 的勾选状态就会来自同一业务状态，而不是菜单自己保存一份副本。

### 9.4 注册和注销顺序

```cpp
void FSlateThemeSwitcherModule::StartupModule()
{
    FSlateThemeCommands::Register();
    CommandList = MakeShared<FUICommandList>();
    MapCommands();
    // 然后注册菜单与 Tab
}

void FSlateThemeSwitcherModule::ShutdownModule()
{
    // 先移除菜单与 Tab
    CommandList.Reset();
    FSlateThemeCommands::Unregister();
}
```

不要在 Commands 注册之前调用 `FSlateThemeCommands::Get()`；也不要在菜单仍引用命令时过早注销。

### 9.5 让 ToolMenus 使用命令

```cpp
void FSlateThemeSwitcherModule::RegisterMenus()
{
    FToolMenuOwnerScoped OwnerScoped(this);

    UToolMenu* Menu = UToolMenus::Get()->ExtendMenu(
        TEXT("LevelEditor.MainMenu.Window"));
    FToolMenuSection& Section = Menu->FindOrAddSection(TEXT("WindowLayout"));

    Section.AddMenuEntry(
        FSlateThemeCommands::Get().OpenPanel,
        CommandList);
}
```

菜单因此自动获得命令的标签、说明、图标、快捷键和可用状态。工具栏也可以引用相同命令，不需要复制执行委托。

### 9.6 CanExecute 为什么重要

禁用状态应表达真实业务约束。例如：

- 没有注册任何主题时不能“切换主题”。
- 异步导入进行中时不能重复“导入”。
- 没有选中列表项时不能“删除主题”。

`CanExecute` 应快速、无副作用。不要在菜单每次查询可用状态时执行磁盘扫描。

### 9.7 快捷键作用域与冲突

全局编辑器命令要谨慎选择快捷键。常见策略：

- 插件主入口可以有全局快捷键。
- 只对某个面板有意义的命令，附加到该面板或 Tab 的 CommandList。
- 文本输入框获得焦点时，不要抢占常用编辑按键。
- 使用 Input Binding Editor 检查冲突，并允许用户覆盖默认绑定。

### 9.8 本章练习

1. 为“打开主题面板”定义 `Ctrl+Alt+T` 默认快捷键。
2. 把同一命令同时添加到 Window 菜单和工具栏。
3. 添加“下一个主题”命令，在主题少于两个时禁用。
4. 添加一个 ToggleButton 命令，让菜单勾选状态始终来自管理器。

---

## 10. Details Panel 属性定制

### 10.1 Details Panel 与普通 Slate 面板的区别

普通 Slate 面板完全由你组织控件树；Details Panel 根据 `UCLASS` / `USTRUCT` 的反射属性自动生成编辑 UI。

当默认属性排列不够用时，有两类定制：

- `IDetailCustomization`：定制一个 UObject 类的整个详情面板。
- `IPropertyTypeCustomization`：定制某个 USTRUCT 属性类型的标题行和子项。

它们仍然使用 Slate 生成内容，但属性读取、撤销重做、多对象编辑和元数据由 PropertyEditor 系统协助处理。

### 10.2 模块依赖

编辑器模块的 `Build.cs` 通常增加：

```csharp
PrivateDependencyModuleNames.AddRange(new[]
{
    "PropertyEditor",
    "EditorSubsystem",
    "UnrealEd"
});
```

定制代码应放在 Editor 模块，避免运行时目标依赖编辑器模块。

### 10.3 IDetailCustomization 基本结构

```cpp
class FThemeSettingsCustomization final : public IDetailCustomization
{
public:
    static TSharedRef<IDetailCustomization> MakeInstance()
    {
        return MakeShared<FThemeSettingsCustomization>();
    }

    virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
};
```

```cpp
void FThemeSettingsCustomization::CustomizeDetails(
    IDetailLayoutBuilder& DetailBuilder)
{
    TSharedRef<IPropertyHandle> ThemeIdProperty =
        DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UThemeSettings, ThemeId));

    IDetailCategoryBuilder& Category =
        DetailBuilder.EditCategory(TEXT("Theme"));

    Category.AddProperty(ThemeIdProperty);

    Category.AddCustomRow(FText::FromString(TEXT("Preview")))
    .WholeRowContent()
    [
        SNew(SButton)
        .Text(FText::FromString(TEXT("预览当前主题")))
        .OnClicked_Lambda([]()
        {
            // 调用主题预览逻辑
            return FReply::Handled();
        })
    ];
}
```

使用 `GET_MEMBER_NAME_CHECKED` 可以在属性重命名后尽早暴露编译错误。

### 10.4 为什么优先使用 IPropertyHandle

不要通过裸 UObject 指针直接修改 Details 中的属性。`IPropertyHandle` 帮助处理：

- PreEditChange / PostEditChange
- Undo / Redo 事务
- 多对象同时编辑
- 数组、Map、Set 与嵌套属性
- 元数据和编辑条件
- 值为 Multiple Values 的状态

```cpp
FName ThemeId;
if (ThemeIdProperty->GetValue(ThemeId) == FPropertyAccess::Success)
{
    // 使用读取结果
}

ThemeIdProperty->SetValue(NewThemeId);
```

### 10.5 自定义 NameContent 与 ValueContent

```cpp
Category.AddCustomRow(FText::FromString(TEXT("Theme Accent")))
.NameContent()
[
    SNew(STextBlock)
    .Text(FText::FromString(TEXT("强调色")))
    .Font(IDetailLayoutBuilder::GetDetailFont())
]
.ValueContent()
.MinDesiredWidth(220.0f)
[
    SNew(SColorBlock)
    .Color(this, &FThemeSettingsCustomization::GetAccentColor)
    .OnMouseButtonDown(this, &FThemeSettingsCustomization::OpenColorPicker)
];
```

Details Panel 有自己的字体、间距和行高规范。尽量使用 Detail 系统提供的字体与默认属性控件，让自定义行与编辑器原生界面一致。

### 10.6 注册 Class Layout

```cpp
FPropertyEditorModule& PropertyEditor =
    FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));

PropertyEditor.RegisterCustomClassLayout(
    UThemeSettings::StaticClass()->GetFName(),
    FOnGetDetailCustomizationInstance::CreateStatic(
        &FThemeSettingsCustomization::MakeInstance));

PropertyEditor.NotifyCustomizationModuleChanged();
```

关闭模块时：

```cpp
if (FModuleManager::Get().IsModuleLoaded(TEXT("PropertyEditor")))
{
    FPropertyEditorModule& PropertyEditor =
        FModuleManager::GetModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));

    PropertyEditor.UnregisterCustomClassLayout(
        UThemeSettings::StaticClass()->GetFName());
}
```

注册名和注销名必须完全一致，否则热重载后可能留下旧定制。

### 10.7 Property Type Customization

如果多个类都包含同一个 `FThemePalette` 结构，应该定制结构类型，而不是在每个类的 Details 中重复代码。

```cpp
class FThemePaletteCustomization final : public IPropertyTypeCustomization
{
public:
    static TSharedRef<IPropertyTypeCustomization> MakeInstance();

    virtual void CustomizeHeader(
        TSharedRef<IPropertyHandle> StructPropertyHandle,
        FDetailWidgetRow& HeaderRow,
        IPropertyTypeCustomizationUtils& Utils) override;

    virtual void CustomizeChildren(
        TSharedRef<IPropertyHandle> StructPropertyHandle,
        IDetailChildrenBuilder& ChildBuilder,
        IPropertyTypeCustomizationUtils& Utils) override;
};
```

它通过 `RegisterCustomPropertyTypeLayout` 注册，使用结构类型名而不是拥有它的 UObject 类名。

### 10.8 刷新与生命周期

当外部主题注册表变化会改变 Details 的行结构时，可以请求刷新：

```cpp
DetailBuilder.ForceRefreshDetails();
```

不要每帧刷新 Details。定制对象可能随着选择变化被销毁；绑定外部委托时应保存 Handle，并在定制对象析构或生命周期结束时解除。

### 10.9 本章练习

1. 创建 `UThemeSettings`，让默认 Details 显示主题 Id 和强调色。
2. 使用 `IDetailCustomization` 添加实时主题预览行。
3. 用 `IPropertyHandle` 修改属性，验证 Undo / Redo。
4. 将色板提取为 `FThemePalette`，再用 `IPropertyTypeCustomization` 复用显示逻辑。

---

## 11. 动画、Active Timer 与异步任务

### 11.1 三种“随时间变化”的工具

| 机制 | 适合 | 不适合 |
| --- | --- | --- |
| `Tick` | 必须逐帧响应的交互或连续状态 | 低频轮询、昂贵任务 |
| Active Timer | 延迟、周期更新、按需保持活跃 | 后台线程工作、阻塞 IO |
| `FCurveSequence` | Slate 属性过渡和缓动动画 | 业务定时器、文件扫描 |

选择标准不是“哪个 API 最方便”，而是更新频率、面板不可见时是否仍需运行、工作是否必须在 Game Thread 上执行。

### 11.2 Active Timer 的注册与停止

当前插件使用 Active Timer 每 300ms 生成一次仿真快照：

```cpp
SimulationTimerHandle = RegisterActiveTimer(
    0.3f,
    FWidgetActiveTimerDelegate::CreateSP(
        this,
        &SThemeSwitcherPanel::HandleSimulationTick));
```

```cpp
EActiveTimerReturnType SThemeSwitcherPanel::HandleSimulationTick(
    double InCurrentTime,
    float InDeltaTime)
{
    if (bSimulationRunning)
    {
        UpdateSimulationData();
        PushSimulationData();
    }

    return EActiveTimerReturnType::Continue;
}
```

若任务已经结束，返回 `Stop`；如果保存了 Handle，也可以主动注销：

```cpp
if (SimulationTimerHandle.IsValid())
{
    UnRegisterActiveTimer(SimulationTimerHandle.ToSharedRef());
    SimulationTimerHandle.Reset();
}
```

“暂停仿真”和“停止 Timer”并不是同一件事。暂停时间很长时，停止 Timer 更省；需要随时轻量恢复时，可以保留 Timer 但跳过更新。

### 11.3 用 FCurveSequence 做缓动

```cpp
FCurveSequence HoverSequence;
FCurveHandle HoverCurve;

void SAnimatedThemeCard::Construct(const FArguments& InArgs)
{
    HoverCurve = HoverSequence.AddCurve(
        0.0f,
        0.18f,
        ECurveEaseFunction::QuadOut);
}
```

鼠标进入和离开时播放正向或反向动画：

```cpp
void SAnimatedThemeCard::OnMouseEnter(
    const FGeometry& MyGeometry,
    const FPointerEvent& MouseEvent)
{
    HoverSequence.Play(SharedThis(this));
}

void SAnimatedThemeCard::OnMouseLeave(
    const FPointerEvent& MouseEvent)
{
    HoverSequence.Reverse();
}
```

在属性 Getter 或 `OnPaint` 中读取 `HoverCurve.GetLerp()`，得到 0～1 的进度，再插值颜色、透明度或 Render Transform。动画值应驱动视觉，不应在 Getter 中修改业务状态。

### 11.4 Layout 动画与 Render Transform

改变 Padding、Desired Size 会触发布局；改变 Render Transform 通常只影响绘制阶段。纯视觉位移、缩放和淡入淡出优先考虑 Render Transform 与透明度，这比每帧重排控件树轻。

但 Render Transform 不会天然改变周围控件的布局占位。需要其他控件让路时，仍必须使用 Layout 变化。

### 11.5 昂贵工作必须离开 Game Thread

文件扫描、解析大型配置和网络请求不应在 Slate 回调或属性 Getter 中同步执行。典型模式：

```text
Game Thread：收集不可变输入，显示“加载中”
       ↓
后台线程：只处理线程安全的数据
       ↓
Game Thread：验证控件仍存在，提交结果并刷新 UI
```

```cpp
TWeakPtr<SThemeLibrary> WeakThis = SharedThis(this);
const FString SearchRoot = RequestedRoot;

AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask,
    [WeakThis, SearchRoot]()
    {
        TArray<FThemeFileInfo> Results = ScanThemeFiles(SearchRoot);

        AsyncTask(ENamedThreads::GameThread,
            [WeakThis, Results = MoveTemp(Results)]() mutable
            {
                if (TSharedPtr<SThemeLibrary> Pinned = WeakThis.Pin())
                {
                    Pinned->ApplyScanResults(MoveTemp(Results));
                }
            });
    });
```

弱指针检查很重要：后台任务结束时，用户可能已经关闭了 Tab。

### 11.6 线程边界规则

- 大多数 Slate Widget 操作只能在 Game Thread 进行。
- 不要在后台线程读取会变化的 Widget 属性。
- UObject 是否能在后台访问取决于具体 API；默认按不安全处理。
- 后台线程使用输入副本和普通数据结构，完成后回 Game Thread 更新 UI。
- 关闭模块时，要考虑尚未完成的任务，避免回调进入已卸载模块代码。

### 11.7 防止旧任务覆盖新结果

用户连续输入搜索词时，较早任务可能更晚完成。可以使用请求序号：

```cpp
const uint64 ThisRequest = ++LatestRequestId;
```

结果回到 Game Thread 后，只有 `ThisRequest == LatestRequestId` 才提交。取消标志、任务句柄或可取消异步框架也可以实现相同目标。

### 11.8 本章练习

1. 给主题卡片增加 0.18 秒悬停淡入动画。
2. 将仿真 Timer 改成停止时真正注销、继续时重新注册。
3. 后台扫描主题 JSON 文件，完成后用 `RequestListRefresh()` 更新列表。
4. 快速连续搜索，验证旧结果不会覆盖新结果。

---

## 12. DPI、本地化、无障碍与可测试性

### 12.1 成熟 UI 不只是在当前电脑上“看起来没问题”

编辑器工具至少要面对：

- 不同窗口大小和高 DPI 缩放。
- 中文、英文以及更长的本地化文本。
- 键盘操作和焦点反馈。
- 色觉差异与足够对比度。
- 自动化测试和可重复验证。

这些不是最后补丁，而会反过来影响布局和数据流设计。

### 12.2 DPI 与尺寸策略

Slate 的几何会受到应用缩放和显示器 DPI 影响。实践原则：

- 不要用屏幕像素假设控件最终物理大小。
- 优先 Auto、Fill、MinDesiredWidth 和可换行文本。
- 图标资源准备合适分辨率，避免放大后模糊。
- 自定义绘制使用 `AllottedGeometry`，不要硬编码绝对屏幕坐标。
- 多显示器测试窗口跨越不同 DPI 时的表现。

`HeightOverride(32)` 表达 Slate Unit 中的逻辑高度，不等同于永远占 32 个物理像素。

### 12.3 使用 FText，而不是把 FString 当界面文本

| 类型 | 主要用途 |
| --- | --- |
| `FText` | 面向用户、可本地化和格式化的文本 |
| `FString` | 普通可变字符串、路径、序列化中间值 |
| `FName` | 稳定标识、快速比较、样式键和注册 Id |

显示文本使用 `LOCTEXT`：

```cpp
#define LOCTEXT_NAMESPACE "SlateThemeSwitcher"

const FText Label = LOCTEXT("ToggleTheme", "切换主题");

#undef LOCTEXT_NAMESPACE
```

Key 在同一 Namespace 中必须稳定且语义清楚。不要根据运行时字符串动态生成 `LOCTEXT` Key。

### 12.4 格式化与复数

不要用字符串拼接构造面向用户的句子：

```cpp
const FText Status = FText::Format(
    LOCTEXT("ThemeCount", "已注册 {0} 个主题"),
    FText::AsNumber(ThemeCount));
```

不同语言的词序不一样，完整格式字符串才能让译者调整顺序。日期、时间、数字和百分比也优先使用 `FText` 的区域化格式。

### 12.5 长文本和双向文本

- 标签不要依赖刚好容纳中文的固定宽度。
- 说明文本启用 `AutoWrapText`，并提供合理可用宽度。
- 按钮至少留出文本扩展空间。
- 用最长目标语言测试菜单、表头和弹窗。
- 图标与文字的方向关系要考虑从右到左语言。

布局被长文本撑开时，应修正容器约束和换行策略，而不是截掉所有文字。

### 12.6 键盘与焦点可见性

最低要求：

- 所有主要操作都能通过 Tab、方向键或命令快捷键到达。
- 当前焦点有明显视觉反馈。
- 弹窗打开后焦点进入弹窗，关闭后合理恢复。
- Escape 能关闭临时菜单或取消操作。
- 未处理按键返回 `Unhandled`，不破坏编辑器全局快捷键。

用纯键盘完整走一遍功能，是最便宜也最有效的可访问性测试之一。

### 12.7 无障碍语义与颜色

Slate 控件具有无障碍行为、摘要文本与子控件可访问性相关设置。组合现有标准控件通常比完全自绘更容易获得正确语义。

设计时还要注意：

- 不要只靠红/绿区分状态；同时使用文本、图标或形状。
- 正文和背景保持足够对比度。
- Disabled 状态仍应可辨认，不要淡到完全看不清。
- 图标按钮需要 Tooltip 或可访问名称。
- 动画不应成为理解状态的唯一方式。

### 12.8 让控件更容易测试

可测试的 Slate 控件通常具有：

- 业务状态集中在管理器或 ViewModel，而不是散落在 Widget 中。
- 显示 Getter 无副作用。
- 按钮调用明确的方法或命令。
- 稳定的 Tab Id、Command Id、Widget 类型和可见文本。
- 时间与随机数可以注入或使用固定种子。

当前插件使用 `FRandomStream` 管理仿真数据；测试时可以固定 Seed，使一系列快照可重复。

### 12.9 测试矩阵

| 维度 | 建议状态 |
| --- | --- |
| 窗口 | 最窄、常用宽度、最大化 |
| DPI | 100%、150%、200% |
| 主题 | Day、Night、第三方扩展主题 |
| 文本 | 中文、英文、长说明、空文本 |
| 输入 | 鼠标、纯键盘、快捷键 |
| 数据 | 空列表、单项、大量条目、错误数据 |
| 生命周期 | 首次打开、重复打开、关闭、热重载 |

### 12.10 本章练习

1. 将界面中所有面向用户的 `FString` 替换为稳定 Key 的 `FText`。
2. 在 150% DPI 和窄 Tab 下检查主题面板。
3. 不使用鼠标完成打开、切换主题、停止仿真和关闭 Tab。
4. 固定随机种子，为仿真快照编写可重复的逻辑测试。

---

## 13. SWebBrowser 与 Slate/网页混合界面

### 13.1 什么时候适合混合界面

Slate 擅长编辑器集成、原生输入、菜单、命令和普通工具控件；网页渲染擅长复杂图表、富文本和成熟 Web 可视化库。

当前插件采用的边界是：

```text
Slate/C++：主题、仿真状态、定时器、历史数据、启停按钮
                         ↓ JSON 快照
SWebBrowser/CEF：ECharts 图表渲染
```

网页只展示 C++ 提供的完整状态，不再自己生成随机数或启动独立 Timer，因此不会出现两套状态源。

### 13.2 模块依赖与初始化

`Build.cs` 需要：

```csharp
PrivateDependencyModuleNames.AddRange(new[]
{
    "WebBrowser",
    "Json",
    "Projects"
});
```

创建 `SWebBrowser` 之前确保 WebBrowser 模块已加载：

```cpp
IWebBrowserModule& WebBrowserModule = IWebBrowserModule::Get();
if (!WebBrowserModule.IsWebModuleAvailable())
{
    UE_LOG(LogTemp, Error, TEXT("WebBrowser 模块不可用。"));
}
```

缺少这一步时，某些环境中浏览器窗口可能创建失败，表现为长期停在加载动画。

### 13.3 本地资源的可靠加载策略

直接导航 `file:///.../index.html` 可能遇到 CEF 本地协议、跨域或子资源读取限制。当前插件采用完全离线的内存文档：

1. 用 `IPluginManager` 找到插件目录。
2. 读取 `Resources/Web/realtime-chart.html`。
3. 读取本地 `echarts.min.js`。
4. 把外部 `<script src>` 替换成内联脚本。
5. 使用 `ContentsToLoad` 交给 `SWebBrowser`。

```cpp
SAssignNew(ChartBrowser, SWebBrowser)
.InitialURL(TEXT("http://slate-theme-switcher.local/#text/html"))
.ContentsToLoad(TOptional<FString>(ChartDocument))
.ShowControls(false)
.ShowAddressBar(false)
.BrowserFrameRate(30)
.OnLoadCompleted(this, &SThemeSwitcherPanel::HandleChartLoaded)
.OnLoadError(this, &SThemeSwitcherPanel::HandleChartLoadError);
```

虚拟 URL 提供文档上下文，`ContentsToLoad` 才是真正内容；资源完全内联后不需要网络访问。

### 13.4 等页面 Ready 后再推送

Widget 已创建不代表 JavaScript 已加载。正确流程：

```text
创建 SWebBrowser
      ↓
CEF 加载内存页面
      ↓
OnLoadCompleted
      ↓
首次 PushSimulationData
      ↓
后续 Active Timer 推送快照
```

在加载完成前调用 `ExecuteJavascript`，脚本可能因为页面函数尚未定义而没有效果。可以保存 `bBrowserReady` 状态，失败时显示清楚的 Slate 错误提示和日志。

### 13.5 C++ 到 JavaScript 的安全数据桥

不要手工拼接包含用户文本的 JavaScript 字符串。使用 UE Json API 序列化：

```cpp
TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
Payload->SetBoolField(TEXT("running"), bSimulationRunning);
Payload->SetStringField(TEXT("themeId"), CurrentThemeId.ToString());

FString PayloadJson;
TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&PayloadJson);
FJsonSerializer::Serialize(Payload, Writer);

const FString Script = FString::Printf(
    TEXT("window.updateDashboard && window.updateDashboard(%s);"),
    *PayloadJson);

ChartBrowser->ExecuteJavascript(Script);
```

Json Serializer 会正确处理引号、换行与转义，避免脚本语法破坏和注入风险。

### 13.6 快照协议应当完整、可版本化

推荐每次推送一个自洽快照：

```json
{
  "schemaVersion": 1,
  "running": true,
  "theme": {
    "background": "#10131A",
    "text": "#F0F3F8",
    "accent": "#4DA3FF"
  },
  "stations": ["站点 A", "站点 B"],
  "timestamps": ["14:30:00.000", "14:30:00.300"],
  "load": [[42.10, 43.02], [61.22, 60.88]]
}
```

完整快照比许多零散脚本调用更容易调试。`schemaVersion` 让网页在 C++ 数据结构升级后能明确判断兼容性。

### 13.7 网页到 C++ 的边界

如果网页只负责展示，尽量让控制操作留在 Slate 中。确实需要网页回调 C++ 时，应：

- 只暴露最小接口。
- 验证所有来自网页的数据。
- 不让网页直接持有核心业务对象生命周期。
- 明确调用发生在哪个线程。
- 页面卸载和 Tab 关闭时解除绑定。

对于主题切换、启动/停止等编辑器命令，原生 Slate 按钮通常更易测试，也更符合编辑器输入和焦点体系。

### 13.8 性能与刷新频率

- 浏览器帧率不必等于编辑器最大帧率，图表常用 30 FPS 已足够。
- C++ 快照频率和网页渲染频率可以不同。
- 避免每次更新重建整个 ECharts 实例；更新 series 数据即可。
- 页面隐藏或 Tab 关闭后停止不必要的数据推送。
- 大型历史数组应限制窗口长度，当前插件使用固定 32 点历史。

### 13.9 错误处理与调试

至少记录：

- 插件目录未找到。
- HTML 或 JS 文件读取失败。
- WebBrowser 模块不可用。
- 页面加载失败。
- Json 序列化失败或协议版本不匹配。

浏览器端也应捕获初始化异常并在页面中显示可读错误。不要只留下一个永远旋转的 Loading 图标。

### 13.10 本章练习

1. 用内存 HTML 创建一个只显示“Hello Slate”的 `SWebBrowser`。
2. 从 C++ 序列化主题名和强调色，调用 JavaScript 更新页面。
3. 加入 `schemaVersion`，让网页拒绝未知协议并显示错误。
4. 关闭 Tab 后确认 Active Timer、浏览器引用和事件委托都已释放。

---

## 后续可继续追加的专题

- Asset Editor Toolkit 与自定义资源编辑器。
- 主题切换插件的完整工程实现与 UE 5.8 编译验证。
- Slate 自动化测试与性能基准实战。
- Editor Utility Widget、UMG 与原生 Slate 的混合架构。
