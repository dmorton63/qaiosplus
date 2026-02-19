#include "../include/QCCommandRegistry.h"

#include "QCString.h"

namespace QC
{
    namespace Cmd
    {
        static inline char lowerAscii(char c)
        {
            if (c >= 'A' && c <= 'Z')
                return static_cast<char>(c + 32);
            return c;
        }

        Registry &Registry::instance()
        {
            static Registry reg;
            return reg;
        }

        const char *Registry::skipSpaces(const char *p)
        {
            while (p && (*p == ' ' || *p == '\t'))
                ++p;
            return p;
        }

        bool Registry::equalsIgnoreCase(const char *a, const char *b)
        {
            if (!a || !b)
                return a == b;
            while (*a && *b)
            {
                if (lowerAscii(*a) != lowerAscii(*b))
                    return false;
                ++a;
                ++b;
            }
            return *a == '\0' && *b == '\0';
        }

        bool Registry::registerCommand(const char *name, Handler handler, void *userData)
        {
            if (!name || !handler)
                return false;

            // Reject duplicates.
            for (QC::usize i = 0; i < m_entries.size(); ++i)
            {
                if (m_entries[i].name && equalsIgnoreCase(m_entries[i].name, name))
                    return false;
            }

            Entry e;
            e.name = name;
            e.handler = handler;
            e.userData = userData;
            m_entries.push_back(e);
            return true;
        }

        const char *Registry::commandNameAt(QC::usize index) const
        {
            if (index >= m_entries.size())
                return nullptr;
            return m_entries[index].name;
        }

        bool Registry::execute(const char *line, const Context &ctx)
        {
            if (!line)
                return false;

            const char *p = skipSpaces(line);
            if (*p == '\0')
                return false;

            // Extract command token.
            char cmd[48];
            QC::usize ci = 0;
            while (*p && *p != ' ' && *p != '\t' && ci + 1 < sizeof(cmd))
            {
                cmd[ci++] = *p++;
            }
            cmd[ci] = '\0';

            p = skipSpaces(p);

            for (QC::usize i = 0; i < m_entries.size(); ++i)
            {
                const Entry &e = m_entries[i];
                if (!e.name || !e.handler)
                    continue;

                if (equalsIgnoreCase(e.name, cmd))
                {
                    return e.handler(p, ctx, e.userData);
                }
            }

            return false;
        }

    } // namespace Cmd
} // namespace QC
