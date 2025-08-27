#define _POSIX_C_SOURCE 199309L

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>     // for exit
#include <semaphore.h>
#include <errno.h>
#include <poste.h>
#include <direttore.h>  

typedef struct S_poste_stats poste_stats;

int main(void) {
    const char *name = SHM_STATS_NAME;
    size_t size = SHM_STATS_SIZE;
    int open_fds[1];
    int fd_index = 0;

    printf("\n[TEST] Starting shared memory init/cleanup test...\n");

    // init_shared_memory
    printf("[STEP] Initializing shared memory segment: %s (size=%zu)\n", name, size);
    poste_stats *p = init_shared_memory(name, size, open_fds, &fd_index);
    if (!p) {
        perror("[FAIL] init_shared_memory");
        exit(EXIT_FAILURE);
    }
    printf("[OK] Shared memory initialized at %p.\n", (void*)p);

    // initialize everything to 0
    printf("[STEP] Initializing all values to a known state (0)\n");
    memset(p, 0, SHM_STATS_SIZE);

    // Initialize semaphore inside SHM
    printf("[STEP] Creating the semaphore in shared memory (pshared=1, value=1)...\n");
    if (sem_init(&p->stats_lock, 1, 1) != 0) {
        perror("[FAIL] sem_init");
        cleanup_shared_memory(name, size, open_fds[0], p);
        exit(EXIT_FAILURE);
    }
    printf("[OK] Semaphore initialized.\n");

    // Memory must be zero-initialized
    printf("[STEP] Checking zero-initialization of shared memory...\n");
    if (p->current_day != 0) {
        fprintf(stderr, "[FAIL] current_day not zero (value=%d)\n", p->current_day);
        cleanup_shared_memory(name, size, open_fds[0], p);
        exit(EXIT_FAILURE);
    }
    printf("[OK] Memory zero-initialized.\n");

    // Test semaphore locking/updating
    printf("[STEP] Locking semaphore and updating current_day...\n");
    if (sem_wait(&p->stats_lock) != 0) {
        perror("[FAIL] sem_wait");
        cleanup_shared_memory(name, size, open_fds[0], p);
        exit(EXIT_FAILURE);
    }
    p->current_day = 10;
    if (sem_post(&p->stats_lock) != 0) {
        perror("[FAIL] sem_post");
        cleanup_shared_memory(name, size, open_fds[0], p);
        exit(EXIT_FAILURE);
    }
    assert(p->current_day == 10);
    printf("[OK] Semaphore works; current_day updated to %d.\n", p->current_day);

    // Cleanup
    printf("[STEP] Cleaning up shared memory...\n");
    cleanup_shared_memory(name, size, open_fds[0], p);
    printf("[OK] Shared memory cleaned up and unlinked.\n");

    printf("[TEST] All shared memory tests passed successfully!\n\n");
    return 0;
}
