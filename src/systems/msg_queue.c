#include "msg_queue.h"
#include <errno.h>
#include <string.h>


mq_id mq_open(key_t key, int flags, int perms) {
    return msgget(key, flags | IPC_CREAT | perms);
}

int mq_send(mq_id msqid, long mtype, const void *data, size_t length) {
    struct {
        long mtype;
        char mtext[length];
    } msg;
    msg.mtype = mtype;
    memcpy(msg.mtext, data, length);
    return msgsnd(msqid, &msg, length, 0);
}

ssize_t mq_receive(mq_id msqid, long mtype, void *buffer, size_t length, int flags) {
    struct {
        long mtype;
        char mtext[length];
    } msg;
    ssize_t ret = msgrcv(msqid, &msg, length, mtype, flags);
    if (ret >= 0) {
        memcpy(buffer, msg.mtext, ret);
    }
    return ret;
}

int mq_close(mq_id msqid) {
    return msgctl(msqid, IPC_RMID, NULL);
}
