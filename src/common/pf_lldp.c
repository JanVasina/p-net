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

#ifdef UNIT_TEST
#define os_eth_send           mock_os_eth_send
#endif

 /*
  * ToDo: Differentiate between device and port MAC addresses.
  * ToDo: Handle PNET_MAX_PORT ports.
  * ToDo: Receive LLDP and build a per-port peer DB.
  */

#include <string.h>

#include "pf_includes.h"
#include "pf_block_writer.h"
#include "pf_block_reader.h"

#define LLDP_TYPE_END                     0
#define LLDP_TYPE_CHASSIS_ID              1
#define LLDP_TYPE_PORT_ID                 2
#define LLDP_TYPE_TTL                     3
#define LLDP_TYPE_MANAGEMENT              8
#define LLDP_TYPE_ORG_SPEC                127

#define LLDP_SUBTYPE_CHASSIS_ID_MAC       4
#define LLDP_SUBTYPE_CHASSIS_ID_NAME      7
#define LLDP_SUBTYPE_PORT_ID_LOCAL        7

#define LLDP_IEEE_SUBTYPE_MAC_PHY         1

static const char             *lldp_sync_name = "lldp";

typedef enum lldp_pnio_subtype_values
{
  LLDP_PNIO_SUBTYPE_RESERVED = 0,
  LLDP_PNIO_SUBTYPE_MEAS_DELAY_VALUES = 1,
  LLDP_PNIO_SUBTYPE_PORT_STATUS = 2,
  LLDP_PNIO_SUBTYPE_PORT_ALIAS = 3,
  LLDP_PNIO_SUBTYPE_MRP_RING_PORT_STATUS = 4,
  LLDP_PNIO_SUBTYPE_INTERFACE_MAC = 5,
  LLDP_PNIO_SUBTYPE_PTCP_STATUS = 6,
  LLDP_PNIO_SUBTYPE_MAU_TYPE_EXTENSION = 7,
  LLDP_PNIO_SUBTYPE_MRP_INTERCONNECTION_PORT_STATUS = 8,
  /* 0x09..0xff reserved */
} lldp_pnio_subtype_values_t;

static const pnet_ethaddr_t   lldp_dst_addr = {
   { 0x01, 0x80, 0xc2, 0x00, 0x00, 0x0e }       /* LLDP Multicast */
};

/**
 * @internal
 * Insert a TLV header into a buffer.
 * @param p_buf            InOut:The buffer.
 * @param p_pos            InOut:The buffer position.
 * @param typ              In:   The TLV header type.
 * @param len              In:   The TLV length.
 */
static inline void pf_lldp_tlv_header(
  uint8_t                 *p_buf,
  uint16_t                *p_pos,
  uint8_t                 typ,
  uint8_t                 len)
{
  pf_put_uint16(true, ((typ) << 9) + ((len) & 0x1ff), PF_FRAME_BUFFER_SIZE, p_buf, p_pos);
}

/**
 * @internal
 * Insert a PNIO header into a buffer.
 * @param p_buf            InOut:The buffer.
 * @param p_pos            InOut:The buffer position.
 * @param typ              In:   The TLV header type.
 * @param len              In:   The TLV length.
 */
static inline void pf_lldp_pnio_header(
  uint8_t                 *p_buf,
  uint16_t                *p_pos,
  uint8_t                 len)
{
  pf_lldp_tlv_header(p_buf, p_pos, LLDP_TYPE_ORG_SPEC, (len)+3);
  pf_put_byte(0x00, PF_FRAME_BUFFER_SIZE, p_buf, p_pos);
  pf_put_byte(0x0e, PF_FRAME_BUFFER_SIZE, p_buf, p_pos);
  pf_put_byte(0xcf, PF_FRAME_BUFFER_SIZE, p_buf, p_pos);
}

/**
 * @internal
 * Insert a IEEE header into a buffer.
 * @param p_buf            InOut:The buffer.
 * @param p_pos            InOut:The buffer position.
 * @param typ              In:   The TLV header type.
 * @param len              In:   The TLV length.
 */
