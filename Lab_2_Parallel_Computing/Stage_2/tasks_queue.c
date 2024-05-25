#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tasks_queue.h"
#include <pthread.h>

tasks_queue_t* create_tasks_queue(void)
{
    tasks_queue_t *q = (tasks_queue_t*) malloc(sizeof(tasks_queue_t));

    q->task_buffer_size = QUEUE_SIZE;
    q->task_buffer = (task_t**) malloc(sizeof(task_t*) * q->task_buffer_size);

    q->index = 0;
    q->size = 0;
    q->capacity = QUEUE_SIZE;

    // Initialize mutex and condition variable
    pthread_mutex_init(&q->queue_mutex, NULL);
    pthread_cond_init(&q->queue_not_full_cv, NULL);

    return q;
}


void free_tasks_queue(tasks_queue_t *q)
{
    /* IMPORTANT: We chose not to free the queues to simplify the
     * termination of the program (and make debugging less complex) */
    
    /* free(q->task_buffer); */
    /* free(q); */
}


// void enqueue_task(tasks_queue_t *q, task_t *t)
// {
//     pthread_mutex_lock(&q->queue_mutex);

//     while(q->index == q->task_buffer_size){
//         pthread_cond_wait(&q->queue_not_full_cv, &q->queue_mutex);
//     }

//     q->task_buffer[q->index] = t;
//     q->index++;

//     // Signal the condition variable to notify waiting threads at dequeue_task && thread_start_routine if-check
//     pthread_cond_signal(&sys_state.task_available_cv);

//     pthread_mutex_unlock(&q->queue_mutex);

// }

void enqueue_task(tasks_queue_t *q, task_t *t) {
    pthread_mutex_lock(&q->queue_mutex);

    if (is_queue_full(q)) {
        // Resize the queue (expand the capacity)
        int new_capacity = q->capacity * 2; // Doubling the capacity, can use other strategies
        task_t **new_task_buffer = (task_t **)realloc(q->task_buffer, new_capacity * sizeof(task_t *));
        if (new_task_buffer == NULL) {
            fprintf(stderr, "Error: Unable to resize the task queue\n");
            pthread_mutex_unlock(&q->queue_mutex);
            exit(EXIT_FAILURE);
        }

        q->task_buffer = new_task_buffer;
        q->capacity = new_capacity;
    }

    q->task_buffer[q->size] = t;
    q->size++;

    pthread_cond_signal(&sys_state.task_available_cv);

    pthread_mutex_unlock(&q->queue_mutex);
}

task_t* dequeue_task(tasks_queue_t *q)
{

    pthread_mutex_lock(&q->queue_mutex);
    
    while (q->size == 0) {
        // Wait on the condition variable until the queue is not empty
        pthread_cond_wait(&sys_state.task_available_cv, &q->queue_mutex);
    }

    task_t *t = q->task_buffer[q->size-1];
    q->size--;

    pthread_cond_signal(&q->queue_not_full_cv);

    pthread_mutex_unlock(&q->queue_mutex);

    return t;
}

int is_tasks_queue_empty(tasks_queue_t *q)
{
    return (q->index == 0);
}

int is_queue_full(tasks_queue_t *q) {
    return q->size == q->capacity;
}