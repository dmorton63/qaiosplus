#pragma once

// QDesktop Terminal - Simple command interpreter window
// Namespace: QD

#include "QCTypes.h"

namespace QW
{
    class Window;
    struct Message;
}

namespace QW::Controls
{
    class Label;
    class TextBox;
    class Panel;
    class Button;
}

namespace QD
{

    class Desktop;

    class Terminal
    {
    public:
        explicit Terminal(Desktop *desktop);
        ~Terminal();

        void open();
        bool isOpen() const { return m_window != nullptr; }
        void close();

        void focus();

    private:
        static void onSubmit(QW::Controls::TextBox *textBox, void *userData);
        static void onCloseClick(QW::Controls::Button *button, void *userData);
        static bool onWindowMessage(QW::Window *window, const QW::Message &msg, void *userData);

        void appendLine(const char *line);
        void executeLine(const char *line);
        void listDirectory(const char *path);

        Desktop *m_desktop;

        QW::Window *m_window;
        QW::Controls::Panel *m_root;
        QW::Controls::Label *m_output;
        QW::Controls::TextBox *m_input;

        static constexpr QC::usize OUTPUT_CAP = 4096;
        char m_outputBuf[OUTPUT_CAP];
        QC::usize m_outputLen;
    };

} // namespace QD
