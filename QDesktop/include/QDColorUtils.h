#pragma once

// QDesktop Color Utilities
// Namespace: QD

#include "QCColor.h"
#include "QCTypes.h"
#include "QCString.h"

namespace QD
{
    inline bool parseColorString(const char *text, QC::Color &out)
    {
        if (!text)
            return false;

        if (text[0] != '#')
            return false;

        const QC::usize len = QC::String::strlen(text);
        if (len != 7 && len != 9)
            return false;

        auto hex = [](char c) -> int
        {
            if (c >= '0' && c <= '9')
                return c - '0';
            if (c >= 'a' && c <= 'f')
                return 10 + (c - 'a');
            if (c >= 'A' && c <= 'F')
                return 10 + (c - 'A');
            return -1;
        };

        auto parsePair = [&](char hi, char lo, QC::u8 &value) -> bool
        {
            int h = hex(hi);
            int l = hex(lo);
            if (h < 0 || l < 0)
                return false;
            value = static_cast<QC::u8>((h << 4) | l);
            return true;
        };

        QC::u8 a = 0xFF;
        QC::u8 r = 0;
        QC::u8 g = 0;
        QC::u8 b = 0;

        const bool hasAlpha = (len == 9);
        const char *cursor = text + 1;
        if (hasAlpha)
        {
            if (!parsePair(cursor[0], cursor[1], a))
                return false;
            cursor += 2;
        }

        if (!parsePair(cursor[0], cursor[1], r))
            return false;
        cursor += 2;
        if (!parsePair(cursor[0], cursor[1], g))
            return false;
        cursor += 2;
        if (!parsePair(cursor[0], cursor[1], b))
            return false;

        out = QC::Color(r, g, b, a);
        return true;
    }

} // namespace QD
