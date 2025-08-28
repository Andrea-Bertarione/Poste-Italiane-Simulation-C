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

static bool been_late_today = false;

// Send a ticket request returns 0 on failure and 1 on success
int send_ticket_request(mq_id qid, int service) {
    ticket_request req;
    req.sender_pid = getpid();
    req.service_id = service;

    printf(PREFIX " Sending ticket request (service_id=%d)\n", getpid(), req.service_id);
    fflush(stdout);

    if (mq_send(qid, MSG_TYPE_TICKET_REQUEST, &req, sizeof(req)) < 0) {
        fprintf(stderr, PREFIX " ERROR mq_send request: %s\n", getpid(), strerror(errno));
        fflush(stderr);
        return 0;
    }
    return 1;
}

// Wait for ticket response
ticket_response await_ticket_response(mq_id qid) {
    ticket_response res;
    ssize_t n = mq_receive(qid, getpid(), &res, sizeof(res), 0);

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
    printf(PREFIX " Sending service request for ticket %d to operator %d, msg_type=%ld\n", getpid(), ticket_number, operator_pid, mtype);
    fflush(stdout);

    if (mq_send(qid, mtype, &req, sizeof(req)) < 0) {
        fprintf(stderr, PREFIX " ERROR mq_send service request: %s\n", getpid(), strerror(errno));
        fflush(stderr);
        return 0;
    }
    return 1;
}

