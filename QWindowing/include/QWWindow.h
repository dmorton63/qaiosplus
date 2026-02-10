#pragma once

// QWindowing Window - Window class
// Namespace: QW

#include "QCTypes.h"
#include "QWWindowManager.h"
#include "QKEventTypes.h"
#include "QKEventListener.h"

namespace QW
{

    // Window flags
    namespace WindowFlags
    {
        constexpr QC::u32 Visible = 0x0001;
        constexpr QC::u32 Resizable = 0x0002;
        constexpr QC::u32 Movable = 0x0004;
        constexpr QC::u32 HasTitle = 0x0008;
        constexpr QC::u32 HasBorder = 0x0010;
        constexpr QC::u32 HasClose = 0x0020;
        constexpr QC::u32 HasMinimize = 0x0040;
        constexpr QC::u32 HasMaximize = 0x0080;

        constexpr QC::u32 Default = Visible | Resizable | Movable |
                                    HasTitle | HasBorder | HasClose |
                                    HasMinimize | HasMaximize;
    }

    class Window : public QK::Event::IEventReceiver
    {
    public:
        Window(const char *title, Rect bounds);
        ~Window();

        // IEventReceiver implementation
        bool onEvent(const QK::Event::Event &event) override;
        QK::Event::Category getEventMask() const override;

        // Window ID for event routing
        QC::u32 windowId() const { return m_windowId; }
        void setWindowId(QC::u32 id) { m_windowId = id; }

        // Properties
        const char *title() const { return m_title; }
        void setTitle(const char *title);

        Rect bounds() const { return m_bounds; }
        void setBounds(const Rect &bounds);

        Rect clientRect() const;

        QC::u32 flags() const { return m_flags; }
        void setFlags(QC::u32 flags) { m_flags = flags; }

        bool isVisible() const { return m_flags & WindowFlags::Visible; }
        void setVisible(bool visible);

        bool isFocused() const;

        // Content buffer
        QC::u32 *buffer() const { return m_buffer; }
        QC::usize bufferSize() const { return m_bufferWidth * m_bufferHeight * 4; }
        QC::u32 bufferWidth() const { return m_bufferWidth; }
        QC::u32 bufferHeight() const { return m_bufferHeight; }

        // Drawing helpers
        void clear(Color color);
        void setPixel(QC::i32 x, QC::i32 y, Color color);
        void fillRect(const Rect &rect, Color color);
        void drawRect(const Rect &rect, Color color);
        void drawText(QC::i32 x, QC::i32 y, const char *text, Color color);

        // Invalidation
        void invalidate();
        void invalidateRect(const Rect &rect);

    protected:
        // Event handlers - override in subclasses
        virtual void onMouseMove(QC::i32 x, QC::i32 y, QC::i32 deltaX, QC::i32 deltaY);
        virtual void onMouseDown(QC::i32 x, QC::i32 y, QK::Event::MouseButton button);
        virtual void onMouseUp(QC::i32 x, QC::i32 y, QK::Event::MouseButton button);
        virtual void onMouseScroll(QC::i32 delta);
        virtual void onKeyDown(QC::u8 scancode, QC::u8 keycode, char character, QK::Event::Modifiers mods);
        virtual void onKeyUp(QC::u8 scancode, QC::u8 keycode, QK::Event::Modifiers mods);
        virtual void onFocus();
        virtual void onBlur();
        virtual void onResize(QC::u32 width, QC::u32 height);
        virtual void onPaint();
        virtual void onClose();

    private:
        QC::u32 m_windowId;
        char m_title[256];
        Rect m_bounds;
        QC::u32 m_flags;

        QC::u32 *m_buffer;
        QC::u32 m_bufferWidth;
        QC::u32 m_bufferHeight;
    };

} // namespace QW
