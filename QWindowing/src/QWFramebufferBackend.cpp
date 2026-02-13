// QWindowing FramebufferBackend implementation

#include "QWFramebufferBackend.h"

#include <algorithm>

namespace QW
{
    namespace
    {
        inline QC::Color lerpColor(const QC::Color &from, const QC::Color &to, QC::f32 t)
        {
            return QC::Color::lerp(from, to, t);
        }
    } // namespace

    FramebufferBackend::FramebufferBackend(Framebuffer *framebuffer)
        : m_framebuffer(framebuffer)
    {
        m_caps.supportsRoundedRect = false;
        m_caps.supportsShadows = true;
        m_caps.supportsAlpha = true;
        updateTarget();
    }

    void FramebufferBackend::setFramebuffer(Framebuffer *framebuffer)
    {
        m_framebuffer = framebuffer;
        updateTarget();
    }

    bool FramebufferBackend::beginFrame()
    {
        return updateTarget();
    }

    void FramebufferBackend::endFrame()
    {
        if (m_framebuffer)
        {
            m_framebuffer->swap();
        }
    }

    void FramebufferBackend::clear(QC::Color color)
    {
        if (!updateTarget())
            return;

        m_renderer.clear(color);
    }

    void FramebufferBackend::drawRect(const QC::Rect &rect,
                                      QC::Color fill,
                                      QC::Color stroke,
                                      QC::u32 strokeWidth)
    {
        if (!updateTarget())
            return;

        if (rect.width == 0 || rect.height == 0)
            return;

        if (fill.a > 0)
        {
            m_renderer.fillRect(rect, fill);
        }

        if (strokeWidth == 0 || stroke.a == 0)
            return;

        QC::Rect outline = rect;
        for (QC::u32 i = 0; i < strokeWidth; ++i)
        {
            if (outline.width == 0 || outline.height == 0)
                break;

            m_renderer.drawRect(outline, stroke);

            if (outline.width < 2 || outline.height < 2)
                break;

            outline.x += 1;
            outline.y += 1;
            outline.width -= 2;
            outline.height -= 2;
        }
    }

    void FramebufferBackend::drawGradient(const QC::Rect &rect,
                                          QC::Color from,
                                          QC::Color to,
                                          QG::GradientDirection direction)
    {
        if (!updateTarget())
            return;

        if (rect.width == 0 || rect.height == 0)
            return;

        QC::Rect clipped;
        if (!clipRect(rect, clipped))
            return;

        if (direction == QG::GradientDirection::Vertical)
        {
            QC::f32 denom = rect.height > 1 ? static_cast<QC::f32>(rect.height - 1) : 1.0f;
            for (QC::i32 row = 0; row < static_cast<QC::i32>(clipped.height); ++row)
            {
                QC::i32 globalY = clipped.y + row;
                QC::i32 relative = globalY - rect.y;
                QC::f32 t = denom == 0.0f ? 0.0f : static_cast<QC::f32>(relative) / denom;
                QC::Color color = lerpColor(from, to, t);
                m_renderer.drawHLine(clipped.x, globalY, clipped.width, color);
            }
        }
        else
        {
            QC::f32 denom = rect.width > 1 ? static_cast<QC::f32>(rect.width - 1) : 1.0f;
            for (QC::i32 col = 0; col < static_cast<QC::i32>(clipped.width); ++col)
            {
                QC::i32 globalX = clipped.x + col;
                QC::i32 relative = globalX - rect.x;
                QC::f32 t = denom == 0.0f ? 0.0f : static_cast<QC::f32>(relative) / denom;
                QC::Color color = lerpColor(from, to, t);
                m_renderer.drawVLine(globalX, clipped.y, clipped.height, color);
            }
        }
    }

    void FramebufferBackend::drawRoundedRect(const QC::Rect &rect,
                                             QC::u32 radius,
                                             QC::Color fill,
                                             QC::Color stroke,
                                             QC::u32 strokeWidth)
    {
        (void)radius; // TODO: add rounded corner rasterization
        drawRect(rect, fill, stroke, strokeWidth);
    }

    void FramebufferBackend::drawShadow(const QC::Rect &rect,
                                        QC::Point offset,
                                        QC::i32 blurRadius,
                                        QC::Color color,
                                        QC::u8 opacity)
    {
        (void)blurRadius; // Future blur support
        if (!updateTarget())
            return;

        if (opacity == 0)
            return;

        QC::Rect shadow = rect;
        shadow.x += offset.x;
        shadow.y += offset.y;
        fillRectAlpha(shadow, color.withAlpha(opacity));
    }

