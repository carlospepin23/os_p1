#ifndef AIR_CONTROL_C_INCLUDE_FUNCTIONS_H_
#define AIR_CONTROL_C_INCLUDE_FUNCTIONS_H_

#include <pthread.h>
#include <sys/types.h>

// External variable declarations
extern volatile int planes;
extern int takeoffs;
extern int total_takeoffs;
extern int* array_mmap;
extern int fd;
extern pid_t air_control_pid;
extern pid_t radio_control_pid;

// Function declarations
void MemoryCreate(void);
void SigHandler2(int signal);
void* TakeOffsFunction(void* param);

#endif  // AIR_CONTROL_C_INCLUDE_FUNCTIONS_H_
