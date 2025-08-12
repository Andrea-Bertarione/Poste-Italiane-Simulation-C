// include/msg_queue.h

#ifndef MSG_QUEUE_H
#define MSG_QUEUE_H

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <stddef.h> 

// Opaque handle
typedef int mq_id;

// Initialize or connect to a message queue.
// Returns queue ID on success, or -1 on error.
mq_id mq_open(key_t key, int flags, int perms);

// Send a message of type `mtype` with `data` buffer of `length` bytes.
// Returns 0 on success, or -1 on error.
int mq_send(mq_id msqid, long mtype, const void *data, size_t length);

// Receive a message of exactly `length` bytes of type `mtype` (or 0 for any).
// On success, writes into `buffer` and returns number of bytes received.
// Returns -1 on error.
ssize_t mq_receive(mq_id msqid, long mtype, void *buffer, size_t length, int flags);

// Remove (destroy) the message queue identified by `msqid`.
// Returns 0 on success, or -1 on error.
int mq_close(mq_id msqid);

#endif
