#pragma once

// QWControls Button - Button control
// Namespace: QW::Controls

#include "QCTypes.h"
#include "QWControls/Base/ControlBase.h"
#include "QKEventTypes.h"

namespace QW
{
    class Window;

    namespace Controls
    {

        // Button click callback
        class Button;
        using ButtonClickHandler = void (*)(Button *button, void *userData);

        enum class ButtonStyle : QC::u8
        {
            Flat = 0,
            Vista
        };

        class Button : public ControlBase
        {
        public:
            Button();
            Button(Window *window, const char *text, Rect bounds);
            virtual ~Button();

            // Properties
            const char *text() const { return m_text; }
            void setText(const char *text);

            // Appearance
            Color textColor() const { return m_textColor; }
            void setTextColor(Color color) { m_textColor = color; }

            Color borderColor() const { return m_borderColor; }
            void setBorderColor(Color color) { m_borderColor = color; }

            Color hoverColor() const { return m_hoverColor; }
            void setHoverColor(Color color) { m_hoverColor = color; }

            Color pressedColor() const { return m_pressedColor; }
            void setPressedColor(Color color) { m_pressedColor = color; }

            ButtonStyle visualStyle() const { return m_visualStyle; }
            void setVisualStyle(ButtonStyle style);

            void setVistaGradient(Color top, Color bottom);
            void setVistaHighlight(Color top, Color bottom);
            void setVistaBorders(Color outer, Color inner);
            void setVistaGlow(Color glow);

            // Events
            void setClickHandler(ButtonClickHandler handler, void *userData);

            // Rendering (override from ControlBase)
            void paint() override;

            // Event handlers (override from ControlBase)
            bool onMouseMove(QC::i32 x, QC::i32 y, QC::i32 deltaX, QC::i32 deltaY) override;
            bool onMouseDown(QC::i32 x, QC::i32 y, QK::Event::MouseButton button) override;
            bool onMouseUp(QC::i32 x, QC::i32 y, QK::Event::MouseButton button) override;

        private:
            void paintVista(const Rect &abs);

            char m_text[256];
            Color m_textColor;
            Color m_borderColor;
            Color m_hoverColor;
            Color m_pressedColor;

            ButtonStyle m_visualStyle;
            Color m_vistaGradientTop;
            Color m_vistaGradientBottom;
            Color m_vistaHighlightTop;
            Color m_vistaHighlightBottom;
            Color m_vistaGlow;
            Color m_vistaBorderOuter;
            Color m_vistaBorderInner;

            ButtonClickHandler m_clickHandler;
            void *m_clickUserData;
        };

    } // namespace Controls
} // namespace QW
