
/*!
 ***************************************************************************
 * \file vlc.c
 *
 * \brief
 *    (CA)VLC coding functions
 *
 * \author
 *    Main contributors (see contributors.h for copyright, address and affiliation details)
 *    - Inge Lille-Langoy               <inge.lille-langoy@telenor.com>
 *    - Detlev Marpe
 *    - Stephan Wenger                  <stewe@cs.tu-berlin.de>
 ***************************************************************************
 */

#include "contributors.h"

#include <math.h>

#include "global.h"
#include "enc_statistics.h"
#include "vlc.h"

#if TRACE
#define SYMTRACESTRING(s) strncpy(sym.tracestring,s,TRACESTRING_SIZE)
#else
#define SYMTRACESTRING(s) // do nothing
#endif

//! gives codeword number from CBP value, both for intra and inter
//Ö¡ÄÚ/Ö¡¼äÔ¤²âºê¿éµÄÓ³Éä¹ØÏµ±í
//µ±²»Ê¹ÓÃ16x16µÄÖ¡ÄÚÔ¤²âÊ±»áÊ¹ÓÃcbpÓï·¨ÔªËØ
//CodeBlockPatternLuma(±äÁ¿) = coded_block_pattern(Óï·¨ÔªËØ) % 16 cbpµÄºó4Î»:¹²16ÖÖ
//CodeBlockPatternChroma = coded_block_pattern /16  cbpµÄÇ°4Î»:¹²3ÖÖ
//Luma and Chroma×Ü¹²48ÖÐ×éºÏ
//Luma:     0) ËùÓÐ²Ð²î£¨°üÀ¨ DC¡¢AC£©¶¼²»±àÂë¡£  
//              1) Ö»¶Ô DC ÏµÊý±àÂë¡£  
//              2) ËùÓÐ²Ð²î£¨°üÀ¨ DC¡¢AC£©¶¼±àÂë¡£
//cbp = NCBP[other/normal][cbp][intra/inter]
static const byte NCBP[2][48][2]=
{
  {  // 0      1        2       3       4       5       6       7       8       9      10      11
    { 1, 0},{10, 1},{11, 2},{ 6, 5},{12, 3},{ 7, 6},{14,14},{ 2,10},{13, 4},{15,15},{ 8, 7},{ 3,11},
    { 9, 8},{ 4,12},{ 5,13},{ 0, 9},{ 0, 0},{ 0, 0},{ 0, 0},{ 0, 0},{ 0, 0},{ 0, 0},{ 0, 0},{ 0, 0},
    { 0, 0},{ 0, 0},{ 0, 0},{ 0, 0},{ 0, 0},{ 0, 0},{ 0, 0},{ 0, 0},{ 0, 0},{ 0, 0},{ 0, 0},{ 0, 0},
    { 0, 0},{ 0, 0},{ 0, 0},{ 0, 0},{ 0, 0},{ 0, 0},{ 0, 0},{ 0, 0},{ 0, 0},{ 0, 0},{ 0, 0},{ 0, 0}
  },
  {
    { 3, 0},{29, 2},{30, 3},{17, 7},{31, 4},{18, 8},{37,17},{ 8,13},{32, 5},{38,18},{19, 9},{ 9,14},
    {20,10},{10,15},{11,16},{ 2,11},{16, 1},{33,32},{34,33},{21,36},{35,34},{22,37},{39,44},{ 4,40},
    {36,35},{40,45},{23,38},{ 5,41},{24,39},{ 6,42},{ 7,43},{ 1,19},{41, 6},{42,24},{43,25},{25,20},
    {44,26},{26,21},{46,46},{12,28},{45,27},{47,47},{27,22},{13,29},{28,23},{14,30},{15,31},{ 0,12}
  }
};


/*!
 *************************************************************************************
 * \brief
 *    write_ue_v, writes an ue(v) syntax element, returns the length in bits
 *
 * \param tracestring
 *    the string for the trace file
 * \param value
 *    the value to be coded ´ý±àÂëµÄÊýÖµ
 *  \param bitstream
 *    the target bitstream the value should be coded into ÓÃÓÚ´æ´¢±àÂëµÄ±ÈÌØÁ÷
 * \return  ·µ»Ø±àÂë½á¹ûµÄ±ÈÌØÎ»Êý
 *    Number of bits used by the coded syntax element
 * \ note
 *    This function writes always the bit buffer for the progressive scan flag, and
 *    should not be used (or should be modified appropriately) for the interlace crap
 *    When used in the context of the Parameter Sets, this is obviously not a
 *    problem.
 *
 *************************************************************************************
 */
 //ÒÔÎÞ·ûºÅÖ¸Êý¸çÂ×²¼±àÂë·½Ê½±àÂëÓï·¨ÔªËØ
int write_ue_v (char *tracestring, int value, Bitstream *bitstream)
{
  SyntaxElement symbol, *sym=&symbol;
  sym->value1 = value;
  sym->value2 = 0;

  //assert (bitstream->streamBuffer != NULL);

  ue_linfo(sym->value1,sym->value2,&(sym->len),&(sym->inf));
  symbol2uvlc(sym);

  writeUVLC2buffer (sym, bitstream);

#if TRACE
  strncpy(sym->tracestring,tracestring,TRACESTRING_SIZE);
  trace2out (sym);
#endif

  return (sym->len);
}


/*!
 *************************************************************************************
 * \brief
 *    write_se_v, writes an se(v) syntax element, returns the length in bits
 *
 * \param tracestring
 *    the string for the trace file
 * \param value
 *    the value to be coded
 *  \param bitstream
 *    the target bitstream the value should be coded into
 *
 * \return
 *    Number of bits used by the coded syntax element
 *
 * \ note
 *    This function writes always the bit buffer for the progressive scan flag, and
 *    should not be used (or should be modified appropriately) for the interlace crap
 *    When used in the context of the Parameter Sets, this is obviously not a
 *    problem.
 *
 *************************************************************************************
 */ 
//ÒÔÓÐ·ûºÅÖ¸Êý¸çÂ×²¼±àÂë·½Ê½±àÂëÓï·¨ÔªËØ
int write_se_v (char *tracestring, int value, Bitstream *bitstream)
{
  SyntaxElement symbol, *sym=&symbol;
  sym->value1 = value;
  sym->value2 = 0;

  //assert (bitstream->streamBuffer != NULL);

  se_linfo(sym->value1,sym->value2,&(sym->len),&(sym->inf));
  symbol2uvlc(sym);

  writeUVLC2buffer (sym, bitstream);

#if TRACE
  strncpy(sym->tracestring,tracestring,TRACESTRING_SIZE);
  trace2out (sym);
#endif

  return (sym->len);
}


/*!
 *************************************************************************************
 * \brief
 *    write_u_1, writes a flag (u(1) syntax element, returns the length in bits,
 *    always 1
 *
 * \param tracestring
 *    the string for the trace file
 * \param value
 *    the value to be coded
 *  \param bitstream
 *    the target bitstream the value should be coded into
 *
 * \return
 *    Number of bits used by the coded syntax element (always 1)
 *
 * \ note
 *    This function writes always the bit buffer for the progressive scan flag, and
 *    should not be used (or should be modified appropriately) for the interlace crap
 *    When used in the context of the Parameter Sets, this is obviously not a
 *    problem.
 *
 *************************************************************************************
 */
