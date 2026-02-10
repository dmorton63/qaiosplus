// QWControls Frame - Border and frame rendering implementation
// Namespace: QW::Controls

#include "QWCtrlFrame.h"

namespace QW
{
    namespace Controls
    {

        // ==================== FrameColors Presets ====================

        FrameColors FrameColors::defaultColors()
        {
            FrameColors c;
            c.background = Color(240, 240, 240, 255);
            c.backgroundEnd = Color(220, 220, 220, 255);
            c.borderLight = Color(255, 255, 255, 255);
            c.borderDark = Color(100, 100, 100, 255);
            c.borderMid = Color(160, 160, 160, 255);
            c.shadow = Color(0, 0, 0, 80);
            c.glow = Color(0, 120, 215, 128);
            return c;
        }

        FrameColors FrameColors::vistaColors()
        {
            FrameColors c;
            c.background = Color(245, 246, 247, 255);
            c.backgroundEnd = Color(225, 226, 227, 255);
            c.borderLight = Color(255, 255, 255, 255);
            c.borderDark = Color(112, 112, 112, 255);
            c.borderMid = Color(174, 179, 185, 255);
            c.shadow = Color(0, 0, 0, 60);
            c.glow = Color(60, 127, 177, 180);
            return c;
        }

        FrameColors FrameColors::darkColors()
        {
            FrameColors c;
            c.background = Color(45, 45, 48, 255);
            c.backgroundEnd = Color(30, 30, 30, 255);
            c.borderLight = Color(70, 70, 70, 255);
            c.borderDark = Color(20, 20, 20, 255);
            c.borderMid = Color(63, 63, 70, 255);
            c.shadow = Color(0, 0, 0, 120);
            c.glow = Color(0, 122, 204, 180);
            return c;
        }

        FrameColors FrameColors::lightColors()
        {
            FrameColors c;
            c.background = Color(255, 255, 255, 255);
            c.backgroundEnd = Color(245, 245, 245, 255);
            c.borderLight = Color(255, 255, 255, 255);
            c.borderDark = Color(180, 180, 180, 255);
            c.borderMid = Color(200, 200, 200, 255);
            c.shadow = Color(0, 0, 0, 40);
            c.glow = Color(0, 120, 215, 100);
            return c;
        }

        // ==================== Frame Constructor ====================

        Frame::Frame()
            : m_style(FrameStyle::BorderFlat | FrameStyle::FillSolid),
              m_bounds{0, 0, 0, 0},
              m_colors(),
              m_metrics()
        {
        }

        Frame::Frame(QC::u32 style)
            : m_style(style),
              m_bounds{0, 0, 0, 0},
              m_colors(),
              m_metrics()
        {
        }

        Frame::Frame(QC::u32 style, const FrameColors &colors)
            : m_style(style),
              m_bounds{0, 0, 0, 0},
              m_colors(colors),
              m_metrics()
        {
        }

        // ==================== Style Methods ====================

        void Frame::setBorderStyle(QC::u32 borderFlag)
        {
            // Clear existing border flags and set new one
            m_style = (m_style & ~FrameStyle::BorderMask) | (borderFlag & FrameStyle::BorderMask);
        }

        // ==================== Geometry ====================

        Rect Frame::contentRect() const
        {
            Rect content;
            QC::u32 totalBorder = m_metrics.borderWidth;

            // Account for double borders
            if (hasStyle(FrameStyle::BorderDouble) || hasStyle(FrameStyle::BorderEtched) ||
                hasStyle(FrameStyle::BorderGroove))
            {
                totalBorder *= 2;
            }

            content.x = m_bounds.x + static_cast<QC::i32>(totalBorder + m_metrics.paddingLeft);
            content.y = m_bounds.y + static_cast<QC::i32>(totalBorder + m_metrics.paddingTop);
            content.width = m_bounds.width - totalBorder * 2 - m_metrics.paddingLeft - m_metrics.paddingRight;
            content.height = m_bounds.height - totalBorder * 2 - m_metrics.paddingTop - m_metrics.paddingBottom;

            return content;
        }

