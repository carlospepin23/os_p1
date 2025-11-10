#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "functions.h"

#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

#define SH_MEMORY_NAME "shm_pids_"
#define N_ELEM 3
#define BLOCK_SIZE (N_ELEM * sizeof(int))
#define TOTAL_TAKEOFFS 20

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

  memset(array_mmap, 0, BLOCK_SIZE);
}

void SigHandler2(int signal) {
  (void)signal;
  // Must use atomic operation - signal handlers can't use mutexes
  // and planes += 5 is not atomic (read-modify-write)
  __atomic_fetch_add(&planes, 5, __ATOMIC_SEQ_CST);
}

void* TakeOffsFunction(void* param) {
  (void)param;

  while (1) {
    pthread_mutex_lock(&state_lock);

    if (total_takeoffs >= TOTAL_TAKEOFFS) {
      pthread_mutex_unlock(&state_lock);
      break;
    }

    // Use atomic read to avoid race with signal handler
    int current_planes = __atomic_load_n(&planes, __ATOMIC_SEQ_CST);
    if (current_planes <= 0) {
      pthread_mutex_unlock(&state_lock);
      usleep(100000);
      continue;
    }

    pthread_mutex_unlock(&state_lock);

    pthread_mutex_t* acquired_runway = NULL;

    while (acquired_runway == NULL) {
      pthread_mutex_lock(&state_lock);
      if (total_takeoffs >= TOTAL_TAKEOFFS) {
        pthread_mutex_unlock(&state_lock);
        return NULL;
      }
      pthread_mutex_unlock(&state_lock);

      if (pthread_mutex_trylock(&runway1_lock) == 0) {
        acquired_runway = &runway1_lock;
      } else if (pthread_mutex_trylock(&runway2_lock) == 0) {
        acquired_runway = &runway2_lock;
      } else {
        usleep(10000);
      }
    }

    pthread_mutex_lock(&state_lock);

    if (total_takeoffs >= TOTAL_TAKEOFFS) {
      pthread_mutex_unlock(&state_lock);
      pthread_mutex_unlock(acquired_runway);
      break;
    }

    // Atomic decrement with protection against negative values
    int old_planes = __atomic_fetch_sub(&planes, 1, __ATOMIC_SEQ_CST);

    // If planes was already 0 or negative, rollback and retry
    if (old_planes <= 0) {
      __atomic_fetch_add(&planes, 1, __ATOMIC_SEQ_CST);
      pthread_mutex_unlock(&state_lock);
      pthread_mutex_unlock(acquired_runway);
      usleep(10000);
      continue;
    }

    // Successfully decremented planes, now increment takeoffs
    total_takeoffs++;
    int current_total = total_takeoffs;

    int send_signal = (current_total % 5 == 0);

    if (send_signal && radio_control_pid > 0) {
      kill(radio_control_pid, SIGUSR1);
    }

    pthread_mutex_unlock(&state_lock);

    sleep(1);

    pthread_mutex_unlock(acquired_runway);
  }

  return NULL;
}