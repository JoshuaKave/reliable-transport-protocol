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

A simulated Ethernet switch with reliable transport. Hosts send and receive framed messages through a virtual switch with configurable corruption, window sizing, cumulative ACKs, retransmission, and congestion control.

This cumulative README combines the base Sliding Window TCP design documentation with the AIMD / Slow Start / Fast Retransmit / Fast Recovery congestion-control extension.

---

## Project Info

| Field | Value |
|-------|-------|
| **Name** | Joshua Kave |

---

## Testing

Test cases live in `test_suite/test{1,2,3}/`. Each test compares `stdout.txt` against `exp_out.txt`. CSV diagnostics are written to `csv_files/`.

---

## Config Files

| File                      | Description                 |
|---------------------------|-----------------------------|
| `test_suite/basic.cfg`    | Basic 2-host setup          |
| `test_suite/cc_basic.cfg` | Congestion control baseline |
| `test_suite/sw4.cfg`      | 4-port switch topology      |
| `test_suite/corrupt.cfg`  | Max corruption and 256 hosts|

---

# Design Documentation

TritonTalk takes user commands to send messages between hosts. It parses user input, splits messages into frames, sends those frames to a receiver host, and buffers outstanding frames in the sender window.

The receiver verifies frame corruption, drops corrupted frames, buffers valid frames, and sends cumulative ACKs back to the sender. A cumulative ACK represents the next expected frame, so only one ACK is needed after successfully receiving a consecutive sequence of frames.

When the sender receives an ACK, it removes the corresponding acknowledged frames from the send-window buffer. If no ACK is received because the ACK was corrupted, the sent frame was corrupted, or the receive window was full, the sender retransmits the frame after a timeout.

The receiver prints a full message only after it successfully receives all frames corresponding to that message.

---

## Base Sliding Window TCP Implementation

### Structs

#### `Host`

The `Host` struct has the following important members added for the sliding-window implementation:

- `__uint8_t* seq_nums` — represents the sequence number to assign to the next frame. A pointer is used so each host has its own sequence-number state rather than sharing one globally. This is necessary because if only the `Frame` struct stored sequence numbers, every new message would restart at sequence `0`. Storing sequence-number state in the host preserves progress across messages and allows each sender to implement the sliding-window protocol.
- `struct receive_windows* receive_window` — stores an array of receive-window structs for each host. This allows one host to handle multiple other hosts sending information at the same time.
- `char** print_buffer` — stores reassembled message data before printing. This is needed when a message exceeds the receiver window size. Each host has a separate print buffer for each unique sender host ID.

#### `Frame`

The `Frame` struct has the following important members added for functionality:

- `uint8_t ack_num` — tracks the sequence number that the ACK is acknowledging.
- `uint8_t flags` — indicates whether the frame is an ACK or part of a message.
- `uint8_t checksum` — stores the CRC8 checksum of the frame. It is stored at the end of the struct so the CRC8 algorithm can loop through the frame once to compute the checksum.
- `FRAME_PAYLOAD_SIZE` is set to `55` to accommodate the maximum frame size of `64` bytes.

#### `receive_windows`

Represents the container of all receive windows for each host.

- `struct receive_window_slot*` — pointer to the receive window for a particular host, indexed by host ID.
- `uint8_t nfe` — represents the next frame expected by the receiver. This is used to derive both LFR and RWS. Since each host's `receive_windows*` member points to unique memory, each host has its own `nfe` state.

#### `receive_window_slot`

Represents one slot in the receive window for a particular host.

- `Frame* frame` — the frame stored in the receive-window slot.

---

## Sliding Window Functions

### `sender.c`

#### `handle_input_cmds`

This is the first function involved in sending frames. It takes parsed messages from user input and turns them into frames.

An important part of this function is fragmentation. After a message is taken from input, a loop continuously takes chunks of size `FRAME_PAYLOAD_SIZE` and turns them into frames until the whole message has been converted. Each frame is then prepared to be sent over the network. Fragmentation is important because it allows larger messages to be split across multiple bounded-size frames.

#### `handle_outgoing_frames`

This function ties `switch.c` and `sender.c` together by actually calling the frame-sending logic. It also handles retransmitting timed-out frames.

