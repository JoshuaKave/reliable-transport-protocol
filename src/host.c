#include "host.h"
#include "sender.h"
#include "receiver.h"
#include "switch.h"
#include "util.h"
#include <assert.h>

void init_host(Host* host, int id) {
    host->id = id;
    host->active = 0; 
    host->awaiting_ack = 0; 
    host->round_trip_num = 0; 
    host->csv_out = 0; 
    
    host->input_cmdlist_head = NULL;
    host->incoming_frames_head = NULL; 
    host->buffered_outframes_head = NULL; 
    host->outgoing_frames_head = NULL; 
    host->send_window = calloc(glb_sysconfig.window_size, sizeof(struct send_window_slot)); 
    for (int i = 0; i < glb_sysconfig.window_size; i++) {
        host->send_window[i].frame = NULL;
        host->send_window[i].timeout = NULL;
    }
    host->latest_timeout = malloc(sizeof(struct timeval));
    gettimeofday(host->latest_timeout, NULL);

    // TODO: You should fill in this function as necessary to initialize variables


    // *********** PA1b ONLY ***********
    host->cc = calloc(glb_num_hosts, sizeof(CongestionControl));
    for (int i = 0; i < glb_num_hosts; i++) {
        host->cc[i].cwnd = 1.0; 
        host->cc[i].ssthresh = (double)glb_sysconfig.window_size; 
        host->cc[i].dup_acks = 0; 
        host->cc[i].state = cc_SS; 
    }
}

void set_frame_members(Frame* frame, uint16_t remaining_msg_bytes, uint8_t dst_id, uint8_t src_id, uint8_t seq_num, uint8_t ack_num, uint8_t flags){

	frame->remaining_msg_bytes = 0;
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
	
	frame->checksum = crc_val; 	 	

}

void run_hosts() {
    run_senders(); 
    send_data_frames(); 
    run_receivers(); 
    send_ack_frames(); 
}
