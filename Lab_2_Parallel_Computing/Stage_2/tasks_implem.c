#include <stdio.h>

#include "tasks_implem.h"
#include "tasks_queue.h"
#include "debug.h"

#include <pthread.h> // Include pthread library for thread handling
#include <stdbool.h>

tasks_queue_t *tqueue= NULL;

void create_queues(void)
{
    tqueue = create_tasks_queue();
}

void delete_queues(void)
{
    free_tasks_queue(tqueue);
}    


void* thread_start_routine(void* arg) {
    task_t* active_task;

    while (1) {

        // If there is no active task, then it terminates
        if (is_tasks_queue_empty_wrapper()) { // queue empty!

            // Acquire the count mutex to update the thread count
            pthread_mutex_lock(&sys_state.count_mutex);
            sys_state.active_threads_count--; // I am not active!

            if(sys_state.active_threads_count==0){
            pthread_cond_signal(&sys_state.terminate_cv); // signal task_waitall() -> Program Termination!
            }
            pthread_cond_wait(&sys_state.task_available_cv, &sys_state.count_mutex); // Wait for new tasks
            sys_state.active_threads_count++; // If there is a task -> I am active again!
            pthread_mutex_unlock(&sys_state.count_mutex);

            continue; // Compete for the queue_mutex again ! 
        }

        active_task = get_task_to_execute(); 

        task_return_value_t ret = exec_task(active_task);

        if (ret == TASK_COMPLETED) {
            terminate_task(active_task);
        }
#ifdef WITH_DEPENDENCIES
        else {
            active_task->status = WAITING;
        }
#endif
    }

    return NULL;
}


void create_thread_pool(void)
{
    pthread_t thread_pool[sys_state.THREAD_POOL_SIZE];

    for (int i = 0; i < sys_state.THREAD_POOL_SIZE; ++i) {
        pthread_create(&thread_pool[i], NULL, thread_start_routine, NULL);
    }
}

void dispatch_task(task_t *t)
{
    enqueue_task(tqueue, t);


}

task_t* get_task_to_execute(void)
{
    return dequeue_task(tqueue);
}

unsigned int exec_task(task_t *t)
{
    t->step++;
    t->status = RUNNING;

    PRINT_DEBUG(10, "Execution of task %u (step %u)\n", t->task_id, t->step);
    
    unsigned int result = t->fct(t, t->step);
    
    switch (result) {
        case TASK_COMPLETED:
            if (t->task_dependency_count == 0) {
                terminate_task(t); // No dependencies, mark the task as terminated
            }
#ifdef WITH_DEPENDENCIES
            else {
                t->status = WAITING; // Set status to WAITING when dependencies exist
            }
#endif
            break;
#ifdef WITH_DEPENDENCIES
        case TASK_TO_BE_RESUMED:
            t->status = WAITING; // Set status to WAITING for a task that needs to be resumed later
            break;
#endif
        default:
            // Handle other cases or errors accordingly
            break;
    }

    return result;
}

void terminate_task(task_t *t)
{
    t->status = TERMINATED;
    
    PRINT_DEBUG(10, "Task terminated: %u\n", t->task_id);

#ifdef WITH_DEPENDENCIES
    if(t->parent_task != NULL){
        task_t *waiting_task = t->parent_task;
        waiting_task->task_dependency_done++;
        
        task_check_runnable(waiting_task);
    }
#endif

}

void task_check_runnable(task_t *t) {
#ifdef WITH_DEPENDENCIES
    pthread_mutex_lock(&t->tstate.dependencies_mutex);
    if(t->task_dependency_done == t->task_dependency_count){
        t->status = READY;
        dispatch_task(t);
    } else {
        t->status = WAITING; // If there are dependencies pending, set the status to WAITING
    }
    pthread_mutex_unlock(&t->tstate.dependencies_mutex);
#endif
}

int is_tasks_queue_empty_wrapper() {
    return is_tasks_queue_empty(tqueue);
}
