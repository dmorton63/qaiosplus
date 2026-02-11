#include "QG/PainterSurface.h"

#include <algorithm>

namespace QG
{

    namespace
    {
        constexpr QC::i32 kGlyphW = 6; // 5px glyph + 1px spacing
        constexpr QC::i32 kGlyphH = 8; // 7px glyph + 1px spacing

        struct Glyph5x7
        {
            QC::u8 rows[7];
        };

        inline Glyph5x7 glyphForChar(char c)
        {
            if (c >= 'a' && c <= 'z')
            {
                c = static_cast<char>(c - 'a' + 'A');
            }

            switch (c)
            {
            case ' ':
                return {{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}};
            case '!':
                return {{0x04, 0x04, 0x04, 0x04, 0x04, 0x00, 0x04}};
            case '"':
                return {{0x0A, 0x0A, 0x0A, 0x00, 0x00, 0x00, 0x00}};
            case '\'':
                return {{0x04, 0x04, 0x02, 0x00, 0x00, 0x00, 0x00}};
            case ',':
                return {{0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x02}};
            case '.':
                return {{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04}};
            case ':':
                return {{0x00, 0x04, 0x00, 0x00, 0x04, 0x00, 0x00}};
            case '-':
                return {{0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00}};
            case '+':
                return {{0x00, 0x04, 0x04, 0x1F, 0x04, 0x04, 0x00}};
            case '/':
                return {{0x10, 0x08, 0x04, 0x02, 0x01, 0x00, 0x00}};
            case '_':
                return {{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1F}};
            case '(':
                return {{0x08, 0x04, 0x02, 0x02, 0x02, 0x04, 0x08}};
            case ')':
                return {{0x02, 0x04, 0x08, 0x08, 0x08, 0x04, 0x02}};
            case '[':
                return {{0x0E, 0x02, 0x02, 0x02, 0x02, 0x02, 0x0E}};
            case ']':
                return {{0x0E, 0x08, 0x08, 0x08, 0x08, 0x08, 0x0E}};
            case '=':
                return {{0x00, 0x00, 0x1F, 0x00, 0x1F, 0x00, 0x00}};
            case '?':
                return {{0x0E, 0x11, 0x10, 0x08, 0x04, 0x00, 0x04}};

            case '0':
                return {{0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E}};
            case '1':
                return {{0x04, 0x06, 0x04, 0x04, 0x04, 0x04, 0x0E}};
            case '2':
                return {{0x0E, 0x11, 0x10, 0x08, 0x04, 0x02, 0x1F}};
            case '3':
                return {{0x1F, 0x08, 0x04, 0x08, 0x10, 0x11, 0x0E}};
            case '4':
                return {{0x08, 0x0C, 0x0A, 0x09, 0x1F, 0x08, 0x08}};
            case '5':
                return {{0x1F, 0x01, 0x0F, 0x10, 0x10, 0x11, 0x0E}};
            case '6':
                return {{0x0C, 0x02, 0x01, 0x0F, 0x11, 0x11, 0x0E}};
            case '7':
                return {{0x1F, 0x10, 0x08, 0x04, 0x02, 0x02, 0x02}};
            case '8':
                return {{0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E}};
            case '9':
                return {{0x0E, 0x11, 0x11, 0x1E, 0x10, 0x08, 0x06}};

            case 'A':
                return {{0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}};
            case 'B':
                return {{0x0F, 0x11, 0x11, 0x0F, 0x11, 0x11, 0x0F}};
            case 'C':
                return {{0x0E, 0x11, 0x01, 0x01, 0x01, 0x11, 0x0E}};
            case 'D':
                return {{0x07, 0x09, 0x11, 0x11, 0x11, 0x09, 0x07}};
            case 'E':
                return {{0x1F, 0x01, 0x01, 0x0F, 0x01, 0x01, 0x1F}};
            case 'F':
                return {{0x1F, 0x01, 0x01, 0x0F, 0x01, 0x01, 0x01}};
            case 'G':
                return {{0x0E, 0x11, 0x01, 0x1D, 0x11, 0x11, 0x0E}};
            case 'H':
                return {{0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}};
            case 'I':
                return {{0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E}};
            case 'J':
                return {{0x1C, 0x08, 0x08, 0x08, 0x08, 0x09, 0x06}};
            case 'K':
                return {{0x11, 0x09, 0x05, 0x03, 0x05, 0x09, 0x11}};
            case 'L':
                return {{0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x1F}};
            case 'M':
                return {{0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11}};
            case 'N':
                return {{0x11, 0x11, 0x13, 0x15, 0x19, 0x11, 0x11}};
            case 'O':
                return {{0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}};
            case 'P':
                return {{0x0F, 0x11, 0x11, 0x0F, 0x01, 0x01, 0x01}};
            case 'Q':
                return {{0x0E, 0x11, 0x11, 0x11, 0x15, 0x09, 0x16}};
            case 'R':
                return {{0x0F, 0x11, 0x11, 0x0F, 0x05, 0x09, 0x11}};
            case 'S':
                return {{0x1E, 0x01, 0x01, 0x0E, 0x10, 0x10, 0x0F}};
            case 'T':
                return {{0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04}};
            case 'U':
                return {{0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}};
            case 'V':
                return {{0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04}};
            case 'W':
                return {{0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0x0A}};
            case 'X':
                return {{0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11}};
            case 'Y':
                return {{0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04}};
            case 'Z':
                return {{0x1F, 0x10, 0x08, 0x04, 0x02, 0x01, 0x1F}};

            default:
                return {{0x0E, 0x11, 0x10, 0x08, 0x04, 0x00, 0x04}}; // '?'
            }
        }

