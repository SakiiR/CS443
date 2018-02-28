/**
 * Producer - Consumer 
 * Made by Erwan Dupard - CSUSM Student - ALCI
 * 2018 - CS443
 * 
 * Compile with: gcc prodcon.c -o prodcon -pthread
 */

#include <stdint.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define RETURN_SUCCESS      (0)
#define RETURN_FAILURE      (1)
#define SYSCALL_FAILED      (-1)
#define THREAD_SUCCESS      (RETURN_SUCCESS)
#define THREAD_FAILURE      (RETURN_FAILURE)
#define DEBUG               (0)
#define BLOCK_SIZE          (32)
#define CHECKSUM_SIZE       (2)
#define DATA_SIZE           (BLOCK_SIZE - CHECKSUM_SIZE)

/* Command line argument and shared memory */
typedef struct              s_prodcon
{
  int                       memsize;
  int                       ntimes;
  unsigned char             *shared_memory;
}                           t_prodcon;

/* Thread relative structure */
typedef struct              s_thread
{
  pthread_t                 thread_id;
  pthread_attr_t            thread_attr;
  struct s_prodcon          *prodcon; /* Keep the track of the program status (critical) */
  sem_t                     *semaphore;
  pthread_mutex_t           *mutex;
}                           t_thread;

/* Global data structure */
t_prodcon                   prodcon = {0, 0, NULL};

/* Consumer/Producer worker funcs */
void                        *consumer(void *arg);
void                        *producer(void *arg);

/* Consumer/Producer Thread */
t_thread                    consumer_thread;
t_thread                    producer_thread;

/* Semaphore to know which block to consume/produce */
sem_t                       semaphore;

/* Mutex to protect the critical section */
pthread_mutex_t             mutex;

/* Init the threads attr and then start them with pthread_create() call */
int                         init_threads()
{
  pthread_attr_init(&consumer_thread.thread_attr);
  pthread_attr_init(&producer_thread.thread_attr);
  consumer_thread.prodcon = &prodcon;
  producer_thread.prodcon = &prodcon;
  consumer_thread.semaphore = &semaphore;
  producer_thread.semaphore = &semaphore;
  consumer_thread.mutex = &mutex;
  producer_thread.mutex = &mutex;
  if (pthread_create(&producer_thread.thread_id,
        &producer_thread.thread_attr,
        producer,
        &producer_thread) != RETURN_SUCCESS)
  {
    fprintf(stderr, "[-] Failed to start producer thread\n");
    return RETURN_FAILURE;
  }
  if (pthread_create(&consumer_thread.thread_id,
        &consumer_thread.thread_attr,
        consumer,
        &consumer_thread) != RETURN_SUCCESS)
  {
    fprintf(stderr, "[-] Failed to start consumer thread\n");
    return RETURN_FAILURE;
  }
  return RETURN_SUCCESS;
}

/* Wait for the both threads to finish */
void                        join_threads()
{
  pthread_join(producer_thread.thread_id, NULL);
  pthread_join(consumer_thread.thread_id, NULL);
}

/**
 * Entry point
 */
