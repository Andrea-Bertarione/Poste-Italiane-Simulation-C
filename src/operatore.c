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

// Function that updates requests statistics
void update_requests_stats(poste_stats *shared_stats, int user_service) {
    sem_wait(&shared_stats->stats_lock);
    shared_stats->simulation_services[user_service].total_requests++;
    shared_stats->simulation_global.total_requests++;
    shared_stats->today.services[user_service].total_requests++;
    shared_stats->today.global.total_requests++;
    sem_post(&shared_stats->stats_lock);
}


// Function that updates pause statistics
void update_pause_stats(poste_stats *shared_stats) {
    sem_wait(&shared_stats->stats_lock);
    shared_stats->total_simulation_pauses++;
    shared_stats->today.total_pauses++;
    sem_post(&shared_stats->stats_lock);
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

// Function that Yield and wait for the simulated process to end
long long process_service(poste_stats *shared_stats, service_request service_req, int user_service) {
    int nominal = services_duration[user_service];
                
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

    printf(PREFIX " [%02d:%02d] Finished service for ticket %d, service time: %lld minutes\n",
        getpid(),
        shared_stats->current_minute / 60,
        shared_stats->current_minute % 60,
        service_req.ticket_number, rand_nano / g_config.minute_duration);

    return rand_nano;
}

// Function that finds a free seat for a specific service
int find_seat(poste_stations *shared_stations, int user_service) {
    for (int i = 0; i < g_config.num_worker_seats; i++) {
        if (shared_stations->NOF_WORKER_SEATS[i].operator_status == FREE && shared_stations->NOF_WORKER_SEATS[i].service_id == user_service)  {
            return i;
        }
    }
    return -1;
}

// Function that checks if today the operator can work, searching if any stations are of his proficency
bool can_work_today(poste_stations *shared_stations, int user_service) {
    for (int i = 0; i < g_config.num_worker_seats; i++) {
        if (shared_stations->NOF_WORKER_SEATS[i].service_id == user_service) {
            return true;
        }
    }
    return false;
}

//check if the poste is closed
bool did_poste_close(poste_stats *shared_stats) {
    return sem_trywait(&shared_stats->close_poste_event) == 0;
}

// Function that handles the waiting for a station, return station index or -1 if the day finished while searching a seat
int wait_for_station(poste_stats *shared_stats, poste_stations *shared_stations, int user_service) {
    // Wait for a free station, doesn't complettly yield and checks for poste closed to make sure the operator doesn't get stuck
    struct timespec timeout;
    timeout.tv_nsec = g_config.minute_duration * 5; // Check every 5 minutes if poste is closed
    timeout.tv_sec = 0;
    while (true) {
        while (sem_timedwait(&shared_stations->stations_event, &timeout) != 0) {
            if (did_poste_close(shared_stats)) {
                // Poste closed while waiting
                return -1;
            }
        }

        int available_seat = find_seat(shared_stations, user_service);
        if (available_seat != -1) {
            return available_seat;
        }

        sem_post(&shared_stations->stations_event); // No seat found, re-signal and wait again
    }
}

// main work loop, return false if the operator did not work and return true if it did
bool work_loop(poste_stats *shared_stats, poste_stations *shared_stations, mq_id qid, int user_service, int *pauses_done) {
    if (!can_work_today(shared_stations, user_service)) {
        printf(PREFIX " [%02d:%02d] Operator cannot work today, going home\n",
               getpid(),
               shared_stats->current_minute / 60,
               shared_stats->current_minute % 60);
        fflush(stdout);
        return false;
    }

    bool on_shift = true;
    int current_seat = -1;

    // Search for a free station
    sem_wait(&shared_stations->stations_lock);
    int available_seat = find_seat(shared_stations, user_service);
    if (available_seat != -1) {
        take_seat(shared_stations, available_seat);
        current_seat = available_seat;

        sem_post(&shared_stations->stations_lock);
    }
    else {
        sem_post(&shared_stations->stations_lock);

        available_seat = wait_for_station(shared_stats, shared_stations, user_service);
        if (available_seat == -1) {
            // Day ended while waiting
            return false;
        }

        sem_wait(&shared_stations->stations_lock);

        take_seat(shared_stations, available_seat);
        current_seat = available_seat;

        sem_post(&shared_stations->stations_lock);
    }

    // Each iteration is a ticket being solved and worked on
    while (on_shift) {
        // Check if the poste closed while working on the previous ticket
        if (did_poste_close(shared_stats)) {
            on_shift = false;
            release_seat(shared_stations, current_seat);
            
            printf(PREFIX " [%02d:%02d] Shift ended, going home\n",
                    getpid(),
                    shared_stats->current_minute / 60,
                    shared_stats->current_minute % 60);
            fflush(stdout);

            continue;
        }

        // Non blocking check for service requests, im using a non blocking version to make sure the operator doesn't get stuck
        service_request service_req = await_service_request_nb(qid);
        if (service_req.ticket_number == -2) {
            // No message available, short sleep ( 3 simulation minutes ) and continue
            struct timespec short_sleep = { .tv_sec = 0, .tv_nsec = g_config.minute_duration * 3 };
            nanosleep(&short_sleep, NULL);
            continue;
        }

        if (service_req.ticket_number == -1) {
            printf(PREFIX " Service request interrupted, retrying\n", getpid());
            fflush(stdout);
            continue;
        }

        // Service request received, update statistics
        update_requests_stats(shared_stats, user_service);

        printf(PREFIX " [%02d:%02d] Received service request for ticket %d\n",
                       getpid(),
                       shared_stats->current_minute / 60,
                       shared_stats->current_minute % 60,
                       service_req.ticket_number);
        fflush(stdout);

        // Handle the service
        long long time_taken = process_service(shared_stats, service_req, user_service);

        // Send back the response
        if (!send_service_done(qid, service_req.ticket_number, service_req.sender_pid, user_service, (double)time_taken / g_config.minute_duration)) {
            printf(PREFIX " Failed to send service done message for ticket %d\n",
                getpid(), service_req.ticket_number);
            fflush(stdout);
        }

        // Should i go home early?
        if (rand() % 100 < 10 && (*pauses_done) < g_config.nof_pause) {
            // 10% chance to go home early if i still have pauses left
            on_shift = false;
            (*pauses_done)++;
            release_seat(shared_stations, current_seat);

            update_pause_stats(shared_stats);

            printf(PREFIX " [%02d:%02d] Finished work early for the day, going home\n",
                   getpid(),
                   shared_stats->current_minute / 60,
                   shared_stats->current_minute % 60);
            fflush(stdout);

        }
    }

    return true;
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

    int pauses_done = 0;
    int user_service = rand() % NUM_SERVICE_TYPES; // Choose a service for the operator on creation
    printf(PREFIX " Assigned service %s to operator\n", getpid(), services[user_service]);
    fflush(stdout);

    while (true) {
        printf(PREFIX " Starting work for the day\n", getpid());
        fflush(stdout);
        sleep(1);

        // Wait for the poste to open
        sem_wait(&shared_stats->open_poste_event);

        printf(PREFIX " Entering the poste at %02d:%02d\n",
               getpid(),
               shared_stats->current_minute / 60,
               shared_stats->current_minute % 60);
        fflush(stdout);

       bool worked_today = work_loop(shared_stats, shared_stations, qid, user_service, &pauses_done);

        // update stats if the operator worked today
        if (worked_today) {
            sem_wait(&shared_stats->stats_lock);
            shared_stats->total_active_operators++;
            shared_stats->today.active_operators++;
            sem_post(&shared_stats->stats_lock);
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