// Wait for service done
service_done await_service_done(mq_id qid) {
    service_done res;
    ssize_t n = mq_receive(qid, getpid(), &res, sizeof(res), 0);
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
bool will_go_to_poste(void) {
    return (rand() % g_config.p_serv_max) >= g_config.p_serv_min;
}

// Generate the list of services to be done for the day
int generate_service_list(int list[MAX_N_REQUESTS_COMPILE]) {
    int max = (rand() % g_config.max_n_requests) + 1;
    for (int i = 0; i < max; i++) {
        list[i] = rand() % NUM_SERVICE_TYPES;
    }
    return max;
}

int generate_walk_in_time(int num_requests) {
    int shift_start = g_config.worker_shift_open * 60;     // in minutes
    int shift_end   = g_config.worker_shift_close * 60;    // in minutes

    // Reserve 5 minutes per request on average, or some fixed margin
    int margin = (int)(5.0 * num_requests);  

    int max_time = shift_end - margin - shift_start;
    
    if (max_time < 1) {
        max_time = 1;  // fallback to earliest possible time
    }
    
    int walk_in = shift_start + (rand() % max_time) + 1;
    return walk_in;
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

// Busy-wait until appointed walk-in time
void busy_wait_until_walk_in(int walk_in_time, poste_stats *shared_stats) {
    struct timespec t1 = { .tv_sec = 0, .tv_nsec = g_config.minute_duration * 5 };
    struct timespec t2;
    while (shared_stats->current_minute < walk_in_time) {
        if (nanosleep(&t1, &t2) != 0) {
            printf(PREFIX " Sleep was interrupted.\n", getpid());
            exit(EXIT_FAILURE);
        }
    }
}

// Function that handles users that remains late
void handle_late_users(poste_stats *shared_stats, int service_id) {
    sem_wait(&shared_stats->stats_lock);

    if (been_late_today) {
        sem_post(&shared_stats->stats_lock);
        return;
    }
    else {
        been_late_today = true;
    }

    shared_stats->today.late_users++;
    shared_stats->simulation_global.late_users++;
    shared_stats->simulation_services[service_id].late_users++;
    sem_post(&shared_stats->stats_lock);
    printf(PREFIX " Late user, incrementing late users count\n", getpid());
    fflush(stdout);
}

// Function that search for a operator seat with the correct ID that is occupied
// Returns the number of valid seats found
// and fills the valid_seats array with the indices of those seats
int find_valid_seats(poste_stations *shared_stations, int service_id, int valid_seats[MAX_WORKER_SEATS]) {
    int count = 0;

    sem_wait(&shared_stations->stations_lock);
    for (int i = 0; i < g_config.num_worker_seats; i++) {
        if (shared_stations->NOF_WORKER_SEATS[i].service_id == service_id &&
            shared_stations->NOF_WORKER_SEATS[i].operator_status == OCCUPIED ) {

            valid_seats[count++] = i;
        }
    }
    sem_post(&shared_stations->stations_lock);

    return count;
}

// Function that makes the user attempt to take a seat
// Returns the index of the seat taken, or -1 if no seat was available
int attempt_take_seat(poste_stations *shared_stations, int valid_seats[MAX_WORKER_SEATS], int n_valid_seats) {
    int current_seat = -1;

    sem_wait(&shared_stations->stations_lock);
    for (int i = 0; i < n_valid_seats; i++) {
        if (shared_stations->NOF_WORKER_SEATS[valid_seats[i]].user_status == FREE) {
            current_seat = valid_seats[i];
            shared_stations->NOF_WORKER_SEATS[valid_seats[i]].user_status = OCCUPIED;
            break;
        }
    }
    sem_post(&shared_stations->stations_lock);

    if (current_seat != -1) {
        printf(PREFIX " Took seat %d\n", getpid(), current_seat);
        fflush(stdout);
    }

    return current_seat;
}

// Function that handles the service process
void handle_service(int service_id, mq_id qid, poste_stats *stats, poste_stations *stations) {
    // send ticket, await response
    if (!send_ticket_request(qid, service_id)) return;
    ticket_response tres = await_ticket_response(qid);
    if (tres.ticket_number < 0) return;

    // find an operator for the service
    int valid_seats[MAX_WORKER_SEATS];
    int n_valid_seats = find_valid_seats(stations, service_id, valid_seats);
    if (n_valid_seats == 0) {
        printf(PREFIX " No operators available for service %s, failed...\n", getpid(), services[service_id]);
        fflush(stdout);
        update_fails_stats(stats, service_id);
        return;
    }

    // Attempt to take a seat
    int current_seat = attempt_take_seat(stations, valid_seats, n_valid_seats);
    while (current_seat == -1) {
        // Wait 5 minutes
        struct timespec t1 = { .tv_sec = 0, .tv_nsec = g_config.minute_duration * 5 };
        struct timespec t2;
        if (nanosleep(&t1, &t2) != 0) {
            printf(PREFIX " Sleep was interrupted.\n", getpid());
            exit(EXIT_FAILURE);
        }

        // Check if shift finished while waiting
        if (stats->current_minute >= g_config.worker_shift_close * 60) {
            printf(PREFIX " Shift ended while waiting for a seat, they made me late, add a explode counter\n", getpid());
            fflush(stdout);

            handle_late_users(stats, service_id);
            update_fails_stats(stats, service_id);

            return;
        }

        // Wait for a seat to become available
        sem_trywait(&stations->stations_freed_event);
        current_seat = attempt_take_seat(stations, valid_seats, n_valid_seats);
    }

    // Seat found, now we can send the service request
    pid_t op_pid = stations->NOF_WORKER_SEATS[current_seat].operator_process;

    int start_wait = stats->current_minute;

    // send to operator and await done
    if (!send_service_request(qid, tres.ticket_number, op_pid)) {
        update_fails_stats(stats, service_id);
        return;
    }
    service_done dres = await_service_done(qid);
    if (dres.ticket_number < 0) {
        update_fails_stats(stats, service_id);
        return;
    }

    // update stats
    int wait_time = stats->current_minute - start_wait;
    update_success_stats(stats, dres.service_id, wait_time, dres.service_time);

    if (stats->current_minute >= g_config.worker_shift_close * 60) {
        printf(PREFIX " Shift ended while waiting for a ticket to finish, they made me late, add a explode counter\n", getpid());
        fflush(stdout);

        handle_late_users(stats, service_id);

        return;
    }

    // Release seat that was taken
    sem_wait(&stations->stations_lock);
    stations->NOF_WORKER_SEATS[current_seat].user_status = FREE;
    sem_post(&stations->stations_lock);
    sem_post(&stations->stations_freed_event);
}

void day_loop(poste_stats *shared_stats, poste_stations *shared_stations, mq_id qid) {
    int service_list[MAX_N_REQUESTS_COMPILE];
    int n_services = generate_service_list(service_list);
    int walk_in_time = generate_walk_in_time(n_services);

    printf(PREFIX " Generated service list with %d services, walk-in time at %02d:%02d\n", getpid(), n_services, walk_in_time / 60, walk_in_time % 60);
    fflush(stdout);

    // Wait for the poste to open
    sem_wait(&shared_stats->open_poste_event);
    // Then wait for the walk in time
    busy_wait_until_walk_in(walk_in_time, shared_stats);

    for (int i = 0; i < n_services; i++) {
        // Check if shift finished while waiting
        if (shared_stats->current_minute >= g_config.worker_shift_close * 60) {
            printf(PREFIX " Shift ended while waiting for a seat, going home\n", getpid());
            fflush(stdout);

            handle_late_users(shared_stats, service_list[i]);
            update_fails_stats(shared_stats, service_list[i]);

            return;
        }
        handle_service(service_list[i], qid, shared_stats, shared_stations);
    }
}

#ifndef UNIT_TEST
int main() {
    int open_shm[2] = {};
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
        printf(PREFIX " Starting the day\n", getpid());
        fflush(stdout);
        sleep(1);

        been_late_today = false;

        if (will_go_to_poste()) {
            printf(PREFIX " Going to the poste today.\n", getpid());
            fflush(stdout);
            day_loop(shared_stats, shared_stations, qid);
        } else {
            printf(PREFIX " Decided not to go to the poste today.\n", getpid());
            fflush(stdout);
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
