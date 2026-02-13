#include "QWWindow.h"
#include "QWControls/Containers/Panel.h"
#include "QKEventTypes.h"
#include <cstring>

namespace QW
{

    Window::Window(const char *title, Rect bounds)
        : m_windowId(0),
          m_bounds(bounds),
          m_flags(WindowFlags::Default),
          m_root(nullptr),
          m_surfaceBackend(),
          m_styleRenderer(),
          m_painter(),
          m_surfacePixels(),
          m_bufferWidth(0),
          m_bufferHeight(0),
          m_bufferPitchBytes(0)
    {
        std::strncpy(m_title, title ? title : "", sizeof(m_title) - 1);
        m_title[sizeof(m_title) - 1] = '\0';

        // Create root panel
        m_root = new Controls::Panel();
        m_root->setWindow(this);
        m_root->setBounds(Rect{0, 0, bounds.width, bounds.height});

        // Connect backend to style renderer
        m_styleRenderer.setBackend(&m_surfaceBackend);

        ensureSurface(bounds.width, bounds.height);
    }

    Window::~Window()
    {
        delete m_root;
        m_root = nullptr;
    }

    uint32_t Window::windowId() const { return m_windowId; }
    void Window::setWindowId(uint32_t id) { m_windowId = id; }

    const char *Window::title() const { return m_title; }

    void Window::setTitle(const char *title)
    {
        std::strncpy(m_title, title ? title : "", sizeof(m_title) - 1);
        m_title[sizeof(m_title) - 1] = '\0';
    }

    Rect Window::bounds() const { return m_bounds; }

    void Window::setBounds(const Rect &bounds)
    {
        m_bounds = bounds;
        onResize(bounds.width, bounds.height);
    }

    bool Window::isVisible() const
    {
        return (m_flags & WindowFlags::Visible) != 0;
    }

    void Window::setVisible(bool visible)
    {
        if (visible)
            m_flags |= WindowFlags::Visible;
        else
            m_flags &= ~WindowFlags::Visible;
    }

    uint32_t Window::flags() const
    {
        return m_flags;
    }

    void Window::setFlags(uint32_t flags)
    {
        m_flags = flags;
    }

    Controls::Panel *Window::root() const { return m_root; }

    void Window::setStyleSnapshot(const StyleSnapshot *snapshot)
    {
        m_styleRenderer.setStyleSnapshot(snapshot);
        invalidate();
    }

    StyleRenderer *Window::styleRenderer() { return &m_styleRenderer; }

    const QC::u32 *Window::buffer() const
    {
        return m_surfacePixels.empty() ? nullptr : m_surfacePixels.data();
    }

    QC::u32 Window::bufferWidth() const
    {
        return m_bufferWidth;
    }

    QC::u32 Window::bufferHeight() const
    {
        return m_bufferHeight;
    }

    QC::u32 Window::bufferPitchBytes() const
    {
        return m_bufferPitchBytes;
    }

    void Window::invalidate()
    {
        paint();
    }

    void Window::invalidateRect(const Rect &)
    {
        paint();
    }

    void Window::paint()
    {
        if (!isVisible())
            return;

        if (!ensureSurface(m_bounds.width, m_bounds.height))
            return;

        FrameContext frameCtx{};
        frameCtx.surfaceBounds = Rect{0, 0, m_bounds.width, m_bounds.height};
        frameCtx.painter = &m_painter;

        if (!m_styleRenderer.beginFrame(frameCtx))
            return;

        WindowPaintArgs chromeArgs{};
        chromeArgs.bounds = frameCtx.surfaceBounds;
        chromeArgs.title = m_title;
        chromeArgs.active = true; // TODO: hook into WindowManager focus state
        chromeArgs.focused = true;

        m_styleRenderer.drawWindowChrome(chromeArgs);

        Controls::PaintContext controlCtx{};
        controlCtx.window = this;
        controlCtx.styleRenderer = &m_styleRenderer;
        controlCtx.painter = &m_painter;

        if (m_root)
            m_root->paint(controlCtx);

        m_styleRenderer.endFrame();

        onPaint();
    }

    void Window::onPaint()
    {
        // Platform compositor will blit m_surfaceBackend output
    }

    void Window::onResize(uint32_t w, uint32_t h)
    {
        ensureSurface(w, h);
        if (m_root)
            m_root->setBounds(Rect{0, 0, (int)w, (int)h});
        invalidate();
    }

    bool Window::onEvent(const QK::Event::Event &e)
    {
        if (!m_root)
            return false;

        return m_root->onEvent(e);
    }

    bool Window::ensureSurface(QC::u32 width, QC::u32 height)
    {
        if (width == 0 || height == 0)
        {
            m_surfacePixels.resize(0);
            m_bufferWidth = 0;
            m_bufferHeight = 0;
            m_bufferPitchBytes = 0;
            m_painter.setSurface(nullptr, 0, 0, 0);
            m_surfaceBackend.setSurface(nullptr, nullptr, 0, 0);
            return false;
        }

        const bool sizeChanged = (width != m_bufferWidth) || (height != m_bufferHeight) || m_surfacePixels.empty();
        if (sizeChanged)
        {
            const QC::usize pixelCount = static_cast<QC::usize>(width) * static_cast<QC::usize>(height);
            m_surfacePixels.resize(pixelCount);
            m_bufferWidth = width;
            m_bufferHeight = height;
            m_bufferPitchBytes = width * sizeof(QC::u32);
        }

        if (m_surfacePixels.empty())
            return false;

        QC::u32 *pixelData = m_surfacePixels.data();
        m_painter.setSurface(pixelData, m_bufferWidth, m_bufferHeight, m_bufferWidth);
        m_surfaceBackend.setSurface(&m_painter,
                                    pixelData,
                                    m_bufferWidth,
                                    m_bufferHeight,
                                    m_bufferPitchBytes);
        return true;
    }

} // namespace QW