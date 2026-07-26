#include "SThemeSwitcherPanel.h"

#include "Brushes/SlateColorBrush.h"
#include "Dom/JsonObject.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "SlateThemeManager.h"
#include "Styling/CoreStyle.h"
#include "SWebBrowser.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SThemeSwitcherPanel"

DEFINE_LOG_CATEGORY_STATIC(LogSlateThemeChart, Log, All);

namespace
{
    /** Slate ActiveTimer 的更新间隔，同时会随数据快照发送给网页用于状态说明。 */
    constexpr float SimulationIntervalSeconds = 0.3f;

    /** 多站点折线图保留的历史采样数量，固定上限可以避免编辑器长时间运行后持续占用内存。 */
    constexpr int32 SimulationHistoryCapacity = 32;

    /**
     * 仿真数据在 C++ 数据源处就统一保留两位小数。
     * 网页端仍会使用 toFixed(2) 格式化，保证 12.5 也显示成 12.50。
     */
    float RoundToTwoDecimals(const float Value)
    {
        return FMath::RoundToFloat(Value * 100.0f) / 100.0f;
    }

    /** 将浮点数组转换为 UE JSON 数值数组，供多个图表复用同一套序列化逻辑。 */
    TArray<TSharedPtr<FJsonValue>> MakeNumberJsonArray(const TArray<float>& Values)
    {
        TArray<TSharedPtr<FJsonValue>> JsonValues;
        JsonValues.Reserve(Values.Num());
        for (const float Value : Values)
        {
            JsonValues.Add(MakeShared<FJsonValueNumber>(Value));
        }
        return JsonValues;
    }

    /** ECharts 的 time 轴使用 Unix 毫秒时间戳；保留毫秒精度才能体现 300ms 的采样间隔。 */
    double DateTimeToUnixMilliseconds(const FDateTime& DateTime)
    {
        static const FDateTime UnixEpoch(1970, 1, 1);
        return static_cast<double>((DateTime - UnixEpoch).GetTicks())
            / static_cast<double>(ETimespan::TicksPerMillisecond);
    }
}

void SThemeSwitcherPanel::Construct(const FArguments& InArgs)
{
    // AddSP 会使用当前 Slate 控件的共享指针生命周期，控件销毁后不会被委托继续调用。
    // 这里分别监听“当前主题改变”和“可用主题列表改变”，两种情况都可能影响面板内容。
    ThemeChangedHandle = FSlateThemeManager::Get().OnThemeChanged().AddSP(
        this, &SThemeSwitcherPanel::HandleThemeChanged);
    ThemeRegistryChangedHandle = FSlateThemeManager::Get().OnThemeRegistryChanged().AddSP(
        this, &SThemeSwitcherPanel::HandleThemeRegistryChanged);

    InitializeSimulationData();
    RebuildContent();

    // ActiveTimer 在普通编辑器状态下也会稳定触发，不需要依赖关卡 Tick 或 PIE。
    SimulationTimerHandle = RegisterActiveTimer(
        SimulationIntervalSeconds,
        FWidgetActiveTimerDelegate::CreateSP(this, &SThemeSwitcherPanel::HandleSimulationTick));
}

SThemeSwitcherPanel::~SThemeSwitcherPanel()
{
    // 主动解绑可以让模块热重载和编辑器关闭时的清理顺序更加明确。
    if (ThemeChangedHandle.IsValid())
    {
        FSlateThemeManager::Get().OnThemeChanged().Remove(ThemeChangedHandle);
    }
    if (ThemeRegistryChangedHandle.IsValid())
    {
        FSlateThemeManager::Get().OnThemeRegistryChanged().Remove(ThemeRegistryChangedHandle);
    }
    if (SimulationTimerHandle.IsValid())
    {
        UnRegisterActiveTimer(SimulationTimerHandle.ToSharedRef());
        SimulationTimerHandle.Reset();
    }
}

void SThemeSwitcherPanel::RebuildContent()
{
    // SComboBox 引用了 ThemeOptions，因此先释放旧控件，再更新选项数组。
    ThemeComboBox.Reset();
    ChartBrowser.Reset();
    RefreshThemeOptions();
    RefreshButtonStyles();

    // 主题切换频率很低，直接重建控件树比为每个样式字段维护复杂绑定更清晰、可靠。
    ChildSlot.AttachWidget(BuildContent());
}

void SThemeSwitcherPanel::RefreshThemeOptions()
{
    ThemeOptions.Reset();
    for (const FName ThemeId : FSlateThemeManager::Get().GetThemeIds())
    {
        // SComboBox 的 OptionType 通常使用共享指针，保证弹出列表绘制期间选项地址稳定。
        ThemeOptions.Add(MakeShared<FName>(ThemeId));
    }
}

