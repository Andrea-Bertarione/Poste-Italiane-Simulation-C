#define _POSIX_C_SOURCE 199309L

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include <string.h>
#include <comunications.h>
#include <poste.h>
#include <shared_mem.h>
#include <utente.h>

// TYPES
typedef struct S_ticket_request    ticket_request;
typedef struct S_ticket_response   ticket_response;
typedef struct S_poste_stats       poste_stats;
typedef struct S_daily_stats       daily_stats;

// Send a ticket request returns 0 on failure and 1 on success
int send_ticket_request(mq_id qid, int service) {
    ticket_request req;
    req.sender_pid = getpid();
    req.service_id = service;

    printf("UTENTE[%d]: Sending ticket request (service_id=%d)\n",
        getpid(), req.service_id);
    fflush(stdout);

    if (mq_send(qid,
                MSG_TYPE_TICKET_REQUEST,
                &req,
                sizeof(req)) < 0) {
        fprintf(stderr, "UTENTE[%d] ERROR mq_send request: %s\n",
                getpid(), strerror(errno));
        fflush(stderr);

        return 0;
    }

    return 1;
}

// wait for ticket response returns -1 on error, 0 on failure and 1 on success
ticket_response await_ticket_response(mq_id qid) {
    ticket_response res;
    ssize_t n = mq_receive(qid,
                               getpid(),
                               &res,
                               sizeof(res),
                               0);
    if (n < 0) {
        if (errno == EINTR) {
            printf("UTENTE[%d]: mq_receive interrupted, retrying\n", getpid());
            fflush(stdout);
            res.ticket_number = -1;
        }
        fprintf(stderr, "UTENTE[%d] ERROR mq_receive: %s\n",
                getpid(), strerror(errno));
        fflush(stderr);
        res.ticket_number = -1;
    }

    printf("UTENTE[%d]: Received response, ticket_number=%d\n",
           getpid(), res.ticket_number);
    fflush(stdout);

    return res;
}

// Function that decides if to go to the poste or no
bool poste_decision() {
    int p_serv = rand() % P_SERV_MAX;

    if (p_serv >= P_SERV_MIN) {
        return true;
    }

    return false;
}

// generate the list of services to be done for the day, returns the amount of services to be done and edit the list
int generate_service_list(int list[MAX_N_REQUESTS]) {
    int max = (rand() % MAX_N_REQUESTS) + 1;

    for (int i = 0; i < max; i++) {
        list[i] = rand() % NUM_SERVICE_TYPES;
    }

    return max;
}

int generate_walk_in_time(int num_requests) {
    unsigned int max = (WORKER_SHIFT_CLOSE - (0.4 * num_requests)) - WORKER_SHIFT_OPEN;

    int hour = (rand() % max) + WORKER_SHIFT_OPEN;

    return (hour * 60) + (rand() % 60) + 1;
}

// Main implementation
#ifndef UNIT_TEST
int main() {
    int open_shm[] = {};
    int open_shm_index = 0;

    key_t key = ftok(KEY_TICKET_MSG, PROJ_ID);
    if (key == -1) {
        fprintf(stderr, "UTENTE[%d] ERROR ftok: %s\n", getpid(), strerror(errno));
        fflush(stderr);
        return 1;
    }
    mq_id qid = mq_open(key, 0, 0666);
    if (qid < 0) {
        fprintf(stderr, "UTENTE[%d] ERROR mq_open: %s\n", getpid(), strerror(errno));
        fflush(stderr);
        exit(1);
    }

    struct timespec t1, t2;
    t1.tv_sec = 0;
    t1.tv_nsec = N_NANO_SECS * 5; //avoid spamming too much (check every 5 minutes simulation time)

    poste_stats *shared_stats = (poste_stats *) init_shared_memory(
        SHM_STATS_NAME, SHM_STATS_SIZE, open_shm, &open_shm_index);

    srand((unsigned)getpid());

    sem_wait(&shared_stats->day_update_event);

    while (true) {
        printf("UTENTE[%d]: Starting work for the day\n", getpid());
        fflush(stdout);

        // Avoid weird timing errors
        sleep(1);

        if (poste_decision()) {
            int list[MAX_N_REQUESTS] = {};
            int n_requests = generate_service_list(list);

            //Generate exact minute of walk in
            int walk_in_time = generate_walk_in_time(n_requests);

            while (shared_stats->current_minute < walk_in_time) {
                int sleep_nano_res = nanosleep(&t1, &t2);
                if (sleep_nano_res != 0) {
                    printf("Sleep was interrupted.\n");
                    exit(EXIT_FAILURE);
                }

                //printf("current time: %d\n", shared_stats->current_minute);
            }

            printf("UTENTE[%d]: [%d:%d] Going to the Poste\n", getpid(), (walk_in_time % 24) + 1, (int) walk_in_time / 24);

            for (int i = 0; i < n_requests; i++) {
                sem_wait(&shared_stats->stats_lock);
                if (shared_stats->current_minute > WORKER_SHIFT_CLOSE) {
                    shared_stats->simulation_global.failed_services++;
                    shared_stats->simulation_services[list[i]].failed_services++;
                    shared_stats->today.global.failed_services++;
                    shared_stats->today.services[list[i]].failed_services++;
                    
                    sem_post(&shared_stats->stats_lock);
                    continue;
                }
                sem_post(&shared_stats->stats_lock);

                if (send_ticket_request(qid, list[i]) == 0) { continue; }

                ticket_response res = await_ticket_response(qid);
                if (res.ticket_number == -1) { continue; }

                // TO-DO Logic for actually doing the work after implementing workers
            } 
            
        }
        else {
            printf("UTENTE[%d]: Decided to not go to Poste today\n", getpid());
        }

        printf("UTENTE[%d]: Waiting for next day signal\n", getpid());
        fflush(stdout);
        sem_wait(&shared_stats->day_update_event);  // await new day
        printf("UTENTE[%d]: Next day arrived\n", getpid());
        fflush(stdout);

        // Delay to not clutter the terminal
        sleep(1);
    }

    return EXIT_SUCCESS;
}
#endif  // UNIT_TEST