        inline QC::Size measureTextMono5x7(const char *text)
        {
            if (!text)
                return QC::Size(0, 0);

            QC::i32 maxLineChars = 0;
            QC::i32 lineChars = 0;
            QC::i32 lines = 1;

            for (const char *p = text; *p; ++p)
            {
                if (*p == '\n')
                {
                    if (lineChars > maxLineChars)
                        maxLineChars = lineChars;
                    lineChars = 0;
                    ++lines;
                    continue;
                }
                ++lineChars;
            }

            if (lineChars > maxLineChars)
                maxLineChars = lineChars;

            return QC::Size(maxLineChars * kGlyphW, lines * kGlyphH);
        }

        inline bool hasFlag(TextAlign value, TextAlign flag)
        {
            return (static_cast<QC::u8>(value) & static_cast<QC::u8>(flag)) != 0;
        }
    } // namespace

    namespace
    {
        inline QC::u8 gradientChannel(QC::u8 start, QC::u8 end, QC::i32 step, QC::i32 segments)
        {
            if (segments <= 0)
                return start;

            QC::i32 diff = static_cast<QC::i32>(end) - static_cast<QC::i32>(start);
            QC::i64 scaled = static_cast<QC::i64>(diff) * static_cast<QC::i64>(step);

            QC::i64 roundTerm = segments / 2;
            if (scaled < 0)
                scaled -= roundTerm;
            else
                scaled += roundTerm;

            QC::i32 delta = static_cast<QC::i32>(scaled / segments);
            QC::i32 value = static_cast<QC::i32>(start) + delta;

            if (value < 0)
                value = 0;
            else if (value > 255)
                value = 255;

            return static_cast<QC::u8>(value);
        }

        inline QC::Color gradientColor(const QC::Color &a, const QC::Color &b, QC::i32 step, QC::i32 segments)
        {
            if (segments <= 0)
                return a;

            return QC::Color(
                gradientChannel(a.r, b.r, step, segments),
                gradientChannel(a.g, b.g, step, segments),
                gradientChannel(a.b, b.b, step, segments),
                gradientChannel(a.a, b.a, step, segments));
        }
    } // namespace