void SThemeSwitcherPanel::RefreshButtonStyles()
{
    const FSlateThemePalette& Palette = GetPalette();

    // 主按钮使用 Accent 系列颜色，承担“一键切换”这一最重要操作。
    ToggleButtonStyle = FButtonStyle()
        .SetNormal(FSlateColorBrush(Palette.Accent))
        .SetHovered(FSlateColorBrush(Palette.AccentHovered))
        .SetPressed(FSlateColorBrush(Palette.AccentHovered * 0.82f))
        .SetDisabled(FSlateColorBrush(Palette.Divider))
        .SetNormalPadding(FMargin(18.0f, 10.0f))
        .SetPressedPadding(FMargin(18.0f, 11.0f, 18.0f, 9.0f));

    // 次按钮弱化视觉层级，主要用于展示普通 Surface/Divider 的效果。
    SecondaryButtonStyle = FButtonStyle()
        .SetNormal(FSlateColorBrush(Palette.Surface))
        .SetHovered(FSlateColorBrush(Palette.Divider))
        .SetPressed(FSlateColorBrush(Palette.Divider * 0.85f))
        .SetDisabled(FSlateColorBrush(Palette.Divider))
        .SetNormalPadding(FMargin(14.0f, 8.0f))
        .SetPressedPadding(FMargin(14.0f, 9.0f, 14.0f, 7.0f));
}

void SThemeSwitcherPanel::HandleThemeChanged(FName PreviousThemeId, FName NewThemeId)
{
    // 除了重建子控件，还显式使布局和易变性缓存失效，确保当前帧之后立即重绘。
    RebuildContent();
    Invalidate(EInvalidateWidgetReason::LayoutAndVolatility);
}

void SThemeSwitcherPanel::HandleThemeRegistryChanged()
{
    RebuildContent();
    Invalidate(EInvalidateWidgetReason::LayoutAndVolatility);
}