Boolean write_u_1 (char *tracestring, int value, Bitstream *bitstream)
{
  SyntaxElement symbol, *sym=&symbol;

  sym->bitpattern = value;
  sym->value1 = value;
  sym->len = 1;

  //assert (bitstream->streamBuffer != NULL);

  writeUVLC2buffer(sym, bitstream);

#if TRACE
  strncpy(sym->tracestring,tracestring,TRACESTRING_SIZE);
  trace2out (sym);
#endif

  return ((Boolean) sym->len);
}


/*!
 *************************************************************************************
 * \brief
 *    write_u_v, writes a n bit fixed length syntax element, returns the length in bits,
 *  ÒÔÔ­Ê¼±ÈÌØÐÎÊ½±àÂëN¸öÎ»
 * \param n
 *    length in bits
 * \param tracestring
 *    the string for the trace file
 * \param value
 *    the value to be coded
 *  \param bitstream
 *    the target bitstream the value should be coded into
 *
 * \return
 *    Number of bits used by the coded syntax element
 *
 * \ note
 *    This function writes always the bit buffer for the progressive scan flag, and
 *    should not be used (or should be modified appropriately) for the interlace crap
 *    When used in the context of the Parameter Sets, this is obviously not a
 *    problem.
 *
 *************************************************************************************
 */

int write_u_v (int n, char *tracestring, int value, Bitstream *bitstream)
{
  SyntaxElement symbol, *sym=&symbol;

  sym->bitpattern = value;
  sym->value1 = value;
  sym->len = n;  

  //assert (bitstream->streamBuffer != NULL);

  writeUVLC2buffer(sym, bitstream);

#if TRACE
  strncpy(sym->tracestring,tracestring,TRACESTRING_SIZE);
  trace2out (sym);
#endif

  return (sym->len);
}


/*!
 ************************************************************************
 * \brief
 *    mapping for ue(v) syntax elements
 * \param ue
 *    value to be mapped ´ý±àÂëµÄËãÊõÖµ
 * \param dummy
 *    dummy parameter Ðé¼Ù²ÎÊý(Î´Ê¹ÓÃ)
 * \param info
 *    returns mapped value ÓÃÓÚ´æ´¢Ö¸Êý¸çÂ×²¼±àÂëµÄinfo²¿·Ö
 * \param len
 *    returns mapped value length ±àÂëµÄ±ÈÌØ³¤¶È
 ************************************************************************
 */
 //½«Óï·¨ÔªËØµÄËãÊõÖµ°´ÕÕÎÞ·ûºÅÖ¸Êý¸çÂ×²¼±àÂëµÄ·½Ê½
 //Ó³Éä³É±ÈÌØµÄÐÎÊ½
void ue_linfo(int ue, int dummy, int *len,int *info)
{
  int i, nn =(ue + 1) >> 1;

  for (i=0; i < 33 && nn != 0; i++)
  {
    nn >>= 1;
  }
  *len  = (i << 1) + 1;
  *info = ue + 1 - (1 << i);
}


/*!
 ************************************************************************
 * \brief
 *    mapping for se(v) syntax elements
 * \param se
 *    value to be mapped ´ý±àÂëµÄËãÊõÖ
 * \param dummy
 *    dummy parameter
 * \param len ±àÂëµÄ±ÈÌØ³¤¶È
 *    returns mapped value length
 * \param info
 *    returns mapped value ÓÃÓÚ´æ´¢Ö¸Êý¸çÂ×²¼±àÂëµÄinfo²¿·Ö
 ************************************************************************
 */
 //½«Óï·¨ÔªËØµÄËãÊõÖµ°´ÕÕÓÐ·ûºÅÖ¸Êý¸çÂ×²¼±àÂëµÄ·½Ê½
 //Ó³Éä³É±ÈÌØµÄÐÎÊ½
void se_linfo(int se, int dummy, int *len,int *info)
{  
  int sign = (se <= 0) ? 1 : 0;
  int n = iabs(se) << 1;   //  n+1 is the number in the code table.  Based on this we find length and info
  int nn = (n >> 1);
  int i;
  for (i = 0; i < 33 && nn != 0; i++)
  {
    nn >>= 1;
  }
  *len  = (i << 1) + 1;
  *info = n - (1 << i) + sign;
}


/*!
 ************************************************************************
 * \par Input:
 *    Number in the code table
 * \par Output:
 *    length and info
 ************************************************************************
 */
void cbp_linfo_intra_other(int cbp, int dummy, int *len,int *info)
{
  ue_linfo(NCBP[0][cbp][0], dummy, len, info);
}


/*!
 ************************************************************************
 * \par Input:   cbp : ºê¿é±àÂëÄ£°ålen: ±àÂëµÄ±ÈÌØ³¤¶È
 *    Number in the code table
 * \par Output:
 *    length and info
 ************************************************************************
 */
//ÔÚÖ¡ÄÚÔ¤²âµÄÇé¿öÏÂ,½«Óï·¨ÔªËØcbpµÄËãÊõÖµ°´ÕÕÓ³ÉäÖ¸Êý¸çÂ×²¼±àÂë
//µÄ·½Ê½Ó³Éä³É±ÈÌØÐÎÊ½
void cbp_linfo_intra_normal(int cbp, int dummy, int *len,int *info)
{
  ue_linfo(NCBP[1][cbp][0], dummy, len, info);
}

/*!
 ************************************************************************
 * \par Input:
 *    Number in the code table
 * \par Output:
 *    length and info
 ************************************************************************
 */
void cbp_linfo_inter_other(int cbp, int dummy, int *len,int *info)
{
  ue_linfo(NCBP[0][cbp][1], dummy, len, info);
}

/*!
 ************************************************************************
 * \par Input:
 *    Number in the code table
 * \par Output:
 *    length and info
 ************************************************************************
 */
//ÔÚÖ¡¼äÔ¤²âµÄÇé¿öÏÂ,½«Óï·¨ÔªËØcbpµÄËãÊõÖµ°´ÕÕÓ³ÉäÖ¸Êý¸çÂ×²¼±àÂë
//µÄ·½Ê½Ó³Éä³É±ÈÌØÐÎÊ½
void cbp_linfo_inter_normal(int cbp, int dummy, int *len,int *info)
{
  ue_linfo(NCBP[1][cbp][1], dummy, len, info);
}

/*!
 ************************************************************************
 * \brief
 *    2x2 transform of chroma DC
 * \par Input:
 *    level and run for coefficients ´ý±àÂëµÄrun/levelÖµ
 * \par Output:
 *    length and info len:±àÂëµÄ±ÈÌØ³¤¶Èinfo:´æ´¢±àÂëµÄ±ÈÌØ½á¹û
 * \note
 *    see ITU document for bit assignment
 ************************************************************************
 */
