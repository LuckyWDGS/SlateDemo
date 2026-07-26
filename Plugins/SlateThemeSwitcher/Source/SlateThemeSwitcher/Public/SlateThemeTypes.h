#pragma once

#include "CoreMinimal.h"

/**
 * 主题的“语义色板”。
 *
 * 控件只关心颜色的用途（窗口背景、正文、强调色等），不直接写死某个 RGB 值。
 * 因此以后新增主题时，只需要提供一组新的色板数据，不需要修改控件布局代码。
 */
struct SLATETHEMESWITCHER_API FSlateThemePalette
{
    /** 整个主题面板最外层的背景色。 */
    FLinearColor WindowBackground = FLinearColor::Black;

    /** 卡片、分组容器等主要面板的背景色。 */
    FLinearColor PanelBackground = FLinearColor::Black;

    /** 次一级表面，例如普通按钮、提示条和轻量容器。 */
    FLinearColor Surface = FLinearColor::Black;

    /** 标题和正文等高优先级文字颜色。 */
    FLinearColor PrimaryText = FLinearColor::White;

    /** 说明、提示等低优先级文字颜色。 */
    FLinearColor SecondaryText = FLinearColor::Gray;

    /** 主操作、选中状态使用的品牌强调色。 */
    FLinearColor Accent = FLinearColor(0.1f, 0.4f, 1.0f, 1.0f);

    /** 鼠标悬停在强调色控件上时使用的颜色。 */
    FLinearColor AccentHovered = FLinearColor(0.2f, 0.5f, 1.0f, 1.0f);

    /** 绘制在强调色背景之上的文字颜色。 */
    FLinearColor AccentText = FLinearColor::White;

    /** 分隔线、禁用态和边界的颜色。 */
    FLinearColor Divider = FLinearColor::Gray;

    /** 成功、可用、正常运行等正向状态颜色。 */
    FLinearColor Success = FLinearColor(0.1f, 0.7f, 0.35f, 1.0f);
};

/** 一份可以注册到 FSlateThemeManager 的完整主题定义。 */
struct SLATETHEMESWITCHER_API FSlateThemeDefinition
{
    /** 程序内部使用的稳定标识，例如 Day、Night；不建议用于直接显示。 */
    FName Id = NAME_None;

    /** 面向用户显示的本地化名称。 */
    FText DisplayName;

    /** 面向用户显示的主题说明。 */
    FText Description;

    /** 该主题实际提供的全部语义颜色。 */
    FSlateThemePalette Palette;

    FSlateThemeDefinition() = default;

    FSlateThemeDefinition(
        const FName InId,
        FText InDisplayName,
        FText InDescription,
        const FSlateThemePalette& InPalette)
        : Id(InId)
        , DisplayName(MoveTemp(InDisplayName))
        , Description(MoveTemp(InDescription))
        , Palette(InPalette)
    {
    }

    bool IsValid() const
    {
        // Id 用于查找主题，DisplayName 用于界面显示，两者缺一不可。
        return !Id.IsNone() && !DisplayName.IsEmpty();
    }
};
