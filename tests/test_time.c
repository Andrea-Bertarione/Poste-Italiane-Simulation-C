#define _POSIX_C_SOURCE 199309L

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <semaphore.h>
#include "direttore.h"  // declares day_to_minutes, start_new_day, poste_stats

int main(void) {
    printf("\n[TEST] Starting time conversion and start_new_day tests...\n");

    // ---- Test day_to_minutes ----
    printf("[STEP] Testing day_to_minutes()...\n");
    int m0 = day_to_minutes(0);
    printf("       Input: 0 days -> Output: %d minutes\n", m0);
    assert(m0 == 0);

    int m1 = day_to_minutes(1);
    printf("       Input: 1 day -> Output: %d minutes\n", m1);
    assert(m1 == 1440);

    int m5 = day_to_minutes(5);
    printf("       Input: 5 days -> Output: %d minutes\n", m5);
    assert(m5 == 5 * 1440);

    printf("[OK] day_to_minutes() outputs are correct.\n");

    // ---- Test start_new_day ----
    printf("[STEP] Testing start_new_day() update...\n");
    poste_stats stats = {0};

    printf("       Initializing semaphore inside poste_stats struct...\n");
    if (sem_init(&stats.stats_lock, 0, 1) != 0) {
        perror("[FAIL] sem_init");
        exit(EXIT_FAILURE);
    }

    printf("       Calling start_new_day(3)...\n");
    start_new_day(3, &stats);

    printf("       Verifying current_day value...\n");
    assert(stats.current_day == 3);
    printf("[OK] current_day updated correctly to %d.\n", stats.current_day);

    printf("       Destroying semaphore...\n");
    if (sem_destroy(&stats.stats_lock) != 0) {
        perror("[WARN] sem_destroy");
    }

    printf("[TEST] All time function tests passed successfully!\n\n");
    return 0;
}
