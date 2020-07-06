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
#include <stdlib.h>
#include "pf_includes.h"
#include "pf_block_reader.h"
#include "pf_block_writer.h"
#include "pf_cmwrr.h"


void pf_cmwrr_init(
  pnet_t                  *net)
{
  net->cmwrr_state = PF_CMWRR_STATE_IDLE;
}

/**
 * @internal
 * Return a string representation of a CMWRR state.
 * @param state            In:   The CMWRR state
 * @return A string representation of the CMWRR state.
 */
static const char *pf_cmwrr_state_to_string(
  pf_cmwrr_state_values_t state)
{
  const char *s = "<unknown>";

  switch (state)
  {
  case PF_CMWRR_STATE_IDLE:
    s = "PF_CMWRR_STATE_IDLE";
    break;
  case PF_CMWRR_STATE_STARTUP:
    s = "PF_CMWRR_STATE_STARTUP";
    break;
  case PF_CMWRR_STATE_PRMEND:
    s = "PF_CMWRR_STATE_PRMEND";
    break;
  case PF_CMWRR_STATE_DATA:
    s = "PF_CMWRR_STATE_DATA";
    break;
  }

  return s;
}

void pf_cmwrr_show(
  pnet_t                  *net,
  pf_ar_t                 *p_ar)
{
  const char *s = pf_cmwrr_state_to_string(net->cmwrr_state);

  printf("CMWRR state           = %s\n", s);
  (void)s;
}

int pf_cmwrr_cmdev_state_ind(
  pnet_t                  *net,
  pf_ar_t                 *p_ar,
  pnet_event_values_t     event)
{
  switch (net->cmwrr_state)
  {
  case PF_CMWRR_STATE_IDLE:
    if (event == PNET_EVENT_STARTUP)
    {
      net->cmwrr_state = PF_CMWRR_STATE_STARTUP;
    }
    break;
  case PF_CMWRR_STATE_STARTUP:
    if (event == PNET_EVENT_PRMEND)
    {
      net->cmwrr_state = PF_CMWRR_STATE_PRMEND;
    }
    else if (event == PNET_EVENT_ABORT)
    {
      net->cmwrr_state = PF_CMWRR_STATE_IDLE;
    }
    break;
  case PF_CMWRR_STATE_PRMEND:
    if (event == PNET_EVENT_ABORT)
    {
      net->cmwrr_state = PF_CMWRR_STATE_IDLE;
    }
    else if (event == PNET_EVENT_APPLRDY)
    {
      net->cmwrr_state = PF_CMWRR_STATE_DATA;
    }
    break;
  case PF_CMWRR_STATE_DATA:
    if (event == PNET_EVENT_ABORT)
    {
      net->cmwrr_state = PF_CMWRR_STATE_IDLE;
    }
    break;
  default:
    LOG_ERROR(PNET_LOG, "CMWRR(%d): BAD state in cmwrr %u\n", __LINE__, (unsigned)net->cmwrr_state);
    break;
  }

  return -1;
}

// check the validity of check peers data
// the checks are just guess for now
bool pf_check_peers_is_valid(pf_check_peers_t *p_check_peers)
{
  // port-001, port-002, port-003 allowed
  if (p_check_peers->length_peer_port_id != 8)
  {
    return false;
  }
  if (strncmp(p_check_peers->peer_port_id, "port-", 5) != 0)
  {
    return false;
  }
  if (strtol(&p_check_peers->peer_port_id[5], NULL, 0) > 3)
  {
    return false;
  }
  if (p_check_peers->length_peer_chassis_id != 1)
  {
    return false;
  }
  if (   (p_check_peers->peer_chassis_id[0] != 'a')
      && (p_check_peers->peer_chassis_id[0] != 'b'))
  {
    return false;
  }
  return true;
}

