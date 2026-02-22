// QWindowing StyleRenderer implementation

#include "QWStyleRenderer.h"
#include "QGPainter.h"
#include "QCUIStyle.h"

namespace QW
{

    namespace
    {
        inline QC::u32 roleIndex(ButtonRole role)
        {
            QC::u32 idx = static_cast<QC::u32>(role);
            if (idx >= static_cast<QC::u32>(ButtonRole::Count))
            {
                idx = 0;
            }
            return idx;
        }

        inline const StyleSnapshot::ButtonStyle &buttonStyle(const StyleSnapshot &snapshot, ButtonRole role)
        {
            return snapshot.buttonStyles[roleIndex(role)];
        }

        inline QC::Rect insetRect(const QC::Rect &rect, QC::u32 inset)
        {
            if (inset == 0)
            {
                return rect;
            }

            QC::Rect result = rect;
            const QC::u32 insetAmount = inset * 2;
            result.x += static_cast<QC::i32>(inset);
            result.y += static_cast<QC::i32>(inset);
            result.width = (result.width > insetAmount) ? (result.width - insetAmount) : 0;
            result.height = (result.height > insetAmount) ? (result.height - insetAmount) : 0;
            return result;
        }

        inline QC::Rect expandRect(const QC::Rect &rect, QC::i32 amount)
        {
            if (amount <= 0)
            {
                return rect;
            }

            QC::Rect result = rect;
            result.x -= amount;
            result.y -= amount;

            const QC::i64 width = static_cast<QC::i64>(result.width) + static_cast<QC::i64>(amount) * 2;
            const QC::i64 height = static_cast<QC::i64>(result.height) + static_cast<QC::i64>(amount) * 2;
            result.width = width > 0 ? static_cast<QC::u32>(width) : 0;
            result.height = height > 0 ? static_cast<QC::u32>(height) : 0;
            return result;
        }

        inline void drawFlatPanelBorder(QG::GraphicsBackend *backend,
                                        const QC::Rect &bounds,
                                        QC::Color color,
                                        QC::u32 width)
        {
            if (!backend || width == 0)
            {
                return;
            }

            backend->drawRect(bounds,
                              QC::Color::transparent(),
                              color,
                              width);
        }

        void drawPanelBorder(const PanelPaintArgs &args,
                             QG::GraphicsBackend *backend,
                             QG::IPainter *painter,
                             QC::Color borderColor,
                             QC::u32 borderWidth)
        {
            if (args.borderStyle == PanelBorderStyle::None || borderWidth == 0)
            {
                return;
            }

            if (args.borderStyle == PanelBorderStyle::Flat || !painter)
            {
                drawFlatPanelBorder(backend, args.bounds, borderColor, borderWidth);
                return;
            }

            const QC::Color light = borderColor.lighter(0.35f);
            const QC::Color dark = borderColor.darker(0.4f);

            switch (args.borderStyle)
            {
            case PanelBorderStyle::Raised:
                painter->drawRaisedBorder(args.bounds, light, dark, borderWidth);
                break;
            case PanelBorderStyle::Sunken:
                painter->drawSunkenBorder(args.bounds, light, dark, borderWidth);
                break;
            case PanelBorderStyle::Etched:
                painter->drawEtchedBorder(args.bounds, light, dark);
                break;
            default:
                drawFlatPanelBorder(backend, args.bounds, borderColor, borderWidth);
                break;
            }
        }
    } // namespace

    const StyleSnapshot &StyleSnapshot::fallback()
    {
        static StyleSnapshot snapshot;
        return snapshot;
    }

