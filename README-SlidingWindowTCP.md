```
  🔱  T R I T O N T A L K  —  S L I D I N G   W I N D O W   T C P  🔱

     SENDER (HOST A)                          RECEIVER (HOST B)
    .-----------.                              .-----------.
    |  .-----. |    [SEQ 0]─────────────────> |  .-----. |
    |  |     | |    [SEQ 1]─────────────────> |  |     | |
    |  | >_  | |    [SEQ 2]─────────────────> |  | >_  | |
    |  |     | |    [SEQ 3]─────────────────> |  |     | |
    |  '-----' | <─────────────────[ACK 4]    |  '-----' |
    |    🔱    |                              |    🔱    |
    '-----------'   window: [0][1][2][3]      '-----------'
         |||          ^^ 4 in-flight ^^            |||
        _|||_                                     _|||_
       [_____]                                   [_____]
        HOST A                                    HOST B
```

# TritonTalk

A simulated Ethernet switch with reliable transport — hosts send and receive
framed messages through a virtual switch with configurable corruption, window
sizing, and congestion control.

---

## Project Info

| Field | Value |
|-------|-------|
| **Name** |Joshua Kave|
| **PID**  |A17391911|

---

## Running

> Requires Docker installed on your machine. Builds a clean Ubuntu 22.04 container and mounts the current
> directory — no local dependencies needed.

If you prefer to develop entirely in the terminal (with VIM no IDE required), you can use
`./dockerrun.sh` to run any command inside the Docker container. For example:

```bash
./dockerrun.sh make                                  # compile the project
./dockerrun.sh ./tritontalk test_suite/basic.cfg     # run tritontalk directly
./dockerrun.sh ./tester.sh                           # run the test suite
```

Alternatively, you can develop and run the project inside **VS Code** using the
Dev Container setup described in the project description.

---

## Testing

Test cases live in `test_suite/test{1,2,3}/`. Each test compares `stdout.txt`
against `exp_out.txt`. CSV diagnostics are written to `csv_files/`.

---

## Config Files

| File                      | Description                 |
|---------------------------|-----------------------------|
| `test_suite/basic.cfg`    | Basic 2-host setup          |
| `test_suite/cc_basic.cfg` | Congestion control baseline |
| `test_suite/sw4.cfg`      | 4-port switch topology      |
| `test_suite/corrupt.cfg`  | Max corruption and 256 hosts|

<hr>

# Design Documentation

Tritontalk is a program that takers user commands to send messages between hosts. 

It parses user input, splits messages into frames, then sends those frames to a receiver host, while also buffering those frames.

The receiver verifies frame corruption (drops if corrupted), buffers those frames, then sends an ack to the sender. Acks are cumulative, so only 1 ack is sent after successfully receiving consecutive amounts of frames.

When the sender receives an ack, it drops the corresponding frame from the send window buffer. If no ack is received (which means the ack got corrupted, the sent frame got corrupted, or the receive_window was full), the sender resends the frame after a timeout.

The receiver then prints out the message all at once after it successfully receives all frames corresponding to that message.

Here are structs and functions used to implement this successfully:

## Structs

### Host
The host struct has the following important members that I added to complete functionality:

- __uint8_t* seq_nums__ - represents the sequence number to assign to the frame. We use a pointer so that each host has its own seq_num, rather than sharing one globally between all hosts. This field is also necessary. If the Frame struct was the only struct that stored sequence numbers, every new message would restart at sequence 0. Having the host store the sequence number allows for preserving state. This is also how each send_window implements the sliding window protocol: the seq_num for the host is used to indicate which frame to send next as a minimum.

- __struct receive_windows* receive_window__ - A pointer to the receive_windows struct, which stores an array of receive_window structs for each host. I found it necessary to have hosts hold a unique receive_window for each host, so that it could handle multiple hosts sending information at once. More information on the receive_windows struct below.

- __char** print_buffer__ - A buffer to hold reassembled messages before printing. This is to account for cases where frames exceed the receiver window size, so the host needs a place to store frames while waiting for the whole message. A host has a separate print_buffer for each unique host id.

### Frame
The frame struct has the following impprtant members that I added to complete functionality:

