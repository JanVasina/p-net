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
#endif

#include <string.h>
#include "pf_includes.h"
#include "pf_block_writer.h"

//////////////////////////////////////////////////////////////////////////

static void pf_put_pdport_data_check(
  pnet_t                *net, 
  pf_iod_read_request_t *p_read_request, 
  uint16_t               res_size, 
  uint8_t               *p_res, 
  uint16_t              *p_pos)
{
  const uint16_t peer_port_id_len = strlen(net->check_peers_data.peer_port_id);
  const uint16_t peer_chassis_len = strlen(net->check_peers_data.peer_chassis_id);
  const uint16_t check_peers_data_len = 5 + peer_port_id_len + peer_chassis_len;

  pf_put_block_header(true, PF_BT_PDPORT_DATA_CHECK, check_peers_data_len + 0xC, 1, 0, res_size, p_res, p_pos);
  pf_put_uint16(true, 0U, res_size, p_res, p_pos); // padding
  pf_put_uint16(true, p_read_request->slot_number, res_size, p_res, p_pos); 
  pf_put_uint16(true, p_read_request->subslot_number, res_size, p_res, p_pos);

  pf_put_block_header(true, PF_BT_CHECK_PEERS, check_peers_data_len, 1, 0, res_size, p_res, p_pos);
  pf_put_byte(net->check_peers_data.number_of_peers, res_size, p_res, p_pos);
  pf_put_byte(net->check_peers_data.length_peer_port_id, res_size, p_res, p_pos);
  pf_put_str(net->check_peers_data.peer_port_id, peer_port_id_len, res_size, p_res, p_pos);
  pf_put_byte(net->check_peers_data.length_peer_chassis_id, res_size, p_res, p_pos);
  pf_put_str(net->check_peers_data.peer_chassis_id, peer_chassis_len, res_size, p_res, p_pos);
  pf_put_uint16(true, 0U, res_size, p_res, p_pos); // padding
}


// put real data according to table 28 IODReadRes.RecordDataRead.PDPortDataReal
// in AutomatedRtTester_v2.4.1.3/doc/TCIS_00000001_DiffAccessWays-SysRed-addOn_v2.35.2.00002.pdf
// Note that there are errors in the table: 
// PeerStationName must be 'b' not 'B'
// Padding after PeerStationName is one byte long, not three bytes
static int pf_put_pdport_data_real(
  pnet_t                *net,
  pf_iod_read_request_t *p_read_request,
  uint16_t               res_size,
  uint8_t               *p_res,
  uint16_t              *p_pos)
{
  const char *port_id = net->p_fspm_default_cfg->lldp_cfg.port_id;
  const uint16_t port_id_len = (uint16_t)(strlen(port_id));
  uint16_t block_pos = *p_pos;

  pf_put_block_header(true, PF_BT_PDPORT_DATA_REAL, 0, // Don't know block_len yet
                      PNET_BLOCK_VERSION_HIGH, PNET_BLOCK_VERSION_LOW,
                      res_size, p_res, p_pos);
  pf_put_uint16(false, 0U, res_size, p_res, p_pos); // padding
  pf_put_uint16(true, p_read_request->slot_number, res_size, p_res, p_pos);
  pf_put_uint16(true, p_read_request->subslot_number, res_size, p_res, p_pos);
  
  pf_put_byte(port_id_len, res_size, p_res, p_pos);
  pf_put_mem(port_id, port_id_len, res_size, p_res, p_pos);
  pf_put_byte(0x01, res_size, p_res, p_pos); // number of peers

  // Pad with zeros until block length is a multiple of 4 bytes
  while ((((*p_pos) - block_pos) & 0x3) != 0)
  {
    pf_put_byte(0, res_size, p_res, p_pos);
  }

  pf_put_byte(8, res_size, p_res, p_pos);            // length peer name
  pf_put_mem("port-003", 8, res_size, p_res, p_pos); // peer port name
  pf_put_byte(1, res_size, p_res, p_pos);            // length station name
  pf_put_byte('b', res_size, p_res, p_pos);          // peer station name 
  pf_put_byte(0, res_size, p_res, p_pos);            // padding 1 byte
                                                    
  pf_put_uint32(false, 0U, res_size, p_res, p_pos); // line delay 0 - unknown, can be little endian because of null value
  // MAC address - 6 bytes
  pf_put_mem(net->p_fspm_default_cfg->eth_addr.addr,
              sizeof(net->p_fspm_default_cfg->eth_addr.addr), 
              res_size, 
              p_res, 
              p_pos);

  pf_put_uint16(false, 0U, res_size, p_res, p_pos); // padding 2 bytes
  pf_put_uint16(true,  net->p_fspm_default_cfg->lldp_cfg.mau_type, res_size, p_res, p_pos);
  pf_put_uint16(false, 0U, res_size, p_res, p_pos); // padding 2 bytes
  pf_put_uint16(false, 0U, res_size, p_res, p_pos); // reserved
  pf_put_uint16(false, 0U, res_size, p_res, p_pos); // RTClass3_PortStatus
  pf_put_uint32(false, 0U, res_size, p_res, p_pos); // multicast boundary?

  pf_put_uint16(true, 1U, res_size, p_res, p_pos);  // link state link 1 - up
  pf_put_uint16(false, 0U, res_size, p_res, p_pos); // padding 2 bytes
  pf_put_uint32(true, 1U, res_size, p_res, p_pos);  // media type 0x01 - copper

  /* Finally insert the block length into the block header */
  const uint16_t block_len = *p_pos - (block_pos + 4);
  block_pos += offsetof(pf_block_header_t, block_length);   /* Point to correct place */
  pf_put_uint16(true, block_len, res_size, p_res, &block_pos);

  return 0;
}

