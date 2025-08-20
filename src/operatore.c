#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <semaphore.h>

#include <comunications.h>
#include <shared_mem.h>
#include <poste.h>

// TYPES
typedef struct S_poste_stats       poste_stats;
typedef struct S_daily_stats       daily_stats;
typedef struct S_service_stats     service_stats;
typedef struct S_poste_stations    poste_stations;
typedef struct S_worker_seat       worker_seat;
typedef struct S_service_request   service_request;
typedef struct S_service_done      service_done;

#define PREFIX "\033[34m[OPERATORE(%d)]:\033[0m"

// Centralized function to release a seat and signal waiting operators
void release_seat(poste_stations *shared_stations, int seat_index) {
    sem_wait(&shared_stations->stations_lock);
    shared_stations->NOF_WORKER_SEATS[seat_index].operator_status = FREE;
    shared_stations->NOF_WORKER_SEATS[seat_index].operator_process = 0;
    sem_post(&shared_stations->stations_lock);
    sem_post(&shared_stations->stations_event);
    
    printf(PREFIX " Released seat %d\n", getpid(), seat_index);
    fflush(stdout);
}

// function to handle operators taking seats
void take_seat(poste_stations *shared_stations, int i) {
    shared_stations->NOF_WORKER_SEATS[i].operator_process = getpid();
    shared_stations->NOF_WORKER_SEATS[i].operator_status = OCCUPIED;

    printf(PREFIX " Taking seat %d for service %s\n",
           getpid(), i, services[shared_stations->NOF_WORKER_SEATS[i].service_id]);
    fflush(stdout);
}

// await service request from users - non blocking version
service_request await_service_request_nb(mq_id qid) {
    service_request res;
    ssize_t n = mq_receive(qid,
                           (MSG_TYPE_SERVICE_REQUEST MSG_TYPE_TICKET_REQUEST_MULT) + getpid(),
                           &res,
                           sizeof(res),
                           IPC_NOWAIT);
    if (n < 0) {
        if (errno == ENOMSG) {
            res.ticket_number = -2; // No message available
            return res;
        } else if (errno == EINTR) {
            printf(PREFIX " mq_receive interrupted, retrying\n", getpid());
            fflush(stdout);
            res.ticket_number = -1;
        } else {
            fprintf(stderr, PREFIX " ERROR mq_receive: %s\n", getpid(), strerror(errno));
            fflush(stderr);
            res.ticket_number = -1;
        }
    } else {
        printf(PREFIX " Received response, ticket_number=%d\n", getpid(), res.ticket_number);
        fflush(stdout);
    }

    return res;
}

