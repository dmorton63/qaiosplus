#pragma once

// QWindowing Window Manager - Desktop window management
// Namespace: QW

#include "QCTypes.h"
#include "QCVector.h"
#include "QCGeometry.h"
#include "QCColor.h"
#include "QKEventTypes.h"
#include "QKEventListener.h"
#include "QWStyleSystem.h"

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

    // Type aliases - use QC types in QW namespace for convenience
    using Point = QC::Point;
    using Size = QC::Size;
    using Rect = QC::Rect;
    using Color = QC::Color;
    using Margins = QC::Margins;

    class WindowManager : public QK::Event::IEventReceiver,
                          public StyleSystem::IStyleListener
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

        void onStyleChanged(const StyleSnapshot &snapshot) override;

    private:
        WindowManager();
        ~WindowManager();
        WindowManager(const WindowManager &) = delete;
        WindowManager &operator=(const WindowManager &) = delete;

        Window *windowAt(Point p);
        void routeMouseEvent(const QK::Event::MouseEventData &mouse);
        void routeKeyEvent(const QK::Event::KeyEventData &key);
        void postWindowEvent(QK::Event::Type type, Window *window);
        void applyStyleToWindow(Window *window, const StyleSnapshot &snapshot);

        struct PendingDestroy
        {
            Window *window;
            Rect bounds;
        };

        void processPendingDestroy();

        QC::u32 m_nextWindowId;
        QC::Vector<Window *> m_windows;
        Window *m_focusedWindow;
        Window *m_hoveredWindow;
        Framebuffer *m_framebuffer;
        Compositor *m_compositor;

        Point m_mousePos;
        QK::Event::ListenerId m_listenerId;

        // Window drag/move state (title bar)
        Window *m_dragWindow;
        Point m_dragOffset;
        Rect m_dragStartBounds;

        QC::u32 m_dispatchDepth = 0;
        QC::Vector<PendingDestroy> m_pendingDestroy;
    };

} // namespace QW
