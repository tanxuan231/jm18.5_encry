
/*!
 ************************************************************************
 * \file  nal.c
 *
 * \brief
 *    Converts Encapsulated Byte Sequence Packets (EBSP) to Raw Byte
 *    Sequence Packets (RBSP), and then onto String Of Data Bits (SODB)
 *
 * \author
 *    Main contributors (see contributors.h for copyright, address and affiliation details)
 *    - Shankar L. Regunathan <shanre@microsoft.com>
 *    - Tobias Oelbaum <oelbaum@drehvial.de>
************************************************************************
 */

#include "contributors.h"
#include "global.h"
#include "nalucommon.h"

 /*!
 ************************************************************************
 * \brief
 *    Converts RBSP to string of data bits
 * \param streamBuffer
 *          pointer to buffer containing data
 *  \param last_byte_pos
 *          position of the last byte containing data.
 * \return last_byte_pos
 *          position of the last byte pos. If the last-byte was entirely a stuffing byte,
 *          it is removed, and the last_byte_pos is updated.
 *
************************************************************************/
//传入RBSP数据和其长度
int RBSPtoSODB(byte *streamBuffer, int last_byte_pos)
{
  int ctr_bit, bitoffset;

  bitoffset = 0;
  //find trailing 1
  ctr_bit = (streamBuffer[last_byte_pos-1] & (0x01<<bitoffset));   // set up control bit

  while (ctr_bit==0)
  {                 // find trailing 1 bit
    ++bitoffset;
    if(bitoffset == 8)
    {
      if(last_byte_pos == 0)
        printf(" Panic: All zero data sequence in RBSP \n");
      assert(last_byte_pos != 0);
      --last_byte_pos;
      bitoffset = 0;
    }
    ctr_bit= streamBuffer[last_byte_pos - 1] & (0x01<<(bitoffset));
  }

  // We keep the stop bit for now
/*  if (remove_stop)
  {
    streamBuffer[last_byte_pos-1] -= (0x01<<(bitoffset));
    if(bitoffset == 7)
      return(last_byte_pos-1);
    else
      return(last_byte_pos);
  }
*/
  return(last_byte_pos);

}


/*!
************************************************************************
* \brief
*    Converts Encapsulated Byte Sequence Packets to RBSP
* \param streamBuffer
*    pointer to data stream
* \param end_bytepos
*    size of data stream
* \param begin_bytepos
*    Position after beginning
************************************************************************/
//防止伪起始码字节（0X03）	
//例如: 0x40 00 00 03 00 40 中03是防止伪起始码字节,应该在解码时去掉
//在加密的时候必须记住0x03的位置
int EBSPtoRBSP(NALU_t *nalu, int begin_bytepos)
{
  int i, j, count;
  count = 0;	//记录每个连续的0x00的个数
	byte *streamBuffer = nalu->buf;
	int end_bytepos = nalu->len;
	nalu->fake_start_code_len = 0;
	
  if(end_bytepos < begin_bytepos)
    return end_bytepos;

  j = begin_bytepos;

  //NAL层语法的rbsp_byte[i++]的处理
  for(i = begin_bytepos; i < end_bytepos; ++i)
  { //starting from begin_bytepos to avoid header information
    //in NAL unit, 0x000000, 0x000001 or 0x000002 shall not occur at any byte-aligned position
    if(count == ZEROBYTES_SHORTSTARTCODE && streamBuffer[i] < 0x03) 
      return -1;
    if(count == ZEROBYTES_SHORTSTARTCODE && streamBuffer[i] == 0x03)	//即出现:0x00 00 03时
    {
      //check the 4th byte after 0x000003, except when cabac_zero_word is used, 
      //in which case the last three bytes of this NAL unit must be 0x000003
      if((i < end_bytepos-1) && (streamBuffer[i+1] > 0x03))
        return -1;
      //if cabac_zero_word is used, the final byte of this NAL unit(0x03) is discarded, 
      //and the last two bytes of RBSP must be 0x0000
      if(i == end_bytepos-1)
        return j;

			nalu->fake_start_code_offset[nalu->fake_start_code_len++] = i;
      ++i;
      count = 0;
    }
    streamBuffer[j] = streamBuffer[i];
    if(streamBuffer[i] == 0x00)
      ++count;
    else
      count = 0;
    ++j;
  }
	
  return j;	//返回RBSP的字节数
}
