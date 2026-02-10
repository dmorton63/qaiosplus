// QDesktop Desktop - Implementation using Window and Controls
// Namespace: QD

#include "QDDesktop.h"
#include "QWWindowManager.h"

namespace QD
{
    // Sidebar item labels
    static const char *SIDEBAR_LABELS[] = {
        "Home",
        "Apps",
        "Settings",
        "Files",
        "Terminal",
        "Power"};

    Desktop::Desktop()
        : m_initialized(false),
          m_screenWidth(0),
          m_screenHeight(0),
          m_desktopWindow(nullptr),
          m_topBar(nullptr),
          m_sidebar(nullptr),
          m_taskbar(nullptr),
          m_logoButton(nullptr),
          m_titleLabel(nullptr),
          m_clockLabel(nullptr),
          m_selectedSidebarItem(SidebarItem::Home),
          m_taskbarWindowCount(0),
          m_hours(10),
          m_minutes(32)
    {
        for (QC::u8 i = 0; i < static_cast<QC::u8>(SidebarItem::Count); ++i)
        {
            m_sidebarButtons[i] = nullptr;
        }

        for (QC::u32 i = 0; i < MAX_TASKBAR_WINDOWS; ++i)
        {
            m_taskbarEntries[i].windowId = 0;
            m_taskbarEntries[i].button = nullptr;
            m_taskbarEntries[i].isActive = false;
        }
    }

    Desktop::~Desktop()
    {
        shutdown();
    }

    void Desktop::initialize(QC::u32 screenWidth, QC::u32 screenHeight)
    {
        if (m_initialized)
            return;

        m_screenWidth = screenWidth;
        m_screenHeight = screenHeight;

        // Create fullscreen desktop window via WindowManager (so it gets rendered)
        QW::Rect desktopBounds = {0, 0, screenWidth, screenHeight};
        m_desktopWindow = QW::WindowManager::instance().createWindow("Desktop", desktopBounds);
        m_desktopWindow->setFlags(QW::WindowFlags::Visible); // No border, no title

        // Create the panels
        createTopBar();
        createSidebar();
        createTaskbar();

        // Apply colors based on current style
        applyColors();

        m_initialized = true;
    }

    void Desktop::resize(QC::u32 screenWidth, QC::u32 screenHeight)
    {
        m_screenWidth = screenWidth;
        m_screenHeight = screenHeight;

        if (m_desktopWindow)
        {
            QW::Rect bounds = {0, 0, screenWidth, screenHeight};
            m_desktopWindow->setBounds(bounds);
        }

        updateLayout();
    }

    void Desktop::shutdown()
    {
        if (!m_initialized)
            return;

        // Clean up taskbar buttons
        for (QC::u32 i = 0; i < m_taskbarWindowCount; ++i)
        {
            if (m_taskbarEntries[i].button)
            {
                delete m_taskbarEntries[i].button;
                m_taskbarEntries[i].button = nullptr;
            }
        }
        m_taskbarWindowCount = 0;

        // Clean up sidebar buttons
        for (QC::u8 i = 0; i < static_cast<QC::u8>(SidebarItem::Count); ++i)
        {
            if (m_sidebarButtons[i])
            {
                delete m_sidebarButtons[i];
                m_sidebarButtons[i] = nullptr;
            }
        }

        // Clean up labels
        delete m_clockLabel;
        m_clockLabel = nullptr;
        delete m_titleLabel;
        m_titleLabel = nullptr;
        delete m_logoButton;
        m_logoButton = nullptr;

        // Clean up panels
        delete m_taskbar;
        m_taskbar = nullptr;
        delete m_sidebar;
        m_sidebar = nullptr;
        delete m_topBar;
        m_topBar = nullptr;

        // Clean up window (via WindowManager since it was created there)
        if (m_desktopWindow)
        {
            QW::WindowManager::instance().destroyWindow(m_desktopWindow);
            m_desktopWindow = nullptr;
        }

        m_initialized = false;
    }

    void Desktop::createTopBar()
    {
        // TopBar: full width, at top
        QW::Rect topBarBounds = {0, 0, m_screenWidth, TOP_BAR_HEIGHT};
        m_topBar = new QW::Controls::Panel(m_desktopWindow, topBarBounds);
        m_topBar->setBorderStyle(QW::Controls::BorderStyle::None);
        m_topBar->setFrameVisible(false);

        // Logo button (left)
        QW::Rect logoBounds = {8, 6, 20, 20};
        m_logoButton = new QW::Controls::Button(m_desktopWindow, "Q", logoBounds);
        m_topBar->addChild(m_logoButton);

        // Title label (center-ish)
        QW::Rect titleBounds = {40, 8, 200, 16};
        m_titleLabel = new QW::Controls::Label(m_desktopWindow, "QAIOS+ Desktop", titleBounds);
        m_topBar->addChild(m_titleLabel);

        // Clock label (right)
        QW::Rect clockBounds = {static_cast<QC::i32>(m_screenWidth) - 80, 8, 60, 16};
        m_clockLabel = new QW::Controls::Label(m_desktopWindow, "10:32", clockBounds);
        m_topBar->addChild(m_clockLabel);
    }

