/*

    Construct a TCP packet based upon a template.

    The (eventual) idea of this module is to make this scanner extensible
    by providing an arbitrary packet template. Thus, the of this module
    is to take an existing packet template, parse it, then make
    appropriate changes.
*/
#include "templ-pkt.h"
#include "templ-port.h"
#include "proto-preprocess.h"
#include "string_s.h"
#include "pixie-timer.h"
#include "proto-preprocess.h"
#include "logger.h"
#include "templ-payloads.h"
#include "unusedparm.h"

#include <assert.h>
#include <string.h>
#include <stdlib.h>

static unsigned char default_tcp_template[] =
    "\0\1\2\3\4\5"  /* Ethernet: destination */
    "\6\7\x8\x9\xa\xb"  /* Ethernet: source */
    "\x08\x00"      /* Etenrent type: IPv4 */
    "\x45"          /* IP type */
    "\x00"
    "\x00\x28"      /* total length = 40 bytes */
    "\x00\x00"      /* identification */
    "\x00\x00"      /* fragmentation flags */
    "\xFF\x06"      /* TTL=255, proto=TCP */
    "\xFF\xFF"      /* checksum */
    "\0\0\0\0"      /* source address */
    "\0\0\0\0"      /* destination address */

    "\xfe\xdc"      /* source port */
    "\0\0"          /* destination port */
    "\0\0\0\0"      /* sequence number */
    "\0\0\0\0"      /* ack number */
    "\x50"          /* header length */
    "\x02"          /* SYN */
    "\x0\x0"        /* window */
    "\xFF\xFF"      /* checksum */
    "\x00\x00"      /* urgent pointer */
;

static unsigned char default_udp_template[] =
    "\0\1\2\3\4\5"  /* Ethernet: destination */
    "\6\7\x8\x9\xa\xb"  /* Ethernet: source */
    "\x08\x00"      /* Etenrent type: IPv4 */
    "\x45"          /* IP type */
    "\x00"
    "\x00\x1c"      /* total length = 28 bytes */
    "\x00\x00"      /* identification */
    "\x00\x00"      /* fragmentation flags */
    "\xFF\x11"      /* TTL=255, proto=UDP */
    "\xFF\xFF"      /* checksum */
    "\0\0\0\0"      /* source address */
    "\0\0\0\0"      /* destination address */

    "\xfe\xdc"      /* source port */
    "\x00\x00"      /* destination port */
    "\x00\x08"      /* length */
    "\x00\x00"      /* checksum */
;

static unsigned char default_sctp_template[] =
    "\0\1\2\3\4\5"  /* Ethernet: destination */
    "\6\7\x8\x9\xa\xb"  /* Ethernet: source */
    "\x08\x00"      /* Etenrent type: IPv4 */
    "\x45"          /* IP type */
    "\x00"
    "\x00\x1c"      /* total length = 40 bytes */
    "\x00\x00"      /* identification */
    "\x00\x00"      /* fragmentation flags */
    "\xFF\x11"      /* TTL=255, proto=UDP */
    "\xFF\xFF"      /* checksum */
    "\0\0\0\0"      /* source address */
    "\0\0\0\0"      /* destination address */

    "\xfe\xdc"      /* source port */
    "\0\0"          /* destination port */
    "\0\0\0\0"      /* checksum */
    "\0\0\0\0"      /* length */
;


static unsigned char default_icmp_ping_template[] =
    "\0\1\2\3\4\5"  /* Ethernet: destination */
    "\6\7\x8\x9\xa\xb"  /* Ethernet: source */
    "\x08\x00"      /* Etherent type: IPv4 */
    "\x45"          /* IP type */
    "\x00"
    "\x00\x4c"      /* total length = 76 bytes */
    "\x00\x00"      /* identification */
    "\x00\x00"      /* fragmentation flags */
    "\xFF\x01"      /* TTL=255, proto=UDP */
    "\xFF\xFF"      /* checksum */
    "\0\0\0\0"      /* source address */
    "\0\0\0\0"      /* destination address */

    "\x08\x00"      /* Ping Request */
    "\x00\x00"      /* checksum */

    "\x00\x00\x00\x00" /* ID, seqno */
    
    "\x08\x09\x0a\x0b" /* payload */
    "\x0c\x0d\x0e\x0f"
    "\x10\x11\x12\x13"
    "\x14\x15\x16\x17"
    "\x18\x19\x1a\x1b"
    "\x1c\x1d\x1e\x1f"
    "\x20\x21\x22\x23"
    "\x24\x25\x26\x27"
    "\x28\x29\x2a\x2b"
    "\x2c\x2d\x2e\x2f"
    "\x30\x31\x32\x33"
    "\x34\x35\x36\x37"
