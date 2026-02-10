// QNetwork IP - Internet Protocol layer implementation
// Namespace: QNet

#include "QNetIP.h"
#include "QNetStack.h"
#include "QNetEthernet.h"
#include "QNetTCP.h"
#include "QNetUDP.h"
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

    IP::IP()
        : m_identification(0)
    {
        m_address.value = 0;
        m_subnetMask.value = 0;
        m_gateway.value = 0;
    }

    IP::~IP()
    {
    }

    void IP::initialize()
    {
        m_identification = 1;
    }

    void IP::receivePacket(const void *data, QC::usize length)
    {
        if (length < sizeof(IPv4Header))
            return;

        const auto *header = static_cast<const IPv4Header *>(data);

        // Verify version (must be 4)
        QC::u8 version = (header->versionIHL >> 4) & 0x0F;
        if (version != 4)
            return;

        // Get header length (in 32-bit words)
        QC::u8 ihl = header->versionIHL & 0x0F;
        QC::usize headerLen = ihl * 4;
        if (headerLen < 20 || headerLen > length)
            return;

        // Verify checksum
        if (checksum(header, headerLen) != 0)
            return;

        // Check destination
        if (header->destination.value != m_address.value &&
            !header->destination.isBroadcast() &&
            !header->destination.isMulticast())
        {
            return; // Not for us
        }

        // Get payload
        const void *payload = static_cast<const QC::u8 *>(data) + headerLen;
        QC::usize payloadLen = ntohs(header->totalLength) - headerLen;

        // Dispatch based on protocol
        switch (header->protocol)
        {
        case static_cast<QC::u8>(Protocol::ICMP):
            handleICMP(header->source, payload, payloadLen);
            break;

        case static_cast<QC::u8>(Protocol::TCP):
            Stack::instance().tcp()->receivePacket(header->source, payload, payloadLen);
            break;

        case static_cast<QC::u8>(Protocol::UDP):
            Stack::instance().udp()->receivePacket(header->source, payload, payloadLen);
            break;
        }
    }

    void IP::sendPacket(IPv4Address dest, QC::u8 protocol,
                        const void *payload, QC::usize length)
    {
        // Build IP header
        QC::usize totalLen = sizeof(IPv4Header) + length;
        QC::u8 *packet = static_cast<QC::u8 *>(QK::Memory::Heap::instance().allocate(totalLen));
        if (!packet)
            return;

        auto *header = reinterpret_cast<IPv4Header *>(packet);
        header->versionIHL = 0x45; // IPv4, 5 words (20 bytes)
        header->tos = 0;
        header->totalLength = htons(static_cast<QC::u16>(totalLen));
        header->identification = htons(m_identification++);
        header->flagsFragment = htons(0x4000); // Don't fragment
        header->ttl = 64;
        header->protocol = protocol;
        header->headerChecksum = 0;
        header->source = m_address;
        header->destination = dest;

        // Calculate checksum
        header->headerChecksum = checksum(header, sizeof(IPv4Header));

        // Copy payload
        memcpy(packet + sizeof(IPv4Header), payload, length);

        // Determine next hop
        IPv4Address nextHopAddr = nextHop(dest);

        // Resolve MAC and send
        MACAddress destMAC;
        if (Stack::instance().ethernet()->resolveMAC(nextHopAddr.value, &destMAC))
        {
            Stack::instance().ethernet()->sendFrame(destMAC, EtherType::IPv4, packet, totalLen);
        }
        // If MAC resolution fails, packet is queued by ARP (in a real implementation)

        QK::Memory::Heap::instance().free(packet);
    }

    void IP::sendICMP(IPv4Address dest, QC::u8 type, QC::u8 code,
                      const void *payload, QC::usize length)
    {
        QC::usize icmpLen = sizeof(ICMPHeader) + length;
        QC::u8 *icmpPacket = static_cast<QC::u8 *>(QK::Memory::Heap::instance().allocate(icmpLen));
        if (!icmpPacket)
            return;

        auto *icmp = reinterpret_cast<ICMPHeader *>(icmpPacket);
        icmp->type = type;
        icmp->code = code;
        icmp->checksum = 0;
        icmp->rest = 0;

        if (length > 0)
        {
            memcpy(icmpPacket + sizeof(ICMPHeader), payload, length);
        }

        // Calculate ICMP checksum
        icmp->checksum = checksum(icmpPacket, icmpLen);

        sendPacket(dest, static_cast<QC::u8>(Protocol::ICMP), icmpPacket, icmpLen);

        QK::Memory::Heap::instance().free(icmpPacket);
    }

    bool IP::isLocal(IPv4Address addr) const
    {
        return (addr.value & m_subnetMask.value) == (m_address.value & m_subnetMask.value);
    }

    IPv4Address IP::nextHop(IPv4Address dest) const
    {
        // If destination is on local network, send directly
        if (isLocal(dest))
        {
            return dest;
        }
        // Otherwise, send to gateway
        return m_gateway;
    }

    QC::u16 IP::checksum(const void *data, QC::usize length)
    {
        const QC::u16 *words = static_cast<const QC::u16 *>(data);
        QC::u32 sum = 0;

        // Sum all 16-bit words
        while (length > 1)
        {
            sum += *words++;
            length -= 2;
        }

        // Add odd byte if present
        if (length == 1)
        {
            sum += *reinterpret_cast<const QC::u8 *>(words);
        }

        // Fold 32-bit sum to 16 bits
        while (sum >> 16)
        {
            sum = (sum & 0xFFFF) + (sum >> 16);
        }

        return static_cast<QC::u16>(~sum);
    }

    void IP::handleICMP(IPv4Address source, const void *data, QC::usize length)
    {
        if (length < sizeof(ICMPHeader))
            return;

        const auto *icmp = static_cast<const ICMPHeader *>(data);

        // ICMP Echo Request (ping)
        if (icmp->type == 8 && icmp->code == 0)
        {
            // Send Echo Reply
            const void *echoData = static_cast<const QC::u8 *>(data) + sizeof(ICMPHeader);
            QC::usize echoLen = length - sizeof(ICMPHeader);

            // Build reply with same data
            QC::usize replyLen = length;
            QC::u8 *reply = static_cast<QC::u8 *>(QK::Memory::Heap::instance().allocate(replyLen));
            if (!reply)
                return;

            auto *replyICMP = reinterpret_cast<ICMPHeader *>(reply);
            replyICMP->type = 0; // Echo Reply
            replyICMP->code = 0;
            replyICMP->checksum = 0;
            replyICMP->rest = icmp->rest; // Copy identifier and sequence

            if (echoLen > 0)
            {
                memcpy(reply + sizeof(ICMPHeader), echoData, echoLen);
            }

            replyICMP->checksum = checksum(reply, replyLen);

            sendPacket(source, static_cast<QC::u8>(Protocol::ICMP), reply, replyLen);

            QK::Memory::Heap::instance().free(reply);
        }
    }

} // namespace QNet