static inline void pf_lldp_ieee_header(
  uint8_t                 *p_buf,
  uint16_t                *p_pos,
  uint8_t                 len)
{
  pf_lldp_tlv_header(p_buf, p_pos, LLDP_TYPE_ORG_SPEC, (len)+3);
  pf_put_byte(0x00, PF_FRAME_BUFFER_SIZE, p_buf, p_pos);
  pf_put_byte(0x12, PF_FRAME_BUFFER_SIZE, p_buf, p_pos);
  pf_put_byte(0x0f, PF_FRAME_BUFFER_SIZE, p_buf, p_pos);
}

/**
 * @internal
 * Insert the mandatory chassis_id TLV into a buffer.
 * @param p_cfg            In:   The Profinet configuration.
 * @param p_buf            InOut:The buffer.
 * @param p_pos            InOut:The position in the buffer.
 */
static void lldp_add_chassis_id_tlv(
  pnet_t                  *net,
  pnet_cfg_t              *p_cfg,
  uint8_t                 *p_buf,
  uint16_t                *p_pos)
{
  uint16_t                  len;
  const char              *p_station_name = NULL;

  (void)pf_cmina_get_station_name(net, &p_station_name);

  len = (uint16_t)strlen(p_station_name);
  if (len == 0)
  {
    /* Use the MAC address */
    pf_lldp_tlv_header(p_buf, p_pos, LLDP_TYPE_CHASSIS_ID, 1 + sizeof(pnet_ethaddr_t));

    pf_put_byte(LLDP_SUBTYPE_CHASSIS_ID_MAC, PF_FRAME_BUFFER_SIZE, p_buf, p_pos);
    memcpy(&p_buf[*p_pos], p_cfg->eth_addr.addr, sizeof(pnet_ethaddr_t)); /* ToDo: Shall be device MAC */
    (*p_pos) += sizeof(pnet_ethaddr_t);
  }
  else
  {
    /* Use the chassis_id from the cfg */
    pf_lldp_tlv_header(p_buf, p_pos, LLDP_TYPE_CHASSIS_ID, 1 + len);

    pf_put_byte(LLDP_SUBTYPE_CHASSIS_ID_NAME, PF_FRAME_BUFFER_SIZE, p_buf, p_pos);
    pf_put_mem(p_station_name, len, PF_FRAME_BUFFER_SIZE, p_buf, p_pos);
  }
}

/**
 * @internal
 * Insert the mandatory port_id TLV into a buffer.
 * @param p_cfg            In:   The Profinet configuration.
 * @param p_buf            InOut:The buffer.
 * @param p_pos            InOut:The position in the buffer.
 */
static void lldp_add_port_id_tlv(
  pnet_cfg_t              *p_cfg,
  uint8_t                 *p_buf,
  uint16_t                *p_pos)
{
  uint16_t                len;

  len = (uint16_t)strlen(p_cfg->lldp_cfg.port_id);

  pf_lldp_tlv_header(p_buf, p_pos, LLDP_TYPE_PORT_ID, 1 + len);

  pf_put_byte(LLDP_SUBTYPE_PORT_ID_LOCAL, PF_FRAME_BUFFER_SIZE, p_buf, p_pos);
  pf_put_mem(p_cfg->lldp_cfg.port_id, len, PF_FRAME_BUFFER_SIZE, p_buf, p_pos);
}

/**
 * @internal
 * Insert the mandatory TTL TLV into a buffer.
 * @param p_cfg            In:   The Profinet configuration.
 * @param p_buf            InOut:The buffer.
 * @param p_pos            InOut:The position in the buffer.
 */
static void lldp_add_ttl_tlv(
  pnet_cfg_t              *p_cfg,
  uint8_t                 *p_buf,
  uint16_t                *p_pos)
{
  pf_lldp_tlv_header(p_buf, p_pos, LLDP_TYPE_TTL, 2);
  pf_put_uint16(true, p_cfg->lldp_cfg.ttl, PF_FRAME_BUFFER_SIZE, p_buf, p_pos);
}

