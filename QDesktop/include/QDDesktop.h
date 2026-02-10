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
#include "QWCtrlPanel.h"
#include "QWCtrlButton.h"
#include "QWCtrlLabel.h"
#include "QDAccent.h"

namespace QD
{
    using QC::Rect;
    using QC::Size;

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
        void createTopBar();
        void createSidebar();
        void createTaskbar();
        void updateLayout();
        void applyColors();

        // Sidebar button click handler
        static void onSidebarClick(QW::Controls::Button *button, void *userData);

        // Taskbar button click handler
        static void onTaskbarClick(QW::Controls::Button *button, void *userData);

        bool m_initialized;
        QC::u32 m_screenWidth;
        QC::u32 m_screenHeight;

        // The desktop window (fullscreen, no chrome)
        QW::Window *m_desktopWindow;

        // Panels
        QW::Controls::Panel *m_topBar;
        QW::Controls::Panel *m_sidebar;
        QW::Controls::Panel *m_taskbar;

        // Top bar controls
        QW::Controls::Button *m_logoButton;
        QW::Controls::Label *m_titleLabel;
        QW::Controls::Label *m_clockLabel;

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
    };

} // namespace QD