;

static unsigned char default_icmp_timestamp_template[] =
"\0\1\2\3\4\5"  /* Ethernet: destination */
"\6\7\x8\x9\xa\xb"  /* Ethernet: source */
"\x08\x00"      /* Etenrent type: IPv4 */
"\x45"          /* IP type */
"\x00"
"\x00\x28"      /* total length = 84 bytes */
"\x00\x00"      /* identification */
"\x00\x00"      /* fragmentation flags */
"\xFF\x01"      /* TTL=255, proto=UDP */
"\xFF\xFF"      /* checksum */
"\0\0\0\0"      /* source address */
"\0\0\0\0"      /* destination address */

"\x0d\x00"  /* timestamp request */
"\x00\x00"  /* checksum */
"\x00\x00"  /* identifier */
"\x00\x00"  /* sequence number */
"\x00\x00\x00\x00"
"\x00\x00\x00\x00"
"\x00\x00\x00\x00"
;


static unsigned char default_arp_template[] =
    "\xff\xff\xff\xff\xff\xff"  /* Ethernet: destination */
    "\x00\x00\x00\x00\x00\x00"  /* Ethernet: source */
    "\x08\x06"      /* Ethernet type: ARP */
	"\x00\x01" /* hardware = Ethernet */
    "\x08\x00" /* protocol = IPv4 */
    "\x06\x04" /* MAC length = 6, IPv4 length = 4 */
    "\x00\x01" /* opcode = request */
  
	"\x00\x00\x00\x00\x00\x00"
	"\x00\x00\x00\x00"

	"\x00\x00\x00\x00\x00\x00"
	"\x00\x00\x00\x00"
;


/***************************************************************************
 ***************************************************************************/
static unsigned
ip_checksum(struct TemplatePacket *tmpl)
{
    unsigned xsum = 0;
    unsigned i;

    xsum = 0;
    for (i=tmpl->offset_ip; i<tmpl->offset_tcp; i += 2) {
        xsum += tmpl->packet[i]<<8 | tmpl->packet[i+1];
    }
    xsum = (xsum & 0xFFFF) + (xsum >> 16);
    xsum = (xsum & 0xFFFF) + (xsum >> 16);

    return xsum;
}

/***************************************************************************
 ***************************************************************************/
unsigned
tcp_checksum2(const unsigned char *px, unsigned offset_ip,
              unsigned offset_tcp, size_t tcp_length)
{
    uint64_t xsum = 0;
    unsigned i;
    
    /* pseudo checksum */
    xsum = 6;
    xsum += tcp_length;
    xsum += px[offset_ip + 12] << 8 | px[offset_ip + 13];
    xsum += px[offset_ip + 14] << 8 | px[offset_ip + 15];
    xsum += px[offset_ip + 16] << 8 | px[offset_ip + 17];
    xsum += px[offset_ip + 18] << 8 | px[offset_ip + 19];
    
    /* tcp checksum */
    for (i=0; i<tcp_length; i += 2) {
        xsum += px[offset_tcp + i]<<8 | px[offset_tcp + i + 1];
    }
    
    xsum -= (tcp_length & 1) * px[offset_tcp + i - 1]; /* yea I know going off end of packet is bad so sue me */
    xsum = (xsum & 0xFFFF) + (xsum >> 16);
    xsum = (xsum & 0xFFFF) + (xsum >> 16);
    xsum = (xsum & 0xFFFF) + (xsum >> 16);
    
    return (unsigned)xsum;
}

/***************************************************************************
 ***************************************************************************/
/***************************************************************************
 ***************************************************************************/
