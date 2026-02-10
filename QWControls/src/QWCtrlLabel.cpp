// QWControls Label - Static text label implementation
// Namespace: QW::Controls

#include "QWCtrlLabel.h"
#include "QKMemHeap.h"
#include "QCMemUtil.h"
#include "QCString.h"

namespace QW
{
    namespace Controls
    {

        Label::Label(Window *parent, const char *text, Rect bounds)
            : m_parent(parent),
              m_text(nullptr),
              m_bounds(bounds),
              m_textAlign(TextAlign::Left),
              m_verticalAlign(VerticalAlign::Top),
              m_wordWrap(false),
              m_transparent(false),
              m_bgColor(Color(255, 255, 255, 255)),
              m_textColor(Color(0, 0, 0, 255))
        {
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
            if (!m_parent)
                return;

            // Draw background if not transparent
            if (!m_transparent)
            {
                // TODO: Use window drawing primitives
                // m_parent->fillRect(m_bounds, m_bgColor);
            }

            if (m_text)
            {
                // TODO: Use window drawing primitives
                // Calculate text position based on alignment
                // m_parent->drawText(m_text, m_bounds, m_textColor, m_textAlign);
            }
        }

    } // namespace Controls
} // namespace QW
