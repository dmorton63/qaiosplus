// QDesktop Terminal - Simple command interpreter window
// Namespace: QD

#include "QDTerminal.h"

#include "QDDesktop.h"

#include "QCLogger.h"
#include "QCString.h"
#include "QCBuiltins.h"
#include "QKEventManager.h"
#include "QKShutdownController.h"
#include "QFSVFS.h"
#include "QFSFile.h"
#include "QFSDirectory.h"

#include "QWWindowManager.h"
#include "QWWindow.h"
#include "QWMessageBus.h"
#include "QWControls/Containers/Panel.h"
#include "QWControls/Leaf/Button.h"
#include "QWControls/Leaf/Label.h"
#include "QWControls/Leaf/TextBox.h"

#include "QKServiceRegistry.h"
#include "QKMsgBus.h"

#include "QDCommandMessages.h"

namespace
{
    static inline bool streq(const char *a, const char *b)
    {
        if (!a || !b)
            return false;
        while (*a && *b)
        {
            if (*a != *b)
                return false;
            ++a;
            ++b;
        }
        return *a == '\0' && *b == '\0';
    }

    static inline char lowerAscii(char c)
    {
        if (c >= 'A' && c <= 'Z')
            return static_cast<char>(c + 32);
        return c;
    }

    static inline bool streqIgnoreCase(const char *a, const char *b)
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

    static inline const char *skipSpaces(const char *p)
    {
        while (p && (*p == ' ' || *p == '\t'))
            ++p;
        return p;
    }

    static inline bool hasSlash(const char *p)
    {
        if (!p)
            return false;
        for (; *p; ++p)
        {
            if (*p == '/')
                return true;
        }
        return false;
    }

    static inline bool startsWith(const char *s, const char *prefix)
    {
        if (!s || !prefix)
            return false;
        while (*prefix)
        {
            if (*s++ != *prefix++)
                return false;
        }
        return true;
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
}

namespace QD
{

    namespace
    {
        static QC::u64 nextCorrelationId()
        {
            static QC::u64 g = 1;
            return g++;
        }
    }

    bool Terminal::onWindowMessage(QW::Window * /*window*/, const QW::Message &msg, void *userData)
    {
        auto *self = static_cast<Terminal *>(userData);
        if (!self)
            return false;

        if (msg.msgId == QD::CmdMsg::OutputLine || msg.msgId == QD::CmdMsg::ErrorLine)
        {
            const char *line = msg.payload ? static_cast<const char *>(msg.payload) : "";
            self->appendLine(line);
            return true;
        }

        if (msg.msgId == QD::CmdMsg::Done)
        {
            return true;
        }

        return false;
    }

    Terminal::Terminal(Desktop *desktop)
        : m_desktop(desktop), m_window(nullptr), m_root(nullptr), m_output(nullptr), m_input(nullptr), m_outputLen(0)
    {
        m_outputBuf[0] = '\0';
    }

    Terminal::~Terminal()
    {
        close();
    }

