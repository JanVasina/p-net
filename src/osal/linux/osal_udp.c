/*********************************************************************
 *        _       _         _
 *  _ __ | |_  _ | |  __ _ | |__   ___
 * | '__|| __|(_)| | / _` || '_ \ / __|
 * | |   | |_  _ | || (_| || |_) |\__ \
 * |_|    \__|(_)|_| \__,_||_.__/ |___/
 *
 * www.rt-labs.com
 * Copyright 2018 rt-labs AB, Sweden.
 *
 * This software is dual-licensed under GPLv3 and a commercial
 * license. See the file LICENSE.md distributed with this software for
 * full license information.
 ********************************************************************/

#include <string.h>
#include "pf_includes.h"
#include <stdio.h>
#include <unistd.h>
#include <net/if.h>
#include <sys/socket.h>
#include <string.h>
#include <sys/ioctl.h>
#include <netpacket/packet.h>

int os_udp_socket(void)
{
  return socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
}

int os_udp_open(os_ipaddr_t addr, os_ipport_t port)
{
  int ret = -1;
  struct sockaddr_in local;
  int id;

  id = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);

  if (id < 0)
  {
    ret = id;
  }
  else
  {
    /* set IP and port number */
    local = (struct sockaddr_in) {
       .sin_family = AF_INET,
       .sin_addr.s_addr = htonl(addr),
       .sin_port = htons(port),
       .sin_zero = { 0 },
    };
    ret = bind(id, (struct sockaddr *) & local, sizeof(local));
    if (ret != 0)
    {
      close(id);
    }
    else
    {
      ret = id;
    }
  }

  return ret;
}

int os_udp_sendto(uint32_t id,
                  os_ipaddr_t dst_addr,
                  os_ipport_t dst_port,
                  const uint8_t *data,
                  int size)
{
  struct sockaddr_in remote;
  int  len;

  remote = (struct sockaddr_in)
  {
     .sin_family = AF_INET,
     .sin_addr.s_addr = htonl(dst_addr),
     .sin_port = htons(dst_port),
     .sin_zero = { 0 },
  };
  len = sendto(id, data, size, 0, (struct sockaddr *) & remote, sizeof(remote));

  return len;
}

int os_udp_recvfrom(uint32_t id,
                    os_ipaddr_t *dst_addr,
                    os_ipport_t *dst_port,
                    uint8_t *data,
                    int size)
{
  struct sockaddr_in remote;
  socklen_t addr_len = sizeof(remote);
  int  len;

  memset(&remote, 0, sizeof(remote));
  len = recvfrom(id, data, size, MSG_DONTWAIT, (struct sockaddr *) & remote, &addr_len);
  if (len > 0)
  {
    *dst_addr = ntohl(remote.sin_addr.s_addr);
    *dst_port = ntohs(remote.sin_port);
  }

  return len;
}

void os_udp_close(uint32_t id)
{
  if(id > 0)
  {
    close(id);
  }
}

//////////////////////////////////////////////////////////////////////////
// UDP sockets created from raw data
// 

// The IP header's structure
CC_PACKED_BEGIN
struct CC_PACKED ipheader
{
  uint8_t     iph_ihl : 4;
  uint8_t     iph_ver : 4;
  uint8_t     iph_tos;
  uint16_t    iph_len;
  uint16_t    iph_ident;
  uint16_t    iph_flag;
  uint8_t     iph_ttl;
  uint8_t     iph_protocol;
  uint16_t    iph_chksum;
  uint32_t    iph_sourceip;
  uint32_t    iph_destip;
};
CC_PACKED_END


// UDP header's structure
CC_PACKED_BEGIN
struct CC_PACKED udpheader
{
  uint16_t udph_srcport;
  uint16_t udph_destport;
  uint16_t udph_len;
  uint16_t udph_chksum;
};
CC_PACKED_END

// total udp header length: 8 bytes (=64 bits)

// Function for checksum calculation. From the RFC,
// the checksum algorithm is:
//  "The checksum field is the 16 bit one's complement of the one's
//  complement sum of all 16 bit words in the header.  For purposes of
//  computing the checksum, the value of the checksum field is zero."
static uint16_t csum(size_t len, uint8_t *buf)
{       
  uint32_t sum = 0;
  size_t i;

  for (i = 0; i < len; i++)
  {
    if (i & 1)
    {
      sum += (uint32_t)buf[i];
    }
    else
    {
      sum += (uint32_t)buf[i] << 8;
    }
  }
  sum = (sum >> 16) + (sum & 0xffff);
  sum += (sum >> 16);
  return (uint16_t)(~sum);
}

uint32_t net_checksum_add(size_t len, uint8_t *buf)
{
  uint32_t sum = 0;
  size_t i;

  for (i = 0; i < len; i++)
  {
    if (i & 1)
      sum += (uint32_t)buf[i];
    else
      sum += (uint32_t)buf[i] << 8;
  }
  return sum;
}

uint16_t net_checksum_finish(uint32_t sum)
{
  while (sum >> 16)
  {
    sum = (sum & 0xFFFF) + (sum >> 16);
  }
  return ~sum;
}

uint16_t net_checksum_tcpudp(uint16_t length, uint16_t proto,
                             uint8_t *addrs, uint8_t *buf)
{
  uint32_t sum = 0;

  sum += net_checksum_add(length, buf);         // payload
  sum += net_checksum_add(8, addrs);            // src + dst address
  sum += proto + length;                        // protocol & length
  return net_checksum_finish(sum);
}