    void Desktop::createSidebar()
    {
        // Sidebar: left side, below topbar, above taskbar
        QW::Rect sidebarBounds = {
            0,
            static_cast<QC::i32>(TOP_BAR_HEIGHT),
            SIDEBAR_WIDTH,
            m_screenHeight - TOP_BAR_HEIGHT - TASKBAR_HEIGHT};
        m_sidebar = new QW::Controls::Panel(m_desktopWindow, sidebarBounds);
        m_sidebar->setBorderStyle(QW::Controls::BorderStyle::None);
        m_sidebar->setFrameVisible(false);

        // Create sidebar buttons
        QC::u32 buttonHeight = 48;
        QC::u32 buttonMargin = 4;

        for (QC::u8 i = 0; i < static_cast<QC::u8>(SidebarItem::Count); ++i)
        {
            QC::i32 y;
            if (static_cast<SidebarItem>(i) == SidebarItem::Power)
            {
                // Power button at bottom
                y = static_cast<QC::i32>(sidebarBounds.height) - static_cast<QC::i32>(buttonHeight) - static_cast<QC::i32>(buttonMargin);
            }
            else
            {
                y = static_cast<QC::i32>(buttonMargin + i * (buttonHeight + buttonMargin));
            }

            QW::Rect btnBounds = {
                static_cast<QC::i32>(buttonMargin),
                y,
                SIDEBAR_WIDTH - buttonMargin * 2,
                buttonHeight};

            m_sidebarButtons[i] = new QW::Controls::Button(m_desktopWindow, SIDEBAR_LABELS[i], btnBounds);
            m_sidebarButtons[i]->setId(i + 100); // Use ID to identify which button
            m_sidebarButtons[i]->setClickHandler(onSidebarClick, this);
            m_sidebar->addChild(m_sidebarButtons[i]);
        }
    }

    void Desktop::createTaskbar()
    {
        // Taskbar: bottom, after sidebar
        QW::Rect taskbarBounds = {
            static_cast<QC::i32>(SIDEBAR_WIDTH),
            static_cast<QC::i32>(m_screenHeight - TASKBAR_HEIGHT),
            m_screenWidth - SIDEBAR_WIDTH,
            TASKBAR_HEIGHT};
        m_taskbar = new QW::Controls::Panel(m_desktopWindow, taskbarBounds);
        m_taskbar->setBorderStyle(QW::Controls::BorderStyle::None);
        m_taskbar->setFrameVisible(false);
    }

    void Desktop::updateLayout()
    {
        if (!m_initialized)
            return;

        // Update topbar
        if (m_topBar)
        {
            QW::Rect topBarBounds = {0, 0, m_screenWidth, TOP_BAR_HEIGHT};
            m_topBar->setBounds(topBarBounds);

            // Update clock position
            if (m_clockLabel)
            {
                QW::Rect clockBounds = {static_cast<QC::i32>(m_screenWidth) - 80, 8, 60, 16};
                m_clockLabel->setBounds(clockBounds);
            }
        }

        // Update sidebar
        if (m_sidebar)
        {
            QW::Rect sidebarBounds = {
                0,
                static_cast<QC::i32>(TOP_BAR_HEIGHT),
                SIDEBAR_WIDTH,
                m_screenHeight - TOP_BAR_HEIGHT - TASKBAR_HEIGHT};
            m_sidebar->setBounds(sidebarBounds);
        }

        // Update taskbar
        if (m_taskbar)
        {
            QW::Rect taskbarBounds = {
                static_cast<QC::i32>(SIDEBAR_WIDTH),
                static_cast<QC::i32>(m_screenHeight - TASKBAR_HEIGHT),
                m_screenWidth - SIDEBAR_WIDTH,
                TASKBAR_HEIGHT};
            m_taskbar->setBounds(taskbarBounds);
        }
    }