- For frames placed into the send buffer by `handle_input_cmds`, `handle_outgoing_frames` pops those frames, copies them into the host's send window, and sends them to the receiver by appending them to `outgoing_frames_head`, which `switch.c` uses to deliver frames.
- For timed-out frames, `handle_outgoing_frames` checks whether a frame's `timeval` is `NULL`. If so, the function resends that frame.

#### `handle_timedout_frames`

Sets timed-out frame `timeval` values to `NULL`. This function is called before `handle_outgoing_frames` so that `handle_outgoing_frames` can detect and retransmit timed-out frames.

#### `handle_incoming_acks`

Removes frames from the send window when an ACK arrives. This ensures that `handle_outgoing_frames` does not try to resend frames that were already acknowledged. It also verifies that received frames are ACKs and are not corrupted.

#### `host_get_next_expiring_timeval`

Loops through the send window and returns the smallest `timeval`, representing the frame closest to expiring.

### `receiver.c`

#### `handle_incoming_frames`

Handles incoming data frames. It checks whether each frame can fit in the receiver window, whether it is corrupted, and whether it is a data frame rather than an ACK. If all checks pass, the frame is added to the receive window and an ACK is sent back to the sender.

This function also checks whether all frames of a message have been received through `print_message()`. If the full message has been received, the receiver prints the complete message and clears the relevant receive-window and print-buffer state.

#### `send_ack`

Sends an ACK to the correct sender. It uses `nfe` to determine the cumulative ACK number.

#### `frameInBounds`

Checks whether a frame is within the receiver window bounds.

#### `print_message`

Allocates space for the print buffer and adds the received message frame to it. If the received frame is the last frame of the message, the function prints the entire reassembled message.

### Frame Utilities

A helper file, `frame_utils`, contains shared functions used by the sender and receiver.

#### `set_frame_members`

Initializes frame struct members when creating new frames.

#### `set_frame_crc`

Calculates the frame CRC using `compute_crc8` and sets the frame's checksum value.

#### `isFrameCorrupted`

Takes a frame and its checksum and returns whether the frame is corrupted. It uses the CRC result to determine whether corruption occurred.

#### `printSendWindow`

Prints the contents of a send window to `stderr`. Used for debugging.

#### `isReceiveWindowFull`

Checks whether the receive window is full.

---

# Congestion Control Extension

The congestion-control implementation uses **Additive Increase Multiplicative Decrease (AIMD)** with **Slow Start**, **Fast Retransmit**, and **Fast Recovery**. Each sender tracks a separate congestion-control state for every possible receiver, so the sender can maintain independent `cwnd`, `ssthresh`, duplicate-ACK count, and state values per sender/receiver pair.

## Congestion-Control Send Rule

A frame may only be sent if the number of currently in-flight frames for that destination is less than:

```c
min(cwnd, glb_sysconfig.window_size)
```

This ensures the sender respects both the receiver/window-size limit and the dynamic congestion window.

---

## Congestion-Control State Transitions

- **Slow Start (`cc_SS`)**: Starts with `cwnd = 1`. For every new ACK, increase `cwnd` by `1`, causing exponential growth over each RTT.
- **AIMD / Congestion Avoidance (`cc_AIMD`)**: Once `cwnd >= ssthresh`, increase `cwnd` by `1 / cwnd` for each new ACK.
- **Fast Retransmit and Fast Recovery (`cc_FRFT`)**: On three duplicate ACKs for the same frame number, that frame is retransmitted immediately. Then `ssthresh = max(cwnd / 2, 2)` and `cwnd = ssthresh + 3`. Additional duplicate ACKs inflate `cwnd` by `1`. A new ACK exits Fast Recovery by setting `cwnd = ssthresh` and returning to AIMD.
- **Timeout**: On timeout, set `ssthresh = max(cwnd / 2, 2)`, reset `cwnd = 1`, clear duplicate-ACK tracking, enter Slow Start, and mark all outstanding frame timeouts as `NULL` so they are retransmitted from `handle_outgoing_frames()`.

---

## Congestion-Control Struct Changes

The main struct change from the base sliding-window implementation was adding a `last_ack` field to the `CongestionControl` struct.

