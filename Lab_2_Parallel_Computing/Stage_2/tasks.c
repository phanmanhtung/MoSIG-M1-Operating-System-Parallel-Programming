#include <stdlib.h>

#include "tasks.h"
#include "tasks_implem.h"
#include "debug.h"
#include "utils.h"

#include <pthread.h>

system_state_t sys_state;

__thread task_t *active_task;

void initialize_system_state() {
    sys_state.task_counter = 0;   
    sys_state.THREAD_POOL_SIZE = 10;
    sys_state.active_threads_count = sys_state.THREAD_POOL_SIZE;
    pthread_mutex_init(&sys_state.count_mutex , NULL);
    pthread_cond_init(&sys_state.terminate_cv , NULL);
    pthread_cond_init(&sys_state.task_available_cv , NULL);
}

void runtime_init(void)
{
    /* a random number generator might be useful towards the end of
       the lab */
    rand_generator_init();

    create_queues();

    initialize_system_state();

    create_thread_pool(); // Call function to create a thread pool
    
}

void runtime_init_with_deps(void)
{
#ifndef WITH_DEPENDENCIES
    fprintf(stderr, "ERROR: dependencies are not supported by the runtime. This application cannot be executed\n");
    exit(EXIT_FAILURE);
#endif

    runtime_init();
}



void runtime_finalize(void)
{
    task_waitall();

    PRINT_DEBUG(1, "Terminating ... \t Total task count: %lu \n", sys_state.task_counter);
    
    delete_queues();
}


task_t* create_task(task_routine_t f)
{
    task_t *t = malloc(sizeof(task_t));

    t->task_id = ++sys_state.task_counter;    
    t->fct = f;
    t->step = 0;

    t->tstate.input_list = NULL;
    t->tstate.output_list = NULL;

#ifdef WITH_DEPENDENCIES
    pthread_mutex_init(&t->tstate.dependencies_mutex, NULL);
    t->tstate.output_from_dependencies_list = NULL;
    t->task_dependency_count = 0;
    t->parent_task = NULL;
#endif
    
    t->status = INIT;
    
    PRINT_DEBUG(10, "task created with id %u\n", t->task_id);    
    
    return t;
}

void submit_task(task_t *t)
{
    t->status = READY;

#ifdef WITH_DEPENDENCIES    
    if(active_task != NULL){
        t->parent_task = active_task;
        active_task->task_dependency_count++;
        
        PRINT_DEBUG(100, "Dependency %u -> %u\n", active_task->task_id, t->task_id);
    }
#endif
    
    dispatch_task(t);
}

void task_waitall(void) // Synchronize all the worker threads, can be called at any time
{
    // Wait until all threads finish their tasks && There is no more task in queue !
    while (sys_state.active_threads_count > 0 || !is_tasks_queue_empty_wrapper()) {
        printf("Waiting for completion: Active threads: %d, Tasks in queue: %s\n",
               sys_state.active_threads_count, is_tasks_queue_empty_wrapper() ? "Empty" : "Non-empty");
        pthread_cond_wait(&sys_state.terminate_cv, &sys_state.count_mutex);
    }

    printf("Waitall condition is valid: No active threads, and task queue is empty.\n");
}