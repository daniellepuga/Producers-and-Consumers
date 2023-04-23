#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <semaphore.h>
#include <pthread.h>
#include "eventbuf.h"

struct eventbuf *eb;

// number of producers
int num_producers;
// number of consumers
int num_consumers;
// how many events each producer will generate
int num_events;
// how many outstanding (produced but not consumed) events there can be at a time
int num_outstanding;

sem_t *mutex;
sem_t *spaces;
sem_t *items;

// helper function provided to create a semaphore
sem_t *sem_open_temp(const char *name, int value)
{
    sem_t *sem;

    // Create the semaphore
    if ((sem = sem_open(name, O_CREAT, 0600, value)) == SEM_FAILED)
        return SEM_FAILED;

    // Unlink it so it will go away after this process exits
    if (sem_unlink(name) == -1) {
        sem_close(sem);
        return SEM_FAILED;
    }

    return sem;
}

void *producer_routine(void *arg)
{
    int *id = arg;
    
    for(int i = 0; i < num_events; i++) {   
        int event_num = *id * 100 + i;

        // waiting for room in our queueu
        sem_wait(spaces);
        // mutex for eb
        sem_wait(mutex);

        printf("P%d: adding event %d\n", *id, event_num);
        eventbuf_add(eb, event_num);

        sem_post(mutex);
        sem_post(items);
    }
    printf("P%d: exiting\n", *id);
    return NULL;
}

void *consumer_routine(void *arg)
{
    int *id = arg;

    // while true
    while(1)
    {
        // waiting for signal from item
        sem_wait(items);

        sem_wait(mutex);

        // if eb is empty and items signaled, we can quit
        if(eventbuf_empty(eb))
        {
            sem_post(mutex);
            break;
        }
        int event_num = eventbuf_get(eb);
        printf("C%d: got event %d\n", *id, event_num);

        sem_post(mutex);
        sem_post(spaces);
    }
    printf("C%d: exiting\n", *id);
    return NULL;
}

// 
int main(int argc, char *argv[]) 
{
    // THE PLAN from project 3 specifications:
    // checking for all required arguments
    if (argc != 5) {
        fprintf(stderr, "Error: We need 4 arguments\n");
    }

    // parse the command line
    num_producers = atoi(argv[1]);
    num_consumers = atoi(argv[2]);
    num_events = atoi(argv[3]);
    num_outstanding = atoi(argv[4]);

    // creating the event buffer
    eb = eventbuf_create();

    // semaphore initalization
    mutex = sem_open_temp("mutex", 1);
    items = sem_open_temp("items", 0);
    spaces = sem_open_temp("spaces", num_outstanding);

    // producer and consumer thread memory allocation
    pthread_t *prod_thread = calloc(num_producers, sizeof *prod_thread);
    int *prod_thread_id = calloc(num_producers, sizeof *prod_thread_id);

    // start correct number of producer threads
    for (int i = 0; i < num_producers; i++) {
        prod_thread_id[i] = i;
        pthread_create(prod_thread + i, NULL, producer_routine, prod_thread_id + i);
    }
    pthread_t *cons_thread = calloc(num_consumers, sizeof *cons_thread);
    int *cons_thread_id = calloc(num_consumers, sizeof *cons_thread_id);

    // start correct number of consumer threads
    for (int i = 0; i < num_consumers; i++) {
        cons_thread_id[i] = i;
        pthread_create(cons_thread + i, NULL, consumer_routine, cons_thread_id + i);
    }
    // wait on producers to complete
    for (int i = 0; i < num_producers; i++) {
        pthread_join(prod_thread[i], NULL);
    }
    // notify all the consumers that they're done
    for (int i = 0; i < num_consumers; i++) {
        sem_post(items);
    }
    // wait for all conssumer threads to complete
    for (int i = 0; i < num_consumers; i++) {
        pthread_join(cons_thread[i], NULL);
    }
    // free the event buffer
    eventbuf_free(eb);
    // sem_close(mutex);
    // sem_close(items);
    // sem_close(spaces);
}