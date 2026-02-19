#pragma once

// QCCommandRegistry - shared command registration + dispatch
// Namespace: QC::Cmd
//
// Design:
// - Freestanding-friendly (no STL/exceptions)
// - Command handlers stream output via a callback
// - Registry is shared so multiple front-ends (terminal, command processor, etc.) can reuse the same commands

#include "QCTypes.h"
#include "QCVector.h"

namespace QC
{
    namespace Cmd
    {
        using OutputFn = void (*)(const char *text, void *userData);

        struct Context
        {
            OutputFn out = nullptr;
            void *userData = nullptr;

            void writeLine(const char *text) const
            {
                if (out)
                    out(text, userData);
            }
        };

        using Handler = bool (*)(const char *args, const Context &ctx, void *userData);

        class Registry
        {
        public:
            static Registry &instance();

            bool registerCommand(const char *name, Handler handler, void *userData = nullptr);

            // Execute a command line. Returns true if a command was found and invoked.
            bool execute(const char *line, const Context &ctx);

            // Best-effort enumeration for help output.
            QC::usize commandCount() const { return m_entries.size(); }
            const char *commandNameAt(QC::usize index) const;

        private:
            Registry() = default;

            struct Entry
            {
                const char *name = nullptr;
                Handler handler = nullptr;
                void *userData = nullptr;
            };

            QC::Vector<Entry> m_entries;

            static bool equalsIgnoreCase(const char *a, const char *b);
            static const char *skipSpaces(const char *p);
        };

    } // namespace Cmd
} // namespace QC
