#pragma once

// QDesktop Desktop - Main desktop shell using Window and Controls
// Namespace: QD
//
// The desktop is built using the existing QW::Window and QW::Controls:
// - A fullscreen Window serves as the desktop background
// - Panel controls for TopBar, Sidebar, Taskbar
// - Button controls for sidebar icons and taskbar window buttons
// - Label controls for clock and titles
//
// Layout:
// ┌──────────────────────────────────────────────────────────────┐
// │  [Logo]  QAIOS+ Desktop                      10:32 AM   [⋯]  │  ← TopBar Panel
// ├───────┬──────────────────────────────────────────────────────┤
// │       │                                                      │
// │ Side  │                                                      │
// │ bar   │                Desktop Area                          │
// │ Panel │                (for windows)                         │
// │       │                                                      │
// ├───────┴──────────────────────────────────────────────────────┤
// │  [Win1]  [Win2]                              [Tray][Clock]   │  ← Taskbar Panel
// └──────────────────────────────────────────────────────────────┘

#include "QCTypes.h"
#include "QCGeometry.h"
#include "QCUIStyle.h"
#include "QWWindow.h"
#include "QWStyleTypes.h"
#include "QWControls/Containers/Panel.h"
#include "QWControls/Leaf/Button.h"
#include "QWControls/Leaf/Label.h"
#include "QWInterfaces/IControl.h"
#include "QCVector.h"
#include "QDAccent.h"
#include "QDTheme.h"
#include "QDTerminal.h"

namespace QK
{
    namespace Shutdown
    {
        enum class Reason : QC::u8;
    }
}

namespace QC
{
    namespace JSON
    {
        class Value;
    }
}

namespace QD
{
    using QC::Rect;
    using QC::Size;

    class ShutdownDialog;

    // Layout constants
    constexpr QC::u32 TOP_BAR_HEIGHT = 32;
    constexpr QC::u32 SIDEBAR_WIDTH = 64;
    constexpr QC::u32 TASKBAR_HEIGHT = 40;

    // Maximum windows in taskbar
    constexpr QC::u32 MAX_TASKBAR_WINDOWS = 12;

    /// Sidebar item identifiers
    enum class SidebarItem : QC::u8
    {
        Home = 0,
        Apps,
        Settings,
        Files,
        Terminal,
        Power,
        Count
    };

    /// Desktop - Main desktop shell
    /// Owns a fullscreen Window and control panels
    class Desktop
    {
    public:
        Desktop();
        ~Desktop();

        // ==================== Initialization ====================

        /// Initialize desktop with screen dimensions
        /// Creates the desktop window and all panels
        void initialize(QC::u32 screenWidth, QC::u32 screenHeight);

        /// Resize desktop (on resolution change)
        void resize(QC::u32 screenWidth, QC::u32 screenHeight);

        /// Clean up resources
        void shutdown();

        // ==================== Access ====================

        /// Get the desktop window
        QW::Window *window() { return m_desktopWindow; }
        const QW::Window *window() const { return m_desktopWindow; }

        /// Get screen dimensions
        QC::u32 screenWidth() const { return m_screenWidth; }
        QC::u32 screenHeight() const { return m_screenHeight; }

        /// Get the desktop work area (where app windows can appear)
        Rect workArea() const;

        // ==================== Panels ====================

        QW::Controls::Panel *topBar() { return m_topBar; }
        QW::Controls::Panel *sidebar() { return m_sidebar; }
        QW::Controls::Panel *taskbar() { return m_taskbar; }

        // ==================== Time ====================

        /// Update displayed clock time
        void setTime(QC::u32 hours, QC::u32 minutes);

        // ==================== Window Management ====================

        /// Set the focused window title (shown in top bar)
        void setFocusedWindowTitle(const char *title);

        /// Add a window button to the taskbar
        void addTaskbarWindow(QC::u32 windowId, const char *title);

        /// Remove a window button from the taskbar
        void removeTaskbarWindow(QC::u32 windowId);

        /// Set which taskbar window is active
        void setActiveTaskbarWindow(QC::u32 windowId);

        // ==================== Rendering ====================

        /// Paint the entire desktop
        void paint();

        /// Paint just the background gradient
        void paintBackground();

        // ==================== State ====================

        bool isInitialized() const { return m_initialized; }

    private:
        bool tryInitializeFromJson();

        void clearJsonDesktopState();
        void resetThemeOverrides();
        void parseThemeOverrides(const QC::JSON::Value *themeValue);

        void createTopBar();
        void createSidebar();
        void createTaskbar();
        void updateLayout();
        void applyColors();
        void publishStyleSnapshot(const DesktopColors &colors);
        void applyThemeOverrides(QW::StyleSnapshot &snapshot) const;
        void applyThemeToDesktopColors(DesktopColors &colors) const;
        void updateSidebarButtonRoles();

        void openTerminal();
        void toggleTerminal();
        void recomputeTaskbarWindowBase();
        void showShutdownPrompt(QK::Shutdown::Reason reason);

