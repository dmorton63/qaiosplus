#include "QDCommandProcessor.h"

#include "QDCommandMessages.h"

#include "QCCommandRegistry.h"
#include "QCString.h"

#include "QKServiceRegistry.h"
#include "QKMsgBus.h"

#include "QFSVFS.h"
#include "QFSDirectory.h"

#include "QKEventManager.h"
#include "QKShutdownController.h"

namespace
{
    static const char *skipSpaces(const char *p)
    {
        while (p && (*p == ' ' || *p == '\t'))
            ++p;
        return p;
    }

    static void destroyOwnedString(void *p)
    {
        char *s = static_cast<char *>(p);
        operator delete[](s);
    }

    static char *dupString(const char *s)
    {
        if (!s)
            s = "";
        const QC::usize len = QC::String::strlen(s);
        char *out = static_cast<char *>(operator new[](len + 1));
        for (QC::usize i = 0; i < len; ++i)
            out[i] = s[i];
        out[len] = '\0';
        return out;
    }

    static bool publishWindowLine(QC::u32 toWindowId, QC::u32 msgId, QC::u64 correlationId, const char *text)
    {
        QK::Msg::Envelope *env = QK::Msg::makeEnvelope(QK::Msg::Topic::WinMsg, correlationId);
        env->senderId = 0;
        env->targetId = toWindowId;
        env->param1 = msgId;
        env->param2 = 0;

        if (text)
        {
            env->payload = dupString(text);
            env->destroyPayload = &destroyOwnedString;
        }

        const bool ok = QK::Msg::Bus::instance().publish(env);
        QK::Msg::release(env);
        return ok;
    }

    // ---------------- Commands (shared registry handlers) ----------------

    static bool cmdEcho(const char *args, const QC::Cmd::Context &ctx, void *)
    {
        ctx.writeLine(args && *args ? args : "");
        return true;
    }

    static bool cmdLs(const char *args, const QC::Cmd::Context &ctx, void *)
    {
        const char *path = (args && *args) ? skipSpaces(args) : "/";

        QFS::Directory *dir = QFS::VFS::instance().openDir(path);
        if (!dir)
        {
            ctx.writeLine("ls: cannot open path");
            return true;
        }

        // Heading
        char heading[320];
        QC::String::memset(heading, 0, sizeof(heading));
        const char prefix[] = "Listing ";
        QC::usize idx = 0;
        for (QC::usize i = 0; prefix[i] && idx + 1 < sizeof(heading); ++i)
            heading[idx++] = prefix[i];
        for (QC::usize i = 0; path[i] && idx + 1 < sizeof(heading); ++i)
            heading[idx++] = path[i];
        heading[idx] = '\0';
        ctx.writeLine(heading);

        QFS::DirEntry entry;
        while (dir->read(&entry))
        {
            char line[320];
            QC::String::memset(line, 0, sizeof(line));
            QC::usize pos = 0;

            char typeChar = '-';
            if (entry.type == QFS::FileType::Directory)
                typeChar = 'd';
            else if (entry.type == QFS::FileType::SymLink)
                typeChar = 'l';
            line[pos++] = typeChar;
            line[pos++] = ' ';

            // Size (decimal)
            char sizeBuf[32];
            QC::String::memset(sizeBuf, 0, sizeof(sizeBuf));
            QC::u64 value = entry.size;
            int sizeIdx = 0;
            if (value == 0)
            {
                sizeBuf[sizeIdx++] = '0';
            }
            else
            {
                char temp[32];
                int tempIdx = 0;
                while (value > 0 && tempIdx < 31)
                {
                    temp[tempIdx++] = static_cast<char>('0' + (value % 10));
                    value /= 10;
                }
                for (int i = tempIdx - 1; i >= 0; --i)
                    sizeBuf[sizeIdx++] = temp[i];
            }
            for (int i = 0; i < sizeIdx && pos + 1 < sizeof(line); ++i)
                line[pos++] = sizeBuf[i];
            line[pos++] = ' ';

            for (int i = 0; entry.name[i] && pos + 1 < sizeof(line); ++i)
                line[pos++] = entry.name[i];

            line[pos] = '\0';
            ctx.writeLine(line);
        }

        QFS::VFS::instance().closeDir(dir);
        return true;
    }

    static bool cmdShutdown(const char *, const QC::Cmd::Context &ctx, void *)
    {
        ctx.writeLine("Shutdown requested. Awaiting confirmation...");
        QK::Event::EventManager::instance().postShutdownEvent(
            QK::Event::Type::ShutdownRequest,
            static_cast<QC::u32>(QK::Shutdown::Reason::ShellCommand));
        return true;
    }

}

namespace QD
{
    CommandProcessor &CommandProcessor::instance()
    {
        static CommandProcessor proc;
        return proc;
    }

    void CommandProcessor::registerCommands()
    {
        if (m_commandsRegistered)
            return;

        auto &reg = QC::Cmd::Registry::instance();
        (void)reg.registerCommand("echo", &cmdEcho, nullptr);
        (void)reg.registerCommand("ls", &cmdLs, nullptr);
        (void)reg.registerCommand("shutdown", &cmdShutdown, nullptr);

        m_commandsRegistered = true;
    }

    void CommandProcessor::initialize()
    {
        if (m_initialized)
            return;

        registerCommands();

        // Register as a named service.
        m_serviceId = QK::Svc::Registry::instance().registerService(QD::CmdMsg::ServiceName, &CommandProcessor::onServiceMessage, this);
        m_initialized = (m_serviceId != 0);
    }

    void CommandProcessor::onServiceMessage(QK::Msg::Envelope *env, void *userData)
    {
        auto *self = static_cast<CommandProcessor *>(userData);
        if (!self || !env)
            return;

        const QC::u32 msgId = static_cast<QC::u32>(env->param1);
        if (msgId != QD::CmdMsg::Request)
            return;

        const QC::u32 replyWindowId = env->senderId;
        const QC::u64 corr = env->correlationId;
        const char *line = env->payload ? static_cast<const char *>(env->payload) : "";

        if (replyWindowId == 0)
            return;

        // Output callback streams back to the terminal window.
        QC::Cmd::Context ctx;
        ctx.out = [](const char *text, void *ud)
        {
            const QC::u64 corrId = reinterpret_cast<QC::u64>(ud);
            // ud packs (windowId in low 32) + corr in high 32 is not enough; so we publish via globals.
            (void)corrId;
        };

        // We need both windowId and correlationId in the callback; store them in a small struct.
        struct OutCtx
        {
            QC::u32 windowId;
            QC::u64 corr;
        };

        OutCtx outCtx{replyWindowId, corr};
        ctx.out = [](const char *text, void *ud)
        {
            OutCtx *c = static_cast<OutCtx *>(ud);
            if (!c)
                return;
            (void)publishWindowLine(c->windowId, QD::CmdMsg::OutputLine, c->corr, text);
        };
        ctx.userData = &outCtx;

        // Execute via shared registry.
        const bool handled = QC::Cmd::Registry::instance().execute(line, ctx);
        if (!handled)
        {
            (void)publishWindowLine(replyWindowId, QD::CmdMsg::ErrorLine, corr, "Unknown command. Type 'help'.");
        }

        (void)publishWindowLine(replyWindowId, QD::CmdMsg::Done, corr, nullptr);
    }

}
