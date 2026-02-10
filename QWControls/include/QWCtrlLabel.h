#pragma once

// QWControls Label - Static text label
// Namespace: QW::Controls

#include "QCTypes.h"
#include "QWWindowManager.h"
#include "QWWindow.h"
#include "QKEventTypes.h"

namespace QW
{
    namespace Controls
    {

        // Text alignment (use the QEvent-compatible definition)
        enum class TextAlign : QC::u8
        {
            Left,
            Center,
            Right
        };

        enum class VerticalAlign : QC::u8
        {
            Top,
            Middle,
            Bottom
        };

        class Label
        {
        public:
            Label(Window *parent, const char *text, Rect bounds);
            ~Label();

            // Properties
            const char *text() const { return m_text; }
            void setText(const char *text);

            Rect bounds() const { return m_bounds; }
            void setBounds(const Rect &bounds) { m_bounds = bounds; }

            // Alignment
            TextAlign textAlign() const { return m_textAlign; }
            void setTextAlign(TextAlign align) { m_textAlign = align; }

            VerticalAlign verticalAlign() const { return m_verticalAlign; }
            void setVerticalAlign(VerticalAlign align) { m_verticalAlign = align; }

            // Word wrap
            bool wordWrap() const { return m_wordWrap; }
            void setWordWrap(bool wrap) { m_wordWrap = wrap; }

            // Appearance
            Color backgroundColor() const { return m_bgColor; }
            void setBackgroundColor(Color color) { m_bgColor = color; }

            Color textColor() const { return m_textColor; }
            void setTextColor(Color color) { m_textColor = color; }

            bool transparent() const { return m_transparent; }
            void setTransparent(bool transparent) { m_transparent = transparent; }

            // Rendering
            void paint();

        private:
            Window *m_parent;
            char *m_text;
            Rect m_bounds;

            TextAlign m_textAlign;
            VerticalAlign m_verticalAlign;
            bool m_wordWrap;
            bool m_transparent;

            Color m_bgColor;
            Color m_textColor;
        };

    } // namespace Controls
} // namespace QW