TSharedRef<SWidget> SThemeSwitcherPanel::BuildContent()
{
    // InitiallySelectedItem 需要传入 OptionsSource 中的同一个共享指针实例，
    // 所以不能在这里临时 MakeShared，而要先从 ThemeOptions 中找到对应项。
    TSharedPtr<FName> SelectedTheme;
    const FName CurrentThemeId = FSlateThemeManager::Get().GetCurrentThemeId();
    for (const TSharedPtr<FName>& Option : ThemeOptions)
    {
        if (Option.IsValid() && *Option == CurrentThemeId)
        {
            SelectedTheme = Option;
            break;
        }
    }

    // 控件树结构：
    // 外层主题背景 -> 可滚动内容 -> 标题/切换按钮 -> 当前主题与组件预览 -> 扩展提示。
    return SNew(SBorder)
        .BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
        .BorderBackgroundColor(this, &SThemeSwitcherPanel::GetWindowColor)
        .Padding(0.0f)
        [
            SNew(SScrollBox)
            + SScrollBox::Slot()
            [
                SNew(SVerticalBox)

                // 顶部区域：插件标题和最主要的一键切换按钮。
                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(24.0f, 24.0f, 24.0f, 12.0f)
                [
                    SNew(SBorder)
                    .BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
                    .BorderBackgroundColor(this, &SThemeSwitcherPanel::GetPanelColor)
                    .Padding(22.0f)
                    [
                        SNew(SHorizontalBox)

                        + SHorizontalBox::Slot()
                        .FillWidth(1.0f)
                        .VAlign(VAlign_Center)
                        [
                            SNew(SVerticalBox)
                            + SVerticalBox::Slot()
                            .AutoHeight()
                            [
                                SNew(STextBlock)
                                .Text(LOCTEXT("Title", "Slate Theme Switcher"))
                                .Font(FCoreStyle::GetDefaultFontStyle("Bold", 22))
                                .ColorAndOpacity(this, &SThemeSwitcherPanel::GetPrimaryTextColor)
                            ]
                            + SVerticalBox::Slot()
                            .AutoHeight()
                            .Padding(0.0f, 6.0f, 0.0f, 0.0f)
                            [
                                SNew(STextBlock)
                                .Text(LOCTEXT("Subtitle", "语义色板 · 一键切换 · 可扩展主题注册表"))
                                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
                                .ColorAndOpacity(this, &SThemeSwitcherPanel::GetSecondaryTextColor)
                            ]
                        ]

                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        .VAlign(VAlign_Center)
                        .Padding(20.0f, 0.0f, 0.0f, 0.0f)
                        [
                            SNew(SButton)
                            .ButtonStyle(&ToggleButtonStyle)
                            .ToolTipText(LOCTEXT("ToggleTooltip", "切换到下一个已注册主题"))
                            .OnClicked(this, &SThemeSwitcherPanel::HandleToggleTheme)
                            [
                                SNew(STextBlock)
                                .Text(LOCTEXT("ToggleButton", "切换主题  ↻"))
                                .Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
                                .ColorAndOpacity(this, &SThemeSwitcherPanel::GetAccentTextColor)
                            ]
                        ]
                    ]
                ]

                // 中部区域：左侧主题信息与选择器，右侧展示语义颜色在常见控件中的效果。
                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(24.0f, 0.0f, 24.0f, 12.0f)
                [
                    SNew(SHorizontalBox)

                    + SHorizontalBox::Slot()
                    .FillWidth(1.0f)
                    .Padding(0.0f, 0.0f, 6.0f, 0.0f)
                    [
                        SNew(SBorder)
                        .BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
                        .BorderBackgroundColor(this, &SThemeSwitcherPanel::GetPanelColor)
                        .Padding(20.0f)
                        [
                            SNew(SVerticalBox)
                            + SVerticalBox::Slot()
                            .AutoHeight()
                            [
                                SNew(STextBlock)
                                .Text(LOCTEXT("CurrentThemeLabel", "当前主题"))
                                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
                                .ColorAndOpacity(this, &SThemeSwitcherPanel::GetSecondaryTextColor)
                            ]
                            + SVerticalBox::Slot()
                            .AutoHeight()
                            .Padding(0.0f, 8.0f, 0.0f, 0.0f)
                            [
                                SNew(STextBlock)
                                .Text(this, &SThemeSwitcherPanel::GetCurrentThemeName)
                                .Font(FCoreStyle::GetDefaultFontStyle("Bold", 17))
                                .ColorAndOpacity(this, &SThemeSwitcherPanel::GetPrimaryTextColor)
                            ]
                            + SVerticalBox::Slot()
                            .AutoHeight()
                            .Padding(0.0f, 6.0f, 0.0f, 14.0f)
                            [
                                SNew(STextBlock)
                                .Text(this, &SThemeSwitcherPanel::GetCurrentThemeDescription)
                                .AutoWrapText(true)
                                .ColorAndOpacity(this, &SThemeSwitcherPanel::GetSecondaryTextColor)
                            ]
                            + SVerticalBox::Slot()
                            .AutoHeight()
                            [
                                SAssignNew(ThemeComboBox, SComboBox<TSharedPtr<FName>>)
                                .OptionsSource(&ThemeOptions)
                                .InitiallySelectedItem(SelectedTheme)
                                .OnGenerateWidget(this, &SThemeSwitcherPanel::BuildThemeOption)
                                .OnSelectionChanged(this, &SThemeSwitcherPanel::HandleThemeSelected)
                                [
                                    SNew(STextBlock)
                                    .Text(this, &SThemeSwitcherPanel::GetCurrentThemeName)
                                    .ColorAndOpacity(this, &SThemeSwitcherPanel::GetPrimaryTextColor)
                                ]
                            ]
                        ]
                    ]

                    + SHorizontalBox::Slot()
                    .FillWidth(1.0f)
                    .Padding(6.0f, 0.0f, 0.0f, 0.0f)
                    [
                        SNew(SBorder)
                        .BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
                        .BorderBackgroundColor(this, &SThemeSwitcherPanel::GetPanelColor)
                        .Padding(20.0f)
                        [
                            SNew(SVerticalBox)
                            + SVerticalBox::Slot()
                            .AutoHeight()
                            [
                                SNew(STextBlock)
                                .Text(LOCTEXT("ComponentPreview", "组件预览"))
                                .Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))
                                .ColorAndOpacity(this, &SThemeSwitcherPanel::GetPrimaryTextColor)
                            ]
                            + SVerticalBox::Slot()
                            .AutoHeight()
                            .Padding(0.0f, 12.0f)
                            [
                                SNew(SSeparator)
                                .ColorAndOpacity(this, &SThemeSwitcherPanel::GetDividerColor)
                            ]
                            + SVerticalBox::Slot()
                            .AutoHeight()
                            [
                                SNew(SBorder)
                                .BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
                                .BorderBackgroundColor(this, &SThemeSwitcherPanel::GetAccentSubtleColor)
                                .Padding(12.0f)
                                [
                                    SNew(SHorizontalBox)
                                    + SHorizontalBox::Slot()
                                    .AutoWidth()
                                    .VAlign(VAlign_Center)
                                    [
                                        SNew(SBox)
                                        .WidthOverride(8.0f)
                                        .HeightOverride(8.0f)
                                        [
                                            SNew(SBorder)
                                            .BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
                                            .BorderBackgroundColor(this, &SThemeSwitcherPanel::GetSuccessColor)
                                        ]
                                    ]
                                    + SHorizontalBox::Slot()
                                    .FillWidth(1.0f)
                                    .VAlign(VAlign_Center)
                                    .Padding(10.0f, 0.0f)
                                    [
                                        SNew(STextBlock)
                                        .Text(LOCTEXT("StatusReady", "主题系统运行正常"))
                                        .ColorAndOpacity(this, &SThemeSwitcherPanel::GetPrimaryTextColor)
                                    ]
                                ]
                            ]
                            + SVerticalBox::Slot()
                            .AutoHeight()
                            .Padding(0.0f, 12.0f, 0.0f, 0.0f)
                            [
                                SNew(SButton)
                                .ButtonStyle(&SecondaryButtonStyle)
                                [
                                    SNew(STextBlock)
                                    .Text(LOCTEXT("SecondaryAction", "次要操作"))
                                    .ColorAndOpacity(this, &SThemeSwitcherPanel::GetPrimaryTextColor)
                                ]
                            ]
                            + SVerticalBox::Slot()
                            .AutoHeight()
                            .Padding(0.0f, 8.0f, 0.0f, 0.0f)
                            [
                                SNew(STextBlock)
                                .Text(LOCTEXT("PreviewHint", "所有颜色来自当前主题的语义色板。"))
                                .ColorAndOpacity(this, &SThemeSwitcherPanel::GetSecondaryTextColor)
                            ]
                        ]
                    ]
                ]

                // 多图表仿真仪表板：所有图表数据都由本 Slate 控件生成并统一推送。
                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(24.0f, 0.0f, 24.0f, 12.0f)
                [
                    BuildChartPanel()
                ]

                // 底部说明：提醒维护者通过注册主题数据进行扩展，而不是修改本面板的分支逻辑。
                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(24.0f, 0.0f, 24.0f, 24.0f)
                [
                    SNew(SBorder)
                    .BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
                    .BorderBackgroundColor(this, &SThemeSwitcherPanel::GetSurfaceColor)
                    .Padding(14.0f)
                    [
                        SNew(STextBlock)
                        .Text(LOCTEXT("ExtensionHint", "扩展方式：创建 FSlateThemeDefinition → RegisterTheme() → UI 自动进入循环与下拉列表。"))
                        .AutoWrapText(true)
                        .ColorAndOpacity(this, &SThemeSwitcherPanel::GetSecondaryTextColor)
                    ]
                ]
            ]
        ];
}

