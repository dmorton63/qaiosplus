#pragma once

// QArch Port I/O - Low-level port access
// Namespace: QArch

#include "QCTypes.h"

namespace QArch
{

    // Inline port I/O functions
    inline void outb(QC::u16 port, QC::u8 value)
    {
        asm volatile("outb %0, %1" : : "a"(value), "Nd"(port));
    }

    inline QC::u8 inb(QC::u16 port)
    {
        QC::u8 value;
        asm volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
        return value;
    }

    inline void outw(QC::u16 port, QC::u16 value)
    {
        asm volatile("outw %0, %1" : : "a"(value), "Nd"(port));
    }

    inline QC::u16 inw(QC::u16 port)
    {
        QC::u16 value;
        asm volatile("inw %1, %0" : "=a"(value) : "Nd"(port));
        return value;
    }

    inline void outl(QC::u16 port, QC::u32 value)
    {
        asm volatile("outl %0, %1" : : "a"(value), "Nd"(port));
    }

    inline QC::u32 inl(QC::u16 port)
    {
        QC::u32 value;
        asm volatile("inl %1, %0" : "=a"(value) : "Nd"(port));
        return value;
    }

    // I/O wait (for legacy devices)
    inline void io_wait()
    {
        outb(0x80, 0);
    }

    // String I/O
    inline void outsb(QC::u16 port, const void *data, QC::usize count)
    {
        asm volatile("rep outsb" : "+S"(data), "+c"(count) : "d"(port));
    }

    inline void insb(QC::u16 port, void *data, QC::usize count)
    {
        asm volatile("rep insb" : "+D"(data), "+c"(count) : "d"(port) : "memory");
    }

    inline void outsw(QC::u16 port, const void *data, QC::usize count)
    {
        asm volatile("rep outsw" : "+S"(data), "+c"(count) : "d"(port));
    }

    inline void insw(QC::u16 port, void *data, QC::usize count)
    {
        asm volatile("rep insw" : "+D"(data), "+c"(count) : "d"(port) : "memory");
    }

    inline void outsl(QC::u16 port, const void *data, QC::usize count)
    {
        asm volatile("rep outsl" : "+S"(data), "+c"(count) : "d"(port));
    }

    inline void insl(QC::u16 port, void *data, QC::usize count)
    {
        asm volatile("rep insl" : "+D"(data), "+c"(count) : "d"(port) : "memory");
    }

} // namespace QArch
