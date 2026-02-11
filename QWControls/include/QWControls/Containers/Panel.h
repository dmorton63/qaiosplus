#pragma once

// Panel - decorated container control
// Namespace: QW::Controls

#include "QWControls/Containers/Container.h"
#include "QWControls/Containers/Frame.h"

namespace QW
{
    namespace Controls
    {
        enum class BorderStyle : QC::u8
        {
            None,
            Flat,
            Raised,
            Sunken,
            Etched
        };

        class Panel : public Container
        {
        public:
            Panel();
            Panel(Window *window, Rect bounds);
            virtual ~Panel() = default;

            Panel *asPanel() override { return this; }
            const Panel *asPanel() const override { return this; }

            Frame &frame() { return m_frame; }
            const Frame &frame() const { return m_frame; }

            void setFrameStyle(QC::u32 style) { m_frame.setStyle(style); }
            QC::u32 frameStyle() const { return m_frame.style(); }

            bool isFrameVisible() const { return m_frameVisible; }
            void setFrameVisible(bool visible) { m_frameVisible = visible; }

            BorderStyle borderStyle() const { return m_borderStyle; }
            void setBorderStyle(BorderStyle style);

            Color borderColor() const { return m_frame.colors().borderMid; }
            void setBorderColor(Color color);

            QC::u32 borderWidth() const { return m_frame.metrics().borderWidth; }
            void setBorderWidth(QC::u32 width);

            void setPadding(QC::u32 left, QC::u32 top, QC::u32 right, QC::u32 bottom);
            void setPadding(QC::u32 all) { setPadding(all, all, all, all); }

            QC::u32 paddingLeft() const { return m_paddingLeft; }
            QC::u32 paddingTop() const { return m_paddingTop; }
            QC::u32 paddingRight() const { return m_paddingRight; }
            QC::u32 paddingBottom() const { return m_paddingBottom; }

            Rect clientRect() const;

            void paint() override;

        protected:
            void syncFrameFromBorderStyle();

            Frame m_frame;
            bool m_frameVisible;
            BorderStyle m_borderStyle;
            QC::u32 m_paddingLeft;
            QC::u32 m_paddingTop;
            QC::u32 m_paddingRight;
            QC::u32 m_paddingBottom;
        };
    }
} // namespace QW
