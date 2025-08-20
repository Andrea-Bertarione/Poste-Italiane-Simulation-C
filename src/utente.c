#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include <string.h>
#include <comunications.h>
#include <poste.h>
#include <shared_mem.h>

// TYPES
typedef struct S_ticket_request    ticket_request;
typedef struct S_ticket_response   ticket_response;
typedef struct S_service_request   service_request;
typedef struct S_service_done      service_done;
typedef struct S_poste_stats       poste_stats;
typedef struct S_daily_stats       daily_stats;
typedef struct S_poste_stations    poste_stations;
typedef struct S_worker_seat       worker_seat;

#define PREFIX "\033[32m[UTENTE(%d)]:\033[0m"

// Send a ticket request returns 0 on failure and 1 on success
int send_ticket_request(mq_id qid, int service) {
    ticket_request req;
    req.sender_pid = getpid();
    req.service_id = service;

    printf(PREFIX " Sending ticket request (service_id=%d)\n", getpid(), req.service_id);
    fflush(stdout);

    if (mq_send(qid,
                MSG_TYPE_TICKET_REQUEST,
                &req,
                sizeof(req)) < 0) {
        fprintf(stderr, PREFIX " ERROR mq_send request: %s\n", getpid(), strerror(errno));
        fflush(stderr);
        return 0;
    }
    return 1;
}

// Wait for ticket response
ticket_response await_ticket_response(mq_id qid) {
    ticket_response res;
    ssize_t n = mq_receive(qid,
                           getpid(),
                           &res,
                           sizeof(res),
                           0);
    if (n < 0) {
        if (errno == EINTR) {
            printf(PREFIX " mq_receive interrupted, retrying\n", getpid());
            fflush(stdout);
            res.ticket_number = -1;
        }
        fprintf(stderr, PREFIX " ERROR mq_receive: %s\n", getpid(), strerror(errno));
        fflush(stderr);
        res.ticket_number = -1;
    }
    printf(PREFIX " Received response, ticket_number=%d\n", getpid(), res.ticket_number);
    fflush(stdout);
    return res;
}

// Send service request returns 0 on failure and 1 on success
int send_service_request(mq_id qid, int ticket_number, pid_t operator_pid) {
    service_request req;
    req.sender_pid = getpid();
    req.ticket_number = ticket_number;

    long mtype = (MSG_TYPE_SERVICE_REQUEST MSG_TYPE_TICKET_REQUEST_MULT) + operator_pid;
    printf(PREFIX " Sending service request for ticket %d to operator %d, msg_type=%ld\n",
           getpid(), ticket_number, operator_pid, mtype);
    fflush(stdout);

    if (mq_send(qid,
                mtype,
                &req,
                sizeof(req)) < 0) {
        fprintf(stderr, PREFIX " ERROR mq_send service request: %s\n", getpid(), strerror(errno));
        fflush(stderr);
        return 0;
    }
    return 1;
}

// Wait for service done
service_done await_service_done(mq_id qid) {
    service_done res;
    ssize_t n = mq_receive(qid,
                           getpid(),
                           &res,
                           sizeof(res),
                           0);
    if (n < 0) {
        if (errno == EINTR) {
            printf(PREFIX " mq_receive interrupted, retrying\n", getpid());
            fflush(stdout);
            res.ticket_number = -1;
        }
        fprintf(stderr, PREFIX " ERROR mq_receive: %s\n", getpid(), strerror(errno));
        fflush(stderr);
        res.ticket_number = -1;
    }
    printf(PREFIX " Received response, ticket_number=%d\n", getpid(), res.ticket_number);
    fflush(stdout);
    return res;
}

// Function that decides whether to go to the poste
bool poste_decision() {
    return (rand() % g_config.p_serv_max) >= g_config.p_serv_min;
}

// Generate the list of services to be done for the day
int generate_service_list(int list[g_config.max_n_requests]) {
    int max = (rand() % g_config.max_n_requests) + 1;
    for (int i = 0; i < max; i++) {
        list[i] = rand() % NUM_SERVICE_TYPES;
    }
    return max;
}

int generate_walk_in_time(int num_requests) {
    unsigned int max = (g_config.worker_shift_close - (0.4 * num_requests)) - g_config.worker_shift_open;
    int hour = (rand() % max) + g_config.worker_shift_open;
    return (hour * 60) + (rand() % 60) + 1;
}

void update_fails_stats(poste_stats *shared_stats, int service_id) {
    sem_wait(&shared_stats->stats_lock);
    shared_stats->simulation_global.failed_services+=1;
    shared_stats->simulation_services[service_id].failed_services+=1;
    shared_stats->today.global.failed_services+=1;
    shared_stats->today.services[service_id].failed_services+=1;
    sem_post(&shared_stats->stats_lock);
}

