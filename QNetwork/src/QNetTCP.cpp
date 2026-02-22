// QNetwork TCP - Transmission Control Protocol implementation
// Namespace: QNet

#include "QNetTCP.h"
#include "QNetStack.h"
#include "QNetIP.h"
#include "QCMemUtil.h"
#include "QKMemHeap.h"

namespace QNet
{

    // Byte swap utilities
    static inline QC::u16 htons(QC::u16 val)
    {
        return (val >> 8) | (val << 8);
    }

    static inline QC::u16 ntohs(QC::u16 val)
    {
        return htons(val);
    }

    static inline QC::u32 htonl(QC::u32 val)
    {
        return ((val >> 24) & 0x000000FF) |
               ((val >> 8) & 0x0000FF00) |
               ((val << 8) & 0x00FF0000) |
               ((val << 24) & 0xFF000000);
    }

    static inline QC::u32 ntohl(QC::u32 val)
    {
        return htonl(val);
    }

    static inline void checksumAddBytes(QC::u32 &sum, const void *data, QC::usize length)
    {
        const auto *bytes = static_cast<const QC::u8 *>(data);
        while (length >= 2)
        {
            const QC::u16 word = static_cast<QC::u16>((static_cast<QC::u16>(bytes[0]) << 8) | bytes[1]);
            sum += word;
            bytes += 2;
            length -= 2;
        }

        if (length == 1)
        {
            const QC::u16 word = static_cast<QC::u16>(static_cast<QC::u16>(bytes[0]) << 8);
            sum += word;
        }
    }

    // Buffer sizes
    static constexpr QC::usize DEFAULT_SEND_BUFFER = 8192;
    static constexpr QC::usize DEFAULT_RECV_BUFFER = 8192;
    static constexpr QC::usize DEFAULT_WINDOW = 65535;

    TCP::TCP()
        : m_nextPort(49152) // Start of ephemeral port range
    {
        memset(m_connections, 0, sizeof(m_connections));
    }

    TCP::~TCP()
    {
        for (QC::usize i = 0; i < MAX_CONNECTIONS; i++)
        {
            if (m_connections[i])
            {
                if (m_connections[i]->sendBuffer)
                    QK::Memory::Heap::instance().free(m_connections[i]->sendBuffer);
                if (m_connections[i]->recvBuffer)
                    QK::Memory::Heap::instance().free(m_connections[i]->recvBuffer);
                QK::Memory::Heap::instance().free(m_connections[i]);
            }
        }
    }

    void TCP::initialize()
    {
        m_nextPort = 49152;
    }

    TCPConnection *TCP::connect(IPv4Address remoteAddr, QC::u16 remotePort)
    {
        // Find free slot
        QC::usize slot = MAX_CONNECTIONS;
        for (QC::usize i = 0; i < MAX_CONNECTIONS; i++)
        {
            if (!m_connections[i])
            {
                slot = i;
                break;
            }
        }
        if (slot == MAX_CONNECTIONS)
            return nullptr;

        // Create connection
        auto *conn = static_cast<TCPConnection *>(QK::Memory::Heap::instance().allocate(sizeof(TCPConnection)));
        if (!conn)
            return nullptr;

        memset(conn, 0, sizeof(TCPConnection));
        conn->localAddr = Stack::instance().ip()->address();
        conn->localPort = allocatePort();
        conn->remoteAddr = remoteAddr;
        conn->remotePort = remotePort;
        conn->state = TCPState::SynSent;

        // Allocate buffers
        conn->sendBuffer = static_cast<QC::u8 *>(QK::Memory::Heap::instance().allocate(DEFAULT_SEND_BUFFER));
        conn->sendBufferSize = DEFAULT_SEND_BUFFER;
        conn->recvBuffer = static_cast<QC::u8 *>(QK::Memory::Heap::instance().allocate(DEFAULT_RECV_BUFFER));
        conn->recvBufferSize = DEFAULT_RECV_BUFFER;

        if (!conn->sendBuffer || !conn->recvBuffer)
        {
            if (conn->sendBuffer)
                QK::Memory::Heap::instance().free(conn->sendBuffer);
            if (conn->recvBuffer)
                QK::Memory::Heap::instance().free(conn->recvBuffer);
            QK::Memory::Heap::instance().free(conn);
            return nullptr;
        }

        // Initialize sequence numbers (should use random ISN in production)
        conn->sendUnacked = 1000;
        conn->sendNext = 1000;
        conn->sendWindow = DEFAULT_WINDOW;
        conn->recvNext = 0;
        conn->recvWindow = DEFAULT_WINDOW;

        m_connections[slot] = conn;

        // Send SYN
        sendSegment(conn, TCPFlags::SYN, nullptr, 0);
        conn->sendNext++;

        return conn;
    }

