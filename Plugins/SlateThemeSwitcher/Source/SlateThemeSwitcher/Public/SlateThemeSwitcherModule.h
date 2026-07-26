#pragma once

#include "Modules/ModuleManager.h"

/** 插件的编辑器模块：负责主题生命周期、Window 菜单入口和 Nomad Tab。 */
class FSlateThemeSwitcherModule final : public IModuleInterface
{
public:
    /** 模块加载时注册内置主题、Tab 生成器和菜单回调。 */
    virtual void StartupModule() override;

    /** 模块卸载时对称地释放所有注册项。 */
    virtual void ShutdownModule() override;

    /** 打开或聚焦主题切换面板。 */
    void OpenThemeSwitcher();

private:
    /** 把插件入口追加到 Level Editor 的 Window 菜单。 */
    void RegisterMenus();

    /** 当编辑器首次打开该 Tab 时创建实际的 Slate 内容。 */
    TSharedRef<class SDockTab> SpawnThemeSwitcherTab(const class FSpawnTabArgs& Args);
};
