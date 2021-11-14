#ifndef _H_CODEC_UTILS_H_
#define _H_CODEC_UTILS_H_

#include <stdint.h>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h> 
}

/** 
 RFC-6184 -- RTP Payload Format for H.264 Video


 H264 NALU(network abstract layer unit)
 IDR: instantaneous decoding refresh is I frame, but I frame not must be IDR

 |0|1|2|3|4|5|6|7|
 |F|NRI|Type     |

 F: forbidden_zero_bit, must be 0
 NRI:nal_ref_idc, the importance indicator
 Type: nal_unit_type

 0x67(103) (0 11 00111) SPS     very important       type = 7
 0x68(104) (0 11 01000) PPS     very important       type = 8
 0x65(101) (0 11 00101) IDR     very important       type = 5
 0x61(97)  (0 11 00001) I       important            type = 1 it's not IDR
 0x41(65)  (0 10 00001) P       important            type = 1
 0x01(1)   (0 00 00001) B       not important        type = 1
 0x06(6)   (0 00 00110) SEI     not important        type = 6

 NAL Unit Type     Packet Type      Packet Type Name
 -------------------------------------------------------------
 0                 reserved         -
 1-23              NAL unit         Single NAL unit packet
 24                STAP-A           Single-time aggregation packet
 25                STAP-B           Single-time aggregation packet
 26                MTAP16           Multi-time aggregation packet
 27                MTAP24           Multi-time aggregation packet
 28                FU-A             Fragmentation unit
 29                FU-B             Fragmentation unit
 30-31             reserved
*/

const uint8_t* avc_find_start_code(const uint8_t *start, const uint8_t *end);
bool avc_find_key_frame(const uint8_t *data, size_t size);
int count_avc_key_frames(const uint8_t *data, size_t size);
int count_frames(const uint8_t *data, size_t size);

#endif