//¶ÔÉ«²îDC·ÖÁ¿µÄÒ»¸ö(run/level)¶Ô½øÐÐ±ä³¤±àÂë
void levrun_linfo_c2x2(int level,int run,int *len,int *info)
{
  static const byte NTAB[2][2]=
  {
    {1,5},
    {3,0}
  };
  static const byte LEVRUN[4]=
  {
    2,1,0,0
  };

  if (level == 0) //  check if the coefficient sign EOB (level=0)
  {
    *len=1;
    return;
  }
  else
  {
    int levabs = iabs(level);
    int sign = level <= 0 ? 1 : 0;
    int n = (levabs <= LEVRUN[run]) ? NTAB[levabs - 1][run] + 1 : (levabs - LEVRUN[run]) * 8 + run * 2;
    int nn = n >> 1;
    int i;

    for (i=0; i < 16 && nn != 0; i++)
    {
      nn >>= 1;
    }
    *len  = (i << 1) + 1;
    *info = n - (1 << i) + sign;
  }
}


/*!
 ************************************************************************
 * \brief
 *    Single scan coefficients
 * \par Input:
 *    level and run for coefficients
 * \par Output:
 *    length and info
 * \note
 *    see ITU document for bit assignment
 ************************************************************************
 */
void levrun_linfo_inter(int level,int run,int *len,int *info)
{
  static const byte LEVRUN[16]=
  {
    4,2,2,1,1,1,1,1,1,1,0,0,0,0,0,0
  };
  static const byte NTAB[4][10]=
  {
    { 1, 3, 5, 9,11,13,21,23,25,27},
    { 7,17,19, 0, 0, 0, 0, 0, 0, 0},
    {15, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {29, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  };

  if (level == 0)           //  check for EOB
  {
    *len=1;
    return;
  }
  else
  {

    int sign   = (level <= 0) ? 1 : 0;
    int levabs = iabs(level);

    int n = (levabs <= LEVRUN[run]) ? NTAB[levabs - 1][run] + 1 : (levabs - LEVRUN[run]) * 32 + run * 2;

    int nn = n >> 1;
    int i;

    for (i=0; i < 16 && nn != 0; i++)
    {
      nn >>= 1;
    }
    *len  = (i << 1) + 1;
    *info = n - (1 << i) + sign;
  }
}


/*!
 ************************************************************************
 * \brief
 *    Makes code word and passes it back
 *    A code word has the following format: 0 0 0 ... 1 Xn ...X2 X1 X0.
 *
 * \par Input:
 *    Info   : Xn..X2 X1 X0                                             \n
 *    Length : Total number of bits in the codeword
 ************************************************************************
 */
 // NOTE this function is called with sym->inf > (1<<(sym->len >> 1)).  The upper bits of inf are junk
//sym :Ö¸Ïò´ý±àÂëµÄÓï·¨ÔªËØ
//¼ÆËãÓï·¨ÔªËØUVLC±àÂëµÄ±ÈÌØÐÎÊ½,²¢±£´æÔÚ³ÉÔ±±äÁ¿bitmapÖÐ
int symbol2uvlc(SyntaxElement *sym)
{
  int suffix_len = sym->len >> 1;
  //assert (suffix_len < 32);
  suffix_len = (1 << suffix_len);
  sym->bitpattern = suffix_len | (sym->inf & (suffix_len - 1));
  return 0;
}

/*!
************************************************************************
* \brief
*    generates UVLC code and passes the codeword to the buffer
************************************************************************
*/
void writeSE_UVLC(SyntaxElement *se, DataPartition *dp)
{
  ue_linfo (se->value1,se->value2,&(se->len),&(se->inf));
  symbol2uvlc(se);

  writeUVLC2buffer(se, dp->bitstream);

  if(se->type != SE_HEADER)
    dp->bitstream->write_flag = 1;

#if TRACE
  if(dp->bitstream->trace_enabled)
    trace2out (se);
#endif
}

/*!
************************************************************************
* \brief
*    generates UVLC code and passes the codeword to the buffer
************************************************************************
*/
void writeSE_SVLC(SyntaxElement *se, DataPartition *dp)
{
  se_linfo (se->value1,se->value2,&(se->len),&(se->inf));
  symbol2uvlc(se);

  writeUVLC2buffer(se, dp->bitstream);

  if(se->type != SE_HEADER)
    dp->bitstream->write_flag = 1;

#if TRACE
  if(dp->bitstream->trace_enabled)
    trace2out (se);
#endif
}

/*!
************************************************************************
* \brief
*    generates UVLC code and passes the codeword to the buffer
************************************************************************
*/
void writeCBP_VLC (Macroblock* currMB, SyntaxElement *se, DataPartition *dp)
{
  if (currMB->mb_type == I4MB || currMB->mb_type == SI4MB ||  currMB->mb_type == I8MB)
  {
    currMB->cbp_linfo_intra (se->value1, se->value2, &(se->len), &(se->inf));
  }
  else
  {
    currMB->cbp_linfo_inter (se->value1,se->value2,&(se->len),&(se->inf));
  }
  symbol2uvlc(se);

  writeUVLC2buffer(se, dp->bitstream);

  if(se->type != SE_HEADER)
    dp->bitstream->write_flag = 1;

#if TRACE
  if(dp->bitstream->trace_enabled)
    trace2out (se);
#endif
}


/*!
 ************************************************************************
 * \brief
 *    generates code and passes the codeword to the buffer
 ************************************************************************
 */
void writeIntraPredMode_CAVLC(SyntaxElement *se, DataPartition *dp)
{

  if (se->value1 == -1)
  {
    se->len = 1;
    se->inf = 1;
  }
  else
  {
    se->len = 4;
    se->inf = se->value1;
  }

  se->bitpattern = se->inf;
  writeUVLC2buffer(se, dp->bitstream);

  if(se->type != SE_HEADER)
    dp->bitstream->write_flag = 1;

#if TRACE
  if(dp->bitstream->trace_enabled)
    trace2out (se);
#endif

  return;
}


/*!
 ************************************************************************
 * \brief
 *    generates UVLC code and passes the codeword to the buffer
 * \author
 *  Tian Dong
 ************************************************************************
 */
//¼ÆËãÓï·¨ÔªËØUVLC±àÂëµÄ±ÈÌØÐÎÊ½£¬²¢½«ÆäÐ´Èëbuffer
//·µ»ØÐ´ÈëµÄ±ÈÌØÊý
int writeSyntaxElement2Buf_UVLC(SyntaxElement *se, Bitstream* this_streamBuffer )
{
  //¼ÆËãlen/inf
  se->mapping(se->value1,se->value2,&(se->len),&(se->inf));
  //µÃµ½±àÂëµÄ±ÈÌØÐÎÊ½
  symbol2uvlc(se);
  //½«Æä±ÈÌØÐÎÊ½Ð´Èëthis_streamBuffer 
  writeUVLC2buffer(se, this_streamBuffer );

#if TRACE
  if(se->type <= 1)
    trace2out (se);
#endif

  return (se->len);
}


/*!
 ************************************************************************
 * \brief
 *    writes UVLC code to the appropriate buffer
 ************************************************************************
 */
//se: ´ý±àÂëµÄÓï·¨ÔªËØcurrStream: ´æ´¢±àÂëµÄ±ÈÌØÁ÷
//½«UVLC±àÂëµÄ±ÈÌØÐÎÊ½Ð´ÈëcurrStream
void  writeUVLC2buffer(SyntaxElement *se, Bitstream *currStream)
{
  unsigned int mask = 1 << (se->len - 1);
  byte *byte_buf  = &currStream->byte_buf;  //µ±Ç°»º´æÇø
  int *bits_to_go = &currStream->bits_to_go;  //µ±Ç°Î»ÖÃ
  int i;

  // Add the new bits to the bitstream.
  // Write out a byte if it is full  Èç¹ûÂú1×Ö½Ú(8bit)
  if ( se->len < 33 )
  {
    for (i = 0; i < se->len; i++)
    {
      *byte_buf <<= 1;

      if (se->bitpattern & mask)
        *byte_buf |= 1;

      mask >>= 1;

      if ((--(*bits_to_go)) == 0)
      {
        *bits_to_go = 8;      
        currStream->streamBuffer[currStream->byte_pos++] = *byte_buf;
        *byte_buf = 0;      
      }
    }
  }
  else
  {
    // zeros
    for (i = 0; i < (se->len - 32); i++)
    {
      *byte_buf <<= 1;

      if ((--(*bits_to_go)) == 0)
      {
        *bits_to_go = 8;      
        currStream->streamBuffer[currStream->byte_pos++] = *byte_buf;
        *byte_buf = 0;      
      }
    }
    // actual info
    mask = (unsigned int) 1 << 31;
    for (i = 0; i < 32; i++)
    {
      *byte_buf <<= 1;

      if (se->bitpattern & mask)
        *byte_buf |= 1;

      mask >>= 1;

      if ((--(*bits_to_go)) == 0)
      {
        *bits_to_go = 8;      
        currStream->streamBuffer[currStream->byte_pos++] = *byte_buf;
        *byte_buf = 0;      
      }
    }
  }
}


/*!
 ************************************************************************
 * \brief
 *    generates UVLC code and passes the codeword to the buffer
 * \author
 *  Tian Dong
 ************************************************************************
 */
//½«¶¨³¤±ÈÌØÐÎÊ½µÄÓï·¨ÔªËØÐ´ÈëÂëÁ÷
//·µ»Ø±àÂëµÄ±ÈÌØÊý
int writeSyntaxElement2Buf_Fixed(SyntaxElement *se, Bitstream* this_streamBuffer )
{
  writeUVLC2buffer(se, this_streamBuffer );

#if TRACE
  if(se->type <= 1)
    trace2out (se);
#endif
  return (se->len);
}

/*!
************************************************************************
* \brief
*    generates UVLC code and passes the codeword to the buffer
* \author
*  Tian Dong
************************************************************************
*/
void writeSE_Flag(SyntaxElement *se, DataPartition *dp )
{
  se->len        = 1;
  se->bitpattern = (se->value1 & 1);

  writeUVLC2buffer(se, dp->bitstream );

#if TRACE
  if(dp->bitstream->trace_enabled)
    trace2out (se);
#endif
}

/*!
************************************************************************
* \brief
*    generates UVLC code and passes the codeword to the buffer
* \author
*  Tian Dong
************************************************************************
*/
void writeSE_invFlag(SyntaxElement *se, DataPartition *dp )
{
  se->len        = 1;
  se->bitpattern = 1 - (se->value1 & 1);

  writeUVLC2buffer(se, dp->bitstream );

#if TRACE
  if(dp->bitstream->trace_enabled)
    trace2out (se);
#endif
}

/*!
************************************************************************
* \brief
*    generates UVLC code and passes the codeword to the buffer
* \author
*  Tian Dong
************************************************************************
*/
void writeSE_Dummy(SyntaxElement *se, DataPartition *dp )
{
  se->len = 0;
}


/*!
************************************************************************
* \brief
*    generates UVLC code and passes the codeword to the buffer
* \author
*  Tian Dong
************************************************************************
*/
void writeSE_Fix(SyntaxElement *se, Bitstream *bitstream )
{
  writeUVLC2buffer(se, bitstream);

#if TRACE
  if(bitstream->trace_enabled)
    trace2out (se);
#endif
}


/*!
 ************************************************************************
 * \brief
 *    Makes code word and passes it back
 *
 * \par Input:
 *    Info   : Xn..X2 X1 X0                                             \n
 *    Length : Total number of bits in the codeword
 ************************************************************************
 */
//sym: ´ý±àÂëÓï·¨ÔªËØ
//¼ÆËãÓï·¨ÔªËØVLC±àÂëµÄ±ÈÌØÐÎÊ½
int symbol2vlc(SyntaxElement *sym)
{
  int info_len = sym->len;

  // Convert info into a bitpattern int
  sym->bitpattern = 0;

  // vlc coding
  while(--info_len >= 0)
  {
    sym->bitpattern <<= 1;
    sym->bitpattern |= (0x01 & (sym->inf >> info_len));
  }
  return 0;
}


/*!
 ************************************************************************
 * \brief
 *    generates VLC code and passes the codeword to the buffer
 ************************************************************************
 */
//se: ´ý±àÂëµÄÓï·¨ÔªËØdp:´æ´¢±àÂëµÄ±ÈÌØÁ÷
//¼ÆËãÓï·¨ÔªËØVLC±àÂëµÄ±ÈÌØÐÎÊ½,²¢½«ÆäÐ´ÈëÂëÁ÷vlc²»°üÀ¨Ç°×º²¿·Ö(suffix)
//±àÂëÃ¿¸öTrailingOnesµÄ·ûºÅ
int writeSyntaxElement_VLC(SyntaxElement *se, DataPartition *dp)
{
  se->inf = se->value1;
  se->len = se->value2;
  symbol2vlc(se);

  writeUVLC2buffer(se, dp->bitstream);

  if(se->type != SE_HEADER)
    dp->bitstream->write_flag = 1;

#if TRACE
  if(dp->bitstream->trace_enabled)
    trace2out (se);
#endif

  return (se->len);
}


/*!
 ************************************************************************
 * \brief
 *    write VLC for NumCoeff and TrailingOnes
 ************************************************************************
 */
//Totalcoeff±íÊ¾·ÇÁãÏµÊýµÄ×ÜÊýTrailingOnes±íÊ¾·ÇÁãÏµÊýÖÐ¡À1µÄ¸öÊý
//CAVLCÖÐ,±àÂë·ÇÁã²Ð²îÏµÊý¸öÊýºÍÍÏÎ²ÏµÊý¸öÊý
int writeSyntaxElement_NumCoeffTrailingOnes(SyntaxElement *se, DataPartition *dp)
{
  //±ê×¼ÖÐ±í9-5
  //Ã¿¸ö±àÂëµÄ±ÈÌØ³¤¶ÈTrailingOnes 0~3 TotalCoeff 0~16
  //lentab[NC(Number Currentµ±Ç°¿éÖµ)][TrailingOnes][TotalCoeff]
  static const byte lentab[3][4][17] =
  {
    {   // 0702
      { 1, 6, 8, 9,10,11,13,13,13,14,14,15,15,16,16,16,16},
      { 0, 2, 6, 8, 9,10,11,13,13,14,14,15,15,15,16,16,16},
      { 0, 0, 3, 7, 8, 9,10,11,13,13,14,14,15,15,16,16,16},
      { 0, 0, 0, 5, 6, 7, 8, 9,10,11,13,14,14,15,15,16,16},
    },
    {
      { 2, 6, 6, 7, 8, 8, 9,11,11,12,12,12,13,13,13,14,14},
      { 0, 2, 5, 6, 6, 7, 8, 9,11,11,12,12,13,13,14,14,14},
      { 0, 0, 3, 6, 6, 7, 8, 9,11,11,12,12,13,13,13,14,14},
      { 0, 0, 0, 4, 4, 5, 6, 6, 7, 9,11,11,12,13,13,13,14},
    },
    {
      { 4, 6, 6, 6, 7, 7, 7, 7, 8, 8, 9, 9, 9,10,10,10,10},
      { 0, 4, 5, 5, 5, 5, 6, 6, 7, 8, 8, 9, 9, 9,10,10,10},
      { 0, 0, 4, 5, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9,10,10,10},
      { 0, 0, 0, 4, 4, 4, 4, 4, 5, 6, 7, 8, 8, 9,10,10,10},
    },

  };

  //Ã¿¸ö±àÂë½á¹ûµÄ¶þ½øÖÆ±íÊ¾
  //codtab[NC(NumCoeff)][TrailingOnes][TotalCoeff]
  static const byte codtab[3][4][17] =
  {
    {
      { 1, 5, 7, 7, 7, 7,15,11, 8,15,11,15,11,15,11, 7,4},
      { 0, 1, 4, 6, 6, 6, 6,14,10,14,10,14,10, 1,14,10,6},
      { 0, 0, 1, 5, 5, 5, 5, 5,13, 9,13, 9,13, 9,13, 9,5},
      { 0, 0, 0, 3, 3, 4, 4, 4, 4, 4,12,12, 8,12, 8,12,8},
    },
    {
      { 3,11, 7, 7, 7, 4, 7,15,11,15,11, 8,15,11, 7, 9,7},
      { 0, 2, 7,10, 6, 6, 6, 6,14,10,14,10,14,10,11, 8,6},
      { 0, 0, 3, 9, 5, 5, 5, 5,13, 9,13, 9,13, 9, 6,10,5},
      { 0, 0, 0, 5, 4, 6, 8, 4, 4, 4,12, 8,12,12, 8, 1,4},
    },
    {
      {15,15,11, 8,15,11, 9, 8,15,11,15,11, 8,13, 9, 5,1},
      { 0,14,15,12,10, 8,14,10,14,14,10,14,10, 7,12, 8,4},
      { 0, 0,13,14,11, 9,13, 9,13,10,13, 9,13, 9,11, 7,3},
      { 0, 0, 0,12,11,10, 9, 8,13,12,12,12, 8,12,10, 6,2},
    },
  };
  int vlcnum = se->len;  //NC

  // se->value1 : Totalcoeff  numcoeff ·ÇÁã²Ð²îÏµÊý¸öÊý
  // se->value2 : TrailingOnes numtrailingones ÍÏÎ²ÏµÊý¸öÊý
  //ÏÈÍ¨¹ý²é±ílentab[TrailingOnes][NC][TotalCoeff]µÃµ½±àÂë³¤¶È
  //codtab[TrailingOnes][NC][TotalCoeff]µÃµ½¶þ½øÖÆ±íÊ¾
  if (vlcnum == 3)
  {
    se->len = 6;  // 4 + 2 bit FLC
    if (se->value1 > 0)
    {
      se->inf = ((se->value1-1) << 2) | se->value2;
    }
    else
    {
      se->inf = 3;
    }
  }
  else
  {
    //²é±í
    se->len = lentab[vlcnum][se->value2][se->value1];
    se->inf = codtab[vlcnum][se->value2][se->value1];
  }

  if (se->len == 0)
  {
    printf("ERROR: (numcoeff,trailingones) not valid: vlc=%d (%d, %d)\n",
      vlcnum, se->value1, se->value2);
    exit(-1);
  }

  //¼ÆËã³öÓï·¨ÔªËØµÄbitmap
  symbol2vlc(se);
  //½«½á¹ûÐ´Èë±àÂë»º´æÇø
  writeUVLC2buffer(se, dp->bitstream);

  if(se->type != SE_HEADER)
    dp->bitstream->write_flag = 1;

#if TRACE
  if(dp->bitstream->trace_enabled)
    trace2out (se);
#endif

  return (se->len);
}


/*!
 ************************************************************************
 * \brief
 *    write VLC for NumCoeff and TrailingOnes for Chroma DC
 ************************************************************************
 */
//CAVLCÖÐ,±àÂëÉ«¶ÈDC·ÖÁ¿·ÇÁã²Ð²îÏµÊý¸öÊýºÍÍÏÎ²ÏµÊý¸öÊý
int writeSyntaxElement_NumCoeffTrailingOnesChromaDC(VideoParameters *p_Vid, SyntaxElement *se, DataPartition *dp)
{
  //lentab[TrailingOnes][NC(NumCoeff)][TotalCoeff]    
  static const byte lentab[3][4][17] =
  {
    //YUV420
   {{ 2, 6, 6, 6, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    { 0, 1, 6, 7, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    { 0, 0, 3, 7, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    { 0, 0, 0, 6, 7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    //YUV422
   {{ 1, 7, 7, 9, 9,10,11,12,13, 0, 0, 0, 0, 0, 0, 0, 0},
    { 0, 2, 7, 7, 9,10,11,12,12, 0, 0, 0, 0, 0, 0, 0, 0},
    { 0, 0, 3, 7, 7, 9,10,11,12, 0, 0, 0, 0, 0, 0, 0, 0},
    { 0, 0, 0, 5, 6, 7, 7,10,11, 0, 0, 0, 0, 0, 0, 0, 0}},
    //YUV444
   {{ 1, 6, 8, 9,10,11,13,13,13,14,14,15,15,16,16,16,16},
    { 0, 2, 6, 8, 9,10,11,13,13,14,14,15,15,15,16,16,16},
    { 0, 0, 3, 7, 8, 9,10,11,13,13,14,14,15,15,16,16,16},
    { 0, 0, 0, 5, 6, 7, 8, 9,10,11,13,14,14,15,15,16,16}}
  };

  static const byte codtab[3][4][17] =
  {
    //YUV420
   {{ 1, 7, 4, 3, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    { 0, 1, 6, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    { 0, 0, 1, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    { 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    //YUV422
   {{ 1,15,14, 7, 6, 7, 7, 7, 7, 0, 0, 0, 0, 0, 0, 0, 0},
    { 0, 1,13,12, 5, 6, 6, 6, 5, 0, 0, 0, 0, 0, 0, 0, 0},
    { 0, 0, 1,11,10, 4, 5, 5, 4, 0, 0, 0, 0, 0, 0, 0, 0},
    { 0, 0, 0, 1, 1, 9, 8, 4, 4, 0, 0, 0, 0, 0, 0, 0, 0}},
    //YUV444
   {{ 1, 5, 7, 7, 7, 7,15,11, 8,15,11,15,11,15,11, 7, 4},
    { 0, 1, 4, 6, 6, 6, 6,14,10,14,10,14,10, 1,14,10, 6},
    { 0, 0, 1, 5, 5, 5, 5, 5,13, 9,13, 9,13, 9,13, 9, 5},
    { 0, 0, 0, 3, 3, 4, 4, 4, 4, 4,12,12, 8,12, 8,12, 8}}

  };  
  int yuv = p_Vid->yuv_format - 1;

  // se->value1 : numcoeff
  // se->value2 : numtrailingones
  se->len = lentab[yuv][se->value2][se->value1];
  se->inf = codtab[yuv][se->value2][se->value1];

  if (se->len == 0)
  {
    printf("ERROR: (numcoeff,trailingones) not valid: (%d, %d)\n",
      se->value1, se->value2);
    exit(-1);
  }

  symbol2vlc(se);

  writeUVLC2buffer(se, dp->bitstream);

  if(se->type != SE_HEADER)
    dp->bitstream->write_flag = 1;

#if TRACE
  if(dp->bitstream->trace_enabled)
    trace2out (se);
#endif

  return (se->len);
}


/*!
 ************************************************************************
 * \brief
 *    write VLC for TotalZeros
 ************************************************************************
 */
//CAVLCÖÐ,±àÂë×îºóÒ»¸ö·ÇÁã²Ð²îÏµÊýÖ®Ç°µÄÁãÏµÊýµÄ¸öÊý:TotalZeros
int writeSyntaxElement_TotalZeros(SyntaxElement *se, DataPartition *dp)
{
  //±í 9-7£­TotalCoeff( coeff_token ) 1µ½ 7µÄ4x4¿étotal_zeros±í¸ñ 
  //±í 9-8£­TotalCoeff( coeff_token ) 8µ½ 15µÄ4x4¿étotal_zeros±í¸ñ 
  static const byte lentab[TOTRUN_NUM][16] =
  {
    { 1,3,3,4,4,5,5,6,6,7,7,8,8,9,9,9},
    { 3,3,3,3,3,4,4,4,4,5,5,6,6,6,6},
    { 4,3,3,3,4,4,3,3,4,5,5,6,5,6},
    { 5,3,4,4,3,3,3,4,3,4,5,5,5},
    { 4,4,4,3,3,3,3,3,4,5,4,5},
    { 6,5,3,3,3,3,3,3,4,3,6},
    { 6,5,3,3,3,2,3,4,3,6},
    { 6,4,5,3,2,2,3,3,6},
    { 6,6,4,2,2,3,2,5},
    { 5,5,3,2,2,2,4},
    { 4,4,3,3,1,3},
    { 4,4,2,1,3},
    { 3,3,1,2},
    { 2,2,1},
    { 1,1},
  };

  static const byte codtab[TOTRUN_NUM][16] =
  {
    {1,3,2,3,2,3,2,3,2,3,2,3,2,3,2,1},
    {7,6,5,4,3,5,4,3,2,3,2,3,2,1,0},
    {5,7,6,5,4,3,4,3,2,3,2,1,1,0},
    {3,7,5,4,6,5,4,3,3,2,2,1,0},
    {5,4,3,7,6,5,4,3,2,1,1,0},
    {1,1,7,6,5,4,3,2,1,1,0},
    {1,1,5,4,3,3,2,1,1,0},
    {1,1,1,3,3,2,2,1,0},
    {1,0,1,3,2,1,1,1,},
    {1,0,1,3,2,1,1,},
    {0,1,1,2,1,3},
    {0,1,1,1,1},
    {0,1,1,1},
    {0,1,1},
    {0,1},
  };
  int vlcnum = se->len;

  // se->value1 : TotalZeros
  se->len = lentab[vlcnum][se->value1];
  se->inf = codtab[vlcnum][se->value1];

  if (se->len == 0)
  {
    printf("ERROR: (TotalZeros) not valid: (%d)\n",se->value1);
    exit(-1);
  }

  symbol2vlc(se);

  writeUVLC2buffer(se, dp->bitstream);

  if(se->type != SE_HEADER)
    dp->bitstream->write_flag = 1;

#if TRACE
  if(dp->bitstream->trace_enabled)
    trace2out (se);
#endif

  return (se->len);
}


/*!
 ************************************************************************
 * \brief
 *    write VLC for TotalZeros for Chroma DC
 ************************************************************************
 */
//CAVLCÖÐ,±àÂëÉ«¶ÈDC·ÖÁ¿×îºóÒ»¸ö·ÇÁã²Ð²îÏµÊýÖ®Ç°µÄÁãÏµÊýµÄ¸öÊý
int writeSyntaxElement_TotalZerosChromaDC(VideoParameters *p_Vid, SyntaxElement *se, DataPartition *dp)
{ 
  static const byte lentab[3][TOTRUN_NUM][16] =
  {
    //YUV420
   {{ 1,2,3,3},
    { 1,2,2},
    { 1,1}},
    //YUV422
   {{ 1,3,3,4,4,4,5,5},
    { 3,2,3,3,3,3,3},
    { 3,3,2,2,3,3},
    { 3,2,2,2,3},
    { 2,2,2,2},
    { 2,2,1},
    { 1,1}},
    //YUV444
   {{ 1,3,3,4,4,5,5,6,6,7,7,8,8,9,9,9},
    { 3,3,3,3,3,4,4,4,4,5,5,6,6,6,6},
    { 4,3,3,3,4,4,3,3,4,5,5,6,5,6},
    { 5,3,4,4,3,3,3,4,3,4,5,5,5},
    { 4,4,4,3,3,3,3,3,4,5,4,5},
    { 6,5,3,3,3,3,3,3,4,3,6},
    { 6,5,3,3,3,2,3,4,3,6},
    { 6,4,5,3,2,2,3,3,6},
    { 6,6,4,2,2,3,2,5},
    { 5,5,3,2,2,2,4},
    { 4,4,3,3,1,3},
    { 4,4,2,1,3},
    { 3,3,1,2},
    { 2,2,1},
    { 1,1}}
  };
  
  //±í 9-9£­É«¶ÈDC 2x2ºÍ2x4¿éµÄtotal_zeros±í¸ñ
  static const byte codtab[3][TOTRUN_NUM][16] =
  {
    //YUV420
   {{ 1,1,1,0},
    { 1,1,0},
    { 1,0}},
    //YUV422
   {{ 1,2,3,2,3,1,1,0},
    { 0,1,1,4,5,6,7},
    { 0,1,1,2,6,7},
    { 6,0,1,2,7},
    { 0,1,2,3},
    { 0,1,1},
    { 0,1}},
    //YUV444
   {{1,3,2,3,2,3,2,3,2,3,2,3,2,3,2,1},
    {7,6,5,4,3,5,4,3,2,3,2,3,2,1,0},
    {5,7,6,5,4,3,4,3,2,3,2,1,1,0},
    {3,7,5,4,6,5,4,3,3,2,2,1,0},
    {5,4,3,7,6,5,4,3,2,1,1,0},
    {1,1,7,6,5,4,3,2,1,1,0},
    {1,1,5,4,3,3,2,1,1,0},
    {1,1,1,3,3,2,2,1,0},
    {1,0,1,3,2,1,1,1,},
    {1,0,1,3,2,1,1,},
    {0,1,1,2,1,3},
    {0,1,1,1,1},
    {0,1,1,1},
    {0,1,1},
    {0,1}}
  };
  int vlcnum = se->len;
  int yuv = p_Vid->yuv_format - 1;  

  // se->value1 : TotalZeros
  se->len = lentab[yuv][vlcnum][se->value1];
  se->inf = codtab[yuv][vlcnum][se->value1];

  if (se->len == 0)
  {
    printf("ERROR: (TotalZeros) not valid: (%d)\n",se->value1);
    exit(-1);
  }

  symbol2vlc(se);

  writeUVLC2buffer(se, dp->bitstream);

  if(se->type != SE_HEADER)
    dp->bitstream->write_flag = 1;

#if TRACE
  if(dp->bitstream->trace_enabled)
    trace2out (se);
#endif

  return (se->len);
}


/*!
 ************************************************************************
 * \brief
 *    write VLC for Run Before Next Coefficient, VLC0
 ************************************************************************
 */
//CAVLCÖÐ,±àÂë·ÇÁã²Ð²îÏµÊýÇ°Á¬ÐøÁãÏµÊýµÄ¸öÊý(run_before)
int writeSyntaxElement_Run(SyntaxElement *se, DataPartition *dp)
{
  static const byte lentab[TOTRUN_NUM][16] =
  {
    {1,1},
    {1,2,2},
    {2,2,2,2},
    {2,2,2,3,3},
    {2,2,3,3,3,3},
    {2,3,3,3,3,3,3},
    {3,3,3,3,3,3,3,4,5,6,7,8,9,10,11},
  };

  static const byte codtab[TOTRUN_NUM][16] =
  {
    {1,0},
    {1,1,0},
    {3,2,1,0},
    {3,2,1,1,0},
    {3,2,3,2,1,0},
    {3,0,1,3,2,5,4},
    {7,6,5,4,3,2,1,1,1,1,1,1,1,1,1},
  };
  int vlcnum = se->len;

  // se->value1 : run
  se->len = lentab[vlcnum][se->value1];
  se->inf = codtab[vlcnum][se->value1];

  if (se->len == 0)
  {
    printf("ERROR: (run) not valid: (%d)\n",se->value1);
    exit(-1);
  }

  symbol2vlc(se);

  writeUVLC2buffer(se, dp->bitstream);

  if(se->type != SE_HEADER)
    dp->bitstream->write_flag = 1;

#if TRACE
  if(dp->bitstream->trace_enabled)
    trace2out (se);
#endif

  return (se->len);
}


/*!
 ************************************************************************
 * \brief
 *    write VLC for Coeff Level (VLC1)
 ************************************************************************
 */
//±àÂëÓàÏÂ·ÇÁãÏµÊýµÄ¼¶±ð(·ûºÅºÍ·ù¶È)Ò²°´µ¹Ðò½øÐÐ,¼´´Ó¸ßÆµ¿ªÊ¼
//±àÂëµÚÒ»¸ö·ÇÁãÏµÊý
int writeSyntaxElement_Level_VLC1(SyntaxElement *se, DataPartition *dp, int profile_idc)
{
  int level  = se->value1;  //level
  //printf("\033[1;31m VLC1: level = %d \n \033[0m",level);
  int sign   = (level < 0 ? 1 : 0);  //·ûºÅ1¸º0Õý
  int levabs = iabs(level);

  if (levabs < 8)
  {
  /*
       1<=levabs <8Ê±,±àÂë³ÉÒ»´®Ç°×º0¼Ó1¸ö1µÄÐÎÊ½
       ¼´[levabs * 2 + sign - 1]+[Ò»¸ö1]
   */
    se->len = levabs * 2 + sign - 1;  //Âë³¤
    se->inf = 1;
  }
  else if (levabs < 16) 
  {
  /*
       8<=levabs <16Ê±,±àÂë³ÉÒ»¼¶¶¨³¤Âë,ÆäÂë³¤Îª14+1+4=19
       ¼´[14¸ö0]+[Ò»¸ö1]+[((levabs -8) <<1) | sign]
   */
    // escape code1
    se->len = 19;
    se->inf = 16 | ((levabs << 1) - 16) | sign; //Âë×Ö
  }
  else
  {
  /*
       levabs <= 16Ê±,±àÂë³É¶þ¼¶¶¨³¤Âë,ÆäÂë³¤Îª14+2+12=28
       Âë×ÖÐÎÊ½:[15¸ö0]+[Ò»¸ö1]+[((levabs -16) <<1) | sign]
   */    
    int iMask = 4096, numPrefix = 0;
    int levabsm16 = levabs + 2032;

    // escape code2
    if ((levabsm16) >= 4096)
    {
      numPrefix++;
      while ((levabsm16) >= (4096 << numPrefix))
      {
        numPrefix++;
      }
    }
   
    iMask <<= numPrefix;
    se->inf = iMask | ((levabsm16 << 1) - iMask) | sign;  //Âë×Ö

    /* Assert to make sure that the code fits in the VLC */
    /* make sure that we are in High Profile to represent level_prefix > 15 */
    if (numPrefix > 0 && !is_FREXT_profile( profile_idc ))
    {
      //error( "level_prefix must be <= 15 except in High Profile\n",  1000 );
      se->len = 0x0000FFFF; // This can be some other big number
      return (se->len);
    }
    
    se->len = 28 + (numPrefix << 1);
  }

  symbol2vlc(se);

  writeUVLC2buffer(se, dp->bitstream);

  if(se->type != SE_HEADER)
    dp->bitstream->write_flag = 1;

#if TRACE
  if(dp->bitstream->trace_enabled)
    trace2out (se);
#endif

  return (se->len);
}


/*!
 ************************************************************************
 * \brief
 *    write VLC for Coeff Level
 ************************************************************************
 */
//±àÂëÓàÏÂ·ÇÁãÏµÊýµÄ¼¶±ð(·ûºÅºÍ·ù¶È)Ò²°´µ¹Ðò½øÐÐ,¼´´Ó¸ßÆµ¿ªÊ¼
//±àÂë³ýµÚÒ»¸öÒÔÍâµÄ·ÇÁãÏµÊý
int writeSyntaxElement_Level_VLCN(SyntaxElement *se, int vlc, DataPartition *dp, int profile_idc)
{  
  int level  = se->value1;
  //printf("\033[1;33m VLCN: level = %d \n \033[0m",level);
  int sign   = (level < 0 ? 1 : 0);
  int levabs = iabs(level) - 1;  

  int shift = vlc - 1;        
  int escape = (15 << shift);

  if (levabs < escape)
  {
    //°´±ä³¤·½Ê½±àÂë
    int sufmask   = ~((0xffffffff) << shift);
    int suffix    = (levabs) & sufmask;

    se->len = ((levabs) >> shift) + 1 + vlc;
    se->inf = (2 << shift) | (suffix << 1) | sign;
  }
  else
  {
    //±àÂë³É¶¨³¤Âë
    int iMask = 4096;
    int levabsesc = levabs - escape + 2048;
    int numPrefix = 0;

    if ((levabsesc) >= 4096)
    {
      numPrefix++;
      while ((levabsesc) >= (4096 << numPrefix))
      {
        numPrefix++;
      }
    }

    iMask <<= numPrefix;
    se->inf = iMask | ((levabsesc << 1) - iMask) | sign;

    /* Assert to make sure that the code fits in the VLC */
    /* make sure that we are in High Profile to represent level_prefix > 15 */
    if (numPrefix > 0 &&  !is_FREXT_profile( profile_idc ))
    {
      //error( "level_prefix must be <= 15 except in High Profile\n",  1000 );
      se->len = 0x0000FFFF; // This can be some other big number
      return (se->len);
    }
    se->len = 28 + (numPrefix << 1);
  }

  symbol2vlc(se);

  writeUVLC2buffer(se, dp->bitstream);

  if(se->type != SE_HEADER)
    dp->bitstream->write_flag = 1;

#if TRACE
  if(dp->bitstream->trace_enabled)
    trace2out (se);
#endif

  return (se->len);
}


#if TRACE
int bitcounter = 0;

/*!
 ************************************************************************
 * \brief
 *    Write out a trace string on the trace file
 ************************************************************************
 */
void trace2out(SyntaxElement *sym)
{
  int i, chars;

  if (p_Enc->p_trace != NULL)
  {
    putc('@', p_Enc->p_trace);
    chars = fprintf(p_Enc->p_trace, "%i", bitcounter);
    while(chars++ < 6)
      putc(' ',p_Enc->p_trace);

    chars += fprintf(p_Enc->p_trace, "%s", sym->tracestring);
    while(chars++ < 55)
      putc(' ',p_Enc->p_trace);

    // align bit pattern
    if(sym->len<15)
    {
      for(i=0 ; i<15-sym->len ; i++)
        fputc(' ', p_Enc->p_trace);
    }

    // print bit pattern
    bitcounter += sym->len;
    for(i=1 ; i<=sym->len ; i++)
    {
      if((sym->bitpattern >> (sym->len-i)) & 0x1)
        fputc('1', p_Enc->p_trace);
      else
        fputc('0', p_Enc->p_trace);
    }
    fprintf(p_Enc->p_trace, " (%3d) \n",sym->value1);
  }
  fflush (p_Enc->p_trace);
}

void trace2out_cabac(SyntaxElement *sym)
{
  int chars;

  if (p_Enc->p_trace != NULL)
  {
    putc('@', p_Enc->p_trace);
    chars = fprintf(p_Enc->p_trace, "%i", bitcounter);
    while(chars++ < 6)
      putc(' ',p_Enc->p_trace);

    chars += fprintf(p_Enc->p_trace, "%s", sym->tracestring);
    while(chars++ < 70)
      putc(' ',p_Enc->p_trace);

    fprintf(p_Enc->p_trace, " (%3d) \n",sym->value1);
  }
  fflush (p_Enc->p_trace);
  bitcounter += sym->len;
}
#endif


/*!
 ************************************************************************
 * \brief
 *    puts the less than 8 bits in the byte buffer of the Bitstream into
 *    the streamBuffer.
 *
 * \param p_Vid
 *    VideoParameters for current picture encoding
 * \param currStream
 *    the Bitstream the alignment should be established
 * \param cur_stats
 *   currently used statistics parameters
 *
 ************************************************************************
 */
void writeVlcByteAlign(VideoParameters *p_Vid, Bitstream* currStream, StatParameters *cur_stats)
{
  if (currStream->bits_to_go < 8)
  { // trailing bits to process
    currStream->byte_buf = (byte) ((currStream->byte_buf << currStream->bits_to_go) | (0xff >> (8 - currStream->bits_to_go)));
    currStream->streamBuffer[currStream->byte_pos++] = currStream->byte_buf;
    cur_stats->bit_use_stuffing_bits[p_Vid->type] += currStream->bits_to_go;    
    currStream->bits_to_go = 8;
  }
}

/*!
 ************************************************************************
 * \brief
 *    Resets the nz_coeff parameters for a macroblock
 ************************************************************************/
void reset_mb_nz_coeff(VideoParameters *p_Vid, int mb_number)
{
   memset(&p_Vid->nz_coeff [mb_number][0][0], 0, BLOCK_SIZE * (BLOCK_SIZE + p_Vid->num_blk8x8_uv) * sizeof(int));
}


void writeUVLC_CAVLC(Macroblock *currMB, SyntaxElement *se, DataPartition *dp)
{
  writeSE_UVLC(se, dp);
}

void writeSVLC_CAVLC(Macroblock *currMB, SyntaxElement *se, DataPartition *dp)
{
  writeSE_SVLC(se, dp);
}

void writeFlag_CAVLC(Macroblock *currMB, SyntaxElement *se, DataPartition *dp)
{
  writeSE_Flag(se, dp);
}

int bs_bitlength(Bitstream *bitstream)
{
  return (bitstream->byte_pos*8+(8-bitstream->bits_to_go));
}

/*!
************************************************************************
* \brief
*    Dummy function for reference picture bit writing
* \author
************************************************************************
*/
void writeRefPic_Dummy(Macroblock *currMB, SyntaxElement *se, DataPartition *dp )
{
  se->len = 0;
}

/*!
************************************************************************
* \brief
*    Function for writing the bits for the reference index when up to 2 refs are available
* \author
************************************************************************
*/
void writeRefPic_2Ref_CAVLC(Macroblock *currMB, SyntaxElement *se, DataPartition *dp )
{
  writeSE_invFlag( se, dp );
}

/*!
************************************************************************
* \brief
*    Function for writing the bits for the reference index when  N (N>2) refs are available
* \author
************************************************************************
*/
void writeRefPic_NRef_CAVLC(Macroblock *currMB, SyntaxElement *se, DataPartition *dp )
{
  writeSE_UVLC( se, dp );
}

/*!
 ************************************************************************
 * \brief
 *  Reads bits from the bitstream buffer
 *
 * \param buffer
 *    containing VLC-coded data bits
 * \param totbitoffset
 *    bit offset from start of partition
 * \param info
 *    returns value of the read bits
 * \param bitcount
 *    total bytes in bitstream
 * \param numbits
 *    number of bits to read
 *
 ************************************************************************
 */
int GetBits (byte *buffer,
             int totbitoffset,
             int *info, 
             int bitcount,
             int numbits)
{
  if ((totbitoffset + numbits ) > bitcount) 
  {
    return 0;
  }
  else
  {
    int bitoffset  = 7 - (totbitoffset & 0x07); // bit from start of byte
    int byteoffset = (totbitoffset >> 3); // byte from start of buffer
    int bitcounter = numbits;
    byte *curbyte  = &(buffer[byteoffset]);
    int inf = 0;

    while (numbits--)
    {
      inf <<=1;    
      inf |= ((*curbyte)>> (bitoffset--)) & 0x01;    
      if (bitoffset == -1 ) 
      { //Move onto next byte to get all of numbits
        curbyte++;
        bitoffset = 7;
      }
      // Above conditional could also be avoided using the following:
      // curbyte   -= (bitoffset >> 3);
      // bitoffset &= 0x07;
    }
    *info = inf;

    return bitcounter;           // return absolute offset in bit from start of frame
  }
}
