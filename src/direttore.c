#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <time.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "direttore.h"
#include "poste.h"
#include "shared_mem.h"

#define DIRETTORE_PREFIX "\033[31m[DIRETTORE]:\033[0m"

#define PRINT_STAT(label, value) \
    printf(DIRETTORE_PREFIX " " label ": %d\n", (value))

#define PRINT_FLOAT_STAT(label, numerator, denominator) \
    printf(DIRETTORE_PREFIX " " label ": %.2f\n", \
           (denominator) > 0 ? (float)(numerator) / (denominator) : 0.0f)

typedef struct S_poste_stats      poste_stats;
typedef struct S_daily_stats      daily_stats;
typedef struct S_service_stats    service_stats;
typedef struct S_poste_stations   poste_stations;
typedef struct S_worker_seat      worker_seat;

typedef enum PROCESS_INDEXES {
    TICKET,
    OPERATORE,
    UTENTE
} PROCESS_INDEXES;

static const char *PROCESS_TYPES[] = {
    "erogatore ticket",
    "NOF WORKERS",
    "NOF USERS"
};

static const char *PROCESS_PATHS[] = {
    "bin/erogatore_ticket",
    "bin/operatore",
    "bin/utente"
};

int day_to_minutes(int days) {
    return days * 24 * 60;
}

pid_t start_process(PROCESS_INDEXES type) {
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        exit(EXIT_FAILURE);
    }
    if (pid == 0) {
        printf(DIRETTORE_PREFIX " %s running\n", PROCESS_TYPES[type]);
        execl(PROCESS_PATHS[type], PROCESS_PATHS[type], (char *)NULL);
        perror("execl failed");
        _exit(EXIT_FAILURE);
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
                   today.services[i].total_wait_time / today.services[i].total_requests);
            printf("  Avg service wait time: %.2f\n",
                   today.services[i].total_service_time / today.services[i].total_requests);
        } else {
            printf("  Avg general wait time: N/A\n");
            printf("  Avg service wait time: N/A\n");
        }
    }
    printf("\n" DIRETTORE_PREFIX " ========================\n");
}

void print_final_stats(poste_stats *shared_stats) {
    printf("\n" DIRETTORE_PREFIX " === Final Statistics ===\n");

    PRINT_STAT("Total active operators",      shared_stats->total_active_operators);
    PRINT_STAT("Total simulation pauses",     shared_stats->total_simulation_pauses);
    PRINT_STAT("Current day",                 shared_stats->current_day);
    PRINT_STAT("Current minute",              shared_stats->current_minute);

    printf("\n" DIRETTORE_PREFIX " === Global Statistics ===\n");
    PRINT_STAT("Total served users",          shared_stats->simulation_global.served_users);
    PRINT_STAT("Total failed services",       shared_stats->simulation_global.failed_services);
    PRINT_FLOAT_STAT("Avg general wait time",
                     shared_stats->simulation_global.total_wait_time,
                     shared_stats->simulation_global.total_requests);
    PRINT_FLOAT_STAT("Avg service wait time",
                     shared_stats->simulation_global.total_service_time,
                     shared_stats->simulation_global.total_requests);

    printf("\n" DIRETTORE_PREFIX " === Global Service Statistics ===\n");
    for (int i = 0; i < NUM_SERVICE_TYPES; i++) {
        printf("\n" DIRETTORE_PREFIX " %s:\n", services[i]);
        printf("  Served users: %d\n", shared_stats->simulation_services[i].served_users);
        printf("  Failed services: %d\n", shared_stats->simulation_services[i].failed_services);
        if (shared_stats->simulation_services[i].total_requests > 0) {
            printf("  Avg general wait time: %.2f\n",
                   shared_stats->simulation_services[i].total_wait_time /
                   shared_stats->simulation_services[i].total_requests);
            printf("  Avg service wait time: %.2f\n",
                   shared_stats->simulation_services[i].total_service_time /
                   shared_stats->simulation_services[i].total_requests);
        } else {
            printf("  Avg general wait time: N/A\n");
            printf("  Avg service wait time: N/A\n");
        }
    }
    printf("\n" DIRETTORE_PREFIX " ========================\n");
}