// read PDPortCheckData from controller and store to net structure
static int pf_write_pd_port_data_check(
  pnet_t                 *net, 
  pf_ar_t                *p_ar,
  uint16_t                data_length,
  uint8_t                *p_req_buf)
{
  if (data_length < sizeof(pf_block_header_t) + 6) // sizeof block_header_t + padding + slot + subslot
  {
    return -1;
  }

  uint16_t      req_pos = 0;
  pf_get_info_t get_info;
  get_info.result = PF_PARSE_OK;
  get_info.p_buf = p_req_buf;
  get_info.is_big_endian = true;
  get_info.len = data_length;      /* Bytes in input buffer */

  // from wireshark:
  // padding 2 bytes, slot 2 bytes, subslot 2 bytes
  (void)pf_get_uint16(&get_info, &req_pos);
  const uint16_t slot = pf_get_uint16(&get_info, &req_pos);
  const uint16_t subslot = pf_get_uint16(&get_info, &req_pos);

  pf_block_header_t hdr_host;
  pf_get_block_header(&get_info, &req_pos, &hdr_host);

  if (hdr_host.block_type == PF_BT_CHECK_PEERS && slot == 0 && subslot == 0x8001)
  {
    pf_check_peers_t *p_dst_peers = &(net->check_peers_data);
    p_dst_peers->number_of_peers = pf_get_byte(&get_info, &req_pos);
    const uint8_t length_peer_port_id = pf_get_byte(&get_info, &req_pos) & 0xF; // max 15 chars
    p_dst_peers->length_peer_port_id = length_peer_port_id;
    pf_get_mem(&get_info, &req_pos, length_peer_port_id, p_dst_peers->peer_port_id);
    p_dst_peers->peer_port_id[length_peer_port_id] = '\0';

    const uint8_t length_peer_chassis_id = pf_get_byte(&get_info, &req_pos) & 0xF; // max 15 chars
    p_dst_peers->length_peer_chassis_id = length_peer_chassis_id;
    pf_get_mem(&get_info, &req_pos, length_peer_chassis_id, p_dst_peers->peer_chassis_id);
    p_dst_peers->peer_chassis_id[length_peer_chassis_id] = '\0';

    // save always

    if(pf_check_peers_is_valid(p_dst_peers))
    {
      os_save_im_data(net, false);
    }
    else if (p_ar != NULL)
    {
      pf_ppm_set_problem_indicator(p_ar, true);

      LOG_WARNING(PNET_LOG,
                  "Invalid check peers data:\n\tNumberOfPeers %u\n\tLengthPeerPortID %u\n\tPeerPortID %s\n\tLengthPeerChassisID %u\n\tPeerChassisID %s\n",
                  p_dst_peers->number_of_peers,
                  p_dst_peers->length_peer_port_id,
                  p_dst_peers->peer_port_id,
                  p_dst_peers->length_peer_chassis_id,
                  p_dst_peers->peer_chassis_id);

      os_save_im_data(net, true);
    }

    return 0;
  }

  return -1;
}

/**
 * @internal
 * Write one data record.
 *
 * This function performs a write of one data record.
 * The index in the IODWrite request selects the item to write to.
 *
 * @param net              InOut: The p-net stack instance
 * @param p_ar             In:   The AR instance.
 * @param p_write_request  In:   The IODWrite request.
 * @param p_req_buf        In:   The request buffer.
 * @param data_length      In:   Size of the data to write.
 * @param p_req_pos        InOut:Position within the request buffer.
 * @param p_result         Out:  Detailed error information.
 * @return  0  if operation succeeded.
 *          -1 if an error occurred.
 */
static int pf_cmwrr_write(
  pnet_t                  *net,
  pf_ar_t                 *p_ar,
  pf_iod_write_request_t  *p_write_request,
  uint8_t                 *p_req_buf,
  uint16_t                data_length,
  uint16_t                *p_req_pos,
  pnet_result_t           *p_result)
{
  int ret = -1;

  if (p_write_request->index < 0x7fff)
  {
    ret = pf_fspm_cm_write_ind(net, p_ar, p_write_request,
                               data_length, &p_req_buf[*p_req_pos], p_result);
  }
  else if ((PF_IDX_SUB_IM_0 <= p_write_request->index) && (p_write_request->index <= PF_IDX_SUB_IM_15))
  {
    ret = pf_fspm_cm_write_ind(net, p_ar, p_write_request,
                               data_length, &p_req_buf[*p_req_pos], p_result);
  }
  else
  {
    switch (p_write_request->index)
    {
    case PF_IDX_SUB_PDPORT_DATA_CHECK:
      // /* Save the information -> let the application do it */
      // /* Request:
      //  * IODWriteReqHeader    <= Already handled. Data in p_write_request.
      //  * PDPortDataCheck
      //  *    blockHeader
      //  *       blabla
      //  *    padding(2)
      //  *    slotNumber
      //  *    subslotNumber
      //  *    checkPeers
      //  *       blockHeader
      //  *          blabla
      //  *       numberOfPeers
      //  *       lengthPeerPortID
      //  *       peerPortID
      //  *       lengthPeerChassisID
      //  *       peerChassisId
      //  * Response:
      //  * IODWriteResHeader    <= Handled by caller (pf_cmwrr_rm_write_ind)
      //  */      
      ret = pf_write_pd_port_data_check(net,
                                        p_ar,
                                        data_length,
                                        &p_req_buf[*p_req_pos]);
      if (ret != 0)
      {
        p_result->pnio_status.error_code = PNET_ERROR_CODE_PNIO;
        p_result->pnio_status.error_decode = PNET_ERROR_DECODE_PNIORW;
        p_result->pnio_status.error_code_1 = PNET_ERROR_CODE_1_ACC_STATE_CONFLICT;
      }
      break;
    case PF_IDX_SUB_PDPORT_DATA_ADJ:
      /* ToDo: Handle the request. */
      break;
    default:
      break;
    }
  }

  (*p_req_pos) += data_length;

  return ret;
}

