#pragma once

// QWControls TextBox - Text input control
// Namespace: QW::Controls

#include "QCTypes.h"
#include "QWWindowManager.h"
#include "QWWindow.h"
#include "QKEventTypes.h"

namespace QW
{
    namespace Controls
    {

        // Text change callback
        class TextBox;
        using TextChangeHandler = void (*)(TextBox *textBox, void *userData);

        class TextBox
        {
        public:
            TextBox(Window *parent, Rect bounds);
            ~TextBox();

            // Properties
            const char *text() const { return m_text; }
            void setText(const char *text);

            const char *placeholder() const { return m_placeholder; }
            void setPlaceholder(const char *placeholder);

            Rect bounds() const { return m_bounds; }
            void setBounds(const Rect &bounds) { m_bounds = bounds; }

            bool isEnabled() const { return m_enabled; }
            void setEnabled(bool enabled) { m_enabled = enabled; }

            bool isReadOnly() const { return m_readOnly; }
            void setReadOnly(bool readOnly) { m_readOnly = readOnly; }

            bool isPassword() const { return m_password; }
            void setPassword(bool password) { m_password = password; }

            QC::usize maxLength() const { return m_maxLength; }
            void setMaxLength(QC::usize length) { m_maxLength = length; }

            // Selection
            QC::usize cursorPosition() const { return m_cursorPos; }
            void setCursorPosition(QC::usize pos);

            QC::usize selectionStart() const { return m_selStart; }
            QC::usize selectionEnd() const { return m_selEnd; }
            void setSelection(QC::usize start, QC::usize end);
            void selectAll();
            void clearSelection();

            // Appearance
            Color backgroundColor() const { return m_bgColor; }
            void setBackgroundColor(Color color) { m_bgColor = color; }

            Color textColor() const { return m_textColor; }
            void setTextColor(Color color) { m_textColor = color; }

            // Events
            void setTextChangeHandler(TextChangeHandler handler, void *userData);

            // Rendering
            void paint();

            // Input handling (using QEvent types)
            void handleKeyDown(QC::u8 scancode, QC::u8 keycode, QK::Event::Modifiers mods);
            void handleChar(char c);
            void handleMouseDown(QC::i32 x, QC::i32 y, QK::Event::MouseButton button);
            void handleMouseMove(QC::i32 x, QC::i32 y);
            void handleMouseUp(QC::i32 x, QC::i32 y, QK::Event::MouseButton button);

            // Focus
            void setFocused(bool focused);
            bool isFocused() const { return m_focused; }

        private:
            void insertChar(char c);
            void deleteChar(bool forward);
            void moveCursor(QC::isize delta, bool extend);

            Window *m_parent;
            Rect m_bounds;

            char *m_text;
            QC::usize m_textLength;
            QC::usize m_textCapacity;
            char m_placeholder[256];

            QC::usize m_cursorPos;
            QC::usize m_selStart;
            QC::usize m_selEnd;
            QC::usize m_maxLength;

            bool m_enabled;
            bool m_readOnly;
            bool m_password;
            bool m_focused;

            Color m_bgColor;
            Color m_textColor;

            TextChangeHandler m_changeHandler;
            void *m_changeUserData;

            // Scroll position for long text
            QC::usize m_scrollOffset;
        };

    } // namespace Controls
} // namespace QW
