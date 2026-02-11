#pragma once

// QWindowing Window - Window hosting the UI control hierarchy
// Namespace: QW

#include "QCTypes.h"
#include "QKEventListener.h"
#include "QKEventTypes.h"
#include "QWControls/Containers/Panel.h"
#include "QWInterfaces/IControl.h"
#include "QG/PainterSurface.h"

namespace QW
{
    class WindowManager;

    using Rect = QC::Rect;
    using Color = QC::Color;

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

    /// Window - wraps a painter surface and owns the root control tree
    class Window : public QK::Event::IEventReceiver
    {
    public:
        Window(const char *title, Rect bounds);
        ~Window();

        bool onEvent(const QK::Event::Event &event) override;
        QK::Event::Category getEventMask() const override;

        // Identity
        QC::u32 windowId() const { return m_windowId; }
        void setWindowId(QC::u32 id) { m_windowId = id; }

        // Window properties
        const char *title() const { return m_title; }
        void setTitle(const char *title);

        Rect bounds() const { return m_bounds; }
        void setBounds(const Rect &bounds);
        Rect clientRect() const;

        QC::u32 flags() const { return m_flags; }
        void setFlags(QC::u32 flags) { m_flags = flags; }

        bool isVisible() const { return (m_flags & WindowFlags::Visible) != 0; }
        void setVisible(bool visible);

        bool isFocused() const;

        // Control hierarchy
        Controls::Panel *root() const { return m_root; }

        // Buffer access (for compositor)
        QC::u32 *buffer() const { return m_buffer; }
        QC::u32 bufferWidth() const { return m_bufferWidth; }
        QC::u32 bufferHeight() const { return m_bufferHeight; }

        // Raw painter surface (for low-level helpers)
        QG::IPainter *painter() const { return m_painter; }

        // Painter accessors (wrappers around the internal surface)
        QC::Size surfaceSize() const;
        QC::Rect surfaceBounds() const;

        void setClipRect(const QC::Rect &rect);
        void clearClipRect();
        QC::Rect clipRect() const;

        void setOrigin(QC::i32 x, QC::i32 y);
        QC::Point origin() const;
        void translate(QC::i32 dx, QC::i32 dy);

        void setPixel(QC::i32 x, QC::i32 y, QC::Color color);
        QC::Color pixel(QC::i32 x, QC::i32 y) const;

        void drawLine(QC::i32 x1, QC::i32 y1, QC::i32 x2, QC::i32 y2, const QG::Pen &pen);
        void drawHLine(QC::i32 x, QC::i32 y, QC::u32 length, QC::Color color);
        void drawVLine(QC::i32 x, QC::i32 y, QC::u32 length, QC::Color color);

        void fillRect(const QC::Rect &rect, const QG::Brush &brush);
        void drawRect(const QC::Rect &rect, const QG::Pen &pen);

        void drawRaisedBorder(const QC::Rect &rect, QC::Color light, QC::Color dark, QC::u32 width = 1);
        void drawSunkenBorder(const QC::Rect &rect, QC::Color light, QC::Color dark, QC::u32 width = 1);
        void drawEtchedBorder(const QC::Rect &rect, QC::Color light, QC::Color dark);

        void fillGradientV(const QC::Rect &rect, QC::Color top, QC::Color bottom);
        void fillGradientH(const QC::Rect &rect, QC::Color left, QC::Color right);

        void drawText(QC::i32 x, QC::i32 y, const char *text, QC::Color color);
        void drawText(const QC::Rect &rect, const char *text, QC::Color color, const QG::TextFormat &format);
        QC::Size measureText(const char *text) const;

        void blit(QC::i32 x, QC::i32 y, const QC::u32 *pixels, QC::u32 width, QC::u32 height, QC::u32 stride = 0);
        void blitAlpha(QC::i32 x, QC::i32 y, const QC::u32 *pixels, QC::u32 width, QC::u32 height, QC::u32 stride = 0);

        void clear(QC::Color color);

        // Convenience overloads used by controls
        void fillRect(const Rect &rect, Color color) { fillRect(static_cast<const QC::Rect &>(rect), QG::Brush::solid(color)); }
        void drawRect(const Rect &rect, Color color) { drawRect(static_cast<const QC::Rect &>(rect), QG::Pen(color)); }

        // Invalidation helpers
        void invalidate();
        void invalidateRect(const Rect &rect);

    protected:
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
        void resizeBuffer(QC::u32 width, QC::u32 height);
        void paint();

        QC::u32 m_windowId;
        char m_title[256];
        Rect m_bounds;
        QC::u32 m_flags;

        QC::u32 *m_buffer;
        QC::u32 m_bufferWidth;
        QC::u32 m_bufferHeight;

        QG::PainterSurface *m_painter;
        Controls::Panel *m_root;
    };

} // namespace QW
