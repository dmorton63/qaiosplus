// QWControls ListView - List/table view control implementation
// Namespace: QW::Controls

#include "QWCtrlListView.h"
#include "QCMemUtil.h"
#include "QCString.h"
#include "QWWindow.h"

namespace QW
{
    namespace Controls
    {

        ListView::ListView()
            : ControlBase(),
              m_selectionMode(SelectionMode::Single),
              m_scrollOffset(0),
              m_itemHeight(20),
              m_showHeader(true),
              m_textColor(Color(0, 0, 0, 255)),
              m_selColor(Color(0, 120, 215, 255)),
              m_headerColor(Color(230, 230, 230, 255)),
              m_selChangeHandler(nullptr),
              m_selChangeUserData(nullptr),
              m_dblClickHandler(nullptr),
              m_dblClickUserData(nullptr),
              m_hoverIndex(-1)
        {
            m_bgColor = Color(255, 255, 255, 255);
        }

        ListView::ListView(Window *window, Rect bounds)
            : ControlBase(window, bounds),
              m_selectionMode(SelectionMode::Single),
              m_scrollOffset(0),
              m_itemHeight(20),
              m_showHeader(true),
              m_textColor(Color(0, 0, 0, 255)),
              m_selColor(Color(0, 120, 215, 255)),
              m_headerColor(Color(230, 230, 230, 255)),
              m_selChangeHandler(nullptr),
              m_selChangeUserData(nullptr),
              m_dblClickHandler(nullptr),
              m_dblClickUserData(nullptr),
              m_hoverIndex(-1)
        {
            m_bgColor = Color(255, 255, 255, 255);
        }

        ListView::~ListView()
        {
        }

        void ListView::addColumn(const char *header, QC::u32 width, TextAlign align)
        {
            ListViewColumn col;
            if (header)
            {
                strncpy(col.header, header, sizeof(col.header) - 1);
                col.header[sizeof(col.header) - 1] = '\0';
            }
            else
            {
                col.header[0] = '\0';
            }
            col.width = width;
            col.align = align;
            m_columns.push_back(col);
        }

        void ListView::removeColumn(QC::usize index)
        {
            if (index < m_columns.size())
            {
                // Shift elements left to fill the gap
                for (QC::usize j = index; j < m_columns.size() - 1; ++j)
                {
                    m_columns[j] = m_columns[j + 1];
                }
                m_columns.pop_back();
            }
        }

        const ListViewColumn *ListView::column(QC::usize index) const
        {
            if (index < m_columns.size())
            {
                return &m_columns[index];
            }
            return nullptr;
        }

        QC::usize ListView::addItem(const char *text, void *userData)
        {
            ListViewItem item;
            if (text)
            {
                strncpy(item.text, text, sizeof(item.text) - 1);
                item.text[sizeof(item.text) - 1] = '\0';
            }
            else
            {
                item.text[0] = '\0';
            }
            item.userData = userData;
            item.selected = false;
            m_items.push_back(item);
            return m_items.size() - 1;
        }

        void ListView::removeItem(QC::usize index)
        {
            if (index < m_items.size())
            {
                // Shift elements left to fill the gap
                for (QC::usize j = index; j < m_items.size() - 1; ++j)
                {
                    m_items[j] = m_items[j + 1];
                }
                m_items.pop_back();
            }
        }

        void ListView::clearItems()
        {
            m_items.clear();
            m_scrollOffset = 0;
        }

        const ListViewItem *ListView::item(QC::usize index) const
        {
            if (index < m_items.size())
            {
                return &m_items[index];
            }
            return nullptr;
        }

        void ListView::setItemText(QC::usize index, const char *text)
        {
            if (index < m_items.size() && text)
            {
                strncpy(m_items[index].text, text, sizeof(m_items[index].text) - 1);
                m_items[index].text[sizeof(m_items[index].text) - 1] = '\0';
            }
        }

        void ListView::setItemData(QC::usize index, void *userData)
        {
            if (index < m_items.size())
            {
                m_items[index].userData = userData;
            }
        }

        QC::isize ListView::selectedIndex() const
        {
            for (QC::usize i = 0; i < m_items.size(); ++i)
            {
                if (m_items[i].selected)
                {
                    return static_cast<QC::isize>(i);
                }
            }
            return -1;
        }

        void ListView::setSelectedIndex(QC::isize index)
        {
            clearSelection();
            if (index >= 0 && static_cast<QC::usize>(index) < m_items.size())
            {
                m_items[index].selected = true;
                if (m_selChangeHandler)
                {
                    m_selChangeHandler(this, m_selChangeUserData);
                }
            }
        }

        void ListView::selectAll()
        {
            if (m_selectionMode != SelectionMode::Multiple)
                return;

            for (QC::usize i = 0; i < m_items.size(); ++i)
            {
                m_items[i].selected = true;
            }
            if (m_selChangeHandler)
            {
                m_selChangeHandler(this, m_selChangeUserData);
            }
        }

        void ListView::clearSelection()
        {
            bool hadSelection = false;
            for (QC::usize i = 0; i < m_items.size(); ++i)
            {
                if (m_items[i].selected)
                {
                    hadSelection = true;
                }
                m_items[i].selected = false;
            }
            if (hadSelection && m_selChangeHandler)
            {
                m_selChangeHandler(this, m_selChangeUserData);
            }
        }

        bool ListView::isSelected(QC::usize index) const
        {
            if (index < m_items.size())
            {
                return m_items[index].selected;
            }
            return false;
        }

        void ListView::setScrollOffset(QC::usize offset)
        {
            if (offset < m_items.size())
            {
                m_scrollOffset = offset;
            }
        }