    PainterSurface::PainterSurface(QC::u32 *pixels,
                                   QC::u32 width,
                                   QC::u32 height,
                                   QC::u32 pitch)
        : m_pixels(pixels),
          m_width(width),
          m_height(height),
          m_pitch(pitch ? pitch : width),
          m_clip(0, 0, width, height),
          m_hasClip(false),
          m_origin(0, 0)
    {
    }

    void PainterSurface::setSurface(QC::u32 *pixels,
                                    QC::u32 width,
                                    QC::u32 height,
                                    QC::u32 pitch)
    {
        m_pixels = pixels;
        m_width = width;
        m_height = height;
        m_pitch = pitch ? pitch : width;
        m_clip = QC::Rect(0, 0, width, height);
        m_hasClip = false;
    }

    QC::Size PainterSurface::size() const
    {
        return QC::Size(m_width, m_height);
    }

    QC::Rect PainterSurface::bounds() const
    {
        return QC::Rect(0, 0, m_width, m_height);
    }

    void PainterSurface::setClipRect(const QC::Rect &rect)
    {
        m_clip = rect;
        m_hasClip = true;
    }

    void PainterSurface::clearClipRect()
    {
        m_hasClip = false;
    }

    QC::Rect PainterSurface::clipRect() const
    {
        return m_hasClip ? m_clip : bounds();
    }

    void PainterSurface::setOrigin(QC::i32 x, QC::i32 y)
    {
        m_origin = QC::Point(x, y);
    }

    QC::Point PainterSurface::origin() const
    {
        return m_origin;
    }

    void PainterSurface::translate(QC::i32 dx, QC::i32 dy)
    {
        m_origin.x += dx;
        m_origin.y += dy;
    }

    void PainterSurface::setPixel(QC::i32 x, QC::i32 y, QC::Color color)
    {
        if (!m_pixels || m_pitch == 0)
            return;

        x += m_origin.x;
        y += m_origin.y;

        if (x < 0 || y < 0)
            return;
        if (static_cast<QC::u32>(x) >= m_width ||
            static_cast<QC::u32>(y) >= m_height)
            return;

        if (!inClip(x, y))
            return;

        m_pixels[y * m_pitch + x] = color.value;
    }

    QC::Color PainterSurface::pixel(QC::i32 x, QC::i32 y) const
    {
        if (!m_pixels || m_pitch == 0)
            return QC::Color();

        x += m_origin.x;
        y += m_origin.y;

        if (x < 0 || y < 0)
            return QC::Color();
        if (static_cast<QC::u32>(x) >= m_width ||
            static_cast<QC::u32>(y) >= m_height)
            return QC::Color();

        return QC::Color(m_pixels[y * m_pitch + x]);
    }

    void PainterSurface::drawLine(QC::i32 x1, QC::i32 y1, QC::i32 x2, QC::i32 y2, const Pen &pen)
    {
        if (pen.isNull())
            return;

        QC::Color color = pen.color();

        QC::i32 dx = x2 - x1;
        QC::i32 dy = y2 - y1;
        QC::i32 sx = (dx > 0) ? 1 : -1;
        QC::i32 sy = (dy > 0) ? 1 : -1;
        dx = (dx > 0) ? dx : -dx;
        dy = (dy > 0) ? dy : -dy;

        QC::i32 err = dx - dy;
        QC::i32 x = x1;
        QC::i32 y = y1;

        while (true)
        {
            setPixel(x, y, color);
            if (x == x2 && y == y2)
                break;

            QC::i32 e2 = err << 1;
            if (e2 > -dy)
            {
                err -= dy;
                x += sx;
            }
            if (e2 < dx)
            {
                err += dx;
                y += sy;
            }
        }
    }

