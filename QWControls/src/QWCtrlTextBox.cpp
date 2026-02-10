// QWControls TextBox - Text input control implementation
// Namespace: QW::Controls

#include "QWCtrlTextBox.h"
#include "QKMemHeap.h"
#include "QCMemUtil.h"
#include "QCString.h"

namespace QW
{
    namespace Controls
    {

        TextBox::TextBox(Window *parent, Rect bounds)
            : m_parent(parent),
              m_bounds(bounds),
              m_text(nullptr),
              m_textLength(0),
              m_textCapacity(0),
              m_cursorPos(0),
              m_selStart(0),
              m_selEnd(0),
              m_maxLength(1024),
              m_enabled(true),
              m_readOnly(false),
              m_password(false),
              m_focused(false),
              m_bgColor(Color(255, 255, 255, 255)),
              m_textColor(Color(0, 0, 0, 255)),
              m_changeHandler(nullptr),
              m_changeUserData(nullptr),
              m_scrollOffset(0)
        {
            m_placeholder[0] = '\0';

            // Initial allocation
            m_textCapacity = 256;
            m_text = static_cast<char *>(QK::Memory::Heap::instance().allocate(m_textCapacity));
            if (m_text)
            {
                m_text[0] = '\0';
            }
        }

        TextBox::~TextBox()
        {
            if (m_text)
            {
                QK::Memory::Heap::instance().free(m_text);
                m_text = nullptr;
            }
        }

        void TextBox::setText(const char *text)
        {
            if (!text)
            {
                if (m_text)
                {
                    m_text[0] = '\0';
                    m_textLength = 0;
                }
                return;
            }

            QC::usize len = strlen(text);
            if (len > m_maxLength)
            {
                len = m_maxLength;
            }

            if (len + 1 > m_textCapacity)
            {
                QC::usize newCapacity = len + 1;
                char *newText = static_cast<char *>(QK::Memory::Heap::instance().allocate(newCapacity));
                if (newText)
                {
                    QK::Memory::Heap::instance().free(m_text);
                    m_text = newText;
                    m_textCapacity = newCapacity;
                }
                else
                {
                    return;
                }
            }

            strncpy(m_text, text, len);
            m_text[len] = '\0';
            m_textLength = len;
            m_cursorPos = len;
            clearSelection();
        }

        void TextBox::setPlaceholder(const char *placeholder)
        {
            if (placeholder)
            {
                strncpy(m_placeholder, placeholder, sizeof(m_placeholder) - 1);
                m_placeholder[sizeof(m_placeholder) - 1] = '\0';
            }
            else
            {
                m_placeholder[0] = '\0';
            }
        }

        void TextBox::setCursorPosition(QC::usize pos)
        {
            if (pos > m_textLength)
            {
                pos = m_textLength;
            }
            m_cursorPos = pos;
        }

        void TextBox::setSelection(QC::usize start, QC::usize end)
        {
            if (start > m_textLength)
                start = m_textLength;
            if (end > m_textLength)
                end = m_textLength;

            m_selStart = start;
            m_selEnd = end;
            m_cursorPos = end;
        }

        void TextBox::selectAll()
        {
            m_selStart = 0;
            m_selEnd = m_textLength;
            m_cursorPos = m_textLength;
        }

        void TextBox::clearSelection()
        {
            m_selStart = 0;
            m_selEnd = 0;
        }

        void TextBox::setTextChangeHandler(TextChangeHandler handler, void *userData)
        {
            m_changeHandler = handler;
            m_changeUserData = userData;
        }

        void TextBox::paint()
        {
            if (!m_parent)
                return;

            // TODO: Use window drawing primitives
            // Draw background
            // m_parent->fillRect(m_bounds, m_bgColor);
            // m_parent->drawRect(m_bounds, Color(128, 128, 128, 255));

            // Draw text or placeholder
            // if (m_textLength > 0)
            // {
            //     if (m_password)
            //         draw masked text
            //     else
            //         draw m_text
            // }
            // else if (m_placeholder[0] != '\0')
            // {
            //     draw placeholder with dimmed color
            // }

            // Draw cursor if focused
            // if (m_focused)
            // {
            //     draw cursor at m_cursorPos
            // }

            // Draw selection if any
            // if (m_selStart != m_selEnd)
            // {
            //     highlight selection
            // }
        }