    TCPConnection *TCP::listen(QC::u16 port)
    {
        // Find free slot
        QC::usize slot = MAX_CONNECTIONS;
        for (QC::usize i = 0; i < MAX_CONNECTIONS; i++)
        {
            if (!m_connections[i])
            {
                slot = i;
                break;
            }
        }
        if (slot == MAX_CONNECTIONS)
            return nullptr;

        // Create listening connection
        auto *conn = static_cast<TCPConnection *>(QK::Memory::Heap::instance().allocate(sizeof(TCPConnection)));
        if (!conn)
            return nullptr;

        memset(conn, 0, sizeof(TCPConnection));
        conn->localAddr = Stack::instance().ip()->address();
        conn->localPort = port;
        conn->state = TCPState::Listen;

        // Allocate buffers
        conn->sendBuffer = static_cast<QC::u8 *>(QK::Memory::Heap::instance().allocate(DEFAULT_SEND_BUFFER));
        conn->sendBufferSize = DEFAULT_SEND_BUFFER;
        conn->recvBuffer = static_cast<QC::u8 *>(QK::Memory::Heap::instance().allocate(DEFAULT_RECV_BUFFER));
        conn->recvBufferSize = DEFAULT_RECV_BUFFER;

        if (!conn->sendBuffer || !conn->recvBuffer)
        {
            if (conn->sendBuffer)
                QK::Memory::Heap::instance().free(conn->sendBuffer);
            if (conn->recvBuffer)
                QK::Memory::Heap::instance().free(conn->recvBuffer);
            QK::Memory::Heap::instance().free(conn);
            return nullptr;
        }

        conn->recvWindow = DEFAULT_WINDOW;

        m_connections[slot] = conn;

        return conn;
    }

    void TCP::close(TCPConnection *conn)
    {
        if (!conn)
            return;

        switch (conn->state)
        {
        case TCPState::Listen:
        case TCPState::SynSent:
            conn->state = TCPState::Closed;
            break;

        case TCPState::SynReceived:
        case TCPState::Established:
            sendSegment(conn, TCPFlags::FIN | TCPFlags::ACK, nullptr, 0);
            conn->sendNext++;
            conn->state = TCPState::FinWait1;
            break;

        case TCPState::CloseWait:
            sendSegment(conn, TCPFlags::FIN | TCPFlags::ACK, nullptr, 0);
            conn->sendNext++;
            conn->state = TCPState::LastAck;
            break;

        default:
            break;
        }

        // If closed, clean up
        if (conn->state == TCPState::Closed)
        {
            for (QC::usize i = 0; i < MAX_CONNECTIONS; i++)
            {
                if (m_connections[i] == conn)
                {
                    if (conn->sendBuffer)
                        QK::Memory::Heap::instance().free(conn->sendBuffer);
                    if (conn->recvBuffer)
                        QK::Memory::Heap::instance().free(conn->recvBuffer);
                    QK::Memory::Heap::instance().free(conn);
                    m_connections[i] = nullptr;
                    break;
                }
            }
        }
    }

    QC::isize TCP::send(TCPConnection *conn, const void *data, QC::usize length)
    {
        if (!conn || conn->state != TCPState::Established)
            return -1;

        // For simplicity, send data in one segment (real implementation would segment)
        QC::usize toSend = length;
        if (toSend > conn->sendWindow)
            toSend = conn->sendWindow;

        sendSegment(conn, TCPFlags::PSH | TCPFlags::ACK, data, toSend);
        conn->sendNext += static_cast<QC::u32>(toSend);

        return static_cast<QC::isize>(toSend);
    }

