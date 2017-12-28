#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>        /* For mode constants */
#include <mqueue.h>
#include <semaphore.h>
#include <pthread.h>
#include <time.h>
#include <signal.h>
#include "methods.h"
//reader
static int received = 0;

void stop_process (int sig){
  if (sig == SIGUSR1){
      received = 1;
    }
}

int main (int argc, char **argv){
  sem_t *main_sem;
  mqd_t read_queue,write_queue;
  int i, num_of_readers = 1;
  pid_t *child_pid;
  char data[4096], *pages;
  printf("RAND_MAX = %ld\n", RAND_MAX);
  if(argc > 1){
    num_of_readers = atoi(argv[1]);
  }

  child_pid = malloc(num_of_readers*sizeof(pid_t));
  
  while((main_sem = sem_open(MAIN_SEM, 0)) == SEM_FAILED){
    //printf("Error: %s", strerror(errno));
  }

  sem_wait(main_sem);
  printf("Openning message queues!\n");
  if((read_queue = mq_open(READ_QUEUE, O_RDONLY)) == -1){
    printf("Error: %s", strerror(errno));
    sem_post(main_sem);
    return 0;
  }
  
  if((write_queue = mq_open(WRITE_QUEUE, O_WRONLY)) == -1){
    printf("Error: %s", strerror(errno));
    sem_post(main_sem);
    return 0;
  }

  printf("Openning shared semaphores for %d pages!\n", NUM_OF_PAGES);

  int fd = shm_open(MEM_SEMS, O_RDWR, 0666);
  if(fd == -1){
    printf("Error shm_open: %s\n", strerror(errno));
    sem_post(main_sem);
    return 0;
  }
 
  sem_t *sem = mmap(NULL, NUM_OF_PAGES*sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

  if (sem == MAP_FAILED){
    printf("Error mmap: %s\n", strerror(errno));
    sem_post(main_sem);
    return 0;
  }

   printf("Mapping file to shared memory!\n");
  int file = open(SHARED_FILE, O_CREAT | O_RDWR, 0666);
  if(file == -1){
    printf("Error when open shared file: %s\n", strerror(errno));
    sem_post(main_sem);
    return 0;
  }
  
  pages = mmap(NULL, NUM_OF_PAGES*4096, PROT_READ | PROT_WRITE, MAP_SHARED, file, 0);
  
  if (pages == MAP_FAILED){
    printf("Error when mmap working pages: %s\n", strerror(errno));
    sem_post(main_sem);
    return 0;
  }
  
  sem_post(main_sem);
  
  signal(SIGUSR1,stop_process);
  pid_t pid;
  for (i = 0; i < num_of_readers; i++){
    
    pid=fork();
    if (pid == -1){
      printf("Error in fork()");
      return 1;
    }
    if (pid == 0)
      break;
    
    printf("My child %d pid is: %d\n", i, pid);
    child_pid[i] = pid;

  }

  if (pid == 0){
    printf("I'm a child process! My pid is %d\n", getpid());
    unsigned pri = 1;
    int lock_id;
    ssize_t rc;
    struct timespec work_time = {0,(rand() % (1500000000) + 500000000)}, time_meas;
    char log_name[255];
    sprintf(log_name, "reader-%d.log", getpid());
    FILE* log = fopen(log_name, "w+");

    while(!received){
      rc = mq_receive(read_queue, (char*)&lock_id, sizeof(int), &pri);
      clock_gettime(CLOCK_REALTIME, &time_meas);
      fprintf(log, "PID%d %d %ld\n", getpid(), lock_id*4+1, time_meas.tv_sec*1000000000 + time_meas.tv_nsec);
      sem_wait(&sem[lock_id]);
      clock_gettime(CLOCK_REALTIME, &time_meas);
      fprintf(log, "PID%d %d %ld\n", getpid(), lock_id*4+2, time_meas.tv_sec*1000000000 + time_meas.tv_nsec);
      // fprintf(log, "PID %d, reading page #%d, time: %ld \n", getpid(), lock_id, time_meas.tv_sec*1000000000 + time_meas.tv_nsec);
      memcpy(data, pages+4096*lock_id, 4096);
      nanosleep(&work_time, NULL);
      sem_post(&sem[lock_id]);
      clock_gettime(CLOCK_REALTIME, &time_meas);
      fprintf(log, "PID%d %d %ld\n", getpid(), lock_id*4+3, time_meas.tv_sec*1000000000 + time_meas.tv_nsec);
      //fprintf(log, "PID %d, realising page #%d, time: %ld \n", getpid(), lock_id, time_meas.tv_sec*1000000000 + time_meas.tv_nsec);
      mq_send(write_queue, (char*)&lock_id, sizeof(int), 1);
    }
    
    fclose(log);
    return 0;
    
  }else{
    sleep(WAITING_CONSTANT);
    for (i = 0; i<num_of_readers; i++)
      kill(child_pid[i], SIGUSR1);
    mq_unlink(READ_QUEUE);
    mq_unlink(WRITE_QUEUE);
    return 0;
  }
}