int                         main(int ac, char **av)
{
  if (ac < 3)  
  {
    fprintf(stderr, "[~] Usage: %s <memsize> <ntimes>\n", av[0]);
    return RETURN_FAILURE;
  }
  /* Try to do a pseudo random generator (based on time) */
  srand(time(NULL));
  prodcon.memsize = atoi(av[1]);
  prodcon.ntimes = atoi(av[2]);
  if (prodcon.memsize <= 0 || prodcon.ntimes <= 0)
  {
    fprintf(stderr, "[-] Memsize and ntimes have to be >= 0 - %d and %d\n", prodcon.memsize, prodcon.ntimes);
    return RETURN_FAILURE;
  }
  /* If the memsize command line argument is not a multiple of BLOCK_SIZE */
  if (prodcon.memsize % BLOCK_SIZE != 0)
  {
    fprintf(stderr, "[-] Memsize %d, is not a multiple of BLOCK_SIZE\n", prodcon.memsize);
    return RETURN_FAILURE;
  }
  /* Allocating memory for our blocks buffer */
  if ((prodcon.shared_memory = malloc((sizeof(*prodcon.shared_memory) * prodcon.memsize) + 1)) == NULL)
  {
    fprintf(stderr, "[-] Malloc(%lu) failed ..\n", sizeof(*prodcon.shared_memory) * prodcon.memsize);
    return RETURN_FAILURE;
  }
  printf("[+] Options: memsize: %d, ntimes: %d\n", prodcon.memsize, prodcon.ntimes);
  /* Init counting semaphore */
  if (sem_init(&semaphore, 0, 0) == SYSCALL_FAILED)
  {
    fprintf(stderr, "[-] Failed to init semaphore\n");
    return RETURN_SUCCESS;
  }
  /* Initializing mutex */
  if (pthread_mutex_init(&mutex, NULL) != 0)
  {
    fprintf(stderr, "[-] Failed to init mutex !\n");
    return RETURN_FAILURE;
  }
  /* Initialising threads */
  if (init_threads() == RETURN_FAILURE)
    return RETURN_FAILURE;

  /* Join threads */
  join_threads();

  /* Destroy the mutex */
  if (pthread_mutex_destroy(&mutex) != RETURN_SUCCESS)
  {
    fprintf(stderr, "[-] Failed to destroy mutex\n");
    return RETURN_FAILURE;
  }
  /* Destroy the semaphore */
  if (sem_destroy(&semaphore) == SYSCALL_FAILED)
  {
    fprintf(stderr, "[-] Failed to destroy semaphore\n");
    return RETURN_FAILURE;
  }
  /* Free shared memory */
  free(prodcon.shared_memory);
  printf("[+] Success !\n");
  return RETURN_SUCCESS;
}

/**
 * Generate DATA_SIZE random byte and store it into the memory relative `block` (&memory[block * BLOCK_SIZE]).
 * Store the block pointer into `block_ptr`
 */
int                         generate_random_block(t_thread *thread, int block, unsigned char **block_ptr)
{
  unsigned char             *block_addr;    
  int                       i;

  if (block * BLOCK_SIZE >= thread->prodcon->memsize)
  {
    fprintf(stderr, "[-] producer: Block index is too high ..\n");
    return RETURN_FAILURE;
  }
  block_addr = &thread->prodcon->shared_memory[BLOCK_SIZE * block];
  for (i = 0 ; i <= DATA_SIZE ; ++i)
    block_addr[i] = rand() % 0xFF;
  *block_ptr = block_addr;
  return RETURN_SUCCESS;
}

/**
 * Generate an Internet Checksum from the 
 * given `block` and returns it.
 */
uint16_t                    generate_checksum(unsigned char *block)
{
  unsigned int              count = DATA_SIZE;
  uint16_t                  sum = 0;

  while (count > 1)
  {
    sum += *(unsigned short *)block++; 
    count -= 2;
  }
  sum += (count > 0 ? *block : 0);
  while (sum >> 16)  
    sum = (sum & 0xFFFF) + (sum >> 16);
  return ~sum;
}

/**
 * Store the 16bits value `checksum` into the two last 
 * 31 and 32 bytes of `block`.
 */
void                        store_checksum(unsigned char *block, uint16_t checksum)
{
  block[DATA_SIZE + 1] = checksum & 0x00FF;
  block[DATA_SIZE] =     (checksum >> 8) & 0x00FF;
}

/**
 * Retrieve the checksum from the last two bytes 31 and 32 of `block`
 * and returns it.
 */
uint16_t                    load_checksum(unsigned char *block)
{
  return block[DATA_SIZE + 1] | block[DATA_SIZE] << 8;
}

/**
 * Producer function. Produce data and store into the memory
 */
