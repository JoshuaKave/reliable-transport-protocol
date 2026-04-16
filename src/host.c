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
    
	host->seq_nums = calloc(glb_num_hosts, sizeof(uint8_t));	

	host->input_cmdlist_head = NULL;
    host->incoming_frames_head = NULL; 
    host->buffered_outframes_head = NULL; 
    host->outgoing_frames_head = NULL; 
    host->send_window = calloc(glb_sysconfig.window_size, sizeof(struct send_window_slot)); 
    for (int i = 0; i < glb_sysconfig.window_size; i++) {
        host->send_window[i].frame = NULL;
        host->send_window[i].timeout = NULL;
		host->send_window[i].lar = -1;
		host->send_window[i].lfs = -1;
    }
    host->latest_timeout = malloc(sizeof(struct timeval));
    gettimeofday(host->latest_timeout, NULL);
	
    // TODO: You should fill in this function as necessary to initialize variables
	host->receive_windows = calloc(glb_num_hosts, sizeof(struct receive_windows));
	host->print_buffer = calloc(256, sizeof(char*));
	for(int i = 0; i < glb_num_hosts; i++){
		host->receive_windows[i].nfe = 0;
		(host->receive_windows + i)->receive_window = calloc(glb_sysconfig.window_size, sizeof(struct receive_window_slot));
		for(int j = 0; j < glb_sysconfig.window_size; j++){
			(host->receive_windows + i)->receive_window[j].frame = NULL;
		}
		
	}

    // *********** PA1b ONLY ***********
    host->cc = calloc(glb_num_hosts, sizeof(CongestionControl));
    for (int i = 0; i < glb_num_hosts; i++) {
        host->cc[i].cwnd = 1.0; 
        host->cc[i].ssthresh = (double)glb_sysconfig.window_size; 
        host->cc[i].dup_acks = 0; 
        host->cc[i].state = cc_SS; 
    }
}

void run_hosts() {
    run_senders(); 
    send_data_frames(); 
    run_receivers(); 
    send_ack_frames(); 
}
