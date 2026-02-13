// QWindowing SurfaceBackend implementation

#include "QWSurfaceBackend.h"

#include <algorithm>

namespace QW
{

    SurfaceBackend::SurfaceBackend()
        : m_surface(nullptr)
    {
        m_caps.supportsRoundedRect = false;
        m_caps.supportsShadows = true;
        m_caps.supportsAlpha = true;
        m_target = TargetDesc{};
    }

    void SurfaceBackend::setSurface(QG::PainterSurface *surface,
                                    QC::u32 *pixels,
                                    QC::u32 width,
                                    QC::u32 height,
                                    QC::u32 pitchBytes)
    {
        m_surface = surface;
        if (!surface || !pixels || width == 0 || height == 0)
        {
            m_target = TargetDesc{};
            return;
        }

        m_target.width = width;
        m_target.height = height;
        m_target.pitch = pitchBytes ? pitchBytes : width * sizeof(QC::u32);
        m_target.pixels = reinterpret_cast<QC::u8 *>(pixels);
        m_target.format = QG::PixelFormat::ARGB8888;
    }

    bool SurfaceBackend::beginFrame()
    {
        return ensureSurface();
    }

    void SurfaceBackend::endFrame()
    {
        // No swap needed for software surfaces
    }

    void SurfaceBackend::clear(QC::Color color)
    {
        if (!ensureSurface())
            return;

        m_surface->clear(color);
    }

    void SurfaceBackend::drawRect(const QC::Rect &rect,
                                  QC::Color fill,
                                  QC::Color stroke,
                                  QC::u32 strokeWidth)
    {
        if (!ensureSurface())
            return;

        if (fill.a > 0 && rect.width > 0 && rect.height > 0)
        {
            m_surface->fillRect(rect, QG::Brush::solid(fill));
        }

        if (stroke.a > 0 && strokeWidth > 0)
        {
            QG::Pen pen(stroke);
            pen.setWidth(strokeWidth);
            m_surface->drawRect(rect, pen);
        }
    }

    void SurfaceBackend::drawGradient(const QC::Rect &rect,
                                      QC::Color from,
                                      QC::Color to,
                                      QG::GradientDirection direction)
    {
        if (!ensureSurface())
            return;

        if (direction == QG::GradientDirection::Vertical)
        {
            m_surface->fillGradientV(rect, from, to);
        }
        else
        {
            m_surface->fillGradientH(rect, from, to);
        }
    }

    void SurfaceBackend::drawRoundedRect(const QC::Rect &rect,
                                         QC::u32 radius,
                                         QC::Color fill,
                                         QC::Color stroke,
                                         QC::u32 strokeWidth)
    {
        (void)radius;
        drawRect(rect, fill, stroke, strokeWidth);
    }

    void SurfaceBackend::drawShadow(const QC::Rect &rect,
                                    QC::Point offset,
                                    QC::i32 blurRadius,
                                    QC::Color color,
                                    QC::u8 opacity)
    {
        (void)blurRadius; // Blur not supported yet on surface backend
        if (!ensureSurface() || opacity == 0)
            return;

        QC::Rect shadowRect = rect;
        shadowRect.x += offset.x;
        shadowRect.y += offset.y;
        fillRectAlpha(shadowRect, color.withAlpha(opacity));
    }

    void SurfaceBackend::blit(const QC::Rect &rect,
                              const QC::u32 *pixels,
                              QC::u32 stride,
                              bool useAlpha)
    {
        if (!ensureSurface() || !pixels)
            return;

        if (useAlpha)
        {
            m_surface->blitAlpha(rect.x, rect.y, pixels, rect.width, rect.height, stride);
        }
        else
        {
            m_surface->blit(rect.x, rect.y, pixels, rect.width, rect.height, stride);
        }
    }

    bool SurfaceBackend::clipRect(const QC::Rect &rect, QC::Rect &clipped) const
    {
        if (!ensureSurface())
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

    void SurfaceBackend::fillRectAlpha(const QC::Rect &rect, QC::Color color)
    {
        if (!ensureSurface() || color.a == 0)
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

} // namespace QW