`last_ack` records the most recent ACK number received from a particular receiver. It is used to determine whether a newly received ACK is a duplicate ACK or an ACK for new data. In `host.c`, `last_ack` is initialized to `255` as a sentinel value so the first real ACK is treated as new data rather than being incorrectly counted as a duplicate ACK for sequence number `0`.

---

## Congestion-Control Function Changes

### `handle_incoming_acks` in `sender.c`

This function was extended to handle ACK processing, duplicate-ACK detection, Fast Retransmit, Fast Recovery, and congestion-window updates.

For every incoming frame:

1. Pop the frame from `incoming_frames_head`.
2. Drop it if the ACK frame is corrupted.
3. If it is not an ACK frame, put it back on the incoming queue.
4. Extract `ack_num` and `src_id`.
5. Compare `ack_num` against `host->cc[src_id].last_ack`.

If `ack_num == last_ack`, the ACK is treated as a duplicate ACK. If `ack_num != last_ack`, it is treated as a new ACK. On a new ACK, the implementation resets duplicate-ACK tracking and updates `last_ack`.

#### Fast Retransmit

When exactly three duplicate ACKs are received, the sender immediately retransmits the frame corresponding to the duplicate ACK sequence number. After the retransmission, the sender updates `latest_timeout` so that timeout ordering remains consistent with the rest of the send path.

#### Fast Recovery

On the third duplicate ACK, the function performs Fast Recovery and updates congestion-control variables:

```c
ssthresh = max(cwnd / 2.0, 2.0);
cwnd = ssthresh + 3;
state = cc_FRFT;
```

For duplicate ACKs after the third one, the function inflates the congestion window by one:

```c
cwnd += 1;
```

When a new ACK arrives while the sender is in Fast Recovery, the implementation exits Fast Recovery by setting:

```c
cwnd = ssthresh;
state = cc_AIMD;
```

#### Slow Start and AIMD

For a new ACK that is not being used to exit Fast Recovery:

- If `cwnd < ssthresh`, the sender remains in Slow Start and increments `cwnd` by `1`.
- Otherwise, the sender enters AIMD and increments `cwnd` by `1 / cwnd`.

### `handle_timedout_frames` in `sender.c`

This function was updated to time out all frames in the send window when a frame times out:

1. Identify the destination associated with the timed-out frame.
2. Apply multiplicative decrease:

```c
ssthresh = max(cwnd / 2.0, 2.0);
cwnd = 1;
dup_acks = 0;
state = cc_SS;
```

3. Call `timeout_window_frames(host)`.
4. Return after handling one timeout event.

Retransmission is deferred to `handle_outgoing_frames()`.

### `timeout_window_frames` in `sender.c`

This helper separates the responsibility of finding and timing out frames in the send window from `handle_timedout_frames`.

### `handle_outgoing_frames` in `sender.c`

This function was updated to comply with congestion-control rules so that outgoing frames respect congestion-window boundaries.

The function first counts:

- `curr_frames`: frames currently considered in flight, meaning send-window slots with both `frame != NULL` and `timeout != NULL`.
- `timedout_frames`: outstanding frames whose `frame != NULL` but `timeout == NULL`.

This distinction is important because a timed-out frame is still unacknowledged and still occupies the sender window, but it needs to be retransmitted before newer buffered frames are allowed to use the connection.

#### Retransmitting Timed-Out Frames First

Before sending any new buffered frames, the function repeatedly searches for a timed-out frame in the send window. Among timed-out frames, it selects the oldest one by sequence number, retransmits it with `send_new_frame()`, increments `curr_frames`, and decrements `timedout_frames`.

Retransmission still respects the congestion window:

```c
curr_frames < min(cwnd, glb_sysconfig.window_size)
```

If there are still timed-out frames waiting after the retransmission loop, the function updates `latest_timeout` and returns without sending new frames. This prevents newer frames from being sent before older timed-out frames have been retransmitted.

#### Sending New Buffered Frames

Only after all timed-out frames have been retransmitted does the sender consider new frames from `buffered_outframes_head`. The logic is similar to the base sliding-window implementation, except it is additionally bounded by the congestion-control window.

---

## Notes

Some initialization code was also added in `host.c` to allocate memory and initialize values for the added struct members and congestion-control state.

---

### Thank you for reading my design document!