    void PainterSurface::drawHLine(QC::i32 x, QC::i32 y, QC::u32 length, QC::Color color)
    {
        if (!m_pixels || length == 0 || m_pitch == 0)
            return;

        x += m_origin.x;
        y += m_origin.y;

        if (y < 0 || static_cast<QC::u32>(y) >= m_height)
            return;

        QC::i32 x1 = x;
        QC::i32 x2 = x + static_cast<QC::i32>(length);

        if (x2 <= 0 || x1 >= static_cast<QC::i32>(m_width))
            return;

        if (x1 < 0)
            x1 = 0;
        if (x2 > static_cast<QC::i32>(m_width))
            x2 = static_cast<QC::i32>(m_width);

        if (m_hasClip)
        {
            QC::i32 clipTop = m_clip.y;
            QC::i32 clipBottom = m_clip.y + static_cast<QC::i32>(m_clip.height);
            if (y < clipTop || y >= clipBottom)
                return;

            QC::i32 clipLeft = m_clip.x;
            QC::i32 clipRight = m_clip.x + static_cast<QC::i32>(m_clip.width);
            if (x1 < clipLeft)
                x1 = clipLeft;
            if (x2 > clipRight)
                x2 = clipRight;
        }

        if (x1 >= x2)
            return;

        QC::u32 *row = m_pixels + y * m_pitch;
        for (QC::i32 px = x1; px < x2; ++px)
        {
            row[px] = color.value;
        }
    }

    void PainterSurface::drawVLine(QC::i32 x, QC::i32 y, QC::u32 length, QC::Color color)
    {
        if (!m_pixels || length == 0 || m_pitch == 0)
            return;

        x += m_origin.x;
        y += m_origin.y;

        if (x < 0 || static_cast<QC::u32>(x) >= m_width)
            return;

        QC::i32 y1 = y;
        QC::i32 y2 = y + static_cast<QC::i32>(length);

        if (y2 <= 0 || y1 >= static_cast<QC::i32>(m_height))
            return;

        if (y1 < 0)
            y1 = 0;
        if (y2 > static_cast<QC::i32>(m_height))
            y2 = static_cast<QC::i32>(m_height);

        if (m_hasClip)
        {
            QC::i32 clipLeft = m_clip.x;
            QC::i32 clipRight = m_clip.x + static_cast<QC::i32>(m_clip.width);
            if (x < clipLeft || x >= clipRight)
                return;

            QC::i32 clipTop = m_clip.y;
            QC::i32 clipBottom = m_clip.y + static_cast<QC::i32>(m_clip.height);
            if (y1 < clipTop)
                y1 = clipTop;
            if (y2 > clipBottom)
                y2 = clipBottom;
        }

        if (y1 >= y2)
            return;

        for (QC::i32 py = y1; py < y2; ++py)
        {
            m_pixels[py * m_pitch + x] = color.value;
        }
    }

    void PainterSurface::fillRect(const QC::Rect &rect, const Brush &brush)
    {
        if (!m_pixels || m_pitch == 0 || brush.isNull())
            return;

        QC::Rect r = rect.offset(m_origin.x, m_origin.y);

        QC::i32 x1 = r.x < 0 ? 0 : r.x;
        QC::i32 y1 = r.y < 0 ? 0 : r.y;
        QC::i32 x2 = r.right();
        QC::i32 y2 = r.bottom();

        if (x2 > static_cast<QC::i32>(m_width))
            x2 = static_cast<QC::i32>(m_width);
        if (y2 > static_cast<QC::i32>(m_height))
            y2 = static_cast<QC::i32>(m_height);

        if (m_hasClip)
        {
            QC::i32 clipLeft = m_clip.x;
            QC::i32 clipTop = m_clip.y;
            QC::i32 clipRight = m_clip.x + static_cast<QC::i32>(m_clip.width);
            QC::i32 clipBottom = m_clip.y + static_cast<QC::i32>(m_clip.height);

            if (x1 < clipLeft)
                x1 = clipLeft;
            if (y1 < clipTop)
                y1 = clipTop;
            if (x2 > clipRight)
                x2 = clipRight;
            if (y2 > clipBottom)
                y2 = clipBottom;
        }

        if (x1 >= x2 || y1 >= y2)
            return;

        if (brush.style() == BrushStyle::Solid)
        {
            QC::Color color = brush.color();
            for (QC::i32 yy = y1; yy < y2; ++yy)
            {
                QC::u32 *row = m_pixels + yy * m_pitch;
                for (QC::i32 xx = x1; xx < x2; ++xx)
                {
                    row[xx] = color.value;
                }
            }
            return;
        }

        if (brush.style() == BrushStyle::LinearGradientV)
        {
            fillGradientV(QC::Rect(x1, y1,
                                   static_cast<QC::u32>(x2 - x1),
                                   static_cast<QC::u32>(y2 - y1)),
                          brush.color(), brush.colorEnd());
            return;
        }

        if (brush.style() == BrushStyle::LinearGradientH)
        {
            fillGradientH(QC::Rect(x1, y1,
                                   static_cast<QC::u32>(x2 - x1),
                                   static_cast<QC::u32>(y2 - y1)),
                          brush.color(), brush.colorEnd());
        }
    }

