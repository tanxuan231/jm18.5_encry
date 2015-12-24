
/*!
 **************************************************************************************
 * \file
 *    nalucommon.h
 * \brief
 *    NALU handling common to encoder and decoder
 * \author
 *    Main contributors (see contributors.h for copyright, address and affiliation details)
 *      - Stephan Wenger        <stewe@cs.tu-berlin.de>
 *      - Karsten Suehring
 ***************************************************************************************
 */

#ifndef _NALUCOMMON_H_
#define _NALUCOMMON_H_

#include "defines.h"

#define MAXRBSPSIZE 64000
#define MAXNALUSIZE 64000

//! values for nal_unit_type
typedef enum {
 NALU_TYPE_SLICE    = 1,
 NALU_TYPE_DPA      = 2,
 NALU_TYPE_DPB      = 3,
 NALU_TYPE_DPC      = 4,
 NALU_TYPE_IDR      = 5,
 NALU_TYPE_SEI      = 6,
 NALU_TYPE_SPS      = 7,
 NALU_TYPE_PPS      = 8,
 NALU_TYPE_AUD      = 9,
 NALU_TYPE_EOSEQ    = 10,
 NALU_TYPE_EOSTREAM = 11,
 NALU_TYPE_FILL     = 12,
#if (MVC_EXTENSION_ENABLE)
 NALU_TYPE_PREFIX   = 14,
 NALU_TYPE_SUB_SPS  = 15,
 NALU_TYPE_SLC_EXT  = 20,
 NALU_TYPE_VDRD     = 24  // View and Dependency Representation Delimiter NAL Unit
#endif
} NaluType;

//! values for nal_ref_idc
typedef enum {
 NALU_PRIORITY_HIGHEST     = 3,
 NALU_PRIORITY_HIGH        = 2,
 NALU_PRIORITY_LOW         = 1,
 NALU_PRIORITY_DISPOSABLE  = 0
} NalRefIdc;

//! NAL unit structure
typedef struct nalu_t
{
  int       startcodeprefix_len;   //!< NALU前缀码长度(4 or 3B)"00 00 00 01" 或 "00 00 01" 4 for parameter sets and first slice in picture, 3 for everything else (suggested)
  unsigned  len;                   //!< NALU的长度/RBSP的长度 Length of the NAL unit (Excluding the start code, which does not belong to the NALU)
  unsigned  max_size;              //!< NAL Unit Buffer size
  int       forbidden_bit;         //!< should be always FALSE  占1位(NALU header)
  NaluType  nal_unit_type;         //!< NALU_TYPE_xxxx  NALU类型占5位(NALU header)
  NalRefIdc nal_reference_idc;     //!< nal_ref_idc NALU_PRIORITY_xxxx  NAL的重要性2位(NALU header)
  byte     *buf;                   //!< NALU/RBSP的数据 contains the first byte followed by the EBSP
  uint16    lost_packets;          //!< true, if packet loss is detected
#if (MVC_EXTENSION_ENABLE)
  int       svc_extension_flag;    //!< should be always 0, for MVC
  int       non_idr_flag;          //!< 0 = current is IDR
  int       priority_id;           //!< a lower value of priority_id specifies a higher priority
  int       view_id;               //!< view identifier for the NAL unit
  int       temporal_id;           //!< temporal identifier for the NAL unit
  int       anchor_pic_flag;       //!< anchor access unit
  int       inter_view_flag;       //!< inter-view prediction enable
  int       reserved_one_bit;      //!< shall be equal to 1
#endif

	int cur_nalu_start;		//当前NALU在H264文件中的位置(不包括前缀码)
  int next_nalu_start_prefix;	//下一个NALU在H264文件中的位置(包括前缀码)
  int fake_start_code_offset[MAXNALUSIZE];  //存放着当前NAL的伪起始码从RBSP(不包括NALU head)起始位置的偏移
  //ex:  {0x67, 0x64, 0x0, 0xb, 0xac, 0xd9, 0x42, 0xc4, 0xe8, 0x40, 0x0, 0x0, 0x3, 0x0, 0x40, 0x0, 0x0, 0xc}
  //中NALU head是0x67,0x3所在的偏移为12
  int fake_start_code_len;
	int fake_start_code_curpos;	//fake_start_code_offset当前需要判断的位置
} NALU_t;

//! allocate one NAL Unit
extern NALU_t *AllocNALU(int);

//! free one NAL Unit
extern void FreeNALU(NALU_t *n);

#if (MVC_EXTENSION_ENABLE)
extern void nal_unit_header_svc_extension();
extern void prefix_nal_unit_svc();
#endif

#endif
