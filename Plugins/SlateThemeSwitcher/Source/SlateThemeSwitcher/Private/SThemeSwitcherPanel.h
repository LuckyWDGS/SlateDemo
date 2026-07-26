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
    FReply HandleToggleSimulation();
    void HandleThemeSelected(TSharedPtr<FName> ThemeId, ESelectInfo::Type SelectInfo);
    EActiveTimerReturnType HandleSimulationTick(double InCurrentTime, float InDeltaTime);

    /** 创建站点名称，以及折线图、柱状图和饼图所需的三组初始仿真数据。 */
    void InitializeSimulationData();

    /** 把当前仿真快照、运行状态和主题色板序列化后推送到 ECharts 页面。 */
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
    FText GetSimulationButtonText() const;
    FText GetSimulationStatusText() const;
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
    FSlateColor GetSimulationStatusColor() const;

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

    /**
     * 300ms 仿真定时器及其数据。
     *
     * 三组当前值与每站点历史窗口都由 C++ 更新，网页端只负责显示，
     * 避免出现 Slate 状态与图表状态不一致。
     * 停止仿真时只把 bSimulationRunning 设为 false，不清空数组，因此图表会保留最后一帧。
     */
    TSharedPtr<FActiveTimerHandle> SimulationTimerHandle;
    TArray<FString> SimulationNames;
    TArray<float> SimulationLoadValues;
    TArray<float> SimulationThroughputValues;
    TArray<float> SimulationDistributionValues;

    /** 每个站点最近一段负载历史；外层下标与 SimulationNames 一一对应。 */
    TArray<TArray<float>> SimulationLoadHistory;

    /**
     * 多站点折线图共用的采样时间轴。
     * 时间只在仿真运行时向前推进，因此暂停后横轴和所有曲线都会一起冻结。
     */
    TArray<FDateTime> SimulationHistoryTimestamps;

    FRandomStream SimulationRandom;
    int32 SimulationSampleIndex = 0;
    bool bSimulationRunning = true;
};
