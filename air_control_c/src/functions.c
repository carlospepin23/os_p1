// #define _POSIX_C_SOURCE 200809L
// #include <fcntl.h>
// #include <stdio.h>
// #include <stdlib.h>
// #include <string.h>
// #include <sys/mman.h>
// #include <unistd.h>
// #include <pthread.h>
// #include <signal.h>


// #define SH_MEMORY_NAME "/shm_pids"
// #define N_ELEM 3
// #define BLOCK_SIZE (N_ELEM * sizeof(int))
// #define TOTAL_TAKEOFFS 20

// int planes = 0;
// int takeoffs = 0;
// int total_takeoffs = 0;
// int *array_mmap;
// int fd;

// pid_t air_control_pid, radio_control_pid;
// pthread_mutex_t state_lock, runway1_lock, runway2_lock;

// void MemoryCreate() {
//   // TODO2: create the shared memory segment, configure it and store the PID of
//   // the process in the first position of the memory block.
//     // Create block
//     fd = shm_open(SH_MEMORY_NAME, O_CREAT | O_RDWR, 0666);
//     ftruncate(fd, BLOCK_SIZE);
//     array_mmap =
//         mmap(0, BLOCK_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

//     if (array_mmap == MAP_FAILED) {
//     perror("mmap failed");
//     exit(1);
//     }
// }

// void SigHandler2(int signal) {
//   planes+=5;
// }

// void* TakeOffsFunction() {
//   // TODO: implement the logic to control a takeoff thread
//   //    Use a loop that runs while total_takeoffs < TOTAL_TAKEOFFS
//   //    Use runway1_lock or runway2_lock to simulate a locked runway
//   //    Use state_lock for safe access to shared variables (planes,
//   //    takeoffs, total_takeoffs)
//   //    Simulate the time a takeoff takes with sleep(1)
//   //    Send SIGUSR1 every 5 local takeoffs
//   //    Send SIGTERM when the total takeoffs target is reached
// }






#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <sys/wait.h>

#define SH_MEMORY_NAME "/shm_pids"
#define N_ELEM 3
#define BLOCK_SIZE (N_ELEM * sizeof(int))
#define TOTAL_TAKEOFFS 20

// Variable DEFINITIONS (only here!)
int planes = 0;
int takeoffs = 0;
int total_takeoffs = 0;
int *array_mmap = NULL;
int fd = -1;
pid_t air_control_pid = 0;
pid_t radio_control_pid = 0;

// External declarations for mutexes (defined in main.c)
extern pthread_mutex_t state_lock, runway1_lock, runway2_lock;

void MemoryCreate(void) {
    fd = shm_open(SH_MEMORY_NAME, O_CREAT | O_RDWR, 0666);
    if (fd == -1) {
        perror("shm_open failed");
        exit(1);
    }
    
    if (ftruncate(fd, BLOCK_SIZE) == -1) {
        perror("ftruncate failed");
        exit(1);
    }
    
    array_mmap = mmap(0, BLOCK_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    if (array_mmap == MAP_FAILED) {
        perror("mmap failed");
        exit(1);
    }
    
    // Initialize the shared memory
    memset(array_mmap, 0, BLOCK_SIZE);
}

void SigHandler2(int signal) {
    // Signal-safe operation
    planes += 5;
}

void* TakeOffsFunction(void* param) {    
    (void)param;  // Unused parameter
    
    while (1) {
        // Use state_lock for safe access to shared variables
        pthread_mutex_lock(&state_lock);
        
        // Check if we've reached the target
        if (total_takeoffs >= TOTAL_TAKEOFFS) {
            pthread_mutex_unlock(&state_lock);
            break;
        }
        
        // Check if planes available - if not, wait
        if (planes <= 0) {
            pthread_mutex_unlock(&state_lock);
            usleep(100000);  // Wait 100ms before checking again
            continue;
        }
        
        pthread_mutex_unlock(&state_lock);
        
        // Attempt to acquire one of the two runways using trylock
        pthread_mutex_t* acquired_runway = NULL;
        
        // Try to get a runway
        while (acquired_runway == NULL) {
            // Check if we should exit while waiting for runway
            pthread_mutex_lock(&state_lock);
            if (total_takeoffs >= TOTAL_TAKEOFFS) {
                pthread_mutex_unlock(&state_lock);
                return NULL;
            }
            pthread_mutex_unlock(&state_lock);
            
            // Try to acquire runway1 or runway2
            if (pthread_mutex_trylock(&runway1_lock) == 0) {
                acquired_runway = &runway1_lock;
            } else if (pthread_mutex_trylock(&runway2_lock) == 0) {
                acquired_runway = &runway2_lock;
            } else {
                usleep(10000);  // Wait 10ms before retrying
            }
        }
        
        // Enter critical section protected by state_lock
        pthread_mutex_lock(&state_lock);
        
        // Double-check conditions after acquiring runway
        if (total_takeoffs >= TOTAL_TAKEOFFS) {
            pthread_mutex_unlock(&state_lock);
            pthread_mutex_unlock(acquired_runway);
            break;
        }
        
        if (planes <= 0) {
            pthread_mutex_unlock(&state_lock);
            pthread_mutex_unlock(acquired_runway);
            continue;
        }
        
        // Perform takeoff - update shared variables
        planes--;
        takeoffs++;
        total_takeoffs++;
        
        // Send SIGUSR1 every 5 local takeoffs
        if (takeoffs >= 5) {
            if (radio_control_pid > 0) {
                kill(radio_control_pid, SIGUSR1);
            }
            takeoffs = 0;
        }
        
        // Release the state_lock
        pthread_mutex_unlock(&state_lock);
        
        // Simulate the time a takeoff takes with sleep(1)
        sleep(1);
        
        // Release the runway
        pthread_mutex_unlock(acquired_runway);
    }
    
    return NULL;
}