    void Terminal::open()
    {
        if (m_window)
        {
            focus();
            return;
        }

        if (!m_desktop)
            return;

        // Create a movable window inside the work area.
        const QC::Rect wa = m_desktop->workArea();
        const QC::u32 w = 640;
        const QC::u32 h = 360;
        QC::i32 x = wa.x + static_cast<QC::i32>((wa.width > w) ? ((wa.width - w) / 2) : 0);
        QC::i32 y = wa.y + 24;

        m_window = QW::WindowManager::instance().createWindow("Terminal", {x, y, w, h});
        if (!m_window)
            return;

        // Receive streaming output from CommandProcessor.
        m_window->setMessageHandler(&Terminal::onWindowMessage, this);

        // Disable close/min/max for now (keeps taskbar state simple).
        m_window->setFlags(QW::WindowFlags::Visible | QW::WindowFlags::Resizable | QW::WindowFlags::Movable | QW::WindowFlags::HasTitle | QW::WindowFlags::HasBorder);

        m_root = m_window->root();
        m_root->setBorderStyle(QW::Controls::BorderStyle::None);
        m_root->setPadding(8);

        // Output label (multiline)
        QC::Rect outBounds = {8, 8, static_cast<QC::u32>(w - 16), static_cast<QC::u32>(h - 16 - 28)};
        m_output = new QW::Controls::Label(m_window, "QAIOS+ Terminal\nType 'help'\n", outBounds);
        m_output->setWordWrap(true);
        m_output->setTransparent(false);
        m_output->setBackgroundColor(QW::Color(20, 20, 20, 255));
        m_output->setTextColor(QW::Color(230, 230, 230, 255));
        m_root->addChild(m_output);

        // Input textbox
        QC::Rect inBounds = {8, static_cast<QC::i32>(h - 8 - 20), static_cast<QC::u32>(w - 16), 20};
        m_input = new QW::Controls::TextBox(m_window, inBounds);
        m_input->setPlaceholder("command...");
        m_input->setBackgroundColor(QW::Color(20, 20, 20, 255));
        m_input->setTextColor(QW::Color(230, 230, 230, 255));
        m_input->setBorderColor(QW::Color(110, 110, 110, 255));
        m_input->setSelectionColor(QW::Color(80, 120, 170, 255));
        m_input->setTextSubmitHandler(&Terminal::onSubmit, this);
        m_root->addChild(m_input);

        // Close button in the upper-right corner
        QC::Rect closeBounds = {static_cast<QC::i32>(w - 28), 8, 20, 20};
        auto *closeButton = new QW::Controls::Button(m_window, "X", closeBounds);
        closeButton->setRole(QW::ButtonRole::Destructive);
        closeButton->setClickHandler(&Terminal::onCloseClick, this);
        m_root->addChild(closeButton);

        // Initialize output buffer from initial text
        const char *initial = m_output->text();
        if (initial)
        {
            QC::usize len = QC::String::strlen(initial);
            if (len >= OUTPUT_CAP)
                len = OUTPUT_CAP - 1;
            for (QC::usize i = 0; i < len; ++i)
                m_outputBuf[i] = initial[i];
            m_outputBuf[len] = '\0';
            m_outputLen = len;
        }

        focus();
        m_window->invalidate();
        QW::WindowManager::instance().render();

        // Optional taskbar entry
        m_desktop->addTaskbarWindow(m_window->windowId(), "Terminal");
        m_desktop->setActiveTaskbarWindow(m_window->windowId());
    }

    void Terminal::close()
    {
        if (!m_window)
            return;

        if (m_desktop)
        {
            m_desktop->removeTaskbarWindow(m_window->windowId());
        }

        QW::WindowManager::instance().destroyWindow(m_window);
        m_window = nullptr;
        m_root = nullptr;
        m_output = nullptr;
        m_input = nullptr;
    }

    void Terminal::focus()
    {
        if (!m_window)
            return;
        QW::WindowManager::instance().bringToFront(m_window);
        QW::WindowManager::instance().setFocus(m_window);
    }

    void Terminal::onSubmit(QW::Controls::TextBox *textBox, void *userData)
    {
        auto *self = static_cast<Terminal *>(userData);
        if (!self || !textBox)
            return;

        const char *line = textBox->text();
        if (!line)
            line = "";

        self->appendLine("> ");
        self->appendLine(line);
        self->executeLine(line);

        textBox->setText("");
    }

    void Terminal::appendLine(const char *line)
    {
        if (!line)
            return;

        const QC::usize addLen = QC::String::strlen(line);
        const QC::usize need = addLen + 1; // +\n

        // If overflow, drop oldest by resetting (minimal behavior)
        if (m_outputLen + need + 1 >= OUTPUT_CAP)
        {
            m_outputLen = 0;
            m_outputBuf[0] = '\0';
        }

        for (QC::usize i = 0; i < addLen; ++i)
        {
            m_outputBuf[m_outputLen++] = line[i];
        }
        m_outputBuf[m_outputLen++] = '\n';
        m_outputBuf[m_outputLen] = '\0';

        if (m_output)
        {
            m_output->setText(m_outputBuf);
        }
    }