/**
 * @internal
 * Insert the optional port status TLV into a buffer.
 *
 * The port status TLV is mandatory for ProfiNet.
 * @param p_cfg            In:   The Profinet configuration.
 * @param p_buf            InOut:The buffer.
 * @param p_pos            InOut:The position in the buffer.
 */
static void lldp_add_port_status(
  pnet_cfg_t              *p_cfg,
  uint8_t                 *p_buf,
  uint16_t                *p_pos)
{
  pf_lldp_pnio_header(p_buf, p_pos, 5);

  pf_put_byte(LLDP_PNIO_SUBTYPE_PORT_STATUS, PF_FRAME_BUFFER_SIZE, p_buf, p_pos);
  pf_put_uint16(true, p_cfg->lldp_cfg.rtclass_2_status, PF_FRAME_BUFFER_SIZE, p_buf, p_pos);
  pf_put_uint16(true, p_cfg->lldp_cfg.rtclass_3_status, PF_FRAME_BUFFER_SIZE, p_buf, p_pos);
}

/**
 * @internal
 * Insert the optional chassis MAC TLV into a buffer.
 *
 * The chassis MAC TLV is mandatory for ProfiNet.
 * @param p_cfg            In:   The Profinet configuration.
 * @param p_buf            InOut:The buffer.
 * @param p_pos            InOut:The position in the buffer.
 */
static void lldp_add_chassis_mac(
  pnet_cfg_t              *p_cfg,
  uint8_t                 *p_buf,
  uint16_t                *p_pos)
{
  pf_lldp_pnio_header(p_buf, p_pos, 1 + sizeof(pnet_ethaddr_t));

  pf_put_byte(LLDP_PNIO_SUBTYPE_INTERFACE_MAC, PF_FRAME_BUFFER_SIZE, p_buf, p_pos);
  memcpy(&p_buf[*p_pos], p_cfg->eth_addr.addr, sizeof(pnet_ethaddr_t)); /* ToDo: Should be device MAC */
  (*p_pos) += sizeof(pnet_ethaddr_t);
}

/**
 * @internal
 * Insert the optional IEEE 802.3 MAC TLV into a buffer.
 *
 * The IEEE 802.3 MAC TLV is mandatory for ProfiNet on 803.2 interfaces.
 * @param p_cfg            In:   The Profinet configuration.
 * @param p_buf            InOut:The buffer.
 * @param p_pos            InOut:The position in the buffer.
 */
static void lldp_add_ieee_mac_phy(
  pnet_cfg_t              *p_cfg,
  uint8_t                 *p_buf,
  uint16_t                *p_pos)
{
  pf_lldp_ieee_header(p_buf, p_pos, 6);

  pf_put_byte(LLDP_IEEE_SUBTYPE_MAC_PHY, PF_FRAME_BUFFER_SIZE, p_buf, p_pos);
  pf_put_byte(p_cfg->lldp_cfg.cap_aneg, PF_FRAME_BUFFER_SIZE, p_buf, p_pos);
  pf_put_uint16(true, p_cfg->lldp_cfg.cap_phy, PF_FRAME_BUFFER_SIZE, p_buf, p_pos);
  pf_put_uint16(true, p_cfg->lldp_cfg.mau_type, PF_FRAME_BUFFER_SIZE, p_buf, p_pos);
}

/**
 * Insert the optional management data TLV into a buffer.
 * It is mandatory for ProfiNet.
 * @param net              InOut: The p-net stack instance
 * @param p_cfg            In:   The Profinet configuration.
 * @param p_buf            InOut:The buffer.
 * @param p_pos            InOut:The position in the buffer.
 */