TSharedRef<SWidget> SThemeSwitcherPanel::BuildThemeOption(TSharedPtr<FName> ThemeId) const
{
    // 下拉项只保存主题 Id，显示文本在生成时向管理器查询，避免复制整份主题数据。
    const FSlateThemeDefinition* Theme = ThemeId.IsValid()
        ? FSlateThemeManager::Get().FindTheme(*ThemeId)
        : nullptr;

    return SNew(STextBlock)
        .Text(Theme ? Theme->DisplayName : LOCTEXT("UnknownTheme", "未知主题"))
        .ColorAndOpacity(this, &SThemeSwitcherPanel::GetPrimaryTextColor);
}

TSharedRef<SWidget> SThemeSwitcherPanel::BuildChartPanel()
{
    // ContentsToLoad 会让 CEF 直接读取内存中的网页内容，避免 Windows/CEF 对 file:/// 的限制。
    // Dummy URL 末尾的 #text/html 是 UE WebBrowser 约定的 MIME 类型提示，不会发起网络请求。
    const FString ChartDocument = BuildChartDocument();

    return SNew(SBorder)
        .BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
        .BorderBackgroundColor(this, &SThemeSwitcherPanel::GetPanelColor)
        .Padding(18.0f)
        [
            SNew(SVerticalBox)

            // 控制条属于真正的 Slate UI；网页只接收状态和数据，不能自行启停仿真。
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(2.0f, 0.0f, 2.0f, 14.0f)
            [
                SNew(SHorizontalBox)

                + SHorizontalBox::Slot()
                .FillWidth(1.0f)
                .VAlign(VAlign_Center)
                [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        SNew(STextBlock)
                        .Text(LOCTEXT("DashboardTitle", "Slate 仿真数据仪表板"))
                        .Font(FCoreStyle::GetDefaultFontStyle("Bold", 15))
                        .ColorAndOpacity(this, &SThemeSwitcherPanel::GetPrimaryTextColor)
                    ]
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, 6.0f, 0.0f, 0.0f)
                    [
                        SNew(SHorizontalBox)
                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        .VAlign(VAlign_Center)
                        [
                            SNew(SBox)
                            .WidthOverride(8.0f)
                            .HeightOverride(8.0f)
                            [
                                SNew(SBorder)
                                .BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
                                .BorderBackgroundColor(this, &SThemeSwitcherPanel::GetSimulationStatusColor)
                            ]
                        ]
                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        .VAlign(VAlign_Center)
                        .Padding(8.0f, 0.0f, 0.0f, 0.0f)
                        [
                            SNew(STextBlock)
                            .Text(this, &SThemeSwitcherPanel::GetSimulationStatusText)
                            .ColorAndOpacity(this, &SThemeSwitcherPanel::GetSecondaryTextColor)
                        ]
                    ]
                ]

                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                .Padding(16.0f, 0.0f, 0.0f, 0.0f)
                [
                    SNew(SButton)
                    .ButtonStyle(&SecondaryButtonStyle)
                    .ToolTipText(LOCTEXT("SimulationToggleTooltip", "暂停时保留最后一帧数据；再次开始会从当前值继续仿真"))
                    .OnClicked(this, &SThemeSwitcherPanel::HandleToggleSimulation)
                    [
                        SNew(STextBlock)
                        .Text(this, &SThemeSwitcherPanel::GetSimulationButtonText)
                        .Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
                        .ColorAndOpacity(this, &SThemeSwitcherPanel::GetPrimaryTextColor)
                    ]
                ]
            ]

            + SVerticalBox::Slot()
            .AutoHeight()
            [
                SNew(SBox)
                // 第四张多站点折线图需要更大的垂直空间；窄面板下网页内部会改为纵向堆叠。
                .HeightOverride(980.0f)
                [
                    SAssignNew(ChartBrowser, SWebBrowser)
                    .InitialURL(TEXT("http://slatethemeswitcher.local/realtime-chart.html#text/html"))
                    .ContentsToLoad(TOptional<FString>(ChartDocument))
                    .ShowControls(false)
                    .ShowAddressBar(false)
                    .ShowErrorMessage(true)
                    .SupportsTransparency(false)
                    .ShowInitialThrobber(true)
                    .BackgroundColor(GetPalette().WindowBackground.ToFColorSRGB())
                    .BrowserFrameRate(30)
                    .OnLoadCompleted(FSimpleDelegate::CreateSP(this, &SThemeSwitcherPanel::HandleChartLoaded))
                    .OnLoadError(FSimpleDelegate::CreateSP(this, &SThemeSwitcherPanel::HandleChartLoadError))
                ]
            ]
        ];
}

