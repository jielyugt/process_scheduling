
/*
 * student.c
 * Multithreaded OS Simulation for CS 2200
 *
 * This file contains the CPU scheduler for the simulation.
 */

#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#include <string.h>

#include "os-sim.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"

/** Function prototypes **/
extern void idle(unsigned int cpu_id);
extern void preempt(unsigned int cpu_id);
extern void yield(unsigned int cpu_id);
extern void terminate(unsigned int cpu_id);
extern void wake_up(pcb_t *process);


/*
 * current[] is an array of pointers to the currently running processes.
 * There is one array element corresponding to each CPU in the simulation.
 *
 * current[] should be updated by schedule() each time a process is scheduled
 * on a CPU.  Since the current[] array is accessed by multiple threads, you
 * will need to use a mutex to protect it.  current_mutex has been provided
 * for your use.
 */
static pcb_t **current;
static pthread_mutex_t current_mutex;



// static variables I added

static int algo;            // 1 = FIFO, 2 = RR, 3 = SRTF
static int rr_time_slice;

static pcb_t *ready;
static pthread_mutex_t ready_mutex;
pthread_cond_t ready_added;

static unsigned int srtf_cpu_count;



/*
 * schedule() is your CPU scheduler.  It should perform the following tasks:
 *
 *   1. Select and remove a runnable process from your ready queue which 
 *	you will have to implement with a linked list or something of the sort.
 *
 *   2. Set the process state to RUNNING
 *
 *   3. Set the currently running process using the current array
 *
 *   4. Call context_switch(), to tell the simulator which process to execute
 *      next on the CPU.  If no process is runnable, call context_switch()
 *      with a pointer to NULL to select the idle process.
 *
 *	The current array (see above) is how you access the currently running process indexed by the cpu id. 
 *	See above for full description.
 *	context_switch() is prototyped in os-sim.h. Look there for more information 
 *	about it and its parameters.
 */
static void schedule(unsigned int cpu_id)
{
    // printf("+++ +++ in schedule\n");
    // Select and remove a runnable process, and set the process state to RUNNING
    pcb_t *candidate = NULL;
    pthread_mutex_lock(&ready_mutex);


    if (ready != NULL) {
        if (algo == 3) {                                // STRF
            
            pcb_t *curr = ready;

            unsigned int min_time = curr -> time_remaining;
            int min_time_index = 0;
            pcb_t *min_time_pcb = curr;
            int index_counter = 0;

            while (curr != NULL) {
                // printf("+++ SCHEDULER looks at process %s at index %d with remaining time %u\n", curr -> name, index_counter, curr -> time_remaining);
                if (curr -> time_remaining < min_time) {
                    min_time = curr -> time_remaining;
                    min_time_index = index_counter;
                    min_time_pcb = curr;
                }
                curr = curr -> next;
                index_counter++;
            }

            // print ready queue
            if (ready == NULL) {
                // printf("\n?????? [Previous READY QUEUE: NULL] ??????\n\n");
            } else {
                // printf("\n??????[Previous READY QUEUE: ");
                pcb_t *temp = ready;
                while (temp != NULL) {
                    // printf(" %s =>", temp -> name);
                    temp = temp -> next;
                }
                // printf(" NULL] ??????\n\n");
            }

            candidate = min_time_pcb;
            // printf("+++ SCHEDULER chose %s at position %d\n", min_time_pcb -> name, min_time_index);

            if (min_time_index == 0) {
                ready = candidate -> next;
            } else {
                pcb_t *iterator = ready;
                for (int i = 0; i < min_time_index - 1; i++) {
                    iterator = iterator -> next;
                }
                iterator -> next = candidate -> next;
                // printf("%s had next set to %ld\n", iterator -> name, (long) candidate -> next);

            }

            // print ready queue
            if (ready == NULL) {
                // printf("\n?????? [READY QUEUE: NULL] ??????\n\n");
            } else {
                // printf("\n??????[READY QUEUE: ");
                pcb_t *temp = ready;
                while (temp != NULL) {
                    // printf(" %s =>", temp -> name);
                    temp = temp -> next;
                }
                // printf(" NULL] ??????\n\n");
            }
            

        } else {                                        // FIFO or RR
            candidate = ready;
            ready = ready -> next;
        }

        candidate -> state = PROCESS_RUNNING;
        candidate -> next = NULL;
    }
    
    pthread_mutex_unlock(&ready_mutex);

    // Set the currently running process
    pthread_mutex_lock(&current_mutex);
    current[cpu_id] = candidate;
    pthread_mutex_unlock(&current_mutex);

    // Call context_switch()
    if ((algo == 2) && (candidate != NULL)) {
        context_switch(cpu_id, candidate, rr_time_slice);
    } else {
        context_switch(cpu_id, candidate, -1);
    }
}


