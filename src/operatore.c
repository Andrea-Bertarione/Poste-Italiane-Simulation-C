#define _POSIX_C_SOURCE 199309L

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

#define PREFIX "\033[34m[OPERATORE]:\033[0m"

//List of available services
const char* services[] = {
    "Invio e ritiro pacchi",
    "Invio e ritiro lettere e raccomandate",
    "Prelievi e versamenti Bancoposta",
    "Pagamento bollettini postali",
    "Acquisto prodotti finanziari",
    "Acquisto orologi e braccialetti"
};

//List of durations(Minutes) refenced to services
const int services_duration[] = {
    10,
    8,
    6,
    8,
    20,
    20
};

// function to handle operators taking seats
void take_seat(poste_stations *shared_stations, int i) {
    shared_stations->NOF_WORKER_SEATS[i].operator_process = getpid();
    shared_stations->NOF_WORKER_SEATS[i].operator_status = OCCUPIED;

    printf(PREFIX " Taking seat %d for service %s\n",
           i, services[shared_stations->NOF_WORKER_SEATS[i].service_id]);
    fflush(stdout);
}

// await service request from users
service_request await_service_request(mq_id qid) {
    service_request res;
    ssize_t n = mq_receive(qid,
                           (MSG_TYPE_SERVICE_REQUEST MSG_TYPE_TICKET_REQUEST_MULT) + getpid(),
                           &res,
                           sizeof(res),
                           0);
    if (n < 0) {
        if (errno == EINTR) {
            printf(PREFIX " mq_receive interrupted, retrying\n");
            fflush(stdout);
            res.ticket_number = -1;
        }
        fprintf(stderr, PREFIX " ERROR mq_receive: %s\n",
                strerror(errno));
        fflush(stderr);
        res.ticket_number = -1;
    }

    printf(PREFIX " Received response, ticket_number=%d\n",
           res.ticket_number);
    fflush(stdout);

    return res;
}

// send service done returns 0 on failure and 1 on success
int send_service_done(mq_id qid, int ticket_number, pid_t user_pid, int service_id, double service_time) {
    service_done req;
    req.sender_pid = getpid();
    req.ticket_number = ticket_number;
    req.service_time = service_time;
    req.service_id = service_id;

    printf(PREFIX " Sending service done message for ticket %d\n", ticket_number);
    fflush(stdout);

    if (mq_send(qid,
                user_pid,
                &req,
                sizeof(req)) < 0) {
        fprintf(stderr, PREFIX " ERROR mq_send service request: %s\n",
                strerror(errno));
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

    struct timespec t1 = { .tv_sec = 0, .tv_nsec = N_NANO_SECS * 5 };
    struct timespec t2;

    struct timespec t3 = { .tv_sec = 0, .tv_nsec = N_NANO_SECS * 5 };
    struct timespec t4;

    poste_stats *shared_stats = (poste_stats*) init_shared_memory(
        SHM_STATS_NAME, SHM_STATS_SIZE, open_shm, &open_shm_index);

    poste_stations *shared_stations = (poste_stations*) init_shared_memory(
        SHM_STATIONS_NAME, SHM_STATIONS_SIZE, open_shm, &open_shm_index);

    srand((unsigned)getpid());

    const int selected_service = rand() % NUM_SERVICE_TYPES;

    sem_wait(&shared_stats->day_update_event);

    while (true) {
        printf(PREFIX " Starting work for the day\n");
        fflush(stdout);
        sleep(1);

        // Wait for the poste to open
        sem_wait(&shared_stats->open_poste_event);

        printf(PREFIX " Entering the poste at %02d:%02d\n",
               shared_stats->current_minute / 60, shared_stats->current_minute % 60);

        bool seated_flag = false;
        int current_seat = -1;

        // First batch of workers will try to find a seat
        sem_wait(&shared_stations->stations_lock);
        for (int i = 0; i < NUM_WORKER_SEATS; i++) {
            if (
                shared_stations->NOF_WORKER_SEATS[i].operator_status == FREE && 
                shared_stations->NOF_WORKER_SEATS[i].service_id == selected_service
            ) {
                take_seat(shared_stations, i);
                seated_flag = true;
                current_seat = i;
                break;
            }
        }
        sem_post(&shared_stations->stations_lock);

        if (!seated_flag) {
            printf(PREFIX " No available seats for service %s, waiting for a place to open up\n",
                   services[selected_service]);
        }

        // Wait for a seat to become available
        while (!seated_flag) {
            sem_wait(&shared_stations->stations_event);
            sem_wait(&shared_stations->stations_lock);
            for (int i = 0; i < NUM_WORKER_SEATS; i++) {
                if (
                    shared_stations->NOF_WORKER_SEATS[i].operator_status == FREE && 
                    shared_stations->NOF_WORKER_SEATS[i].service_id == selected_service
                ) {
                    take_seat(shared_stations, i);
                    seated_flag = true;
                    current_seat = i;
                    break;
                }
            }
            sem_post(&shared_stations->stations_lock);

            if (nanosleep(&t1, &t2) != 0) {
                printf(PREFIX " Sleep was interrupted.\n");
                exit(EXIT_FAILURE);
            }
        }

        worker_seat seat_struct = shared_stations->NOF_WORKER_SEATS[current_seat];
        
        // Start working loop
        while (true) {
            service_request service_req = await_service_request(qid);
            if (service_req.ticket_number == -1) {
                printf(PREFIX " Service request interrupted, retrying\n");
                fflush(stdout);
                continue;  
            }

            printf(PREFIX " [%02d:%02d] Received service request for ticket %d\n",
                shared_stats->current_minute / 60,
                shared_stats->current_minute % 60,
                service_req.ticket_number);
            fflush(stdout);

            // Simulate service time
            int nominal = services_duration[seat_struct.service_id];
            printf(PREFIX " [%02d:%02d] Starting service for ticket %d, service time: around %d minutes\n",
                   shared_stats->current_minute / 60,
                   shared_stats->current_minute % 60,
                   service_req.ticket_number, nominal);
            fflush(stdout);

            long long base_nano = (long long)nominal * N_NANO_SECS; // nanoseconds for nominal time

            // Compute jittered range [50%,150%] of base_nano
            long long min_nano = base_nano / 2;                     // 0.5×
            long long max_nano = base_nano + base_nano / 2;         // 1.5×
            long long span     = max_nano - min_nano + 1;
            long long rand_nano = min_nano + (rand() % span);

            t3.tv_nsec = rand_nano;

            if (nanosleep(&t3, &t4) != 0) {
                printf(PREFIX " Sleep was interrupted.\n");
                exit(EXIT_FAILURE);
            }

            printf(PREFIX " [%02d:%02d] Finished service for ticket %d, service time: %lld nanoseconds\n",
                   shared_stats->current_minute / 60,
                   shared_stats->current_minute % 60,
                   service_req.ticket_number, rand_nano);

            // Send service done message
            if (!send_service_done(qid, service_req.ticket_number, service_req.sender_pid,
                                   seat_struct.service_id, (double)rand_nano / N_NANO_SECS))
            {
                printf(PREFIX " Failed to send service done message for ticket %d\n",
                       service_req.ticket_number);
                fflush(stdout);
                continue;
            }

            
        }
    }

    return 0;
}