// QWControls Label - Static text label implementation
// Namespace: QW::Controls

#include "QWControls/Leaf/Label.h"
#include "QKMemHeap.h"
#include "QCMemUtil.h"
#include "QCString.h"
#include "QWWindow.h"

namespace QW
{
    namespace Controls
    {

        Label::Label()
            : ControlBase(),
              m_text(nullptr),
              m_textAlign(TextAlign::Left),
              m_verticalAlign(VerticalAlign::Top),
              m_wordWrap(false),
              m_transparent(true),
              m_textColor(Color(0, 0, 0, 255))
        {
            m_bgColor = Color(255, 255, 255, 255);
        }

        Label::Label(Window *window, const char *text, Rect bounds)
            : ControlBase(window, bounds),
              m_text(nullptr),
              m_textAlign(TextAlign::Left),
              m_verticalAlign(VerticalAlign::Top),
              m_wordWrap(false),
              m_transparent(false),
              m_textColor(Color(0, 0, 0, 255))
        {
            m_bgColor = Color(255, 255, 255, 255);
            setText(text);
        }

        Label::~Label()
        {
            if (m_text)
            {
                QK::Memory::Heap::instance().free(m_text);
                m_text = nullptr;
            }
        }

        void Label::setText(const char *text)
        {
            if (m_text)
            {
                QK::Memory::Heap::instance().free(m_text);
                m_text = nullptr;
            }

            if (text)
            {
                QC::usize len = strlen(text);
                m_text = static_cast<char *>(QK::Memory::Heap::instance().allocate(len + 1));
                if (m_text)
                {
                    strcpy(m_text, text);
                }
            }
        }

        void Label::paint()
        {
            if (!m_window || !m_visible)
                return;

            Rect abs = absoluteBounds();

            // Draw background if not transparent
            if (!m_transparent)
            {
                m_window->fillRect(abs, m_bgColor);
            }

            if (m_text)
            {
                // Calculate text position based on alignment
                QC::i32 textX = abs.x;
                QC::i32 textY = abs.y;

                switch (m_textAlign)
                {
                case TextAlign::Center:
                    textX = abs.x + static_cast<QC::i32>(abs.width / 2);
                    break;
                case TextAlign::Right:
                    textX = abs.x + static_cast<QC::i32>(abs.width);
                    break;
                default:
                    break;
                }

                switch (m_verticalAlign)
                {
                case VerticalAlign::Middle:
                    textY = abs.y + static_cast<QC::i32>(abs.height / 2);
                    break;
                case VerticalAlign::Bottom:
                    textY = abs.y + static_cast<QC::i32>(abs.height);
                    break;
                default:
                    break;
                }

                m_window->drawText(textX, textY, m_text, m_textColor);
            }
        }

    } // namespace Controls
} // namespace QW
