/**
 * Monte Carlo - Threads exercise - CSUSM - Erwan Dupard
 *
 * Compile with: gcc ./mcarlo.c -o mcarlo -lpthread -lm 
 */
#include <unistd.h>
#include <time.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#define RETURN_SUCCESS         (0)
#define RETURN_FAILURE         (1)
#define THREAD_SUCCESS         (RETURN_SUCCESS)
#define THREAD_FAILURE         (RETURN_FAILURE)
#define SYSCALL_FAILURE        (-1)

#define PROGRESS               (1)

#define WORKER_COUNT           (4)
#define WITHIN_CIRCLE(x, y)    (sqrt(x*x + y*y) < 1)
#define RANDOM_DOUBLE          ((random() / ((double)RAND_MAX + 1)) * 2.0 - 1.0)
#define PI(a, b)               ((double)(4.0 * ((double)b / (double)a)))

typedef struct                  s_mcarlo
{
  unsigned long long            p_todo;
  unsigned long long            p_count;
  unsigned long long            p_within_circle;
  double                        pi;
}                               t_mcarlo;

typedef struct                  s_thread
{
  pthread_t                     thread_id;
  pthread_attr_t                thread_attr;
  int                           status;
  unsigned long long            computed_points;
  t_mcarlo                      *mcarlo;
}                               t_thread;

/* Worker func ptr prototype */
void                            *worker(void *arg);

/* General results structure (Shared memory) */
t_mcarlo                        mcarlo = {0, 0, 0, 0.0};

/* Creating a thread mutex to protect the access to a shared resource (avoiding dead locks ) */
pthread_mutex_t                 lock;

/* Threads array, keep the track of each thread */
t_thread                        threads[WORKER_COUNT];

int                             main(int argc, char **argv)
{
  unsigned int                  i = 0;
  long long                     point_number = 0;

  /* Checking command line parameters */
  if (argc < 2)  
  {
    fprintf(stderr, "[^] USAGE: ./mcarlo <point_number>\n");
    return RETURN_FAILURE;
  }
  /* Converting the char* argv[1] into long long */
  if ((point_number = atoll(argv[1])) < 1)
  {
    fprintf(stderr, "[-] point_number should be >= 1\n");
    return RETURN_FAILURE;
  }
  /* Initializing mutex */
  if (pthread_mutex_init(&lock, NULL) != 0)
  {
    fprintf(stderr, "[-] Failed to init mutex !\n");
    return RETURN_FAILURE;
  }
  /* Setting the random seed to the currant time in second */
  srand(time(NULL));

  /* Number of points to compute stored in the working structure */
  mcarlo.p_todo = point_number;

  /* Initializing threads, then, start them */
  printf("[~] Launching %d threads with points number %llu ..\n", WORKER_COUNT, point_number);
  for (i = 0 ; i < WORKER_COUNT ; ++i)
  {
    /* By default, the thread succeed */
    threads[i].status = THREAD_SUCCESS;
    threads[i].mcarlo = &mcarlo;
    /* Setting default thread attr */
    pthread_attr_init(&threads[i].thread_attr);
    /* Starting threads */
    if (pthread_create(&threads[i].thread_id, &threads[i].thread_attr, worker, &threads[i]) != RETURN_SUCCESS)
    {
      fprintf(stderr, "[-] Failed to start thread %d\n", i);
      threads[i].status = THREAD_FAILURE; /* Set the thread creation status to know if we have to join() it */
    }
  }

  /* Waiting for the threads to finish */
  for (i = 0 ; i < WORKER_COUNT ; ++i)
  {
    /* Waiting for thread i to finish */
    pthread_join(threads[i].thread_id, NULL);

    /* Display thread computed points */
    printf("Thread [%d] Computed Points: %llu\n", i, threads[i].computed_points);
  }

  printf("[+] All Threads finished ! %llu/%llu PI = %f\n", mcarlo.p_within_circle, mcarlo.p_count, PI(mcarlo.p_count, mcarlo.p_within_circle));
  /* Destroy the mutex, we don't using it anymore */
  pthread_mutex_destroy(&lock);
  return RETURN_SUCCESS;
}

void                            *worker(void *arg)
{
  /* Casting the void* pointer to retrieve our t_mcarlo structure */
  t_thread                      *thread = (t_thread *)arg;
  double                        x = 0, y = 0;

  while (1)
  {
    /* Creating random values (x, y coordinates) */
    x = RANDOM_DOUBLE;
    y = RANDOM_DOUBLE;

    /* Locking the mutex, accessing shared memory */
    pthread_mutex_lock(&lock);

    if (!(thread->mcarlo->p_count < thread->mcarlo->p_todo))
    {
      pthread_mutex_unlock(&lock); /* Don't forget to unlock before leaving the loop */
      break;
    }

    /* Increasing number of computed points */
    if (++thread->mcarlo->p_count && WITHIN_CIRCLE(x, y)) /* Check is the coordinates are within the circle */
      ++thread->mcarlo->p_within_circle;

    /* Computed point in THIS tread */
    ++thread->computed_points;

    /* Logging some info 10 times if we are in debug/progress mode */
    if (PROGRESS && thread->mcarlo->p_count % ((thread->mcarlo->p_todo / 10) + 1) == 0)
      printf("Progress: %llu/%llu -> %f\n",
             thread->mcarlo->p_within_circle,
             thread->mcarlo->p_count,
             PI(thread->mcarlo->p_count, thread->mcarlo->p_within_circle)
            );
    /* Unlocking the mutex, not using shared memory anymore */
    pthread_mutex_unlock(&lock);
  }
  /* Exiting at the end of the job */
  pthread_exit(NULL);
  return NULL;
}
