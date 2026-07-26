#include "SlateThemeSwitcherModule.h"

#include "Framework/Docking/TabManager.h"
#include "SThemeSwitcherPanel.h"
#include "SlateThemeManager.h"
#include "ToolMenus.h"
#include "WebBrowserModule.h"
#include "Widgets/Docking/SDockTab.h"

#define LOCTEXT_NAMESPACE "SlateThemeSwitcherModule"

DEFINE_LOG_CATEGORY_STATIC(LogSlateThemeSwitcher, Log, All);

namespace SlateThemeSwitcher
{
    // Tab Id 是编辑器内部查找、打开和恢复布局时使用的稳定标识。
    static const FName TabName(TEXT("SlateThemeSwitcher"));
}

void FSlateThemeSwitcherModule::StartupModule()
{
    // SWebBrowserView 不会自行加载 WebBrowser 模块；它假定调用方已提前初始化模块。
    // 如果省略这一步，BrowserWindow 会创建失败，界面只会永久显示加载动画。
    IWebBrowserModule& WebBrowserModule = IWebBrowserModule::Get();
    if (!WebBrowserModule.IsWebModuleAvailable())
    {
        UE_LOG(LogSlateThemeSwitcher, Error, TEXT("WebBrowser/CEF 模块初始化失败，ECharts 无法显示。"));
    }

    // 先准备主题数据，随后创建的任何 Slate 控件都能立即取得当前主题。
    FSlateThemeManager::Get().RegisterBuiltInThemes();

    // Nomad Tab 可以像普通编辑器面板一样停靠、拖动，并参与布局保存。
    FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
        SlateThemeSwitcher::TabName,
        FOnSpawnTab::CreateRaw(this, &FSlateThemeSwitcherModule::SpawnThemeSwitcherTab))
        .SetDisplayName(LOCTEXT("TabTitle", "Slate Theme Switcher"))
        .SetTooltipText(LOCTEXT("TabTooltip", "Open the Day/Night Slate theme preview."));

    // ToolMenus 可能晚于插件模块完成初始化，因此通过启动回调延迟扩展菜单。
    UToolMenus::RegisterStartupCallback(
        FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FSlateThemeSwitcherModule::RegisterMenus));
}

void FSlateThemeSwitcherModule::ShutdownModule()
{
    // 注销顺序与 StartupModule 中的注册项一一对应，避免热重载产生重复菜单和重复 Tab。
    UToolMenus::UnRegisterStartupCallback(this);
    UToolMenus::UnregisterOwner(this);
    FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(SlateThemeSwitcher::TabName);
    FSlateThemeManager::Get().ClearThemes();
}

void FSlateThemeSwitcherModule::RegisterMenus()
{
    // OwnerScoped 会把新增菜单项归属到本模块，ShutdownModule 可以按 Owner 一次性清理。
    FToolMenuOwnerScoped OwnerScoped(this);
    UToolMenu* WindowMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window");
    FToolMenuSection& Section = WindowMenu->FindOrAddSection("WindowLayout");
    Section.AddMenuEntry(
        "OpenSlateThemeSwitcher",
        LOCTEXT("OpenMenuLabel", "Slate Theme Switcher"),
        LOCTEXT("OpenMenuTooltip", "Open the extensible Day/Night theme switcher."),
        FSlateIcon(),
        FUIAction(FExecuteAction::CreateRaw(this, &FSlateThemeSwitcherModule::OpenThemeSwitcher)));
}

void FSlateThemeSwitcherModule::OpenThemeSwitcher()
{
    // 已经打开时 TryInvokeTab 会聚焦现有 Tab，而不是重复创建窗口。
    FGlobalTabmanager::Get()->TryInvokeTab(SlateThemeSwitcher::TabName);
}

TSharedRef<SDockTab> FSlateThemeSwitcherModule::SpawnThemeSwitcherTab(const FSpawnTabArgs& Args)
{
    // SDockTab 只负责编辑器停靠行为，实际界面全部封装在 SThemeSwitcherPanel 中。
    return SNew(SDockTab)
        .TabRole(ETabRole::NomadTab)
        [
            SNew(SThemeSwitcherPanel)
        ];
}

IMPLEMENT_MODULE(FSlateThemeSwitcherModule, SlateThemeSwitcher)

#undef LOCTEXT_NAMESPACE
