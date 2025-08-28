#ifndef COMUNICATIONS_H
#define COMUNICATIONS_H

#include <sys/types.h>
#include <msg_queue.h>

#define KEY_TICKET_MSG "./msg/ticket"
#define KEY_NEW_USERS "./msg/new_users"
#define PROJ_ID  'P'
#define proj_ID_USERS 'Z'

enum MESSAGE_TYPES {
    MSG_TYPE_TICKET_REQUEST = 1,
    MSG_TYPE_TICKET_RESPONSE,
    MSG_TYPE_SERVICE_REQUEST,
    MSG_TYPE_SERVICE_DONE
};

#define MSG_TYPE_TICKET_REQUEST_MULT * 10000
#define MSG_TYPE_ADD_USERS_REQUEST 10

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
    double service_time; 
};

struct S_new_users_request {
    pid_t sender_pid;
    int N_NEW_USERS;
};

struct S_new_users_done {
    int status; // 0 -> error - 1 -> success
};

#endif
