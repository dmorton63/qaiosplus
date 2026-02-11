#include "QWWindow.h"

#include <algorithm>
#include <cstring>

#include "QWWindowManager.h"

namespace QW
{

    Window::Window(const char *title, Rect bounds)
        : m_windowId(0),
          m_title{0},
          m_bounds(bounds),
          m_flags(WindowFlags::Default),
          m_buffer(nullptr),
          m_bufferWidth(0),
          m_bufferHeight(0),
          m_painter(new QG::PainterSurface()),
          m_root(nullptr)
    {
        setTitle(title);
        resizeBuffer(bounds.width, bounds.height);
        m_root = new Controls::Panel(this, Rect{0, 0, bounds.width, bounds.height});
    }

    Window::~Window()
    {
        delete m_root;
        m_root = nullptr;

        delete m_painter;
        m_painter = nullptr;

        delete[] m_buffer;
        m_buffer = nullptr;
    }

    bool Window::onEvent(const QK::Event::Event &event)
    {
        using namespace QK::Event;

        if (event.isWindow())
        {
            const auto &windowEvent = event.asWindow();
            if (windowEvent.windowId != m_windowId)
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
            {
                Rect newBounds{windowEvent.x, windowEvent.y, windowEvent.width, windowEvent.height};
                setBounds(newBounds);
                onResize(windowEvent.width, windowEvent.height);
                return true;
            }
            case Type::WindowMove:
                m_bounds.x = windowEvent.x;
                m_bounds.y = windowEvent.y;
                return true;
            case Type::WindowPaint:
                paint();
                WindowManager::instance().invalidate(m_bounds);
                return true;
            case Type::WindowDestroy:
                onClose();
                return true;
            default:
                break;
            }
        }
        else if (event.isInput() && m_root)
        {
            QK::Event::Event localEvent = event;

            switch (event.type())
            {
            case Type::MouseMove:
            case Type::MouseButtonDown:
            case Type::MouseButtonUp:
            case Type::MouseScroll:
            {
                auto &mouse = localEvent.asMouse();
                mouse.x -= m_bounds.x;
                mouse.y -= m_bounds.y;

                switch (event.type())
                {
                case Type::MouseMove:
                    onMouseMove(mouse.x, mouse.y, mouse.deltaX, mouse.deltaY);
                    break;
                case Type::MouseButtonDown:
                    onMouseDown(mouse.x, mouse.y, mouse.button);
                    break;
                case Type::MouseButtonUp:
                    onMouseUp(mouse.x, mouse.y, mouse.button);
                    break;
                case Type::MouseScroll:
                    onMouseScroll(mouse.scrollDelta);
                    break;
                default:
                    break;
                }
                break;
            }
            case Type::KeyDown:
            {
                const auto &key = localEvent.asKey();
                onKeyDown(key.scancode, key.keycode, key.character, key.modifiers);
                break;
            }
            case Type::KeyUp:
            {
                const auto &key = localEvent.asKey();
                onKeyUp(key.scancode, key.keycode, key.modifiers);
                break;
            }
            default:
                break;
            }

            return m_root->onEvent(localEvent);
        }