void start_new_day(int day,
                   poste_stats    *shared_stats,
                   poste_stations *shared_stations)
{
    printf(DIRETTORE_PREFIX " New day beginning, current day = %d\n\n", day);
    printf(DIRETTORE_PREFIX " Showing stats for the day:\n");
    print_day_stats(shared_stats->today);

    sem_wait(&shared_stats->stats_lock);
    shared_stats->current_day = day;
    shared_stats->today = (daily_stats){0};
    sem_post(&shared_stats->stats_lock);

    sem_wait(&shared_stations->stations_lock);
    for (int i = 0; i < g_config.num_worker_seats; i++) {
        shared_stations->NOF_WORKER_SEATS[i].operator_process = 0;
        shared_stations->NOF_WORKER_SEATS[i].operator_status  = FREE;
        shared_stations->NOF_WORKER_SEATS[i].user_status      = FREE;
        shared_stations->NOF_WORKER_SEATS[i].service_id       = rand() % NUM_SERVICE_TYPES;
    }
    sem_post(&shared_stations->stations_lock);

    for (int i = 0; i < g_config.num_users + g_config.num_operators; i++) {
        sem_post(&shared_stats->day_update_event);
    }
}

#ifndef UNIT_TEST
int main(const int argc, const char *argv[]) {
    if (argc > 1) {
        if (strcmp(argv[1], "--config") == 0 && argc == 3) {
            load_config((char *)argv[2]);
        } else {
            load_config(NULL);
        }
    } else {
        load_config(NULL);
    }

    pid_t children[1 + g_config.num_operators + g_config.num_users];
    int idx = 0;

    int open_shm[2];
    int open_shm_index = 0;

    poste_stats    *shared_stats;
    poste_stations *shared_stations;

    struct timespec t1 = { .tv_sec = 0, .tv_nsec = g_config.minute_duration };
    struct timespec t2;

    int minutes_elapsed = 0;
    int days_elapsed    = 0;

    shared_stats = init_shared_memory(SHM_STATS_NAME,
                                         SHM_STATS_SIZE,
                                         open_shm,
                                         &open_shm_index);
    shared_stations = init_shared_memory(SHM_STATIONS_NAME,
                                         SHM_STATIONS_SIZE,
                                         open_shm,
                                         &open_shm_index);

    memset(shared_stats, 0, SHM_STATS_SIZE);

    // Initialize semaphores...
    sem_init(&shared_stats->stats_lock,       1, 1);
    sem_init(&shared_stats->open_poste_event, 1, 0);
    sem_init(&shared_stats->close_poste_event,1, 0);
    sem_init(&shared_stats->day_update_event, 1, 0);
    sem_init(&shared_stations->stations_lock, 1, 1);
    sem_init(&shared_stations->stations_event,1, 0);
    sem_init(&shared_stations->stations_freed_event,1,0);

    children[idx++] = start_process(TICKET);
    sleep(1);
    for (int i = 0; i < g_config.num_operators; i++)
        children[idx++] = start_process(OPERATORE);
    for (int i = 0; i < g_config.num_users; i++)
        children[idx++] = start_process(UTENTE);

    printf(DIRETTORE_PREFIX " Waiting for children to start\n");
    sleep(3);

    while (days_elapsed < g_config.sim_duration) {
        if (nanosleep(&t1, &t2) != 0) {
            perror("nanosleep");
            exit(EXIT_FAILURE);
        }

        if (minutes_elapsed == g_config.worker_shift_open * 60
         && minutes_elapsed != 0)
        {
            for (int i = 0;
                 i < g_config.num_operators + g_config.num_users;
                 i++)
            {
                sem_post(&shared_stats->open_poste_event);
            }
        }

        if (minutes_elapsed == g_config.worker_shift_close * 60
         && minutes_elapsed != 0)
        {
            for (int i = 0; i < g_config.num_operators; i++)
                sem_post(&shared_stats->close_poste_event);
        }

        if (minutes_elapsed % (g_config.worker_shift_close * 60) == 0) {
            days_elapsed++;
            start_new_day(days_elapsed,
                          shared_stats,
                          shared_stations);
            minutes_elapsed = 0;
        }

        minutes_elapsed++;
        sem_wait(&shared_stats->stats_lock);
        shared_stats->current_minute = minutes_elapsed;
        sem_post(&shared_stats->stats_lock);
    }

    // Terminate children and clean up...
    for (int i = 0; i < idx; i++)
        kill(children[i], SIGKILL);
    for (int i = 0; i < idx; i++) {
        int status;
        waitpid(children[i], &status, 0);
    }

    print_final_stats(shared_stats);

    sleep(1);
    sem_destroy(&shared_stats->stats_lock);
    sem_destroy(&shared_stations->stations_lock);
    cleanup_shared_memory(SHM_STATS_NAME,
                          SHM_STATS_SIZE,
                          open_shm[0],
                          shared_stats);
    cleanup_shared_memory(SHM_STATIONS_NAME,
                          SHM_STATIONS_SIZE,
                          open_shm[1],
                          shared_stations);

    return EXIT_SUCCESS;
}
#endif
