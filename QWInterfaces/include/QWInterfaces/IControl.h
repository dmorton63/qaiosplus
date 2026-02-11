#pragma once

// Core UI control interface contract
// Namespace: QW::Controls

#include "QCTypes.h"
#include "QCGeometry.h"
#include "QCColor.h"
#include "QKEventTypes.h"
#include "QKEventListener.h"

namespace QW
{
    using Rect = QC::Rect;
    using Point = QC::Point;
    using Color = QC::Color;

    class Window;

    namespace Controls
    {
        class Panel;

        /// Control ID type
        using ControlId = QC::u32;
        constexpr ControlId InvalidControlId = 0;

        /// Control state description for styling/interaction
        enum class ControlState : QC::u8
        {
            Normal,
            Hovered,
            Focused,
            Pressed,
            Disabled
        };

        /// Base interface for all UI controls
        class IControl : public QK::Event::IEventReceiver
        {
        public:
            virtual ~IControl() = default;

            // ==================== Type Information ====================
            virtual bool isContainer() const { return false; }
            virtual Panel *asPanel() { return nullptr; }
            virtual const Panel *asPanel() const { return nullptr; }

            // ==================== Identity ====================
            virtual ControlId id() const = 0;
            virtual void setId(ControlId id) = 0;

            // ==================== Hierarchy ====================
            virtual Panel *parent() const = 0;
            virtual void setParent(Panel *parent) = 0;

            virtual Window *window() const = 0;
            virtual void setWindow(Window *window) = 0;

            // ==================== Geometry ====================
            virtual Rect bounds() const = 0;
            virtual void setBounds(const Rect &bounds) = 0;
            virtual Rect absoluteBounds() const = 0;
            virtual bool hitTest(QC::i32 x, QC::i32 y) const = 0;

            // ==================== State ====================
            virtual bool isEnabled() const = 0;
            virtual void setEnabled(bool enabled) = 0;

            virtual bool isVisible() const = 0;
            virtual void setVisible(bool visible) = 0;

            virtual bool isFocused() const = 0;
            virtual void setFocused(bool focused) = 0;

            virtual ControlState state() const = 0;

            // ==================== Appearance ====================
            virtual Color backgroundColor() const = 0;
            virtual void setBackgroundColor(Color color) = 0;

            // ==================== Rendering ====================
            virtual void paint() = 0;
            virtual void invalidate() = 0;

            // ==================== Event Handling ====================
            virtual bool onEvent(const QK::Event::Event &event) override = 0;

            QK::Event::Category getEventMask() const override
            {
                return QK::Event::Category::Input | QK::Event::Category::Window;
            }

            virtual bool onMouseMove(QC::i32 x, QC::i32 y, QC::i32 deltaX, QC::i32 deltaY) = 0;
            virtual bool onMouseDown(QC::i32 x, QC::i32 y, QK::Event::MouseButton button) = 0;
            virtual bool onMouseUp(QC::i32 x, QC::i32 y, QK::Event::MouseButton button) = 0;
            virtual bool onMouseScroll(QC::i32 delta) = 0;
            virtual bool onKeyDown(QC::u8 scancode, QC::u8 keycode, char character, QK::Event::Modifiers mods) = 0;
            virtual bool onKeyUp(QC::u8 scancode, QC::u8 keycode, QK::Event::Modifiers mods) = 0;
            virtual void onFocus() = 0;
            virtual void onBlur() = 0;
        };
    }
} // namespace QW