void update_success_stats(poste_stats *shared_stats, int service_id, double wait_time, double service_time) {
    sem_wait(&shared_stats->stats_lock);
    shared_stats->simulation_global.served_users++;
    shared_stats->simulation_global.total_wait_time += wait_time;
    shared_stats->simulation_global.total_service_time += service_time;
    shared_stats->simulation_services[service_id].served_users++;
    shared_stats->simulation_services[service_id].total_wait_time += wait_time;
    shared_stats->simulation_services[service_id].total_service_time += service_time;
    shared_stats->today.global.served_users++;
    shared_stats->today.services[service_id].served_users++;
    shared_stats->today.services[service_id].total_wait_time += wait_time;
    shared_stats->today.services[service_id].total_service_time += service_time;
    shared_stats->today.global.total_wait_time += wait_time;
    shared_stats->today.global.total_service_time += service_time;
    sem_post(&shared_stats->stats_lock);
}

#ifndef UNIT_TEST
int main() {
    int open_shm[] = {};
    int open_shm_index = 0;

    key_t key = ftok(KEY_TICKET_MSG, PROJ_ID);
    if (key == -1) {
        fprintf(stderr, PREFIX " ERROR ftok: %s\n", getpid(), strerror(errno));
        fflush(stderr);
        return 1;
    }
    mq_id qid = mq_open(key, 0, 0666);
    if (qid < 0) {
        fprintf(stderr, PREFIX " ERROR mq_open: %s\n", getpid(), strerror(errno));
        fflush(stderr);
        exit(EXIT_FAILURE);
    }

    poste_stats *shared_stats = (poste_stats*) init_shared_memory(
        SHM_STATS_NAME, SHM_STATS_SIZE, open_shm, &open_shm_index);

    poste_stations *shared_stations = (poste_stations*) init_shared_memory(
        SHM_STATIONS_NAME, SHM_STATIONS_SIZE, open_shm, &open_shm_index);
    
    load_config(shared_stats->configuration_file);

    struct timespec t1 = { .tv_sec = 0, .tv_nsec = g_config.minute_duration * 5 };
    struct timespec t2;

    srand((unsigned)getpid());

    sem_wait(&shared_stats->day_update_event);

    while (true) {
        printf(PREFIX " Starting the day\n", getpid());
        fflush(stdout);
        sleep(1);

        bool is_late = false;

        if (poste_decision()) {
            int list[50]; // Assuming max 50 requests (using an extern variable makes it so its a 0 length array)
            int n_requests = generate_service_list(list);
            int walk_in_time = generate_walk_in_time(n_requests);

            // Wait for the poste to open
            sem_wait(&shared_stats->open_poste_event);

            // Busy wait until walk-in time
            while (shared_stats->current_minute < walk_in_time) {
                if (nanosleep(&t1, &t2) != 0) {
                    printf(PREFIX " Sleep was interrupted.\n", getpid());
                    exit(EXIT_FAILURE);
                }
            }

            printf(PREFIX " [%02d:%02d] Going to the Poste\n",
                   getpid(),
                   (walk_in_time / 60), (walk_in_time % 60));

            for (int i = 0; i < n_requests; i++) {
                if (shared_stats->current_minute > (g_config.worker_shift_close * 60)) {
                    update_fails_stats(shared_stats, list[i]);
                    printf(PREFIX " [%02d:%02d] Failed to get service %s, too late\n",
                           getpid(),
                           shared_stats->current_minute / 60,
                           shared_stats->current_minute % 60,
                           services[list[i]]);
                    fflush(stdout);

                    is_late = true;

                    continue;
                }

                bool found_valid_service = false;
                for (int j = 0; j < NUM_SERVICE_TYPES; j++) {
                    if (
                        shared_stations->NOF_WORKER_SEATS[j].service_id == list[i] && 
                        shared_stations->NOF_WORKER_SEATS[j].operator_status == OCCUPIED &&
                        shared_stations->NOF_WORKER_SEATS[j].user_status == FREE &&
                        shared_stations->NOF_WORKER_SEATS[j].operator_process != 0
                    ) {
                        printf(PREFIX " [%02d:%02d] Requesting service %s\n",
                               getpid(),
                               shared_stats->current_minute / 60,
                               shared_stats->current_minute % 60,
                               services[j]);
                        fflush(stdout);
                        found_valid_service = true;
                        break;
                    }
                    else if (
                        shared_stations->NOF_WORKER_SEATS[j].service_id == list[i] &&
                        shared_stations->NOF_WORKER_SEATS[j].operator_status == OCCUPIED &&
                        shared_stations->NOF_WORKER_SEATS[j].operator_process != 0 &&
                        shared_stations->NOF_WORKER_SEATS[j].user_status == OCCUPIED
                    ) {
                        printf(PREFIX " [%02d:%02d] Service %s is available, but someone else is seated, wait in line...\n",
                               getpid(),
                               shared_stats->current_minute / 60,
                               shared_stats->current_minute % 60,
                               services[list[i]]);
                        fflush(stdout);

                        sem_wait(&shared_stations->stations_freed_event);
                        i--;
                        continue;
                    }
                }

                if (!found_valid_service) {
                    update_fails_stats(shared_stats, list[i]);
                    printf(PREFIX " [%02d:%02d] Service %s not available, skipping\n",
                           getpid(),
                           shared_stats->current_minute / 60,
                           shared_stats->current_minute % 60,
                           services[list[i]]);
                    fflush(stdout);
                    continue;
                }

                if (!send_ticket_request(qid, list[i])) {
                    continue;
                }
                ticket_response res = await_ticket_response(qid);
                if (res.ticket_number == -1) {
                    continue;
                }

                // Search for an available operator
                bool found_operator = false;
                int operator_index = -1;
                for (int j = 0; j < NUM_WORKER_SEATS; j++) {
                    if (shared_stations->NOF_WORKER_SEATS[j].user_status == FREE &&
                        shared_stations->NOF_WORKER_SEATS[j].service_id == list[i]) {
                        printf(PREFIX " [%02d:%02d] Found operator for service %s, ticket number %d\n",
                               getpid(),
                               shared_stats->current_minute / 60,
                               shared_stats->current_minute % 60,
                               services[list[i]], res.ticket_number);
                        fflush(stdout);

                        sem_wait(&shared_stations->stations_lock);
                        shared_stations->NOF_WORKER_SEATS[j].user_status = OCCUPIED;
                        sem_post(&shared_stations->stations_lock);

                        found_operator = true;
                        operator_index = j;
                        break;
                    }
                }

                // Wait for a station to become available
                while (!found_operator) {
                    sem_wait(&shared_stations->stations_event);
                    sem_wait(&shared_stations->stations_lock);
                    for (int j = 0; j < NUM_WORKER_SEATS; j++) {
                        if (shared_stations->NOF_WORKER_SEATS[j].user_status == FREE &&
                            shared_stations->NOF_WORKER_SEATS[j].service_id == list[i]) {
                            printf(PREFIX " [%02d:%02d] Found operator for service %s, ticket number %d\n",
                                   getpid(),
                                   shared_stats->current_minute / 60,
                                   shared_stats->current_minute % 60,
                                   services[list[i]], res.ticket_number);
                            fflush(stdout);

                            shared_stations->NOF_WORKER_SEATS[j].user_status = OCCUPIED;

                            found_operator = true;
                            operator_index = j;
                            break;
                        }
                    }
                    sem_post(&shared_stations->stations_lock);
                }

                worker_seat operator_seat = shared_stations->NOF_WORKER_SEATS[operator_index];
                int start_wait = shared_stats->current_minute;

                if (!send_service_request(qid, res.ticket_number, operator_seat.operator_process)) {
                    update_fails_stats(shared_stats, list[i]);
                    continue;
                }

                printf(PREFIX " [%02d:%02d] Sent service request for ticket number %d\n",
                       getpid(),
                       shared_stats->current_minute / 60,
                       shared_stats->current_minute % 60,
                       res.ticket_number);

                service_done done = await_service_done(qid);
                if (done.ticket_number == -1) {
                    update_fails_stats(shared_stats, list[i]);
                    continue;
                }

                int wait_time = shared_stats->current_minute - start_wait;

                printf(PREFIX " [%02d:%02d] Service done for ticket number %d, service time: %.2f seconds\n",
                       getpid(),
                       shared_stats->current_minute / 60,
                       shared_stats->current_minute % 60,
                       done.ticket_number,
                       done.service_time);

                update_success_stats(shared_stats, done.service_id, wait_time, done.service_time);

                sem_wait(&shared_stations->stations_lock);
                shared_stations->NOF_WORKER_SEATS[operator_index].user_status = FREE;
                sem_post(&shared_stations->stations_lock);
                sem_post(&shared_stations->stations_freed_event);
                
            }
        } else {
            printf(PREFIX " Decided not to go to Poste today\n", getpid());
        }

        if (is_late) {
            sem_wait(&shared_stats->stats_lock);
            shared_stats->today.late_users++;
            sem_post(&shared_stats->stats_lock);
        }

        printf(PREFIX " Waiting for next day signal\n", getpid());
        fflush(stdout);
        sem_wait(&shared_stats->day_update_event);
        printf(PREFIX " Next day signal received\n", getpid());
        fflush(stdout);
        sleep(1);
    }

    return EXIT_SUCCESS;
}
#endif // UNIT_TEST