        return false;
    }

    QK::Event::Category Window::getEventMask() const
    {
        return QK::Event::Category::Input | QK::Event::Category::Window;
    }

    void Window::setTitle(const char *title)
    {
        if (!title)
        {
            m_title[0] = '\0';
            return;
        }

        std::strncpy(m_title, title, sizeof(m_title) - 1);
        m_title[sizeof(m_title) - 1] = '\0';
    }

    void Window::setBounds(const Rect &bounds)
    {
        bool sizeChanged = bounds.width != m_bounds.width || bounds.height != m_bounds.height;

        m_bounds.x = bounds.x;
        m_bounds.y = bounds.y;
        m_bounds.width = bounds.width;
        m_bounds.height = bounds.height;

        if (sizeChanged)
        {
            resizeBuffer(bounds.width, bounds.height);
            if (m_root)
            {
                m_root->setBounds(Rect{0, 0, bounds.width, bounds.height});
            }
        }
    }

    Rect Window::clientRect() const
    {
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

        invalidate();
    }

    bool Window::isFocused() const
    {
        return WindowManager::instance().focusedWindow() == this;
    }

    QC::Size Window::surfaceSize() const
    {
        return m_painter ? m_painter->size() : QC::Size();
    }

    QC::Rect Window::surfaceBounds() const
    {
        return m_painter ? m_painter->bounds() : QC::Rect();
    }

    void Window::setClipRect(const QC::Rect &rect)
    {
        if (m_painter)
        {
            m_painter->setClipRect(rect);
        }
    }

    void Window::clearClipRect()
    {
        if (m_painter)
        {
            m_painter->clearClipRect();
        }
    }

    QC::Rect Window::clipRect() const
    {
        return m_painter ? m_painter->clipRect() : QC::Rect();
    }

    void Window::setOrigin(QC::i32 x, QC::i32 y)
    {
        if (m_painter)
        {
            m_painter->setOrigin(x, y);
        }
    }

    QC::Point Window::origin() const
    {
        return m_painter ? m_painter->origin() : QC::Point();
    }

    void Window::translate(QC::i32 dx, QC::i32 dy)
    {
        if (m_painter)
        {
            m_painter->translate(dx, dy);
        }
    }

    void Window::setPixel(QC::i32 x, QC::i32 y, QC::Color color)
    {
        if (m_painter)
        {
            m_painter->setPixel(x, y, color);
        }
    }

    QC::Color Window::pixel(QC::i32 x, QC::i32 y) const
    {
        return m_painter ? m_painter->pixel(x, y) : QC::Color();
    }

    void Window::drawLine(QC::i32 x1, QC::i32 y1, QC::i32 x2, QC::i32 y2, const QG::Pen &pen)
    {
        if (m_painter)
        {
            m_painter->drawLine(x1, y1, x2, y2, pen);
        }
    }

    void Window::drawHLine(QC::i32 x, QC::i32 y, QC::u32 length, QC::Color color)
    {
        if (m_painter)
        {
            m_painter->drawHLine(x, y, length, color);
        }
    }

    void Window::drawVLine(QC::i32 x, QC::i32 y, QC::u32 length, QC::Color color)
    {
        if (m_painter)
        {
            m_painter->drawVLine(x, y, length, color);
        }
    }

    void Window::fillRect(const QC::Rect &rect, const QG::Brush &brush)
    {
        if (m_painter)
        {
            m_painter->fillRect(rect, brush);
        }
    }

    void Window::drawRect(const QC::Rect &rect, const QG::Pen &pen)
    {
        if (m_painter)
        {
            m_painter->drawRect(rect, pen);
        }
    }

    void Window::drawRaisedBorder(const QC::Rect &rect, QC::Color light, QC::Color dark, QC::u32 width)
    {
        if (m_painter)
        {
            m_painter->drawRaisedBorder(rect, light, dark, width);
        }
    }

    void Window::drawSunkenBorder(const QC::Rect &rect, QC::Color light, QC::Color dark, QC::u32 width)
    {
        if (m_painter)
        {
            m_painter->drawSunkenBorder(rect, light, dark, width);
        }
    }

    void Window::drawEtchedBorder(const QC::Rect &rect, QC::Color light, QC::Color dark)
    {
        if (m_painter)
        {
            m_painter->drawEtchedBorder(rect, light, dark);
        }
    }

    void Window::fillGradientV(const QC::Rect &rect, QC::Color top, QC::Color bottom)
    {
        if (m_painter)
        {
            m_painter->fillGradientV(rect, top, bottom);
        }
    }

    void Window::fillGradientH(const QC::Rect &rect, QC::Color left, QC::Color right)
    {
        if (m_painter)
        {
            m_painter->fillGradientH(rect, left, right);
        }
    }

    void Window::drawText(QC::i32 x, QC::i32 y, const char *text, QC::Color color)
    {
        if (m_painter)
        {
            m_painter->drawText(x, y, text, color);
        }
    }

    void Window::drawText(const QC::Rect &rect, const char *text, QC::Color color, const QG::TextFormat &format)
    {
        if (m_painter)
        {
            m_painter->drawText(rect, text, color, format);
        }
    }

    QC::Size Window::measureText(const char *text) const
    {
        return m_painter ? m_painter->measureText(text) : QC::Size();
    }

    void Window::blit(QC::i32 x, QC::i32 y, const QC::u32 *pixels, QC::u32 width, QC::u32 height, QC::u32 stride)
    {
        if (m_painter)
        {
            m_painter->blit(x, y, pixels, width, height, stride);
        }
    }

    void Window::blitAlpha(QC::i32 x, QC::i32 y, const QC::u32 *pixels, QC::u32 width, QC::u32 height, QC::u32 stride)
    {
        if (m_painter)
        {
            m_painter->blitAlpha(x, y, pixels, width, height, stride);
        }
    }

    void Window::clear(QC::Color color)
    {
        if (m_painter)
        {
            m_painter->clear(color);
        }
    }

    void Window::invalidate()
    {
        if (!isVisible())
        {
            return;
        }

        paint();
        WindowManager::instance().invalidate(m_bounds);
    }

    void Window::invalidateRect(const Rect &rect)
    {
        if (!isVisible())
        {
            return;
        }

        paint();
        Rect screenRect{
            m_bounds.x + rect.x,
            m_bounds.y + rect.y,
            rect.width,
            rect.height};
        WindowManager::instance().invalidate(screenRect);
    }

    void Window::resizeBuffer(QC::u32 width, QC::u32 height)
    {
        delete[] m_buffer;
        m_buffer = nullptr;

        m_bufferWidth = width;
        m_bufferHeight = height;

        if (width == 0 || height == 0)
        {
            if (m_painter)
            {
                m_painter->setSurface(nullptr, 0, 0);
            }
            return;
        }

        QC::usize pixelCount = static_cast<QC::usize>(width) * static_cast<QC::usize>(height);
        m_buffer = new QC::u32[pixelCount];
        std::fill_n(m_buffer, pixelCount, 0);

        if (m_painter)
        {
            m_painter->setSurface(m_buffer, width, height);
        }
    }

    void Window::paint()
    {
        if (!m_painter || !m_root)
        {
            return;
        }

        m_painter->setSurface(m_buffer, m_bufferWidth, m_bufferHeight);
        m_painter->setOrigin(0, 0);
        m_painter->clearClipRect();
        m_painter->clear(Color(240, 240, 240, 255));

        m_root->paint();
        onPaint();
    }

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
