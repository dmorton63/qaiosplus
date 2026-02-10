// QWControls Panel - Decorated container implementation
// Namespace: QW::Controls
//
// Panel = Container + Frame
// Extends Container for children, uses Frame for visual decoration

#include "QWCtrlPanel.h"
#include "QWWindow.h"

namespace QW
{
    namespace Controls
    {

        Panel::Panel()
            : Container(),
              m_frame(),
              m_frameVisible(true),
              m_borderStyle(BorderStyle::Flat),
              m_paddingLeft(0),
              m_paddingTop(0),
              m_paddingRight(0),
              m_paddingBottom(0)
        {
            m_bgColor = Color(240, 240, 240, 255);
            syncFrameFromBorderStyle();
        }

        Panel::Panel(Window *window, Rect bounds)
            : Container(window, bounds),
              m_frame(),
              m_frameVisible(true),
              m_borderStyle(BorderStyle::Flat),
              m_paddingLeft(0),
              m_paddingTop(0),
              m_paddingRight(0),
              m_paddingBottom(0)
        {
            m_bgColor = Color(240, 240, 240, 255);
            syncFrameFromBorderStyle();
        }

        // ==================== BorderStyle API ====================

        void Panel::setBorderStyle(BorderStyle style)
        {
            m_borderStyle = style;
            syncFrameFromBorderStyle();
        }

        void Panel::setBorderColor(Color color)
        {
            FrameColors colors = m_frame.colors();
            colors.borderMid = color;
            m_frame.setColors(colors);
        }

        void Panel::setBorderWidth(QC::u32 width)
        {
            FrameMetrics metrics = m_frame.metrics();
            metrics.borderWidth = width;
            m_frame.setMetrics(metrics);
        }

        void Panel::syncFrameFromBorderStyle()
        {
            QC::u32 frameStyle = FrameStyle::FillSolid;
            switch (m_borderStyle)
            {
            case BorderStyle::None:
                frameStyle = FrameStyle::FillSolid;
                break;
            case BorderStyle::Flat:
                frameStyle = FrameStyle::BorderFlat | FrameStyle::FillSolid;
                break;
            case BorderStyle::Raised:
                frameStyle = FrameStyle::BorderRaised | FrameStyle::FillSolid;
                break;
            case BorderStyle::Sunken:
                frameStyle = FrameStyle::BorderSunken | FrameStyle::FillSolid;
                break;
            case BorderStyle::Etched:
                frameStyle = FrameStyle::BorderEtched | FrameStyle::FillSolid;
                break;
            }
            m_frame.setStyle(frameStyle);
        }

        // ==================== Padding ====================

        void Panel::setPadding(QC::u32 left, QC::u32 top, QC::u32 right, QC::u32 bottom)
        {
            m_paddingLeft = left;
            m_paddingTop = top;
            m_paddingRight = right;
            m_paddingBottom = bottom;
        }

        Rect Panel::clientRect() const
        {
            QC::u32 bw = m_frame.metrics().borderWidth;
            Rect client;
            client.x = static_cast<QC::i32>(bw + m_paddingLeft);
            client.y = static_cast<QC::i32>(bw + m_paddingTop);
            client.width = m_bounds.width - bw * 2 - m_paddingLeft - m_paddingRight;
            client.height = m_bounds.height - bw * 2 - m_paddingTop - m_paddingBottom;
            return client;
        }

        // ==================== Rendering ====================

        void Panel::paint()
        {
            if (!m_visible || !m_window)
                return;

            // Paint frame (background + border) if visible
            if (m_frameVisible)
            {
                Rect abs = absoluteBounds();
                m_frame.setBounds(abs);

                // Sync background color
                FrameColors colors = m_frame.colors();
                colors.background = m_bgColor;
                m_frame.setColors(colors);

                m_frame.paint(m_window);
            }

            // Paint children (delegated to Container)
            paintChildren();
        }

    } // namespace Controls
} // namespace QW
