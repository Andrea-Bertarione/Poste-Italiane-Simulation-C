#define _POSIX_C_SOURCE 199309L
#define _GNU_SOURCE

#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <sys/mman.h>
#include <sys/stat.h> 
#include <fcntl.h>

#include <direttore.h>
#include <shared_mem.h>

#define DIRETTORE_PREFIX "\033[31m[DIRETTORE]:\033[0m"

#define PRINT_STAT(label, value) \
    printf(DIRETTORE_PREFIX " " label ": %d\n", (value))

#define PRINT_FLOAT_STAT(label, numerator, denominator) \
    printf(DIRETTORE_PREFIX " " label ": %.2f\n", \
           (denominator) > 0 ? (float)(numerator) / (denominator) : 0.0f)

typedef struct S_poste_stats poste_stats;
typedef struct S_daily_stats daily_stats;
typedef struct S_service_stats service_stats;
typedef struct S_poste_stations poste_stations;
typedef struct S_worker_seat worker_seat;

enum PROCESS_INDEXES {
    TICKET,
    OPERATORE,
    UTENTE
};

static const char* PROCESS_TYPES[] = {
    "erogatore ticket",
    "NOF WORKERS",
    "NOF USERS"
};

static const char *PROCESS_PATHS[] = {
    "bin/erogatore_ticket",
    "bin/operatore",
    "bin/utente"
};

//List of available services
const char* services[] = {
    "Invio e ritiro pacchi",
    "Invio e ritiro lettere e raccomandate",
    "Prelievi e versamenti Bancoposta",
    "Pagamento bollettini postali",
    "Acquisto prodotti finanziari",
    "Acquisto orologi e braccialetti"
};


int day_to_minutes(int days) {
    return days * 24 * 60;
}

pid_t start_process(enum PROCESS_INDEXES type) {
    pid_t pid = fork();
    if (pid < 0) { perror("fork"); exit(1); }
    if (pid == 0) {
        printf(DIRETTORE_PREFIX " %s running\n", PROCESS_TYPES[type]);
        execl(PROCESS_PATHS[type], PROCESS_PATHS[type], (char *)NULL);
        perror("execl failed");
        _exit(1);
    }
    return pid;
}

void print_day_stats(daily_stats today) {
    printf("\n" DIRETTORE_PREFIX " === Daily Statistics ===\n");
    
    PRINT_STAT("Active operators", today.active_operators);
    PRINT_STAT("Total pauses", today.total_pauses);
    PRINT_STAT("Served users", today.global.served_users);
    PRINT_STAT("Failed services", today.global.failed_services);
    
    PRINT_FLOAT_STAT("Average general wait time", 
                     today.global.total_wait_time, 
                     today.global.total_requests);
    
    PRINT_FLOAT_STAT("Average service wait time", 
                     today.global.total_service_time, 
                     today.global.total_requests);

    printf("\n" DIRETTORE_PREFIX " === Service Statistics ===\n");
    for (int i = 0; i < NUM_SERVICE_TYPES; i++) {
        printf("\n" DIRETTORE_PREFIX " %s:\n", services[i]);
        
        printf("  Served users: %d\n", today.services[i].served_users);
        printf("  Failed services: %d\n", today.services[i].failed_services);
        
        if (today.services[i].total_requests > 0) {
            printf("  Avg general wait time: %.2f\n", 
                   (float)today.services[i].total_wait_time / today.services[i].total_requests);
            printf("  Avg service wait time: %.2f\n", 
                   (float)today.services[i].total_service_time / today.services[i].total_requests);
        } else {
            printf("  Avg general wait time: N/A\n");
            printf("  Avg service wait time: N/A\n");
        }
    }
    
    printf("\n" DIRETTORE_PREFIX " ========================\n");
}

void start_new_day(int day, poste_stats* shared_stats, poste_stations* shared_stations) {
    printf(DIRETTORE_PREFIX " New day beginning, current day = %d\n\n", day);
    printf(DIRETTORE_PREFIX " Showing stats for the day:\n");
    
    sem_wait(&shared_stats->stats_lock);
    shared_stats->current_day = day;
    shared_stats->today = (daily_stats) {};
    sem_post(&shared_stats->stats_lock);

    print_day_stats(shared_stats->today);

    // Setup worker seats for the new day
    sem_wait(&shared_stations->stations_lock);
    for (int i = 0; i < NUM_WORKER_SEATS; i++) {
        shared_stations->NOF_WORKER_SEATS[i].operator_process = 0;
        shared_stations->NOF_WORKER_SEATS[i].operator_status = FREE;
        shared_stations->NOF_WORKER_SEATS[i].user_status = FREE;
        shared_stations->NOF_WORKER_SEATS[i].service_id = rand() % NUM_SERVICE_TYPES; // Randomly assign a service for the day
    }
    sem_post(&shared_stations->stations_lock);

    for (int i = 0; i < NUM_USERS + NUM_OPERATORS; i++) {
        sem_post(&shared_stats->day_update_event);
    }
}

