
//#define _POSIX_C_SOURCE 199309L

#ifndef POSTE_H
#define POSTE_H

#include <unistd.h>
#include <sys/types.h>
#include <semaphore.h>

#define SIM_DURATION 3 // days
#define N_NANO_SECS 10000000L // 0.1s
#define NUM_OPERATORS     2 // default without configs
#define NUM_USERS         1 // default without configs
#define NUM_WORKER_SEATS 2 // default without configs

#define WORKER_SHIFT_OPEN 7
#define WORKER_SHIFT_CLOSE 19

#define P_SERV_MIN 20
#define P_SERV_MAX 100

#define MAX_N_REQUESTS 10

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
    int *operator_counter_ratios;  // Per counter
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
    int current_minute;
    
    // Synchronization
    sem_t stats_lock;  // Semaphore index for atomic updates
    sem_t day_update_event; // Semaphore that tells processes when a new day starts
};

struct S_worker_seat {
    pid_t operator_process;
    SEAT_STATUS operator_status;
    SEAT_STATUS user_status;
    char* service; //Name of the serive, inside the service table
};

struct S_poste_stations {
    //Array of worker seats
    struct S_worker_seat *NOF_WORKER_SEATS;

    // Synchronization
    sem_t stations_lock;  // Semaphore index for atomic updates
};

#define SHM_STATS_NAME   "/poste_stats"
#define SHM_STATS_SIZE   sizeof(struct S_poste_stats)
    
#define SHM_STATIONS_NAME "/poste_stations"
#define SHM_STATIONS_SIZE   sizeof(struct S_poste_stations)

#endif