    StyleSnapshot StyleSnapshot::makeVista(const VistaThemeConfig &config)
    {
        StyleSnapshot snapshot;

        snapshot.palette.windowBackground = config.windowBackground;
        snapshot.palette.windowBorderActive = config.windowBorder;
        snapshot.palette.windowBorderInactive = config.windowBorder.darker();
        snapshot.palette.panelBackground = config.sidebarBackground;
        snapshot.palette.buttonFace = config.sidebarBackground;
        snapshot.palette.buttonHover = config.sidebarHover;
        snapshot.palette.buttonPressed = config.sidebarBackground.darker(0.6f);
        snapshot.palette.buttonBorder = config.topBarDivider;
        snapshot.palette.controlText = config.sidebarText;
        snapshot.palette.accent = config.accent;
        snapshot.palette.desktopBackgroundTop = config.desktopBackgroundTop;
        snapshot.palette.desktopBackgroundBottom = config.desktopBackgroundBottom;

        snapshot.metrics.windowCornerRadius = (QC::currentUIStyle() == QC::UIStyle::Vista) ? 6 : 4;
        snapshot.metrics.buttonCornerRadius = snapshot.metrics.windowCornerRadius;
        snapshot.metrics.borderWidth = 1;
        snapshot.metrics.shadowSize = config.windowShadow.a > 0 ? 8 : 0;
        snapshot.metrics.buttonHoverLift = 1;
        snapshot.metrics.buttonPressDepth = 1;
        snapshot.metrics.buttonTextHoverOffset = 0;
        snapshot.metrics.buttonTextPressedOffset = 1;
        snapshot.metrics.buttonShadowOffsetX = 0;
        snapshot.metrics.buttonShadowOffsetY = 2;
        snapshot.metrics.buttonShadowSoftness = 10;
        snapshot.metrics.focusRingWidth = 2;
        snapshot.metrics.textScale = 1.0f;

        const auto configureButton = [&](ButtonRole role, auto &&fn)
        {
            QC::u32 idx = static_cast<QC::u32>(role);
            if (idx >= static_cast<QC::u32>(ButtonRole::Count))
                return;
            auto &spec = snapshot.buttonStyles[idx];
            spec = ButtonStyle{};
            fn(spec);

            if (spec.borderWidth == 0)
                spec.borderWidth = snapshot.metrics.borderWidth;
            spec.cornerRadius = snapshot.metrics.buttonCornerRadius;

            if (spec.fillDisabled.a == 0)
                spec.fillDisabled = spec.fillNormal.darker(0.25f);
            if (spec.textDisabled.a == 0)
                spec.textDisabled = spec.text.withAlpha(180);
            if (spec.borderDisabled.a == 0)
                spec.borderDisabled = spec.border;
        };

        const QC::Color textOnDark(255, 255, 255, 255);
        const QC::Color destructiveBase(200, 64, 64, 255);

        configureButton(ButtonRole::Default, [&](ButtonStyle &spec)
                        {
            spec.fillNormal = config.windowBackground;
            spec.fillHover = config.windowBackground.lighter(0.05f);
            spec.fillPressed = config.windowBackground.darker(0.15f);
            spec.text = config.sidebarText;
            spec.border = config.topBarDivider;
            spec.glow = QC::Color::transparent();
            spec.glass = false;
            spec.overlayHover = config.windowBackground.lighter(0.2f).withAlpha(45);
            spec.overlayPressed = config.windowBackground.darker(0.25f).withAlpha(70);
            spec.outline = config.topBarDivider.withAlpha(140);
            spec.outlineHover = config.topBarDivider.withAlpha(200);
            spec.outlinePressed = config.topBarDivider.darker(0.2f).withAlpha(220);
            spec.focusOutline = config.accent.withAlpha(200);
            spec.castsShadow = false; });

        configureButton(ButtonRole::Accent, [&](ButtonStyle &spec)
                        {
            QC::Color a = config.accent;
            spec.fillNormal = a;
            spec.fillHover = a.lighter(0.08f);
            spec.fillPressed = a.darker(0.2f);
            spec.text = textOnDark;
            spec.border = a.darker(0.25f);
            spec.glow = config.accent.withAlpha(90);
            spec.glass = true;
            spec.overlayHover = QC::Color(255, 255, 255, 90);
            spec.overlayPressed = a.darker(0.35f).withAlpha(110);
            spec.outline = a.darker(0.35f);
            spec.outlineHover = a.lighter(0.06f);
            spec.outlinePressed = a.darker(0.45f);
            spec.focusOutline = config.windowBorder;
            spec.castsShadow = true; });

        configureButton(ButtonRole::Sidebar, [&](ButtonStyle &spec)
                        {
            spec.fillNormal = config.sidebarBackground;
            spec.fillHover = config.sidebarHover;
            spec.fillPressed = config.sidebarHover.darker(0.15f);
            spec.text = config.sidebarText;
            spec.border = config.topBarDivider;
            spec.glow = QC::Color::transparent();
            spec.glass = false;
            spec.overlayHover = QC::Color(255, 255, 255, 35);
            spec.overlayPressed = config.sidebarHover.darker(0.2f).withAlpha(80);
            spec.outline = config.topBarDivider.withAlpha(90);
            spec.outlineHover = config.topBarDivider.withAlpha(140);
            spec.outlinePressed = config.topBarDivider.darker(0.25f).withAlpha(170);
            spec.focusOutline = config.accent.withAlpha(140);
            spec.castsShadow = false; });

        configureButton(ButtonRole::SidebarSelected, [&](ButtonStyle &spec)
                        {
            QC::Color base = config.sidebarSelected;
            spec.fillNormal = base;
            spec.fillHover = base.lighter(0.1f);
            spec.fillPressed = base.darker(0.2f);
            spec.text = textOnDark;
            spec.border = base.darker(0.3f);
            spec.glow = config.accent.withAlpha(70);
            spec.glass = true;
            spec.overlayHover = QC::Color(255, 255, 255, 60);
            spec.overlayPressed = base.darker(0.35f).withAlpha(110);
            spec.outline = base.darker(0.35f);
            spec.outlineHover = base.lighter(0.05f);
            spec.outlinePressed = base.darker(0.45f);
            spec.focusOutline = base.withAlpha(200);
            spec.castsShadow = true; });

        configureButton(ButtonRole::Taskbar, [&](ButtonStyle &spec)
                        {
            spec.fillNormal = config.taskbarBackground;
            spec.fillHover = config.taskbarHover;
            spec.fillPressed = config.taskbarHover.darker(0.15f);
            spec.text = config.taskbarText;
            spec.border = config.topBarDivider;
            spec.glow = QC::Color::transparent();
            spec.glass = false;
            spec.overlayHover = QC::Color(255, 255, 255, 30);
            spec.overlayPressed = config.taskbarHover.darker(0.25f).withAlpha(70);
            spec.outline = config.topBarDivider.withAlpha(120);
            spec.outlineHover = config.topBarDivider.withAlpha(170);
            spec.outlinePressed = config.topBarDivider.darker(0.2f).withAlpha(200);
            spec.focusOutline = config.accent.withAlpha(180);
            spec.castsShadow = false; });

        configureButton(ButtonRole::TaskbarActive, [&](ButtonStyle &spec)
                        {
            QC::Color base = config.taskbarActiveWindow;
            spec.fillNormal = base;
            spec.fillHover = base.lighter(0.1f);
            spec.fillPressed = base.darker(0.2f);
            spec.text = config.taskbarText;
            spec.border = base.darker(0.3f);
            spec.glow = base.withAlpha(90);
            spec.glass = true;
            spec.overlayHover = QC::Color(255, 255, 255, 70);
            spec.overlayPressed = base.darker(0.35f).withAlpha(110);
            spec.outline = base.darker(0.35f);
            spec.outlineHover = base.lighter(0.06f);
            spec.outlinePressed = base.darker(0.45f);
            spec.focusOutline = config.windowBorder;
            spec.castsShadow = true; });

        configureButton(ButtonRole::Destructive, [&](ButtonStyle &spec)
                        {
            spec.fillNormal = destructiveBase;
            spec.fillHover = destructiveBase.lighter(0.08f);
            spec.fillPressed = destructiveBase.darker(0.25f);
            spec.text = textOnDark;
            spec.border = destructiveBase.darker(0.25f);
            spec.glow = destructiveBase.withAlpha(80);
            spec.glass = true;
            spec.overlayHover = QC::Color(255, 255, 255, 85);
            spec.overlayPressed = destructiveBase.darker(0.35f).withAlpha(120);
            spec.outline = destructiveBase.darker(0.35f);
            spec.outlineHover = destructiveBase.lighter(0.05f);
            spec.outlinePressed = destructiveBase.darker(0.45f);
            spec.focusOutline = destructiveBase.withAlpha(210);
            spec.castsShadow = true; });

        return snapshot;
    }

