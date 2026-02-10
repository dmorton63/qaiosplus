// QWindowing Window - Window class with IPainter implementation
// Namespace: QW

#include "QWWindow.h"
#include "QWWindowManager.h"
#include "QKMemHeap.h"
#include "QCMemUtil.h"

namespace QW
{

    Window::Window(const char *title, Rect bounds)
        : m_windowId(0),
          m_bounds(bounds),
          m_flags(WindowFlags::Default),
          m_buffer(nullptr),
          m_bufferWidth(0),
          m_bufferHeight(0),
          m_clipRect{0, 0, 0, 0},
          m_hasClip(false),
          m_origin{0, 0}
    {
        setTitle(title);

        // Allocate content buffer
        m_bufferWidth = bounds.width;
        m_bufferHeight = bounds.height;
        QC::usize bufferBytes = m_bufferWidth * m_bufferHeight * sizeof(QC::u32);
        m_buffer = static_cast<QC::u32 *>(QK::Memory::Heap::instance().allocate(bufferBytes));
        if (m_buffer)
        {
            memset(m_buffer, 0, bufferBytes);
        }
    }

    Window::~Window()
    {
        if (m_buffer)
        {
            QK::Memory::Heap::instance().free(m_buffer);
            m_buffer = nullptr;
        }
    }

    bool Window::onEvent(const QK::Event::Event &event)
    {
        using namespace QK::Event;

        // Only handle events targeted at this window or global input events
        if (event.isWindow())
        {
            const auto &winEvent = event.asWindow();
            if (winEvent.windowId != m_windowId)
            {
                return false;
            }

            switch (event.type())
            {
            case Type::WindowFocus:
                onFocus();
                return true;
            case Type::WindowBlur:
                onBlur();
                return true;
            case Type::WindowResize:
                onResize(winEvent.width, winEvent.height);
                return true;
            case Type::WindowPaint:
                onPaint();
                return true;
            case Type::WindowDestroy:
                onClose();
                return true;
            default:
                break;
            }
        }
        else if (event.isInput() && isFocused())
        {
            switch (event.type())
            {
            case Type::MouseMove:
            {
                const auto &mouse = event.asMouse();
                // Convert to window-local coordinates
                QC::i32 localX = mouse.x - m_bounds.x;
                QC::i32 localY = mouse.y - m_bounds.y;
                onMouseMove(localX, localY, mouse.deltaX, mouse.deltaY);
                return true;
            }
            case Type::MouseButtonDown:
            {
                const auto &mouse = event.asMouse();
                QC::i32 localX = mouse.x - m_bounds.x;
                QC::i32 localY = mouse.y - m_bounds.y;
                onMouseDown(localX, localY, mouse.button);
                return true;
            }
            case Type::MouseButtonUp:
            {
                const auto &mouse = event.asMouse();
                QC::i32 localX = mouse.x - m_bounds.x;
                QC::i32 localY = mouse.y - m_bounds.y;
                onMouseUp(localX, localY, mouse.button);
                return true;
            }
            case Type::MouseScroll:
            {
                const auto &mouse = event.asMouse();
                onMouseScroll(mouse.scrollDelta);
                return true;
            }
            case Type::KeyDown:
            {
                const auto &key = event.asKey();
                onKeyDown(key.scancode, key.keycode, key.character, key.modifiers);
                return true;
            }
            case Type::KeyUp:
            {
                const auto &key = event.asKey();
                onKeyUp(key.scancode, key.keycode, key.modifiers);
                return true;
            }
            default:
                break;
            }
        }

        return false;
    }

    QK::Event::Category Window::getEventMask() const
    {
        return QK::Event::Category::Input | QK::Event::Category::Window;
    }

    void Window::setTitle(const char *title)
    {
        if (title)
        {
            strncpy(m_title, title, sizeof(m_title) - 1);
            m_title[sizeof(m_title) - 1] = '\0';
        }
        else
        {
            m_title[0] = '\0';
        }
    }

    void Window::setBounds(const Rect &bounds)
    {
        bool sizeChanged = (bounds.width != m_bounds.width ||
                            bounds.height != m_bounds.height);
        m_bounds = bounds;

        if (sizeChanged)
        {
            // Reallocate buffer
            if (m_buffer)
            {
                QK::Memory::Heap::instance().free(m_buffer);
            }

            m_bufferWidth = bounds.width;
            m_bufferHeight = bounds.height;
            QC::usize bufferBytes = m_bufferWidth * m_bufferHeight * sizeof(QC::u32);
            m_buffer = static_cast<QC::u32 *>(QK::Memory::Heap::instance().allocate(bufferBytes));
            if (m_buffer)
            {
                memset(m_buffer, 0, bufferBytes);
            }
        }
    }

