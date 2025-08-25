#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <config.h>

#define MAX_LINE_LEN 128

char *services[NUM_SERVICE_TYPES] = {
    "Invio e ritiro pacchi",
    "Invio e ritiro lettere e raccomandate",
    "Prelievi e versamenti Bancoposta",
    "Pagamento bollettini postali",
    "Acquisto prodotti finanziari",
    "Acquisto orologi e braccialetti"
};

int services_duration[NUM_SERVICE_TYPES] = {
    10,
    8,
    6,
    8,
    20,
    20
};

struct poste_config g_config = {
    .num_operators = NUM_OPERATORS,
    .num_users = NUM_USERS,
    .num_worker_seats = NUM_WORKER_SEATS,
    .sim_duration = SIM_DURATION,
    .p_serv_min = P_SERV_MIN,
    .p_serv_max = P_SERV_MAX,
    .minute_duration = N_NANO_SECS,
    .worker_shift_open = WORKER_SHIFT_OPEN,
    .worker_shift_close = WORKER_SHIFT_CLOSE,
    .explode_max = EXPLODE_MAX,
    .max_n_requests = MAX_N_REQUESTS,
    .nof_pause = NOF_PAUSE
};

// Load configuration from a file or set default values
void load_config(char *config_file_path) {
    // Load configuration from a file or
    // This is a placeholder for actual configuration loading logic
    if (config_file_path == NULL) {
        printf("Using default configuration.\n");
        return;
    }

    if (config_file_path[0] == '\0') {
        printf("Using default configuration.\n");
        return;
    }

    printf("Loading configuration from %s\n", config_file_path);
    fflush(stdout);

    FILE *file = fopen(config_file_path, "r");
    if (file == NULL) {
        perror("Failed to open config file");
        exit(EXIT_FAILURE);
    }

    char line[MAX_LINE_LEN];
    while (fgets(line, sizeof(line), file)) {
        // Strip comment
        char *hash = strchr(line, '#');
        if (hash) *hash = '\0';

        // Trim whitespace
        char *key = strtok(line, " =\t\n");
        if (key == NULL) continue;
        char *val = strtok(NULL, " =\t\n");
        if (val == NULL) continue;

        int  iv; long lv;
        if      (strcmp(key, "num_operators") == 0) {
            iv = atoi(val);
            if (iv > 0) g_config.num_operators = iv;
        }
        else if (strcmp(key, "num_users") == 0) {
            iv = atoi(val);
            if (iv > 0) g_config.num_users = iv;
        }
        else if (strcmp(key, "num_worker_seats") == 0) {
            iv = atoi(val);
            if (iv > 0) g_config.num_worker_seats = iv;
        }
        else if (strcmp(key, "sim_duration") == 0) {
            iv = atoi(val);
            if (iv > 0) g_config.sim_duration = iv;
        }
        else if (strcmp(key, "p_serv_min") == 0) {
            iv = atoi(val);
            if (iv >= 0) g_config.p_serv_min = iv;
        }
        else if (strcmp(key, "p_serv_max") == 0) {
            iv = atoi(val);
            if (iv >= 0) g_config.p_serv_max = iv;
        }
        else if (strcmp(key, "minute_duration") == 0) {
            lv = atol(val);
            if (lv > 0) g_config.minute_duration = lv;
        }
        else if (strcmp(key, "worker_shift_open") == 0) {
            iv = atoi(val);
            if (iv >= 0) g_config.worker_shift_open = iv;
        }
        else if (strcmp(key, "worker_shift_close") == 0) {
            iv = atoi(val);
            if (iv >= 0) g_config.worker_shift_close = iv;
        }
        else if (strcmp(key, "explode_max") == 0) {
            iv = atoi(val);
            if (iv >= 0) g_config.explode_max = iv;
        }
        else if (strcmp(key, "max_n_requests") == 0) {
            iv = atoi(val);
            if (iv > 0) g_config.max_n_requests = iv;
        }
        else if (strcmp(key, "nof_pause") == 0) {
            iv = atoi(val);
            if (iv > 0) g_config.nof_pause = iv;
        }
        // unrecognized keys are ignored
    }

    // print current configuration
    /*
    printf("Current configuration:\n");
    printf("  num_operators: %d\n", g_config.num_operators);
    printf("  num_users: %d\n", g_config.num_users);
    printf("  num_worker_seats: %d\n", g_config.num_worker_seats);
    printf("  sim_duration: %d days\n", g_config.sim_duration);
    printf("  p_serv_min: %ld\n", g_config.p_serv_min);
    printf("  p_serv_max: %ld\n", g_config.p_serv_max);
    printf("  minute_duration: %ld nanoseconds\n", g_config.minute_duration);
    printf("  worker_shift_open: %d\n", g_config.worker_shift_open);
    printf("  worker_shift_close: %d\n", g_config.worker_shift_close);
    printf("  explode_max: %d\n", g_config.explode_max);
    printf("  max_n_requests: %d\n", g_config.max_n_requests);
    fflush(stdout);
    */
   
    fclose(file);
}