#ifndef POSTE_H
#define POSTE_H

#include <unistd.h>
#include <sys/types.h>
#include <semaphore.h>

#include <config.h>

#define MAX_PATH_LENGTH 256

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
    int late_users;
    int late_user_done;
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
    sem_t open_poste_event; // Semaphore that tells processes when the poste opens
    sem_t close_poste_event; // Semaphore that tells processes when the poste closes
    sem_t day_update_event; // Semaphore that tells processes when a new day starts
    char configuration_file[MAX_PATH_LENGTH]; // Path to the configuration file
};

struct S_worker_seat {
    pid_t operator_process;
    SEAT_STATUS operator_status;
    SEAT_STATUS user_status;
    int service_id; //Id of the service, inside the service table
};

struct S_poste_stations {
    //Array of worker seats
    struct S_worker_seat NOF_WORKER_SEATS[MAX_WORKER_SEATS]; // 30 maximum seats

    // Synchronization
    sem_t stations_lock;  // Semaphore index for atomic updates
    sem_t stations_event; // Semaphore that tells workers when a station opens up
    sem_t stations_freed_event; // Semaphore that tells users when a station is freed
};

#define SHM_STATS_NAME   "/poste_stats"
#define SHM_STATS_SIZE   sizeof(struct S_poste_stats)
    
#define SHM_STATIONS_NAME "/poste_stations"
#define SHM_STATIONS_SIZE   sizeof(struct S_poste_stations)

#endif