    QC::isize TCP::receive(TCPConnection *conn, void *buffer, QC::usize length)
    {
        if (!conn || conn->state != TCPState::Established)
            return -1;

        // In a real implementation, we'd read from the receive buffer
        // For now, this is a placeholder
        (void)buffer;
        (void)length;

        return 0;
    }

    void TCP::receivePacket(IPv4Address source, const void *data, QC::usize length)
    {
        if (length < sizeof(TCPHeader))
            return;

        const auto *header = static_cast<const TCPHeader *>(data);

        QC::u16 destPort = ntohs(header->destPort);
        QC::u16 srcPort = ntohs(header->sourcePort);

        // Find matching connection
        TCPConnection *conn = findConnection(source, srcPort, destPort);

        // Check for listening socket if no established connection
        if (!conn)
        {
            for (QC::usize i = 0; i < MAX_CONNECTIONS; i++)
            {
                if (m_connections[i] &&
                    m_connections[i]->state == TCPState::Listen &&
                    m_connections[i]->localPort == destPort)
                {
                    conn = m_connections[i];
                    break;
                }
            }
        }

        if (!conn)
        {
            // Send RST for unknown connection
            return;
        }

        // Get data offset
        QC::u8 dataOffset = (header->dataOffset >> 4) & 0x0F;
        QC::usize headerLen = dataOffset * 4;
        const void *payload = static_cast<const QC::u8 *>(data) + headerLen;
        QC::usize payloadLen = length - headerLen;

        processSegment(conn, header, payload, payloadLen);
    }

    void TCP::sendSegment(TCPConnection *conn, QC::u8 flags,
                          const void *data, QC::usize length)
    {
        QC::usize segmentLen = sizeof(TCPHeader) + length;
        QC::u8 *segment = static_cast<QC::u8 *>(QK::Memory::Heap::instance().allocate(segmentLen));
        if (!segment)
            return;

        auto *header = reinterpret_cast<TCPHeader *>(segment);
        header->sourcePort = htons(conn->localPort);
        header->destPort = htons(conn->remotePort);
        header->seqNumber = htonl(conn->sendNext);
        header->ackNumber = htonl(conn->recvNext);
        header->dataOffset = (5 << 4); // 5 words, no options
        header->flags = flags;
        header->window = htons(static_cast<QC::u16>(conn->recvWindow));
        header->checksum = 0;
        header->urgentPointer = 0;

        if (length > 0)
        {
            memcpy(segment + sizeof(TCPHeader), data, length);
        }

        // Calculate checksum with pseudo-header
        header->checksum = calculateChecksum(conn->localAddr, conn->remoteAddr,
                                             segment, segmentLen);

        Stack::instance().ip()->sendPacket(conn->remoteAddr,
                                           static_cast<QC::u8>(Protocol::TCP),
                                           segment, segmentLen);

        QK::Memory::Heap::instance().free(segment);
    }

