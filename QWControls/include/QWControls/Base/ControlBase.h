#pragma once

// ControlBase - default implementation of the IControl contract
// Namespace: QW::Controls

#include "QWInterfaces/IControl.h"

namespace QW
{
    class Window;

    namespace Controls
    {
        class Panel;

        class ControlBase : public IControl
        {
        public:
            ControlBase();
            ControlBase(Window *window, Rect bounds);
            virtual ~ControlBase() = default;

            // Identity
            ControlId id() const override { return m_id; }
            void setId(ControlId id) override { m_id = id; }

            // Hierarchy
            Panel *parent() const override { return m_parent; }
            void setParent(Panel *parent) override { m_parent = parent; }

            Window *window() const override { return m_window; }
            void setWindow(Window *window) override { m_window = window; }

            // Geometry
            Rect bounds() const override { return m_bounds; }
            void setBounds(const Rect &bounds) override { m_bounds = bounds; }

            Rect absoluteBounds() const override;
            bool hitTest(QC::i32 x, QC::i32 y) const override;

            // State
            bool isEnabled() const override { return m_enabled; }
            void setEnabled(bool enabled) override;

            bool isVisible() const override { return m_visible; }
            void setVisible(bool visible) override { m_visible = visible; }

            bool isFocused() const override { return m_focused; }
            void setFocused(bool focused) override;

            ControlState state() const override { return m_state; }

            // Appearance
            Color backgroundColor() const override { return m_bgColor; }
            void setBackgroundColor(Color color) override { m_bgColor = color; }

            // Rendering
            void invalidate() override;

            // Event routing
            bool onEvent(const QK::Event::Event &event) override;

            // Default handlers (no-op)
            bool onMouseMove(QC::i32, QC::i32, QC::i32, QC::i32) override { return false; }
            bool onMouseDown(QC::i32, QC::i32, QK::Event::MouseButton) override { return false; }
            bool onMouseUp(QC::i32, QC::i32, QK::Event::MouseButton) override { return false; }
            bool onMouseScroll(QC::i32) override { return false; }
            bool onKeyDown(QC::u8, QC::u8, char, QK::Event::Modifiers) override { return false; }
            bool onKeyUp(QC::u8, QC::u8, QK::Event::Modifiers) override { return false; }
            void onFocus() override {}
            void onBlur() override {}

        protected:
            void setState(ControlState state) { m_state = state; }

            ControlId m_id;
            Panel *m_parent;
            Window *m_window;
            QC::Rect m_bounds;
            bool m_enabled;
            bool m_visible;
            bool m_focused;
            ControlState m_state;
            Color m_bgColor;

        private:
            static ControlId s_nextId;
        };
    }
} // namespace QW