        void ListView::ensureVisible(QC::usize index)
        {
            if (index >= m_items.size())
                return;

            QC::usize visible = visibleItemCount();
            if (index < m_scrollOffset)
            {
                m_scrollOffset = index;
            }
            else if (index >= m_scrollOffset + visible)
            {
                m_scrollOffset = index - visible + 1;
            }
        }

        void ListView::setSelectionChangeHandler(SelectionChangeHandler handler, void *userData)
        {
            m_selChangeHandler = handler;
            m_selChangeUserData = userData;
        }

        void ListView::setItemDoubleClickHandler(ItemDoubleClickHandler handler, void *userData)
        {
            m_dblClickHandler = handler;
            m_dblClickUserData = userData;
        }

        void ListView::paint()
        {
            if (!m_window || !m_visible)
                return;

            Rect abs = absoluteBounds();

            // Draw background
            m_window->fillRect(abs, m_bgColor);
            m_window->drawRect(abs, Color(128, 128, 128, 255));

            QC::i32 currentY = abs.y;

            // Draw header if enabled
            if (m_showHeader && m_columns.size() > 0)
            {
                Rect headerRect = {abs.x, currentY, abs.width, m_itemHeight};
                m_window->fillRect(headerRect, m_headerColor);

                QC::i32 colX = abs.x;
                for (QC::usize i = 0; i < m_columns.size(); ++i)
                {
                    m_window->drawText(colX + 4, currentY + static_cast<QC::i32>(m_itemHeight / 2),
                                       m_columns[i].header, m_textColor);
                    colX += static_cast<QC::i32>(m_columns[i].width);
                }
                currentY += static_cast<QC::i32>(m_itemHeight);
            }

            // Draw visible items
            QC::usize visible = visibleItemCount();
            for (QC::usize i = 0; i < visible && (m_scrollOffset + i) < m_items.size(); ++i)
            {
                QC::usize itemIndex = m_scrollOffset + i;
                const ListViewItem &item = m_items[itemIndex];

                Rect itemRect = {abs.x, currentY, abs.width, m_itemHeight};

                // Draw selection background
                if (item.selected)
                {
                    m_window->fillRect(itemRect, m_selColor);
                    m_window->drawText(abs.x + 4, currentY + static_cast<QC::i32>(m_itemHeight / 2),
                                       item.text, Color(255, 255, 255, 255));
                }
                else
                {
                    m_window->drawText(abs.x + 4, currentY + static_cast<QC::i32>(m_itemHeight / 2),
                                       item.text, m_textColor);
                }

                currentY += static_cast<QC::i32>(m_itemHeight);
            }
        }

        bool ListView::onMouseMove(QC::i32 x, QC::i32 y, QC::i32 deltaX, QC::i32 deltaY)
        {
            (void)deltaX;
            (void)deltaY;
            m_hoverIndex = static_cast<QC::i32>(itemAtPoint(x, y));
            return hitTest(x, y);
        }

        bool ListView::onMouseDown(QC::i32 x, QC::i32 y, QK::Event::MouseButton button)
        {
            if (!m_enabled || button != QK::Event::MouseButton::Left)
                return false;

            QC::isize index = itemAtPoint(x, y);
            if (index >= 0)
            {
                if (m_selectionMode == SelectionMode::Single)
                {
                    setSelectedIndex(index);
                }
                else if (m_selectionMode == SelectionMode::Multiple)
                {
                    m_items[static_cast<QC::usize>(index)].selected = !m_items[static_cast<QC::usize>(index)].selected;
                    if (m_selChangeHandler)
                    {
                        m_selChangeHandler(this, m_selChangeUserData);
                    }
                }
                invalidate();
                return true;
            }

            return hitTest(x, y);
        }

        bool ListView::onMouseUp(QC::i32 x, QC::i32 y, QK::Event::MouseButton button)
        {
            (void)x;
            (void)y;
            (void)button;
            return false;
        }

        bool ListView::onMouseScroll(QC::i32 delta)
        {
            if (delta > 0 && m_scrollOffset > 0)
            {
                m_scrollOffset--;
                invalidate();
                return true;
            }
            else if (delta < 0 && m_scrollOffset + visibleItemCount() < m_items.size())
            {
                m_scrollOffset++;
                invalidate();
                return true;
            }
            return false;
        }

        bool ListView::onKeyDown(QC::u8 scancode, QC::u8 keycode, char character, QK::Event::Modifiers mods)
        {
            (void)scancode;
            (void)keycode;
            (void)character;
            (void)mods;
            // TODO: Handle arrow keys for navigation
            return false;
        }

        QC::isize ListView::itemAtPoint(QC::i32 x, QC::i32 y)
        {
            Rect abs = absoluteBounds();

            if (x < abs.x || x >= abs.x + static_cast<QC::i32>(abs.width))
                return -1;

            QC::i32 headerHeight = m_showHeader ? static_cast<QC::i32>(m_itemHeight) : 0;
            QC::i32 contentY = abs.y + headerHeight;

            if (y < contentY || y >= abs.y + static_cast<QC::i32>(abs.height))
                return -1;

            QC::usize relY = static_cast<QC::usize>(y - contentY);
            QC::usize index = m_scrollOffset + relY / m_itemHeight;

            if (index < m_items.size())
            {
                return static_cast<QC::isize>(index);
            }
            return -1;
        }

        QC::usize ListView::visibleItemCount() const
        {
            QC::u32 headerHeight = m_showHeader ? m_itemHeight : 0;
            QC::u32 contentHeight = m_bounds.height > headerHeight ? m_bounds.height - headerHeight : 0;
            return contentHeight / m_itemHeight;
        }

    } // namespace Controls
} // namespace QW
