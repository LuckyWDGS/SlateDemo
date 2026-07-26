#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateTypes.h"
#include "Widgets/SCompoundWidget.h"

template<typename OptionType>
class SComboBox;
class SWebBrowser;
class FActiveTimerHandle;

/**
 * 主题切换演示面板。
 *
 * 面板本身不保存“当前主题”，唯一状态来源是 FSlateThemeManager。
 * 这样同一编辑器内打开多个主题控件时，它们会保持一致。
 */
class SThemeSwitcherPanel final : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SThemeSwitcherPanel) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);
    virtual ~SThemeSwitcherPanel() override;

private:
    /** 主题或主题列表改变后，重新组装面板的控件树。 */
    void RebuildContent();

    /** 把注册中心的 FName 列表转换成 SComboBox 需要的共享指针列表。 */
    void RefreshThemeOptions();

    /** 根据当前色板重建按钮样式；样式对象必须作为成员保持稳定地址。 */
    void RefreshButtonStyles();

    void HandleThemeChanged(FName PreviousThemeId, FName NewThemeId);
    void HandleThemeRegistryChanged();
    void HandleChartLoaded();
    void HandleChartLoadError();

    TSharedRef<SWidget> BuildContent();
    TSharedRef<SWidget> BuildThemeOption(TSharedPtr<FName> ThemeId) const;
    TSharedRef<SWidget> BuildChartPanel();

    FReply HandleToggleTheme();
    void HandleThemeSelected(TSharedPtr<FName> ThemeId, ESelectInfo::Type SelectInfo);
    EActiveTimerReturnType HandleSimulationTick(double InCurrentTime, float InDeltaTime);

    /** 创建站点名称和初始数值。 */
    void InitializeSimulationData();

    /** 把当前名称、数值和主题色板序列化后推送到 ECharts 页面。 */
    void PushSimulationData();

    /**
     * 读取本地 HTML 与 ECharts 脚本，并合成为一份可直接交给 CEF 的内存页面。
     *
     * 不再使用 file:/// 导航：UE 内嵌 Chromium 在部分编辑器环境会拒绝本地文件协议，
     * 表现为图表区域一直显示加载动画。内存页面同时保持运行时完全离线。
     */
    FString BuildChartDocument() const;

    /** 把 UE 线性颜色转成网页使用的 #RRGGBB。 */
    static FString ColorToCss(const FLinearColor& Color);

    /** 获取当前色板；异常情况下返回安全的静态回退色板。 */
    const struct FSlateThemePalette& GetPalette() const;
    FText GetCurrentThemeName() const;
    FText GetCurrentThemeDescription() const;
    FSlateColor GetWindowColor() const;
    FSlateColor GetPanelColor() const;
    FSlateColor GetSurfaceColor() const;
    FSlateColor GetPrimaryTextColor() const;
    FSlateColor GetSecondaryTextColor() const;
    FSlateColor GetAccentColor() const;
    FSlateColor GetAccentTextColor() const;
    FSlateColor GetAccentSubtleColor() const;
    FSlateColor GetDividerColor() const;
    FSlateColor GetSuccessColor() const;

    /** SComboBox 在生命周期内会持有 OptionsSource 地址，因此数组必须是成员变量。 */
    TArray<TSharedPtr<FName>> ThemeOptions;
    TSharedPtr<SComboBox<TSharedPtr<FName>>> ThemeComboBox;
    TSharedPtr<SWebBrowser> ChartBrowser;

    /** SButton 只保存样式指针，因此不能把 FButtonStyle 写成 BuildContent() 内的临时变量。 */
    FButtonStyle ToggleButtonStyle;
    FButtonStyle SecondaryButtonStyle;

    /** 保存委托句柄，以便析构时精确解绑当前面板自己的回调。 */
    FDelegateHandle ThemeChangedHandle;
    FDelegateHandle ThemeRegistryChangedHandle;

    /** 300ms 仿真定时器及其数据。 */
    TSharedPtr<FActiveTimerHandle> SimulationTimerHandle;
    TArray<FString> SimulationNames;
    TArray<float> SimulationValues;
    FRandomStream SimulationRandom;
};