void SThemeSwitcherPanel::InitializeSimulationData()
{
    // 名称是固定分类；三组数值分别服务于折线图、柱状图和饼图。
    // 每组数据后续都采用有边界的随机游走，避免每 300ms 完全跳到无关位置。
    SimulationNames = {
        TEXT("站点 A"),
        TEXT("站点 B"),
        TEXT("站点 C"),
        TEXT("站点 D"),
        TEXT("站点 E")
    };

    SimulationRandom.Initialize(static_cast<int32>(FPlatformTime::Cycles64()));
    SimulationLoadValues.Reset(SimulationNames.Num());
    SimulationThroughputValues.Reset(SimulationNames.Num());
    SimulationDistributionValues.Reset(SimulationNames.Num());
    SimulationLoadHistory.Reset(SimulationNames.Num());
    SimulationLoadHistory.SetNum(SimulationNames.Num());
    SimulationHistoryTimestamps.Reset(SimulationHistoryCapacity);

    // 先建立一段等间隔的时间轴，让页面首次打开时就能看到完整的动态窗口。
    // 后续每次 Tick 只追加 300ms；暂停期间不推进时间，恢复时不会产生突兀的大空档。
    const FDateTime InitialHistoryEnd = FDateTime::Now();
    for (int32 HistoryIndex = 0; HistoryIndex < SimulationHistoryCapacity; ++HistoryIndex)
    {
        const int32 MillisecondsBeforeEnd =
            (SimulationHistoryCapacity - 1 - HistoryIndex)
            * FMath::RoundToInt(SimulationIntervalSeconds * 1000.0f);
        SimulationHistoryTimestamps.Add(
            InitialHistoryEnd - FTimespan::FromMilliseconds(MillisecondsBeforeEnd));
    }

    for (int32 Index = 0; Index < SimulationNames.Num(); ++Index)
    {
        // 先生成一段初始历史，让多站点折线图首次打开时就有完整曲线，而不是只有一个点。
        float HistoryValue = SimulationRandom.FRandRange(30.0f, 72.0f);
        TArray<float>& StationHistory = SimulationLoadHistory[Index];
        StationHistory.Reserve(SimulationHistoryCapacity);
        for (int32 HistoryIndex = 0; HistoryIndex < SimulationHistoryCapacity; ++HistoryIndex)
        {
            HistoryValue = RoundToTwoDecimals(FMath::Clamp(
                HistoryValue + SimulationRandom.FRandRange(-4.0f, 4.0f),
                5.0f,
                95.0f));
            StationHistory.Add(HistoryValue);
        }

        SimulationLoadValues.Add(StationHistory.Last());
        SimulationThroughputValues.Add(RoundToTwoDecimals(SimulationRandom.FRandRange(55.0f, 165.0f)));
        SimulationDistributionValues.Add(RoundToTwoDecimals(SimulationRandom.FRandRange(12.0f, 48.0f)));
    }

    SimulationSampleIndex = 0;
    bSimulationRunning = true;
}

EActiveTimerReturnType SThemeSwitcherPanel::HandleSimulationTick(double InCurrentTime, float InDeltaTime)
{
    // ActiveTimer 始终保留，停止时只跳过数值更新。这样无需反复注册/注销定时器，
    // 也能保证最后一帧数据一直保存在 C++ 数组和 ECharts 页面中。
    if (!bSimulationRunning)
    {
        return EActiveTimerReturnType::Continue;
    }

    for (int32 Index = 0; Index < SimulationNames.Num(); ++Index)
    {
        const float LoadDelta = SimulationRandom.FRandRange(-5.0f, 5.0f);
        SimulationLoadValues[Index] = RoundToTwoDecimals(FMath::Clamp(
            SimulationLoadValues[Index] + LoadDelta,
            5.0f,
            95.0f));

        // 处理量会受到当前负载的轻微影响，同时叠加随机扰动，让两张图有关联但不完全相同。
        const float ThroughputDelta = SimulationRandom.FRandRange(-9.0f, 9.0f)
            + (SimulationLoadValues[Index] - 50.0f) * 0.04f;
        SimulationThroughputValues[Index] = RoundToTwoDecimals(FMath::Clamp(
            SimulationThroughputValues[Index] + ThroughputDelta,
            20.0f,
            220.0f));

        const float DistributionDelta = SimulationRandom.FRandRange(-3.0f, 3.0f);
        SimulationDistributionValues[Index] = RoundToTwoDecimals(FMath::Clamp(
            SimulationDistributionValues[Index] + DistributionDelta,
            5.0f,
            80.0f));

        // 每个站点只保留固定数量的最近采样，既形成连续曲线，又不会无限增长。
        TArray<float>& StationHistory = SimulationLoadHistory[Index];
        StationHistory.Add(SimulationLoadValues[Index]);
        if (StationHistory.Num() > SimulationHistoryCapacity)
        {
            StationHistory.RemoveAt(
                0,
                StationHistory.Num() - SimulationHistoryCapacity,
                EAllowShrinking::No);
        }
    }

    // 所有站点共用同一时间轴。只在完成本轮所有数值更新后追加一个时间点，
    // 这样每条站点曲线的第 N 个值都会严格对应同一个采样时刻。
    const FDateTime NextSampleTime = SimulationHistoryTimestamps.IsEmpty()
        ? FDateTime::Now()
        : SimulationHistoryTimestamps.Last()
            + FTimespan::FromMilliseconds(SimulationIntervalSeconds * 1000.0f);
    SimulationHistoryTimestamps.Add(NextSampleTime);
    if (SimulationHistoryTimestamps.Num() > SimulationHistoryCapacity)
    {
        SimulationHistoryTimestamps.RemoveAt(
            0,
            SimulationHistoryTimestamps.Num() - SimulationHistoryCapacity,
            EAllowShrinking::No);
    }

    ++SimulationSampleIndex;
    PushSimulationData();
    return EActiveTimerReturnType::Continue;
}

