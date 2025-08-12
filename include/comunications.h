#ifndef COMUNICATIONS_H
#define COMUNICATIONS_H

#include <sys/types.h>
#include <msg_queue.h>

#define KEY_TICKET_MSG "/msg/ticket"
#define PROJ_ID  'P'

enum MESSAGE_TYPES {
    MSG_TYPE_TICKET_REQUEST,
    MSG_TYPE_TICKET_RESPONSE,
    MSG_TYPE_SERVICE_REQUEST,
    MSG_TYPE_SERVICE_DONE
};

struct S_ticket_request {
    pid_t sender_pid;
    int service_id;
};

struct S_ticket_response {
    pid_t generator_pid;
    int ticket_number;
};

#endif
