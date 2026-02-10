#pragma once

// QWControls Container - Pure container control for child controls
// Namespace: QW::Controls
//
// This is a pure container that knows about:
// - Child list management
// - Event routing to children
// - Focus management
// - Hit-testing
// - Coordinate transforms
// - Painting children
//
// It does NOT know about:
// - Borders, padding, or visual decoration (that's Frame's job)
// - Background colors or styling

#include "QCTypes.h"
#include "QCVector.h"
#include "QWCtrlBase.h"
#include "QWWindowManager.h"
#include "QKEventTypes.h"

namespace QW
{
    class Window;

    namespace Controls
    {

        /// Container - A pure container control that holds and manages child controls
        /// Handles event routing and focus management, but no visual decoration
        class Container : public ControlBase
        {
        public:
            Container();
            Container(Window *window, Rect bounds);
            virtual ~Container();

            // ==================== Type Information ====================
            bool isContainer() const override { return true; }
            Panel *asPanel() override;
            const Panel *asPanel() const override;

            // ==================== Child Management ====================

            /// Add a child control to this container
            void addChild(IControl *child);

            /// Remove a child control from this container
            void removeChild(IControl *child);

            /// Remove child at index
            void removeChildAt(QC::usize index);

            /// Clear all children
            void clearChildren();

            /// Get child count
            QC::usize childCount() const { return m_children.size(); }

            /// Get child at index
            IControl *childAt(QC::usize index);
            const IControl *childAt(QC::usize index) const;

            /// Find child by ID (recursive search)
            IControl *findChild(ControlId id);

            /// Find child at point (for hit testing)
            IControl *childAtPoint(QC::i32 x, QC::i32 y);

            // ==================== Rendering ====================

            /// Paint the container (just paints children, no decoration)
            void paint() override;

            /// Paint all visible children
            virtual void paintChildren();

            // ==================== Event Handling ====================

            /// Override to propagate events to children
            bool onEvent(const QK::Event::Event &event) override;

            /// Mouse events - propagate to children
            bool onMouseMove(QC::i32 x, QC::i32 y, QC::i32 deltaX, QC::i32 deltaY) override;
            bool onMouseDown(QC::i32 x, QC::i32 y, QK::Event::MouseButton button) override;
            bool onMouseUp(QC::i32 x, QC::i32 y, QK::Event::MouseButton button) override;
            bool onMouseScroll(QC::i32 delta) override;

            /// Keyboard events - propagate to focused child
            bool onKeyDown(QC::u8 scancode, QC::u8 keycode, char character, QK::Event::Modifiers mods) override;
            bool onKeyUp(QC::u8 scancode, QC::u8 keycode, QK::Event::Modifiers mods) override;

            // ==================== Focus Management ====================

            /// Currently focused child control
            IControl *focusedChild() const { return m_focusedChild; }

            /// Set focus to a child control
            void setFocusedChild(IControl *child);

            /// Move focus to next/previous control
            void focusNext();
            void focusPrevious();

            // ==================== Coordinate Conversion ====================

            /// Convert window coordinates to local coordinates
            Point windowToLocal(QC::i32 x, QC::i32 y) const;

            /// Convert local coordinates to window coordinates
            Point localToWindow(QC::i32 x, QC::i32 y) const;

        protected:
            QC::Vector<IControl *> m_children;
            IControl *m_focusedChild;
            IControl *m_hoveredChild;
            IControl *m_capturedChild; // Mouse capture
        };

    } // namespace Controls
} // namespace QW