static unsigned
tcp_checksum(struct TemplatePacket *tmpl)
{
    const unsigned char *px = tmpl->packet;
    unsigned xsum = 0;
    unsigned i;
    
    /* pseudo checksum */
    xsum = 6;
    xsum += tmpl->offset_app - tmpl->offset_tcp;
    xsum += px[tmpl->offset_ip + 12] << 8 | px[tmpl->offset_ip + 13];
    xsum += px[tmpl->offset_ip + 14] << 8 | px[tmpl->offset_ip + 15];
    xsum += px[tmpl->offset_ip + 16] << 8 | px[tmpl->offset_ip + 17];
    xsum += px[tmpl->offset_ip + 18] << 8 | px[tmpl->offset_ip + 19];
    
    /* tcp checksum */
    for (i=tmpl->offset_tcp; i<tmpl->offset_app; i += 2) {
        xsum += tmpl->packet[i]<<8 | tmpl->packet[i+1];
    }
    xsum = (xsum & 0xFFFF) + (xsum >> 16);
    xsum = (xsum & 0xFFFF) + (xsum >> 16);
    xsum = (xsum & 0xFFFF) + (xsum >> 16);
    
    return xsum;
}



/***************************************************************************
 ***************************************************************************/
unsigned
udp_checksum2(const unsigned char *px, unsigned offset_ip,
              unsigned offset_tcp, size_t tcp_length)
{
    uint64_t xsum = 0;
    unsigned i;
    
    /* pseudo checksum */
    xsum = 17;
    xsum += tcp_length;
    xsum += px[offset_ip + 12] << 8 | px[offset_ip + 13];
    xsum += px[offset_ip + 14] << 8 | px[offset_ip + 15];
    xsum += px[offset_ip + 16] << 8 | px[offset_ip + 17];
    xsum += px[offset_ip + 18] << 8 | px[offset_ip + 19];
    
    /* tcp checksum */
    for (i=0; i<tcp_length; i += 2) {
        xsum += px[offset_tcp + i]<<8 | px[offset_tcp + i + 1];
    }
    
    xsum -= (tcp_length & 1) * px[offset_tcp + i - 1]; /* yea I know going off end of packet is bad so sue me */
    xsum = (xsum & 0xFFFF) + (xsum >> 16);
    xsum = (xsum & 0xFFFF) + (xsum >> 16);
    xsum = (xsum & 0xFFFF) + (xsum >> 16);
    
    return (unsigned)xsum;
}

/***************************************************************************
 ***************************************************************************/
static unsigned
udp_checksum(struct TemplatePacket *tmpl)
{
    return udp_checksum2(
                         tmpl->packet,
                         tmpl->offset_ip,
                         tmpl->offset_tcp,
                         tmpl->length - tmpl->offset_tcp);
}

/***************************************************************************
 ***************************************************************************/
unsigned
icmp_checksum2(const unsigned char *px,
              unsigned offset_icmp, size_t icmp_length)
{
    uint64_t xsum = 0;
    unsigned i;
    
    for (i=0; i<icmp_length; i += 2) {
        xsum += px[offset_icmp + i]<<8 | px[offset_icmp + i + 1];
    }
    
    xsum -= (icmp_length & 1) * px[offset_icmp + i - 1]; /* yea I know going off end of packet is bad so sue me */
    xsum = (xsum & 0xFFFF) + (xsum >> 16);
    xsum = (xsum & 0xFFFF) + (xsum >> 16);
    xsum = (xsum & 0xFFFF) + (xsum >> 16);
    
    return (unsigned)xsum;
}

/***************************************************************************
 ***************************************************************************/
static unsigned
icmp_checksum(struct TemplatePacket *tmpl)
{
    return icmp_checksum2(
                         tmpl->packet,
                         tmpl->offset_tcp,
                         tmpl->length - tmpl->offset_tcp);
}


/***************************************************************************
 ***************************************************************************/