    Rect Window::clientRect() const
    {
        // TODO: Account for window decorations (title bar, borders)
        return Rect{0, 0, m_bounds.width, m_bounds.height};
    }

    void Window::setVisible(bool visible)
    {
        if (visible)
        {
            m_flags |= WindowFlags::Visible;
        }
        else
        {
            m_flags &= ~WindowFlags::Visible;
        }
    }

    bool Window::isFocused() const
    {
        return WindowManager::instance().focusedWindow() == this;
    }

    // ==================== IPainter Implementation ====================

    QC::Size Window::size() const
    {
        return QC::Size(m_bufferWidth, m_bufferHeight);
    }

    QC::Rect Window::bounds() const
    {
        return QC::Rect(0, 0, m_bufferWidth, m_bufferHeight);
    }

    void Window::setClipRect(const QC::Rect &rect)
    {
        m_clipRect = rect;
        m_hasClip = true;
    }

    void Window::clearClipRect()
    {
        m_hasClip = false;
    }

    QC::Rect Window::clipRect() const
    {
        return m_hasClip ? m_clipRect : bounds();
    }

    void Window::setOrigin(QC::i32 x, QC::i32 y)
    {
        m_origin = QC::Point(x, y);
    }

    QC::Point Window::origin() const
    {
        return m_origin;
    }

    void Window::translate(QC::i32 dx, QC::i32 dy)
    {
        m_origin.x += dx;
        m_origin.y += dy;
    }

    void Window::setPixel(QC::i32 x, QC::i32 y, QC::Color color)
    {
        if (!m_buffer)
            return;

        x += m_origin.x;
        y += m_origin.y;

        if (x < 0 || y < 0)
            return;
        if (static_cast<QC::u32>(x) >= m_bufferWidth ||
            static_cast<QC::u32>(y) >= m_bufferHeight)
            return;

        if (m_hasClip && !m_clipRect.contains(x, y))
            return;

        m_buffer[y * m_bufferWidth + x] = color.value;
    }

    QC::Color Window::pixel(QC::i32 x, QC::i32 y) const
    {
        x += m_origin.x;
        y += m_origin.y;

        if (!m_buffer || x < 0 || y < 0)
            return QC::Color();
        if (static_cast<QC::u32>(x) >= m_bufferWidth ||
            static_cast<QC::u32>(y) >= m_bufferHeight)
            return QC::Color();

        return QC::Color(m_buffer[y * m_bufferWidth + x]);
    }

    void Window::drawLine(QC::i32 x1, QC::i32 y1, QC::i32 x2, QC::i32 y2, const QG::Pen &pen)
    {
        if (pen.isNull())
            return;

        QC::Color color = pen.color();

        // Bresenham's line algorithm
        QC::i32 dx = x2 - x1;
        QC::i32 dy = y2 - y1;
        QC::i32 sx = (dx > 0) ? 1 : -1;
        QC::i32 sy = (dy > 0) ? 1 : -1;
        dx = (dx > 0) ? dx : -dx;
        dy = (dy > 0) ? dy : -dy;

        QC::i32 err = dx - dy;
        QC::i32 x = x1, y = y1;

        while (true)
        {
            setPixel(x, y, color);
            if (x == x2 && y == y2)
                break;

            QC::i32 e2 = 2 * err;
            if (e2 > -dy)
            {
                err -= dy;
                x += sx;
            }
            if (e2 < dx)
            {
                err += dx;
                y += sy;
            }
        }
    }

    void Window::drawHLine(QC::i32 x, QC::i32 y, QC::u32 length, QC::Color color)
    {
        if (!m_buffer || length == 0)
            return;

        x += m_origin.x;
        y += m_origin.y;

        if (y < 0 || static_cast<QC::u32>(y) >= m_bufferHeight)
            return;

        QC::i32 x1 = (x < 0) ? 0 : x;
        QC::i32 x2 = x + static_cast<QC::i32>(length);
        if (x2 > static_cast<QC::i32>(m_bufferWidth))
            x2 = static_cast<QC::i32>(m_bufferWidth);

        if (m_hasClip)
        {
            if (y < m_clipRect.y || y >= m_clipRect.bottom())
                return;
            if (x1 < m_clipRect.x)
                x1 = m_clipRect.x;
            if (x2 > m_clipRect.right())
                x2 = m_clipRect.right();
        }

        if (x1 >= x2)
            return;

        QC::u32 *row = m_buffer + y * m_bufferWidth;
        for (QC::i32 px = x1; px < x2; ++px)
        {
            row[px] = color.value;
        }
    }