    void TCP::processSegment(TCPConnection *conn, const TCPHeader *header,
                             const void *data, QC::usize length)
    {
        QC::u8 flags = header->flags;
        QC::u32 seqNum = ntohl(header->seqNumber);
        QC::u32 ackNum = ntohl(header->ackNumber);

        switch (conn->state)
        {
        case TCPState::Listen:
            if (flags & TCPFlags::SYN)
            {
                conn->remoteAddr.value = ntohl(header->sourcePort); // This is wrong, need source IP
                conn->remotePort = ntohs(header->sourcePort);
                conn->recvNext = seqNum + 1;
                conn->sendUnacked = 2000;
                conn->sendNext = 2000;

                sendSegment(conn, TCPFlags::SYN | TCPFlags::ACK, nullptr, 0);
                conn->sendNext++;
                conn->state = TCPState::SynReceived;
            }
            break;

        case TCPState::SynSent:
            if ((flags & (TCPFlags::SYN | TCPFlags::ACK)) == (TCPFlags::SYN | TCPFlags::ACK))
            {
                conn->recvNext = seqNum + 1;
                conn->sendUnacked = ackNum;

                sendSegment(conn, TCPFlags::ACK, nullptr, 0);
                conn->state = TCPState::Established;
            }
            break;

        case TCPState::SynReceived:
            if (flags & TCPFlags::ACK)
            {
                conn->sendUnacked = ackNum;
                conn->state = TCPState::Established;
            }
            break;

        case TCPState::Established:
            if (flags & TCPFlags::FIN)
            {
                conn->recvNext = seqNum + 1;
                sendSegment(conn, TCPFlags::ACK, nullptr, 0);
                conn->state = TCPState::CloseWait;
            }
            else if (flags & TCPFlags::ACK)
            {
                conn->sendUnacked = ackNum;

                // Process data
                if (length > 0)
                {
                    // Copy to receive buffer (simplified)
                    conn->recvNext += static_cast<QC::u32>(length);
                    sendSegment(conn, TCPFlags::ACK, nullptr, 0);
                }
            }
            break;

        case TCPState::FinWait1:
            if ((flags & TCPFlags::ACK) && (flags & TCPFlags::FIN))
            {
                conn->recvNext = seqNum + 1;
                sendSegment(conn, TCPFlags::ACK, nullptr, 0);
                conn->state = TCPState::TimeWait;
            }
            else if (flags & TCPFlags::ACK)
            {
                conn->state = TCPState::FinWait2;
            }
            else if (flags & TCPFlags::FIN)
            {
                conn->recvNext = seqNum + 1;
                sendSegment(conn, TCPFlags::ACK, nullptr, 0);
                conn->state = TCPState::Closing;
            }
            break;

        case TCPState::FinWait2:
            if (flags & TCPFlags::FIN)
            {
                conn->recvNext = seqNum + 1;
                sendSegment(conn, TCPFlags::ACK, nullptr, 0);
                conn->state = TCPState::TimeWait;
            }
            break;

        case TCPState::Closing:
            if (flags & TCPFlags::ACK)
            {
                conn->state = TCPState::TimeWait;
            }
            break;

        case TCPState::LastAck:
            if (flags & TCPFlags::ACK)
            {
                conn->state = TCPState::Closed;
            }
            break;

        default:
            break;
        }

        (void)data; // Data processing would happen above in Established state
    }

    QC::u16 TCP::allocatePort()
    {
        QC::u16 port = m_nextPort++;
        if (m_nextPort >= 65535)
            m_nextPort = 49152;
        return port;
    }

    TCPConnection *TCP::findConnection(IPv4Address remoteAddr, QC::u16 remotePort,
                                       QC::u16 localPort)
    {
        for (QC::usize i = 0; i < MAX_CONNECTIONS; i++)
        {
            if (m_connections[i] &&
                m_connections[i]->remoteAddr == remoteAddr &&
                m_connections[i]->remotePort == remotePort &&
                m_connections[i]->localPort == localPort &&
                m_connections[i]->state != TCPState::Listen)
            {
                return m_connections[i];
            }
        }
        return nullptr;
    }

    QC::u16 TCP::calculateChecksum(IPv4Address srcAddr, IPv4Address destAddr,
                                   const void *segment, QC::usize length)
    {
        QC::u32 sum = 0;

        // Pseudo-header (RFC 793): src IP, dst IP, zero, protocol, TCP length.
        QC::u8 pseudo[12];
        pseudo[0] = srcAddr.octets[0];
        pseudo[1] = srcAddr.octets[1];
        pseudo[2] = srcAddr.octets[2];
        pseudo[3] = srcAddr.octets[3];
        pseudo[4] = destAddr.octets[0];
        pseudo[5] = destAddr.octets[1];
        pseudo[6] = destAddr.octets[2];
        pseudo[7] = destAddr.octets[3];
        pseudo[8] = 0;
        pseudo[9] = static_cast<QC::u8>(Protocol::TCP);
        const QC::u16 tcpLen = htons(static_cast<QC::u16>(length));
        pseudo[10] = static_cast<QC::u8>(tcpLen >> 8);
        pseudo[11] = static_cast<QC::u8>(tcpLen & 0xFF);

        checksumAddBytes(sum, pseudo, sizeof(pseudo));
        checksumAddBytes(sum, segment, length);

        // Fold
        while (sum >> 16)
        {
            sum = (sum & 0xFFFF) + (sum >> 16);
        }

        const QC::u16 result = static_cast<QC::u16>(~sum);
        return htons(result);
    }

} // namespace QNet
