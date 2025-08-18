#ifndef COMUNICATIONS_H
#define COMUNICATIONS_H

#include <sys/types.h>
#include <msg_queue.h>

#define KEY_TICKET_MSG "./msg/ticket"
#define PROJ_ID  'P'

enum MESSAGE_TYPES {
    MSG_TYPE_TICKET_REQUEST = 1,
    MSG_TYPE_TICKET_RESPONSE,
    MSG_TYPE_SERVICE_REQUEST,
    MSG_TYPE_SERVICE_DONE
};

#define MSG_TYPE_TICKET_REQUEST_MULT * 10000

struct S_ticket_request {
    pid_t sender_pid;
    int service_id;
};

struct S_ticket_response {
    pid_t generator_pid;
    int ticket_number;
};

struct S_service_request {
    pid_t sender_pid;
    int ticket_number;
};

struct S_service_done {
    pid_t sender_pid;
    int ticket_number;
    int service_id;
    double service_time; // in seconds
};

#endif