/*
 * idle() is your idle process.  It is called by the simulator when the idle
 * process is scheduled.
 *
 * This function should block until a process is added to your ready queue.
 * It should then call schedule() to select the process to run on the CPU.
 */
extern void idle(unsigned int cpu_id)
{

    /*
     * REMOVE THE LINE BELOW AFTER IMPLEMENTING IDLE()
     *
     * idle() must block when the ready queue is empty, or else the CPU threads
     * will spin in a loop.  Until a ready queue is implemented, we'll put the
     * thread to sleep to keep it from consuming 100% of the CPU time.  Once
     * you implement a proper idle() function using a condition variable,
     * remove the call to mt_safe_usleep() below.
     */
    // printf("+++ +++ in idle \n");

    pthread_mutex_lock(&ready_mutex);
    if (ready == NULL) {
        pthread_cond_wait(&ready_added, &ready_mutex);
    }
    pthread_mutex_unlock(&ready_mutex);
    schedule(cpu_id);
}


/*
 * preempt() is the handler called by the simulator when a process is
 * preempted due to its timeslice expiring.
 *
 * This function should place the currently running process back in the
 * ready queue, and call schedule() to select a new runnable process.
 *
 * Remember to set the status of the process to the proper value.
 */
extern void preempt(unsigned int cpu_id)
{
    // printf("+++ +++ in preempt\n");
    // mark the process ready
    pthread_mutex_lock(&current_mutex);
    current[cpu_id] -> state = PROCESS_READY;

    // place the currently running process back in the ready queue
    pthread_mutex_lock(&ready_mutex);

    if (ready == NULL) {
        ready = current[cpu_id];
    } else {
        pcb_t *curr = ready;
        while (curr -> next != NULL) {
            curr = curr -> next;
        }
        curr -> next = current[cpu_id];
        // printf("+++ preempted %s is placed after %s in ready queue\n", current[cpu_id] -> name, curr -> name);
    }
    pthread_cond_signal(&ready_added);
    pthread_mutex_unlock(&ready_mutex);
    pthread_mutex_unlock(&current_mutex);

    schedule(cpu_id);
}


/*
 * yield() is the handler called by the simulator when a process yields the
 * CPU to perform an I/O request.
 *
 * It should mark the process as WAITING, then call schedule() to select
 * a new process for the CPU.
 */
extern void yield(unsigned int cpu_id)
{
    // printf("+++ +++ in yield\n");
    // printf("+++ +++ %s yielded CPU %u for I/O\n", current[cpu_id] -> name, cpu_id);
    // mark the process as WAITING
    pthread_mutex_lock(&current_mutex);
    current[cpu_id] -> state = PROCESS_WAITING;
    pthread_mutex_unlock(&current_mutex);

    schedule(cpu_id);
}


/*
 * terminate() is the handler called by the simulator when a process completes.
 * It should mark the process as terminated, then call schedule() to select
 * a new process for the CPU.
 */
extern void terminate(unsigned int cpu_id)
{
    // printf("+++ +++ in terminate\n");
    // mark the process as terminated
    pthread_mutex_lock(&current_mutex);
    current[cpu_id] -> state = PROCESS_TERMINATED;
    pthread_mutex_unlock(&current_mutex);

    schedule(cpu_id);
}