#ifndef UNIT_TEST
int main() {
    pid_t children[1 + NUM_OPERATORS + NUM_USERS];
    int idx = 0;

    int open_shm[] = {};
    int open_shm_index = 0;

    poste_stats *shared_stats;
    poste_stations *shared_stations;

    struct timespec t1, t2;
    t1.tv_sec = 0;
    t1.tv_nsec = N_NANO_SECS;

    int minutes_elapsed = 0;
    int days_elapsed = 0;

    shared_stats = (poste_stats*) init_shared_memory(SHM_STATS_NAME, SHM_STATS_SIZE, open_shm, &open_shm_index);
    shared_stations = (poste_stations*) init_shared_memory(SHM_STATIONS_NAME, SHM_STATIONS_SIZE, open_shm, &open_shm_index);

    memset(shared_stats, 0, SHM_STATS_SIZE);

    int sem_stats_result = sem_init(&shared_stats->stats_lock, 1, 1);
    if (sem_stats_result < 0) {
        perror("sem_init");
        exit(EXIT_FAILURE);
    }

    int sem_open_poste_result = sem_init(&shared_stats->open_poste_event, 1, 0);
    if (sem_open_poste_result < 0) {
        perror("sem_init");
        exit(EXIT_FAILURE);
    }

    int sem_day_result = sem_init(&shared_stats->day_update_event, 1, 0);
    if (sem_day_result < 0) {
        perror("sem_init");
        exit(EXIT_FAILURE);
    }

    int sem_stations_result = sem_init(&shared_stations->stations_lock, 1, 1);
    if (sem_stations_result < 0) {
        perror("sem_init");
        exit(EXIT_FAILURE);
    }

    int sem_stations_event_result = sem_init(&shared_stations->stations_event, 1, 0);
    if (sem_stations_event_result < 0) {
        perror("sem_init");
        exit(EXIT_FAILURE);
    }

    int sem_stations_freed_result = sem_init(&shared_stations->stations_freed_event, 1, 0);
    if (sem_stations_freed_result < 0) {
        perror("sem_init");
        exit(EXIT_FAILURE);
    }

    children[idx++] = start_process(TICKET);

    sleep(1);

    for (int i = 0; i < NUM_OPERATORS; i++) {
        children[idx++] = start_process(OPERATORE);
    }

    for (int i = 0; i < NUM_USERS; i++) {
        children[idx++] = start_process(UTENTE);
    }

    printf(DIRETTORE_PREFIX " Waiting for children to properly startup\n");
    sleep(3);

    while (days_elapsed < SIM_DURATION) {
        int response = nanosleep(&t1, &t2);
        if (response != 0) {
            printf("Sleep was interrupted.\n");
            exit(EXIT_FAILURE);
        }

        if (minutes_elapsed % WORKER_SHIFT_OPEN == 0) {
            for (int i = 0; i < NUM_OPERATORS + NUM_USERS; i++) {
                sem_post(&shared_stats->open_poste_event);
            }
            printf(DIRETTORE_PREFIX " Poste opened for the day\n");
        }

        if (minutes_elapsed % 1440 == 0) {
            days_elapsed++;
            start_new_day(days_elapsed, shared_stats, shared_stations);
            minutes_elapsed = 0;
        }

        minutes_elapsed++;
        sem_wait(&shared_stats->stats_lock);
        shared_stats->current_minute = minutes_elapsed;
        sem_post(&shared_stats->stats_lock);
    }

    sleep(2);

    for (int i = 0; i < idx; i++) {
        kill(children[i], SIGKILL);
    }

    for (int i = 0; i < idx; i++) {
        int status;
        if (waitpid(children[i], &status, 0) < 0) {
            perror("waitpid");
        } else if (WIFEXITED(status)) {
            printf(DIRETTORE_PREFIX " Child %d exited with status %d\n",
                children[i], WEXITSTATUS(status));
        } else if (WIFSIGNALED(status)) {
            printf(DIRETTORE_PREFIX " Child %d killed by signal %d\n",
                children[i], WTERMSIG(status));
        }
    }

    sem_destroy(&shared_stats->stats_lock);
    sem_destroy(&shared_stations->stations_lock);

    cleanup_shared_memory(SHM_STATS_NAME, SHM_STATS_SIZE, open_shm[0], shared_stats);
    cleanup_shared_memory(SHM_STATIONS_NAME, SHM_STATIONS_SIZE, open_shm[1], shared_stations);

    return EXIT_SUCCESS;
}
#endif
