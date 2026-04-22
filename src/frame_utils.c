#include "frame_utils.h"
#include "util.h"
#include <stdbool.h>

void set_frame_members(Frame* frame, uint16_t remaining_msg_bytes, uint8_t dst_id, uint8_t src_id, uint8_t seq_num, uint8_t ack_num, uint8_t flags){

	frame->remaining_msg_bytes = remaining_msg_bytes;
	frame->src_id = src_id;
	frame->dst_id = dst_id;
	frame->seq_num = seq_num;
	frame->ack_num = ack_num;
	frame->Flags = flags;
	frame->checksum = 0;
		
}

void set_frame_crc(Frame* frame){

	char* frame_char = convert_frame_to_char(frame);
	uint8_t crc_val = compute_crc8(frame_char);
	free(frame_char);
	frame->checksum = crc_val; 	 	

}

bool isFrameCorrupted(Frame* frame){

	char* char_frame = convert_frame_to_char(frame);
	uint8_t result = compute_crc8(char_frame);	
	free(char_frame);
	return result;
}

void printSendWindow(Host* host){
	fprintf(stderr, "=========================\n");
	for(int i = 0; i < glb_sysconfig.window_size; i++){
		
		if(host->send_window + i == NULL){
			fprintf(stderr, "SLOT [%d] == NULL\n", i);
			continue;
		}
		struct send_window_slot window_slot = host->send_window[i];
		
		fprintf(stderr, "SLOT [%d]\n", i);
		if(window_slot.frame == NULL){
			fprintf(stderr, "Frame - NULL\n");
			continue;
		}
		print_frame(window_slot.frame);
		if(window_slot.timeout == NULL){
			fprintf(stderr, "Timeout - NULL\n");
			continue;
		}
		fprintf(stderr, "Timeout - %ld.%06ld\n", (long)window_slot.timeout->tv_usec); 	
	
	}

	fprintf(stderr, "=========================\n");
	

}

bool isReceiveWindowFull(struct receive_window_slot* slot){

	for(int i = 0; i < glb_sysconfig.window_size; i++){

		Frame* frame = slot[i].frame;
		if(frame == NULL){
			return false;
		}

	}
	
	return true;

}
