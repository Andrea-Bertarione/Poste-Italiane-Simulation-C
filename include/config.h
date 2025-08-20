#define SIM_DURATION 5 // days
#define N_NANO_SECS 50000000L // 0.05s (1 minute)
#define NUM_OPERATORS     10 // default without configs
#define NUM_USERS         5 // default without configs
#define NUM_WORKER_SEATS 15 // default without configs

#define WORKER_SHIFT_OPEN 8 // 8:00 AM
#define WORKER_SHIFT_CLOSE 20 // 8:00 PM
#define P_SERV_MIN 20
#define P_SERV_MAX 100
#define EXPLODE_MAX 30 // Maximum number of users that can still waiting at the end of the day, otherwise the process explodes
#define MAX_N_REQUESTS 10 // Maximum number of requests a user can make in a day

#define MAX_N_REQUESTS_COMPILE 50 // Maximum number of requests a user can make in a day for compile time
#define MAX_WORKER_SEATS 30 // Maximum number of worker seats

struct poste_config {
    int num_operators; // Number of operators in the simulation
    int num_users; // Number of users in the simulation
    int num_worker_seats; // Number of worker seats available
    int sim_duration; // Duration of the simulation in days
    long p_serv_min; // Probability user skips day min
    long p_serv_max; // Probability user skips day max
    long minute_duration; // Duration of a minute in nanoseconds
    int worker_shift_open; // Opening time of the worker shift in minutes
    int worker_shift_close; // Closing time of the worker shift in minutes
    int explode_max; // Maximum number of users that can still waiting at the end of the day, otherwise the process explodes
    int max_n_requests; // Maximum number of requests a user can make in a day
};

#define NUM_SERVICE_TYPES 6  // From Table 1 in specs

extern struct poste_config g_config;
extern char *services[NUM_SERVICE_TYPES]; // List of available services
extern int services_duration[NUM_SERVICE_TYPES];// List of durations (Minutes) referenced to services

void load_config(char *config_file_path);