        void TextBox::handleKeyDown(QC::u8 scancode, QC::u8 keycode, QK::Event::Modifiers mods)
        {
            if (!m_enabled)
                return;

            (void)keycode;
            (void)mods;
            // TODO: Map scancodes to actions
            // Handle arrow keys, home, end, delete, backspace
            (void)scancode;
        }

        void TextBox::handleChar(char c)
        {
            if (!m_enabled || m_readOnly)
                return;

            // Delete selection first if any
            if (m_selStart != m_selEnd)
            {
                QC::usize start = m_selStart < m_selEnd ? m_selStart : m_selEnd;
                QC::usize end = m_selStart > m_selEnd ? m_selStart : m_selEnd;

                // Remove selected text
                QC::usize remaining = m_textLength - end;
                for (QC::usize i = 0; i <= remaining; ++i)
                {
                    m_text[start + i] = m_text[end + i];
                }
                m_textLength -= (end - start);
                m_cursorPos = start;
                clearSelection();
            }

            insertChar(c);
        }

        void TextBox::handleMouseDown(QC::i32 x, QC::i32 y, QK::Event::MouseButton button)
        {
            if (!m_enabled || button != QK::Event::MouseButton::Left)
                return;

            bool inside = x >= m_bounds.x && x < m_bounds.x + static_cast<QC::i32>(m_bounds.width) &&
                          y >= m_bounds.y && y < m_bounds.y + static_cast<QC::i32>(m_bounds.height);

            if (inside)
            {
                setFocused(true);
                // TODO: Calculate cursor position from x coordinate
                clearSelection();
            }
        }

        void TextBox::handleMouseMove(QC::i32 x, QC::i32 y)
        {
            (void)x;
            (void)y;
            // TODO: Handle selection dragging
        }

        void TextBox::handleMouseUp(QC::i32 x, QC::i32 y, QK::Event::MouseButton button)
        {
            (void)x;
            (void)y;
            (void)button;
        }

        void TextBox::setFocused(bool focused)
        {
            m_focused = focused;
            if (!focused)
            {
                clearSelection();
            }
        }

        void TextBox::insertChar(char c)
        {
            if (m_textLength >= m_maxLength)
                return;

            // Grow buffer if needed
            if (m_textLength + 2 > m_textCapacity)
            {
                QC::usize newCapacity = m_textCapacity * 2;
                char *newText = static_cast<char *>(QK::Memory::Heap::instance().allocate(newCapacity));
                if (newText)
                {
                    strcpy(newText, m_text);
                    QK::Memory::Heap::instance().free(m_text);
                    m_text = newText;
                    m_textCapacity = newCapacity;
                }
                else
                {
                    return;
                }
            }

            // Shift characters after cursor
            for (QC::usize i = m_textLength; i > m_cursorPos; --i)
            {
                m_text[i] = m_text[i - 1];
            }

            m_text[m_cursorPos] = c;
            m_cursorPos++;
            m_textLength++;
            m_text[m_textLength] = '\0';

            if (m_changeHandler)
            {
                m_changeHandler(this, m_changeUserData);
            }
        }

        void TextBox::deleteChar(bool forward)
        {
            if (forward)
            {
                if (m_cursorPos >= m_textLength)
                    return;

                for (QC::usize i = m_cursorPos; i < m_textLength; ++i)
                {
                    m_text[i] = m_text[i + 1];
                }
                m_textLength--;
            }
            else
            {
                if (m_cursorPos == 0)
                    return;

                m_cursorPos--;
                for (QC::usize i = m_cursorPos; i < m_textLength; ++i)
                {
                    m_text[i] = m_text[i + 1];
                }
                m_textLength--;
            }

            if (m_changeHandler)
            {
                m_changeHandler(this, m_changeUserData);
            }
        }

        void TextBox::moveCursor(QC::isize delta, bool extend)
        {
            QC::isize newPos = static_cast<QC::isize>(m_cursorPos) + delta;
            if (newPos < 0)
                newPos = 0;
            if (static_cast<QC::usize>(newPos) > m_textLength)
                newPos = static_cast<QC::isize>(m_textLength);

            if (extend)
            {
                if (m_selStart == m_selEnd)
                {
                    m_selStart = m_cursorPos;
                }
                m_selEnd = static_cast<QC::usize>(newPos);
            }
            else
            {
                clearSelection();
            }

            m_cursorPos = static_cast<QC::usize>(newPos);
        }

    } // namespace Controls
} // namespace QW
