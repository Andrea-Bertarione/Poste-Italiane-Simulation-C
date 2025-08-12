#include <stdlib.h>

// Returns mapped pointer or NULL on error
void* init_shared_memory(const char *name, size_t size, int *open_shm, int *open_shm_index);

// Cleans up shared_stats;
void cleanup_shared_memory(const char *name, size_t size, int shm_info, void* shared_info);
