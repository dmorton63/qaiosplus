// QDesktop Terminal - Simple command interpreter window
// Namespace: QD

#include "QDTerminal.h"

#include "QDDesktop.h"

#include "QCLogger.h"
#include "QCString.h"
#include "QCBuiltins.h"

#include "QWWindowManager.h"
#include "QWWindow.h"
#include "QWControls/Containers/Panel.h"
#include "QWControls/Leaf/Label.h"
#include "QWControls/Leaf/TextBox.h"

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

    static inline const char *skipSpaces(const char *p)
    {
        while (p && (*p == ' ' || *p == '\t'))
            ++p;
        return p;
    }
}

namespace QD
{

    Terminal::Terminal(Desktop *desktop)
        : m_desktop(desktop), m_window(nullptr), m_root(nullptr), m_output(nullptr), m_input(nullptr), m_outputLen(0)
    {
        m_outputBuf[0] = '\0';
    }

    Terminal::~Terminal()
    {
        // For now, keep it simple: if still open, let WindowManager own/destroy it at shutdown.
        m_window = nullptr;
        m_root = nullptr;
        m_output = nullptr;
        m_input = nullptr;
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

        if (streq(cmd, "help"))
        {
            appendLine("Commands:");
            appendLine("  help");
            appendLine("  echo <text>");
            appendLine("  clear");
            appendLine("  shutdown");
            return;
        }

        if (streq(cmd, "echo"))
        {
            appendLine(p && *p ? p : "");
            return;
        }

        if (streq(cmd, "clear"))
        {
            m_outputLen = 0;
            m_outputBuf[0] = '\0';
            if (m_output)
                m_output->setText("");
            return;
        }

        if (streq(cmd, "shutdown"))
        {
            appendLine("Shutting down...");
            // QEMU/Bochs ACPI shutdown
            QC::outw(0x604, 0x2000);
            QC::cli();
            for (;;)
                QC::halt();
        }

        appendLine("Unknown command. Type 'help'.");
    }

} // namespace QD
