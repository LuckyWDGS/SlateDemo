using UnrealBuildTool;

public class SlateThemeSwitcher : ModuleRules
{
    public SlateThemeSwitcher(ReadOnlyTargetRules Target) : base(Target)
    {
        // 显式或共享 PCH 能让模块依赖更清楚，也符合 UE5 新插件的常见配置。
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        // 公开头文件会暴露 Slate 类型，因此这些依赖必须对引用本模块的其他模块可见。
        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "Slate",
                "SlateCore"
            });

        // InputCore 提供 SComboBox 内部使用的 EKeys；ToolMenus 用于添加 Window 菜单入口。
        // WebBrowser 承载本地 ECharts 页面，Json 用来安全地序列化推送给 JavaScript 的数据。
        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "CoreUObject",
                "InputCore",
                "Json",
                "Projects",
                "WebBrowser",
                "ToolMenus"
            });
    }
}