void net_checksum_calculate(uint8_t *data)
{
  int hlen, plen, proto, csum_offset;
  uint16_t csum;

  if ((data[14] & 0xf0) != 0x40)
  {
    return; /* not IPv4 */
  }
  hlen = (data[14] & 0x0f) * 4;
  plen = (data[16] << 8 | data[17]) - hlen;
  proto = data[23];

  switch (proto)
  {
  case IPPROTO_TCP:
    csum_offset = 16;
    break;
  case IPPROTO_UDP:
    csum_offset = 6;
    break;
  default:
    return;
  }

  if (plen < csum_offset + 2)
    return;

  data[14 + hlen + csum_offset] = 0;
  data[14 + hlen + csum_offset + 1] = 0;
  csum = net_checksum_tcpudp(plen, proto, data + 14 + 12, data + 14 + hlen);
  data[14 + hlen + csum_offset] = csum >> 8;
  data[14 + hlen + csum_offset + 1] = csum & 0xff;
}

int os_udp_sendto_raw(pnet_t *net,
                      os_ipaddr_t src_addr,
                      os_ipport_t src_port,
                      os_ipaddr_t dst_addr,
                      os_ipport_t dst_port,
                      const uint8_t *data,
                      int size)
{
  int                     i;
  struct ifreq            ifr;
  struct sockaddr_ll      sll;
  int                     ifindex;
  struct timeval          timeout;

  // Create a raw socket with UDP protocol
  int sd = socket(PF_PACKET, SOCK_RAW, IPPROTO_IP);

  if (sd < 0)
  {
    printf("#### cannot open socket raw\n");
    return -1;
  }

  timeout.tv_sec = 0;
  timeout.tv_usec = 1;
  setsockopt(sd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

  i = 1;
  setsockopt(sd, SOL_SOCKET, SO_DONTROUTE, &i, sizeof(i));

  strcpy(ifr.ifr_name, net->interface_name);
  ioctl(sd, SIOCGIFINDEX, &ifr);

  ifindex = ifr.ifr_ifindex;
  strcpy(ifr.ifr_name, net->interface_name);
  ifr.ifr_flags = 0;
  /* reset flags of NIC interface */
  ioctl(sd, SIOCGIFFLAGS, &ifr);

  /* set flags of NIC interface, here promiscuous and broadcast */
  ifr.ifr_flags = ifr.ifr_flags | IFF_PROMISC | IFF_BROADCAST;
  ioctl(sd, SIOCSIFFLAGS, &ifr);

  /* bind socket to protocol, in this case UDP */
  sll.sll_family = AF_PACKET;
  sll.sll_ifindex = ifindex;
  sll.sll_protocol = htons(OS_ETHTYPE_IP);
  bind(sd, (struct sockaddr *) & sll, sizeof(sll));

  os_buf_t *p_raw_buf = os_buf_alloc(PF_FRAME_BUFFER_SIZE);
  uint8_t *p_send = p_raw_buf->payload;
  uint8_t *p_buf = p_raw_buf->payload;

  // copy dst address
  memcpy(p_buf, net->dcp_sam.addr, sizeof(pnet_ethaddr_t));
  p_buf += sizeof(pnet_ethaddr_t);

  // copy src address
  const pnet_cfg_t *p_cfg = NULL;
  pf_fspm_get_default_cfg(net, &p_cfg);

  memcpy(p_buf, p_cfg->eth_addr.addr, sizeof(pnet_ethaddr_t));
  p_buf += sizeof(pnet_ethaddr_t);
  // ToDo: change to htons
  *p_buf++ = 0x08;
  *p_buf++ = 0x00;

  // Our own headers' structures
  struct ipheader *ip = (struct ipheader *)(p_buf);
  p_buf += sizeof(struct ipheader);
  struct udpheader *udp = (struct udpheader *)(p_buf);
  p_buf += sizeof(struct udpheader);

  const uint16_t totalSize = sizeof(struct ipheader) + sizeof(struct udpheader) + size;
  // Fabricate the IP header or we can use the
// standard header structures but assign our own values.
  ip->iph_ihl = 5; // 20 bytes
  ip->iph_ver = 4;
  ip->iph_tos = 0; // Low delay
  ip->iph_len = htons(totalSize);
  ip->iph_ident = 0;
  ip->iph_flag = htons(0x4000); // don't fragment
  ip->iph_ttl = 64; // hops
  ip->iph_protocol = IPPROTO_UDP; // UDP
  ip->iph_chksum = 0;
  // Source IP address, can use spoofed address here!!!
  ip->iph_sourceip = htonl(src_addr);
  // The destination IP address
  ip->iph_destip = htonl(dst_addr);

  // Fabricate the UDP header. Source port number, redundant
  udp->udph_srcport = htons(src_port);
  // Destination port number
  udp->udph_destport = htons(dst_port);
  udp->udph_len = htons(sizeof(struct udpheader) + size);
  udp->udph_chksum = 0;
  // Calculate the checksum for integrity
  ip->iph_chksum = htons(csum(sizeof(struct ipheader), (uint8_t *)ip));

  memcpy(p_buf, data, size);
  const uint32_t totalSize_withHeader = totalSize + 14;
  net_checksum_calculate(p_send);

  int ret = send(sd, p_send, totalSize_withHeader, 0);
  
  close(sd);
  os_buf_free(p_raw_buf);

  printf("#### send raw returned %i\n", ret);

  return ret;
}