    StyleRenderer::StyleRenderer()
        : StyleRenderer(nullptr)
    {
    }

    StyleRenderer::StyleRenderer(QG::GraphicsBackend *backend)
        : m_backend(backend),
          m_snapshot(nullptr),
          m_context{},
          m_frameActive(false)
    {
    }

    void StyleRenderer::setBackend(QG::GraphicsBackend *backend)
    {
        if (m_frameActive)
        {
            endFrame();
        }
        m_backend = backend;
    }

    void StyleRenderer::setStyleSnapshot(const StyleSnapshot *snapshot)
    {
        m_snapshot = snapshot;
    }

    bool StyleRenderer::beginFrame(const FrameContext &context)
    {
        m_context = context;
        m_frameActive = m_backend && m_backend->beginFrame();
        return m_frameActive;
    }

    void StyleRenderer::endFrame()
    {
        if (!m_backend || !m_frameActive)
            return;

        m_backend->endFrame();
        m_frameActive = false;
    }

    void StyleRenderer::drawWindowChrome(const WindowPaintArgs &args)
    {
        const StyleSnapshot &styleData = style();
        drawWindowBackground(args, styleData);
        if (args.surface != WindowPaintArgs::Surface::Desktop)
        {
            drawWindowBorder(args, styleData);
        }
    }