size_t
tcp_create_packet(
        struct TemplatePacket *tmpl, 
        unsigned ip, unsigned port,
        unsigned seqno, unsigned ackno,
        unsigned flags,
        const unsigned char *payload, size_t payload_length,
        unsigned char *px, size_t px_length)
{
    unsigned ip_id = ip ^ port ^ seqno;
    unsigned offset_ip = tmpl->offset_ip;
    unsigned offset_tcp = tmpl->offset_tcp;
    unsigned offset_payload = offset_tcp + ((tmpl->packet[offset_tcp+12]&0xF0)>>2);
    size_t new_length = offset_payload + payload_length;
    uint64_t xsum;
    size_t ip_len = (offset_payload - offset_ip) + payload_length;
    unsigned old_len;
    
    if (new_length > px_length) {
        fprintf(stderr, "tcp: err generating packet: too much payload\n");
        return 0;
    }

    memcpy(px + 0,              tmpl->packet,   tmpl->length);
    memcpy(px + offset_payload, payload,        payload_length);
    old_len = px[offset_ip+2]<<8 | px[offset_ip+3];
    
    /*
     * Fill in the empty fields in the IP header and then re-calculate
     * the checksum.
     */
    px[offset_ip+2] = (unsigned char)(ip_len>> 8);
    px[offset_ip+3] = (unsigned char)(ip_len & 0xFF);
    px[offset_ip+4] = (unsigned char)(ip_id >> 8);
    px[offset_ip+5] = (unsigned char)(ip_id & 0xFF);
    px[offset_ip+16] = (unsigned char)((ip >> 24) & 0xFF);
    px[offset_ip+17] = (unsigned char)((ip >> 16) & 0xFF);
    px[offset_ip+18] = (unsigned char)((ip >>  8) & 0xFF);
    px[offset_ip+19] = (unsigned char)((ip >>  0) & 0xFF);

    xsum = tmpl->checksum_ip;
    xsum += (ip_id&0xFFFF);
    xsum += ip;
    xsum += ip_len - old_len;
    xsum = (xsum >> 16) + (xsum & 0xFFFF);
    xsum = (xsum >> 16) + (xsum & 0xFFFF);
    xsum = ~xsum;

    px[offset_ip+10] = (unsigned char)(xsum >> 8);
    px[offset_ip+11] = (unsigned char)(xsum & 0xFF);

    /*
     * now do the same for TCP
     */
    px[offset_tcp+ 2] = (unsigned char)(port >> 8);
    px[offset_tcp+ 3] = (unsigned char)(port & 0xFF);
    px[offset_tcp+ 4] = (unsigned char)(seqno >> 24);
    px[offset_tcp+ 5] = (unsigned char)(seqno >> 16);
    px[offset_tcp+ 6] = (unsigned char)(seqno >>  8);
    px[offset_tcp+ 7] = (unsigned char)(seqno >>  0);
    
    px[offset_tcp+ 8] = (unsigned char)(ackno >> 24);
    px[offset_tcp+ 9] = (unsigned char)(ackno >> 16);
    px[offset_tcp+10] = (unsigned char)(ackno >>  8);
    px[offset_tcp+11] = (unsigned char)(ackno >>  0);

    px[offset_tcp+13] = (unsigned char)flags;
    
    px[offset_tcp+14] = (unsigned char)(1200>>8);
    px[offset_tcp+15] = (unsigned char)(1200 & 0xFF);

    px[offset_tcp+16] = (unsigned char)(0 >>  8);
    px[offset_tcp+17] = (unsigned char)(0 >>  0);

    xsum = tcp_checksum2(px, tmpl->offset_ip, tmpl->offset_tcp, 
                         new_length - tmpl->offset_tcp);
    xsum = ~xsum;

    px[offset_tcp+16] = (unsigned char)(xsum >>  8);
    px[offset_tcp+17] = (unsigned char)(xsum >>  0);

    if (new_length < 60) {
        memset(px+new_length, 0, 60-new_length);
        new_length = 60;
    }
    return new_length;
}

/***************************************************************************
 ***************************************************************************/