        // ==================== Main Paint Method ====================

        void Frame::paint(QG::IPainter *painter) const
        {
            if (!painter)
                return;

            // Paint order: shadow -> background -> border
            // This ensures proper layering

            // 1. Drop shadow (painted outside/behind the frame)
            if (hasStyle(FrameStyle::DropShadow) || hasStyle(FrameStyle::DropShadowSoft))
            {
                paintShadow(painter);
            }

            // 2. Background fill
            paintBackground(painter);

            // 3. Inner shadow (painted after background, before border)
            if (hasStyle(FrameStyle::InnerShadow))
            {
                paintInnerShadow(painter);
            }

            // 4. Border
            paintBorder(painter);
        }

        // ==================== Shadow Painting ====================

        void Frame::paintShadow(QG::IPainter *painter) const
        {
            paintDropShadow(painter);
        }

        void Frame::paintDropShadow(QG::IPainter *painter) const
        {
            QC::i32 offset = static_cast<QC::i32>(m_metrics.shadowOffset);
            QC::u32 size = m_metrics.shadowSize;

            // Simple solid shadow (soft shadow would require blur)
            Rect shadowRect = {
                m_bounds.x + offset,
                m_bounds.y + offset,
                m_bounds.width,
                m_bounds.height};

            // Draw shadow layers for soft effect
            if (hasStyle(FrameStyle::DropShadowSoft) && size > 1)
            {
                // Draw multiple expanding rectangles with decreasing alpha
                for (QC::u32 i = 0; i < size; ++i)
                {
                    QC::u8 alpha = static_cast<QC::u8>((m_colors.shadow.a * (size - i)) / size);
                    Color layerColor(m_colors.shadow.r, m_colors.shadow.g, m_colors.shadow.b, alpha);

                    Rect layerRect = {
                        shadowRect.x + static_cast<QC::i32>(i),
                        shadowRect.y + static_cast<QC::i32>(i),
                        shadowRect.width + (size - i) * 2,
                        shadowRect.height + (size - i) * 2};

                    painter->fillRect(layerRect, layerColor);
                }
            }
            else
            {
                // Simple solid shadow
                painter->fillRect(shadowRect, m_colors.shadow);
            }
        }

        void Frame::paintInnerShadow(QG::IPainter *painter) const
        {
            // Inner shadow creates a subtle inset effect
            QC::u32 size = m_metrics.shadowSize > 3 ? 3 : m_metrics.shadowSize;
            QC::u8 baseAlpha = m_colors.shadow.a / 2;

            for (QC::u32 i = 0; i < size; ++i)
            {
                QC::u8 alpha = static_cast<QC::u8>((baseAlpha * (size - i)) / size);
                Color layerColor(m_colors.shadow.r, m_colors.shadow.g, m_colors.shadow.b, alpha);

                QC::i32 inset = static_cast<QC::i32>(m_metrics.borderWidth + i);

                // Top edge
                Rect top = {m_bounds.x + inset, m_bounds.y + inset,
                            m_bounds.width - static_cast<QC::u32>(inset * 2), 1};
                painter->fillRect(top, layerColor);

                // Left edge
                Rect left = {m_bounds.x + inset, m_bounds.y + inset,
                             1, m_bounds.height - static_cast<QC::u32>(inset * 2)};
                painter->fillRect(left, layerColor);
            }
        }

        // ==================== Background Painting ====================

        void Frame::paintBackground(QG::IPainter *painter) const
        {
            QC::u32 fillType = fillStyle();

            if (fillType & FrameStyle::FillTransparent)
            {
                return; // No fill
            }

            if (fillType & FrameStyle::FillGradientV)
            {
                paintFillGradientV(painter);
            }
            else if (fillType & FrameStyle::FillGradientH)
            {
                paintFillGradientH(painter);
            }
            else
            {
                // Default: solid fill
                paintFillSolid(painter);
            }
        }

        void Frame::paintFillSolid(QG::IPainter *painter) const
        {
            painter->fillRect(m_bounds, m_colors.background);
        }

