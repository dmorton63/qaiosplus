// QWindowing Compositor - Window compositing implementation
// Namespace: QW

#include "QWCompositor.h"
#include "QWWindow.h"
#include "QWFramebuffer.h"
#include "QWRenderer.h"
#include "QKMemHeap.h"
#include "QCMemUtil.h"
#include "QCLogger.h"

#include "QWPresentBackend.h"
#include "QWFramebufferPresentBackend.h"
#include "QWVmwareSVGAPresentBackend.h"

#include "QDrvVmwareSVGA.h"

#include "QG/CameraRH.h"

namespace QW
{

    namespace
    {
        // Off by default: set to 1 when you want to visually validate camera/projection math.
        static constexpr bool QAIOS_DEBUG_CAMERA_OVERLAY = false;
    }

    Compositor::Compositor(Framebuffer *fb)
        : m_framebuffer(fb),
          m_renderer(nullptr),
          m_presentBackend(nullptr),
          m_effects(0),
          m_wallpaper(nullptr),
          m_wallpaperWidth(0),
          m_wallpaperHeight(0),
          m_cursorPixels(nullptr),
          m_cursorWidth(0),
          m_cursorHeight(0),
          m_cursorHotspotX(0),
          m_cursorHotspotY(0),
          m_cursorBackground(nullptr),
          m_cursorBackX(0),
          m_cursorBackY(0),
          m_lastComposeTime(0),
          m_frameCount(0)
    {
    }

    Compositor::~Compositor()
    {
        if (m_presentBackend)
        {
            delete m_presentBackend;
            m_presentBackend = nullptr;
        }
        if (m_renderer)
        {
            delete m_renderer;
            m_renderer = nullptr;
        }
        if (m_wallpaper)
        {
            QK::Memory::Heap::instance().free(m_wallpaper);
            m_wallpaper = nullptr;
        }
        if (m_cursorPixels)
        {
            QK::Memory::Heap::instance().free(m_cursorPixels);
            m_cursorPixels = nullptr;
        }
        if (m_cursorBackground)
        {
            QK::Memory::Heap::instance().free(m_cursorBackground);
            m_cursorBackground = nullptr;
        }
    }

    void Compositor::initialize()
    {
        // Select presentation backend.
        // Default is software framebuffer swap; VMware SVGA backend is used when available.
        if (QDrv::VmwareSVGA::instance().initialize() && QDrv::VmwareSVGA::instance().isAvailable())
        {
            m_presentBackend = new VmwareSVGAPresentBackend();
        }
        else
        {
            m_presentBackend = new FramebufferPresentBackend();
        }
        if (m_presentBackend)
        {
            m_presentBackend->initialize(m_framebuffer);
        }

        m_renderer = new Renderer();
        if (m_framebuffer)
        {
            m_renderer->setTarget(
                static_cast<QC::u32 *>(m_framebuffer->backBuffer()),
                m_framebuffer->width(),
                m_framebuffer->height(),
                m_framebuffer->pitch());
        }

        // Create a simple default arrow cursor (12x16)
        static constexpr QC::u32 cursorWidth = 12;
        static constexpr QC::u32 cursorHeight = 16;
        static constexpr QC::u32 W = 0xFFFFFFFF; // White
        static constexpr QC::u32 B = 0xFF000000; // Black
        static constexpr QC::u32 T = 0x00000000; // Transparent

        static const QC::u32 defaultCursor[cursorHeight * cursorWidth] = {
            B,
            T,
            T,
            T,
            T,
            T,
            T,
            T,
            T,
            T,
            T,
            T,
            B,
            B,
            T,
            T,
            T,
            T,
            T,
            T,
            T,
            T,
            T,
            T,
            B,
            W,
            B,
            T,
            T,
            T,
            T,
            T,
            T,
            T,
            T,
            T,
            B,
            W,
            W,
            B,
            T,
            T,
            T,
            T,
            T,
            T,
            T,
            T,
            B,
            W,
            W,
            W,
            B,
            T,
            T,
            T,
            T,
            T,
            T,
            T,
            B,
            W,
            W,
            W,
            W,
            B,
            T,
            T,
            T,
            T,
            T,
            T,
            B,
            W,
            W,
            W,
            W,
            W,
            B,
            T,
            T,
            T,
            T,
            T,
            B,
            W,
            W,
            W,
            W,
            W,
            W,
            B,
            T,
            T,
            T,
            T,
            B,
            W,
            W,
            W,
            W,
            W,
            W,
            W,
            B,
            T,
            T,
            T,
            B,
            W,
            W,
            W,
            W,
            W,
            W,
            W,
            W,
            B,
            T,
            T,
            B,
            W,
            W,
            W,
            W,
            W,
            B,
            B,
            B,
            B,
            T,
            T,
            B,
            W,
            W,
            B,
            W,
            W,
            B,
            T,
            T,
            T,
            T,
            T,
            B,
            W,
            B,
            T,
            B,
            W,
            W,
            B,
            T,
            T,
            T,
            T,
            B,
            B,
            T,
            T,
            B,
            W,
            W,
            B,
            T,
            T,
            T,
            T,
            B,
            T,
            T,
            T,
            T,
            B,
            W,
            W,
            B,
            T,
            T,
            T,
            T,
            T,
            T,
            T,
            T,
            B,
            B,
            B,
            T,
            T,
            T,
            T,
        };

        setCursor(defaultCursor, cursorWidth, cursorHeight, 0, 0);

        const bool hwCursor = (m_presentBackend && m_presentBackend->hasHardwareCursor());
        QC_LOG_INFO("QWCompositor", "Hardware cursor: %s", hwCursor ? "ON" : "OFF");
    }

