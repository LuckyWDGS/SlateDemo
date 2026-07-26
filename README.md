# SlateDemo

这是一个面向 Unreal Engine 5.8 编辑器的 Slate 示例仓库，当前包含 `SlateThemeSwitcher` 插件。

## SlateThemeSwitcher

插件路径：`Plugins/SlateThemeSwitcher`

主要功能：

- 内置白天、夜晚两套主题，可一键切换或通过下拉框选择。
- 采用可扩展主题注册表，未来可以继续注册更多主题。
- 不修改 Unreal Editor 全局 `FAppStyle`，只影响插件自己的 Slate 界面。
- 在编辑器中嵌入完全离线的 ECharts 仪表板，包含实时折线图、动态多站点折线图、柱状图和饼图。
- Slate/C++ 每 300ms 生成一组平滑随机变化的仿真数据，网页只负责绘制。
- 动态折线图以采样时间为 X 轴、仿真数值为 Y 轴，保留最近 32 个采样。
- 站点 A～E 使用蓝、橙、绿、红、紫五种高区分度颜色，并隐藏折线采样点。
- 可以开始或停止仿真；停止后数据与时间轴冻结，再次开始时从原值继续。
- 图表颜色会同步当前 Day/Night 主题，所有显示数值保留两位小数。
- ECharts 脚本随插件本地分发，运行时不依赖网络。
- 源码包含中文注释，并附带完整 Slate 学习文档。

## 使用方式

1. 将 `Plugins/SlateThemeSwitcher` 复制到 UE5.8 项目的 `Plugins` 目录。
2. 打开项目并启用 **Slate Theme Switcher**。
3. 重启编辑器。
4. 通过 **Window > Slate Theme Switcher** 打开面板。

详细接口和扩展示例请查看：

- `Plugins/SlateThemeSwitcher/README.md`
- `Plugins/SlateThemeSwitcher/Docs/SlateLearningNotes.md`
