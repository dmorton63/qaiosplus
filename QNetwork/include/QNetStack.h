#pragma once

// QNetwork Stack - Network stack manager
// Namespace: QNet

#include "QCTypes.h"

namespace QNet
{

    class Ethernet;
    class IP;
    class TCP;
    class UDP;
    class Socket;

    enum class Protocol : QC::u8
    {
        ICMP = 1,
        TCP = 6,
        UDP = 17
    };

    class Stack
    {
    public:
        static Stack &instance();

        void initialize();

        // Layer access
        Ethernet *ethernet() { return m_ethernet; }
        IP *ip() { return m_ip; }
        TCP *tcp() { return m_tcp; }
        UDP *udp() { return m_udp; }

        // Packet handling
        void receivePacket(const void *data, QC::usize length);
        void transmitPacket(const void *data, QC::usize length);

    private:
        Stack();
        ~Stack();
        Stack(const Stack &) = delete;
        Stack &operator=(const Stack &) = delete;

        Ethernet *m_ethernet;
        IP *m_ip;
        TCP *m_tcp;
        UDP *m_udp;
    };

} // namespace QNet
