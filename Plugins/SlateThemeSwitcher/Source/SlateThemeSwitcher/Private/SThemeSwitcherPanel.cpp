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
        0.3f,
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

                // 实时仿真图表：X 轴是站点名称，Y 轴是 0~100 的动态数值。
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
        .Padding(1.0f)
        [
            SNew(SBox)
            .HeightOverride(380.0f)
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
        ];
}

void SThemeSwitcherPanel::InitializeSimulationData()
{
    // 名称是固定分类；数值用随机游走变化，既有随机性又不会每帧突然跳到完全无关的位置。
    SimulationNames = {
        TEXT("站点 A"),
        TEXT("站点 B"),
        TEXT("站点 C"),
        TEXT("站点 D"),
        TEXT("站点 E"),
        TEXT("站点 F"),
        TEXT("站点 G"),
        TEXT("站点 H")
    };

    SimulationRandom.Initialize(static_cast<int32>(FPlatformTime::Cycles64()));
    SimulationValues.Reset(SimulationNames.Num());
    for (int32 Index = 0; Index < SimulationNames.Num(); ++Index)
    {
        SimulationValues.Add(SimulationRandom.FRandRange(30.0f, 72.0f));
    }
}

EActiveTimerReturnType SThemeSwitcherPanel::HandleSimulationTick(double InCurrentTime, float InDeltaTime)
{
    for (float& Value : SimulationValues)
    {
        const float Delta = SimulationRandom.FRandRange(-6.0f, 6.0f);
        Value = FMath::Clamp(Value + Delta, 5.0f, 95.0f);
    }

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

    TArray<TSharedPtr<FJsonValue>> NumberJsonValues;
    NumberJsonValues.Reserve(SimulationValues.Num());
    for (const float Value : SimulationValues)
    {
        NumberJsonValues.Add(MakeShared<FJsonValueNumber>(Value));
    }

    FString NamesJson;
    const TSharedRef<TJsonWriter<>> NamesWriter = TJsonWriterFactory<>::Create(&NamesJson);
    FJsonSerializer::Serialize(NameJsonValues, NamesWriter);

    FString NumbersJson;
    const TSharedRef<TJsonWriter<>> NumbersWriter = TJsonWriterFactory<>::Create(&NumbersJson);
    FJsonSerializer::Serialize(NumberJsonValues, NumbersWriter);

    const FSlateThemePalette& Palette = GetPalette();
    const TSharedRef<FJsonObject> PaletteObject = MakeShared<FJsonObject>();
    PaletteObject->SetStringField(TEXT("background"), ColorToCss(Palette.WindowBackground));
    PaletteObject->SetStringField(TEXT("surface"), ColorToCss(Palette.Surface));
    PaletteObject->SetStringField(TEXT("primaryText"), ColorToCss(Palette.PrimaryText));
    PaletteObject->SetStringField(TEXT("secondaryText"), ColorToCss(Palette.SecondaryText));
    PaletteObject->SetStringField(TEXT("accent"), ColorToCss(Palette.Accent));
    PaletteObject->SetStringField(TEXT("divider"), ColorToCss(Palette.Divider));

    FString PaletteJson;
    const TSharedRef<TJsonWriter<>> PaletteWriter = TJsonWriterFactory<>::Create(&PaletteJson);
    FJsonSerializer::Serialize(PaletteObject, PaletteWriter);

    const FString Script = FString::Printf(
        TEXT("window.updateSimulationData && window.updateSimulationData(%s, %s, %s);"),
        *NamesJson,
        *NumbersJson,
        *PaletteJson);
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

FSlateColor SThemeSwitcherPanel::GetAccentSubtleColor() const
{
    // 使用同一个 Accent RGB，只降低透明度，就能得到与主题一致的弱强调背景。
    FLinearColor Color = GetPalette().Accent;
    Color.A = 0.18f;
    return Color;
}

#undef LOCTEXT_NAMESPACE