static void lldp_add_management(
  pnet_t                  *net,
  pnet_cfg_t              *p_cfg,
  uint8_t                 *p_buf,
  uint16_t                *p_pos)
{
  os_ipaddr_t             ipaddr = 0;

  pf_cmina_get_ipaddr(net, &ipaddr);

  pf_lldp_tlv_header(p_buf, p_pos, LLDP_TYPE_MANAGEMENT, 12);

  /* ToDo: What shall be moved to lldp_cfg? */
  pf_put_byte(1 + 4, PF_FRAME_BUFFER_SIZE, p_buf, p_pos);     /* Address string length (incl type) */
  pf_put_byte(1, PF_FRAME_BUFFER_SIZE, p_buf, p_pos);       /* Type IPV4 */
  pf_put_uint32(true, ipaddr, PF_FRAME_BUFFER_SIZE, p_buf, p_pos);
  pf_put_byte(1, PF_FRAME_BUFFER_SIZE, p_buf, p_pos);       /* Interface Subtype: Unknown */
  pf_put_uint32(true, 0, PF_FRAME_BUFFER_SIZE, p_buf, p_pos);  /* Interface number: Unknown */
  pf_put_byte(0, PF_FRAME_BUFFER_SIZE, p_buf, p_pos);       /* OID string length: 0 => Not supported */
}

/********************* Initialize and send **********************************/

/**
 * @internal
 * Trigger sending a LLDP frame.
 *
 * This is a callback for the scheduler. Arguments should fulfill pf_scheduler_timeout_ftn_t
 *
 * Re-schedules itself after 5 s.
 *
 * @param net              InOut: The p-net stack instance
 * @param arg              In:   Not used.
 * @param current_time     In:   Not used.
 */
static void pf_lldp_trigger_sending(
  pnet_t                  *net,
  void                    *arg,
  uint32_t                current_time)
{
  pf_lldp_send(net);

  /* Reschedule */
  if (pf_scheduler_add(net, PF_LLDP_INTERVAL * 1000,
                       lldp_sync_name, pf_lldp_trigger_sending, NULL, &net->lldp_timeout) != 0)
  {
    LOG_ERROR(PF_ETH_LOG, "LLDP(%d): Failed to reschedule LLDP sending\n", __LINE__);
  }
}

