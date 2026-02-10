#pragma once

// QWControls Button - Button control
// Namespace: QW::Controls

#include "QCTypes.h"
#include "QWCtrlBase.h"
#include "QWWindowManager.h"
#include "QWWindow.h"
#include "QKEventTypes.h"

namespace QW
{
    namespace Controls
    {

        // Button click callback
        class Button;
        using ButtonClickHandler = void (*)(Button *button, void *userData);

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

            // Events
            void setClickHandler(ButtonClickHandler handler, void *userData);

            // Rendering (override from ControlBase)
            void paint() override;

            // Event handlers (override from ControlBase)
            bool onMouseMove(QC::i32 x, QC::i32 y, QC::i32 deltaX, QC::i32 deltaY) override;
            bool onMouseDown(QC::i32 x, QC::i32 y, QK::Event::MouseButton button) override;
            bool onMouseUp(QC::i32 x, QC::i32 y, QK::Event::MouseButton button) override;

        private:
            char m_text[256];
            Color m_textColor;
            Color m_borderColor;
            Color m_hoverColor;
            Color m_pressedColor;

            ButtonClickHandler m_clickHandler;
            void *m_clickUserData;
        };

    } // namespace Controls
} // namespace QW
