#define _POSIX_C_SOURCE 200809L
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

#include "functions.h"

#define SH_MEMORY_NAME "shm_pids_"
#define N_ELEM 3
#define BLOCK_SIZE (N_ELEM * sizeof(int))
#define TOTAL_TAKEOFFS 20

int planes = 0;
int takeoffs = 0;
int total_takeoffs = 0;
int* array_mmap = NULL;
int fd = -1;
pid_t air_control_pid = 0;
pid_t radio_control_pid = 0;

pthread_mutex_t state_lock, runway1_lock, runway2_lock;
pthread_t th1, th2, th3, th4, th5;

int main() {
  // TODO(diego.perez16) 1: Call the function that creates the shared memory
  // segment.
  MemoryCreate();

  // TODO(carlospepin23) 3: Configure the SIGUSR2 signal to increment the
  // planes on the runway by 5.
  struct sigaction sa3;
  // SIGUSR2 handler
  sa3.sa_handler = SigHandler2;
  sigaction(SIGUSR2, &sa3, NULL);

  // TODO(diego.perez16) 4: Launch the 'radio' executable and, once launched,
  // store its PID in the second position of the shared memory block.

  air_control_pid = getpid();
  array_mmap[0] = air_control_pid;
  radio_control_pid = fork();

  if (radio_control_pid == 0) {
    radio_control_pid = getpid();
    array_mmap[1] = radio_control_pid;

    execlp("./radio", "radio", SH_MEMORY_NAME, NULL);
    perror("exec failed");
    _exit(1);
  }

  // TODO(carlospepin23) 6: Launch 5 threads which will be the controllers;
  // each thread will execute the TakeOffsFunction().

  pthread_mutex_init(&state_lock, NULL);
  pthread_mutex_init(&runway1_lock, NULL);
  pthread_mutex_init(&runway2_lock, NULL);

  int create = pthread_create(&th1, NULL, TakeOffsFunction, NULL);
  int create2 = pthread_create(&th2, NULL, TakeOffsFunction, NULL);
  int create3 = pthread_create(&th3, NULL, TakeOffsFunction, NULL);
  int create4 = pthread_create(&th4, NULL, TakeOffsFunction, NULL);
  int create5 = pthread_create(&th5, NULL, TakeOffsFunction, NULL);
  if (create == -1 || create2 == -1 || create3 == -1 || create4 == -1 ||
      create5 == -1) {
    perror("error pthreade create: ");
  }

  pthread_join(th1, NULL);
  pthread_join(th2, NULL);
  pthread_join(th3, NULL);
  pthread_join(th4, NULL);
  pthread_join(th5, NULL);

  usleep(500000);

  pthread_mutex_destroy(&state_lock);
  pthread_mutex_destroy(&runway1_lock);
  pthread_mutex_destroy(&runway2_lock);

  kill(radio_control_pid, SIGTERM);
  waitpid(radio_control_pid, NULL, 0);

  shm_unlink(SH_MEMORY_NAME);
  return EXIT_SUCCESS;
}