- __uint8_t ack_num__ - Used to keep track of the seq_num the ack is for
- __uint8_T Flags__ - Used to indicate if the frame is an ack or part of a message
- __uint8_t checksum__ - Used to store the crc8 checksum of the frame. Stored at the end of the struct so that the crc8 algorithm can loop through the frame just once to compute the checksum.
- We set FRAME_PAYLOAD_SIZE to 55 to accommodate the max frame size which is 64.

### receive_windows
Represents the container of all receive_windows of each host.

- __struct receive_window_slot*__ - A pointer to the receive_window of the particular host, indexed with the host id.
- __uint8_t nfe__ - Represents the next frame expected of the receiver, which is used to derive both lfr and rws. Because the host member of struct receive_windows* is a pointer, each host has its own memory for this struct. Therefore, each host has its own unique nfe.

### receive_window_slot
Represents the receive_window of a particular host. Each receive_windows struct has a receive_window_slot pointer, which is allocated memory for glb_sysconfig.window_size number of memory.

- __Frame* frame__ - The frame stored in the receive_window slot.


# Functions

## sender.c

Here are functions that I implemented to support the sliding window protocol

### handle_input_cmds

This is the first function that is hit that involves sending frames. It takes the parsed messages from user input, and turns them into frames. The function is also responsible for preparing these frames to be sent.

An important part of the frame sending logic that is implemented in handle_input_cmds is the fragmentation. After a message is taken from the input, there is a loop that continously takes chunks of the message of size FRAME_PAYLOAD_SIZE and turns those into frames. This happens until the message has been completely turned into frames. Each frame is then set to be sent over the network. Fragmentation is important for many reasons, including multiplexing.

### handle_outgoing_frames

This function actually calls the frame sending logic, tying switch.c and sender.c together. It also handles sending timedout frames.

- For frames put into the send buffer from handle_input_cmds, handle_outgoing_frames pops those frames, copies them into the host's send window, then sends those frames to the receiver (i.e. appends to outgoing_frames_head which switch.c will use to send frames).
- For timed out frames, handle_outgoing_frames checks if frames timed out by seeing if the timeval == NULL (which handle_timedout_frames sets to NULL if a frame times out), then resends those frames.

### handle_timedout_frames

Sets timed out frames timeval values to NULL. This function is called before handle_outgoing_frames to ensure that handle_outgoing_frames is able to detect timed out frames.

### handle_incoming_acks

This is the function that removes frames from the send_window when an ack arrives, which ensures that handle_outgoing_frames does not try to resend frames that were ack'd. It also checks to makes sure that received frames are not acks (which should be handled by receiver.c), and are not corrupted. 

### host_get_next_expiring_timeval

This function just loops through the send_window and gets the smallest timeval, which represents the frame that is closest to expiring.

## Receiver.c

### handle_incoming_frames

This is the main function that, well, handles incoming frames. It checks if the frame can fit in the receiver window, if it is corrupted, and if it is an actual frame and not an ack. If all conditions pass, the frame gets added to the receiver_window, and an ack is sent back to the sender. This function also checks to see if all frames of a message have been received through printMessage(), and then prints the whole message and clears the send_window and print_buffer if yes (the send_window is also cleared and moved to the print buffer when full).

### send_ack

Function used to send an ack to the correct sender. Uses nfe to decided the ack number (cumulative ack implementation).

### frameInBounds

Checks if the frame is within the receive window bounds of the host

### print_message

Allocates space for the print buffer, and adds the received message frame to the print buffer. If the received message frame is the last frame, then it prints out the entire message.

## Frame Utils

I created a file called frame_utils that has some helper functions that I use.

### set_frame_members

Function that initializes frame struct members when creating new frames.

### set_frame_crc

Calculates the frame crc using compute_crc8 and then sets the checksum value of the given frame to that crc value

### isFrameCorrupted

Takes the given frame and its checksum, and returns true if the checksum is not 0 and false if it is (using implicit conversion). I also realize just now that some of my functions are camel case and others are snake case which is probably not good practice but I do not want to go through and change everything 😢

### printSendWindow

Function to print to stderr the contents of a send window. Used for debugging.

### isReceiveWindowFull

Checks if the receive window is full. Honestly should have put this function in receiver.c

<hr>

### Thank you for reading my design document! 

I also wrote code in host.c, but since that was just to initialize memory and values for struct members, I felt that was not important to touch on.