    void Desktop::applyColors()
    {
        DesktopColors colors = currentColors();
        applyAccent(colors);

        // Apply to panels
        if (m_topBar)
        {
            m_topBar->setBackgroundColor(colors.topBarBg);
        }

        if (m_sidebar)
        {
            m_sidebar->setBackgroundColor(colors.sidebarBg);
        }

        if (m_taskbar)
        {
            m_taskbar->setBackgroundColor(colors.taskbarBg);
        }

        // Apply to buttons
        for (QC::u8 i = 0; i < static_cast<QC::u8>(SidebarItem::Count); ++i)
        {
            if (m_sidebarButtons[i])
            {
                m_sidebarButtons[i]->setBackgroundColor(colors.sidebarBg);
                m_sidebarButtons[i]->setHoverColor(colors.sidebarHover);
                m_sidebarButtons[i]->setTextColor(colors.sidebarText);
            }
        }

        // Apply to labels
        if (m_titleLabel)
        {
            m_titleLabel->setTextColor(colors.topBarText);
            m_titleLabel->setBackgroundColor(QW::Color(0, 0, 0, 0)); // Transparent
        }

        if (m_clockLabel)
        {
            m_clockLabel->setTextColor(colors.topBarText);
            m_clockLabel->setBackgroundColor(QW::Color(0, 0, 0, 0));
        }

        if (m_logoButton)
        {
            m_logoButton->setBackgroundColor(accent());
            m_logoButton->setTextColor(QW::Color(255, 255, 255, 255));
        }
    }

    QW::Rect Desktop::workArea() const
    {
        return {
            static_cast<QC::i32>(SIDEBAR_WIDTH),
            static_cast<QC::i32>(TOP_BAR_HEIGHT),
            m_screenWidth - SIDEBAR_WIDTH,
            m_screenHeight - TOP_BAR_HEIGHT - TASKBAR_HEIGHT};
    }

    void Desktop::setTime(QC::u32 hours, QC::u32 minutes)
    {
        m_hours = hours % 24;
        m_minutes = minutes % 60;

        if (m_clockLabel)
        {
            // Format as HH:MM (simple, no AM/PM for now)
            char timeStr[16];
            QC::u32 displayHour = m_hours;
            // Simple format
            timeStr[0] = '0' + (displayHour / 10);
            timeStr[1] = '0' + (displayHour % 10);
            timeStr[2] = ':';
            timeStr[3] = '0' + (m_minutes / 10);
            timeStr[4] = '0' + (m_minutes % 10);
            timeStr[5] = '\0';

            m_clockLabel->setText(timeStr);
        }
    }

    void Desktop::setFocusedWindowTitle(const char *title)
    {
        if (m_titleLabel)
        {
            m_titleLabel->setText(title ? title : "QAIOS+ Desktop");
        }
    }

    void Desktop::addTaskbarWindow(QC::u32 windowId, const char *title)
    {
        if (m_taskbarWindowCount >= MAX_TASKBAR_WINDOWS || !m_taskbar)
            return;

        // Create button for this window
        QC::u32 buttonWidth = 140;
        QC::u32 buttonHeight = 32;
        QC::i32 x = static_cast<QC::i32>(4 + m_taskbarWindowCount * (buttonWidth + 4));
        QC::i32 y = static_cast<QC::i32>((TASKBAR_HEIGHT - buttonHeight) / 2);

        QW::Rect btnBounds = {x, y, buttonWidth, buttonHeight};

        auto *btn = new QW::Controls::Button(m_desktopWindow, title ? title : "Window", btnBounds);
        btn->setId(windowId);
        btn->setClickHandler(onTaskbarClick, this);

        DesktopColors colors = currentColors();
        btn->setBackgroundColor(colors.taskbarBg);
        btn->setHoverColor(colors.taskbarHover);
        btn->setTextColor(colors.taskbarText);

        m_taskbar->addChild(btn);

        m_taskbarEntries[m_taskbarWindowCount].windowId = windowId;
        m_taskbarEntries[m_taskbarWindowCount].button = btn;
        m_taskbarEntries[m_taskbarWindowCount].isActive = false;
        ++m_taskbarWindowCount;
    }