    void Terminal::executeLine(const char *line)
    {
        if (!line)
            return;

        const char *p = skipSpaces(line);
        if (*p == '\0')
            return;

        // Extract command word
        char cmd[32];
        QC::usize ci = 0;
        while (*p && *p != ' ' && *p != '\t' && ci + 1 < sizeof(cmd))
        {
            cmd[ci++] = *p++;
        }
        cmd[ci] = '\0';

        p = skipSpaces(p);

        // Local-only commands (UI state): help/clear/saveterm.
        if (streqIgnoreCase(cmd, "help"))
        {
            appendLine("Commands:");
            appendLine("  help");
            appendLine("  ls [path]");
            appendLine("  echo <text>");
            appendLine("  clear");
            appendLine("  saveterm [name|/shared/path]");
            appendLine("  shutdown");
            return;
        }

        if (streqIgnoreCase(cmd, "clear"))
        {
            m_outputLen = 0;
            m_outputBuf[0] = '\0';
            if (m_output)
                m_output->setText("");
            return;
        }

        if (streqIgnoreCase(cmd, "saveterm"))
        {
            const char *arg = (p && *p) ? p : nullptr;

            char outPath[256];
            QC::String::memset(outPath, 0, sizeof(outPath));

            if (!arg || *arg == '\0')
            {
                QC::String::strncpy(outPath, "/shared/citadel.txt", sizeof(outPath) - 1);
            }
            else if (!hasSlash(arg))
            {
                QC::String::strncpy(outPath, "/shared/", sizeof(outPath) - 1);
                QC::usize used = QC::String::strlen(outPath);
                QC::String::strncpy(outPath + used, arg, sizeof(outPath) - 1 - used);
            }
            else
            {
                if (!startsWith(arg, "/shared"))
                {
                    appendLine("saveterm: path must be under /shared");
                    return;
                }
                QC::String::strncpy(outPath, arg, sizeof(outPath) - 1);
            }

            QFS::File *file = QFS::VFS::instance().open(
                outPath,
                QFS::OpenMode::Write | QFS::OpenMode::Create | QFS::OpenMode::Truncate);
            if (!file)
            {
                appendLine("saveterm: cannot open output file (is /shared mounted + writable?)");
                return;
            }

            if (m_outputLen > 0)
            {
                (void)file->write(m_outputBuf, m_outputLen);
                (void)file->write("\r\n", 2);
            }

            QFS::VFS::instance().close(file);
            appendLine("saveterm: wrote transcript to:");
            appendLine(outPath);
            return;
        }

        // All other commands go through CommandProcessor.
        if (!m_window)
        {
            appendLine("terminal: no window for command routing");
            return;
        }

        QK::Msg::Envelope *env = QK::Msg::makeEnvelope(QK::Msg::Topic::SvcMsg, nextCorrelationId());
        env->senderId = m_window->windowId();
        env->param1 = QD::CmdMsg::Request;
        env->payload = dupString(line);
        env->destroyPayload = &destroyOwnedString;

        const bool ok = QK::Svc::Registry::instance().sendTo(QD::CmdMsg::ServiceName, env);
        QK::Msg::release(env);

        if (!ok)
        {
            appendLine("command processor: send failed");
        }
    }

    void Terminal::listDirectory(const char *path)
    {
        const char *target = (path && *path) ? path : "/";
        QFS::Directory *dir = QFS::VFS::instance().openDir(target);
        if (!dir)
        {
            appendLine("ls: cannot open path");
            return;
        }

        char heading[320];
        QC::String::memset(heading, 0, sizeof(heading));
        const char prefix[] = "Listing ";
        QC::usize idx = 0;
        for (QC::usize i = 0; prefix[i] && idx + 1 < sizeof(heading); ++i)
        {
            heading[idx++] = prefix[i];
        }
        for (QC::usize i = 0; target[i] && idx + 1 < sizeof(heading); ++i)
        {
            heading[idx++] = target[i];
        }
        heading[idx] = '\0';
        appendLine(heading);

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

            // Size
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
                {
                    sizeBuf[sizeIdx++] = temp[i];
                }
            }
            for (int i = 0; i < sizeIdx && pos + 1 < sizeof(line); ++i)
            {
                line[pos++] = sizeBuf[i];
            }
            line[pos++] = ' ';

            for (int i = 0; entry.name[i] && pos + 1 < sizeof(line); ++i)
            {
                line[pos++] = entry.name[i];
            }
            line[pos] = '\0';
            appendLine(line);
        }

        QFS::VFS::instance().closeDir(dir);
    }

    void Terminal::onCloseClick(QW::Controls::Button *button, void *userData)
    {
        (void)button;
        auto *self = static_cast<Terminal *>(userData);
        if (!self)
            return;

        QC_LOG_INFO("QDTerminal", "Close button clicked");
        self->close();
    }

} // namespace QD