    void PainterSurface::drawRect(const QC::Rect &rect, const Pen &pen)
    {
        if (pen.isNull())
            return;

        QC::Color color = pen.color();
        QC::u32 width = pen.width();

        for (QC::u32 i = 0; i < width; ++i)
        {
            QC::Rect r = rect.inset(static_cast<QC::i32>(i));
            if (r.isEmpty())
                break;

            drawHLine(r.x, r.y, r.width, color);
            drawHLine(r.x, r.bottom() - 1, r.width, color);
            QC::u32 vertical = r.height > 2 ? r.height - 2 : 0;
            drawVLine(r.x, r.y + 1, vertical, color);
            drawVLine(r.right() - 1, r.y + 1, vertical, color);
        }
    }

    void PainterSurface::drawRaisedBorder(const QC::Rect &rect, QC::Color light, QC::Color dark, QC::u32 width)
    {
        for (QC::u32 i = 0; i < width; ++i)
        {
            QC::Rect r = rect.inset(static_cast<QC::i32>(i));
            if (r.isEmpty())
                break;

            drawHLine(r.x, r.y, r.width, light);
            drawVLine(r.x, r.y + 1, r.height > 1 ? r.height - 1 : 0, light);
            drawHLine(r.x, r.bottom() - 1, r.width, dark);
            drawVLine(r.right() - 1, r.y, r.height > 1 ? r.height - 1 : 0, dark);
        }
    }

    void PainterSurface::drawSunkenBorder(const QC::Rect &rect, QC::Color light, QC::Color dark, QC::u32 width)
    {
        for (QC::u32 i = 0; i < width; ++i)
        {
            QC::Rect r = rect.inset(static_cast<QC::i32>(i));
            if (r.isEmpty())
                break;

            drawHLine(r.x, r.y, r.width, dark);
            drawVLine(r.x, r.y + 1, r.height > 1 ? r.height - 1 : 0, dark);
            drawHLine(r.x, r.bottom() - 1, r.width, light);
            drawVLine(r.right() - 1, r.y, r.height > 1 ? r.height - 1 : 0, light);
        }
    }

    void PainterSurface::drawEtchedBorder(const QC::Rect &rect, QC::Color light, QC::Color dark)
    {
        drawSunkenBorder(rect, light, dark, 1);
        drawRaisedBorder(rect.inset(1), light, dark, 1);
    }

