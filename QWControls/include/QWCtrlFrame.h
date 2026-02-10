#pragma once

// QWControls Frame - Border and frame rendering component
// Namespace: QW::Controls
//
// Provides Windows Vista-style visual effects:
// - 3D borders (raised, sunken, etched)
// - Drop shadows
// - Gradient fills
// - Rounded corners (future)
//
// Used as a composition component by all controls for consistent frame rendering.

#include "QCTypes.h"
#include "QWWindowManager.h"
#include "QGPainter.h"

namespace QW
{
    namespace Controls
    {

        // ==================== Frame Style Flags ====================
        // Can be combined with bitwise OR

        namespace FrameStyle
        {
            constexpr QC::u32 None = 0x0000;

            // Border styles (mutually exclusive for border type)
            constexpr QC::u32 BorderFlat = 0x0001;   // Single line border
            constexpr QC::u32 BorderRaised = 0x0002; // 3D raised effect (light top-left, dark bottom-right)
            constexpr QC::u32 BorderSunken = 0x0004; // 3D sunken effect (dark top-left, light bottom-right)
            constexpr QC::u32 BorderEtched = 0x0008; // Double-line etched effect
            constexpr QC::u32 BorderDouble = 0x0010; // Double line border
            constexpr QC::u32 BorderGroove = 0x0020; // Grooved border (inverse of etched)
            constexpr QC::u32 BorderMask = 0x003F;   // Mask for border styles

            // Effects (can be combined)
            constexpr QC::u32 DropShadow = 0x0100;     // Drop shadow effect
            constexpr QC::u32 DropShadowSoft = 0x0200; // Soft/blurred shadow
            constexpr QC::u32 InnerShadow = 0x0400;    // Inner shadow (inset)
            constexpr QC::u32 GlowEffect = 0x0800;     // Outer glow

            // Fill options
            constexpr QC::u32 FillSolid = 0x1000;       // Solid color fill
            constexpr QC::u32 FillGradientV = 0x2000;   // Vertical gradient
            constexpr QC::u32 FillGradientH = 0x4000;   // Horizontal gradient
            constexpr QC::u32 FillTransparent = 0x8000; // No fill (transparent)
            constexpr QC::u32 FillMask = 0xF000;        // Mask for fill styles

            // Common presets
            constexpr QC::u32 Button3D = BorderRaised | FillSolid;
            constexpr QC::u32 ButtonPressed = BorderSunken | FillSolid;
            constexpr QC::u32 TextBox = BorderSunken | FillSolid;
            constexpr QC::u32 Panel3D = BorderEtched | FillSolid;
            constexpr QC::u32 WindowFrame = BorderRaised | DropShadow | FillSolid;
            constexpr QC::u32 FlatButton = BorderFlat | FillSolid;
            constexpr QC::u32 MenuPopup = BorderFlat | DropShadow | FillSolid;
        }

        // ==================== Frame Configuration ====================

        /// Frame appearance configuration
        struct FrameColors
        {
            Color background;    // Background fill color
            Color backgroundEnd; // End color for gradients
            Color borderLight;   // Light edge color (for 3D effects)
            Color borderDark;    // Dark edge color (for 3D effects)
            Color borderMid;     // Middle/flat border color
            Color shadow;        // Drop shadow color
            Color glow;          // Glow effect color

            FrameColors()
                : background(Color(240, 240, 240, 255)),
                  backgroundEnd(Color(220, 220, 220, 255)),
                  borderLight(Color(255, 255, 255, 255)),
                  borderDark(Color(100, 100, 100, 255)),
                  borderMid(Color(160, 160, 160, 255)),
                  shadow(Color(0, 0, 0, 80)),
                  glow(Color(0, 120, 215, 128))
            {
            }

            // Preset color schemes
            static FrameColors defaultColors();
            static FrameColors vistaColors();
            static FrameColors darkColors();
            static FrameColors lightColors();
        };

        /// Frame metrics/sizing
        struct FrameMetrics
        {
            QC::u32 borderWidth;  // Border thickness in pixels
            QC::u32 shadowOffset; // Shadow offset from frame
            QC::u32 shadowSize;   // Shadow blur/spread size
            QC::u32 cornerRadius; // Corner radius (0 = square, future use)
            QC::u32 paddingLeft;
            QC::u32 paddingTop;
            QC::u32 paddingRight;
            QC::u32 paddingBottom;

            FrameMetrics()
                : borderWidth(1),
                  shadowOffset(2),
                  shadowSize(4),
                  cornerRadius(0),
                  paddingLeft(0),
                  paddingTop(0),
                  paddingRight(0),
                  paddingBottom(0)
            {
            }