static void
udp_payload_fixup(struct TemplatePacket *tmpl, unsigned port, unsigned seqno)
{
    const unsigned char *px2 = 0;
    unsigned length2 = 0;
    unsigned source_port2 = 0x1000;
    uint64_t xsum2 = 0;
    unsigned char *px = tmpl->packet;
    SET_COOKIE set_cookie;

    UNUSEDPARM(seqno);

    payloads_lookup(tmpl->payloads,
                    port,
                    &px2,
                    &length2,
                    &source_port2,
                    &xsum2,
                    &set_cookie);

    memcpy( px+tmpl->offset_app,
            px2,
            length2);

    if (set_cookie)
        xsum2 += set_cookie(px+tmpl->offset_app,
                    length2,
                    seqno);

    tmpl->length = tmpl->offset_app + length2;
}


/***************************************************************************
 * Here we take a packet template, parse it, then make it easier to work
 * with.
 ***************************************************************************/
void
template_set_target(
    struct TemplateSet *tmplset, 
    unsigned ip, unsigned port, 
    unsigned seqno)
{
    unsigned char *px;
    unsigned offset_ip;
    unsigned offset_tcp;
    uint64_t xsum;
    unsigned ip_id;
    struct TemplatePacket *tmpl = NULL;
    
    /*
     * Find out which packet template to use. This is because we can
     * simultaneously scan for both TCP and UDP (and others). We've
     * just overloaded the "port" field to signal which protocol we
     * are using
     */
    if (port < Templ_TCP + 65536)
        tmpl = &tmplset->pkts[Proto_TCP];
    else if (port < Templ_UDP + 65536) {
        tmpl = &tmplset->pkts[Proto_UDP];
        port &= 0xFFFF;
        udp_payload_fixup(tmpl, port, seqno);
    } else if (port < Templ_SCTP + 65536) {
        tmpl = &tmplset->pkts[Proto_SCTP];
        port &= 0xFFFF;
    } else if (port == Templ_ICMP_echo) {
        tmpl = &tmplset->pkts[Proto_ICMP_ping];
        port = 1;
    } else if (port == Templ_ICMP_timestamp) {
        tmpl = &tmplset->pkts[Proto_ICMP_timestamp];
        port = 1;
    } else if (port == Templ_ARP) {
        tmpl = &tmplset->pkts[Proto_ARP];
        port = 1;
		px = tmpl->packet + tmpl->offset_ip;
		px[24] = (unsigned char)((ip >> 24) & 0xFF);
		px[25] = (unsigned char)((ip >> 16) & 0xFF);
		px[26] = (unsigned char)((ip >>  8) & 0xFF);
		px[27] = (unsigned char)((ip >>  0) & 0xFF);
		tmplset->px = tmpl->packet;
		tmplset->length = tmpl->length;
		return;
    } else {
        return;
    }

    /* Create some shorter local variables to work with */
    px = tmpl->packet;
    offset_ip = tmpl->offset_ip;
    offset_tcp = tmpl->offset_tcp;
    ip_id = ip ^ port ^ seqno;

    /*
     * Fill in the empty fields in the IP header and then re-calculate
     * the checksum.
     */
    {
        unsigned total_length = tmpl->length - tmpl->offset_ip;
        px[offset_ip+2] = (unsigned char)(total_length>>8);
        px[offset_ip+3] = (unsigned char)(total_length>>0);
    }
    px[offset_ip+4] = (unsigned char)(ip_id >> 8);
    px[offset_ip+5] = (unsigned char)(ip_id & 0xFF);
    px[offset_ip+16] = (unsigned char)((ip >> 24) & 0xFF);
    px[offset_ip+17] = (unsigned char)((ip >> 16) & 0xFF);
    px[offset_ip+18] = (unsigned char)((ip >>  8) & 0xFF);
    px[offset_ip+19] = (unsigned char)((ip >>  0) & 0xFF);

    xsum = tmpl->checksum_ip;
    xsum += tmpl->length - tmpl->offset_app;
    xsum += (ip_id&0xFFFF);
    xsum += ip;
    xsum = (xsum >> 16) + (xsum & 0xFFFF);
    xsum = (xsum >> 16) + (xsum & 0xFFFF);
    xsum = ~xsum;

    px[offset_ip+10] = (unsigned char)(xsum >> 8);
    px[offset_ip+11] = (unsigned char)(xsum & 0xFF);


    /*
     * Now do the checksum for the higher layer protocols
     */
    xsum = 0;
    switch (tmpl->proto) {
    case Proto_TCP:
        px[offset_tcp+ 2] = (unsigned char)(port >> 8);
        px[offset_tcp+ 3] = (unsigned char)(port & 0xFF);
        px[offset_tcp+ 4] = (unsigned char)(seqno >> 24);
        px[offset_tcp+ 5] = (unsigned char)(seqno >> 16);
        px[offset_tcp+ 6] = (unsigned char)(seqno >>  8);
        px[offset_tcp+ 7] = (unsigned char)(seqno >>  0);

        xsum += (uint64_t)tmpl->checksum_tcp
                + (uint64_t)ip
                + (uint64_t)port
                + (uint64_t)seqno;
        xsum = (xsum >> 16) + (xsum & 0xFFFF);
        xsum = (xsum >> 16) + (xsum & 0xFFFF);
        xsum = (xsum >> 16) + (xsum & 0xFFFF);
        xsum = ~xsum;

        px[offset_tcp+16] = (unsigned char)(xsum >>  8);
        px[offset_tcp+17] = (unsigned char)(xsum >>  0);
        break;
    case Proto_UDP:
        px[offset_tcp+ 2] = (unsigned char)(port >> 8);
        px[offset_tcp+ 3] = (unsigned char)(port & 0xFF);
        px[offset_tcp+ 4] = (unsigned char)((tmpl->length - tmpl->offset_app + 8)>>8);
        px[offset_tcp+ 5] = (unsigned char)((tmpl->length - tmpl->offset_app + 8)&0xFF);
        
        px[offset_tcp+6] = (unsigned char)(0);
        px[offset_tcp+7] = (unsigned char)(0);
        xsum = udp_checksum(tmpl);
        /*xsum += (uint64_t)tmpl->checksum_tcp
                + (uint64_t)ip
                + (uint64_t)port
                + (uint64_t)2*(tmpl->length - tmpl->offset_app);
        xsum = (xsum >> 16) + (xsum & 0xFFFF);
        xsum = (xsum >> 16) + (xsum & 0xFFFF);
        xsum = (xsum >> 16) + (xsum & 0xFFFF);
        printf("%04x\n", xsum);*/
        xsum = ~xsum;
        px[offset_tcp+6] = (unsigned char)(xsum >>  8);
        px[offset_tcp+7] = (unsigned char)(xsum >>  0);
        break;
    case Proto_SCTP:
        break;
    case Proto_ICMP_ping:
    case Proto_ICMP_timestamp:
            px[offset_tcp+ 4] = (unsigned char)(seqno >> 24);
            px[offset_tcp+ 5] = (unsigned char)(seqno >> 16);
            px[offset_tcp+ 6] = (unsigned char)(seqno >>  8);
            px[offset_tcp+ 7] = (unsigned char)(seqno >>  0);
            xsum = (uint64_t)tmpl->checksum_tcp
                    + (uint64_t)seqno;
            xsum = (xsum >> 16) + (xsum & 0xFFFF);
            xsum = (xsum >> 16) + (xsum & 0xFFFF);
            xsum = (xsum >> 16) + (xsum & 0xFFFF);
            xsum = ~xsum;
            px[offset_tcp+2] = (unsigned char)(xsum >>  8);
            px[offset_tcp+3] = (unsigned char)(xsum >>  0);
        break;
    case Proto_ARP:
        /* don't do any checksumming */
        break;
    }

    tmplset->px = tmpl->packet;
    tmplset->length = tmpl->length;
}


