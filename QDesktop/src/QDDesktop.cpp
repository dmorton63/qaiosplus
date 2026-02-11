// QDesktop Desktop - Implementation using Window and Controls
// Namespace: QD

#include "QDDesktop.h"
#include "QWWindowManager.h"
#include "QCJson.h"
#include "QCLogger.h"
#include "QFSVFS.h"
#include "QFSFile.h"

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

    namespace
    {
        constexpr const char *LOG_MODULE = "QDesktop";

        inline QC::i32 clampChannel(QC::i32 value)
        {
            if (value < 0)
                return 0;
            if (value > 255)
                return 255;
            return value;
        }

        inline QW::Color blendTowards(QW::Color color, QC::u8 target, QC::u8 percent)
        {
            if (percent > 100)
                percent = 100;

            auto adjust = [&](QC::u8 component) -> QC::u8
            {
                QC::i32 delta = static_cast<QC::i32>(target) - static_cast<QC::i32>(component);
                QC::i32 value = static_cast<QC::i32>(component) + (delta * percent + 50) / 100;
                return static_cast<QC::u8>(clampChannel(value));
            };

            return QW::Color(adjust(color.r), adjust(color.g), adjust(color.b), color.a);
        }

        inline QW::Color lightenColor(QW::Color color, QC::u8 percent)
        {
            return blendTowards(color, 255, percent);
        }

        inline QW::Color darkenColor(QW::Color color, QC::u8 percent)
        {
            return blendTowards(color, 0, percent);
        }

        inline void applyVistaButtonStyle(QW::Controls::Button *button, QW::Color base)
        {
            if (!button)
                return;

            QW::Color top = lightenColor(base, 25);
            QW::Color bottom = darkenColor(base, 60);
            QW::Color highlightTop = lightenColor(base, 55);
            QW::Color highlightBottom = lightenColor(base, 10);
            QW::Color borderOuter = darkenColor(base, 70);
            QW::Color borderInner = lightenColor(base, 70);
            QW::Color glow = lightenColor(base, 35).withAlpha(180);

            button->setVisualStyle(QW::Controls::ButtonStyle::Vista);
            button->setVistaGradient(top, bottom);
            button->setVistaHighlight(highlightTop, highlightBottom);
            button->setVistaBorders(borderOuter, borderInner);
            button->setVistaGlow(glow);
            button->setTextColor(QW::Color(255, 255, 255, 255));
        }

        inline QC::i32 parseInt(const char *s, bool *ok)
        {
            if (ok)
                *ok = false;
            if (!s || !*s)
                return 0;

            bool neg = false;
            QC::usize i = 0;
            if (s[0] == '-')
            {
                neg = true;
                i = 1;
            }

            if (!s[i])
                return 0;

            QC::i32 v = 0;
            for (; s[i]; ++i)
            {
                if (s[i] < '0' || s[i] > '9')
                    return 0;
                v = v * 10 + (s[i] - '0');
            }

            if (ok)
                *ok = true;
            return neg ? -v : v;
        }

        inline bool startsWith(const char *s, const char *prefix)
        {
            if (!s || !prefix)
                return false;
            while (*prefix)
            {
                if (*s != *prefix)
                    return false;
                ++s;
                ++prefix;
            }
            return true;
        }

        inline QC::i32 clampNonNegative(QC::i32 v)
        {
            return v < 0 ? 0 : v;
        }

        inline bool parseHexByte(char hi, char lo, QC::u8 *out)
        {
            auto hex = [](char c) -> int
            {
                if (c >= '0' && c <= '9')
                    return c - '0';
                if (c >= 'a' && c <= 'f')
                    return 10 + (c - 'a');
                if (c >= 'A' && c <= 'F')
                    return 10 + (c - 'A');
                return -1;
            };

            int h = hex(hi);
            int l = hex(lo);
            if (h < 0 || l < 0)
                return false;

            *out = static_cast<QC::u8>((h << 4) | l);
            return true;
        }

        inline bool parseHexColor(const char *s, QW::Color *out)
        {
            if (!s || !out)
                return false;
            // #RRGGBB only (matches current desktop.json)
            if (s[0] != '#' || !s[1] || !s[2] || !s[3] || !s[4] || !s[5] || !s[6] || s[7] != '\0')
                return false;

            QC::u8 r, g, b;
            if (!parseHexByte(s[1], s[2], &r))
                return false;
            if (!parseHexByte(s[3], s[4], &g))
                return false;
            if (!parseHexByte(s[5], s[6], &b))
                return false;

            *out = QW::Color(r, g, b, 255);
            return true;
        }

        inline bool evalLayoutValue(const QC::JSON::Value *value, QC::i32 parentW, QC::i32 parentH, bool isX, bool isY, bool isWidth, bool isHeight, QC::i32 *out)
        {
            if (!value || !out)
                return false;

            if (value->isNumber())
            {
                *out = static_cast<QC::i32>(value->asNumber(0.0));
                return true;
            }

            if (!value->isString())
                return false;

            const char *s = value->asString(nullptr);
            if (!s || !*s)
                return false;

            if (isX && startsWith(s, "right-"))
            {
                bool ok = false;
                QC::i32 n = parseInt(s + 6, &ok);
                if (!ok)
                    return false;
                *out = parentW - n;
                return true;
            }

            if (isY && startsWith(s, "bottom-"))
            {
                bool ok = false;
                QC::i32 n = parseInt(s + 7, &ok);
                if (!ok)
                    return false;
                *out = parentH - n;
                return true;
            }

            // Percent or percent +/- constant: e.g. "100%", "100%-48", "50%+10"
            const char *percent = nullptr;
            for (const char *p = s; *p; ++p)
            {
                if (*p == '%')
                {
                    percent = p;
                    break;
                }
            }

            if (percent)
            {
                // Parse leading int (no spaces)
                QC::i32 pValue = 0;
                {
                    bool ok = false;
                    // Copy substring [s, percent)
                    char tmp[16];
                    QC::usize len = static_cast<QC::usize>(percent - s);
                    if (len == 0 || len >= sizeof(tmp))
                        return false;
                    for (QC::usize i = 0; i < len; ++i)
                        tmp[i] = s[i];
                    tmp[len] = '\0';
                    pValue = parseInt(tmp, &ok);
                    if (!ok)
                        return false;
                }

                QC::i32 baseDim = (isX || isWidth) ? parentW : parentH;
                QC::i32 v = (baseDim * pValue) / 100;

                const char *tail = percent + 1;
                if (*tail == '\0')
                {
                    *out = v;
                    return true;
                }

                char op = *tail;
                if (op != '+' && op != '-')
                    return false;
                ++tail;
                bool ok = false;
                QC::i32 n = parseInt(tail, &ok);
                if (!ok)
                    return false;
                *out = (op == '+') ? (v + n) : (v - n);
                return true;
            }

            // Plain integer string
            bool ok = false;
            QC::i32 v = parseInt(s, &ok);
            if (!ok)
                return false;
            *out = v;
            return true;
        }

        inline QW::Rect parseBounds(const QC::JSON::Value *obj, QC::i32 parentW, QC::i32 parentH, const char *type)
        {
            QC::i32 x = 0;
            QC::i32 y = 0;

            QC::i32 w = 0;
            QC::i32 h = 0;

            // Defaults by type
            if (type && QC::String::strcmp(type, "label") == 0)
            {
                w = 200;
                h = 16;
            }
            else if (type && QC::String::strcmp(type, "button") == 0)
            {
                w = 120;
                h = 32;
            }
            else
            {
                w = parentW;
                h = parentH;
            }

            if (obj && obj->isObject())
            {
                if (const QC::JSON::Value *vx = obj->find("x"))
                    (void)evalLayoutValue(vx, parentW, parentH, true, false, false, false, &x);
                if (const QC::JSON::Value *vy = obj->find("y"))
                    (void)evalLayoutValue(vy, parentW, parentH, false, true, false, false, &y);
                if (const QC::JSON::Value *vw = obj->find("width"))
                    (void)evalLayoutValue(vw, parentW, parentH, false, false, true, false, &w);
                if (const QC::JSON::Value *vh = obj->find("height"))
                    (void)evalLayoutValue(vh, parentW, parentH, false, false, false, true, &h);
            }

            w = clampNonNegative(w);
            h = clampNonNegative(h);

            return QW::Rect{x, y, static_cast<QC::u32>(w), static_cast<QC::u32>(h)};
        }

        inline const char *stringOrNull(const QC::JSON::Value *v)
        {
            return (v && v->isString()) ? v->asString(nullptr) : nullptr;
        }
    } // namespace

    Desktop::Desktop()
        : m_initialized(false),
          m_screenWidth(0),
          m_screenHeight(0),
          m_desktopWindow(nullptr),
          m_jsonDriven(false),
          m_topBar(nullptr),
          m_sidebar(nullptr),
          m_taskbar(nullptr),
          m_logoButton(nullptr),
          m_titleLabel(nullptr),
          m_clockLabel(nullptr),
          m_selectedSidebarItem(SidebarItem::Home),
          m_taskbarWindowCount(0),
          m_hours(10),
          m_minutes(32),
          m_terminal(nullptr)
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

        // Prefer JSON-driven desktop if /desktop.json is present and valid
        if (!tryInitializeFromJson())
        {
            // Create the panels
            createTopBar();
            createSidebar();
            createTaskbar();

            // Apply colors based on current style
            applyColors();
        }

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

        if (m_jsonDriven)
        {
            clearJsonDesktopState();

            // Clean up window (via WindowManager since it was created there)
            if (m_desktopWindow)
            {
                QW::WindowManager::instance().destroyWindow(m_desktopWindow);
                m_desktopWindow = nullptr;
            }

            m_initialized = false;
            return;
        }

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
        if (m_desktopWindow && m_desktopWindow->root())
        {
            m_desktopWindow->root()->clearChildren();
        }

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

        if (m_terminal)
        {
            delete m_terminal;
            m_terminal = nullptr;
        }
    }

    void Desktop::openTerminal()
    {
        if (!m_terminal)
        {
            m_terminal = new Terminal(this);
        }
        if (m_terminal)
        {
            m_terminal->open();
        }
    }

    void Desktop::clearJsonDesktopState()
    {
        if (m_desktopWindow && m_desktopWindow->root())
        {
            // JSON controls are added to the root for input routing; remove them before deletion.
            m_desktopWindow->root()->clearChildren();
        }

        // Clear taskbar bookkeeping in case callers use it later
        for (QC::u32 i = 0; i < MAX_TASKBAR_WINDOWS; ++i)
        {
            m_taskbarEntries[i].windowId = 0;
            m_taskbarEntries[i].button = nullptr;
            m_taskbarEntries[i].isActive = false;
        }
        m_taskbarWindowCount = 0;

        for (QC::u8 i = 0; i < static_cast<QC::u8>(SidebarItem::Count); ++i)
        {
            m_sidebarButtons[i] = nullptr;
        }

        // Delete all JSON-created controls
        for (QC::isize i = static_cast<QC::isize>(m_jsonControls.size()) - 1; i >= 0; --i)
        {
            delete m_jsonControls[static_cast<QC::usize>(i)];
        }
        m_jsonControls.clear();
        m_jsonRootControls.clear();

        m_topBar = nullptr;
        m_sidebar = nullptr;
        m_taskbar = nullptr;
        m_logoButton = nullptr;
        m_titleLabel = nullptr;
        m_clockLabel = nullptr;

        m_jsonDriven = false;
    }

    bool Desktop::tryInitializeFromJson()
    {
        // NOTE: Our FAT32 layer currently does not implement Long File Name (LFN) entries.
        // build.sh copies project-root desktop.json into the ramdisk as an 8.3 name: /DESKTOP.JSN
        // We try both paths for convenience.
        const char *jsonPaths[] = {"/desktop.json", "/DESKTOP.JSN", "/DESKTO~1.JSO"};

        QFS::File *file = nullptr;
        const char *openedPath = nullptr;
        for (QC::usize i = 0; i < sizeof(jsonPaths) / sizeof(jsonPaths[0]); ++i)
        {
            file = QFS::VFS::instance().open(jsonPaths[i], QFS::OpenMode::Read);
            if (file)
            {
                openedPath = jsonPaths[i];
                break;
            }
        }
        if (!file)
        {
            QC_LOG_INFO(LOG_MODULE, "No desktop JSON found (/desktop.json or /DESKTOP.JSN); using hardcoded desktop\n");
            return false;
        }

        if (openedPath)
        {
            QC_LOG_INFO(LOG_MODULE, "Loading desktop definition from %s\n", openedPath);
        }

        QC::u64 size64 = file->size();
        if (size64 == 0 || size64 > 1024 * 256)
        {
            QFS::VFS::instance().close(file);
            QC_LOG_WARN(LOG_MODULE, "desktop.json has invalid size (%llu); using hardcoded desktop\n", static_cast<unsigned long long>(size64));
            return false;
        }

        QC::usize size = static_cast<QC::usize>(size64);
        char *jsonText = static_cast<char *>(operator new[](size + 1));
        QC::isize readCount = file->read(jsonText, size);
        QFS::VFS::instance().close(file);

        if (readCount <= 0)
        {
            operator delete[](jsonText);
            QC_LOG_WARN(LOG_MODULE, "Failed to read /desktop.json; using hardcoded desktop\n");
            return false;
        }

        if (static_cast<QC::usize>(readCount) < size)
        {
            size = static_cast<QC::usize>(readCount);
        }
        jsonText[size] = '\0';

        QC::JSON::Value root;
        bool ok = QC::JSON::parse(jsonText, root);
        operator delete[](jsonText);

        if (!ok)
        {
            QC_LOG_WARN(LOG_MODULE, "Failed to parse /desktop.json; using hardcoded desktop\n");
            return false;
        }

        const QC::JSON::Value *desktop = root.find("desktop");
        if (!desktop || !desktop->isObject())
        {
            QC_LOG_WARN(LOG_MODULE, "desktop.json missing 'desktop' object; using hardcoded desktop\n");
            return false;
        }

        const QC::JSON::Value *layout = desktop->find("layout");
        const QC::JSON::Value *controls = layout ? layout->find("controls") : nullptr;
        if (!controls || !controls->isArray())
        {
            QC_LOG_WARN(LOG_MODULE, "desktop.json missing layout.controls array; using hardcoded desktop\n");
            return false;
        }

        // Build controls
        m_jsonDriven = true;

        auto buildControl = [&](auto &&self, const QC::JSON::Value *controlValue, QW::Controls::Panel *parentPanel, QC::i32 parentW, QC::i32 parentH) -> void
        {
            if (!controlValue || !controlValue->isObject())
                return;

            const char *type = stringOrNull(controlValue->find("type"));
            if (!type)
                return;

            const char *id = stringOrNull(controlValue->find("id"));

            QW::Rect bounds = parseBounds(controlValue, parentW, parentH, type);

            QW::Controls::IControl *created = nullptr;

            if (QC::String::strcmp(type, "panel") == 0)
            {
                auto *panel = new QW::Controls::Panel(m_desktopWindow, bounds);
                panel->setBorderStyle(QW::Controls::BorderStyle::None);
                panel->setFrameVisible(false);

                if (const char *bg = stringOrNull(controlValue->find("background")))
                {
                    QW::Color c;
                    if (parseHexColor(bg, &c))
                        panel->setBackgroundColor(c);
                }

                // Any border hint -> simple flat border
                const char *border = stringOrNull(controlValue->find("border"));
                const char *borderTop = stringOrNull(controlValue->find("borderTop"));
                const char *borderBottom = stringOrNull(controlValue->find("borderBottom"));
                const char *borderLeft = stringOrNull(controlValue->find("borderLeft"));
                const char *borderRight = stringOrNull(controlValue->find("borderRight"));
                const char *borderAny = border ? border : (borderTop ? borderTop : (borderBottom ? borderBottom : (borderLeft ? borderLeft : borderRight)));
                if (borderAny)
                {
                    QW::Color c;
                    if (parseHexColor(borderAny, &c))
                    {
                        panel->setBorderStyle(QW::Controls::BorderStyle::Flat);
                        panel->setBorderColor(c);
                        panel->setBorderWidth(1);
                        panel->setFrameVisible(true);
                    }
                }

                created = panel;
            }
            else if (QC::String::strcmp(type, "label") == 0)
            {
                const char *text = stringOrNull(controlValue->find("text"));
                auto *label = new QW::Controls::Label(m_desktopWindow, text ? text : "", bounds);
                label->setTransparent(true);
                if (const char *color = stringOrNull(controlValue->find("color")))
                {
                    QW::Color c;
                    if (parseHexColor(color, &c))
                        label->setTextColor(c);
                }
                created = label;
            }
            else if (QC::String::strcmp(type, "button") == 0)
            {
                const char *text = stringOrNull(controlValue->find("text"));
                auto *button = new QW::Controls::Button(m_desktopWindow, text ? text : "", bounds);

                if (const char *style = stringOrNull(controlValue->find("style")))
                {
                    if (QC::String::strcmp(style, "vista") == 0)
                    {
                        applyVistaButtonStyle(button, accent());
                    }
                }

                // Wire up known desktop actions.
                if (id && QC::String::strcmp(id, "btnTerminal") == 0)
                {
                    button->setClickHandler(onJsonTerminalClick, this);
                }

                created = button;
            }

            if (!created)
                return;

            // Track ownership
            m_jsonControls.push_back(created);

            // Root vs child
            if (parentPanel)
            {
                parentPanel->addChild(created);
            }
            else
            {
                m_jsonRootControls.push_back(created);
                if (m_desktopWindow && m_desktopWindow->root())
                {
                    // Required for input routing: Window::onEvent dispatches into root control.
                    m_desktopWindow->root()->addChild(created);
                }
            }

            // Capture well-known pointers used by Desktop logic
            if (id)
            {
                if (!m_topBar && QC::String::strcmp(id, "headerBar") == 0)
                    m_topBar = created->asPanel();
                if (!m_sidebar && QC::String::strcmp(id, "sidebar") == 0)
                    m_sidebar = created->asPanel();
                if (!m_taskbar && QC::String::strcmp(id, "taskbar") == 0)
                    m_taskbar = created->asPanel();

                if (!m_titleLabel && QC::String::strcmp(id, "headerTitle") == 0)
                    m_titleLabel = static_cast<QW::Controls::Label *>(created);
                if (!m_clockLabel && QC::String::strcmp(id, "clockLabel") == 0)
                    m_clockLabel = static_cast<QW::Controls::Label *>(created);
                // (logoButton is optional; not present in current desktop.json)
            }

            // Recurse children for panels
            if (created->asPanel())
            {
                const QC::JSON::Value *children = controlValue->find("children");
                if (children && children->isArray())
                {
                    const QC::JSON::Array *arr = children->asArray();
                    for (QC::usize i = 0; i < arr->size(); ++i)
                    {
                        self(self, (*arr)[i], created->asPanel(), static_cast<QC::i32>(bounds.width), static_cast<QC::i32>(bounds.height));
                    }
                }
            }
        };

        const QC::JSON::Array *arr = controls->asArray();
        for (QC::usize i = 0; i < arr->size(); ++i)
        {
            buildControl(buildControl, (*arr)[i], nullptr, static_cast<QC::i32>(m_screenWidth), static_cast<QC::i32>(m_screenHeight));
        }

        if (m_jsonRootControls.size() == 0)
        {
            QC_LOG_WARN(LOG_MODULE, "desktop.json produced no controls; using hardcoded desktop\n");
            clearJsonDesktopState();
            return false;
        }

        QC_LOG_INFO(LOG_MODULE, "Desktop initialized from /desktop.json (%u controls)\n", static_cast<unsigned>(m_jsonControls.size()));
        return true;
    }

    void Desktop::createTopBar()
    {
        // TopBar: full width, at top
        QW::Rect topBarBounds = {0, 0, m_screenWidth, TOP_BAR_HEIGHT};
        m_topBar = new QW::Controls::Panel(m_desktopWindow, topBarBounds);
        m_topBar->setBorderStyle(QW::Controls::BorderStyle::None);
        m_topBar->setFrameVisible(false);

        if (m_desktopWindow && m_desktopWindow->root())
        {
            m_desktopWindow->root()->addChild(m_topBar);
        }

        // Logo button (left)
        QW::Rect logoBounds = {8, 6, 20, 20};
        m_logoButton = new QW::Controls::Button(m_desktopWindow, "Q", logoBounds);
        m_logoButton->setVisualStyle(QW::Controls::ButtonStyle::Vista);
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

        if (m_desktopWindow && m_desktopWindow->root())
        {
            m_desktopWindow->root()->addChild(m_sidebar);
        }

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

        if (m_desktopWindow && m_desktopWindow->root())
        {
            m_desktopWindow->root()->addChild(m_taskbar);
        }
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
                if (static_cast<SidebarItem>(i) == m_selectedSidebarItem)
                {
                    applyVistaButtonStyle(m_sidebarButtons[i], colors.sidebarSelected);
                }
                else
                {
                    m_sidebarButtons[i]->setVisualStyle(QW::Controls::ButtonStyle::Flat);
                    m_sidebarButtons[i]->setBackgroundColor(colors.sidebarBg);
                    m_sidebarButtons[i]->setHoverColor(colors.sidebarHover);
                    m_sidebarButtons[i]->setTextColor(colors.sidebarText);
                }
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
            applyVistaButtonStyle(m_logoButton, accent());
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

        if (m_jsonDriven)
        {
            for (QC::usize i = 0; i < m_jsonRootControls.size(); ++i)
            {
                if (m_jsonRootControls[i])
                    m_jsonRootControls[i]->paint();
            }
            return;
        }

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

            if (desktop->m_selectedSidebarItem == SidebarItem::Terminal)
            {
                desktop->openTerminal();
            }

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

    void Desktop::onJsonTerminalClick(QW::Controls::Button *button, void *userData)
    {
        (void)button;
        if (!userData)
            return;

        Desktop *desktop = static_cast<Desktop *>(userData);
        desktop->openTerminal();
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