    void PainterSurface::fillGradientV(const QC::Rect &rect, QC::Color top, QC::Color bottom)
    {
        if (!m_pixels || m_pitch == 0 || rect.isEmpty())
            return;

        QC::Rect r = rect.offset(m_origin.x, m_origin.y);
        QC::i32 x1 = r.x < 0 ? 0 : r.x;
        QC::i32 y1 = r.y < 0 ? 0 : r.y;
        QC::i32 x2 = r.right();
        QC::i32 y2 = r.bottom();

        if (x2 > static_cast<QC::i32>(m_width))
            x2 = static_cast<QC::i32>(m_width);
        if (y2 > static_cast<QC::i32>(m_height))
            y2 = static_cast<QC::i32>(m_height);

        if (m_hasClip)
        {
            QC::i32 clipLeft = m_clip.x;
            QC::i32 clipTop = m_clip.y;
            QC::i32 clipRight = m_clip.x + static_cast<QC::i32>(m_clip.width);
            QC::i32 clipBottom = m_clip.y + static_cast<QC::i32>(m_clip.height);

            if (x1 < clipLeft)
                x1 = clipLeft;
            if (y1 < clipTop)
                y1 = clipTop;
            if (x2 > clipRight)
                x2 = clipRight;
            if (y2 > clipBottom)
                y2 = clipBottom;
        }

        if (x1 >= x2 || y1 >= y2)
            return;

        QC::i32 height = y2 - y1;
        if (height <= 0)
            return;

        QC::i32 segments = height > 1 ? height - 1 : 0;

        for (QC::i32 y = y1, step = 0; y < y2; ++y, ++step)
        {
            QC::Color color = segments > 0 ? gradientColor(top, bottom, step, segments) : top;
            QC::u32 *row = m_pixels + y * m_pitch;
            for (QC::i32 x = x1; x < x2; ++x)
            {
                row[x] = color.value;
            }
        }
    }

    void PainterSurface::fillGradientH(const QC::Rect &rect, QC::Color left, QC::Color right)
    {
        if (!m_pixels || m_pitch == 0 || rect.isEmpty())
            return;

        QC::Rect r = rect.offset(m_origin.x, m_origin.y);
        QC::i32 x1 = r.x < 0 ? 0 : r.x;
        QC::i32 y1 = r.y < 0 ? 0 : r.y;
        QC::i32 x2 = r.right();
        QC::i32 y2 = r.bottom();

        if (x2 > static_cast<QC::i32>(m_width))
            x2 = static_cast<QC::i32>(m_width);
        if (y2 > static_cast<QC::i32>(m_height))
            y2 = static_cast<QC::i32>(m_height);

        if (m_hasClip)
        {
            QC::i32 clipLeft = m_clip.x;
            QC::i32 clipTop = m_clip.y;
            QC::i32 clipRight = m_clip.x + static_cast<QC::i32>(m_clip.width);
            QC::i32 clipBottom = m_clip.y + static_cast<QC::i32>(m_clip.height);

            if (x1 < clipLeft)
                x1 = clipLeft;
            if (y1 < clipTop)
                y1 = clipTop;
            if (x2 > clipRight)
                x2 = clipRight;
            if (y2 > clipBottom)
                y2 = clipBottom;
        }

        if (x1 >= x2 || y1 >= y2)
            return;

        QC::i32 width = x2 - x1;
        if (width <= 0)
            return;

        QC::i32 segments = width > 1 ? width - 1 : 0;

        for (QC::i32 y = y1; y < y2; ++y)
        {
            QC::u32 *row = m_pixels + y * m_pitch;
            QC::i32 step = 0;
            for (QC::i32 x = x1; x < x2; ++x, ++step)
            {
                QC::Color color = segments > 0 ? gradientColor(left, right, step, segments) : left;
                row[x] = color.value;
            }
        }
    }

    void PainterSurface::drawText(QC::i32 x, QC::i32 y, const char *text, QC::Color color)
    {
        if (!text || !m_pixels || m_pitch == 0)
            return;

        QC::i32 cursorX = x;
        QC::i32 cursorY = y;

        for (const char *p = text; *p; ++p)
        {
            if (*p == '\n')
            {
                cursorX = x;
                cursorY += kGlyphH;
                continue;
            }

            const Glyph5x7 g = glyphForChar(*p);
            for (QC::i32 row = 0; row < 7; ++row)
            {
                // Decode with LSB as leftmost.
                QC::u8 bits = g.rows[row];
                for (QC::i32 col = 0; col < 5; ++col)
                {
                    if (bits & (1u << col))
                    {
                        setPixel(cursorX + col, cursorY + row, color);
                    }
                }
            }

            cursorX += kGlyphW;
        }
    }