// send service done returns 0 on failure and 1 on success
int send_service_done(mq_id qid, int ticket_number, pid_t user_pid, int service_id, double service_time) {
    service_done req;
    req.sender_pid = getpid();
    req.ticket_number = ticket_number;
    req.service_time = service_time;
    req.service_id = service_id;

    printf(PREFIX " Sending service done message for ticket %d\n", getpid(), ticket_number);
    fflush(stdout);

    if (mq_send(qid,
                user_pid,
                &req,
                sizeof(req)) < 0) {
        fprintf(stderr, PREFIX " ERROR mq_send service request: %s\n", getpid(), strerror(errno));
        fflush(stderr);
        return 0;
    }
    return 1;
}

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

    srand((unsigned)getpid());

    sem_wait(&shared_stats->day_update_event);

    while (true) {
        printf(PREFIX " Starting work for the day\n", getpid());
        fflush(stdout);
        sleep(1);

        bool shift_ended = false;  // Flag to prevent multiple semaphore consumption
        bool worked_today = false; // Flag to track if the operator worked today

        // Wait for the poste to open
        sem_wait(&shared_stats->open_poste_event);

        printf(PREFIX " Entering the poste at %02d:%02d\n",
               getpid(),
               shared_stats->current_minute / 60,
               shared_stats->current_minute % 60);

        // Work day loop - continue until shift ends
        while (!shift_ended) {
            // Check if shift ended before doing anything else
            if (sem_trywait(&shared_stats->close_poste_event) == 0) {
                shift_ended = true;
                printf(PREFIX " [%02d:%02d] Shift ended, going home\n",
                       getpid(),
                       shared_stats->current_minute / 60,
                       shared_stats->current_minute % 60);
                fflush(stdout);
                break;
            }

            bool seated_flag = false;
            int current_seat = -1;

            // Try to find ANY available seat (flexible operators)
            sem_wait(&shared_stations->stations_lock);
            for (int i = 0; i < g_config.num_worker_seats; i++) {
                if (shared_stations->NOF_WORKER_SEATS[i].operator_status == FREE) {
                    take_seat(shared_stations, i);
                    worked_today = true;
                    seated_flag = true;
                    current_seat = i;
                    break;
                }
            }
            sem_post(&shared_stations->stations_lock);

            if (!seated_flag) {
                printf(PREFIX " No available seats, waiting for a place to open up\n", getpid());
                fflush(stdout);

                // Use timed wait to prevent indefinite blocking
                struct timespec timeout;
                clock_gettime(CLOCK_REALTIME, &timeout);
                timeout.tv_sec += 2; // Wait max 2 seconds
                
                if (sem_timedwait(&shared_stations->stations_event, &timeout) != 0) {
                    if (errno != ETIMEDOUT && errno != EINTR) {
                        fprintf(stderr, PREFIX " sem_timedwait error: %s\n", getpid(), strerror(errno));
                    }
                }
                continue;
            }

            worker_seat seat_struct = shared_stations->NOF_WORKER_SEATS[current_seat];

            // Working loop - serve customers until break or shift end
            while (seated_flag && !shift_ended) {
                // Check for shift end while working
                if (sem_trywait(&shared_stats->close_poste_event) == 0) {
                    shift_ended = true;
                    printf(PREFIX " [%02d:%02d] Received shift end signal while working, cleaning up\n",
                           getpid(),
                           shared_stats->current_minute / 60,
                           shared_stats->current_minute % 60);
                    fflush(stdout);

                    release_seat(shared_stations, current_seat);
                    seated_flag = false;
                    break;
                }

                service_request service_req = await_service_request_nb(qid);

                if (service_req.ticket_number == -2) {
                    // No message available, short sleep and continue
                    struct timespec short_sleep = { .tv_sec = 0, .tv_nsec = g_config.minute_duration };
                    nanosleep(&short_sleep, NULL);
                    continue;
                }

                if (service_req.ticket_number == -1) {
                    printf(PREFIX " Service request interrupted, retrying\n", getpid());
                    fflush(stdout);
                    continue;
                }

                printf(PREFIX " [%02d:%02d] Received service request for ticket %d\n",
                       getpid(),
                       shared_stats->current_minute / 60,
                       shared_stats->current_minute % 60,
                       service_req.ticket_number);
                fflush(stdout);

                // Simulate service time
                int nominal = services_duration[seat_struct.service_id];
                printf(PREFIX " [%02d:%02d] Starting service for ticket %d, service time: around %d minutes\n",
                       getpid(),
                       shared_stats->current_minute / 60,
                       shared_stats->current_minute % 60,
                       service_req.ticket_number, nominal);
                fflush(stdout);

                long long base_nano = (long long)nominal * g_config.minute_duration;
                long long min_nano  = base_nano / 2;
                long long max_nano  = base_nano + base_nano / 2;
                long long span      = max_nano - min_nano + 1;
                long long rand_nano = min_nano + (rand() % span);

                // Handle sleep duration properly
                struct timespec service_time;
                if (rand_nano >= 1000000000LL) {
                    service_time.tv_sec  = rand_nano / 1000000000LL;
                    service_time.tv_nsec = rand_nano % 1000000000LL;
                } else {
                    service_time.tv_sec = 0;
                    service_time.tv_nsec = rand_nano;
                }

                struct timespec remaining;
                int ret;
                do {
                    errno = 0;
                    ret = nanosleep(&service_time, &remaining);
                    if (ret == -1 && errno == EINTR) {
                        service_time = remaining;
                    }
                } while (ret == -1 && errno == EINTR);

                if (ret == -1) {
                    fprintf(stderr, PREFIX " nanosleep failed: %s\n", getpid(), strerror(errno));
                    exit(EXIT_FAILURE);
                }

                printf(PREFIX " [%02d:%02d] Finished service for ticket %d, service time: %lld nanoseconds\n",
                       getpid(),
                       shared_stats->current_minute / 60,
                       shared_stats->current_minute % 60,
                       service_req.ticket_number, rand_nano);

                // Send service done message
                if (!send_service_done(qid, service_req.ticket_number, service_req.sender_pid,
                                       seat_struct.service_id, (double)rand_nano / g_config.minute_duration)) {
                    printf(PREFIX " Failed to send service done message for ticket %d\n",
                           getpid(), service_req.ticket_number);
                    fflush(stdout);
                    continue;
                }

                // Update statistics
                sem_wait(&shared_stats->stats_lock);
                shared_stats->simulation_services[seat_struct.service_id].served_users++;
                shared_stats->simulation_services[seat_struct.service_id].total_requests++;
                shared_stats->simulation_services[seat_struct.service_id].total_service_time += (double)rand_nano / g_config.minute_duration;
                shared_stats->simulation_global.served_users++;
                shared_stats->simulation_global.total_requests++;
                shared_stats->simulation_global.total_service_time += (double)rand_nano / g_config.minute_duration;
                shared_stats->today.services[seat_struct.service_id].served_users++;
                shared_stats->today.services[seat_struct.service_id].total_requests++;
                shared_stats->today.services[seat_struct.service_id].total_service_time += (double)rand_nano / g_config.minute_duration;
                shared_stats->today.global.served_users++;
                shared_stats->today.global.total_requests++;
                shared_stats->today.global.total_service_time += (double)rand_nano / g_config.minute_duration;
                sem_post(&shared_stats->stats_lock);

                // Randomly decide to take a break (reduced probability to avoid too many breaks)
                if (rand() % 100 < 10) {  // Reduced from 20% to 10%
                    printf(PREFIX " [%02d:%02d] Taking a break\n",
                           getpid(),
                           shared_stats->current_minute / 60,
                           shared_stats->current_minute % 60);
                    fflush(stdout);

                    release_seat(shared_stations, current_seat);

                    // update break number stats
                    sem_wait(&shared_stats->stats_lock);
                    shared_stats->total_simulation_pauses++;
                    shared_stats->today.total_pauses++;
                    sem_post(&shared_stats->stats_lock);

                    // Take break
                    struct timespec break_time = { .tv_sec = 0, .tv_nsec = g_config.minute_duration * ((rand() % 20) + 10) };
                    nanosleep(&break_time, NULL);

                    seated_flag = false;
                    break;
                }
            }
        }

        // update stats if the operator worked today
        if (worked_today) {
            sem_wait(&shared_stats->stats_lock);
            shared_stats->total_active_operators++;
            shared_stats->today.active_operators++;
            sem_post(&shared_stats->stats_lock);
        } else {
            printf(PREFIX " [%02d:%02d] Did not work today, no customers served\n",
                   getpid(),
                   shared_stats->current_minute / 60,
                   shared_stats->current_minute % 60);
            fflush(stdout);
        }

        printf(PREFIX " Waiting for next day signal\n", getpid());
        fflush(stdout);
        sem_wait(&shared_stats->day_update_event);
        printf(PREFIX " Next day signal received\n", getpid());
        fflush(stdout);
        sleep(1);
    }

    return 0;
}
