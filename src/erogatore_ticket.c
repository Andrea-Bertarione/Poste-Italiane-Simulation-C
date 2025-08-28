#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <erogatore_ticket.h>
#include <comunications.h>
#include <shared_mem.h>
#include <stdbool.h>
#include <errno.h>
#include <sys/wait.h>
#include <time.h>
#include <string.h>

#include <poste.h>

// Types
typedef struct S_ticket_queue ticket_queue;
typedef struct S_ticket ticket;
typedef enum SERVICE_ID service_id;
typedef struct S_ticket_request ticket_request;
typedef struct S_ticket_response ticket_response;

#ifndef UNIT_TEST
int main() {
    bool running = true;

    key_t key = ftok(KEY_TICKET_MSG, PROJ_ID);
    if (key == -1) { perror("ftok"); return 1; }
    mq_id qid = mq_open(key, 0, 0666);
    srand(time(NULL));

    int ticket_counter = 0;

    printf("\e[1;33m[EROGATORE TICKET]:\033[0m Ticket generator running on queue %d\n", qid);
    while(running) {
        ticket_request req;
        ssize_t n = mq_receive(qid, MSG_TYPE_TICKET_REQUEST, &req, sizeof(req), 0);
        if (n < 0) {
            if (errno == EINTR) continue;  // interrupted by signal
            perror("msgrcv");
            break;
        }

        printf("\e[1;33m[EROGATORE TICKET]:\033[0m Received request from PID %d for service %s\n", req.sender_pid, services[req.service_id]);
        
        ticket_response resp;
        resp.generator_pid  = getpid();
        resp.ticket_number  = ticket_counter;

        if (mq_send(qid, req.sender_pid, &resp, sizeof(resp)) < 0) {
            perror("mq_send response");
        }
        
        ticket_counter++;
    }

    return 0;
}
#endif  // UNIT_TEST