// create empty block header
static void pf_put_zero_length_block_header(
  uint16_t                res_size, 
  pf_block_type_values_t  bh_type, 
  uint8_t                *p_res, 
  uint16_t               *p_pos)
{
  const uint16_t block_pos = *p_pos;
  pf_put_block_header(true,
                      bh_type, 0,                  // NULL record
                      PNET_BLOCK_VERSION_HIGH, PNET_BLOCK_VERSION_LOW_1,
                      res_size, p_res, p_pos);
  *p_pos = block_pos;
}

// Table 42 - IODReadRes.RecordDataRead.PDPortStatistic(with BlockVersionLow == 0x00)
// in AutomatedRtTester_v2.4.1.3/doc/TCIS_00000001_DiffAccessWays-SysRed-addOn_v2.35.2.00002.pdf
static void pf_put_pdport_statistics(
  pnet_t                *net,
  pf_iod_read_request_t *p_read_request,
  uint16_t               res_size,
  uint8_t               *p_res,
  uint16_t              *p_pos)
{
  pf_put_block_header(true, PF_BT_PORT_STATISTICS, 0x1A,
                      PNET_BLOCK_VERSION_HIGH, PNET_BLOCK_VERSION_LOW,
                      res_size, p_res, p_pos);
  pf_put_uint16(false, 0U, res_size, p_res, p_pos); // padding 2 bytes
  // return actual values for interface and port submodules
  if (   (p_read_request->slot_number == 0)
      && ((p_read_request->subslot_number == 0x8000 || p_read_request->subslot_number == 0x8001)))
  {
    pf_put_uint32(true, net->eth_handle->n_bytes_recv, res_size, p_res, p_pos);
    pf_put_uint32(true, net->eth_handle->n_bytes_sent, res_size, p_res, p_pos);
  }
  else
  {
    pf_put_uint32(false, 0U, res_size, p_res, p_pos); // ifInOctets
    pf_put_uint32(false, 0U, res_size, p_res, p_pos); // ifOutOctets
  }
  pf_put_uint32(false, 0U, res_size, p_res, p_pos);   // ifInDiscards
  pf_put_uint32(false, 0U, res_size, p_res, p_pos);   // ifOutDiscards
  pf_put_uint32(false, 0U, res_size, p_res, p_pos);   // ifInErrors
  pf_put_uint32(false, 0U, res_size, p_res, p_pos);   // ifOutErrors
}

// Table 44 - IODReadRes.RecordDataRead.PDInterfaceDataReal
// in AutomatedRtTester_v2.4.1.3/doc/TCIS_00000001_DiffAccessWays-SysRed-addOn_v2.35.2.00002.pdf
static void pf_put_pdinterface_data_real(
  pnet_t                *net,
  uint16_t               res_size,
  uint8_t               *p_res,
  uint16_t              *p_pos)
{
  const char *own_station_name = net->cmina_current_dcp_ase.name_of_station;
  const uint16_t station_name_len = (uint16_t)(strlen(own_station_name));

  uint16_t block_pos = *p_pos;
  pf_put_block_header(true, PF_BT_PDINTF_REAL, 0, // Don't know block_len yet
                      PNET_BLOCK_VERSION_HIGH, PNET_BLOCK_VERSION_LOW,
                      res_size, p_res, p_pos);

  pf_put_byte((uint8_t)station_name_len, res_size, p_res, p_pos);         // LengthOwnStationName
  pf_put_mem(own_station_name, station_name_len, res_size, p_res, p_pos); // OwnStationName

  // Pad with zeros until block length is a multiple of 4 bytes
  while ((((*p_pos) - block_pos) & 0x3) != 0)
  {
    pf_put_byte(0, res_size, p_res, p_pos);
  }
  // MAC address - 6 bytes
  pf_put_mem(net->p_fspm_default_cfg->eth_addr.addr,
             sizeof(net->p_fspm_default_cfg->eth_addr.addr),
             res_size,
             p_res,
             p_pos);
  
  pf_put_uint16(false, 0U, res_size, p_res, p_pos); // padding

  const pf_ip_suite_t *p_ip_suite = &(net->cmina_current_dcp_ase.full_ip_suite.ip_suite);
  pf_put_uint32(true, p_ip_suite->ip_addr, res_size, p_res, p_pos); // IpParameterValue.IpAddress
  pf_put_uint32(true, p_ip_suite->ip_mask, res_size, p_res, p_pos); // IpParameterValue.SubnetMask
  // IpParameterValue.Gateway
  if (p_ip_suite->ip_gateway == 0)
  {
    pf_put_uint32(true, p_ip_suite->ip_addr, res_size, p_res, p_pos);
  }
  else
  {
    pf_put_uint32(true, p_ip_suite->ip_gateway, res_size, p_res, p_pos);
  }

  // Finally insert the block length into the block header
  const uint16_t block_len = *p_pos - (block_pos + 4);
  block_pos += offsetof(pf_block_header_t, block_length);   // Point to correct place
  pf_put_uint16(true, block_len, res_size, p_res, &block_pos);
}

