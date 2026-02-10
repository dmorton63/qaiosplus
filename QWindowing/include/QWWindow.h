#pragma once

// QWindowing Window - Window class with IPainter support
// Namespace: QW

#include "QCTypes.h"
#include "QWWindowManager.h"
#include "QKEventTypes.h"
#include "QKEventListener.h"
#include "QGPainter.h"

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

    /// Window - implements IPainter for direct drawing
    class Window : public QK::Event::IEventReceiver, public QG::IPainter
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

        Rect windowBounds() const { return m_bounds; }
        void setBounds(const Rect &bounds);

        Rect clientRect() const;

        QC::u32 flags() const { return m_flags; }
        void setFlags(QC::u32 flags) { m_flags = flags; }

        bool isVisible() const { return m_flags & WindowFlags::Visible; }
        void setVisible(bool visible);

        bool isFocused() const;

        // Content buffer (for compositor)
        QC::u32 *buffer() const { return m_buffer; }
        QC::usize bufferSize() const { return m_bufferWidth * m_bufferHeight * 4; }
        QC::u32 bufferWidth() const { return m_bufferWidth; }
        QC::u32 bufferHeight() const { return m_bufferHeight; }

        // Invalidation
        void invalidate();
        void invalidateRect(const Rect &rect);

        // ==================== IPainter Implementation ====================

        QC::Size size() const override;
        QC::Rect bounds() const override;

        void setClipRect(const QC::Rect &rect) override;
        void clearClipRect() override;
        QC::Rect clipRect() const override;

        void setOrigin(QC::i32 x, QC::i32 y) override;
        QC::Point origin() const override;
        void translate(QC::i32 dx, QC::i32 dy) override;

        void setPixel(QC::i32 x, QC::i32 y, QC::Color color) override;
        QC::Color pixel(QC::i32 x, QC::i32 y) const override;

        void drawLine(QC::i32 x1, QC::i32 y1, QC::i32 x2, QC::i32 y2, const QG::Pen &pen) override;
        void drawHLine(QC::i32 x, QC::i32 y, QC::u32 length, QC::Color color) override;
        void drawVLine(QC::i32 x, QC::i32 y, QC::u32 length, QC::Color color) override;

        void fillRect(const QC::Rect &rect, const QG::Brush &brush) override;
        void drawRect(const QC::Rect &rect, const QG::Pen &pen) override;

        void drawRaisedBorder(const QC::Rect &rect, QC::Color light, QC::Color dark, QC::u32 width = 1) override;
        void drawSunkenBorder(const QC::Rect &rect, QC::Color light, QC::Color dark, QC::u32 width = 1) override;
        void drawEtchedBorder(const QC::Rect &rect, QC::Color light, QC::Color dark) override;

        void fillGradientV(const QC::Rect &rect, QC::Color top, QC::Color bottom) override;
        void fillGradientH(const QC::Rect &rect, QC::Color left, QC::Color right) override;

        void drawText(QC::i32 x, QC::i32 y, const char *text, QC::Color color) override;
        void drawText(const QC::Rect &rect, const char *text, QC::Color color, const QG::TextFormat &format) override;
        QC::Size measureText(const char *text) const override;

        void blit(QC::i32 x, QC::i32 y, const QC::u32 *pixels, QC::u32 width, QC::u32 height, QC::u32 stride = 0) override;
        void blitAlpha(QC::i32 x, QC::i32 y, const QC::u32 *pixels, QC::u32 width, QC::u32 height, QC::u32 stride = 0) override;

        void clear(QC::Color color) override;

        // Legacy convenience methods (call IPainter methods)
        void fillRect(const Rect &rect, Color color) { QG::IPainter::fillRect(rect, color); }
        void drawRect(const Rect &rect, Color color) { QG::IPainter::drawRect(rect, color); }

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

        // IPainter state
        QC::Rect m_clipRect;
        bool m_hasClip;
        QC::Point m_origin;
    };

} // namespace QW