    void Window::drawVLine(QC::i32 x, QC::i32 y, QC::u32 length, QC::Color color)
    {
        if (!m_buffer || length == 0)
            return;

        x += m_origin.x;
        y += m_origin.y;

        if (x < 0 || static_cast<QC::u32>(x) >= m_bufferWidth)
            return;

        QC::i32 y1 = (y < 0) ? 0 : y;
        QC::i32 y2 = y + static_cast<QC::i32>(length);
        if (y2 > static_cast<QC::i32>(m_bufferHeight))
            y2 = static_cast<QC::i32>(m_bufferHeight);

        if (m_hasClip)
        {
            if (x < m_clipRect.x || x >= m_clipRect.right())
                return;
            if (y1 < m_clipRect.y)
                y1 = m_clipRect.y;
            if (y2 > m_clipRect.bottom())
                y2 = m_clipRect.bottom();
        }

        if (y1 >= y2)
            return;

        for (QC::i32 py = y1; py < y2; ++py)
        {
            m_buffer[py * m_bufferWidth + x] = color.value;
        }
    }

    void Window::fillRect(const QC::Rect &rect, const QG::Brush &brush)
    {
        if (!m_buffer || brush.isNull())
            return;

        QC::Rect r = rect.offset(m_origin.x, m_origin.y);

        // Clamp to buffer
        QC::i32 x1 = r.x < 0 ? 0 : r.x;
        QC::i32 y1 = r.y < 0 ? 0 : r.y;
        QC::i32 x2 = r.right();
        QC::i32 y2 = r.bottom();

        if (x2 > static_cast<QC::i32>(m_bufferWidth))
            x2 = static_cast<QC::i32>(m_bufferWidth);
        if (y2 > static_cast<QC::i32>(m_bufferHeight))
            y2 = static_cast<QC::i32>(m_bufferHeight);

        // Apply clip
        if (m_hasClip)
        {
            if (x1 < m_clipRect.x)
                x1 = m_clipRect.x;
            if (y1 < m_clipRect.y)
                y1 = m_clipRect.y;
            if (x2 > m_clipRect.right())
                x2 = m_clipRect.right();
            if (y2 > m_clipRect.bottom())
                y2 = m_clipRect.bottom();
        }

        if (x1 >= x2 || y1 >= y2)
            return;

        if (brush.style() == QG::BrushStyle::Solid)
        {
            QC::Color color = brush.color();
            for (QC::i32 y = y1; y < y2; ++y)
            {
                QC::u32 *row = m_buffer + y * m_bufferWidth;
                for (QC::i32 x = x1; x < x2; ++x)
                {
                    row[x] = color.value;
                }
            }
        }
        else if (brush.style() == QG::BrushStyle::LinearGradientV)
        {
            fillGradientV(QC::Rect(x1, y1, static_cast<QC::u32>(x2 - x1), static_cast<QC::u32>(y2 - y1)),
                          brush.color(), brush.colorEnd());
        }
        else if (brush.style() == QG::BrushStyle::LinearGradientH)
        {
            fillGradientH(QC::Rect(x1, y1, static_cast<QC::u32>(x2 - x1), static_cast<QC::u32>(y2 - y1)),
                          brush.color(), brush.colorEnd());
        }
    }

    void Window::drawRect(const QC::Rect &rect, const QG::Pen &pen)
    {
        if (pen.isNull())
            return;

        QC::Color color = pen.color();
        QC::u32 w = pen.width();

        for (QC::u32 i = 0; i < w; ++i)
        {
            QC::Rect r = rect.inset(static_cast<QC::i32>(i));
            if (r.isEmpty())
                break;

            // Top
            drawHLine(r.x, r.y, r.width, color);
            // Bottom
            drawHLine(r.x, r.bottom() - 1, r.width, color);
            // Left
            drawVLine(r.x, r.y + 1, r.height > 2 ? r.height - 2 : 0, color);
            // Right
            drawVLine(r.right() - 1, r.y + 1, r.height > 2 ? r.height - 2 : 0, color);
        }
    }