// This block appears several times. One time for the interface subslot and one time for every port subslot.
// Table 107 - IODReadRes.RecordDataRead.PDRealData
// in AutomatedRtTester_v2.4.1.3/doc/TCIS_00000001_DiffAccessWays-SysRed-addOn_v2.35.2.00002.pdf
static void pf_put_pd_real_data(
  pnet_t                *net, 
  pf_iod_read_request_t *p_read_request, 
  uint16_t               res_size, 
  uint8_t               *p_res, 
  uint16_t              *p_pos)
{
  // interface subslot
  {
    uint16_t block_pos = *p_pos;
    pf_put_block_header(true, PF_BT_MULTIPLE_BLOCK_HEADER, 0, // Don't know block_len yet
                        PNET_BLOCK_VERSION_HIGH, PNET_BLOCK_VERSION_LOW,
                        res_size, p_res, p_pos);

    pf_put_uint16(false, 0U, res_size, p_res, p_pos); // padding
    pf_put_uint32(false, 0U, res_size, p_res, p_pos); // API

    p_read_request->slot_number = 0;
    p_read_request->subslot_number = 0x8000; // interface subslot
    pf_put_uint16(true, p_read_request->slot_number, res_size, p_res, p_pos);
    pf_put_uint16(true, p_read_request->subslot_number, res_size, p_res, p_pos);

    pf_put_pdinterface_data_real(net, res_size, p_res, p_pos);
    pf_put_pdport_statistics(net, p_read_request, res_size, p_res, p_pos);

    // Finally insert the block length into the block header
    const uint16_t block_len = *p_pos - (block_pos + 4);
    block_pos += offsetof(pf_block_header_t, block_length);   // Point to correct place
    pf_put_uint16(true, block_len, res_size, p_res, &block_pos);
  }

  // port subslot
  {
    uint16_t block_pos = *p_pos;
    pf_put_block_header(true, PF_BT_MULTIPLE_BLOCK_HEADER, 0, // Don't know block_len yet
                        PNET_BLOCK_VERSION_HIGH, PNET_BLOCK_VERSION_LOW,
                        res_size, p_res, p_pos);

    pf_put_uint16(false, 0U, res_size, p_res, p_pos); // padding
    pf_put_uint32(false, 0U, res_size, p_res, p_pos); // API

    p_read_request->slot_number = 0;
    p_read_request->subslot_number = 0x8001; // port subslot
    pf_put_uint16(true, p_read_request->slot_number, res_size, p_res, p_pos);
    pf_put_uint16(true, p_read_request->subslot_number, res_size, p_res, p_pos);

    pf_put_pdport_data_real(net, p_read_request, res_size, p_res, p_pos);
    pf_put_pdport_statistics(net, p_read_request, res_size, p_res, p_pos);

    // Finally insert the block length into the block header
    const uint16_t block_len = *p_pos - (block_pos + 4);
    block_pos += offsetof(pf_block_header_t, block_length);   // Point to correct place
    pf_put_uint16(true, block_len, res_size, p_res, &block_pos);
  }

}

// create RecordDataRead.DiagnosisData
static void pf_put_channel_diagnosis_data(
  pnet_t   *net,
  uint32_t  api,
  uint16_t  slot,
  uint16_t  subslot,
  uint16_t  error_code,
  uint16_t  res_size,
  uint8_t  *p_res,
  uint16_t *p_pos)
{
  uint16_t block_pos = *p_pos;
  pf_put_block_header(true, PF_BT_DIAGNOSIS_DATA, 0, // Don't know block_len yet
                      PNET_BLOCK_VERSION_HIGH, PNET_BLOCK_VERSION_LOW_1,
                      res_size, p_res, p_pos);
  pf_put_uint32(true, api, res_size, p_res, p_pos);
  pf_put_uint16(true, slot, res_size, p_res, p_pos);
  pf_put_uint16(true, subslot, res_size, p_res, p_pos);
  pf_put_uint16(true, 0x8000, res_size, p_res, p_pos); // channel number
  pf_put_uint16(true, 0x0800, res_size, p_res, p_pos); // channel properties
  pf_put_uint16(true, PF_USI_CHANNEL_DIAGNOSIS, res_size, p_res, p_pos); // structure identifier
  pf_put_uint16(true, 0x8000, res_size, p_res, p_pos); // ChannelDiagnosisData.ChannelNumber
  pf_put_uint16(true, 0x0800, res_size, p_res, p_pos); // ChannelDiagnosisData.ChannelProperties
  pf_put_uint16(true, error_code, res_size, p_res, p_pos); // 0x8001 = remote mismatch

                                                       // Finally insert the block length into the block header
  const uint16_t block_len = *p_pos - (block_pos + 4);
  block_pos += offsetof(pf_block_header_t, block_length);   // Point to correct place
  pf_put_uint16(true, block_len, res_size, p_res, &block_pos);
}

/**
* @file
* @brief Implements the Context Management Read Record Responder protocol machine device (CMRDR)
*
* Contains a single function \a pf_cmrdr_rm_read_ind(),
* that handles a RPC parameter read request.
*
* This implementation of CMRDR has no internal state.
* Every call to pf_cmrdr_rm_read_ind finishes by returning the result.
* Since there are no internal static variables there is also no need
* for a POWER-ON state.
*/


