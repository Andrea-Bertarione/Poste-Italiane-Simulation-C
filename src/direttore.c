#define _POSIX_C_SOURCE 199309L

#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <sys/mman.h>
#include <sys/stat.h> 
#include <fcntl.h>

#include <direttore.h>
#include <shared_mem.h>

// TYPES
typedef struct S_poste_stats poste_stats;
typedef struct S_daily_stats daily_stats;
typedef struct S_service_stats service_stats;
typedef struct S_poste_stations poste_stations;
typedef struct S_worker_seat worker_seat;

// ENUMS
enum PROCESS_INDEXES {
    TICKET,
    OPERATORE,
    UTENTE
};

// CONSTANTS
static const char* PROCESS_TYPES[] = {
    "erogatore ticket",
    "NOF WORKERS",
    "NOF USERS"
};

static const char *PROCESS_PATHS[] = {
    "./erogatore_ticket",
    "./operatore",
    "./utente"
};

// Helper function for time calculations
int day_to_minutes(int days) {
    return days * 24 * 60;
}

// Helper function that startup processes
pid_t start_process(enum PROCESS_INDEXES type) {
    pid_t pid = fork();
    if (pid < 0) { perror("fork"); exit(1); }
    if (pid == 0) {
        // child
        printf("%s running\n", PROCESS_TYPES[type]);

        execl(PROCESS_PATHS[type], PROCESS_PATHS[type], (char *)NULL);

        _exit(0);
    }

    return pid;
}

// Helper function that operates on a new day
void start_new_day(int day, poste_stats* shared_stats) {
    printf("New day beggining, current day = %d \n", day);

    // Update SHM day count
    sem_wait(&shared_stats->stats_lock);
    shared_stats->current_day = day;
    sem_post(&shared_stats->stats_lock);
}

//Main implementation
#ifndef UNIT_TEST
int main() {
    pid_t children[1 + NUM_OPERATORS + NUM_USERS];
    int idx = 0;

    int open_shm[] = {};
    int open_shm_index = 0;

    // Create pointer to shared_stats and shared_stations, later mapped to shared memory
    poste_stats *shared_stats;
    poste_stations *shared_stations;

    // Start ticket erogatore
    children[idx++] = start_process(TICKET);

    // Start operator processes
    for (int i = 0; i < NUM_OPERATORS; i++) {
        children[idx++] = start_process(OPERATORE);
    }

    // Start user processes
    for (int i = 0; i < NUM_USERS; i++) {
        children[idx++] = start_process(UTENTE);
    }

    // Struct that holds the correlation between irl time and a minute inside the simulation
    struct timespec t1, t2;
    t1.tv_sec = 0;
    t1.tv_nsec = N_NANO_SECS;

    // Current time elapsed since start (In simulation minutes where 0.1 seconds = 1 minute inside the simulation)
    int minutes_elapsed = 0;
    int days_elapsed = 0; //Keeping a copy and not holding this information in shared memory for fast access and concurrency

    // Open shared memory endpoint(calling it endpoint because it looks similar to a REST API's endpoint) for stats and worker stations
    shared_stats = (poste_stats*) init_shared_memory(SHM_STATS_NAME, SHM_STATS_SIZE, open_shm, &open_shm_index);
    shared_stations = (poste_stations*) init_shared_memory(SHM_STATIONS_NAME, SHM_STATIONS_SIZE, open_shm, &open_shm_index);

    // Initialize every value of the SHM to 0 so everything is in a known state
    memset(shared_stats, 0, SHM_STATS_SIZE);

    // Initialize semaphores for atomic operations
    int sem_stats_result = sem_init(&shared_stats->stats_lock, 1, 1);
    if (sem_stats_result < 0) {
        perror("sem_init");
        exit(EXIT_FAILURE);
    }

    int sem_stations_result = sem_init(&shared_stations->stations_lock, 1, 1);
    if (sem_stations_result < 0) {
        perror("sem_init");
        exit(EXIT_FAILURE);
    }

    // Start simulation loop
    while (minutes_elapsed <= day_to_minutes(SIM_DURATION)) {
        int response = nanosleep(&t1, &t2);
        if (response != 0) {
            printf("Sleep was interrupted.\n");
            exit(EXIT_FAILURE);
        }

        // Check if a new days just started
        if (minutes_elapsed % 1440 == 0) {
            days_elapsed++;

            // Handle new day
            start_new_day(days_elapsed, shared_stats);
        }

        minutes_elapsed++;
    }

    // Kill all children
    for (int i = 0; i < idx; i++) {
        int status;
        if (waitpid(children[i], &status, 0) < 0) {
            perror("waitpid");
        } else if (WIFEXITED(status)) {
            printf("Child %d exited with status %d\n",
                   children[i], WEXITSTATUS(status));
        }
    }

    // Delete the semaphore before closing the SHM
    sem_destroy(&shared_stats->stats_lock);
    sem_destroy(&shared_stations->stations_lock);

    cleanup_shared_memory(SHM_STATS_NAME, SHM_STATS_SIZE, open_shm[0], shared_stats);
    cleanup_shared_memory(SHM_STATIONS_NAME, SHM_STATIONS_SIZE, open_shm[1], shared_stations);

    return EXIT_SUCCESS;
}
#endif  // UNIT_TEST