    void Compositor::compose()
    {
        if (!m_framebuffer || !m_renderer)
            return;

        const bool hasHwCursor = (m_presentBackend && m_presentBackend->hasHardwareCursor());

        // If nothing is dirty and we have a hardware cursor, skip recompositing/presenting.
        // Cursor movement is handled via cursor registers, so we don't need framebuffer updates.
        if (hasHwCursor && m_dirtyRegions.empty())
        {
            auto &wm = WindowManager::instance();
            const Point mousePos = wm.mousePosition();
            m_presentBackend->setCursorVisible(true);
            m_presentBackend->setCursorPosition(
                static_cast<QC::u16>(mousePos.x),
                static_cast<QC::u16>(mousePos.y));
            return;
        }

        // TODO: Get timestamp for performance tracking
        // m_lastComposeTime = ...

        // Draw desktop background
        drawDesktop();

        if (QAIOS_DEBUG_CAMERA_OVERLAY)
        {
            // Minimal integration demo: use the UI ortho camera to transform a simple box.
            // This is intentionally simple and self-contained.
            QG::UICameraOrthoRH cam;
            cam.width = m_framebuffer->width();
            cam.height = m_framebuffer->height();

            // A 64x64 box in pixel space.
            const QC::Vec3f p0{24.0f, 24.0f, 0.0f};
            const QC::Vec3f p1{24.0f + 64.0f, 24.0f, 0.0f};
            const QC::Vec3f p2{24.0f + 64.0f, 24.0f + 64.0f, 0.0f};
            const QC::Vec3f p3{24.0f, 24.0f + 64.0f, 0.0f};

            // Transform through view-proj and back to pixel space (since our renderer is pixel-based).
            // For now, this just exercises the math path; the visual is a simple box.
            (void)QC::transformPoint(cam.viewProj(), p0);
            (void)QC::transformPoint(cam.viewProj(), p1);
            (void)QC::transformPoint(cam.viewProj(), p2);
            (void)QC::transformPoint(cam.viewProj(), p3);

            // Draw directly in pixel space.
            m_renderer->drawRect(Rect{24, 24, 64, 64}, Color(255, 0, 255, 255));
        }

        // Compose all windows from bottom to top
        auto &wm = WindowManager::instance();
        for (QC::usize i = 0; i < wm.windowCount(); ++i)
        {
            Window *window = wm.windowAtIndex(i);
            if (window && window->isVisible())
            {
                composeWindow(window);
            }
        }

        // Draw cursor
        Point mousePos = wm.mousePosition();
        if (hasHwCursor)
        {
            m_presentBackend->setCursorVisible(true);
            m_presentBackend->setCursorPosition(
                static_cast<QC::u16>(mousePos.x),
                static_cast<QC::u16>(mousePos.y));
        }
        else
        {
            drawCursor(mousePos.x, mousePos.y);
        }

        // Present frame
        if (m_presentBackend)
        {
            // Present only dirty rectangles when supported.
            static constexpr QC::usize kMaxDirtyRects = 64;
            QC::Rect dirtyRects[kMaxDirtyRects];
            QC::usize dirtyCount = m_dirtyRegions.size();

            if (dirtyCount == 0)
            {
                // No changes recorded; keep legacy behavior for the software cursor path.
                // (With hardware cursor, we'd have returned above.)
                m_presentBackend->present();
            }
            else if (dirtyCount > kMaxDirtyRects)
            {
                // Too many rects; cheaper to do a full present.
                m_presentBackend->present();
            }
            else
            {
                for (QC::usize i = 0; i < dirtyCount; ++i)
                {
                    dirtyRects[i] = m_dirtyRegions[i].rect;
                }

                m_presentBackend->present(dirtyRects, dirtyCount);
            }
        }
        else
        {
            m_framebuffer->swap();
        }

        m_frameCount++;
        clearDirtyRegions();
    }

