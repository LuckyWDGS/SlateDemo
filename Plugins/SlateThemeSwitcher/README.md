# Slate Theme Switcher

An editor-only Slate plugin with built-in **Day** and **Night** themes.

## Use it

1. Open the project and enable **Slate Theme Switcher** if Unreal asks.
2. Restart the editor after enabling it.
3. Open **Window > Slate Theme Switcher**.
4. Press **切换主题** to cycle through every registered theme, or select one from the drop-down.

The plugin intentionally styles its own Slate surface instead of mutating Unreal Editor's global `FAppStyle`.

## Register another theme

Add a definition from another editor module after `SlateThemeSwitcher` has loaded:

```cpp
#include "SlateThemeManager.h"

FSlateThemePalette OceanPalette;
OceanPalette.WindowBackground = FLinearColor(0.01f, 0.05f, 0.08f, 1.0f);
OceanPalette.PanelBackground = FLinearColor(0.02f, 0.09f, 0.13f, 1.0f);
OceanPalette.Surface = FLinearColor(0.03f, 0.14f, 0.19f, 1.0f);
OceanPalette.PrimaryText = FLinearColor::White;
OceanPalette.SecondaryText = FLinearColor(0.55f, 0.75f, 0.80f, 1.0f);
OceanPalette.Accent = FLinearColor(0.0f, 0.65f, 0.80f, 1.0f);
OceanPalette.AccentHovered = FLinearColor(0.1f, 0.78f, 0.92f, 1.0f);
OceanPalette.AccentText = FLinearColor::White;
OceanPalette.Divider = FLinearColor(0.08f, 0.23f, 0.28f, 1.0f);
OceanPalette.Success = FLinearColor(0.2f, 0.85f, 0.55f, 1.0f);

FSlateThemeManager::Get().RegisterTheme(FSlateThemeDefinition(
    TEXT("Ocean"),
    NSLOCTEXT("MyPlugin", "OceanTheme", "Ocean"),
    NSLOCTEXT("MyPlugin", "OceanThemeDescription", "A cool blue theme."),
    OceanPalette));
```

The demo drop-down and one-click cycle update automatically. Custom widgets can subscribe to:

```cpp
FSlateThemeManager::Get().OnThemeChanged().AddSP(this, &SMyWidget::HandleThemeChanged);
```

If a widget displays the available theme list, subscribe to `OnThemeRegistryChanged()` as well.

Call the registry from the game/editor thread. When your module unloads, unregister themes it owns with `UnregisterTheme()`.