    void Desktop::removeTaskbarWindow(QC::u32 windowId)
    {
        for (QC::u32 i = 0; i < m_taskbarWindowCount; ++i)
        {
            if (m_taskbarEntries[i].windowId == windowId)
            {
                // Remove from panel
                if (m_taskbar && m_taskbarEntries[i].button)
                {
                    m_taskbar->removeChild(m_taskbarEntries[i].button);
                    delete m_taskbarEntries[i].button;
                }

                // Shift remaining entries
                for (QC::u32 j = i; j < m_taskbarWindowCount - 1; ++j)
                {
                    m_taskbarEntries[j] = m_taskbarEntries[j + 1];
                }

                --m_taskbarWindowCount;
                m_taskbarEntries[m_taskbarWindowCount].windowId = 0;
                m_taskbarEntries[m_taskbarWindowCount].button = nullptr;
                m_taskbarEntries[m_taskbarWindowCount].isActive = false;

                // Reposition remaining buttons
                for (QC::u32 j = i; j < m_taskbarWindowCount; ++j)
                {
                    if (m_taskbarEntries[j].button)
                    {
                        QC::u32 buttonWidth = 140;
                        QC::u32 buttonHeight = 32;
                        QC::i32 x = static_cast<QC::i32>(4 + j * (buttonWidth + 4));
                        QC::i32 y = static_cast<QC::i32>((TASKBAR_HEIGHT - buttonHeight) / 2);
                        m_taskbarEntries[j].button->setBounds({x, y, buttonWidth, buttonHeight});
                    }
                }

                return;
            }
        }
    }

    void Desktop::setActiveTaskbarWindow(QC::u32 windowId)
    {
        DesktopColors colors = currentColors();
        applyAccent(colors);

        for (QC::u32 i = 0; i < m_taskbarWindowCount; ++i)
        {
            bool isActive = (m_taskbarEntries[i].windowId == windowId);
            m_taskbarEntries[i].isActive = isActive;

            if (m_taskbarEntries[i].button)
            {
                if (isActive)
                {
                    m_taskbarEntries[i].button->setBackgroundColor(colors.taskbarActiveWindow);
                }
                else
                {
                    m_taskbarEntries[i].button->setBackgroundColor(colors.taskbarBg);
                }
            }
        }
    }

    // ==================== Rendering ====================

    void Desktop::paint()
    {
        if (!m_desktopWindow)
            return;

        // Paint background gradient
        paintBackground();

        // Paint panels (they paint their children)
        if (m_topBar)
            m_topBar->paint();
        if (m_sidebar)
            m_sidebar->paint();
        if (m_taskbar)
            m_taskbar->paint();
    }

    void Desktop::paintBackground()
    {
        if (!m_desktopWindow)
            return;

        DesktopColors colors = currentColors();

        // Paint vertical gradient
        for (QC::u32 y = 0; y < m_screenHeight; ++y)
        {
            QC::u32 t = (y * 255) / m_screenHeight;
            QC::u32 invT = 255 - t;

            QW::Color lineColor;
            lineColor.r = static_cast<QC::u8>((colors.bgTop.r * invT + colors.bgBottom.r * t) / 255);
            lineColor.g = static_cast<QC::u8>((colors.bgTop.g * invT + colors.bgBottom.g * t) / 255);
            lineColor.b = static_cast<QC::u8>((colors.bgTop.b * invT + colors.bgBottom.b * t) / 255);
            lineColor.a = 0xFF;

            QW::Rect line = {0, static_cast<QC::i32>(y), m_screenWidth, 1};
            m_desktopWindow->fillRect(line, lineColor);
        }
    }

    // ==================== Callbacks ====================

    void Desktop::onSidebarClick(QW::Controls::Button *button, void *userData)
    {
        if (!button || !userData)
            return;

        Desktop *desktop = static_cast<Desktop *>(userData);
        QC::u32 id = button->id();

        if (id >= 100 && id < 100 + static_cast<QC::u32>(SidebarItem::Count))
        {
            desktop->m_selectedSidebarItem = static_cast<SidebarItem>(id - 100);

            // Update button colors to show selection
            DesktopColors colors = currentColors();
            applyAccent(colors);

            for (QC::u8 i = 0; i < static_cast<QC::u8>(SidebarItem::Count); ++i)
            {
                if (desktop->m_sidebarButtons[i])
                {
                    if (i == id - 100)
                    {
                        desktop->m_sidebarButtons[i]->setBackgroundColor(colors.sidebarSelected);
                        desktop->m_sidebarButtons[i]->setTextColor(QW::Color(255, 255, 255, 255));
                    }
                    else
                    {
                        desktop->m_sidebarButtons[i]->setBackgroundColor(colors.sidebarBg);
                        desktop->m_sidebarButtons[i]->setTextColor(colors.sidebarText);
                    }
                }
            }
        }
    }

    void Desktop::onTaskbarClick(QW::Controls::Button *button, void *userData)
    {
        if (!button || !userData)
            return;

        Desktop *desktop = static_cast<Desktop *>(userData);
        QC::u32 windowId = button->id();

        // Activate this window
        desktop->setActiveTaskbarWindow(windowId);

        // TODO: Tell WindowManager to bring this window to front
    }

} // namespace QD