        void Frame::paintFillGradientV(QG::IPainter *painter) const
        {
            // Vertical gradient: top color to bottom color
            if (m_bounds.height == 0)
                return;

            for (QC::u32 y = 0; y < m_bounds.height; ++y)
            {
                QC::u32 t = (y * 255) / m_bounds.height;
                QC::u32 invT = 255 - t;

                Color lineColor;
                lineColor.r = static_cast<QC::u8>((m_colors.background.r * invT + m_colors.backgroundEnd.r * t) / 255);
                lineColor.g = static_cast<QC::u8>((m_colors.background.g * invT + m_colors.backgroundEnd.g * t) / 255);
                lineColor.b = static_cast<QC::u8>((m_colors.background.b * invT + m_colors.backgroundEnd.b * t) / 255);
                lineColor.a = static_cast<QC::u8>((m_colors.background.a * invT + m_colors.backgroundEnd.a * t) / 255);

                Rect line = {m_bounds.x, m_bounds.y + static_cast<QC::i32>(y), m_bounds.width, 1};
                painter->fillRect(line, lineColor);
            }
        }

        void Frame::paintFillGradientH(QG::IPainter *painter) const
        {
            // Horizontal gradient: left color to right color
            if (m_bounds.width == 0)
                return;

            for (QC::u32 x = 0; x < m_bounds.width; ++x)
            {
                QC::u32 t = (x * 255) / m_bounds.width;
                QC::u32 invT = 255 - t;

                Color lineColor;
                lineColor.r = static_cast<QC::u8>((m_colors.background.r * invT + m_colors.backgroundEnd.r * t) / 255);
                lineColor.g = static_cast<QC::u8>((m_colors.background.g * invT + m_colors.backgroundEnd.g * t) / 255);
                lineColor.b = static_cast<QC::u8>((m_colors.background.b * invT + m_colors.backgroundEnd.b * t) / 255);
                lineColor.a = static_cast<QC::u8>((m_colors.background.a * invT + m_colors.backgroundEnd.a * t) / 255);

                Rect line = {m_bounds.x + static_cast<QC::i32>(x), m_bounds.y, 1, m_bounds.height};
                painter->fillRect(line, lineColor);
            }
        }

        // ==================== Border Painting ====================

        void Frame::paintBorder(QG::IPainter *painter) const
        {
            QC::u32 borderType = borderStyle();

            if (borderType == FrameStyle::None)
            {
                return;
            }

            if (borderType & FrameStyle::BorderRaised)
            {
                paintBorderRaised(painter);
            }
            else if (borderType & FrameStyle::BorderSunken)
            {
                paintBorderSunken(painter);
            }
            else if (borderType & FrameStyle::BorderEtched)
            {
                paintBorderEtched(painter);
            }
            else if (borderType & FrameStyle::BorderGroove)
            {
                paintBorderGroove(painter);
            }
            else if (borderType & FrameStyle::BorderDouble)
            {
                paintBorderDouble(painter);
            }
            else
            {
                // Default: flat border
                paintBorderFlat(painter);
            }
        }

        void Frame::paintBorderFlat(QG::IPainter *painter) const
        {
            // Simple single-line border
            QC::u32 w = m_metrics.borderWidth;

            for (QC::u32 i = 0; i < w; ++i)
            {
                Rect borderRect = {
                    m_bounds.x + static_cast<QC::i32>(i),
                    m_bounds.y + static_cast<QC::i32>(i),
                    m_bounds.width - i * 2,
                    m_bounds.height - i * 2};
                painter->drawRect(borderRect, m_colors.borderMid);
            }
        }