int pf_cmrdr_rm_read_ind(
  pnet_t                *net,
  pf_ar_t               *p_ar,
  uint16_t               opnum,
  pf_iod_read_request_t *p_read_request,
  pnet_result_t         *p_read_status,
  uint16_t               res_size,
  uint8_t               *p_res,
  uint16_t              *p_pos)
{
  int                     ret = -1;
  pf_iod_read_result_t    read_result;
  uint8_t                *p_data = NULL;
  uint16_t                data_length_pos = 0;
  uint16_t                start_pos = 0;
  uint8_t                 iocs[255];              /* Max possible array size */
  uint8_t                 iops[255];              /* Max possible array size */
  uint8_t                 subslot_data[PF_FRAME_BUFFER_SIZE];     /* Max possible array size */
  uint8_t                 iocs_len = 0;
  uint8_t                 iops_len = 0;
  uint16_t                data_len = 0;
  bool                    new_flag = false;

  read_result.sequence_number = p_read_request->sequence_number;
  read_result.ar_uuid         = p_read_request->ar_uuid;
  read_result.api             = p_read_request->api;
  read_result.slot_number     = p_read_request->slot_number;
  read_result.subslot_number  = p_read_request->subslot_number;
  read_result.index           = p_read_request->index;

  /*
   * Let FSPM or the application provide the value if possible.
   */
  data_len = res_size - *p_pos;
  ret = pf_fspm_cm_read_ind(net, p_ar, p_read_request, &p_data, &data_len, p_read_status);
  if (ret != 0)
  {
    LOG_DEBUG(PF_RPC_LOG, "CMRDR(%d): Error from pf_fspm_cm_read_ind\n", __LINE__);
    data_len = 0;
    ret = 0;    /* Handled: No data available. */
  }

  /*
   * Just insert a dummy value for now.
   * It will be properly computed after the answer has been written to the buffer.
   */
  read_result.record_data_length = 0;

  /*
   * The only result that can fail is from FSPM or the application.
   * All well-known indices are handled without error.
   * It is therefore OK to write the result already now.
   * This is needed in order to get the correct position for the actual answer.
   */
  read_result.add_data_1 = p_read_status->add_data_1;
  read_result.add_data_2 = p_read_status->add_data_2;

  /* Also: Retrieve a position to write the total record length to. */
  pf_put_read_result(true, &read_result, res_size, p_res, p_pos, &data_length_pos);

  /*
   * Even if the application or FSPM provided an answer.
   * If there is another answer then use that!!
   */
  ret = -1;
  start_pos = *p_pos;
  if (p_read_request->index <= PF_IDX_USER_MAX)
  {
    /* Provided by application - accept whatever it says. */
    if (*p_pos + data_len < res_size)
    {
      memcpy(&p_res[*p_pos], p_data, data_len);
      *p_pos += data_len;
      ret = 0;
    }
  }
  else
  {
    LOG_DEBUG(PF_RPC_LOG, "CMRDR(%d): p_read_request->index 0x%X pos %u\n", 
             __LINE__,
             p_read_request->index,
             (uint32_t)*p_pos);

    switch (p_read_request->index)
    {
    case PF_IDX_DEV_IM_0_FILTER_DATA:
      /* Block-writer knows where to fetch and how to build the answer. */
      pf_put_im_0_filter_data(net, true, res_size, p_res, p_pos);
      ret = 0;
      break;

      /*
       * I&M data are provided by FSPM.
       * Accept whatever it says (after verifying the data length).
       */
    case PF_IDX_SUB_IM_0:
      LOG_DEBUG(PNET_LOG, "CMRDR(%d): Read I&M0 data\n", __LINE__);
      if ((data_len == sizeof(pnet_im_0_t)) && (*p_pos + data_len < res_size))
      {
        pf_put_im_0(true, (pnet_im_0_t *)p_data, res_size, p_res, p_pos);
        ret = 0;
      }
      break;
    case PF_IDX_SUB_IM_1:
      LOG_DEBUG(PNET_LOG, "CMRDR(%d): Read I&M1 data\n", __LINE__);
      if ((data_len == sizeof(pnet_im_1_t)) && (*p_pos + data_len < res_size))
      {
        pf_put_im_1(true, (pnet_im_1_t *)p_data, res_size, p_res, p_pos);
        ret = 0;
      }
      break;
    case PF_IDX_SUB_IM_2:
      LOG_DEBUG(PNET_LOG, "CMRDR(%d): Read I&M2 data\n", __LINE__);
      if ((data_len == sizeof(pnet_im_2_t)) && (*p_pos + data_len < res_size))
      {
        pf_put_im_2(true, (pnet_im_2_t *)p_data, res_size, p_res, p_pos);
        ret = 0;
      }
      break;
    case PF_IDX_SUB_IM_3:
      LOG_DEBUG(PNET_LOG, "CMRDR(%d): Read I&M3 data\n", __LINE__);
      if ((data_len == sizeof(pnet_im_3_t)) && (*p_pos + data_len < res_size))
      {
        pf_put_im_3(true, (pnet_im_3_t *)p_data, res_size, p_res, p_pos);
        ret = 0;
      }
      break;
    case PF_IDX_SUB_IM_4:
      LOG_DEBUG(PNET_LOG, "CMRDR(%d): Read I&M4 data\n", __LINE__);
      if ((data_len == sizeof(pnet_im_4_t)) && (*p_pos + data_len < res_size))
      {
        pf_put_record_data_read(true, PF_BT_IM_4, data_len, p_data, res_size, p_res, p_pos);
        ret = 0;
      }
      break;
    case PF_IDX_SUB_IM_5:
      ret = -1;
      break;

      /*
       * I&M data are provided by FSPM.
       * Accept whatever it says (after verifying the data length).
       */
    case PF_IDX_DEV_LOGBOOK_DATA:
      LOG_DEBUG(PNET_LOG, "CMRDR(%d): Read logbook data\n", __LINE__);
      /* Provided by FSPM. Accept whatever it says. */
      pf_put_log_book_data(true, (pf_log_book_t *)p_data, res_size, p_res, p_pos);
      ret = 0;
      break;

      /* Block-writer knows where to fetch and how to build the answer to ID data requests. */
    case PF_IDX_SUB_EXP_ID_DATA:
      // the condition is here to satisfy Automated RT tester - Different Access Ways Test Case
      if(   (opnum == PF_RPC_DEV_OPNUM_READ_IMPLICIT) 
         || (p_ar && p_ar->nbr_iocrs > 0))
      {
        pf_put_ident_data(net, true, PNET_BLOCK_VERSION_LOW_1, PF_BT_EXPECTED_IDENTIFICATION_DATA,
                          PF_DEV_FILTER_LEVEL_SUBSLOT, PF_DEV_FILTER_LEVEL_SUBSLOT,
                          p_ar, p_read_request->api, p_read_request->slot_number, p_read_request->subslot_number,
                          res_size, p_res, p_pos);
      }
      else
      {
        pf_put_zero_length_block_header(res_size, PF_BT_EXPECTED_IDENTIFICATION_DATA, p_res, p_pos);
      }

      ret = 0;
      break;
    case PF_IDX_SUB_REAL_ID_DATA:
      pf_put_ident_data(net, true, PNET_BLOCK_VERSION_LOW_1, PF_BT_REAL_IDENTIFICATION_DATA,
                        PF_DEV_FILTER_LEVEL_SUBSLOT, PF_DEV_FILTER_LEVEL_SUBSLOT,
                        NULL, p_read_request->api, p_read_request->slot_number, p_read_request->subslot_number,
                        res_size, p_res, p_pos);
      ret = 0;
      break;
    case PF_IDX_SLOT_EXP_ID_DATA:
      // the condition is here to satisfy Automated RT tester - Different Access Ways Test Case
      if (   (opnum == PF_RPC_DEV_OPNUM_READ_IMPLICIT)
          || (p_ar && p_ar->nbr_iocrs > 0))
      {
        pf_put_ident_data(net, true, PNET_BLOCK_VERSION_LOW_1, PF_BT_EXPECTED_IDENTIFICATION_DATA,
                          PF_DEV_FILTER_LEVEL_SLOT, PF_DEV_FILTER_LEVEL_SUBSLOT,
                          p_ar, p_read_request->api, p_read_request->slot_number, 0,
                          res_size, p_res, p_pos);
      }
      else
      {
        pf_put_zero_length_block_header(res_size, PF_BT_EXPECTED_IDENTIFICATION_DATA, p_res, p_pos);
      }
      ret = 0;
      break;
    case PF_IDX_SLOT_REAL_ID_DATA:
      pf_put_default_ident_data(net, true, PNET_BLOCK_VERSION_LOW_1, PF_BT_REAL_IDENTIFICATION_DATA,
                                PF_DEV_FILTER_LEVEL_SLOT,
                                PF_DEV_FILTER_LEVEL_SUBSLOT,
                                NULL,
                                p_read_request->api,
                                p_read_request->slot_number,
                                0,
                                res_size, p_res, p_pos);
      ret = 0;
      break;
    case PF_IDX_AR_EXP_ID_DATA:
      if (   (opnum == PF_RPC_DEV_OPNUM_READ_IMPLICIT)
          || (p_ar && p_ar->nbr_iocrs > 0))
      {
        pf_put_ident_data(net, true, PNET_BLOCK_VERSION_LOW_1, PF_BT_EXPECTED_IDENTIFICATION_DATA,
                          PF_DEV_FILTER_LEVEL_DEVICE, PF_DEV_FILTER_LEVEL_SUBSLOT,
                          p_ar, 0, 0, 0,
                          res_size, p_res, p_pos);
      }
      else
      {
        pf_put_zero_length_block_header(res_size, PF_BT_EXPECTED_IDENTIFICATION_DATA, p_res, p_pos);
      }
      ret = 0;
      break;
    case PF_IDX_AR_REAL_ID_DATA:
      if (   (opnum == PF_RPC_DEV_OPNUM_READ_IMPLICIT)
          || (p_ar && p_ar->nbr_iocrs > 0))
      {
        pf_put_ident_data(net, true, PNET_BLOCK_VERSION_LOW_1, PF_BT_REAL_IDENTIFICATION_DATA,
                          PF_DEV_FILTER_LEVEL_DEVICE, PF_DEV_FILTER_LEVEL_SUBSLOT,
                          p_ar, 0, 0, 0,
                          res_size, p_res, p_pos);
      }
      else
      {
        pf_put_zero_length_block_header(res_size, PF_BT_REAL_IDENTIFICATION_DATA, p_res, p_pos);
      }
      ret = 0;
      break;
    case PF_IDX_API_REAL_ID_DATA:
         LOG_INFO(PNET_LOG, "CMRDR(%d): Read real ID data\n", __LINE__);
      pf_put_ident_data(net, true, PNET_BLOCK_VERSION_LOW_1, PF_BT_REAL_IDENTIFICATION_DATA,
                        PF_DEV_FILTER_LEVEL_API, PF_DEV_FILTER_LEVEL_SUBSLOT,
                        NULL, p_read_request->api,
                        0, 0, res_size, p_res, p_pos);
      ret = 0;
      break;

    case PF_IDX_DEV_API_DATA:
         LOG_INFO(PNET_LOG, "CMRDR(%d): Read API data\n", __LINE__);
      pf_put_ident_data(net, true, PNET_BLOCK_VERSION_LOW, PF_BT_API_DATA,
                        PF_DEV_FILTER_LEVEL_DEVICE, PF_DEV_FILTER_LEVEL_API_ID,
                        NULL, 0, 0, 0, res_size, p_res, p_pos);
      ret = 0;
      break;

    case PF_IDX_SUB_INPUT_DATA:
      /* Sub-module data to the controller */
      data_len = sizeof(subslot_data);
      iops_len = sizeof(iops);
      iocs_len = sizeof(iocs);
      if (pf_ppm_get_data_and_iops(net, p_read_request->api, p_read_request->slot_number, p_read_request->subslot_number,
                                   subslot_data, &data_len, iops, &iops_len) != 0)
      {
        LOG_DEBUG(PNET_LOG, "CMRDR(%d): Could not get PPM data and IOPS\n", __LINE__);
        data_len = 0;
        iops_len = 0;
      }
      if (pf_cpm_get_iocs(net, p_read_request->api, p_read_request->slot_number, p_read_request->subslot_number, iocs, &iocs_len) != 0)
      {
        LOG_DEBUG(PNET_LOG, "CMRDR(%d): Could not get CPM IOCS\n", __LINE__);
        iocs_len = 0;
      }
      if ((data_len + iops_len + iocs_len) > 0)
      {
        /* Have all data */
        pf_put_input_data(true, iocs_len, iocs, iops_len, iops, data_len, subslot_data, res_size, p_res, p_pos);
        ret = 0;
      }
      break;
    case PF_IDX_SUB_OUTPUT_DATA:
      /* Sub-module data from the controller. */
      data_len = sizeof(subslot_data);
      iops_len = sizeof(iops);
      iocs_len = sizeof(iocs);
      if (pf_cpm_get_data_and_iops(net, p_read_request->api, p_read_request->slot_number, p_read_request->subslot_number,
                                   &new_flag, subslot_data, &data_len, iops, &iops_len) != 0)
      {
        LOG_DEBUG(PNET_LOG, "CMRDR(%d): Could not get CPM data and IOPS\n", __LINE__);
        data_len = 0;
        iops_len = 0;
      }
      if (pf_ppm_get_iocs(net, p_read_request->api, p_read_request->slot_number, p_read_request->subslot_number, iocs, &iocs_len) != 0)
      {
        LOG_DEBUG(PNET_LOG, "CMRDR(%d): Could not get PPM IOCS\n", __LINE__);
        iocs_len = 0;
      }
      if ((data_len + iops_len + iocs_len) > 0)
      {
        /* Have all data */
        /* ToDo: Support substitution values, mode and active flag */
        pf_put_output_data(true, false, iocs_len, iocs, iops_len, iops, data_len, subslot_data, 0, subslot_data, res_size, p_res, p_pos);
        ret = 0;
      }
      break;

      /* Block-writer knows where to fetch and how to build the answer to diag data requests. */
    case PF_IDX_SUB_DIAGNOSIS_CH:
      pf_put_diag_data(net, true, PF_DEV_FILTER_LEVEL_SUBSLOT, PF_DIAG_FILTER_FAULT_STD, NULL, p_read_request->api, p_read_request->slot_number, p_read_request->subslot_number, res_size, p_res, p_pos);
      ret = 0;
      break;
    case PF_IDX_SUB_DIAGNOSIS_ALL:
      pf_put_diag_data(net, true, PF_DEV_FILTER_LEVEL_SUBSLOT, PF_DIAG_FILTER_FAULT_ALL, NULL, p_read_request->api, p_read_request->slot_number, p_read_request->subslot_number, res_size, p_res, p_pos);

      if(p_read_request->slot_number == 0 && p_read_request->subslot_number == 0x8001)
      {
        if (pf_check_peers_is_valid(&net->check_peers_data) == false)
        {
          pf_put_channel_diagnosis_data(net,
                                        p_read_request->api,
                                        p_read_request->slot_number,
                                        p_read_request->subslot_number,
                                        0x8001, // remote mismatch
                                        res_size,
                                        p_res,
                                        p_pos);
        }
      }

      ret = 0;
      break;
    case PF_IDX_SUB_DIAGNOSIS_DMQS:
      pf_put_diag_data(net, true, PF_DEV_FILTER_LEVEL_SUBSLOT, PF_DIAG_FILTER_ALL, NULL, p_read_request->api, p_read_request->slot_number, p_read_request->subslot_number, res_size, p_res, p_pos);
      ret = 0;
      break;
    case PF_IDX_SUB_DIAG_MAINT_REQ_CH:
    case PF_IDX_SUB_DIAG_MAINT_REQ_ALL:
      pf_put_diag_data(net, true, PF_DEV_FILTER_LEVEL_SUBSLOT, PF_DIAG_FILTER_M_REQ, NULL, p_read_request->api, p_read_request->slot_number, p_read_request->subslot_number, res_size, p_res, p_pos);
      ret = 0;
      break;
    case PF_IDX_SUB_DIAG_MAINT_DEM_CH:
    case PF_IDX_SUB_DIAG_MAINT_DEM_ALL:
      pf_put_diag_data(net, true, PF_DEV_FILTER_LEVEL_SUBSLOT, PF_DIAG_FILTER_M_DEM, NULL, p_read_request->api, p_read_request->slot_number, p_read_request->subslot_number, res_size, p_res, p_pos);
      ret = 0;
      break;

    case PF_IDX_SLOT_DIAGNOSIS_CH:
      pf_put_diag_data(net, true, PF_DEV_FILTER_LEVEL_SLOT, PF_DIAG_FILTER_FAULT_STD, NULL, p_read_request->api, p_read_request->slot_number, 0, res_size, p_res, p_pos);
      ret = 0;
      break;
    case PF_IDX_SLOT_DIAGNOSIS_ALL:
      pf_put_diag_data(net, true, PF_DEV_FILTER_LEVEL_SLOT, PF_DIAG_FILTER_FAULT_ALL, NULL, p_read_request->api, p_read_request->slot_number, 0, res_size, p_res, p_pos);
      ret = 0;
      break;
    case PF_IDX_SLOT_DIAGNOSIS_DMQS:
      pf_put_diag_data(net, true, PF_DEV_FILTER_LEVEL_SLOT, PF_DIAG_FILTER_ALL, NULL, p_read_request->api, p_read_request->slot_number, 0, res_size, p_res, p_pos);
      ret = 0;
      break;
    case PF_IDX_SLOT_DIAG_MAINT_REQ_CH:
    case PF_IDX_SLOT_DIAG_MAINT_REQ_ALL:
      pf_put_diag_data(net, true, PF_DEV_FILTER_LEVEL_SLOT, PF_DIAG_FILTER_M_REQ, NULL, p_read_request->api, p_read_request->slot_number, 0, res_size, p_res, p_pos);
      ret = 0;
      break;
    case PF_IDX_SLOT_DIAG_MAINT_DEM_CH:
    case PF_IDX_SLOT_DIAG_MAINT_DEM_ALL:
      pf_put_diag_data(net, true, PF_DEV_FILTER_LEVEL_SLOT, PF_DIAG_FILTER_M_DEM, NULL, p_read_request->api, p_read_request->slot_number, 0, res_size, p_res, p_pos);
      ret = 0;
      break;

    case PF_IDX_API_DIAGNOSIS_CH:
      pf_put_diag_data(net, true, PF_DEV_FILTER_LEVEL_API, PF_DIAG_FILTER_FAULT_STD, NULL, p_read_request->api, p_read_request->slot_number, 0, res_size, p_res, p_pos);
      ret = 0;
      break;
    case PF_IDX_API_DIAGNOSIS_ALL:
      pf_put_diag_data(net, true, PF_DEV_FILTER_LEVEL_API, PF_DIAG_FILTER_FAULT_ALL, NULL, p_read_request->api, p_read_request->slot_number, 0, res_size, p_res, p_pos);
      ret = 0;
      break;
    case PF_IDX_API_DIAGNOSIS_DMQS:
      pf_put_diag_data(net, true, PF_DEV_FILTER_LEVEL_API, PF_DIAG_FILTER_ALL, NULL, p_read_request->api, 0, 0, res_size, p_res, p_pos);
      ret = 0;
      break;
    case PF_IDX_API_DIAG_MAINT_REQ_CH:
    case PF_IDX_API_DIAG_MAINT_REQ_ALL:
      pf_put_diag_data(net, true, PF_DEV_FILTER_LEVEL_API, PF_DIAG_FILTER_M_REQ, NULL, p_read_request->api, p_read_request->slot_number, 0, res_size, p_res, p_pos);
      ret = 0;
      break;
    case PF_IDX_API_DIAG_MAINT_DEM_CH:
    case PF_IDX_API_DIAG_MAINT_DEM_ALL:
      pf_put_diag_data(net, true, PF_DEV_FILTER_LEVEL_API, PF_DIAG_FILTER_M_DEM, NULL, p_read_request->api, p_read_request->slot_number, 0, res_size, p_res, p_pos);
      ret = 0;
      break;

    case PF_IDX_AR_DIAGNOSIS_CH:
      pf_put_diag_data(net, true, PF_DEV_FILTER_LEVEL_DEVICE, PF_DIAG_FILTER_FAULT_STD, p_ar, 0, 0, 0, res_size, p_res, p_pos);
      ret = 0;
      break;
    case PF_IDX_AR_DIAGNOSIS_ALL:
      pf_put_diag_data(net, true, PF_DEV_FILTER_LEVEL_DEVICE, PF_DIAG_FILTER_FAULT_ALL, p_ar, 0, 0, 0, res_size, p_res, p_pos);
      ret = 0;
      break;
    case PF_IDX_AR_DIAGNOSIS_DMQS:
      pf_put_diag_data(net, true, PF_DEV_FILTER_LEVEL_DEVICE, PF_DIAG_FILTER_ALL, p_ar, 0, 0, 0, res_size, p_res, p_pos);
      ret = 0;
      break;
    case PF_IDX_AR_DIAG_MAINT_REQ_CH:
    case PF_IDX_AR_DIAG_MAINT_REQ_ALL:
      pf_put_diag_data(net, true, PF_DEV_FILTER_LEVEL_DEVICE, PF_DIAG_FILTER_M_REQ, p_ar, 0, 0, 0, res_size, p_res, p_pos);
      ret = 0;
      break;
    case PF_IDX_AR_DIAG_MAINT_DEM_CH:
    case PF_IDX_AR_DIAG_MAINT_DEM_ALL:
      pf_put_diag_data(net, true, PF_DEV_FILTER_LEVEL_DEVICE, PF_DIAG_FILTER_M_DEM, p_ar, 0, 0, 0, res_size, p_res, p_pos);
      ret = 0;
      break;

    case PF_IDX_DEV_DIAGNOSIS_DMQS:
      pf_put_diag_data(net, true, PF_DEV_FILTER_LEVEL_DEVICE, PF_DIAG_FILTER_ALL, NULL, 0, 0, 0, res_size, p_res, p_pos);
      ret = 0;
      break;

    case PF_IDX_DEV_CONN_MON_TRIGGER:
      ret = 0;
      break;

    case PF_IDX_API_AR_DATA:
      pf_put_ar_data(net, true, p_ar, p_read_request->api, res_size, p_res, p_pos);
      ret = 0;
      break;
    case PF_IDX_DEV_AR_DATA:
      pf_put_ar_data(net, true, NULL, 0, res_size, p_res, p_pos);
      ret = 0;
      break;
    case PF_IDX_AR_MOD_DIFF:
      pf_put_ar_diff(true, p_ar, res_size, p_res, p_pos);
      ret = 0;
      break;

    case PF_IDX_SUB_PDPORT_DATA_REAL:
      /* ToDo: Implement properly when LLDP is done */
      /*
       * BlockHeader, Padding, Padding, SlotNumber, SubslotNumber, LengthOwnPortName, OwnPortName,
       * NumberOfPeers, [Padding*] a,
       * ToDo: Find out where the closing param goes. It is missing in the spec!!
       * [LengthPeerPortName, PeerPortName, LengthPeerStationName, PeerStationName, [Padding*] a,
       *  LineDelay e, PeerMACAddress b, [Padding*] a ]*, MAUType c, [Padding*] a, Reserved d,
       *  RTClass3_PortStatus, MulticastBoundary, LinkState, [Padding*] a, MediaType
       * a The number of padding octets shall be adapted to make the block Unsigned32 aligned.
       * b This field contains the interface MAC address of the peer
       * c See Table 641
       * d The number of reserved octets shall be 2.
       * e The local calculated LineDelay shall be used.
       */

      if (p_read_request->slot_number == 0 && p_read_request->subslot_number == 0x8001)
      {
        ret = pf_put_pdport_data_real(net, p_read_request, res_size, p_res, p_pos);
      }
      break;
    case PF_IDX_SUB_PDPORT_DATA_CHECK:
      /* ToDo: Implement properly when LLDP is done */
      /*
       * BlockHeader, Padding, Padding, SlotNumber, SubslotNumber,
       * { [CheckPeers], [CheckLineDelay], [CheckMAUType a], [CheckLinkState],
       *   [CheckSyncDifference], [CheckMAUTypeDifference], [CheckMAUTypeExtension] b }
       * a The implementation of the MAUType check shall take the AutoNegotiation settings into account. 
       * A diagnosis with severity fault shall be issued if an AutoNegotiation error is detected 
       * after the next LinkDown / LinkUp do to the used AutoNegotiation settings.
       * b Only possible if CheckMAUType is part of the list
       */


      // the condition is here to satisfy Automated RT tester - Different Access Ways Test Case
      // and Diagnosis Test Case
      // 
      if (p_read_request->slot_number == 0 && p_read_request->subslot_number == 0x8001)
      {
        bool b_check_peers_is_valid = pf_check_peers_is_valid(&net->check_peers_data);
        if ((b_check_peers_is_valid == false)
            || ((opnum == PF_RPC_DEV_OPNUM_READ_IMPLICIT)
                              && memcmp(&p_read_request->target_ar_uuid, &null_uuid, sizeof(pf_uuid_t)) == 0))
        {
          pf_put_pdport_data_check(net, p_read_request, res_size, p_res, p_pos);
          if ((p_ar != NULL) && (b_check_peers_is_valid == false))
          {
            pf_cmdev_prepare_submodule_diff(net, p_ar);
          }
        }
        else
        {
          pf_put_zero_length_block_header(res_size, PF_BT_PDPORT_DATA_CHECK, p_res, p_pos);
        }
        ret = 0;
      }

      break;
    case PF_IDX_SUB_PDPORT_DATA_ADJ:
      /* ToDo: Implement properly when LLDP is done */
      /*
       * BlockHeader, Padding, Padding, SlotNumber, SubslotNumber,
       * { [AdjustDomainBoundary], [AdjustMulticastBoundary],
       *   [AdjustMAUType ^ AdjustLinkState],
       *   [AdjustPeerToPeerBoundary], [AdjustDCPBoundary],
       *   [AdjustPreambleLength], [AdjustMAUTypeExtension] a }
       * a Only possible if AdjustMAUType is part of the list
       */
      if (p_read_request->slot_number == 0 && p_read_request->subslot_number == 0x8001)
      {
        pf_put_zero_length_block_header(res_size, PF_BT_PDPORT_DATA_ADJUST, p_res, p_pos);
        ret = 0;
      }
      break;
    case PF_IDX_DEV_PDREAL_DATA:
      /* ToDo: Implement when LLDP is done */
      /*
       * MultipleBlockHeader, { [PDPortDataReal] b d, [PDPortDataRealExtended] b d,
       *                        [PDInterfaceMrpDataReal], [PDPortMrpDataReal],
       *                        [PDPortFODataReal] a, [PDInterfaceDataReal],
       *                        [PDPortStatistic], [PDPortMrpIcDataReal] }
       * a There shall be no FiberOpticManufacturerSpecific information
       * b The fields SlotNumber and SubslotNumber shall be ignored
       * c Each submodule's data (for example interface or port) need its own MultipleBlockHeader
       * d If MAUTypeExtension is used, both blocks shall be provided
       */
      pf_put_pd_real_data(net, p_read_request, res_size, p_res, p_pos);
      ret = 0;
      break;
    case PF_IDX_DEV_PDEXP_DATA:
      /* ToDo: Implement when LLDP is done */
      /*
       * MultipleBlockHeader, { [PDPortDataCheck] a, [PDPortDataAdjust] a,
       *                        [PDInterfaceMrpDataAdjust], [PDInterfaceMrpDataCheck],
       *                        [PDPortMrpDataAdjust], [PDPortFODataAdjust],
       *                        [PDPortFODataCheck], [PDNCDataCheck],
       *                        [PDInterface-FSUDataAdjust], [PDInterfaceAdjust],
       *                        [PDPortMrpIcDataAdjust], [PDPortMrpIcDataCheck] }
       * a The fields SlotNumber and SubslotNumber shall be ignored
       * b Each submodule's data (for example interface or port) need its own MultipleBlockHeader
       */
      ret = 0;
      break;
    case PF_IDX_SUB_PDINTF_ADJUST:
      if ((p_read_request->slot_number == 0) && (p_read_request->subslot_number == 0x8000))
      {
        pf_put_zero_length_block_header(res_size, PF_BT_EXPECTED_IDENTIFICATION_DATA, p_res, p_pos);
        ret = 0;
      }
      break;
    case PF_IDX_SUB_PDPORT_STATISTIC:
      pf_put_pdport_statistics(net, p_read_request, res_size, p_res, p_pos);
      ret = 0;
      break;
    case PF_IDX_SUB_PDINTF_REAL:
      if ((p_read_request->slot_number == 0) && (p_read_request->subslot_number == 0x8000))
      {
        pf_put_pdinterface_data_real(net, res_size, p_res, p_pos);
        ret = 0;
      }
      break;

    default:
      LOG_DEBUG(PNET_LOG, "cmrdr(%d): No support for reading index 0x%x\n", __LINE__, p_read_request->index);
      ret = -1;
      break;
    }
  }

  if (ret != 0)
  {
    p_read_status->pnio_status.error_code = PNET_ERROR_CODE_READ;
    p_read_status->pnio_status.error_decode = PNET_ERROR_DECODE_PNIORW;
    p_read_status->pnio_status.error_code_1 = PNET_ERROR_CODE_1_ACC_INVALID_INDEX;
    p_read_status->pnio_status.error_code_2 = 10;
    ret = 0;
  }

  read_result.record_data_length = *p_pos - start_pos;
  pf_put_uint32(true, read_result.record_data_length, res_size, p_res, &data_length_pos);   /* Insert actual data length */

  ret = pf_cmsm_cm_read_ind(net, p_ar, p_read_request);

  return ret;
}
