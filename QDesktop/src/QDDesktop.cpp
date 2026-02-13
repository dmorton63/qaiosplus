// QDesktop Desktop - Implementation using Window and Controls
// Namespace: QD

#include "QDDesktop.h"
#include "QDColorUtils.h"
#include "QWWindowManager.h"
#include "QCJson.h"
#include "QCLogger.h"
#include "QFSVFS.h"
#include "QFSFile.h"
#include "QWStyleSystem.h"
#include "QWStyleTypes.h"
#include "QDShutdownDialog.h"
#include "QKEventManager.h"
#include "QKShutdownController.h"
#include "QGPainter.h"

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

        inline QW::ButtonRole roleForJsonButton(const char *id, const QC::JSON::Value *controlValue)
        {
            if (controlValue)
            {
                if (const QC::JSON::Value *roleValue = controlValue->find("role"))
                {
                    const char *roleText = roleValue->isString() ? roleValue->asString(nullptr) : nullptr;
                    if (roleText)
                    {
                        QW::ButtonRole parsed;
                        if (QW::buttonRoleFromString(roleText, &parsed))
                        {
                            return parsed;
                        }

                        const char *warnId = id ? id : "<unnamed>";
                        QC_LOG_WARN(LOG_MODULE, "Unknown button role '%s' on control '%s'", roleText, warnId);
                    }
                }
            }

            if (id)
            {
                if (QC::String::strcmp(id, "shutDownButton") == 0)
                    return QW::ButtonRole::Destructive;
                if (QC::String::strcmp(id, "startButton") == 0)
                    return QW::ButtonRole::Accent;
                if (startsWith(id, "btn"))
                    return QW::ButtonRole::Sidebar;
            }

            return QW::ButtonRole::Default;
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
          m_themeLoaded(false),
          m_topBar(nullptr),
          m_sidebar(nullptr),
          m_taskbar(nullptr),
          m_jsonStartButton(nullptr),
          m_jsonShutdownButton(nullptr),
          m_logoButton(nullptr),
          m_titleLabel(nullptr),
          m_clockLabel(nullptr),
          m_taskbarWindowBaseX(4),
          m_selectedSidebarItem(SidebarItem::Home),
          m_taskbarWindowCount(0),
          m_hours(10),
          m_minutes(32),
          m_terminal(nullptr),
          m_shutdownDialog(nullptr)
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

        resetThemeOverrides();
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
            recomputeTaskbarWindowBase();

            // Apply colors based on current style
            applyColors();
        }

        QK::Shutdown::Controller::instance().registerUIHandler(&Desktop::onShutdownRequested, this);

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

        QK::Shutdown::Controller::instance().registerUIHandler(nullptr, nullptr);

        if (m_shutdownDialog)
        {
            delete m_shutdownDialog;
            m_shutdownDialog = nullptr;
        }

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

    void Desktop::toggleTerminal()
    {
        if (!m_terminal)
        {
            m_terminal = new Terminal(this);
        }

        if (!m_terminal)
            return;

        if (m_terminal->isOpen())
        {
            m_terminal->close();
        }
        else
        {
            m_terminal->open();
        }
    }

    void Desktop::recomputeTaskbarWindowBase()
    {
        m_taskbarWindowBaseX = 4;

        if (!m_taskbar)
            return;

        auto considerControl = [&](QW::Controls::IControl *ctrl)
        {
            if (!ctrl)
                return;

            QW::Rect bounds = ctrl->bounds();
            QC::i32 right = bounds.x + static_cast<QC::i32>(bounds.width) + 8;
            if (right > m_taskbarWindowBaseX)
            {
                m_taskbarWindowBaseX = right;
            }
        };

        considerControl(m_jsonStartButton);
        considerControl(m_jsonShutdownButton);
    }

    void Desktop::showShutdownPrompt(QK::Shutdown::Reason reason)
    {
        if (!m_shutdownDialog)
        {
            m_shutdownDialog = new ShutdownDialog(this);
        }

        if (m_shutdownDialog)
        {
            m_shutdownDialog->open(reason);
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
        m_jsonStartButton = nullptr;
        m_jsonShutdownButton = nullptr;
        m_taskbarWindowBaseX = 4;

        m_jsonDriven = false;

        resetThemeOverrides();
    }

    void Desktop::resetThemeOverrides()
    {
        m_themeOverrides = ThemeOverrides{};
        m_themeLoaded = false;
    }

    float Desktop::clamp01(float value)
    {
        if (value < 0.0f)
            return 0.0f;
        if (value > 1.0f)
            return 1.0f;
        return value;
    }

    QC::u8 Desktop::clampToByte(QC::u32 value)
    {
        return value > 255 ? 255 : static_cast<QC::u8>(value);
    }

    bool Desktop::parseColorOverride(const QC::JSON::Value *object, const char *key, ColorOverride &target)
    {
        if (!object || !object->isObject())
            return false;
        const QC::JSON::Value *value = object->find(key);
        const char *text = stringOrNull(value);
        if (!text)
            return false;
        QC::Color parsed;
        if (!parseColorString(text, parsed))
            return false;
        target.set = true;
        target.value = parsed;
        return true;
    }

    bool Desktop::parseUnsignedOverride(const QC::JSON::Value *object, const char *key, QC::u32 &outValue)
    {
        if (!object || !object->isObject())
            return false;
        const QC::JSON::Value *value = object->find(key);
        if (!value || !value->isNumber())
            return false;
        double number = value->asNumber(static_cast<double>(outValue));
        if (number < 0.0)
            number = 0.0;
        outValue = static_cast<QC::u32>(number);
        return true;
    }

    bool Desktop::parseSignedOverride(const QC::JSON::Value *object, const char *key, QC::i32 &outValue)
    {
        if (!object || !object->isObject())
            return false;
        const QC::JSON::Value *value = object->find(key);
        if (!value || !value->isNumber())
            return false;
        outValue = static_cast<QC::i32>(value->asNumber(static_cast<double>(outValue)));
        return true;
    }

    bool Desktop::parseBoolOverride(const QC::JSON::Value *object, const char *key, bool &outValue)
    {
        if (!object || !object->isObject())
            return false;
        const QC::JSON::Value *value = object->find(key);
        if (!value || !value->isBool())
            return false;
        outValue = value->asBool(false);
        return true;
    }

    bool Desktop::parseButtonStyleOverride(const QC::JSON::Value *buttons,
                                           const char *key,
                                           ButtonStyleOverrides &out)
    {
        out = ButtonStyleOverrides{};
        if (!buttons || !buttons->isObject())
            return false;

        const QC::JSON::Value *value = buttons->find(key);
        if (!value || !value->isObject())
            return false;

        bool changed = false;
        changed |= parseColorOverride(value, "fillNormal", out.fillNormal);
        changed |= parseColorOverride(value, "fillHover", out.fillHover);
        changed |= parseColorOverride(value, "fillPressed", out.fillPressed);
        changed |= parseColorOverride(value, "text", out.text);
        changed |= parseColorOverride(value, "border", out.border);

        bool glass = false;
        if (parseBoolOverride(value, "glass", glass))
        {
            out.glassSet = true;
            out.glass = glass;
            changed = true;
        }

        const QC::JSON::Value *shine = value->find("shineIntensity");
        if (shine && shine->isNumber())
        {
            out.shineSet = true;
            out.shineIntensity = static_cast<float>(shine->asNumber(out.shineIntensity));
            changed = true;
        }

        if (!changed)
        {
            out = ButtonStyleOverrides{};
        }

        return changed;
    }

    bool Desktop::loadThemeDefinition(const QC::JSON::Value *themeValue)
    {
        m_themeLoaded = false;
        m_themeDefinition.reset();
        if (!themeValue)
            return false;

        auto tryLoadPath = [&](const char *path) -> bool
        {
            if (!path || !*path)
                return false;
            if (!m_themeDefinition.loadFromFile(path))
            {
                QC_LOG_WARN(LOG_MODULE, "Failed to load theme file %s", path);
                return false;
            }
            m_themeLoaded = true;
            return true;
        };

        if (themeValue->isString())
        {
            const char *path = themeValue->asString(nullptr);
            return tryLoadPath(path);
        }

        if (!themeValue->isObject())
            return false;

        if (tryLoadPath(stringOrNull(themeValue->find("file"))))
            return true;
        if (tryLoadPath(stringOrNull(themeValue->find("path"))))
            return true;

        if (const QC::JSON::Value *definition = themeValue->find("definition"))
        {
            if (definition->isObject())
            {
                m_themeLoaded = m_themeDefinition.loadFromJson(*definition);
                return m_themeLoaded;
            }
        }

        if (themeValue->find("colors") || themeValue->find("effects") || themeValue->find("animations") || themeValue->find("base"))
        {
            m_themeLoaded = m_themeDefinition.loadFromJson(*themeValue);
            return m_themeLoaded;
        }

        return false;
    }

    void Desktop::applyLoadedThemeToOverrides()
    {
        if (!m_themeLoaded)
            return;

        const auto applyColor = [](ColorOverride &target, const QC::Color &value)
        {
            target.set = true;
            target.value = value;
        };

        const ThemeColorPalette &palette = m_themeDefinition.colors();
        applyColor(m_themeOverrides.palette.accent, palette.accentPrimary);
        applyColor(m_themeOverrides.palette.accentLight, palette.accentSecondary);
        applyColor(m_themeOverrides.palette.accentDark, palette.accentPrimary.darker(0.2f));
        applyColor(m_themeOverrides.palette.panel, palette.windowBackground);
        applyColor(m_themeOverrides.palette.panelBorder, palette.border);
        applyColor(m_themeOverrides.palette.text, palette.textPrimary);
        applyColor(m_themeOverrides.palette.textSecondary, palette.textSecondary);

        const ThemeEffects &effects = m_themeDefinition.effects();
        m_themeOverrides.metrics.cornerRadiusSet = true;
        m_themeOverrides.metrics.cornerRadius = effects.border.radius;
        m_themeOverrides.metrics.buttonCornerRadiusSet = true;
        m_themeOverrides.metrics.buttonCornerRadius = effects.border.radius;
        m_themeOverrides.metrics.borderWidthSet = true;
        m_themeOverrides.metrics.borderWidth = effects.border.width;
        applyColor(m_themeOverrides.effects.borderColor, effects.border.color);

        auto &shadow = m_themeOverrides.effects.shadow;
        shadow.offsetXSet = true;
        shadow.offsetX = effects.shadow.offsetX;
        shadow.offsetYSet = true;
        shadow.offsetY = effects.shadow.offsetY;
        shadow.blurSet = true;
        shadow.blurRadius = effects.shadow.blurRadius;
        applyColor(shadow.color, effects.shadow.color);

        auto &glow = m_themeOverrides.effects.glow;
        glow.radiusSet = true;
        glow.radius = effects.glow.radius;
        glow.intensitySet = true;
        glow.intensity = effects.glow.intensity;
        applyColor(glow.color, effects.glow.color);

        auto assignButton = [&](QW::ButtonRole role,
                                const QC::Color &fillNormal,
                                const QC::Color &fillHover,
                                const QC::Color &fillPressed,
                                const QC::Color &textColor,
                                const QC::Color &borderColor,
                                bool glass)
        {
            auto &entry = m_themeOverrides.button[static_cast<QC::u32>(role)];
            entry.fillNormal.set = true;
            entry.fillNormal.value = fillNormal;
            entry.fillHover.set = true;
            entry.fillHover.value = fillHover;
            entry.fillPressed.set = true;
            entry.fillPressed.value = fillPressed;
            entry.text.set = true;
            entry.text.value = textColor;
            entry.border.set = true;
            entry.border.value = borderColor;
            entry.glassSet = true;
            entry.glass = glass;
        };

        assignButton(QW::ButtonRole::Default,
                     palette.buttonNormal,
                     palette.buttonHover,
                     palette.buttonPressed,
                     palette.textPrimary,
                     palette.border,
                     false);

        assignButton(QW::ButtonRole::Sidebar,
                     palette.buttonNormal,
                     palette.buttonHover,
                     palette.buttonPressed,
                     palette.textSecondary,
                     palette.border,
                     false);

        assignButton(QW::ButtonRole::Accent,
                     palette.accentPrimary,
                     palette.accentSecondary,
                     palette.accentPrimary.darker(0.25f),
                     palette.textPrimary,
                     palette.accentPrimary.darker(0.3f),
                     true);

        m_themeOverrides.active = true;
    }

    void Desktop::parseThemeOverrides(const QC::JSON::Value *themeValue)
    {
        resetThemeOverrides();

        if (!themeValue)
            return;

        if (loadThemeDefinition(themeValue))
        {
            applyLoadedThemeToOverrides();
        }

        if (!themeValue->isObject())
            return;

        const QC::JSON::Value *overrides = themeValue->find("overrides");
        if (!overrides || !overrides->isObject())
            return;

        if (const QC::JSON::Value *palette = overrides->find("palette"))
        {
            bool changed = false;
            changed |= parseColorOverride(palette, "accent", m_themeOverrides.palette.accent);
            changed |= parseColorOverride(palette, "accentLight", m_themeOverrides.palette.accentLight);
            changed |= parseColorOverride(palette, "accentDark", m_themeOverrides.palette.accentDark);
            changed |= parseColorOverride(palette, "panel", m_themeOverrides.palette.panel);
            changed |= parseColorOverride(palette, "panelBorder", m_themeOverrides.palette.panelBorder);
            changed |= parseColorOverride(palette, "text", m_themeOverrides.palette.text);
            changed |= parseColorOverride(palette, "textSecondary", m_themeOverrides.palette.textSecondary);
            if (changed)
                m_themeOverrides.active = true;
        }

        if (const QC::JSON::Value *metrics = overrides->find("metrics"))
        {
            QC::u32 value = 0;
            if (parseUnsignedOverride(metrics, "cornerRadius", value))
            {
                m_themeOverrides.metrics.cornerRadiusSet = true;
                m_themeOverrides.metrics.cornerRadius = value;
                m_themeOverrides.active = true;
            }

            value = 0;
            if (parseUnsignedOverride(metrics, "buttonCornerRadius", value))
            {
                m_themeOverrides.metrics.buttonCornerRadiusSet = true;
                m_themeOverrides.metrics.buttonCornerRadius = value;
                m_themeOverrides.active = true;
            }

            value = 0;
            if (parseUnsignedOverride(metrics, "borderWidth", value))
            {
                m_themeOverrides.metrics.borderWidthSet = true;
                m_themeOverrides.metrics.borderWidth = value;
                m_themeOverrides.active = true;
            }
        }

        if (const QC::JSON::Value *buttons = overrides->find("button"))
        {
            auto assign = [&](const char *key, QW::ButtonRole role)
            {
                if (parseButtonStyleOverride(buttons, key, m_themeOverrides.button[static_cast<QC::u32>(role)]))
                {
                    m_themeOverrides.active = true;
                }
            };

            assign("sidebar", QW::ButtonRole::Sidebar);
            assign("accent", QW::ButtonRole::Accent);
            assign("destructive", QW::ButtonRole::Destructive);
        }

        if (const QC::JSON::Value *effects = overrides->find("effects"))
        {
            if (const QC::JSON::Value *border = effects->find("border"))
            {
                bool changed = false;
                changed |= parseColorOverride(border, "color", m_themeOverrides.effects.borderColor);

                QC::u32 value = 0;
                if (parseUnsignedOverride(border, "width", value))
                {
                    m_themeOverrides.metrics.borderWidthSet = true;
                    m_themeOverrides.metrics.borderWidth = value;
                    changed = true;
                }

                value = 0;
                if (parseUnsignedOverride(border, "radius", value))
                {
                    m_themeOverrides.metrics.cornerRadiusSet = true;
                    m_themeOverrides.metrics.cornerRadius = value;
                    m_themeOverrides.metrics.buttonCornerRadiusSet = true;
                    m_themeOverrides.metrics.buttonCornerRadius = value;
                    changed = true;
                }

                if (changed)
                    m_themeOverrides.active = true;
            }

            if (const QC::JSON::Value *shadow = effects->find("shadow"))
            {
                bool changed = false;
                QC::i32 signedValue = 0;
                if (parseSignedOverride(shadow, "offsetX", signedValue))
                {
                    m_themeOverrides.effects.shadow.offsetXSet = true;
                    m_themeOverrides.effects.shadow.offsetX = signedValue;
                    changed = true;
                }

                signedValue = 0;
                if (parseSignedOverride(shadow, "offsetY", signedValue))
                {
                    m_themeOverrides.effects.shadow.offsetYSet = true;
                    m_themeOverrides.effects.shadow.offsetY = signedValue;
                    changed = true;
                }

                QC::u32 blur = 0;
                if (parseUnsignedOverride(shadow, "blur", blur))
                {
                    m_themeOverrides.effects.shadow.blurSet = true;
                    m_themeOverrides.effects.shadow.blurRadius = blur;
                    changed = true;
                }

                if (parseColorOverride(shadow, "color", m_themeOverrides.effects.shadow.color))
                {
                    changed = true;
                }

                if (changed)
                    m_themeOverrides.active = true;
            }

            if (const QC::JSON::Value *glow = effects->find("glow"))
            {
                bool changed = false;
                QC::u32 value = 0;
                if (parseUnsignedOverride(glow, "radius", value))
                {
                    m_themeOverrides.effects.glow.radiusSet = true;
                    m_themeOverrides.effects.glow.radius = value;
                    changed = true;
                }

                value = 0;
                if (parseUnsignedOverride(glow, "intensity", value))
                {
                    m_themeOverrides.effects.glow.intensitySet = true;
                    m_themeOverrides.effects.glow.intensity = value;
                    changed = true;
                }

                if (parseColorOverride(glow, "color", m_themeOverrides.effects.glow.color))
                {
                    changed = true;
                }

                if (changed)
                    m_themeOverrides.active = true;
            }
        }
    }

    void Desktop::applyThemeOverrides(QW::StyleSnapshot &snapshot) const
    {
        if (!m_themeOverrides.active)
            return;

        const auto &palette = m_themeOverrides.palette;

        if (palette.accent.set)
            snapshot.palette.accent = palette.accent.value;

        if (palette.panel.set)
        {
            snapshot.palette.panelBackground = palette.panel.value;
            snapshot.palette.buttonFace = palette.panel.value;
            snapshot.palette.buttonHover = palette.panel.value.lighter(0.15f);
            snapshot.palette.buttonPressed = palette.panel.value.darker(0.2f);
        }

        if (palette.panelBorder.set)
        {
            snapshot.palette.buttonBorder = palette.panelBorder.value;
            snapshot.palette.windowBorderActive = palette.panelBorder.value;
            snapshot.palette.windowBorderInactive = palette.panelBorder.value.darker(0.3f);
        }

        if (m_themeOverrides.effects.borderColor.set)
        {
            const QC::Color borderColor = m_themeOverrides.effects.borderColor.value;
            snapshot.palette.buttonBorder = borderColor;
            snapshot.palette.windowBorderActive = borderColor;
            snapshot.palette.windowBorderInactive = borderColor.darker(0.3f);
        }

        if (palette.text.set)
        {
            snapshot.palette.controlText = palette.text.value;

            auto applyTextIfUnset = [&](QW::ButtonRole role, const QC::Color &color)
            {
                const QC::u32 idx = static_cast<QC::u32>(role);
                if (!m_themeOverrides.button[idx].text.set)
                {
                    snapshot.buttonStyles[idx].text = color;
                }
            };

            applyTextIfUnset(QW::ButtonRole::Default, palette.text.value);
            applyTextIfUnset(QW::ButtonRole::Taskbar, palette.text.value);
        }

        if (palette.textSecondary.set)
        {
            const QC::u32 sidebarIdx = static_cast<QC::u32>(QW::ButtonRole::Sidebar);
            if (!m_themeOverrides.button[sidebarIdx].text.set)
            {
                snapshot.buttonStyles[sidebarIdx].text = palette.textSecondary.value;
            }
        }

        const QC::u32 accentIdx = static_cast<QC::u32>(QW::ButtonRole::Accent);
        auto &accentOverride = m_themeOverrides.button[accentIdx];
        auto &accentSpec = snapshot.buttonStyles[accentIdx];

        if (palette.accent.set && !accentOverride.fillNormal.set)
            accentSpec.fillNormal = palette.accent.value;
        if (palette.accentLight.set && !accentOverride.fillHover.set)
            accentSpec.fillHover = palette.accentLight.value;
        if (palette.accentDark.set && !accentOverride.fillPressed.set)
            accentSpec.fillPressed = palette.accentDark.value;

        bool updateButtonCorner = false;
        if (m_themeOverrides.metrics.cornerRadiusSet)
        {
            snapshot.metrics.windowCornerRadius = m_themeOverrides.metrics.cornerRadius;
            if (!m_themeOverrides.metrics.buttonCornerRadiusSet)
            {
                snapshot.metrics.buttonCornerRadius = m_themeOverrides.metrics.cornerRadius;
            }
            updateButtonCorner = true;
        }

        if (m_themeOverrides.metrics.buttonCornerRadiusSet)
        {
            snapshot.metrics.buttonCornerRadius = m_themeOverrides.metrics.buttonCornerRadius;
            updateButtonCorner = true;
        }

        bool updateBorderWidth = false;
        if (m_themeOverrides.metrics.borderWidthSet)
        {
            snapshot.metrics.borderWidth = m_themeOverrides.metrics.borderWidth;
            updateBorderWidth = true;
        }

        if (updateButtonCorner)
        {
            const QC::u32 radius = snapshot.metrics.buttonCornerRadius;
            for (QC::u32 i = 0; i < static_cast<QC::u32>(QW::ButtonRole::Count); ++i)
            {
                snapshot.buttonStyles[i].cornerRadius = radius;
            }
        }

        if (updateBorderWidth)
        {
            const QC::u32 width = snapshot.metrics.borderWidth;
            for (QC::u32 i = 0; i < static_cast<QC::u32>(QW::ButtonRole::Count); ++i)
            {
                snapshot.buttonStyles[i].borderWidth = width;
            }
        }

        const auto &shadow = m_themeOverrides.effects.shadow;
        if (shadow.blurSet)
        {
            snapshot.metrics.shadowSize = shadow.blurRadius;
            snapshot.metrics.buttonShadowSoftness = shadow.blurRadius;
        }
        if (shadow.offsetXSet)
        {
            snapshot.metrics.buttonShadowOffsetX = shadow.offsetX;
        }
        if (shadow.offsetYSet)
        {
            snapshot.metrics.buttonShadowOffsetY = shadow.offsetY;
        }

        const auto &glow = m_themeOverrides.effects.glow;
        if (glow.radiusSet)
        {
            snapshot.metrics.focusRingWidth = glow.radius;
        }

        if (glow.radiusSet || glow.intensitySet || glow.color.set)
        {
            const auto applyGlow = [&](QW::ButtonRole role)
            {
                const QC::u32 idx = static_cast<QC::u32>(role);
                auto &spec = snapshot.buttonStyles[idx];
                QC::Color color = glow.color.set ? glow.color.value : spec.glow;
                if (glow.intensitySet)
                {
                    color.a = clampToByte(glow.intensity);
                }
                spec.glow = color;
                if (glow.radiusSet)
                {
                    spec.castsShadow = glow.radius > 0;
                }
            };

            applyGlow(QW::ButtonRole::Accent);
            applyGlow(QW::ButtonRole::SidebarSelected);
            applyGlow(QW::ButtonRole::Destructive);
            applyGlow(QW::ButtonRole::TaskbarActive);
        }

        auto applyButtonOverride = [&](QW::ButtonRole role)
        {
            const QC::u32 idx = static_cast<QC::u32>(role);
            const auto &data = m_themeOverrides.button[idx];
            if (!data.hasAny())
                return;

            auto &spec = snapshot.buttonStyles[idx];
            if (data.fillNormal.set)
                spec.fillNormal = data.fillNormal.value;
            if (data.fillHover.set)
                spec.fillHover = data.fillHover.value;
            if (data.fillPressed.set)
                spec.fillPressed = data.fillPressed.value;
            if (data.text.set)
                spec.text = data.text.value;
            if (data.border.set)
                spec.border = data.border.value;
            if (data.glassSet)
                spec.glass = data.glass;
            if (data.shineSet)
            {
                const float amount = clamp01(data.shineIntensity);
                const QC::u8 alpha = static_cast<QC::u8>(amount * 255.0f);
                spec.glow = spec.fillNormal.withAlpha(alpha);
                spec.overlayHover = QC::Color(255, 255, 255, alpha);
                spec.overlayPressed = spec.fillPressed.withAlpha(static_cast<QC::u8>(alpha * 0.7f));
            }
        };

        applyButtonOverride(QW::ButtonRole::Default);
        applyButtonOverride(QW::ButtonRole::Sidebar);
        applyButtonOverride(QW::ButtonRole::Accent);
        applyButtonOverride(QW::ButtonRole::Destructive);
    }

    void Desktop::applyThemeToDesktopColors(DesktopColors &colors) const
    {
        if (!m_themeOverrides.active)
            return;

        const auto &palette = m_themeOverrides.palette;

        if (palette.panel.set)
        {
            const QC::u8 topAlpha = colors.topBarBg.a;
            const QC::u8 sidebarAlpha = colors.sidebarBg.a;
            const QC::u8 taskbarAlpha = colors.taskbarBg.a;
            colors.topBarBg = palette.panel.value;
            colors.topBarBg.a = topAlpha;
            colors.sidebarBg = palette.panel.value.darker(0.05f);
            colors.sidebarBg.a = sidebarAlpha;
            colors.taskbarBg = palette.panel.value.darker(0.1f);
            colors.taskbarBg.a = taskbarAlpha;
            colors.windowBg = palette.panel.value.lighter(0.05f);
            colors.bgTop = palette.panel.value.lighter(0.08f);
            colors.bgBottom = palette.panel.value.darker(0.08f);
        }

        if (palette.panelBorder.set)
        {
            colors.topBarDivider = palette.panelBorder.value;
            colors.windowBorder = palette.panelBorder.value;
        }

        if (m_themeOverrides.effects.borderColor.set)
        {
            colors.topBarDivider = m_themeOverrides.effects.borderColor.value;
            colors.windowBorder = m_themeOverrides.effects.borderColor.value;
        }

        if (palette.text.set)
        {
            colors.topBarText = palette.text.value;
            colors.taskbarText = palette.text.value;
            colors.windowTitleText = palette.text.value;
        }

        if (palette.textSecondary.set)
        {
            colors.sidebarText = palette.textSecondary.value;
        }

        if (palette.accent.set)
        {
            colors.sidebarSelected = palette.accent.value;
            colors.windowBorder = palette.accent.value;
            QC::Color accentActive = palette.accent.value;
            accentActive.a = colors.taskbarActiveWindow.a;
            colors.taskbarActiveWindow = accentActive;
        }

        if (palette.accentLight.set)
        {
            QC::Color sidebarHover = palette.accentLight.value;
            sidebarHover.a = colors.sidebarHover.a;
            colors.sidebarHover = sidebarHover;

            QC::Color taskbarHover = palette.accentLight.value;
            taskbarHover.a = colors.taskbarHover.a;
            colors.taskbarHover = taskbarHover;
        }

        if (palette.accentDark.set)
        {
            colors.windowTitleBg = palette.accentDark.value;
        }

        if (m_themeOverrides.effects.shadow.color.set)
        {
            colors.windowShadow = m_themeOverrides.effects.shadow.color.value;
        }
        else if (m_themeOverrides.effects.shadow.blurSet && m_themeOverrides.effects.shadow.blurRadius == 0)
        {
            colors.windowShadow.a = 0;
        }
    }

    bool Desktop::tryInitializeFromJson()
    {
        resetThemeOverrides();

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

                button->setRole(roleForJsonButton(id, controlValue));

                // Wire up known desktop actions.
                if (id && QC::String::strcmp(id, "btnTerminal") == 0)
                {
                    button->setClickHandler(onJsonTerminalClick, this);
                }
                else if (id && QC::String::strcmp(id, "shutDownButton") == 0)
                {
                    button->setClickHandler(onJsonShutdownClick, this);
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

                if (!m_jsonStartButton && QC::String::strcmp(id, "startButton") == 0)
                    m_jsonStartButton = static_cast<QW::Controls::Button *>(created);
                if (!m_jsonShutdownButton && QC::String::strcmp(id, "shutDownButton") == 0)
                    m_jsonShutdownButton = static_cast<QW::Controls::Button *>(created);
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

        recomputeTaskbarWindowBase();

        parseThemeOverrides(desktop->find("theme"));

        DesktopColors colors = currentColors();
        applyAccent(colors);
        applyThemeToDesktopColors(colors);
        publishStyleSnapshot(colors);

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
        m_logoButton->setRole(QW::ButtonRole::Accent);
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
            m_sidebarButtons[i]->setRole(QW::ButtonRole::Sidebar);
            m_sidebar->addChild(m_sidebarButtons[i]);
        }

        updateSidebarButtonRoles();
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
        applyThemeToDesktopColors(colors);

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
            m_logoButton->setRole(QW::ButtonRole::Accent);
        }

        updateSidebarButtonRoles();

        publishStyleSnapshot(colors);
    }

    void Desktop::publishStyleSnapshot(const DesktopColors &colors)
    {
        QW::StyleSnapshot::VistaThemeConfig config;
        config.windowBackground = colors.windowBg;
        config.windowBorder = colors.windowBorder;
        config.sidebarBackground = colors.sidebarBg;
        config.sidebarHover = colors.sidebarHover;
        config.sidebarSelected = colors.sidebarSelected;
        config.sidebarText = colors.sidebarText;
        config.topBarDivider = colors.topBarDivider;
        config.taskbarBackground = colors.taskbarBg;
        config.taskbarHover = colors.taskbarHover;
        config.taskbarText = colors.taskbarText;
        config.taskbarActiveWindow = colors.taskbarActiveWindow;
        config.desktopBackgroundTop = colors.bgTop;
        config.desktopBackgroundBottom = colors.bgBottom;
        config.windowShadow = colors.windowShadow;
        config.accent = accent();

        QW::StyleSnapshot snapshot = QW::StyleSnapshot::makeVista(config);
        applyThemeOverrides(snapshot);
        QW::StyleSystem::instance().setStyle(snapshot);
    }

    void Desktop::updateSidebarButtonRoles()
    {
        for (QC::u8 i = 0; i < static_cast<QC::u8>(SidebarItem::Count); ++i)
        {
            auto *button = m_sidebarButtons[i];
            if (!button)
                continue;

            SidebarItem item = static_cast<SidebarItem>(i);
            QW::ButtonRole role = (item == SidebarItem::Power) ? QW::ButtonRole::Destructive
                                                               : QW::ButtonRole::Sidebar;
            if (item == m_selectedSidebarItem)
            {
                role = QW::ButtonRole::SidebarSelected;
            }
            button->setRole(role);
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
            char timeStr[16];
            QC::u32 displayHour = m_hours;
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
        QC::i32 x = static_cast<QC::i32>(m_taskbarWindowBaseX + m_taskbarWindowCount * (buttonWidth + 4));
        QC::i32 y = static_cast<QC::i32>((TASKBAR_HEIGHT - buttonHeight) / 2);

        QW::Rect btnBounds = {x, y, buttonWidth, buttonHeight};

        auto *btn = new QW::Controls::Button(m_desktopWindow, title ? title : "Window", btnBounds);
        btn->setId(windowId);
        btn->setClickHandler(onTaskbarClick, this);
        btn->setRole(QW::ButtonRole::Taskbar);

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
                        QC::i32 x = static_cast<QC::i32>(m_taskbarWindowBaseX + j * (buttonWidth + 4));
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
        for (QC::u32 i = 0; i < m_taskbarWindowCount; ++i)
        {
            bool isActive = (m_taskbarEntries[i].windowId == windowId);
            m_taskbarEntries[i].isActive = isActive;

            if (m_taskbarEntries[i].button)
            {
                m_taskbarEntries[i].button->setRole(isActive ? QW::ButtonRole::TaskbarActive
                                                             : QW::ButtonRole::Taskbar);
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

        QW::Controls::PaintContext paintContext{};
        paintContext.window = m_desktopWindow;
        paintContext.styleRenderer = m_desktopWindow ? m_desktopWindow->styleRenderer() : nullptr;
        paintContext.painter = m_desktopWindow ? m_desktopWindow->painter() : nullptr;

        if (m_jsonDriven)
        {
            for (QC::usize i = 0; i < m_jsonRootControls.size(); ++i)
            {
                if (m_jsonRootControls[i])
                    m_jsonRootControls[i]->paint(paintContext);
            }
            return;
        }

        // Paint panels (they paint their children)
        if (m_topBar)
            m_topBar->paint(paintContext);
        if (m_sidebar)
            m_sidebar->paint(paintContext);
        if (m_taskbar)
            m_taskbar->paint(paintContext);
    }

    void Desktop::paintBackground()
    {
        if (!m_desktopWindow)
            return;

        const auto &style = QW::StyleSystem::instance().currentStyle();
        QW::Color top = style.palette.desktopBackgroundTop;
        QW::Color bottom = style.palette.desktopBackgroundBottom;

        QW::Rect bounds = {0, 0, m_screenWidth, m_screenHeight};
        QG::IPainter *painter = m_desktopWindow->painter();
        if (!painter)
            return;

        if (top == bottom)
        {
            painter->fillRect(bounds, top);
        }
        else
        {
            painter->fillGradientV(bounds, top, bottom);
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
                desktop->toggleTerminal();
            }
            else if (desktop->m_selectedSidebarItem == SidebarItem::Power)
            {
                QK::Event::EventManager::instance().postShutdownEvent(
                    QK::Event::Type::ShutdownRequest,
                    static_cast<QC::u32>(QK::Shutdown::Reason::SidebarPowerButton));
            }

            desktop->updateSidebarButtonRoles();
        }
    }

    bool Desktop::onShutdownRequested(QK::Shutdown::Reason reason, void *userData)
    {
        Desktop *desktop = static_cast<Desktop *>(userData);
        if (!desktop)
            return false;

        desktop->showShutdownPrompt(reason);
        return true;
    }

    void Desktop::onJsonTerminalClick(QW::Controls::Button *button, void *userData)
    {
        (void)button;
        if (!userData)
            return;

        Desktop *desktop = static_cast<Desktop *>(userData);
        desktop->toggleTerminal();
    }

    void Desktop::onJsonShutdownClick(QW::Controls::Button *button, void *userData)
    {
        (void)button;
        (void)userData;

        QK::Event::EventManager::instance().postShutdownEvent(
            QK::Event::Type::ShutdownRequest,
            static_cast<QC::u32>(QK::Shutdown::Reason::UserRequest));
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
