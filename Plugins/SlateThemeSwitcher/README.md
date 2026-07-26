# Slate Theme Switcher

一个仅在 Unreal Editor 中运行的 Slate 示例插件，内置白天、夜晚主题，并提供完全离线的 ECharts 实时仿真数据仪表板。

## 功能

- 一键循环切换所有已注册主题，也可以通过下拉框直接选择。
- 内置白天和夜晚主题，并允许其他编辑器模块注册更多主题。
- Slate/C++ 每 300ms 生成一次有边界的随机游走数据。
- 同一份 C++ 仿真快照驱动四张 ECharts 图：
  - 实时负载折线图；
  - 五站点动态折线图：X 轴为采样时间，Y 轴为仿真数据值，站点 A～E 使用蓝、橙、绿、红、紫五种高区分度颜色，并隐藏采样点圆点；
  - 站点处理量柱状图；
  - 资源分布饼图。
- Slate 按钮可以开始或停止仿真；停止后保留最后一帧数据，再次开始时从原值继续。
- C++ 数据源先四舍五入到小数点后两位，图表标签、提示框和摘要卡片统一显示两位小数。
- ECharts 脚本存放在插件本地，不需要访问网络。
- 图表颜色随当前 Slate 主题同步更新。

## 使用方法

1. 打开项目并启用 **Slate Theme Switcher** 插件。
2. 如果 Unreal 提示需要重启编辑器，请完成重启。
3. 在编辑器菜单中选择 **Window > Slate Theme Switcher**。
4. 使用顶部按钮切换主题，使用仪表板右侧按钮开始或停止仿真。

插件只修改自己的 Slate 控件，不会覆盖 Unreal Editor 全局的 `FAppStyle`。

## 数据职责

`SThemeSwitcherPanel` 是唯一的仿真数据源：

- `SimulationLoadValues`：折线图的实时负载；
- `SimulationThroughputValues`：柱状图的处理量；
- `SimulationDistributionValues`：饼图的资源分布权重；
- `SimulationLoadHistory`：每个站点最近 32 个负载采样，用于动态多站点折线图；
- `SimulationHistoryTimestamps`：所有站点共用的 300ms 逻辑采样时间轴，停止仿真时同步冻结；
- `bSimulationRunning`：控制 ActiveTimer 是否更新数据；
- `PushSimulationData()`：把完整 JSON 快照发送给内嵌页面。

`Resources/Web/realtime-chart.html` 只负责图表绘制、主题颜色应用和两位小数格式化，不包含定时器或随机数生成逻辑。这个边界保证停止仿真时所有图表都会同时停止。

## 注册更多主题

在 `SlateThemeSwitcher` 加载完成后，其他编辑器模块可以注册新的主题定义：

```cpp
#include "SlateThemeManager.h"

FSlateThemePalette OceanPalette;
OceanPalette.WindowBackground = FLinearColor(0.01f, 0.05f, 0.08f, 1.0f);
OceanPalette.PanelBackground = FLinearColor(0.02f, 0.09f, 0.13f, 1.0f);
OceanPalette.Surface = FLinearColor(0.03f, 0.14f, 0.19f, 1.0f);
OceanPalette.PrimaryText = FLinearColor::White;
OceanPalette.SecondaryText = FLinearColor(0.55f, 0.75f, 0.80f, 1.0f);
OceanPalette.Accent = FLinearColor(0.0f, 0.65f, 0.80f, 1.0f);
OceanPalette.AccentHovered = FLinearColor(0.1f, 0.78f, 0.92f, 1.0f);
OceanPalette.AccentText = FLinearColor::White;
OceanPalette.Divider = FLinearColor(0.08f, 0.23f, 0.28f, 1.0f);
OceanPalette.Success = FLinearColor(0.2f, 0.85f, 0.55f, 1.0f);

FSlateThemeManager::Get().RegisterTheme(FSlateThemeDefinition(
    TEXT("Ocean"),
    NSLOCTEXT("MyPlugin", "OceanTheme", "Ocean"),
    NSLOCTEXT("MyPlugin", "OceanThemeDescription", "A cool blue theme."),
    OceanPalette));
```

主题列表和一键循环按钮会自动更新。自定义控件可以订阅主题变化：

```cpp
FSlateThemeManager::Get().OnThemeChanged().AddSP(this, &SMyWidget::HandleThemeChanged);
```

如果控件还会显示主题列表，请同时订阅 `OnThemeRegistryChanged()`。模块卸载时，应通过 `UnregisterTheme()` 注销自己注册的主题。
