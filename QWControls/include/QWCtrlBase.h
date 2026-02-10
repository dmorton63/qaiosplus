#pragma once

// QWControls Base - Base interface for all controls
// Namespace: QW::Controls

#include "QCTypes.h"
#include "QWWindowManager.h"
#include "QKEventTypes.h"
#include "QKEventListener.h"

namespace QW
{

    class Window;

    namespace Controls
    {

        /// Forward declarations
        class Panel;

        /// Control ID type
        using ControlId = QC::u32;
        constexpr ControlId InvalidControlId = 0;

        /// Control states
        enum class ControlState : QC::u8
        {
            Normal,
            Hovered,
            Focused,
            Pressed,
            Disabled
        };

        /// Base interface for all controls - implements IEventReceiver
        class IControl : public QK::Event::IEventReceiver
        {
        public:
            virtual ~IControl() = default;

            // ==================== Type Information ====================
            /// Returns true if this control is a container (Panel or subclass)
            virtual bool isContainer() const { return false; }

            /// Cast to Panel if this is a container (avoids dynamic_cast)
            virtual Panel *asPanel() { return nullptr; }
            virtual const Panel *asPanel() const { return nullptr; }

            // ==================== Identity ====================
            /// Unique control ID
            virtual ControlId id() const = 0;
            virtual void setId(ControlId id) = 0;

            // ==================== Hierarchy ====================
            /// Parent panel (if any)
            virtual Panel *parent() const = 0;
            virtual void setParent(Panel *parent) = 0;

            /// Parent window
            virtual Window *window() const = 0;
            virtual void setWindow(Window *window) = 0;

            // ==================== Geometry ====================
            /// Bounds relative to parent
            virtual Rect bounds() const = 0;
            virtual void setBounds(const Rect &bounds) = 0;

            /// Absolute bounds (relative to window)
            virtual Rect absoluteBounds() const = 0;

            /// Hit testing - returns true if point is within control
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
            /// Paint the control
            virtual void paint() = 0;

            /// Invalidate (request repaint)
            virtual void invalidate() = 0;

            // ==================== Event Handling (IEventReceiver) ====================
            /// Main event handler - routes to specific handlers
            bool onEvent(const QK::Event::Event &event) override = 0;

            /// Event category mask
            QK::Event::Category getEventMask() const override
            {
                return QK::Event::Category::Input | QK::Event::Category::Window;
            }

            // Note: isEnabled() from IEventReceiver is satisfied by the pure virtual isEnabled() above

            // ==================== Specific Event Handlers ====================
            virtual bool onMouseMove(QC::i32 x, QC::i32 y, QC::i32 deltaX, QC::i32 deltaY) = 0;
            virtual bool onMouseDown(QC::i32 x, QC::i32 y, QK::Event::MouseButton button) = 0;
            virtual bool onMouseUp(QC::i32 x, QC::i32 y, QK::Event::MouseButton button) = 0;
            virtual bool onMouseScroll(QC::i32 delta) = 0;
            virtual bool onKeyDown(QC::u8 scancode, QC::u8 keycode, char character, QK::Event::Modifiers mods) = 0;
            virtual bool onKeyUp(QC::u8 scancode, QC::u8 keycode, QK::Event::Modifiers mods) = 0;
            virtual void onFocus() = 0;
            virtual void onBlur() = 0;
        };

        /// Base implementation of IControl with common functionality
        class ControlBase : public IControl
        {
        public:
            ControlBase();
            ControlBase(Window *window, Rect bounds);
            virtual ~ControlBase() = default;

            // ==================== Identity ====================
            ControlId id() const override { return m_id; }
            void setId(ControlId id) override { m_id = id; }

            // ==================== Hierarchy ====================
            Panel *parent() const override { return m_parent; }
            void setParent(Panel *parent) override { m_parent = parent; }

            Window *window() const override { return m_window; }
            void setWindow(Window *window) override { m_window = window; }

            // ==================== Geometry ====================
            Rect bounds() const override { return m_bounds; }
            void setBounds(const Rect &bounds) override { m_bounds = bounds; }

            Rect absoluteBounds() const override;
            bool hitTest(QC::i32 x, QC::i32 y) const override;

            // ==================== State ====================
            bool isEnabled() const override { return m_enabled; }
            void setEnabled(bool enabled) override;

            bool isVisible() const override { return m_visible; }
            void setVisible(bool visible) override { m_visible = visible; }

            bool isFocused() const override { return m_focused; }
            void setFocused(bool focused) override;

            ControlState state() const override { return m_state; }

            // ==================== Appearance ====================
            Color backgroundColor() const override { return m_bgColor; }
            void setBackgroundColor(Color color) override { m_bgColor = color; }

            // ==================== Rendering ====================
            void invalidate() override;

            // ==================== Event Handling ====================
            bool onEvent(const QK::Event::Event &event) override;

            // Default implementations (do nothing, return false = not handled)
            bool onMouseMove(QC::i32 x, QC::i32 y, QC::i32 deltaX, QC::i32 deltaY) override { return false; }
            bool onMouseDown(QC::i32 x, QC::i32 y, QK::Event::MouseButton button) override { return false; }
            bool onMouseUp(QC::i32 x, QC::i32 y, QK::Event::MouseButton button) override { return false; }
            bool onMouseScroll(QC::i32 delta) override { return false; }
            bool onKeyDown(QC::u8 scancode, QC::u8 keycode, char character, QK::Event::Modifiers mods) override { return false; }
            bool onKeyUp(QC::u8 scancode, QC::u8 keycode, QK::Event::Modifiers mods) override { return false; }
            void onFocus() override {}
            void onBlur() override {}

        protected:
            void setState(ControlState state) { m_state = state; }

            ControlId m_id;
            Panel *m_parent;
            Window *m_window;
            Rect m_bounds;
            bool m_enabled;
            bool m_visible;
            bool m_focused;
            ControlState m_state;
            Color m_bgColor;

        private:
            static ControlId s_nextId;
        };

    } // namespace Controls
} // namespace QW