/***************************************************************************
 * Here we take a packet template, parse it, then make it easier to work
 * with.
 ***************************************************************************/
static void
_template_init(
    struct TemplatePacket *tmpl,
    unsigned ip,
    const unsigned char *mac_source,
    const unsigned char *mac_dest,
    const void *packet_bytes,
    size_t packet_size
    )
{
    unsigned char *px;
    struct PreprocessedInfo parsed;
    unsigned x;

    /*
     * Create the new template structure:
     * - zero it out
     * - make copy of the old packet to serve as new template
     */
    memset(tmpl, 0, sizeof(*tmpl));
    tmpl->length = (unsigned)packet_size;
    
    tmpl->packet = (unsigned char *)malloc(2048);
    memcpy(tmpl->packet, packet_bytes, tmpl->length);
    px = tmpl->packet;

    /*
     * Parse the existing packet template. We support TCP, UDP, ICMP,
     * and ARP packets.
     */
    x = preprocess_frame(px, tmpl->length, 1 /*enet*/, &parsed);
    if (!x || parsed.found == FOUND_NOTHING) {
        LOG(0, "ERROR: bad packet template\n");
        exit(1);
    }
    tmpl->offset_ip = parsed.ip_offset;
    tmpl->offset_tcp = parsed.transport_offset;
    tmpl->offset_app = parsed.app_offset;
	if (parsed.found == FOUND_ARP) {
		tmpl->length = parsed.ip_offset + 28;
	} else 
	    tmpl->length = parsed.ip_offset + parsed.ip_length;

    /*
     * Overwrite the MAC and IP addresses
     */
    memcpy(px+0, mac_dest, 6);
    memcpy(px+6, mac_source, 6);
    ((unsigned char*)parsed.ip_src)[0] = (unsigned char)(ip>>24);
    ((unsigned char*)parsed.ip_src)[1] = (unsigned char)(ip>>16);
    ((unsigned char*)parsed.ip_src)[2] = (unsigned char)(ip>> 8);
    ((unsigned char*)parsed.ip_src)[3] = (unsigned char)(ip>> 0);

    /*
     * ARP
     *
     * If this is an ARP template (for doing arpscans), then just set our
     * configured source IP and MAC addresses.
     */
    if (parsed.found == FOUND_ARP) {
        memcpy((char*)parsed.ip_src - 6, mac_source, 6);
        tmpl->proto = Proto_ARP;
        return;
    }

    /*
     * IPv4
     *
     * We need to zero out the fields that'll be overwritten
     * later.
     */
    memset(px + tmpl->offset_ip + 4, 0, 2);  /* IP ID field */
    memset(px + tmpl->offset_ip + 10, 0, 2); /* checksum */
    memset(px + tmpl->offset_ip + 16, 0, 4); /* destination IP address */
    tmpl->checksum_ip = ip_checksum(tmpl);

    /*
     * Higher layer protocols: zero out dest/checksum fields, then calculate
     * a partial checksum
     */
    switch (parsed.ip_protocol) {
    case 1: /* ICMP */
            tmpl->checksum_tcp = icmp_checksum(tmpl);
            switch (px[tmpl->offset_tcp]) {
                case 8: 
                    tmpl->proto = Proto_ICMP_ping;
                    break;
                case 13:
                    tmpl->proto = Proto_ICMP_timestamp;
                    break;
            }
            break;
        break;
    case 6: /* TCP */
        /* zero out fields that'll be overwritten */
        memset(px + tmpl->offset_tcp + 2, 0, 6); /* destination port and seqno */
        memset(px + tmpl->offset_tcp + 16, 0, 2); /* checksum */
        tmpl->checksum_tcp = tcp_checksum(tmpl);
        tmpl->proto = Proto_TCP;
        break;
    case 17: /* UDP */
        memset(px + tmpl->offset_tcp + 6, 0, 2); /* checksum */
        tmpl->checksum_tcp = udp_checksum(tmpl);
        tmpl->proto = Proto_UDP;
        break;
    }
}

