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
//writer

static int received = 0;

void stop_process (int sig){
  if (sig == SIGUSR1){
      received = 1;
    }
}

int main (int argc, char **argv){
  sem_t *main_sem;
  mqd_t read_queue,write_queue;
  struct timespec sl_time = {5,0};
  struct mq_attr attr_for_mq;
  int i, test_msg = 28, num_of_writers = 1;
  pid_t *child_pid;

  if(argc > 1){
    num_of_writers = atoi(argv[1]);
  }

  child_pid = malloc(num_of_writers*sizeof(pid_t));
  
  attr_for_mq.mq_maxmsg = NUM_OF_PAGES;
  attr_for_mq.mq_msgsize = sizeof(int);

  shm_unlink(MEM_SEMS);
  sem_unlink(MAIN_SEM);
  mq_unlink(READ_QUEUE);
  mq_unlink(WRITE_QUEUE);
  
  if((main_sem = sem_open(MAIN_SEM, O_CREAT | O_EXCL, 0644, 1)) == SEM_FAILED){
    printf("Error sem_open: %s\n", strerror(errno));
    return 0;
  }

  sem_wait(main_sem);
  printf("Allocating message queues!\n");

  if((read_queue = mq_open(READ_QUEUE, O_CREAT | O_EXCL | O_WRONLY, 0644, &attr_for_mq)) == -1){
    printf("Error mq_open read_q: %s\n", strerror(errno));
    sem_post(main_sem);
    return 0;
  }
  
  if((write_queue = mq_open(WRITE_QUEUE, O_CREAT | O_EXCL | O_RDWR, 0644, &attr_for_mq)) == -1){
    printf("Error mq_open write_q: %s\n", strerror(errno));
    sem_post(main_sem);
    return 0;
  }

  printf("Creating primary write queue for %d pages!\n", NUM_OF_PAGES);
  for (i = 0; i < NUM_OF_PAGES; i++){
    if(mq_send(write_queue, (char*)&i, sizeof(int), 1) == -1){
      printf("Error mq_send: %s\n", strerror(errno));
      sem_post(main_sem);
      return 0;
    }
  }

  printf("Creating sharing semaphores for %d pages!\n", NUM_OF_PAGES);

  int fd = shm_open(MEM_SEMS, O_CREAT | O_RDWR, 0666);
  if(fd == -1){
    printf("Error shm_open: %s\n", strerror(errno));
    sem_post(main_sem);
    return 0;
  }
  if(ftruncate(fd, NUM_OF_PAGES*sizeof(sem_t)) == -1){
    printf("Error ftruncate: %s\n", strerror(errno));
    sem_post(main_sem);
    return 0;
  }
  sem_t *sem = mmap(NULL, NUM_OF_PAGES*sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

  if (sem == MAP_FAILED){
    printf("Error mmap: %s\n", strerror(errno));
    sem_post(main_sem);
    return 0;
  }
  
  
  for (i = 0; i < NUM_OF_PAGES; i++){
    sem_init(&sem[i], 1, 1);
  }
  
  sem_post(main_sem);
  signal(SIGUSR1,stop_process);
  pid_t pid;
  for (i = 0; i < num_of_writers; i++){
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
    struct timespec work_time = {2,0}, time_meas;
    char log_name[255];
    sprintf(log_name, "writer-%d.log", getpid());
    FILE* log = fopen(log_name, "w+");
    while(!received){
      rc = mq_receive(write_queue, (char*)&lock_id, sizeof(int), &pri);
      clock_gettime(CLOCK_REALTIME, &time_meas);
      fprintf(log, "PID %d, waiting to write page #%d, time: %ld \n", getpid(), lock_id, time_meas.tv_sec*1000000000 + time_meas.tv_nsec);
      sem_wait(&sem[lock_id]);
      clock_gettime(CLOCK_REALTIME, &time_meas);
      fprintf(log, "PID %d, writing to page #%d, time:  %ld\n", getpid(), lock_id, time_meas.tv_sec*1000000000 + time_meas.tv_nsec);
      nanosleep(&work_time, NULL);
      sem_post(&sem[lock_id]);
      clock_gettime(CLOCK_REALTIME, &time_meas);
      fprintf(log, "PID %d, realising page #%d, time: %ld \n", getpid(), lock_id, time_meas.tv_sec*1000000000 + time_meas.tv_nsec);
      mq_send(read_queue, (char*)&lock_id, sizeof(int), 1);
    }
    fclose(log);
    return 0;
    
  }else{

    //mq_send(read_queue, (char*)&test_msg, sizeof(int), 1);

  }
  
  //nanosleep(&sl_time, NULL);
   sleep(14);
   for (i = 0; i<num_of_writers; i++)
    kill(child_pid[i], SIGUSR1);

   sem_unlink(MAIN_SEM);
  shm_unlink(MEM_SEMS);
  mq_unlink(READ_QUEUE);
  mq_unlink(WRITE_QUEUE);
  // return 0;
}