void pf_lldp_send(
  pnet_t                  *net)
{
  os_buf_t                *p_lldp_buffer;
  uint8_t                 *p_buf = NULL;
  uint16_t                pos = 0;
  pnet_cfg_t              *p_cfg = NULL;

  if ((net->adjust_peer_to_peer_boundary & 0x1) != 0)
  {
    // from Wireshark:
    // AdjustPeerToPeer-Boundary: 0x00000001
    // .... .... .... .... .... .... .... ...1 = AdjustPeerToPeer-Boundary: The LLDP agent shall not send LLDP frames (egress filter). (0x1)
    return;
  }

  p_lldp_buffer = os_buf_alloc(PF_FRAME_BUFFER_SIZE);
  pf_fspm_get_cfg(net, &p_cfg);
  /*
   * LLDP-PDU ::=  LLDPChassis, LLDPPort, LLDPTTL, LLDP-PNIO-PDU, LLDPEnd
   *
   * LLDPChassis ::= LLDPChassisStationName ^
   *                 LLDPChassisMacAddress           (If no station name)
   * LLDPChassisStationName ::= LLDP_TLVHeader,      (According to IEEE 802.1AB-2016)
   *                 LLDP_ChassisIDSubType(7),       (According to IEEE 802.1AB-2016)
   *                 LLDP_ChassisID
   * LLDPChassisMacAddress ::= LLDP_TLVHeader,       (According to IEEE 802.1AB-2016)
   *                 LLDP_ChassisIDSubType(4),       (According to IEEE 802.1AB-2016)
   *
   * LLDP-PNIO-PDU ::= {
   *                [LLDP_PNIO_DELAY],               (If LineDelay measurement is supported)
   *                LLDP_PNIO_PORTSTATUS,
   *                [LLDP_PNIO_ALIAS],
   *                [LLDP_PNIO_MRPPORTSTATUS],       (If MRP is activated for this port)
   *                [LLDP_PNIO_MRPICPORTSTATUS],     (If MRP Interconnection is activated for this port)
   *                LLDP_PNIO_CHASSIS_MAC,
   *                LLDP8023MACPHY,                  (If IEEE 802.3 is used)
   *                LLDPManagement,                  (According to IEEE 802.1AB-2016, 8.5.9)
   *                [LLDP_PNIO_PTCPSTATUS],          (If PTCP is activated by means of the PDSyncData Record)
   *                [LLDP_PNIO_MAUTypeExtension],    (If a MAUType with MAUTypeExtension is used and may exist otherwise)
   *                [LLDPOption*],                   (Other LLDP options may be used concurrently)
   *                [LLDP8021*],
   *                [LLDP8023*]
   *                }
   *
   * LLDP_PNIO_HEADER ::= LLDP_TLVHeader,            (According to IEEE 802.1AB-2016)
   *                LLDP_OUI(00-0E-CF)
   *
   * LLDP_PNIO_PORTSTATUS ::= LLDP_PNIO_HEADER, LLDP_PNIO_SubType(0x02), RTClass2_PortStatus, RTClass3_PortStatus
   *
   * LLDP_PNIO_CHASSIS_MAC ::= LLDP_PNIO_HEADER, LLDP_PNIO_SubType(0x05), (
   *                CMResponderMacAdd ^
   *                CMInitiatorMacAdd                (Shall be the interface MAC address of the transmitting node)
   *                )
   *
   * LLDP8023MACPHY ::= LLDP_TLVHeader,              (According to IEEE 802.1AB-2016)
   *                LLDP_OUI(00-12-0F),              (According to IEEE 802.1AB-2016, Annex F)
   *                LLDP_8023_SubType(1),            (According to IEEE 802.1AB-2016, Annex F)
   *                LLDP_8023_AUTONEG,               (According to IEEE 802.1AB-2016, Annex F)
   *                LLDP_8023_PMDCAP,                (According to IEEE 802.1AB-2016, Annex F)
   *                LLDP_8023_OPMAU                  (According to IEEE 802.1AB-2016, Annex F)
   *
   * LLDPManagement ::= LLDP_TLVHeader,              (According to IEEE 802.1AB-2016)
   *                LLDP_ManagementData              (Use PNIO MIB Enterprise number = 24686 (dec))
   *
   * LLDP_ManagementData ::=
   */
  if (p_lldp_buffer != NULL)
  {
    p_buf = p_lldp_buffer->payload;
    if (p_buf != NULL)
    {
      pos = 0;
      pf_put_mem(&lldp_dst_addr, sizeof(lldp_dst_addr), PF_FRAME_BUFFER_SIZE, p_buf, &pos);
      memcpy(&p_buf[pos], p_cfg->eth_addr.addr, sizeof(pnet_ethaddr_t)); /* ToDo: Shall be port MAC address */
      pos += sizeof(pnet_ethaddr_t);

      /* Add FrameID */
      pf_put_uint16(true, OS_ETHTYPE_LLDP, PF_FRAME_BUFFER_SIZE, p_buf, &pos);

      /* Add mandatory parts */
      lldp_add_chassis_id_tlv(net, p_cfg, p_buf, &pos);
      lldp_add_port_id_tlv(p_cfg, p_buf, &pos);
      lldp_add_ttl_tlv(p_cfg, p_buf, &pos);

      /* Add optional parts */
      lldp_add_port_status(p_cfg, p_buf, &pos);
      lldp_add_chassis_mac(p_cfg, p_buf, &pos);
      lldp_add_ieee_mac_phy(p_cfg, p_buf, &pos);
      lldp_add_management(net, p_cfg, p_buf, &pos);

      /* Add end of LLDP-PDU marker */
      pf_lldp_tlv_header(p_buf, &pos, LLDP_TYPE_END, 0);

      p_lldp_buffer->len = pos;

      LOG_DEBUG(PF_ETH_LOG, "LLDP(%d): Sending LLDP frame\n", __LINE__);

      if (os_eth_send(net->eth_handle, p_lldp_buffer) <= 0)
      {
        LOG_ERROR(PNET_LOG, "LLDP(%d): Error from os_eth_send(lldp)\n", __LINE__);
      }
    }

    os_buf_free(p_lldp_buffer);
  }
}