    void Window::drawRaisedBorder(const QC::Rect &rect, QC::Color light, QC::Color dark, QC::u32 width)
    {
        for (QC::u32 i = 0; i < width; ++i)
        {
            QC::Rect r = rect.inset(static_cast<QC::i32>(i));
            if (r.isEmpty())
                break;

            // Top (light)
            drawHLine(r.x, r.y, r.width, light);
            // Left (light)
            drawVLine(r.x, r.y + 1, r.height > 1 ? r.height - 1 : 0, light);
            // Bottom (dark)
            drawHLine(r.x, r.bottom() - 1, r.width, dark);
            // Right (dark)
            drawVLine(r.right() - 1, r.y, r.height > 1 ? r.height - 1 : 0, dark);
        }
    }

    void Window::drawSunkenBorder(const QC::Rect &rect, QC::Color light, QC::Color dark, QC::u32 width)
    {
        for (QC::u32 i = 0; i < width; ++i)
        {
            QC::Rect r = rect.inset(static_cast<QC::i32>(i));
            if (r.isEmpty())
                break;

            // Top (dark)
            drawHLine(r.x, r.y, r.width, dark);
            // Left (dark)
            drawVLine(r.x, r.y + 1, r.height > 1 ? r.height - 1 : 0, dark);
            // Bottom (light)
            drawHLine(r.x, r.bottom() - 1, r.width, light);
            // Right (light)
            drawVLine(r.right() - 1, r.y, r.height > 1 ? r.height - 1 : 0, light);
        }
    }

    void Window::drawEtchedBorder(const QC::Rect &rect, QC::Color light, QC::Color dark)
    {
        // Outer sunken
        drawSunkenBorder(rect, light, dark, 1);
        // Inner raised
        drawRaisedBorder(rect.inset(1), light, dark, 1);
    }

    void Window::fillGradientV(const QC::Rect &rect, QC::Color top, QC::Color bottom)
    {
        if (!m_buffer || rect.isEmpty())
            return;

        QC::Rect r = rect.offset(m_origin.x, m_origin.y);
        QC::i32 x1 = r.x < 0 ? 0 : r.x;
        QC::i32 y1 = r.y < 0 ? 0 : r.y;
        QC::i32 x2 = r.right();
        QC::i32 y2 = r.bottom();

        if (x2 > static_cast<QC::i32>(m_bufferWidth))
            x2 = static_cast<QC::i32>(m_bufferWidth);
        if (y2 > static_cast<QC::i32>(m_bufferHeight))
            y2 = static_cast<QC::i32>(m_bufferHeight);

        if (x1 >= x2 || y1 >= y2)
            return;

        QC::i32 height = y2 - y1;
        for (QC::i32 y = y1; y < y2; ++y)
        {
            QC::f32 t = static_cast<QC::f32>(y - y1) / static_cast<QC::f32>(height);
            QC::Color color = QC::Color::lerp(top, bottom, t);
            QC::u32 *row = m_buffer + y * m_bufferWidth;
            for (QC::i32 x = x1; x < x2; ++x)
            {
                row[x] = color.value;
            }
        }
    }

    void Window::fillGradientH(const QC::Rect &rect, QC::Color left, QC::Color right)
    {
        if (!m_buffer || rect.isEmpty())
            return;

        QC::Rect r = rect.offset(m_origin.x, m_origin.y);
        QC::i32 x1 = r.x < 0 ? 0 : r.x;
        QC::i32 y1 = r.y < 0 ? 0 : r.y;
        QC::i32 x2 = r.right();
        QC::i32 y2 = r.bottom();

        if (x2 > static_cast<QC::i32>(m_bufferWidth))
            x2 = static_cast<QC::i32>(m_bufferWidth);
        if (y2 > static_cast<QC::i32>(m_bufferHeight))
            y2 = static_cast<QC::i32>(m_bufferHeight);

        if (x1 >= x2 || y1 >= y2)
            return;

        QC::i32 width = x2 - x1;
        for (QC::i32 y = y1; y < y2; ++y)
        {
            QC::u32 *row = m_buffer + y * m_bufferWidth;
            for (QC::i32 x = x1; x < x2; ++x)
            {
                QC::f32 t = static_cast<QC::f32>(x - x1) / static_cast<QC::f32>(width);
                QC::Color color = QC::Color::lerp(left, right, t);
                row[x] = color.value;
            }
        }
    }

    void Window::drawText(QC::i32 x, QC::i32 y, const char *text, QC::Color color)
    {
        (void)x;
        (void)y;
        (void)text;
        (void)color;
        // TODO: Implement text rendering with font system
    }