/***************************************************************************
 ***************************************************************************/
void
template_packet_init(
    struct TemplateSet *templset,
    unsigned source_ip,
    const unsigned char *source_mac,
    const unsigned char *router_mac,
    struct NmapPayloads *payloads)
{
    /* [TCP] */
    _template_init( &templset->pkts[Proto_TCP],
                    source_ip, source_mac, router_mac,
                    default_tcp_template,
                    sizeof(default_tcp_template)-1
                    );
    /* [UDP] */
    _template_init( &templset->pkts[Proto_UDP],
                    source_ip, source_mac, router_mac,
                    default_udp_template,
                    sizeof(default_udp_template)-1
                    );
    templset->pkts[Proto_UDP].payloads = payloads;

    /* [SCTP] */
    _template_init( &templset->pkts[Proto_SCTP],
                    source_ip, source_mac, router_mac,
                    default_sctp_template,
                    sizeof(default_sctp_template)-1
                    );
    /* [ICMP ping] */
    _template_init( &templset->pkts[Proto_ICMP_ping],
                   source_ip, source_mac, router_mac,
                   default_icmp_ping_template,
                   sizeof(default_icmp_ping_template)-1
                   );
    
    /* [ICMP timestamp] */
    _template_init( &templset->pkts[Proto_ICMP_timestamp],
                   source_ip, source_mac, router_mac,
                   default_icmp_timestamp_template,
                   sizeof(default_icmp_timestamp_template)-1
                   );
    