void pf_lldp_init(
  pnet_t                  *net)
{
  pf_lldp_send(net);
  if (pf_scheduler_add(net, PF_LLDP_INTERVAL * 1000,
                       lldp_sync_name, pf_lldp_trigger_sending, NULL, &net->lldp_timeout) != 0)
  {
    LOG_ERROR(PF_ETH_LOG, "LLDP(%d): Failed to reschedule LLDP sending\n", __LINE__);
  }
}

/////////////////////////////////////////////////////////////////////////////////////////////////

static int pf_lldp_parse_chassis_id(
  pnet_t        *net,
  pf_get_info_t *p_get_info,
  uint16_t      *p_pos,
  uint8_t        sub_type,
  uint16_t       len)
{
  switch (sub_type)
  {
  case LLDP_SUBTYPE_CHASSIS_ID_NAME:
    if(len < sizeof(net->lldp_check_peers_data.peer_chassis_id)-1)
    {
      char    peer_chassis_id[MAX_PORT_NAME_LENGTH];
      pf_get_mem(p_get_info, p_pos, len, peer_chassis_id);
      peer_chassis_id[len] = '\0';
      if(strcmp(peer_chassis_id, "ipc") != 0) // hack!
      {
        strcpy(net->lldp_check_peers_data.peer_chassis_id, peer_chassis_id);
        //net->lldp_check_peers_data.peer_chassis_id[len] = '\0';
        net->lldp_check_peers_data.length_peer_chassis_id = len;
      }
    }
    else
    {
      *p_pos += len;
    }
    break;
  case LLDP_SUBTYPE_CHASSIS_ID_MAC:
    if (len == sizeof(pnet_ethaddr_t))
    {
      pf_get_mem(p_get_info, p_pos, len, &(net->lldp_peer_mac_addr));
    }
    else
    {
      *p_pos += len;
    }
    break;
  default:
    *p_pos += len;
    break;
  }
  return p_get_info->result == PF_PARSE_OK ? 0 : -1;
}


static int pf_lldp_parse_port_id(
  pnet_t        *net,
  pf_get_info_t *p_get_info,
  uint16_t      *p_pos,
  uint8_t        sub_type,
  uint16_t       len)
{
  switch (sub_type)
  {
  case LLDP_SUBTYPE_PORT_ID_LOCAL:
    if (len < sizeof(net->lldp_check_peers_data.peer_port_id) - 1)
    {
      pf_get_mem(p_get_info, p_pos, len, net->lldp_check_peers_data.peer_port_id);
      net->lldp_check_peers_data.peer_port_id[len] = '\0';
      net->lldp_check_peers_data.length_peer_port_id = len;
    }
    else
    {
      *p_pos += len;
    }
    break;
  default:
    *p_pos += len;
    break;
  }
    
  return p_get_info->result == PF_PARSE_OK ? 0 : -1;
}

static int pf_lldp_parse_org_specific(
  pnet_t        *net,
  pf_get_info_t *p_get_info,
  uint16_t      *p_pos,
  uint8_t        sub_type,
  uint16_t       len)
{
  const uint8_t org_0 = sub_type; // must be 0x00
  const uint8_t org_1 = pf_get_byte(p_get_info, p_pos); // 0x0e
  const uint8_t org_2 = pf_get_byte(p_get_info, p_pos); // 0xcf
  const uint8_t type = pf_get_byte(p_get_info, p_pos);

  len -= 3;
  if (org_0 != 0x00 && org_1 != 0x0e && org_2 != 0xcf)
  {
    *p_pos += len;
    return p_get_info->result == PF_PARSE_OK ? 0 : -1;
  }
  switch (type)
  {
  case LLDP_PNIO_SUBTYPE_INTERFACE_MAC:
    if (len == sizeof(pnet_ethaddr_t))
    {
      pf_get_mem(p_get_info, p_pos, len, &(net->lldp_peer_mac_addr));
    }
    else
    {
      *p_pos += len;
    }
    break;
  default:
    *p_pos += len;
    break;
  }
  return p_get_info->result == PF_PARSE_OK ? 0 : -1;
}