    void FramebufferBackend::blit(const QC::Rect &rect,
                                  const QC::u32 *pixels,
                                  QC::u32 stride,
                                  bool useAlpha)
    {
        if (!updateTarget() || !pixels)
            return;

        if (rect.width == 0 || rect.height == 0)
            return;

        QC::u32 stridePixels = stride == 0 ? rect.width : stride;
        QC::u32 strideBytes = stridePixels * sizeof(QC::u32);

        if (useAlpha)
        {
            m_renderer.blitAlpha(rect.x, rect.y, pixels, rect.width, rect.height, strideBytes);
        }
        else
        {
            m_renderer.blit(rect.x, rect.y, pixels, rect.width, rect.height, strideBytes);
        }
    }

    bool FramebufferBackend::updateTarget()
    {
        if (!m_framebuffer)
        {
            m_target = TargetDesc{};
            return false;
        }

        void *buffer = m_framebuffer->backBuffer();
        if (!buffer)
            buffer = m_framebuffer->buffer();
        if (!buffer)
        {
            m_target = TargetDesc{};
            return false;
        }

        m_target.width = m_framebuffer->width();
        m_target.height = m_framebuffer->height();
        m_target.pitch = m_framebuffer->pitch();
        m_target.format = convertFormat(m_framebuffer->format());
        m_target.pixels = static_cast<QC::u8 *>(buffer);

        if (m_target.format != QG::PixelFormat::ARGB8888 &&
            m_target.format != QG::PixelFormat::ABGR8888)
        {
            m_target.pixels = nullptr;
            return false;
        }

        m_renderer.setTarget(static_cast<QC::u32 *>(buffer),
                             m_target.width,
                             m_target.height,
                             m_target.pitch);
        return true;
    }

    bool FramebufferBackend::clipRect(const QC::Rect &rect, QC::Rect &clipped) const
    {
        if (m_target.width == 0 || m_target.height == 0)
            return false;

        QC::i32 x1 = std::max(rect.x, 0);
        QC::i32 y1 = std::max(rect.y, 0);
        QC::i32 x2 = std::min(rect.x + static_cast<QC::i32>(rect.width), static_cast<QC::i32>(m_target.width));
        QC::i32 y2 = std::min(rect.y + static_cast<QC::i32>(rect.height), static_cast<QC::i32>(m_target.height));

        if (x2 <= x1 || y2 <= y1)
            return false;

        clipped.x = x1;
        clipped.y = y1;
        clipped.width = static_cast<QC::u32>(x2 - x1);
        clipped.height = static_cast<QC::u32>(y2 - y1);
        return true;
    }

    void FramebufferBackend::fillRectAlpha(const QC::Rect &rect, QC::Color color)
    {
        if (color.a == 0 || !m_target.pixels)
            return;

        QC::Rect clipped;
        if (!clipRect(rect, clipped))
            return;

        for (QC::i32 row = 0; row < static_cast<QC::i32>(clipped.height); ++row)
        {
            QC::u32 *dstRow = reinterpret_cast<QC::u32 *>(m_target.pixels + (clipped.y + row) * m_target.pitch);
            for (QC::i32 col = 0; col < static_cast<QC::i32>(clipped.width); ++col)
            {
                QC::u32 &dstPixel = dstRow[clipped.x + col];
                QC::Color dstColor(dstPixel);
                dstPixel = color.blend(dstColor).value;
            }
        }
    }

    QG::PixelFormat FramebufferBackend::convertFormat(PixelFormat format)
    {
        switch (format)
        {
        case PixelFormat::RGB565:
            return QG::PixelFormat::RGB565;
        case PixelFormat::BGR565:
            return QG::PixelFormat::BGR565;
        case PixelFormat::RGB888:
            return QG::PixelFormat::RGB888;
        case PixelFormat::BGR888:
            return QG::PixelFormat::BGR888;
        case PixelFormat::ABGR8888:
            return QG::PixelFormat::ABGR8888;
        case PixelFormat::ARGB8888:
        default:
            return QG::PixelFormat::ARGB8888;
        }
    }

} // namespace QW