void SThemeSwitcherPanel::HandleChartLoaded()
{
    // HTML 和本地 ECharts 脚本加载完成后立即推送一次 C++ 侧的真实初始数据。
    UE_LOG(LogSlateThemeChart, Display, TEXT("实时 ECharts 内存页面加载完成。"));
    PushSimulationData();
}

void SThemeSwitcherPanel::HandleChartLoadError()
{
    UE_LOG(LogSlateThemeChart, Error, TEXT("无法加载实时 ECharts 内存页面。"));
}

void SThemeSwitcherPanel::PushSimulationData()
{
    if (!ChartBrowser.IsValid() || !ChartBrowser->IsLoaded())
    {
        return;
    }

    TArray<TSharedPtr<FJsonValue>> NameJsonValues;
    NameJsonValues.Reserve(SimulationNames.Num());
    for (const FString& Name : SimulationNames)
    {
        NameJsonValues.Add(MakeShared<FJsonValueString>(Name));
    }

    const FSlateThemePalette& Palette = GetPalette();
    const TSharedRef<FJsonObject> PaletteObject = MakeShared<FJsonObject>();
    PaletteObject->SetStringField(TEXT("background"), ColorToCss(Palette.WindowBackground));
    PaletteObject->SetStringField(TEXT("surface"), ColorToCss(Palette.Surface));
    PaletteObject->SetStringField(TEXT("primaryText"), ColorToCss(Palette.PrimaryText));
    PaletteObject->SetStringField(TEXT("secondaryText"), ColorToCss(Palette.SecondaryText));
    PaletteObject->SetStringField(TEXT("accent"), ColorToCss(Palette.Accent));
    PaletteObject->SetStringField(TEXT("success"), ColorToCss(Palette.Success));
    PaletteObject->SetStringField(TEXT("divider"), ColorToCss(Palette.Divider));

    // 汇总值同样在 Slate/C++ 侧计算，网页没有任何业务数据生成逻辑。
    float TotalLoad = 0.0f;
    float PeakLoad = 0.0f;
    float TotalThroughput = 0.0f;
    float TotalDistribution = 0.0f;
    for (int32 Index = 0; Index < SimulationNames.Num(); ++Index)
    {
        TotalLoad += SimulationLoadValues[Index];
        PeakLoad = FMath::Max(PeakLoad, SimulationLoadValues[Index]);
        TotalThroughput += SimulationThroughputValues[Index];
        TotalDistribution += SimulationDistributionValues[Index];
    }

    const float AverageLoad = SimulationLoadValues.IsEmpty()
        ? 0.0f
        : TotalLoad / static_cast<float>(SimulationLoadValues.Num());

    const TSharedRef<FJsonObject> SummaryObject = MakeShared<FJsonObject>();
    SummaryObject->SetNumberField(TEXT("averageLoad"), RoundToTwoDecimals(AverageLoad));
    SummaryObject->SetNumberField(TEXT("peakLoad"), RoundToTwoDecimals(PeakLoad));
    SummaryObject->SetNumberField(TEXT("totalThroughput"), RoundToTwoDecimals(TotalThroughput));
    SummaryObject->SetNumberField(TEXT("totalDistribution"), RoundToTwoDecimals(TotalDistribution));

    // 动态多折线图使用真实 time 轴。网页只把时间戳和值组合成 ECharts 数据点，
    // 不创建定时器，也不自行推进时间，从而与 Slate 的开始/停止状态完全同步。
    TArray<TSharedPtr<FJsonValue>> HistoryTimestampJsonValues;
    HistoryTimestampJsonValues.Reserve(SimulationHistoryTimestamps.Num());
    for (const FDateTime& Timestamp : SimulationHistoryTimestamps)
    {
        HistoryTimestampJsonValues.Add(
            MakeShared<FJsonValueNumber>(DateTimeToUnixMilliseconds(Timestamp)));
    }

    TArray<TSharedPtr<FJsonValue>> HistorySeriesJsonValues;
    HistorySeriesJsonValues.Reserve(SimulationNames.Num());
    for (int32 StationIndex = 0; StationIndex < SimulationNames.Num(); ++StationIndex)
    {
        const TArray<float>& StationHistory = SimulationLoadHistory[StationIndex];

        const TSharedRef<FJsonObject> SeriesObject = MakeShared<FJsonObject>();
        SeriesObject->SetStringField(TEXT("name"), SimulationNames[StationIndex]);
        SeriesObject->SetArrayField(TEXT("values"), MakeNumberJsonArray(StationHistory));
        HistorySeriesJsonValues.Add(MakeShared<FJsonValueObject>(SeriesObject));
    }

    // 使用单个 JSON 快照作为 C++ 与网页的协议，未来增加更多图表时只需扩展字段。
    const TSharedRef<FJsonObject> PayloadObject = MakeShared<FJsonObject>();
    PayloadObject->SetBoolField(TEXT("running"), bSimulationRunning);
    PayloadObject->SetNumberField(TEXT("intervalMs"), SimulationIntervalSeconds * 1000.0f);
    PayloadObject->SetNumberField(TEXT("sampleIndex"), SimulationSampleIndex);
    PayloadObject->SetArrayField(TEXT("names"), NameJsonValues);
    PayloadObject->SetArrayField(TEXT("loadValues"), MakeNumberJsonArray(SimulationLoadValues));
    PayloadObject->SetArrayField(TEXT("throughputValues"), MakeNumberJsonArray(SimulationThroughputValues));
    PayloadObject->SetArrayField(TEXT("distributionValues"), MakeNumberJsonArray(SimulationDistributionValues));
    PayloadObject->SetNumberField(TEXT("historyCapacity"), SimulationHistoryCapacity);
    PayloadObject->SetArrayField(TEXT("historyTimestamps"), HistoryTimestampJsonValues);
    PayloadObject->SetArrayField(TEXT("stationHistorySeries"), HistorySeriesJsonValues);
    PayloadObject->SetObjectField(TEXT("summary"), SummaryObject);
    PayloadObject->SetObjectField(TEXT("palette"), PaletteObject);

    FString PayloadJson;
    const TSharedRef<TJsonWriter<>> PayloadWriter = TJsonWriterFactory<>::Create(&PayloadJson);
    FJsonSerializer::Serialize(PayloadObject, PayloadWriter);

    const FString Script = FString::Printf(
        TEXT("window.updateSimulationDashboard && window.updateSimulationDashboard(%s);"),
        *PayloadJson);
    ChartBrowser->ExecuteJavascript(Script);
}

