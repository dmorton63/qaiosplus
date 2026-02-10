#pragma once

// QWindowing Window Manager - Desktop window management
// Namespace: QW

#include "QCTypes.h"
#include "QCVector.h"
#include "QKEventTypes.h"
#include "QKEventListener.h"

namespace QK
{
    namespace Event
    {
        class EventManager;
    }
}

namespace QW
{

    class Window;
    class Renderer;
    class Framebuffer;
    class Compositor;

    // Point
    struct Point
    {
        QC::i32 x;
        QC::i32 y;
    };

    // Size
    struct Size
    {
        QC::u32 width;
        QC::u32 height;
    };

    // Rectangle
    struct Rect
    {
        QC::i32 x;
        QC::i32 y;
        QC::u32 width;
        QC::u32 height;

        bool contains(Point p) const
        {
            return p.x >= x && p.x < x + static_cast<QC::i32>(width) &&
                   p.y >= y && p.y < y + static_cast<QC::i32>(height);
        }

        bool intersects(const Rect &other) const;
        Rect intersection(const Rect &other) const;
    };

    // Color (ARGB)
    struct Color
    {
        union
        {
            QC::u32 value;
            struct
            {
                QC::u8 b, g, r, a;
            };
        };

        Color() : value(0) {}
        Color(QC::u8 red, QC::u8 green, QC::u8 blue, QC::u8 alpha = 255)
        {
            r = red;
            g = green;
            b = blue;
            a = alpha;
        }

        static Color fromRGB(QC::u8 r, QC::u8 g, QC::u8 b)
        {
            Color c;
            c.r = r;
            c.g = g;
            c.b = b;
            c.a = 255;
            return c;
        }

        static Color fromARGB(QC::u8 a, QC::u8 r, QC::u8 g, QC::u8 b)
        {
            Color c;
            c.a = a;
            c.r = r;
            c.g = g;
            c.b = b;
            return c;
        }
    };

    class WindowManager : public QK::Event::IEventReceiver
    {
    public:
        static WindowManager &instance();

        void initialize(Framebuffer *fb);
        void shutdown();

        // IEventReceiver implementation
        bool onEvent(const QK::Event::Event &event) override;
        QK::Event::Category getEventMask() const override;

        // Window management
        Window *createWindow(const char *title, Rect bounds);
        void destroyWindow(Window *window);
        Window *windowById(QC::u32 id) const;

        // Focus
        void setFocus(Window *window);
        Window *focusedWindow() const { return m_focusedWindow; }

        // Z-order
        void bringToFront(Window *window);
        void sendToBack(Window *window);

        // Rendering
        void invalidate(const Rect &rect);
        void render();

        // Screen properties
        Size screenSize() const;

        // Compositor
        Compositor *compositor() { return m_compositor; }

        // Mouse state
        Point mousePosition() const { return m_mousePos; }

        // Window access for compositor
        QC::usize windowCount() const { return m_windows.size(); }
        Window *windowAtIndex(QC::usize index) { return m_windows[index]; }

    private:
        WindowManager();
        ~WindowManager();
        WindowManager(const WindowManager &) = delete;
        WindowManager &operator=(const WindowManager &) = delete;

        Window *windowAt(Point p);
        void routeMouseEvent(const QK::Event::MouseEventData &mouse);
        void routeKeyEvent(const QK::Event::KeyEventData &key);
        void postWindowEvent(QK::Event::Type type, Window *window);

        QC::u32 m_nextWindowId;
        QC::Vector<Window *> m_windows;
        Window *m_focusedWindow;
        Window *m_hoveredWindow;
        Framebuffer *m_framebuffer;
        Compositor *m_compositor;

        Point m_mousePos;
        QK::Event::ListenerId m_listenerId;
    };

} // namespace QW