    void Window::drawText(const QC::Rect &rect, const char *text, QC::Color color, const QG::TextFormat &format)
    {
        (void)rect;
        (void)text;
        (void)color;
        (void)format;
        // TODO: Implement text rendering with font system
    }

    QC::Size Window::measureText(const char *text) const
    {
        (void)text;
        // TODO: Implement with font system
        return QC::Size(0, 0);
    }

    void Window::blit(QC::i32 x, QC::i32 y, const QC::u32 *pixels, QC::u32 width, QC::u32 height, QC::u32 stride)
    {
        if (!m_buffer || !pixels)
            return;

        if (stride == 0)
            stride = width;

        x += m_origin.x;
        y += m_origin.y;

        for (QC::u32 row = 0; row < height; ++row)
        {
            QC::i32 destY = y + static_cast<QC::i32>(row);
            if (destY < 0 || static_cast<QC::u32>(destY) >= m_bufferHeight)
                continue;

            const QC::u32 *srcRow = pixels + row * stride;
            QC::u32 *destRow = m_buffer + destY * m_bufferWidth;

            for (QC::u32 col = 0; col < width; ++col)
            {
                QC::i32 destX = x + static_cast<QC::i32>(col);
                if (destX < 0 || static_cast<QC::u32>(destX) >= m_bufferWidth)
                    continue;

                destRow[destX] = srcRow[col];
            }
        }
    }

    void Window::blitAlpha(QC::i32 x, QC::i32 y, const QC::u32 *pixels, QC::u32 width, QC::u32 height, QC::u32 stride)
    {
        if (!m_buffer || !pixels)
            return;

        if (stride == 0)
            stride = width;

        x += m_origin.x;
        y += m_origin.y;

        for (QC::u32 row = 0; row < height; ++row)
        {
            QC::i32 destY = y + static_cast<QC::i32>(row);
            if (destY < 0 || static_cast<QC::u32>(destY) >= m_bufferHeight)
                continue;

            const QC::u32 *srcRow = pixels + row * stride;
            QC::u32 *destRow = m_buffer + destY * m_bufferWidth;

            for (QC::u32 col = 0; col < width; ++col)
            {
                QC::i32 destX = x + static_cast<QC::i32>(col);
                if (destX < 0 || static_cast<QC::u32>(destX) >= m_bufferWidth)
                    continue;

                QC::Color src(srcRow[col]);
                QC::Color dest(destRow[destX]);
                destRow[destX] = src.blend(dest).value;
            }
        }
    }

    void Window::clear(QC::Color color)
    {
        if (!m_buffer)
            return;

        QC::usize pixels = m_bufferWidth * m_bufferHeight;
        for (QC::usize i = 0; i < pixels; ++i)
        {
            m_buffer[i] = color.value;
        }
    }

    void Window::invalidate()
    {
        WindowManager::instance().invalidate(m_bounds);
    }

    void Window::invalidateRect(const Rect &rect)
    {
        Rect screenRect{
            m_bounds.x + rect.x,
            m_bounds.y + rect.y,
            rect.width,
            rect.height};
        WindowManager::instance().invalidate(screenRect);
    }

    // Default event handlers - can be overridden
    void Window::onMouseMove(QC::i32 x, QC::i32 y, QC::i32 deltaX, QC::i32 deltaY)
    {
        (void)x;
        (void)y;
        (void)deltaX;
        (void)deltaY;
    }

    void Window::onMouseDown(QC::i32 x, QC::i32 y, QK::Event::MouseButton button)
    {
        (void)x;
        (void)y;
        (void)button;
    }

    void Window::onMouseUp(QC::i32 x, QC::i32 y, QK::Event::MouseButton button)
    {
        (void)x;
        (void)y;
        (void)button;
    }

    void Window::onMouseScroll(QC::i32 delta)
    {
        (void)delta;
    }

    void Window::onKeyDown(QC::u8 scancode, QC::u8 keycode, char character, QK::Event::Modifiers mods)
    {
        (void)scancode;
        (void)keycode;
        (void)character;
        (void)mods;
    }

    void Window::onKeyUp(QC::u8 scancode, QC::u8 keycode, QK::Event::Modifiers mods)
    {
        (void)scancode;
        (void)keycode;
        (void)mods;
    }

    void Window::onFocus()
    {
    }

    void Window::onBlur()
    {
    }

    void Window::onResize(QC::u32 width, QC::u32 height)
    {
        (void)width;
        (void)height;
    }

    void Window::onPaint()
    {
    }

    void Window::onClose()
    {
    }

} // namespace QW
