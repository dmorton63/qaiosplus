#pragma once

// QNetwork TCP - Transmission Control Protocol
// Namespace: QNet

#include "QCTypes.h"
#include "QNetIP.h"

namespace QNet
{

    // TCP header
    struct TCPHeader
    {
        QC::u16 sourcePort;
        QC::u16 destPort;
        QC::u32 seqNumber;
        QC::u32 ackNumber;
        QC::u8 dataOffset;
        QC::u8 flags;
        QC::u16 window;
        QC::u16 checksum;
        QC::u16 urgentPointer;
    } __attribute__((packed));

    // TCP flags
    namespace TCPFlags
    {
        constexpr QC::u8 FIN = 0x01;
        constexpr QC::u8 SYN = 0x02;
        constexpr QC::u8 RST = 0x04;
        constexpr QC::u8 PSH = 0x08;
        constexpr QC::u8 ACK = 0x10;
        constexpr QC::u8 URG = 0x20;
    }

    // TCP states
    enum class TCPState : QC::u8
    {
        Closed,
        Listen,
        SynSent,
        SynReceived,
        Established,
        FinWait1,
        FinWait2,
        CloseWait,
        Closing,
        LastAck,
        TimeWait
    };

    // TCP connection
    struct TCPConnection
    {
        IPv4Address localAddr;
        QC::u16 localPort;
        IPv4Address remoteAddr;
        QC::u16 remotePort;

        TCPState state;

        QC::u32 sendUnacked;
        QC::u32 sendNext;
        QC::u32 sendWindow;

        QC::u32 recvNext;
        QC::u32 recvWindow;

        // Buffers
        QC::u8 *sendBuffer;
        QC::usize sendBufferSize;
        QC::u8 *recvBuffer;
        QC::usize recvBufferSize;
    };

    class TCP
    {
    public:
        TCP();
        ~TCP();

        void initialize();

        // Connection management
        TCPConnection *connect(IPv4Address remoteAddr, QC::u16 remotePort);
        TCPConnection *listen(QC::u16 port);
        void close(TCPConnection *conn);

        // Data transfer
        QC::isize send(TCPConnection *conn, const void *data, QC::usize length);
        QC::isize receive(TCPConnection *conn, void *buffer, QC::usize length);

        // Packet handling
        void receivePacket(IPv4Address source, const void *data, QC::usize length);

    private:
        void sendSegment(TCPConnection *conn, QC::u8 flags,
                         const void *data, QC::usize length);
        void processSegment(TCPConnection *conn, const TCPHeader *header,
                            const void *data, QC::usize length);

        QC::u16 allocatePort();
        TCPConnection *findConnection(IPv4Address remoteAddr, QC::u16 remotePort,
                                      QC::u16 localPort);
        QC::u16 calculateChecksum(IPv4Address srcAddr, IPv4Address destAddr,
                                  const void *segment, QC::usize length);

        static constexpr QC::usize MAX_CONNECTIONS = 256;
        TCPConnection *m_connections[MAX_CONNECTIONS];
        QC::u16 m_nextPort;
    };

} // namespace QNet