int pf_cmwrr_rm_write_ind(
  pnet_t                  *net,
  pf_ar_t                 *p_ar,
  pf_iod_write_request_t  *p_write_request,
  pf_iod_write_result_t   *p_write_result,
  pnet_result_t           *p_result,
  uint8_t                 *p_req_buf,    /* request buffer */
  uint16_t                data_length,
  uint16_t                *p_req_pos)    /* In/out: position in request buffer */
{
  int                     ret = -1;

  p_write_result->sequence_number = p_write_request->sequence_number;
  p_write_result->ar_uuid = p_write_request->ar_uuid;
  p_write_result->api = p_write_request->api;
  p_write_result->slot_number = p_write_request->slot_number;
  p_write_result->subslot_number = p_write_request->subslot_number;
  p_write_result->index = p_write_request->index;
  p_write_result->record_data_length = 0;

  switch (net->cmwrr_state)
  {
  case PF_CMWRR_STATE_IDLE:
    p_result->pnio_status.error_code = PNET_ERROR_CODE_PNIO;
    p_result->pnio_status.error_decode = PNET_ERROR_DECODE_PNIORW;
    p_result->pnio_status.error_code_1 = PNET_ERROR_CODE_1_ACC_STATE_CONFLICT;
    break;
  case PF_CMWRR_STATE_STARTUP:
    if (p_ar->ar_state == PF_AR_STATE_BACKUP)
    {
      p_result->pnio_status.error_code = PNET_ERROR_CODE_PNIO;
      p_result->pnio_status.error_decode = PNET_ERROR_DECODE_PNIORW;
      p_result->pnio_status.error_code_1 = PNET_ERROR_CODE_1_ACC_BACKUP;
    }
    else
    {
      ret = pf_cmwrr_write(net, p_ar, p_write_request, p_req_buf,
                           data_length, p_req_pos, p_result);

      if (ret != 0)
      {
        memcpy(&p_write_result->pnio_status, &p_result->pnio_status, sizeof(pnet_pnio_status_t));
      }
    }
  
    break;
  case PF_CMWRR_STATE_PRMEND:
    p_result->pnio_status.error_code = PNET_ERROR_CODE_PNIO;
    p_result->pnio_status.error_decode = PNET_ERROR_DECODE_PNIORW;
    p_result->pnio_status.error_code_1 = PNET_ERROR_CODE_1_ACC_STATE_CONFLICT;
    break;
  case PF_CMWRR_STATE_DATA:
    if (p_ar->ar_state == PF_AR_STATE_BACKUP)
    {
      p_result->pnio_status.error_code = PNET_ERROR_CODE_PNIO;
      p_result->pnio_status.error_decode = PNET_ERROR_DECODE_PNIORW;
      p_result->pnio_status.error_code_1 = PNET_ERROR_CODE_1_ACC_BACKUP;
    }
    else
    {
      ret = pf_cmwrr_write(net, p_ar, p_write_request, p_req_buf,
                           data_length, p_req_pos, p_result);
      if (ret != 0)
      {
        memcpy(&p_write_result->pnio_status, &p_result->pnio_status, sizeof(pnet_pnio_status_t));
      }
    }
    break;
  }

  p_write_result->add_data_1 = p_result->add_data_1;
  p_write_result->add_data_2 = p_result->add_data_2;

  ret = pf_cmsm_cm_write_ind(net, p_ar, p_write_request);

  return ret;
}
