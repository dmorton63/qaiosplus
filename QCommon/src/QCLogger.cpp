// QCommon Logger - Implementation
// Namespace: QC

#include "QCLogger.h"
#include "QCBuiltins.h"

namespace QC
{

    // Serial port for debug output
    constexpr u16 SERIAL_COM1 = 0x3F8;

    Logger &Logger::instance()
    {
        static Logger instance;
        return instance;
    }

    Logger::Logger() : m_level(LogLevel::Info)
    {
        // Initialize serial port for logging
        outb(SERIAL_COM1 + 1, 0x00); // Disable interrupts
        outb(SERIAL_COM1 + 3, 0x80); // Enable DLAB
        outb(SERIAL_COM1 + 0, 0x03); // Set divisor to 3 (38400 baud)
        outb(SERIAL_COM1 + 1, 0x00);
        outb(SERIAL_COM1 + 3, 0x03); // 8 bits, no parity, one stop bit
        outb(SERIAL_COM1 + 2, 0xC7); // Enable FIFO
        outb(SERIAL_COM1 + 4, 0x0B); // IRQs enabled, RTS/DSR set
    }

    void Logger::setLevel(LogLevel level)
    {
        m_level = level;
    }

    LogLevel Logger::getLevel() const
    {
        return m_level;
    }

    void Logger::outputChar(char c)
    {
        // Wait for transmit buffer to be empty
        while ((inb(SERIAL_COM1 + 5) & 0x20) == 0)
            ;
        outb(SERIAL_COM1, static_cast<u8>(c));
    }

    void Logger::outputString(const char *str)
    {
        while (*str)
        {
            outputChar(*str++);
        }
    }

    void Logger::log(LogLevel level, const char *module, const char *format, ...)
    {
        if (level < m_level)
            return;

        // Print log level prefix
        const char *levelStr;
        switch (level)
        {
        case LogLevel::Trace:
            levelStr = "[TRACE]";
            break;
        case LogLevel::Debug:
            levelStr = "[DEBUG]";
            break;
        case LogLevel::Info:
            levelStr = "[INFO ]";
            break;
        case LogLevel::Warning:
            levelStr = "[WARN ]";
            break;
        case LogLevel::Error:
            levelStr = "[ERROR]";
            break;
        case LogLevel::Fatal:
            levelStr = "[FATAL]";
            break;
        default:
            levelStr = "[?????]";
            break;
        }

        outputString(levelStr);
        outputString(" ");
        outputString(module);
        outputString(": ");
        outputString(format); // TODO: Implement printf-style formatting
        outputString("\n");
    }

    void Logger::trace(const char *module, const char *format, ...)
    {
        log(LogLevel::Trace, module, format);
    }

    void Logger::debug(const char *module, const char *format, ...)
    {
        log(LogLevel::Debug, module, format);
    }

    void Logger::info(const char *module, const char *format, ...)
    {
        log(LogLevel::Info, module, format);
    }

    void Logger::warning(const char *module, const char *format, ...)
    {
        log(LogLevel::Warning, module, format);
    }

    void Logger::error(const char *module, const char *format, ...)
    {
        log(LogLevel::Error, module, format);
    }

    void Logger::fatal(const char *module, const char *format, ...)
    {
        log(LogLevel::Fatal, module, format);
    }

} // namespace QC
