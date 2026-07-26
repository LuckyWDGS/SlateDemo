#include "SlateThemeManager.h"

#define LOCTEXT_NAMESPACE "SlateThemeManager"

namespace SlateThemeIds
{
    // 内置主题 Id 统一放在这里，避免多个位置手写字符串造成拼写不一致。
    static const FName Day(TEXT("Day"));
    static const FName Night(TEXT("Night"));
}

FSlateThemeManager& FSlateThemeManager::Get()
{
    // 函数内静态对象在第一次调用时创建，既避免手动管理生命周期，也保证只有一份注册表。
    static FSlateThemeManager Instance;
    return Instance;
}

void FSlateThemeManager::RegisterBuiltInThemes()
{
    // 白天主题：大面积使用亮色，正文保持深色以获得足够的可读性。
    FSlateThemePalette DayPalette;
    DayPalette.WindowBackground = FLinearColor(0.945f, 0.961f, 0.984f, 1.0f);
    DayPalette.PanelBackground = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);
    DayPalette.Surface = FLinearColor(0.900f, 0.925f, 0.961f, 1.0f);
    DayPalette.PrimaryText = FLinearColor(0.055f, 0.075f, 0.115f, 1.0f);
    DayPalette.SecondaryText = FLinearColor(0.310f, 0.360f, 0.445f, 1.0f);
    DayPalette.Accent = FLinearColor(0.055f, 0.365f, 0.890f, 1.0f);
    DayPalette.AccentHovered = FLinearColor(0.035f, 0.290f, 0.755f, 1.0f);
    DayPalette.AccentText = FLinearColor::White;
    DayPalette.Divider = FLinearColor(0.790f, 0.825f, 0.880f, 1.0f);
    DayPalette.Success = FLinearColor(0.030f, 0.575f, 0.300f, 1.0f);

    // 夜晚主题：降低背景亮度，同时提升正文和交互强调色的对比度。
    FSlateThemePalette NightPalette;
    NightPalette.WindowBackground = FLinearColor(0.020f, 0.027f, 0.043f, 1.0f);
    NightPalette.PanelBackground = FLinearColor(0.045f, 0.059f, 0.090f, 1.0f);
    NightPalette.Surface = FLinearColor(0.080f, 0.102f, 0.145f, 1.0f);
    NightPalette.PrimaryText = FLinearColor(0.925f, 0.945f, 0.980f, 1.0f);
    NightPalette.SecondaryText = FLinearColor(0.590f, 0.655f, 0.755f, 1.0f);
    NightPalette.Accent = FLinearColor(0.240f, 0.525f, 1.0f, 1.0f);
    NightPalette.AccentHovered = FLinearColor(0.345f, 0.620f, 1.0f, 1.0f);
    NightPalette.AccentText = FLinearColor::White;
    NightPalette.Divider = FLinearColor(0.145f, 0.180f, 0.245f, 1.0f);
    NightPalette.Success = FLinearColor(0.180f, 0.790f, 0.450f, 1.0f);

    // 第一个主题同时设为默认主题，确保面板创建时一定能取得有效色板。
    RegisterTheme(FSlateThemeDefinition(
        SlateThemeIds::Day,
        LOCTEXT("DayThemeName", "Day / 白天"),
        LOCTEXT("DayThemeDescription", "明亮、清晰，适合日间环境。"),
        DayPalette), true);

    RegisterTheme(FSlateThemeDefinition(
        SlateThemeIds::Night,
        LOCTEXT("NightThemeName", "Night / 夜晚"),
        LOCTEXT("NightThemeDescription", "低亮度、高对比，适合暗光环境。"),
        NightPalette));
}

void FSlateThemeManager::ClearThemes()
{
    // ClearThemes 只在模块卸载阶段调用。委托也必须清理，避免热重载后回调到已销毁对象。
    Themes.Reset();
    ThemeOrder.Reset();
    CurrentThemeId = NAME_None;
    ThemeChangedEvent.Clear();
    ThemeRegistryChangedEvent.Clear();
}