    void StyleRenderer::drawPanel(const PanelPaintArgs &args)
    {
        if (!m_backend)
            return;

        const StyleSnapshot &styleData = style();
        QC::Color fill = args.hasBackgroundOverride
                             ? args.backgroundColor
                             : (args.sunken ? styleData.palette.panelBackground.darker(0.1f)
                                            : styleData.palette.panelBackground);
        m_backend->drawRect(args.bounds,
                            fill,
                            QC::Color::transparent(),
                            0);

        const QC::u32 width = (args.borderWidth > 0) ? args.borderWidth : styleData.metrics.borderWidth;
        const QC::Color borderColor = args.hasBorderColorOverride
                                          ? args.borderColor
                                          : styleData.palette.windowBorderInactive;
        drawPanelBorder(args,
                        m_backend,
                        m_context.painter,
                        borderColor,
                        width);
    }

    void StyleRenderer::drawButton(const ButtonPaintArgs &args)
    {
        if (!m_backend)
            return;

        const StyleSnapshot &styleData = style();
        const auto &spec = buttonStyle(styleData, args.role);
        const auto &caps = m_backend->capabilities();

        const bool disabled = args.state == ButtonPaintArgs::State::Disabled;
        const bool hovered = args.state == ButtonPaintArgs::State::Hovered;
        const bool pressed = args.state == ButtonPaintArgs::State::Pressed;

        QC::Rect buttonRect = args.bounds;
        if (hovered && styleData.metrics.buttonHoverLift != 0)
        {
            buttonRect.y -= styleData.metrics.buttonHoverLift;
        }
        if (pressed && styleData.metrics.buttonPressDepth != 0)
        {
            buttonRect.y += styleData.metrics.buttonPressDepth;
        }

        if (args.defaultButton && spec.focusOutline.a > 0 && styleData.metrics.focusRingWidth > 0)
        {
            const QC::i32 inflate = static_cast<QC::i32>(styleData.metrics.focusRingWidth);
            QC::Rect focusRect = expandRect(buttonRect, inflate);
            const bool focusRounded = spec.cornerRadius > 0 && caps.supportsRoundedRect;
            if (focusRounded)
            {
                const QC::u32 focusRadius = spec.cornerRadius + styleData.metrics.focusRingWidth;
                m_backend->drawRoundedRect(focusRect,
                                           focusRadius,
                                           QC::Color::transparent(),
                                           spec.focusOutline,
                                           styleData.metrics.focusRingWidth);
            }
            else
            {
                m_backend->drawRect(focusRect,
                                    QC::Color::transparent(),
                                    spec.focusOutline,
                                    styleData.metrics.focusRingWidth);
            }
        }

        QC::Color fill = spec.fillNormal;
        switch (args.state)
        {
        case ButtonPaintArgs::State::Hovered:
            fill = spec.fillHover;
            break;
        case ButtonPaintArgs::State::Pressed:
            fill = spec.fillPressed;
            break;
        case ButtonPaintArgs::State::Disabled:
            fill = spec.fillDisabled;
            break;
        default:
            break;
        }

        QC::Color borderColor = disabled ? spec.borderDisabled : spec.border;
        QC::Color textColor = disabled ? spec.textDisabled : spec.text;

        const QC::u32 borderWidth = (spec.borderWidth > 0) ? spec.borderWidth : styleData.metrics.borderWidth;
        const bool canRound = spec.cornerRadius > 0 && caps.supportsRoundedRect;

        const bool hasShadow = !disabled && spec.castsShadow && caps.supportsShadows && spec.glow.a > 0 && styleData.metrics.buttonShadowSoftness > 0;
        if (hasShadow)
        {
            QC::Rect shadowRect = buttonRect;
            shadowRect.x += styleData.metrics.buttonShadowOffsetX;
            shadowRect.y += styleData.metrics.buttonShadowOffsetY;
            m_backend->drawShadow(shadowRect,
                                  {styleData.metrics.buttonShadowOffsetX, styleData.metrics.buttonShadowOffsetY},
                                  static_cast<QC::i32>(styleData.metrics.buttonShadowSoftness),
                                  spec.glow,
                                  spec.glow.a);
        }

        auto drawShape = [&](const QC::Rect &rect, QC::Color shapeFill, QC::Color shapeBorder, QC::u32 shapeBorderWidth)
        {
            if (canRound)
            {
                m_backend->drawRoundedRect(rect,
                                           spec.cornerRadius,
                                           shapeFill,
                                           shapeBorder,
                                           shapeBorderWidth);
            }
            else
            {
                m_backend->drawRect(rect,
                                    shapeFill,
                                    shapeBorder,
                                    shapeBorderWidth);
            }
        };

        drawShape(buttonRect, fill, borderColor, borderWidth);

        const auto overlayColorForState = [&]() -> QC::Color
        {
            if (disabled)
                return QC::Color::transparent();
            if (pressed)
                return spec.overlayPressed;
            if (hovered)
                return spec.overlayHover;
            return QC::Color::transparent();
        };

        QC::Color overlay = overlayColorForState();
        if (overlay.a > 0)
        {
            drawShape(buttonRect, overlay, QC::Color::transparent(), 0);
        }

        const auto outlineColorForState = [&]() -> QC::Color
        {
            if (pressed && spec.outlinePressed.a > 0)
                return spec.outlinePressed;
            if (hovered && spec.outlineHover.a > 0)
                return spec.outlineHover;
            return spec.outline;
        };

        QC::Color outlineColor = (!disabled) ? outlineColorForState() : QC::Color::transparent();
        if (outlineColor.a > 0)
        {
            drawShape(buttonRect, QC::Color::transparent(), outlineColor, 1);
        }

        const bool supportsAlpha = caps.supportsAlpha && !disabled;
        if (supportsAlpha)
        {
            const QC::u32 inset = borderWidth > 0 ? borderWidth : 1;
            QC::Rect inner = insetRect(buttonRect, inset);
            if (inner.width > 0 && inner.height > 1)
            {
                QC::u32 glossHeight = inner.height / 2;
                if (glossHeight < 2)
                {
                    glossHeight = inner.height;
                }

                QC::Rect glossRect = inner;
                glossRect.height = glossHeight;

                const float glossBoost = spec.glass ? (pressed ? 0.25f : 0.4f) : (pressed ? 0.08f : 0.18f);
                const QC::u8 glossAlpha = spec.glass ? (pressed ? 140 : 190) : (pressed ? 70 : 110);

                QC::Color glossTop = fill.lighter(glossBoost).withAlpha(glossAlpha);
                QC::Color glossBottom = fill.lighter(0.02f).withAlpha(0);
                m_backend->drawGradient(glossRect,
                                        glossTop,
                                        glossBottom,
                                        QG::GradientDirection::Vertical);

                if (inner.height > glossHeight)
                {
                    QC::Rect shadeRect = inner;
                    QC::u32 shadeHeight = inner.height - glossHeight;
                    shadeRect.y = inner.y + static_cast<QC::i32>(inner.height - shadeHeight);
                    shadeRect.height = shadeHeight;

                    const float shadeBoost = spec.glass ? 0.35f : 0.18f;
                    QC::u8 shadeAlpha = spec.glass ? 150 : 110;
                    if (pressed)
                    {
                        shadeAlpha = static_cast<QC::u8>(shadeAlpha * 0.8f);
                    }

                    QC::Color shadeTop = fill.withAlpha(0);
                    QC::Color shadeBottom = fill.darker(shadeBoost).withAlpha(shadeAlpha);
                    m_backend->drawGradient(shadeRect,
                                            shadeTop,
                                            shadeBottom,
                                            QG::GradientDirection::Vertical);
                }
            }
        }

        if (m_context.painter && args.text)
        {
            QC::Size textSize = m_context.painter->measureText(args.text);
            QC::i32 textX = buttonRect.x + (static_cast<QC::i32>(buttonRect.width) - static_cast<QC::i32>(textSize.width)) / 2;
            QC::i32 textY = buttonRect.y + (static_cast<QC::i32>(buttonRect.height) - static_cast<QC::i32>(textSize.height)) / 2;
            if (hovered && styleData.metrics.buttonTextHoverOffset != 0)
            {
                textY -= styleData.metrics.buttonTextHoverOffset;
            }
            if (pressed && styleData.metrics.buttonTextPressedOffset != 0)
            {
                textY += styleData.metrics.buttonTextPressedOffset;
            }
            m_context.painter->drawText(textX, textY, args.text, textColor);
        }
    }

