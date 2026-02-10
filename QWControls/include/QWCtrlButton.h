#pragma once

// QWControls Button - Button control
// Namespace: QW::Controls

#include "QCTypes.h"
#include "QWWindowManager.h"
#include "QWWindow.h"
#include "QKEventTypes.h"

namespace QW
{
    namespace Controls
    {

        // Button states
        enum class ButtonState : QC::u8
        {
            Normal,
            Hovered,
            Pressed,
            Disabled
        };

        // Button click callback
        class Button;
        using ButtonClickHandler = void (*)(Button *button, void *userData);

        class Button
        {
        public:
            Button(Window *parent, const char *text, Rect bounds);
            ~Button();

            // Properties
            const char *text() const { return m_text; }
            void setText(const char *text);

            Rect bounds() const { return m_bounds; }
            void setBounds(const Rect &bounds) { m_bounds = bounds; }

            bool isEnabled() const { return m_enabled; }
            void setEnabled(bool enabled);

            ButtonState state() const { return m_state; }

            // Appearance
            Color backgroundColor() const { return m_bgColor; }
            void setBackgroundColor(Color color) { m_bgColor = color; }

            Color textColor() const { return m_textColor; }
            void setTextColor(Color color) { m_textColor = color; }

            Color borderColor() const { return m_borderColor; }
            void setBorderColor(Color color) { m_borderColor = color; }

            // Events
            void setClickHandler(ButtonClickHandler handler, void *userData);

            // Rendering
            void paint();

            // Input handling (using QEvent types)
            void handleMouseMove(QC::i32 x, QC::i32 y);
            void handleMouseDown(QC::i32 x, QC::i32 y, QK::Event::MouseButton button);
            void handleMouseUp(QC::i32 x, QC::i32 y, QK::Event::MouseButton button);

        private:
            Window *m_parent;
            char m_text[256];
            Rect m_bounds;
            bool m_enabled;
            ButtonState m_state;

            Color m_bgColor;
            Color m_textColor;
            Color m_borderColor;

            ButtonClickHandler m_clickHandler;
            void *m_clickUserData;
        };

    } // namespace Controls
} // namespace QW