        void Frame::paintBorderRaised(QG::IPainter *painter) const
        {
            // 3D raised: light on top-left, dark on bottom-right
            QC::u32 w = m_metrics.borderWidth;

            for (QC::u32 i = 0; i < w; ++i)
            {
                QC::i32 inset = static_cast<QC::i32>(i);

                // Top edge (light)
                Rect top = {m_bounds.x + inset, m_bounds.y + inset,
                            m_bounds.width - static_cast<QC::u32>(inset * 2), 1};
                painter->fillRect(top, m_colors.borderLight);

                // Left edge (light)
                Rect left = {m_bounds.x + inset, m_bounds.y + inset,
                             1, m_bounds.height - static_cast<QC::u32>(inset * 2)};
                painter->fillRect(left, m_colors.borderLight);

                // Bottom edge (dark)
                Rect bottom = {m_bounds.x + inset,
                               m_bounds.y + static_cast<QC::i32>(m_bounds.height) - 1 - inset,
                               m_bounds.width - static_cast<QC::u32>(inset * 2), 1};
                painter->fillRect(bottom, m_colors.borderDark);

                // Right edge (dark)
                Rect right = {m_bounds.x + static_cast<QC::i32>(m_bounds.width) - 1 - inset,
                              m_bounds.y + inset,
                              1, m_bounds.height - static_cast<QC::u32>(inset * 2)};
                painter->fillRect(right, m_colors.borderDark);
            }
        }

        void Frame::paintBorderSunken(QG::IPainter *painter) const
        {
            // 3D sunken: dark on top-left, light on bottom-right
            QC::u32 w = m_metrics.borderWidth;

            for (QC::u32 i = 0; i < w; ++i)
            {
                QC::i32 inset = static_cast<QC::i32>(i);

                // Top edge (dark)
                Rect top = {m_bounds.x + inset, m_bounds.y + inset,
                            m_bounds.width - static_cast<QC::u32>(inset * 2), 1};
                painter->fillRect(top, m_colors.borderDark);

                // Left edge (dark)
                Rect left = {m_bounds.x + inset, m_bounds.y + inset,
                             1, m_bounds.height - static_cast<QC::u32>(inset * 2)};
                painter->fillRect(left, m_colors.borderDark);

                // Bottom edge (light)
                Rect bottom = {m_bounds.x + inset,
                               m_bounds.y + static_cast<QC::i32>(m_bounds.height) - 1 - inset,
                               m_bounds.width - static_cast<QC::u32>(inset * 2), 1};
                painter->fillRect(bottom, m_colors.borderLight);

                // Right edge (light)
                Rect right = {m_bounds.x + static_cast<QC::i32>(m_bounds.width) - 1 - inset,
                              m_bounds.y + inset,
                              1, m_bounds.height - static_cast<QC::u32>(inset * 2)};
                painter->fillRect(right, m_colors.borderLight);
            }
        }

        void Frame::paintBorderEtched(QG::IPainter *painter) const
        {
            // Etched: outer dark line, inner light line
            // Outer rectangle (dark)
            painter->drawRect(m_bounds, m_colors.borderDark);

            // Inner rectangle (light, offset by 1)
            if (m_bounds.width > 2 && m_bounds.height > 2)
            {
                Rect inner = {m_bounds.x + 1, m_bounds.y + 1,
                              m_bounds.width - 2, m_bounds.height - 2};
                painter->drawRect(inner, m_colors.borderLight);
            }
        }

        void Frame::paintBorderGroove(QG::IPainter *painter) const
        {
            // Groove: outer light line, inner dark line (inverse of etched)
            // Outer rectangle (light)
            painter->drawRect(m_bounds, m_colors.borderLight);

            // Inner rectangle (dark, offset by 1)
            if (m_bounds.width > 2 && m_bounds.height > 2)
            {
                Rect inner = {m_bounds.x + 1, m_bounds.y + 1,
                              m_bounds.width - 2, m_bounds.height - 2};
                painter->drawRect(inner, m_colors.borderDark);
            }
        }

        void Frame::paintBorderDouble(QG::IPainter *painter) const
        {
            // Double: two parallel lines
            painter->drawRect(m_bounds, m_colors.borderMid);

            if (m_bounds.width > 4 && m_bounds.height > 4)
            {
                Rect inner = {m_bounds.x + 2, m_bounds.y + 2,
                              m_bounds.width - 4, m_bounds.height - 4};
                painter->drawRect(inner, m_colors.borderMid);
            }
        }

    } // namespace Controls
} // namespace QW