    void Compositor::composeWindow(Window *window)
    {
        if (!window || !m_renderer)
            return;

        // Draw window decorations
        if (window->flags() & WindowFlags::HasBorder)
        {
            drawWindowDecorations(window);
        }

        // Blit window content
        Rect bounds = window->bounds();
        if (window->buffer())
        {
            m_renderer->blit(
                bounds.x, bounds.y,
                window->buffer(),
                window->bufferWidth(),
                window->bufferHeight(),
                window->bufferPitchBytes());
        }
    }

    void Compositor::invalidate(const Rect &rect)
    {
        DirtyRegion region;
        region.rect = rect;
        region.merged = false;
        m_dirtyRegions.push_back(region);
    }

    void Compositor::invalidateAll()
    {
        if (m_framebuffer)
        {
            invalidate(Rect{0, 0, m_framebuffer->width(), m_framebuffer->height()});
        }
    }

    void Compositor::clearDirtyRegions()
    {
        m_dirtyRegions.clear();
    }

    void Compositor::setEffect(CompositionEffect effect, bool enabled)
    {
        QC::u32 bit = 1u << static_cast<QC::u32>(effect);
        if (enabled)
        {
            m_effects |= bit;
        }
        else
        {
            m_effects &= ~bit;
        }
    }

    bool Compositor::hasEffect(CompositionEffect effect) const
    {
        QC::u32 bit = 1u << static_cast<QC::u32>(effect);
        return (m_effects & bit) != 0;
    }

    void Compositor::drawWindowDecorations(Window *window)
    {
        if (!window || !m_renderer)
            return;

        Rect bounds = window->bounds();

        // Draw border
        if (window->flags() & WindowFlags::HasBorder)
        {
            drawBorder(window);
        }

        // Draw title bar
        if (window->flags() & WindowFlags::HasTitle)
        {
            drawTitleBar(window);
        }

        // Draw shadow
        if (hasEffect(CompositionEffect::Shadow))
        {
            drawShadow(window);
        }
    }

    void Compositor::drawTitleBar(Window *window)
    {
        (void)window;
        // TODO: Draw title bar with title text and buttons
    }

    void Compositor::drawBorder(Window *window)
    {
        if (!window || !m_renderer)
            return;

        Rect bounds = window->bounds();
        Color borderColor = Color::fromRGB(100, 100, 100);
        m_renderer->drawRect(bounds, borderColor);
    }

    void Compositor::drawShadow(Window *window)
    {
        (void)window;
        // TODO: Draw window shadow with transparency
    }

