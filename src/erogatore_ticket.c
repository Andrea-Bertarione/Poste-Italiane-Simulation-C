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
    int open_shm[] = {};
    int open_shm_index = 0;

    bool running = true;

    key_t key = ftok(KEY_TICKET_MSG, PROJ_ID);
    if (key == -1) { perror("ftok"); return 1; }
    mq_id qid = mq_open(key, 0, 0666);

    ticket_queue *shared_ticket = (ticket_queue*) init_shared_memory(SHM_TICKET_NAME, SHM_TICKETS_SIZE, open_shm, &open_shm_index);
    memset(shared_ticket, 0, SHM_TICKETS_SIZE);

    int sem_ticket_result = sem_init(&shared_ticket->ticket_lock, 1, 1);
    if (sem_ticket_result < 0) {
        perror("sem_init");
        exit(EXIT_FAILURE);
    }

    srand(time(NULL));

    printf("\033[32m[EROGATORE TICKET]:\033[0m Ticket generator running on queue %d\n", qid);
    while(running) {
        ticket_request req;
        ssize_t n = mq_receive(qid,
                               MSG_TYPE_TICKET_REQUEST,
                               &req,
                               sizeof(req),
                               0);
        if (n < 0) {
            if (errno == EINTR) continue;  // interrupted by signal
            perror("msgrcv");
            break;
        }

        printf("\033[32m[EROGATORE TICKET]:\033[0m Received request from PID %d for service %s\n",
               req.sender_pid, services[req.service_id]);

        //Create new ticket lock from start
        // THIS IS IMPLEMENTATION IS BAREBONES
        // if it needed to scale i would use a Hash map instead of a randomly indexed list
        sem_wait(&shared_ticket->ticket_lock);

        ticket new_ticket;
        new_ticket.service = req.service_id;
        new_ticket.ticket_number = shared_ticket->ticket_counter;
        new_ticket.user = req.sender_pid;
        new_ticket.in_use = 1;

        int ticket_index = rand() % QUEUE_SIZE;
        while (shared_ticket->tickets_queue[ticket_index].in_use) {
            //refresh index

            ticket_index = rand() % QUEUE_SIZE;
        }

        shared_ticket->tickets_queue[ticket_index] = new_ticket;

        shared_ticket->ticket_counter++;

        sem_post(&shared_ticket->ticket_lock);
        
        ticket_response resp;
        resp.generator_pid  = getpid();
        resp.ticket_number  = new_ticket.ticket_number;

        if (mq_send(qid,
            req.sender_pid,          // mtype == client PID
            &resp,
            sizeof(resp)) < 0) {
            perror("mq_send response");
        }
        
    }


    cleanup_shared_memory(SHM_TICKET_NAME, SHM_TICKETS_SIZE, open_shm[0], shared_ticket);
    mq_close(qid);

    return 0;
}
#endif  // UNIT_TEST