    void PainterSurface::drawText(const QC::Rect &rect, const char *text, QC::Color color, const TextFormat &format)
    {
        if (!text || rect.isEmpty())
            return;

        // Minimal implementation: alignment + optional ellipsis for single-line text.
        // Word-wrap can be added later; for now, explicit '\n' is supported.

        const QC::Size textSize = measureTextMono5x7(text);

        QC::i32 startX = rect.x;
        QC::i32 startY = rect.y;

        if (hasFlag(format.align, TextAlign::Center))
            startX = rect.x + (static_cast<QC::i32>(rect.width) - textSize.width) / 2;
        else if (hasFlag(format.align, TextAlign::Right))
            startX = rect.x + static_cast<QC::i32>(rect.width) - textSize.width;

        if (hasFlag(format.align, TextAlign::VCenter))
            startY = rect.y + (static_cast<QC::i32>(rect.height) - textSize.height) / 2;
        else if (hasFlag(format.align, TextAlign::Bottom))
            startY = rect.y + static_cast<QC::i32>(rect.height) - textSize.height;

        // Clip to rect during draw.
        const bool hadClip = m_hasClip;
        const QC::Rect oldClip = m_clip;
        setClipRect(rect.offset(m_origin.x, m_origin.y));

        if (format.ellipsis && static_cast<QC::i32>(rect.width) > 0)
        {
            // Only applies to the first line, single-line-ish usage.
            const QC::i32 maxChars = static_cast<QC::i32>(rect.width) / kGlyphW;
            if (maxChars > 0)
            {
                QC::i32 count = 0;
                for (const char *p = text; *p && *p != '\n'; ++p)
                    ++count;

                if (count > maxChars)
                {
                    // Render truncated text with "...".
                    // Keep it simple and avoid allocations.
                    const QC::i32 dots = 3;
                    QC::i32 keep = maxChars - dots;
                    if (keep < 0)
                        keep = 0;

                    QC::i32 cursorX = startX;
                    QC::i32 cursorY = startY;
                    const char *p = text;
                    for (QC::i32 i = 0; i < keep && *p && *p != '\n'; ++i, ++p)
                    {
                        const Glyph5x7 g = glyphForChar(*p);
                        for (QC::i32 row = 0; row < 7; ++row)
                        {
                            QC::u8 bits = g.rows[row];
                            for (QC::i32 col = 0; col < 5; ++col)
                                if (bits & (1u << col))
                                    setPixel(cursorX + col, cursorY + row, color);
                        }
                        cursorX += kGlyphW;
                    }

                    for (QC::i32 i = 0; i < dots; ++i)
                    {
                        const Glyph5x7 g = glyphForChar('.');
                        for (QC::i32 row = 0; row < 7; ++row)
                        {
                            QC::u8 bits = g.rows[row];
                            for (QC::i32 col = 0; col < 5; ++col)
                                if (bits & (1u << col))
                                    setPixel(cursorX + col, cursorY + row, color);
                        }
                        cursorX += kGlyphW;
                    }

                    if (hadClip)
                        m_clip = oldClip, m_hasClip = true;
                    else
                        m_hasClip = false;
                    return;
                }
            }
        }

        drawText(startX, startY, text, color);

        if (hadClip)
            m_clip = oldClip, m_hasClip = true;
        else
            m_hasClip = false;
    }

    QC::Size PainterSurface::measureText(const char *text) const
    {
        return measureTextMono5x7(text);
    }