    /* [ARP] */
    _template_init( &templset->pkts[Proto_ARP],
                    source_ip, source_mac, router_mac,
                    default_arp_template,
                    sizeof(default_arp_template)-1
                    );
}

/***************************************************************************
 ***************************************************************************/
unsigned
template_get_source_ip(struct TemplateSet *tmplset)
{
    struct TemplatePacket *tmpl = &tmplset->pkts[Proto_TCP];
    const unsigned char *px = tmpl->packet;
    unsigned offset = tmpl->offset_ip;

    return px[offset+12]<<24 | px[offset+13]<<16
        | px[offset+14]<<8 | px[offset+15]<<0;
}

/***************************************************************************
 * Retrieve the source-port of the packet. We parse this from the packet
 * because while the source-port can be configured separately, we usually
 * get a raw packet template.
 ***************************************************************************/
unsigned
template_get_source_port(struct TemplateSet *tmplset)
{
    struct TemplatePacket *tmpl = &tmplset->pkts[Proto_TCP];
    const unsigned char *px = tmpl->packet;
    unsigned offset = tmpl->offset_tcp;

    return px[offset+0]<<8 | px[offset+1]<<0;
}

/***************************************************************************
 * Overwrites the source-port field in the packet template.
 ***************************************************************************/
void
template_set_source_port(struct TemplateSet *tmplset, unsigned port)
{
    int i;

    for (i=0; i<2; i++) {
        struct TemplatePacket *tmpl = &tmplset->pkts[i];
        unsigned char *px = tmpl->packet;
        unsigned offset = tmpl->offset_tcp;

        px[offset+0] = (unsigned char)(port>>8);
        px[offset+1] = (unsigned char)(port>>0);
        tmpl->checksum_tcp = tcp_checksum(tmpl);
    }

}

/***************************************************************************
 * Overwrites the TTL of the packet
 ***************************************************************************/
void
template_set_ttl(struct TemplateSet *tmplset, unsigned ttl)
{
    int i;

    for (i=0; i<8; i++) {
        struct TemplatePacket *tmpl = &tmplset->pkts[i];
        unsigned char *px = tmpl->packet;
        unsigned offset = tmpl->offset_ip;

        px[offset+8] = (unsigned char)(ttl);
        tmpl->checksum_ip = tcp_checksum(tmpl);
    }
}



/***************************************************************************
 ***************************************************************************/
int
template_selftest()
{
    struct TemplateSet tmplset[1];
    int failures = 0;

    template_packet_init(
            tmplset,
            0x12345678,
            (const unsigned char*)"\x00\x11\x22\x33\x44\x55",
            (const unsigned char*)"\x66\x55\x44\x33\x22\x11",
            0
            );
    failures += tmplset->pkts[Proto_TCP].proto  != Proto_TCP;
    failures += tmplset->pkts[Proto_UDP].proto  != Proto_UDP;
    //failures += tmplset->pkts[Proto_SCTP].proto != Proto_SCTP;
    failures += tmplset->pkts[Proto_ICMP_ping].proto != Proto_ICMP_ping;
    //failures += tmplset->pkts[Proto_ICMP_timestamp].proto != Proto_ICMP_timestamp;
    //failures += tmplset->pkts[Proto_ARP].proto  != Proto_ARP;

    if (failures)
        fprintf(stderr, "template: failed\n");
    return failures;
}