/*
 * wake_up() is the handler called by the simulator when a process's I/O
 * request completes.  It should perform the following tasks:
 *
 *   1. Mark the process as READY, and insert it into the ready queue.
 *
 *   2. If the scheduling algorithm is SRTF, wake_up() may need
 *      to preempt the CPU with the highest remaining time left to allow it to
 *      execute the process which just woke up.  However, if any CPU is
 *      currently running idle, or all of the CPUs are running processes
 *      with a lower remaining time left than the one which just woke up, wake_up()
 *      should not preempt any CPUs.
 *	To preempt a process, use force_preempt(). Look in os-sim.h for 
 * 	its prototype and the parameters it takes in.
 */
extern void wake_up(pcb_t *process)
{
    // printf("+++ +++ in wake_up with process %s with remaining time %u\n", process -> name, process -> time_remaining);
    
    pthread_mutex_lock(&ready_mutex);

    process -> state = PROCESS_READY;

    if (ready == NULL) {
        ready = process;
    } else {
        pcb_t *curr = ready;
    
        while (curr -> next != NULL) {
            curr = curr -> next;
        }
        curr -> next = process;
    }
    
    pthread_cond_signal(&ready_added);
    pthread_mutex_unlock(&ready_mutex);

    if (algo == 3) {
        // force preempt
        int has_idle = 0;
        unsigned int highest_time = process -> time_remaining;
        unsigned int highest_cpu_id = 0;

        
        pthread_mutex_lock(&current_mutex);

        for (unsigned int i = 0; i < srtf_cpu_count; i++) {
            if (current[i] == NULL) {
                // printf("+++ CPU %u found idle, don't force preempt\n", i);
                has_idle = 1;
                break;
            } else if (current[i] -> time_remaining > highest_time) {
                // printf("+++ CPU %u is has a larger remaining time of %u\n", i, current[i] -> time_remaining);
                highest_time = current[i] -> time_remaining;
                highest_cpu_id = i;
            }

        }

        pthread_mutex_unlock(&current_mutex);

        if (!((has_idle) || (highest_time == process -> time_remaining))) {
            // printf("+++ force_preempt called in wake_up for CPU %u running %s\n", highest_cpu_id, (current[highest_cpu_id]) -> name);
            force_preempt(highest_cpu_id);
        }
    }      
}


/*
 * main() simply parses command line arguments, then calls start_simulator().
 * You will need to modify it to support the -r and -s command-line parameters.
 */
int main(int argc, char *argv[])
{
    unsigned int cpu_count;

    /*
     * Check here if the number of arguments provided is valid.
     * You will need to modify this when you add more arguments.
     */
    if ((argc < 2) || (argc > 5))
    {
        fprintf(stderr, "CS 2200 Project 4 -- Multithreaded OS Simulator\n"
            "Usage: ./os-sim <# CPUs> [ -r <time slice> | -s ]\n"
            "    Default : FIFO Scheduler\n"
            "         -r : Round-Robin Scheduler\n"
            "         -s : Shortest Remaining Time First Scheduler\n\n");
        return -1;
    }

    /* Parse the command line arguments */
    cpu_count = strtoul(argv[1], NULL, 0);


    // set the scheduling algorithm

    if (argc < 3) {                             // FIFO
        algo = 1;
    } else if (strcmp(argv[2], "-r") == 0) {    // Round Robin
        algo = 2;
        rr_time_slice = atoi(argv[3]);
    } else if (strcmp(argv[2], "-s") == 0) {                                    // SRTF
        algo = 3;
        srtf_cpu_count = cpu_count;
    } else {
        printf("Invalid Input Flags!\n");
        return -1;
    }

    // initialize mutexs and conds
    
    pthread_mutex_init(&ready_mutex, NULL);
    pthread_cond_init(&ready_added, NULL);

    /* Allocate the current[] array and its mutex */
    current = malloc(sizeof(pcb_t*) * cpu_count);
    assert(current != NULL);
    pthread_mutex_init(&current_mutex, NULL);

    /* Start the simulator in the library */
    start_simulator(cpu_count);

    return 0;
}


#pragma GCC diagnostic pop