    void PainterSurface::blit(QC::i32 x, QC::i32 y,
                              const QC::u32 *pixels, QC::u32 width, QC::u32 height,
                              QC::u32 stride)
    {
        if (!m_pixels || !pixels || m_pitch == 0)
            return;

        if (stride == 0)
            stride = width;

        x += m_origin.x;
        y += m_origin.y;

        for (QC::u32 row = 0; row < height; ++row)
        {
            QC::i32 destY = y + static_cast<QC::i32>(row);
            if (destY < 0 || static_cast<QC::u32>(destY) >= m_height)
                continue;

            if (m_hasClip)
            {
                QC::i32 clipTop = m_clip.y;
                QC::i32 clipBottom = m_clip.y + static_cast<QC::i32>(m_clip.height);
                if (destY < clipTop || destY >= clipBottom)
                    continue;
            }

            const QC::u32 *srcRow = pixels + row * stride;
            QC::u32 *destRow = m_pixels + destY * m_pitch;

            for (QC::u32 col = 0; col < width; ++col)
            {
                QC::i32 destX = x + static_cast<QC::i32>(col);
                if (destX < 0 || static_cast<QC::u32>(destX) >= m_width)
                    continue;

                if (m_hasClip)
                {
                    QC::i32 clipLeft = m_clip.x;
                    QC::i32 clipRight = m_clip.x + static_cast<QC::i32>(m_clip.width);
                    if (destX < clipLeft || destX >= clipRight)
                        continue;
                }

                destRow[destX] = srcRow[col];
            }
        }
    }

    void PainterSurface::blitAlpha(QC::i32 x, QC::i32 y,
                                   const QC::u32 *pixels, QC::u32 width, QC::u32 height,
                                   QC::u32 stride)
    {
        if (!m_pixels || !pixels || m_pitch == 0)
            return;

        if (stride == 0)
            stride = width;

        x += m_origin.x;
        y += m_origin.y;

        for (QC::u32 row = 0; row < height; ++row)
        {
            QC::i32 destY = y + static_cast<QC::i32>(row);
            if (destY < 0 || static_cast<QC::u32>(destY) >= m_height)
                continue;

            if (m_hasClip)
            {
                QC::i32 clipTop = m_clip.y;
                QC::i32 clipBottom = m_clip.y + static_cast<QC::i32>(m_clip.height);
                if (destY < clipTop || destY >= clipBottom)
                    continue;
            }

            const QC::u32 *srcRow = pixels + row * stride;
            QC::u32 *destRow = m_pixels + destY * m_pitch;

            for (QC::u32 col = 0; col < width; ++col)
            {
                QC::i32 destX = x + static_cast<QC::i32>(col);
                if (destX < 0 || static_cast<QC::u32>(destX) >= m_width)
                    continue;

                if (m_hasClip)
                {
                    QC::i32 clipLeft = m_clip.x;
                    QC::i32 clipRight = m_clip.x + static_cast<QC::i32>(m_clip.width);
                    if (destX < clipLeft || destX >= clipRight)
                        continue;
                }

                QC::Color src(srcRow[col]);
                QC::Color dst(destRow[destX]);
                destRow[destX] = src.blend(dst).value;
            }
        }
    }

    void PainterSurface::clear(QC::Color color)
    {
        if (!m_pixels || m_pitch == 0)
            return;

        for (QC::u32 y = 0; y < m_height; ++y)
        {
            QC::u32 *row = m_pixels + y * m_pitch;
            std::fill(row, row + m_width, color.value);
        }
    }

    bool PainterSurface::inClip(QC::i32 x, QC::i32 y) const
    {
        if (!m_hasClip)
            return true;

        QC::i32 clipRight = m_clip.x + static_cast<QC::i32>(m_clip.width);
        QC::i32 clipBottom = m_clip.y + static_cast<QC::i32>(m_clip.height);

        return x >= m_clip.x &&
               y >= m_clip.y &&
               x < clipRight &&
               y < clipBottom;
    }

} // namespace QG
