#include "host.h"
#include "frame_utils.h"
#include "switch.h"
#include "receiver.h"
#include <assert.h>

void handle_incoming_frames(Host* host) {
    // TODO: Suggested steps for handling incoming frames
    //    1) Dequeue the Frame from the host->incoming_frames_head
    //    2) Compute CRC of incoming frame to know whether it is corrupted
    //    3) If frame is corrupted, drop it and move on.
    //    4) Implement logic to check if the expected frame has come
    //    5) Implement logic to combine payload received from all frames belonging to a message
    //       and print the final message when all frames belonging to a message have been received.
    //    6) Implement the cumulative acknowledgement part of the sliding window protocol
    //    7) Append acknowledgement frames to the outgoing_frames_head queue
    int incoming_frames_length = ll_get_length(host->incoming_frames_head);
    while (incoming_frames_length > 0) {
//		printSendWindow(host);
        // Pop a node off the front of the link list and update the count
		LLnode* ll_inmsg_node = ll_pop_node(&host->incoming_frames_head);
		incoming_frames_length = ll_get_length(host->incoming_frames_head);
		//fprintf(stderr, "incoming_frames_length: %d\n", incoming_frames_length);
        Frame* inframe = ll_inmsg_node->value; 
		//fprintf(stderr, "seqnum: %d\n", inframe->seq_num);	
		if(isFrameCorrupted(inframe))
		{
			//fprintf(stderr, "Frame is corrupted!\n");
			free(inframe);
			free(ll_inmsg_node);
			continue;
		}

		struct receive_window_slot* receive_window = (host->receive_windows + inframe->src_id)->receive_window; 
		uint8_t nfe = host->receive_windows[inframe->src_id].nfe;
		//fprintf(stderr, "nfe: %d\n", nfe);
		if(inframe->Flags == 1){
			//fprintf(stderr, "Frame is ack\n");
			free(inframe);
			free(ll_inmsg_node);
			continue;
		}	
		if(isReceiveWindowFull(receive_window)){
			sendAck(host, inframe);
			//fprintf(stderr, "Window is full, dropping frame\n");
			free(inframe);
			free(ll_inmsg_node);
			continue;	
	
		}
		if(!frameInBounds(nfe, inframe)){
			sendAck(host, inframe);
			//fprintf(stderr, "Frame out of bounds, dropping frame\n");
			free(inframe);
			free(ll_inmsg_node);
			continue;
		}
		uint8_t idx = inframe->seq_num % glb_sysconfig.window_size;
		struct receive_window_slot* slot = &receive_window[idx]; 	
		
		if(slot->frame == NULL){
			slot->frame = malloc(sizeof(Frame));
			memcpy(slot->frame, inframe, sizeof(Frame));
		}

		struct receive_windows* rw = &(host->receive_windows[inframe->src_id]);
		while(rw->receive_window[rw->nfe % glb_sysconfig.window_size].frame != NULL){
			struct receive_window_slot* curr = &rw->receive_window[rw->nfe % glb_sysconfig.window_size];
			printMessage(host, curr->frame);
			rw->nfe++;
			free(curr->frame);
			curr->frame = NULL;

		}	

		sendAck(host, inframe);
        free(inframe);
        free(ll_inmsg_node);
    }
}

void sendAck(Host* host, Frame* frame){
	uint8_t nfe = host->receive_windows[frame->src_id].nfe;
	uint8_t ack_num = nfe - 1;

	Frame* outgoing_frame = malloc(sizeof(Frame));
	assert(outgoing_frame);
	set_frame_members(outgoing_frame, 0, frame->src_id, frame->dst_id, 0, ack_num, 1);
	set_frame_crc(outgoing_frame);
	ll_append_node(&host->outgoing_frames_head, outgoing_frame);

}

bool frameInBounds(uint8_t nfe, Frame* frame){

	int diff = seq_num_diff(nfe, frame->seq_num);
	return diff >= 0 && diff < glb_sysconfig.window_size;
}

void printMessage(Host* host, Frame* frame){
	
	if(host->print_buffer[frame->src_id] == NULL){

		host->print_buffer[frame->src_id] = calloc(MAX_SEQ_NUM * FRAME_PAYLOAD_SIZE, sizeof(char));

	}

	strcat(host->print_buffer[frame->src_id], frame->data);

	if(frame->remaining_msg_bytes == 0){

		printf("<RECV_host-%d>:[%s]\n", host->id, host->print_buffer[frame->src_id]);
		memset(host->print_buffer[frame->src_id], 0, MAX_SEQ_NUM * FRAME_PAYLOAD_SIZE);

	}	

}

void run_receivers() {
    int recv_order[glb_num_hosts]; 
    get_rand_seq(glb_num_hosts, recv_order); 

    for (int i = 0; i < glb_num_hosts; i++) {
        int recv_id = recv_order[i]; 
        handle_incoming_frames(&glb_hosts_array[recv_id]); 
    }
}
