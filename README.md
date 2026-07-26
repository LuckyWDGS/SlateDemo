# SlateDemo

这是一个面向 Unreal Engine 5.8 编辑器的 Slate 示例仓库，当前包含 `SlateThemeSwitcher` 插件。

## SlateThemeSwitcher

插件路径：`Plugins/SlateThemeSwitcher`

主要功能：

- 内置白天、夜晚两套主题，可一键切换或通过下拉框选择。
- 采用可扩展主题注册表，未来可以继续注册更多主题。
- 不修改 Unreal Editor 全局 `FAppStyle`，只影响插件自己的 Slate 界面。
- 在编辑器中嵌入本地 ECharts 折线图。
- 每 300ms 生成一组平滑随机变化的仿真数据。
- X 轴显示站点名称，Y 轴显示 0～100 数值。
- 图表颜色会同步当前 Day/Night 主题。
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

