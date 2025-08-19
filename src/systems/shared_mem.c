#define _POSIX_C_SOURCE 200809L


#include <sys/mman.h>
#include <sys/stat.h> 
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>  // declares ftruncate

#include <shared_mem.h>

void* init_shared_memory(const char *name, size_t size, int *open_shm, int *open_shm_index) {
    int shm_info = shm_open(name, O_CREAT|O_RDWR, 0666);
    ftruncate(shm_info, size);
    void *shared_info = mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_SHARED, shm_info, 0);

    if (shared_info == MAP_FAILED) {
        perror("mmap");
        exit(EXIT_FAILURE);
    }

    open_shm[*open_shm_index] = shm_info;
    (*open_shm_index)++;

    return shared_info;
}

void cleanup_shared_memory(const char *name, size_t size, int shm_info, void* shared_info) {
    // Close and unlink the shared memory
    munmap(shared_info, size);
    close(shm_info);
    
    shm_unlink(name);
}