bool FSlateThemeManager::RegisterTheme(FSlateThemeDefinition InTheme, const bool bSetAsCurrent)
{
    // 在进入容器前做最小合法性检查，避免出现无法查找或无法显示的主题。
    if (!InTheme.IsValid())
    {
        return false;
    }

    const FName ThemeId = InTheme.Id;
    const bool bAlreadyRegistered = Themes.Contains(ThemeId);
    // TMap::Add 对已有 Key 会直接覆盖，正好支持“用相同 Id 更新主题”的场景。
    Themes.Add(ThemeId, MoveTemp(InTheme));

    if (!bAlreadyRegistered)
    {
        // 只有真正的新主题才加入顺序表；覆盖主题时仍保留原来的 UI 位置。
        ThemeOrder.Add(ThemeId);
    }

    // 注册表事件负责通知下拉列表更新，即使当前主题没有改变也需要广播。
    ThemeRegistryChangedEvent.Broadcast();

    if (CurrentThemeId.IsNone() || bSetAsCurrent)
    {
        return SetCurrentTheme(ThemeId);
    }

    // 当前主题的定义被同 Id 数据覆盖后，Id 虽未变化，颜色可能已经变化，仍需通知控件刷新。
    if (CurrentThemeId == ThemeId)
    {
        ThemeChangedEvent.Broadcast(ThemeId, ThemeId);
    }

    return true;
}

bool FSlateThemeManager::UnregisterTheme(const FName ThemeId)
{
    if (!Themes.Contains(ThemeId))
    {
        return false;
    }

    if (CurrentThemeId == ThemeId)
    {
        // 不允许删除唯一活动主题，否则所有依赖 GetCurrentTheme() 的控件都会失去有效色板。
        if (Themes.Num() <= 1)
        {
            return false;
        }

        // 删除当前主题之前先切到注册顺序中的下一项，保证切换事件里仍能查到完整数据。
        const int32 RemovedIndex = ThemeOrder.IndexOfByKey(ThemeId);
        const int32 NextIndex = (RemovedIndex + 1) % ThemeOrder.Num();
        SetCurrentTheme(ThemeOrder[NextIndex]);
    }

    Themes.Remove(ThemeId);
    ThemeOrder.Remove(ThemeId);
    ThemeRegistryChangedEvent.Broadcast();
    return true;
}

bool FSlateThemeManager::SetCurrentTheme(const FName ThemeId)
{
    // 不接受未注册 Id；调用方可以根据 false 判断是否需要回退处理。
    if (!Themes.Contains(ThemeId))
    {
        return false;
    }

    if (CurrentThemeId == ThemeId)
    {
        return true;
    }

    // 先更新状态，再广播事件，使监听者在回调里读取到的就是新主题。
    const FName PreviousThemeId = CurrentThemeId;
    CurrentThemeId = ThemeId;
    ThemeChangedEvent.Broadcast(PreviousThemeId, CurrentThemeId);
    return true;
}

bool FSlateThemeManager::SwitchToNextTheme()
{
    if (ThemeOrder.Num() == 0)
    {
        return false;
    }

    // 使用取模实现循环：到达数组末尾后会自然回到第一套主题。
    // 如果当前 Id 意外不在顺序表中，则从第一项重新开始。
    const int32 CurrentIndex = ThemeOrder.IndexOfByKey(CurrentThemeId);
    const int32 NextIndex = CurrentIndex == INDEX_NONE ? 0 : (CurrentIndex + 1) % ThemeOrder.Num();
    return SetCurrentTheme(ThemeOrder[NextIndex]);
}

const FSlateThemeDefinition* FSlateThemeManager::GetCurrentTheme() const
{
    return FindTheme(CurrentThemeId);
}

const FSlateThemeDefinition* FSlateThemeManager::FindTheme(const FName ThemeId) const
{
    return Themes.Find(ThemeId);
}

#undef LOCTEXT_NAMESPACE