    const StyleSnapshot &StyleRenderer::style() const
    {
        return m_snapshot ? *m_snapshot : StyleSnapshot::fallback();
    }

    void StyleRenderer::drawWindowBackground(const WindowPaintArgs &args,
                                             const StyleSnapshot &styleData)
    {
        if (!m_backend)
            return;

        QC::Color top = styleData.palette.windowBackground;
        QC::Color bottom = top;

        if (args.surface == WindowPaintArgs::Surface::Desktop)
        {
            top = styleData.palette.desktopBackgroundTop;
            bottom = styleData.palette.desktopBackgroundBottom;
        }

        if (top != bottom)
        {
            m_backend->drawGradient(args.bounds,
                                    top,
                                    bottom,
                                    QG::GradientDirection::Vertical);
        }
        else
        {
            m_backend->drawRect(args.bounds,
                                top,
                                QC::Color::transparent(),
                                0);
        }
    }

    void StyleRenderer::drawWindowBorder(const WindowPaintArgs &args,
                                         const StyleSnapshot &styleData)
    {
        if (!m_backend)
            return;

        QC::Color borderColor = args.active ? styleData.palette.windowBorderActive
                                            : styleData.palette.windowBorderInactive;
        m_backend->drawRect(args.bounds,
                            QC::Color::transparent(),
                            borderColor,
                            styleData.metrics.borderWidth);
    }

} // namespace QW
