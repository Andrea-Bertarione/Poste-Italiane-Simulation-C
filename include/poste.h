
#define _GNU_SOURCE
#define _POSIX_C_SOURCE 199309L

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <sys/mman.h>
#include <sys/stat.h> 
#include <fcntl.h>
#include <semaphore.h>
#include <string.h>

#define SIM_DURATION 10 // days
#define N_NANO_SECS 10000000L // 0.1s
#define NUM_OPERATORS     3 // default without configs
#define NUM_USERS         5 // default without configs
#define NUM_WORKER_SEATS 3 // default without configs

#define NUM_SERVICE_TYPES 6  // From Table 1 in specs

typedef enum SEAT_STATUS {
    FREE,
    OCCUPIED
} SEAT_STATUS;

struct S_service_stats {
    int served_users;
    int failed_services;
    double total_wait_time;      // For calculating averages
    double total_service_time;   // For calculating averages
    int total_requests;          // To calculate averages properly
};

struct S_daily_stats {
    struct S_service_stats global;
    struct S_service_stats services[NUM_SERVICE_TYPES];
    int active_operators;
    int total_pauses;
    int operator_counter_ratios[NUM_OPERATORS];  // Per counter
};

struct S_poste_stats {
    // Cumulative simulation statistics
    struct S_service_stats simulation_global;
    struct S_service_stats simulation_services[NUM_SERVICE_TYPES];
    
    // Daily statistics (reset each day)
    struct S_daily_stats today;
    
    // Simulation-wide counters
    int total_active_operators;
    int total_simulation_pauses;
    int current_day;
    
    // Synchronization
    sem_t  stats_lock;  // Semaphore index for atomic updates
};

struct S_worker_seat {
    pid_t worker_process;
    SEAT_STATUS status;
    char* service;
};

struct S_service_handler {
    pid_t NOF_WORKER_SEATS[NUM_WORKER_SEATS];
};

#define SHM_STATS_NAME   "/poste_stats"
#define SHM_STATS_SIZE   sizeof(struct S_poste_stats)