        // Sidebar button click handler
        static void onSidebarClick(QW::Controls::Button *button, void *userData);
        static bool onShutdownRequested(QK::Shutdown::Reason reason, void *userData);

        // JSON desktop action handlers
        static void onJsonTerminalClick(QW::Controls::Button *button, void *userData);
        static void onJsonShutdownClick(QW::Controls::Button *button, void *userData);

        // Taskbar button click handler
        static void onTaskbarClick(QW::Controls::Button *button, void *userData);

        bool m_initialized;
        QC::u32 m_screenWidth;
        QC::u32 m_screenHeight;

        // The desktop window (fullscreen, no chrome)
        QW::Window *m_desktopWindow;

        bool m_jsonDriven;
        QC::Vector<QW::Controls::IControl *> m_jsonControls;
        QC::Vector<QW::Controls::IControl *> m_jsonRootControls;

        struct ColorOverride
        {
            bool set = false;
            QC::Color value;
        };

        struct PaletteOverrides
        {
            ColorOverride accent;
            ColorOverride accentLight;
            ColorOverride accentDark;
            ColorOverride panel;
            ColorOverride panelBorder;
            ColorOverride text;
            ColorOverride textSecondary;
        };

        struct MetricsOverrides
        {
            bool cornerRadiusSet = false;
            QC::u32 cornerRadius = 0;
            bool buttonCornerRadiusSet = false;
            QC::u32 buttonCornerRadius = 0;
            bool borderWidthSet = false;
            QC::u32 borderWidth = 0;
        };

        struct ButtonStyleOverrides
        {
            ColorOverride fillNormal;
            ColorOverride fillHover;
            ColorOverride fillPressed;
            ColorOverride text;
            ColorOverride border;
            bool glassSet = false;
            bool glass = false;
            bool shineSet = false;
            float shineIntensity = 0.0f;
            bool hasAny() const
            {
                return fillNormal.set || fillHover.set || fillPressed.set || text.set || border.set || glassSet || shineSet;
            }
        };

        struct ShadowOverrides
        {
            bool offsetXSet = false;
            QC::i32 offsetX = 0;
            bool offsetYSet = false;
            QC::i32 offsetY = 0;
            bool blurSet = false;
            QC::u32 blurRadius = 0;
            ColorOverride color;
        };

        struct GlowOverrides
        {
            bool radiusSet = false;
            QC::u32 radius = 0;
            bool intensitySet = false;
            QC::u32 intensity = 0;
            ColorOverride color;
        };

        struct EffectsOverrides
        {
            ColorOverride borderColor;
            ShadowOverrides shadow;
            GlowOverrides glow;
        };

        struct ThemeOverrides
        {
            PaletteOverrides palette;
            MetricsOverrides metrics;
            ButtonStyleOverrides button[static_cast<QC::u32>(QW::ButtonRole::Count)];
            EffectsOverrides effects;
            bool active = false;
        };

        ThemeOverrides m_themeOverrides;
        Theme m_themeDefinition;
        bool m_themeLoaded;

        bool loadThemeDefinition(const QC::JSON::Value *themeValue);
        void applyLoadedThemeToOverrides();

        static float clamp01(float value);
        static QC::u8 clampToByte(QC::u32 value);
        static bool parseColorOverride(const QC::JSON::Value *object, const char *key, ColorOverride &target);
        static bool parseUnsignedOverride(const QC::JSON::Value *object, const char *key, QC::u32 &outValue);
        static bool parseSignedOverride(const QC::JSON::Value *object, const char *key, QC::i32 &outValue);
        static bool parseBoolOverride(const QC::JSON::Value *object, const char *key, bool &outValue);
        static bool parseButtonStyleOverride(const QC::JSON::Value *buttons, const char *key, ButtonStyleOverrides &out);

        // Panels
        QW::Controls::Panel *m_topBar;
        QW::Controls::Panel *m_sidebar;
        QW::Controls::Panel *m_taskbar;

        // JSON-specific buttons we track for layout offsets
        QW::Controls::Button *m_jsonStartButton;
        QW::Controls::Button *m_jsonShutdownButton;

        // Top bar controls
        QW::Controls::Button *m_logoButton;
        QW::Controls::Label *m_titleLabel;
        QW::Controls::Label *m_clockLabel;

        // Dynamic taskbar layout helpers
        QC::i32 m_taskbarWindowBaseX;

        // Sidebar buttons
        QW::Controls::Button *m_sidebarButtons[static_cast<QC::u8>(SidebarItem::Count)];
        SidebarItem m_selectedSidebarItem;

        // Taskbar window buttons
        struct TaskbarEntry
        {
            QC::u32 windowId;
            QW::Controls::Button *button;
            bool isActive;
        };
        TaskbarEntry m_taskbarEntries[MAX_TASKBAR_WINDOWS];
        QC::u32 m_taskbarWindowCount;

        // Clock state
        QC::u32 m_hours;
        QC::u32 m_minutes;

        Terminal *m_terminal;
        class ShutdownDialog *m_shutdownDialog;
    };

} // namespace QD
