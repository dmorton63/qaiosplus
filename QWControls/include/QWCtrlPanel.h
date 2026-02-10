#pragma once

// QWControls Panel - Decorated container control
// Namespace: QW::Controls
//
// Panel = Container + Frame
// - Extends Container for child management and event routing
// - Uses Frame decorator for visual styling (borders, background, padding)

#include "QCTypes.h"
#include "QWCtrlContainer.h"
#include "QWCtrlFrame.h"

namespace QW
{
    namespace Controls
    {

        /// Legacy BorderStyle enum - maps to Frame styles
        enum class BorderStyle : QC::u8
        {
            None,
            Flat,
            Raised,
            Sunken,
            Etched
        };

        /// Panel - A decorated container control
        /// Combines Container (children) with Frame (visual decoration)
        class Panel : public Container
        {
        public:
            Panel();
            Panel(Window *window, Rect bounds);
            virtual ~Panel() = default;

            // ==================== Type Information ====================
            Panel *asPanel() override { return this; }
            const Panel *asPanel() const override { return this; }

            // ==================== Frame Access ====================

            /// Direct access to the Frame decorator
            Frame &frame() { return m_frame; }
            const Frame &frame() const { return m_frame; }

            /// Set frame style directly
            void setFrameStyle(QC::u32 style) { m_frame.setStyle(style); }
            QC::u32 frameStyle() const { return m_frame.style(); }

            /// Enable/disable frame rendering
            bool isFrameVisible() const { return m_frameVisible; }
            void setFrameVisible(bool visible) { m_frameVisible = visible; }

            // ==================== Legacy BorderStyle API ====================
            // Maps to Frame styles for backward compatibility

            BorderStyle borderStyle() const { return m_borderStyle; }
            void setBorderStyle(BorderStyle style);

            Color borderColor() const { return m_frame.colors().borderMid; }
            void setBorderColor(Color color);

            QC::u32 borderWidth() const { return m_frame.metrics().borderWidth; }
            void setBorderWidth(QC::u32 width);

            // ==================== Padding ====================

            /// Padding inside the frame border
            void setPadding(QC::u32 left, QC::u32 top, QC::u32 right, QC::u32 bottom);
            void setPadding(QC::u32 all) { setPadding(all, all, all, all); }

            QC::u32 paddingLeft() const { return m_paddingLeft; }
            QC::u32 paddingTop() const { return m_paddingTop; }
            QC::u32 paddingRight() const { return m_paddingRight; }
            QC::u32 paddingBottom() const { return m_paddingBottom; }

            /// Get client area (inside frame and padding)
            Rect clientRect() const;

            // ==================== Rendering ====================

            /// Paint the panel: frame, then children
            void paint() override;

        protected:
            /// Sync Frame style from legacy BorderStyle enum
            void syncFrameFromBorderStyle();

            Frame m_frame;
            bool m_frameVisible;
            BorderStyle m_borderStyle;

            QC::u32 m_paddingLeft;
            QC::u32 m_paddingTop;
            QC::u32 m_paddingRight;
            QC::u32 m_paddingBottom;
        };

    } // namespace Controls
} // namespace QW
