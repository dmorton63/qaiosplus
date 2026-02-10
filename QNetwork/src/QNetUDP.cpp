// QNetwork UDP - User Datagram Protocol implementation
// Namespace: QNet

#include "QNetUDP.h"
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

    // UDP pseudo-header for checksum
    struct UDPPseudoHeader
    {
        QC::u32 sourceIP;
        QC::u32 destIP;
        QC::u8 zero;
        QC::u8 protocol;
        QC::u16 udpLength;
    } __attribute__((packed));

    UDP::UDP()
        : m_nextPort(49152)
    {
        memset(m_bindings, 0, sizeof(m_bindings));
    }

    UDP::~UDP()
    {
        for (QC::usize i = 0; i < MAX_BINDINGS; i++)
        {
            if (m_bindings[i])
            {
                // Free receive queue
                auto *dgram = m_bindings[i]->recvQueue;
                while (dgram)
                {
                    auto *next = dgram->next;
                    if (dgram->data)
                        QK::Memory::Heap::instance().free(dgram->data);
                    QK::Memory::Heap::instance().free(dgram);
                    dgram = next;
                }
                QK::Memory::Heap::instance().free(m_bindings[i]);
            }
        }
    }

    void UDP::initialize()
    {
        m_nextPort = 49152;
    }

    UDPBinding *UDP::bind(QC::u16 port)
    {
        // Check if port already bound
        if (findBinding(port))
            return nullptr;

        // Find free slot
        QC::usize slot = MAX_BINDINGS;
        for (QC::usize i = 0; i < MAX_BINDINGS; i++)
        {
            if (!m_bindings[i])
            {
                slot = i;
                break;
            }
        }
        if (slot == MAX_BINDINGS)
            return nullptr;

        // Create binding
        auto *binding = static_cast<UDPBinding *>(QK::Memory::Heap::instance().allocate(sizeof(UDPBinding)));
        if (!binding)
            return nullptr;

        binding->port = port;
        binding->active = true;
        binding->recvQueue = nullptr;

        m_bindings[slot] = binding;

        return binding;
    }

    void UDP::unbind(UDPBinding *binding)
    {
        if (!binding)
            return;

        for (QC::usize i = 0; i < MAX_BINDINGS; i++)
        {
            if (m_bindings[i] == binding)
            {
                // Free receive queue
                auto *dgram = binding->recvQueue;
                while (dgram)
                {
                    auto *next = dgram->next;
                    if (dgram->data)
                        QK::Memory::Heap::instance().free(dgram->data);
                    QK::Memory::Heap::instance().free(dgram);
                    dgram = next;
                }
                QK::Memory::Heap::instance().free(binding);
                m_bindings[i] = nullptr;
                break;
            }
        }
    }

    QC::Status UDP::send(IPv4Address dest, QC::u16 destPort, QC::u16 sourcePort,
                         const void *data, QC::usize length)
    {
        QC::usize packetLen = sizeof(UDPHeader) + length;
        QC::u8 *packet = static_cast<QC::u8 *>(QK::Memory::Heap::instance().allocate(packetLen));
        if (!packet)
            return QC::Status::OutOfMemory;

        auto *header = reinterpret_cast<UDPHeader *>(packet);
        header->sourcePort = htons(sourcePort);
        header->destPort = htons(destPort);
        header->length = htons(static_cast<QC::u16>(packetLen));
        header->checksum = 0; // Checksum optional for UDP over IPv4

        if (length > 0)
        {
            memcpy(packet + sizeof(UDPHeader), data, length);
        }

        // Calculate checksum
        header->checksum = calculateChecksum(Stack::instance().ip()->address(),
                                             dest, packet, packetLen);

        Stack::instance().ip()->sendPacket(dest, static_cast<QC::u8>(Protocol::UDP),
                                           packet, packetLen);

        QK::Memory::Heap::instance().free(packet);

        return QC::Status::Success;
    }

    QC::isize UDP::receive(UDPBinding *binding, void *buffer, QC::usize length,
                           IPv4Address *source, QC::u16 *sourcePort)
    {
        if (!binding || !binding->active)
            return -1;

        // Get first datagram from queue
        auto *dgram = binding->recvQueue;
        if (!dgram)
            return 0; // No data available

        // Remove from queue
        binding->recvQueue = dgram->next;

        // Copy data
        QC::usize toCopy = dgram->length;
        if (toCopy > length)
            toCopy = length;

        memcpy(buffer, dgram->data, toCopy);

        if (source)
            *source = dgram->source;
        if (sourcePort)
            *sourcePort = dgram->sourcePort;

        // Free datagram
        if (dgram->data)
            QK::Memory::Heap::instance().free(dgram->data);
        QK::Memory::Heap::instance().free(dgram);

        return static_cast<QC::isize>(toCopy);
    }

    void UDP::receivePacket(IPv4Address source, const void *data, QC::usize length)
    {
        if (length < sizeof(UDPHeader))
            return;

        const auto *header = static_cast<const UDPHeader *>(data);

        QC::u16 destPort = ntohs(header->destPort);
        QC::u16 srcPort = ntohs(header->sourcePort);
        QC::u16 udpLen = ntohs(header->length);

        if (udpLen < sizeof(UDPHeader) || udpLen > length)
            return;

        // Find binding for this port
        UDPBinding *binding = findBinding(destPort);
        if (!binding || !binding->active)
            return;

        // Extract payload
        const void *payload = static_cast<const QC::u8 *>(data) + sizeof(UDPHeader);
        QC::usize payloadLen = udpLen - sizeof(UDPHeader);

        // Create datagram entry
        auto *dgram = static_cast<UDPBinding::Datagram *>(
            QK::Memory::Heap::instance().allocate(sizeof(UDPBinding::Datagram)));
        if (!dgram)
            return;

        dgram->source = source;
        dgram->sourcePort = srcPort;
        dgram->length = payloadLen;
        dgram->next = nullptr;

        if (payloadLen > 0)
        {
            dgram->data = static_cast<QC::u8 *>(QK::Memory::Heap::instance().allocate(payloadLen));
            if (!dgram->data)
            {
                QK::Memory::Heap::instance().free(dgram);
                return;
            }
            memcpy(dgram->data, payload, payloadLen);
        }
        else
        {
            dgram->data = nullptr;
        }

        // Add to receive queue
        if (!binding->recvQueue)
        {
            binding->recvQueue = dgram;
        }
        else
        {
            auto *tail = binding->recvQueue;
            while (tail->next)
                tail = tail->next;
            tail->next = dgram;
        }
    }

    UDPBinding *UDP::findBinding(QC::u16 port)
    {
        for (QC::usize i = 0; i < MAX_BINDINGS; i++)
        {
            if (m_bindings[i] && m_bindings[i]->port == port)
            {
                return m_bindings[i];
            }
        }
        return nullptr;
    }

    QC::u16 UDP::allocatePort()
    {
        QC::u16 port = m_nextPort++;
        if (m_nextPort >= 65535)
            m_nextPort = 49152;
        return port;
    }

    QC::u16 UDP::calculateChecksum(IPv4Address srcAddr, IPv4Address destAddr,
                                   const void *packet, QC::usize length)
    {
        QC::u32 sum = 0;

        // Pseudo-header
        UDPPseudoHeader pseudo;
        pseudo.sourceIP = srcAddr.value;
        pseudo.destIP = destAddr.value;
        pseudo.zero = 0;
        pseudo.protocol = static_cast<QC::u8>(Protocol::UDP);
        pseudo.udpLength = htons(static_cast<QC::u16>(length));

        const QC::u16 *words = reinterpret_cast<const QC::u16 *>(&pseudo);
        for (QC::usize i = 0; i < sizeof(pseudo) / 2; i++)
        {
            sum += words[i];
        }

        // UDP packet
        words = static_cast<const QC::u16 *>(packet);
        QC::usize len = length;
        while (len > 1)
        {
            sum += *words++;
            len -= 2;
        }
        if (len == 1)
        {
            sum += *reinterpret_cast<const QC::u8 *>(words);
        }

        // Fold
        while (sum >> 16)
        {
            sum = (sum & 0xFFFF) + (sum >> 16);
        }

        QC::u16 result = static_cast<QC::u16>(~sum);

        // UDP checksum of 0 means no checksum, use 0xFFFF instead
        return result == 0 ? 0xFFFF : result;
    }

} // namespace QNet