            void setPadding(QC::u32 all)
            {
                paddingLeft = paddingTop = paddingRight = paddingBottom = all;
            }

            void setPadding(QC::u32 horizontal, QC::u32 vertical)
            {
                paddingLeft = paddingRight = horizontal;
                paddingTop = paddingBottom = vertical;
            }

            void setPadding(QC::u32 left, QC::u32 top, QC::u32 right, QC::u32 bottom)
            {
                paddingLeft = left;
                paddingTop = top;
                paddingRight = right;
                paddingBottom = bottom;
            }
        };

        // ==================== Frame Class ====================

        /// Frame - Renders borders, shadows, and backgrounds
        /// This is a composition component - controls own a Frame instance
        class Frame
        {
        public:
            Frame();
            Frame(QC::u32 style);
            Frame(QC::u32 style, const FrameColors &colors);
            ~Frame() = default;

            // ==================== Style ====================

            QC::u32 style() const { return m_style; }
            void setStyle(QC::u32 style) { m_style = style; }

            /// Add style flags
            void addStyle(QC::u32 flags) { m_style |= flags; }

            /// Remove style flags
            void removeStyle(QC::u32 flags) { m_style &= ~flags; }

            /// Check if style flag is set
            bool hasStyle(QC::u32 flag) const { return (m_style & flag) != 0; }

            /// Get border style portion
            QC::u32 borderStyle() const { return m_style & FrameStyle::BorderMask; }

            /// Set border style (clears other border flags first)
            void setBorderStyle(QC::u32 borderFlag);

            /// Get fill style portion
            QC::u32 fillStyle() const { return m_style & FrameStyle::FillMask; }

            // ==================== Bounds ====================

            Rect bounds() const { return m_bounds; }
            void setBounds(const Rect &bounds) { m_bounds = bounds; }

            void setPosition(QC::i32 x, QC::i32 y)
            {
                m_bounds.x = x;
                m_bounds.y = y;
            }

            void setSize(QC::u32 width, QC::u32 height)
            {
                m_bounds.width = width;
                m_bounds.height = height;
            }

            /// Get content area (inside border and padding)
            Rect contentRect() const;

            // ==================== Colors ====================

            const FrameColors &colors() const { return m_colors; }
            FrameColors &colors() { return m_colors; }
            void setColors(const FrameColors &colors) { m_colors = colors; }

            Color backgroundColor() const { return m_colors.background; }
            void setBackgroundColor(Color color) { m_colors.background = color; }

            Color borderColor() const { return m_colors.borderMid; }
            void setBorderColor(Color color) { m_colors.borderMid = color; }

            Color shadowColor() const { return m_colors.shadow; }
            void setShadowColor(Color color) { m_colors.shadow = color; }

            // ==================== Metrics ====================

            const FrameMetrics &metrics() const { return m_metrics; }
            FrameMetrics &metrics() { return m_metrics; }
            void setMetrics(const FrameMetrics &metrics) { m_metrics = metrics; }

            QC::u32 borderWidth() const { return m_metrics.borderWidth; }
            void setBorderWidth(QC::u32 width) { m_metrics.borderWidth = width; }

            QC::u32 shadowOffset() const { return m_metrics.shadowOffset; }
            void setShadowOffset(QC::u32 offset) { m_metrics.shadowOffset = offset; }

            // ==================== Rendering ====================

            /// Paint the complete frame (shadow, background, border)
            void paint(QG::IPainter *painter) const;

            /// Paint only the drop shadow
            void paintShadow(QG::IPainter *painter) const;

            /// Paint only the background fill
            void paintBackground(QG::IPainter *painter) const;

            /// Paint only the border
            void paintBorder(QG::IPainter *painter) const;

        private:
            void paintBorderFlat(QG::IPainter *painter) const;
            void paintBorderRaised(QG::IPainter *painter) const;
            void paintBorderSunken(QG::IPainter *painter) const;
            void paintBorderEtched(QG::IPainter *painter) const;
            void paintBorderDouble(QG::IPainter *painter) const;
            void paintBorderGroove(QG::IPainter *painter) const;

            void paintFillSolid(QG::IPainter *painter) const;
            void paintFillGradientV(QG::IPainter *painter) const;
            void paintFillGradientH(QG::IPainter *painter) const;

            void paintDropShadow(QG::IPainter *painter) const;
            void paintInnerShadow(QG::IPainter *painter) const;

            QC::u32 m_style;
            Rect m_bounds;
            FrameColors m_colors;
            FrameMetrics m_metrics;
        };

    } // namespace Controls
} // namespace QW
