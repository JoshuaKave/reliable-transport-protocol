# Design Documentation

This implementation extends the Project 1a sliding-window sender with congestion control.

The congestion control implementation uses **Additive Increase Multiplicative Decrease (AIMD)** with **Slow Start**, **Fast Retransmit**, and **Fast Recovery**. Each sender tracks a separate congestion-control state (implemented as a struct) for every possible receiver, so the sender can maintain independent `cwnd`, `ssthresh`, duplicate-ACK count, and state values per sender/receiver pair.

## Structs and Vars

A frame may only be sent if the number of currently in-flight frames for that destination is less than:

```c
min(cwnd, glb_sysconfig.window_size)
```

The congestion-control state transitions are:

- **Slow Start (`cc_SS`)**: Start with `cwnd = 1`. For every new ACK, increase `cwnd` by `1`, causing exponential growth over each RTT.
- **AIMD / Congestion Avoidance (`cc_AIMD`)**: Once `cwnd >= ssthresh`, increase `cwnd` by `1 / cwnd` for each new ACK.
- **Fast Recovery and Fast Retransmit (`cc_FRFT`)**: On three duplicate ACKs for the same frame number, that frame is retransmited immediately. Then state var are set to the following: `ssthresh = max(cwnd / 2, 2)`, and `cwnd = ssthresh + 3`. Additional duplicate ACKs inflate `cwnd` by `1`. A new ACK exits Fast Recovery by setting `cwnd = ssthresh` and returning to AIMD.
- **Timeout**: On timeout, `ssthresh = max(cwnd / 2, 2)`, reset `cwnd = 1`, clear duplicate ACK tracking, enter Slow Start, and mark all outstanding frame timeouts as `NULL` so they are retransmitted from `handle_outgoing_frames()`.

## Struct Changes

The only struct change I added from Project 1a was adding a `last_ack` field to the `CongestionControl` struct:


`last_ack` records the most recent ACK number received from a particular receiver. It is used to determine whether a newly received ACK is a duplicate ACK or an ACK for new data. In `host.c`, `last_ack` is initialized to `255` as a sentinel value so that the first real ACK is treated as new data rather than being incorrectly counted as a duplicate ACK for sequence number `0`.

## Modified Functions

### `handle_incoming_acks` in `sender.c`

This function was extended to handle ACK processing, duplicate ACK detection, Fast Retransmit, Fast Recovery, and congestion-window updates.

For every incoming frame:

1. Pop the frame from `incoming_frames_head`.
2. Drop it if the ACK frame is corrupted.
3. If it is not an ACK frame, put it back on the incoming queue.
4. Extract `ack_num` and `src_id`.
5. Compare `ack_num` against `host->cc[src_id].last_ack`.

If `ack_num == last_ack`, the ACK is treated as a duplicate ACK. If the ACK number is different from `last_ack`, it is treated as a new ACK. The implementation resets duplicate ACK tracking and updates `last_ack`.

#### Fast Retransmit

When exactly three duplicate ACKs are received, the sender immediately retransmits the frame with corresponding dup ack sequence number. After the retransmission, the sender updates `latest_timeout` so that timeout ordering remains consistent with the rest of the send path.

#### Fast Recovery

On the third duplicate ACK, the function performs Fast Recovery and updates congestion control vars as so:

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

- If `cwnd < ssthresh`, the sender remains in Slow Start and increments `cwnd` by `1` (exponential growth).
- Otherwise, the sender enters AIMD and increments `cwnd` by `1 / cwnd`.

### `handle_timedout_frames` in `sender.c`

This function was updated to timeout all frames in the send window when a frame times out:

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

This is a helper function I wrote to separate the responsibility of actually finding and timing out the frames in the send window for handle_timedout_frames.

### `handle_outgoing_frames` in `sender.c`

This function was updated to comply to congestion control rules so that outgoing frames respect the cc boundaries.

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

Only after all timed-out frames have been retransmitted does the sender consider new frames from `buffered_outframes_head`. The logic is similar to Project 1a.
