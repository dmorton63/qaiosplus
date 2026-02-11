// Panel implementation
// Namespace: QW::Controls

#include "QWControls/Containers/Panel.h"
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
            QC::u32 style = FrameStyle::FillSolid;
            switch (m_borderStyle)
            {
            case BorderStyle::None:
                style = FrameStyle::FillSolid;
                break;
            case BorderStyle::Flat:
                style = FrameStyle::BorderFlat | FrameStyle::FillSolid;
                break;
            case BorderStyle::Raised:
                style = FrameStyle::BorderRaised | FrameStyle::FillSolid;
                break;
            case BorderStyle::Sunken:
                style = FrameStyle::BorderSunken | FrameStyle::FillSolid;
                break;
            case BorderStyle::Etched:
                style = FrameStyle::BorderEtched | FrameStyle::FillSolid;
                break;
            }
            m_frame.setStyle(style);
        }

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

        void Panel::paint()
        {
            if (!m_visible || !m_window)
            {
                return;
            }

            if (m_frameVisible)
            {
                Rect abs = absoluteBounds();
                m_frame.setBounds(abs);

                FrameColors colors = m_frame.colors();
                colors.background = m_bgColor;
                m_frame.setColors(colors);

                m_frame.paint(m_window->painter());
            }

            paintChildren();
        }
    }
} // namespace QW