void                        *producer(void *arg)
{
  t_thread                  *current_thread = (t_thread *)arg;
  int                       i = 0;
  int                       block = 0;
  unsigned char             *block_ptr = NULL;
  uint16_t                  checksum = 0;
  int                       ntimes;

  /* Retrieve the ntimes number from the critical memory section to put it in the loop condition */
  pthread_mutex_lock(current_thread->mutex);
  ntimes = current_thread->prodcon->ntimes;
  pthread_mutex_unlock(current_thread->mutex);
  while (i < ntimes)
  {
    pthread_mutex_lock(current_thread->mutex);
    /* Retrieve the semaphore value to know which block to produce */
    if (sem_getvalue(current_thread->semaphore, &block) == SYSCALL_FAILED)
    {
      fprintf(stderr, "[-] sem_getvalue() Failed !\n");
      pthread_mutex_unlock(current_thread->mutex);
      return NULL;
    }
    /* Begin of critical section */
    if (block * BLOCK_SIZE >= current_thread->prodcon->memsize)
    {
      #if DEBUG
            printf("[~] Producer: block %d too high\n", block);
      #endif
      pthread_mutex_unlock(current_thread->mutex);
      continue;
    }
    /* Generate a random block into the good memory index */
    if (generate_random_block(current_thread, block, &block_ptr) == RETURN_FAILURE)
      return NULL;
    /* Generate a checksum */
    checksum = generate_checksum(block_ptr);
    /* Store the checksum into the last two bytes of block */
    store_checksum(block_ptr, checksum);
    printf("[~] Produced block %d\n", block);
    ++i;
    /* Let the consumer know we just produced one more block */
    sem_post(current_thread->semaphore);
    /* End of critical section */
    pthread_mutex_unlock(current_thread->mutex);
  }
  printf("[~] I produced enough data\n");
  fflush(stdout);
  return NULL;
}

/**
 * Consumer function. Consume data from the memory 
 * produced by the producer.
 */
void                        *consumer(void *arg)
{
  t_thread                  *current_thread = (t_thread *)arg;
  unsigned char             *block_ptr = NULL;
  uint16_t                  checksum = 0;
  int                       i = 0;
  int                       block = 0;
  int                       ntimes;

  /* Retrieve the ntimes number from the critical memory section to put it in the loop condition */
  pthread_mutex_lock(current_thread->mutex);
  ntimes = current_thread->prodcon->ntimes;
  pthread_mutex_unlock(current_thread->mutex);
  while(i < ntimes)
  {
    /* Critical secion */
    pthread_mutex_lock(current_thread->mutex);
    /* Waiting for sem > 0 */
    if (sem_trywait(current_thread->semaphore) == SYSCALL_FAILED)
    {
      #if DEBUG
        printf("[~] Can't wait .. block should be too low ..\n");
      #endif
      pthread_mutex_unlock(current_thread->mutex);
      continue;
    }
    /* Retrieve the semaphore value to know which block to consume */
    if (sem_getvalue(current_thread->semaphore, &block) == SYSCALL_FAILED)
    {
      fprintf(stderr, "[-] sem_getvalue() Failed !\n");
      pthread_mutex_unlock(current_thread->mutex);
      return NULL;
    }
    if (BLOCK_SIZE * block >= current_thread->prodcon->memsize)
    {
      #if DEBUG
            printf("[~] Consumer: block %d too high\n", block);
      #endif
      pthread_mutex_unlock(current_thread->mutex);
      continue;
    }
    /* Checksum Checks */
    block_ptr = &current_thread->prodcon->shared_memory[block * BLOCK_SIZE];
    checksum = generate_checksum(block_ptr);
    if (checksum != load_checksum(block_ptr))
      fprintf(stderr, "[-] Computed checksum is not equals to the memory checksum\n");
    printf("[~] Consumed block %d\n", block);
    /* End of critical section */
    ++i;
    pthread_mutex_unlock(current_thread->mutex);
  }
  printf("[~] I consumed enough data\n");
  fflush(stdout);
  return NULL;
}