// handle lldp receive packets
int pf_lldp_recv(
  pnet_t                  *net,
  os_buf_t                *p_buf)
{
  int ret      = 0;
  uint16_t pos = 2 * sizeof(pnet_ethaddr_t) + sizeof(uint16_t);
  pf_get_info_t get_info;
  bool b_got_chassis_id = false;
  bool b_got_port_id = false;

  get_info.result = PF_PARSE_OK;
  get_info.is_big_endian = true;
  get_info.p_buf = (uint8_t *)p_buf->payload;
  get_info.len = p_buf->len;

  while ((get_info.result == PF_PARSE_OK) && (ret == 0) && (pos < p_buf->len))
  {
    const uint16_t tlv = pf_get_uint16(&get_info, &pos);
    const uint16_t type = tlv >> 9;
    const uint16_t len = (tlv & 0x1FF) - 1;
    const uint8_t  sub_type = pf_get_byte(&get_info, &pos);
    switch (type)
    {
    case LLDP_TYPE_CHASSIS_ID:
      ret = pf_lldp_parse_chassis_id(net, &get_info, &pos, sub_type, len);
      b_got_chassis_id = true;
      break;
    case LLDP_TYPE_PORT_ID:
      ret = pf_lldp_parse_port_id(net, &get_info, &pos, sub_type, len);
      b_got_port_id = true;
      break;
    case LLDP_TYPE_ORG_SPEC:
      ret = pf_lldp_parse_org_specific(net, &get_info, &pos, sub_type, len);
      break;
    case LLDP_TYPE_END:
      ret = 1;
      break;
    default:
      pos += len;
      break;
    }
  }

  if((b_got_chassis_id == true) || (b_got_port_id == true))
  {
    net->lldp_check_peers_data.number_of_peers = 1; // always one peer - we have only one port
    net->last_valid_lldp_message_time = os_get_current_time_us();

    if (pf_check_peers_data_is_same(&(net->lldp_check_peers_data), &(net->previous_lldp_check_peers_data)) != 0)
    {
      uint16_t ix;
      for (ix = 0U; ix < PNET_MAX_AR; ix++)
      {
        pf_ar_t *p_ar = pf_ar_find_by_index(net, ix);
        if (p_ar != NULL)
        {
          if (p_ar->in_use == true)
          {
            pf_ppm_set_problem_indicator(p_ar, true);
          }
        }
      }

      LOG_WARNING(PF_ETH_LOG,
               "LLDP(%d): Check peers data changed:\nOLD chassis: %s port: %s\nNEW chassis: %s port: %s\n",
               __LINE__,
               net->previous_lldp_check_peers_data.peer_chassis_id,
               net->previous_lldp_check_peers_data.peer_port_id,
               net->lldp_check_peers_data.peer_chassis_id,
               net->lldp_check_peers_data.peer_port_id);

      net->previous_lldp_check_peers_data = net->lldp_check_peers_data;
    }

    // create an alias name for later use in DCP
    net->length_alias_name = snprintf(net->alias_name,
                                      sizeof(net->alias_name),
                                      "%s%c%s",
                                      net->lldp_check_peers_data.peer_port_id,
                                      '.',
                                      net->lldp_check_peers_data.peer_chassis_id);

    LOG_DEBUG(PF_ETH_LOG,
              "LLDP(%d) Peer: chassis: %s\tport: %s\n",
              __LINE__,
              net->lldp_check_peers_data.peer_chassis_id,
              net->lldp_check_peers_data.peer_port_id);
  }

  return 1; // means "handled"
}
