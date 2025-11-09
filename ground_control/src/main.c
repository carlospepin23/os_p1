#define _POSIX_C_SOURCE 200809L
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <unistd.h>

#define SH_MEMORY_NAME "shm_pids_"
#define N_ELEM 3
#define BLOCK_SIZE (N_ELEM * sizeof(int))
#define PLANES_LIMIT 20
int planes = 0;
int takeoffs = 0;
int traffic = 0;

int *array_mmap;
int fd;
pid_t ground_control_pid;

void Traffic(int signum) {
  // TODO(diego.perez16):
  // Calculate the number of waiting planes.
  int waiting = planes - takeoffs;

  // Check if there are 10 or more waiting planes to send a signal and increment
  // planes. Ensure signals are sent and planes are incremented only if the
  // total number of planes has not been exceeded.
  if (waiting >= 10) {
    printf("RUNWAY OVERLOADED\n");
  }

  if (planes < PLANES_LIMIT) {
    planes += 5;
    if (array_mmap[1] > 0) {
      kill(array_mmap[1], SIGUSR2);
    }
  }
}

void SigtermHandler(int sig) {
  munmap(array_mmap, BLOCK_SIZE);
  close(fd);
  printf("finalization of operations...\n");
  shm_unlink(SH_MEMORY_NAME);
  exit(0);
}

void Sigusr1Handler(int sig) {
  takeoffs += 5;
}

// SIGALRM handler to call Traffic
void AlarmHandler(int sig) {
  Traffic(sig);
}

int main(int argc, char *argv[]) {
  // TODO(carlospepin23):
  // 1. Open the shared memory block and store this process PID in position 2
  //    of the memory block.

  // suscribe block
  fd = shm_open(SH_MEMORY_NAME, O_RDWR, 0666);
  array_mmap = mmap(0, BLOCK_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

  if (array_mmap == MAP_FAILED) {
    perror("mmap failed");
    exit(1);
  }
  ground_control_pid = getpid();
  array_mmap[2] = ground_control_pid;  // type casting?

  // 3. Configure SIGTERM and SIGUSR1 handlers
  //    - The SIGTERM handler should: close the shared memory, print
  //      "finalization of operations..." and terminate the program.
  //    - The SIGUSR1 handler should: increase the number of takeoffs by 5.

  struct sigaction sa, sa2;

  sa.sa_handler = SigtermHandler;
  sigaction(SIGTERM, &sa, NULL);

  sa2.sa_handler = Sigusr1Handler;
  sigaction(SIGUSR1, &sa2, NULL);

  // 2. Configure the timer to execute the Traffic function.

  struct sigaction sa_alarm = {0};
  sa_alarm.sa_handler = AlarmHandler;
  sigaction(SIGALRM, &sa_alarm, NULL);

  struct itimerval timer;
  timer.it_value.tv_sec = 0;
  timer.it_value.tv_usec = 500000;
  timer.it_interval.tv_sec = 0;
  timer.it_interval.tv_usec = 500000;
  setitimer(ITIMER_REAL, &timer, NULL);

  while (1)
    pause();

  return EXIT_SUCCESS;
}