    void Compositor::setWallpaper(const QC::u32 *pixels, QC::u32 width, QC::u32 height)
    {
        if (m_wallpaper)
        {
            QK::Memory::Heap::instance().free(m_wallpaper);
            m_wallpaper = nullptr;
        }

        if (pixels && width > 0 && height > 0)
        {
            QC::usize size = width * height * sizeof(QC::u32);
            m_wallpaper = static_cast<QC::u32 *>(QK::Memory::Heap::instance().allocate(size));
            if (m_wallpaper)
            {
                memcpy(m_wallpaper, pixels, size);
                m_wallpaperWidth = width;
                m_wallpaperHeight = height;
            }
        }
    }

    void Compositor::drawDesktop()
    {
        if (!m_renderer)
            return;

        if (m_wallpaper)
        {
            m_renderer->blit(0, 0, m_wallpaper, m_wallpaperWidth, m_wallpaperHeight,
                             m_wallpaperWidth * sizeof(QC::u32));
        }
        else
        {
            // Default desktop background
            m_renderer->clear(Color::fromRGB(0, 128, 128));
        }
    }

    void Compositor::setCursor(const QC::u32 *pixels, QC::u32 width, QC::u32 height,
                               QC::i32 hotspotX, QC::i32 hotspotY)
    {
        if (m_cursorPixels)
        {
            QK::Memory::Heap::instance().free(m_cursorPixels);
            m_cursorPixels = nullptr;
        }
        if (m_cursorBackground)
        {
            QK::Memory::Heap::instance().free(m_cursorBackground);
            m_cursorBackground = nullptr;
        }

        if (pixels && width > 0 && height > 0)
        {
            QC::usize size = width * height * sizeof(QC::u32);
            m_cursorPixels = static_cast<QC::u32 *>(QK::Memory::Heap::instance().allocate(size));
            m_cursorBackground = static_cast<QC::u32 *>(QK::Memory::Heap::instance().allocate(size));
            if (m_cursorPixels)
            {
                memcpy(m_cursorPixels, pixels, size);
                m_cursorWidth = width;
                m_cursorHeight = height;
                m_cursorHotspotX = hotspotX;
                m_cursorHotspotY = hotspotY;

                if (m_presentBackend && m_presentBackend->hasHardwareCursor())
                {
                    QC::i32 safeHotspotX = hotspotX;
                    QC::i32 safeHotspotY = hotspotY;
                    if (safeHotspotX < 0)
                        safeHotspotX = 0;
                    if (safeHotspotY < 0)
                        safeHotspotY = 0;
                    if (safeHotspotX >= static_cast<QC::i32>(width))
                        safeHotspotX = static_cast<QC::i32>(width) - 1;
                    if (safeHotspotY >= static_cast<QC::i32>(height))
                        safeHotspotY = static_cast<QC::i32>(height) - 1;

                    m_presentBackend->setCursorImage(
                        m_cursorPixels,
                        static_cast<QC::u16>(width),
                        static_cast<QC::u16>(height),
                        static_cast<QC::u16>(safeHotspotX),
                        static_cast<QC::u16>(safeHotspotY));
                }
            }
        }
    }

    void Compositor::drawCursor(QC::i32 x, QC::i32 y)
    {
        if (!m_cursorPixels || !m_renderer)
            return;

        QC::i32 drawX = x - m_cursorHotspotX;
        QC::i32 drawY = y - m_cursorHotspotY;

        m_renderer->blitAlpha(drawX, drawY, m_cursorPixels,
                              m_cursorWidth, m_cursorHeight,
                              m_cursorWidth * sizeof(QC::u32));
    }

    void Compositor::saveCursorBackground(QC::i32 x, QC::i32 y)
    {
        (void)x;
        (void)y;
        // TODO: Save background under cursor for restoration
    }

    void Compositor::restoreCursorBackground()
    {
        // TODO: Restore saved background
    }

    void Compositor::mergeDirtyRegions()
    {
        // TODO: Merge overlapping dirty regions for efficiency
    }

} // namespace QW