FString SThemeSwitcherPanel::BuildChartDocument() const
{
    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("SlateThemeSwitcher"));
    if (!Plugin.IsValid())
    {
        UE_LOG(LogSlateThemeChart, Error, TEXT("找不到 SlateThemeSwitcher 插件目录。"));
        return TEXT("<!doctype html><meta charset=\"utf-8\"><body>ECharts 插件目录不可用，请查看输出日志。</body>");
    }

    const FString HtmlPath = FPaths::ConvertRelativePathToFull(
        FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/Web/realtime-chart.html")));
    const FString EChartsPath = FPaths::ConvertRelativePathToFull(
        FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/Web/echarts.min.js")));

    FString HtmlDocument;
    if (!FFileHelper::LoadFileToString(HtmlDocument, *HtmlPath))
    {
        UE_LOG(LogSlateThemeChart, Error, TEXT("无法读取实时图表页面：%s"), *HtmlPath);
        return TEXT("<!doctype html><meta charset=\"utf-8\"><body>图表 HTML 资源读取失败，请查看输出日志。</body>");
    }

    FString EChartsScript;
    if (!FFileHelper::LoadFileToString(EChartsScript, *EChartsPath))
    {
        UE_LOG(LogSlateThemeChart, Error, TEXT("无法读取本地 ECharts 脚本：%s"), *EChartsPath);
        return TEXT("<!doctype html><meta charset=\"utf-8\"><body>ECharts 脚本读取失败，请查看输出日志。</body>");
    }

    // 将外部脚本标签替换为内联脚本，确保页面不再产生任何 file:/// 子资源请求。
    const FString ExternalScriptTag = TEXT("<script src=\"echarts.min.js\"></script>");
    const FString InlineScriptTag = FString::Printf(TEXT("<script>\n%s\n</script>"), *EChartsScript);
    const int32 ReplacementCount = HtmlDocument.ReplaceInline(
        *ExternalScriptTag,
        *InlineScriptTag,
        ESearchCase::CaseSensitive);

    if (ReplacementCount != 1)
    {
        UE_LOG(
            LogSlateThemeChart,
            Error,
            TEXT("实时图表 HTML 中的 ECharts 脚本标签数量异常，期望 1，实际 %d。"),
            ReplacementCount);
        return TEXT("<!doctype html><meta charset=\"utf-8\"><body>ECharts 页面结构异常，请查看输出日志。</body>");
    }

    UE_LOG(
        LogSlateThemeChart,
        Display,
        TEXT("已生成内存图表页面：HTML %d 字符，ECharts %d 字符。"),
        HtmlDocument.Len(),
        EChartsScript.Len());
    return HtmlDocument;
}

FString SThemeSwitcherPanel::ColorToCss(const FLinearColor& Color)
{
    const FColor Srgb = Color.ToFColorSRGB();
    return FString::Printf(TEXT("#%02X%02X%02X"), Srgb.R, Srgb.G, Srgb.B);
}

FReply SThemeSwitcherPanel::HandleToggleTheme()
{
    // 面板不判断 Day/Night；由管理器按照注册顺序决定下一项，因此天然兼容更多主题。
    FSlateThemeManager::Get().SwitchToNextTheme();
    return FReply::Handled();
}

FReply SThemeSwitcherPanel::HandleToggleSimulation()
{
    bSimulationRunning = !bSimulationRunning;

    // 状态切换本身也立即推送一次，使网页状态标签与 Slate 按钮在同一帧更新。
    // 停止时不会修改任何数组，因此四个图表都会保留最后一次采样结果。
    PushSimulationData();
    Invalidate(EInvalidateWidgetReason::Paint);

    UE_LOG(
        LogSlateThemeChart,
        Display,
        TEXT("Slate 仿真已%s，当前采样序号：%d。"),
        bSimulationRunning ? TEXT("开始") : TEXT("停止"),
        SimulationSampleIndex);
    return FReply::Handled();
}

void SThemeSwitcherPanel::HandleThemeSelected(TSharedPtr<FName> ThemeId, ESelectInfo::Type SelectInfo)
{
    // SelectInfo 可用于区分鼠标、键盘或代码选择；当前功能对所有来源采用相同行为。
    if (ThemeId.IsValid())
    {
        FSlateThemeManager::Get().SetCurrentTheme(*ThemeId);
    }
}

const FSlateThemePalette& SThemeSwitcherPanel::GetPalette() const
{
    if (const FSlateThemeDefinition* Theme = FSlateThemeManager::Get().GetCurrentTheme())
    {
        return Theme->Palette;
    }

    // 正常启动流程不会走到这里；静态回退值用于保护模块初始化/卸载边界情况。
    static const FSlateThemePalette FallbackPalette;
    return FallbackPalette;
}

FText SThemeSwitcherPanel::GetCurrentThemeName() const
{
    if (const FSlateThemeDefinition* Theme = FSlateThemeManager::Get().GetCurrentTheme())
    {
        return Theme->DisplayName;
    }
    return LOCTEXT("NoTheme", "未选择主题");
}

FText SThemeSwitcherPanel::GetCurrentThemeDescription() const
{
    if (const FSlateThemeDefinition* Theme = FSlateThemeManager::Get().GetCurrentTheme())
    {
        return Theme->Description;
    }
    return FText::GetEmpty();
}

FText SThemeSwitcherPanel::GetSimulationButtonText() const
{
    return bSimulationRunning
        ? LOCTEXT("StopSimulationButton", "停止仿真  ■")
        : LOCTEXT("StartSimulationButton", "开始仿真  ▶");
}

FText SThemeSwitcherPanel::GetSimulationStatusText() const
{
    return bSimulationRunning
        ? LOCTEXT("SimulationRunningStatus", "仿真运行中 · 每 300 ms 更新")
        : LOCTEXT("SimulationStoppedStatus", "仿真已停止 · 当前数据已保留");
}

// 这些小型 Getter 作为 Slate Attribute 绑定使用；控件重绘时会读取当前色板，而不是缓存旧颜色。
FSlateColor SThemeSwitcherPanel::GetWindowColor() const { return GetPalette().WindowBackground; }
FSlateColor SThemeSwitcherPanel::GetPanelColor() const { return GetPalette().PanelBackground; }
FSlateColor SThemeSwitcherPanel::GetSurfaceColor() const { return GetPalette().Surface; }
FSlateColor SThemeSwitcherPanel::GetPrimaryTextColor() const { return GetPalette().PrimaryText; }
FSlateColor SThemeSwitcherPanel::GetSecondaryTextColor() const { return GetPalette().SecondaryText; }
FSlateColor SThemeSwitcherPanel::GetAccentColor() const { return GetPalette().Accent; }
FSlateColor SThemeSwitcherPanel::GetAccentTextColor() const { return GetPalette().AccentText; }
FSlateColor SThemeSwitcherPanel::GetDividerColor() const { return GetPalette().Divider; }
FSlateColor SThemeSwitcherPanel::GetSuccessColor() const { return GetPalette().Success; }

FSlateColor SThemeSwitcherPanel::GetSimulationStatusColor() const
{
    return bSimulationRunning ? GetPalette().Success : GetPalette().SecondaryText;
}

FSlateColor SThemeSwitcherPanel::GetAccentSubtleColor() const
{
    // 使用同一个 Accent RGB，只降低透明度，就能得到与主题一致的弱强调背景。
    FLinearColor Color = GetPalette().Accent;
    Color.A = 0.18f;
    return Color;
}

#undef LOCTEXT_NAMESPACE
