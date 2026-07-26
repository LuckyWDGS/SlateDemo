#pragma once

#include "CoreMinimal.h"
#include "SlateThemeTypes.h"

/** 当前主题发生变化时广播：参数依次为旧主题 Id 和新主题 Id。 */
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnSlateThemeChanged, FName /* PreviousThemeId */, FName /* NewThemeId */);

/** 主题列表发生增删或同 Id 主题被替换时广播。 */
DECLARE_MULTICAST_DELEGATE(FOnSlateThemeRegistryChanged);

/**
 * 插件和外部 Slate 控件共用的主题注册中心。
 *
 * 本类按照注册顺序保存主题，因此 SwitchToNextTheme() 不仅能在白天/夜晚之间切换，
 * 以后增加第三、第四套主题时也无需改切换算法。
 *
 * Slate 控件可以监听 OnThemeChanged()，在主题变化后主动重绘或重建。
 * 所有注册、切换操作都应在 Game Thread（编辑器主线程）调用。
 */
class SLATETHEMESWITCHER_API FSlateThemeManager
{
public:
    /** 获取进程内唯一的主题管理器实例。 */
    static FSlateThemeManager& Get();

    /** 注册插件自带的 Day 和 Night 主题。 */
    void RegisterBuiltInThemes();

    /** 模块卸载时清空主题和委托，防止热重载后残留旧回调。 */
    void ClearThemes();

    /** 注册新主题；如果 Id 已存在，则用新定义覆盖旧定义。 */
    bool RegisterTheme(FSlateThemeDefinition InTheme, bool bSetAsCurrent = false);

    /** 注销主题。为保证始终存在有效主题，最后一个活动主题不允许被移除。 */
    bool UnregisterTheme(FName ThemeId);

    /** 按 Id 切换主题；Id 不存在时返回 false。 */
    bool SetCurrentTheme(FName ThemeId);

    /** 按注册顺序循环切换到下一套主题。 */
    bool SwitchToNextTheme();

    FName GetCurrentThemeId() const { return CurrentThemeId; }
    const FSlateThemeDefinition* GetCurrentTheme() const;
    const FSlateThemeDefinition* FindTheme(FName ThemeId) const;
    TArray<FName> GetThemeIds() const { return ThemeOrder; }

    FOnSlateThemeChanged& OnThemeChanged() { return ThemeChangedEvent; }
    FOnSlateThemeRegistryChanged& OnThemeRegistryChanged() { return ThemeRegistryChangedEvent; }

private:
    /** 用于通过 Id 快速查找主题。 */
    TMap<FName, FSlateThemeDefinition> Themes;

    /** 单独保存稳定顺序，因为 TMap 的遍历顺序不适合承担 UI 顺序。 */
    TArray<FName> ThemeOrder;
    FName CurrentThemeId = NAME_None;
    FOnSlateThemeChanged ThemeChangedEvent;
    FOnSlateThemeRegistryChanged ThemeRegistryChangedEvent;
};
