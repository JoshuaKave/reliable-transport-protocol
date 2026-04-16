#ifndef __FRAME_UTILS__
#define __FRAME_UTILS__

#include "common.h"
#include "util.h"
#include <stdbool.h>

void set_frame_members(Frame* frame, uint16_t remaining_msg_bytes, uint8_t dst_id, uint8_t src_id, uint8_t seq_num, uint8_t ack_num, uint8_t flags);	

void set_frame_crc(Frame* frame);

bool isFrameCorrupted(Frame* frame);

void printSendWindow(Host* h);

bool isReceiveWindowFull(struct receive_window_slot* slot);
#endif
