// Header Files
#include "Include/TypeDefines.h"
#include "Include/TimerMgrHeader.h"
#include "Include/TimerAPI.h"
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <time.h>

/*****************************************************
 * Global Variables
 *****************************************************
 */
// Timer Pool Global Variables
INT8U FreeTmrCount = 0;
RTOS_TMR *FreeTmrListPtr = NULL;

// Tick Counter
INT32U RTOSTmrTickCtr = 0;

// Hash Table
HASH_OBJ hash_table[HASH_TABLE_SIZE];

// Thread variable for Timer Task
pthread_t thread;

// Semaphore for Signaling the Timer Task
sem_t timer_task_sem;

// Mutex for Protecting Hash Table
pthread_mutex_t hash_table_mutex;

// Mutex for Protecting Timer Pool
pthread_mutex_t timer_pool_mutex;

/*****************************************************
 * Timer API Functions
 *****************************************************
 */

// Function to create a Timer
RTOS_TMR* RTOSTmrCreate(INT32U delay, INT32U period, INT8U option,
                        RTOS_TMR_CALLBACK callback, void *callback_arg, INT8 *name, INT8U *err)
{
    RTOS_TMR *timer_obj = NULL;
    *err = RTOS_ERR_NONE;
    // Check the input Arguments for ERROR
    if(delay < 1){
        //cant be zero as it wont go to stopped state
        *err = RTOS_ERR_TMR_INVALID_DLY;
        return NULL;
    }
    if(option == RTOS_TMR_PERIODIC && period < 1){
        *err = RTOS_ERR_TMR_INVALID_PERIOD;
        return NULL;
    }
    if(option != RTOS_TMR_PERIODIC && option != RTOS_TMR_ONE_SHOT){
        *err = RTOS_ERR_TMR_INVALID_OPT;
        return NULL;
    }

    // Allocate a New Timer Obj
    timer_obj = alloc_timer_obj();

    if(timer_obj == NULL) {
        //No timers left in free pool
        *err = RTOS_ERR_TMR_NON_AVAIL;
        return NULL;
    }


    // Fill up the Timer Object
    timer_obj->RTOSTmrType = RTOS_TMR_TYPE;
    timer_obj->RTOSTmrCallback = callback;
    timer_obj->RTOSTmrCallbackArg = callback_arg;
    timer_obj->RTOSTmrNext = NULL;
    timer_obj->RTOSTmrPrev = NULL;
    timer_obj->RTOSTmrMatch = 0;
    timer_obj->RTOSTmrDelay = delay;
    timer_obj->RTOSTmrPeriod = period;
    timer_obj->RTOSTmrName = name;
    timer_obj->RTOSTmrOpt = option;
    timer_obj->RTOSTmrState = RTOS_TMR_STATE_STOPPED;

    *err = RTOS_SUCCESS;
    return timer_obj;
}

// Function to Delete a Timer
INT8U RTOSTmrDel(RTOS_TMR *ptmr, INT8U *perr)
{
    // ERROR Checking
    *perr = RTOS_ERR_NONE;
    if(ptmr == NULL){
        *perr = RTOS_ERR_TMR_INVALID;
        return RTOS_FALSE;
    }
    if(ptmr->RTOSTmrType != RTOS_TMR_TYPE){
        *perr = RTOS_ERR_TMR_INVALID_TYPE;
        return RTOS_FALSE;
    }

    if(ptmr->RTOSTmrState != RTOS_TMR_STATE_STOPPED && ptmr->RTOSTmrState != RTOS_TMR_STATE_RUNNING && ptmr->RTOSTmrState != RTOS_TMR_STATE_COMPLETED){
        if(ptmr->RTOSTmrState == RTOS_TMR_STATE_UNUSED){
            *perr = RTOS_ERR_TMR_INACTIVE;
            return RTOS_FALSE;
        }
        *perr = RTOS_ERR_TMR_INVALID_STATE;
        return RTOS_FALSE;
    }
    // Free Timer Object according to its State
    remove_hash_entry(ptmr);
    free_timer_obj(ptmr);

    *perr = RTOS_SUCCESS;
    return RTOS_TRUE;
}

// Function to get the Name of a Timer
INT8* RTOSTmrNameGet(RTOS_TMR *ptmr, INT8U *perr)
{
    // ERROR Checking

    if(ptmr == NULL){
        *perr = RTOS_ERR_TMR_INVALID;
        return NULL;
    }
    if(ptmr -> RTOSTmrType != RTOS_TMR_TYPE){
        *perr = RTOS_ERR_TMR_INVALID_TYPE;
        return NULL;
    }
    if(ptmr->RTOSTmrState != RTOS_TMR_STATE_STOPPED && ptmr->RTOSTmrState != RTOS_TMR_STATE_RUNNING && ptmr->RTOSTmrState != RTOS_TMR_STATE_COMPLETED){
        if(ptmr -> RTOSTmrState == RTOS_TMR_STATE_UNUSED){
            *perr = RTOS_ERR_TMR_INACTIVE;
            return NULL;
        }
        *perr = RTOS_ERR_TMR_INVALID_STATE;
        return NULL;
    }
    *perr = RTOS_ERR_NONE;
    // Return the Pointer to the String
    return ptmr->RTOSTmrName;
}

// To Get the Number of ticks remaining in time out
INT32U RTOSTmrRemainGet(RTOS_TMR *ptmr, INT8U *perr)
{
    // ERROR Checking
    if(ptmr == NULL){
        *perr = RTOS_ERR_TMR_INVALID;
        return RTOS_FALSE;
    }
    if(ptmr->RTOSTmrType != RTOS_TMR_TYPE){
        *perr = RTOS_ERR_TMR_INVALID_TYPE;
        return RTOS_FALSE;
    }
    if(ptmr->RTOSTmrState != RTOS_TMR_STATE_STOPPED && ptmr->RTOSTmrState != RTOS_TMR_STATE_RUNNING && ptmr->RTOSTmrState != RTOS_TMR_STATE_COMPLETED){
        if(ptmr -> RTOSTmrState == RTOS_TMR_STATE_UNUSED){
            *perr = RTOS_ERR_TMR_INACTIVE;
            return RTOS_FALSE;
        }
        *perr = RTOS_ERR_TMR_INVALID_STATE;
        return RTOS_FALSE;
    }
    *perr = RTOS_ERR_NONE;
    // Return the remaining ticks
    return ((ptmr->RTOSTmrMatch) - RTOSTmrTickCtr);
}

// To Get the state of the Timer
INT8U RTOSTmrStateGet(RTOS_TMR *ptmr, INT8U *perr)
{
    // ERROR Checking
    if(ptmr == NULL){
        *perr = RTOS_ERR_TMR_INVALID;
        return RTOS_FALSE;
    }
    if(ptmr->RTOSTmrType != RTOS_TMR_TYPE){
        *perr = RTOS_ERR_TMR_INVALID_TYPE;
        return RTOS_FALSE;
    }
    if(ptmr->RTOSTmrState != RTOS_TMR_STATE_STOPPED && ptmr->RTOSTmrState != RTOS_TMR_STATE_RUNNING && ptmr->RTOSTmrState != RTOS_TMR_STATE_COMPLETED){
        if(ptmr -> RTOSTmrState == RTOS_TMR_STATE_UNUSED){
            *perr = RTOS_ERR_TMR_INACTIVE;
            return RTOS_FALSE;
        }
        *perr = RTOS_ERR_TMR_INVALID_STATE;
        return RTOS_FALSE;
    }
    // Return State
    *perr = RTOS_ERR_NONE;
    return ptmr->RTOSTmrState;
}

// Function to start a Timer
INT8U RTOSTmrStart(RTOS_TMR *ptmr, INT8U *perr)
{
    // ERROR Checking
    *perr = RTOS_ERR_NONE;
    if(ptmr == NULL){
        *perr = RTOS_ERR_TMR_INVALID;
        return RTOS_FALSE;
    }
    if(ptmr->RTOSTmrType != RTOS_TMR_TYPE){
        *perr = RTOS_ERR_TMR_INVALID_TYPE;
        return RTOS_FALSE;
    }

    if(ptmr->RTOSTmrState != RTOS_TMR_STATE_STOPPED && ptmr->RTOSTmrState != RTOS_TMR_STATE_RUNNING && ptmr->RTOSTmrState != RTOS_TMR_STATE_COMPLETED){
        if(ptmr -> RTOSTmrState == RTOS_TMR_STATE_UNUSED){
            *perr = RTOS_ERR_TMR_INACTIVE;
            return RTOS_FALSE;
        }
        *perr = RTOS_ERR_TMR_INVALID_STATE;
        return RTOS_FALSE;
    }
    // Based on the Timer State, update the RTOSTmrMatch using RTOSTmrTickCtr, RTOSTmrDelay and RTOSTmrPeriod
    // You may use the Hash Table to Insert the Running Timer Obj
    if(ptmr->RTOSTmrOpt == RTOS_TMR_ONE_SHOT)
        ptmr->RTOSTmrMatch = RTOSTmrTickCtr + ptmr->RTOSTmrDelay;

    else if(ptmr->RTOSTmrOpt == RTOS_TMR_PERIODIC && ptmr->RTOSTmrState == RTOS_TMR_STATE_STOPPED)
        ptmr->RTOSTmrMatch = RTOSTmrTickCtr + ptmr->RTOSTmrDelay;

    else if(ptmr->RTOSTmrOpt == RTOS_TMR_PERIODIC && ptmr->RTOSTmrState == RTOS_TMR_STATE_COMPLETED)
        ptmr->RTOSTmrMatch = RTOSTmrTickCtr + ptmr->RTOSTmrPeriod;

    else if(ptmr->RTOSTmrOpt == RTOS_TMR_PERIODIC && ptmr->RTOSTmrState == RTOS_TMR_STATE_RUNNING) {
        remove_hash_entry(ptmr);
        ptmr->RTOSTmrMatch = RTOSTmrTickCtr + ptmr->RTOSTmrPeriod;

    }
    else{
        *perr = RTOS_ERR_TMR_INVALID;
        return RTOS_FALSE;
    }

    ptmr->RTOSTmrState = RTOS_TMR_STATE_RUNNING;
    insert_hash_entry(ptmr);
    return RTOS_TRUE;
}

// Function to Stop the Timer
INT8U RTOSTmrStop(RTOS_TMR *ptmr, INT8U opt, void *callback_arg, INT8U *perr)
{
    // ERROR Checking
    *perr = RTOS_ERR_NONE;

    if(ptmr == NULL){
        *perr = RTOS_ERR_TMR_INVALID;
        return RTOS_FALSE;
    }
    if(ptmr->RTOSTmrType != RTOS_TMR_TYPE){
        *perr = RTOS_ERR_TMR_INVALID_TYPE;
        return RTOS_FALSE;
    }

    if(ptmr->RTOSTmrState != RTOS_TMR_STATE_STOPPED && ptmr->RTOSTmrState != RTOS_TMR_STATE_RUNNING && ptmr->RTOSTmrState != RTOS_TMR_STATE_COMPLETED){
        if(ptmr->RTOSTmrState == RTOS_TMR_STATE_UNUSED){
            *perr = RTOS_ERR_TMR_INACTIVE;
            return RTOS_FALSE;
        }
        *perr = RTOS_ERR_TMR_INVALID_STATE;
        return RTOS_FALSE;
    }
    if(opt != RTOS_TMR_OPT_NONE && opt != RTOS_TMR_OPT_CALLBACK && opt != RTOS_TMR_OPT_CALLBACK_ARG){
        *perr = RTOS_ERR_TMR_INVALID_OPT;
        return RTOS_FALSE;
    }
    if(ptmr->RTOSTmrState == RTOS_TMR_STATE_STOPPED){
        *perr = RTOS_ERR_TMR_STOPPED;
        return RTOS_FALSE;
    }
    if(ptmr->RTOSTmrCallback == NULL){
        *perr = RTOS_ERR_TMR_NO_CALLBACK;
    }
    // Remove the Timer from the Hash Table List
    remove_hash_entry(ptmr);

    // Change the State to Stopped
    ptmr->RTOSTmrState = RTOS_TMR_STATE_STOPPED;
    // Call the Callback function if required
    if(opt == RTOS_TMR_OPT_CALLBACK){
        ptmr->RTOSTmrCallback(ptmr->RTOSTmrCallbackArg);
    }
    if(opt == RTOS_TMR_OPT_CALLBACK_ARG){
        ptmr->RTOSTmrCallback(callback_arg);
    }
    return RTOS_TRUE;
}

// Function called when OS Tick Interrupt Occurs which will signal the RTOSTmrTask() to update the Timers
void RTOSTmrSignal(int signum)
{
    // Received the OS Tick
    // Send the Signal to Timer Task using the Semaphore
    sem_post(&timer_task_sem);
}

/*****************************************************
 * Internal Functions
 *****************************************************
 */

// Create Pool of Timers
INT8U Create_Timer_Pool(INT32U timer_count)
{
    // Create the Timer pool using Dynamic Memory Allocation
    // You can imagine of LinkedList Creation for Timer Obj

    for(INT32U i = 0; i < timer_count; i++){
        RTOS_TMR* new_timer = (RTOS_TMR*)malloc(sizeof(RTOS_TMR));
        new_timer->RTOSTmrState = RTOS_TMR_STATE_UNUSED;
        new_timer->RTOSTmrType = RTOS_TMR_TYPE;
        new_timer->RTOSTmrNext = FreeTmrListPtr;
        new_timer->RTOSTmrPrev = NULL;
        if ((FreeTmrListPtr) != NULL)
            (FreeTmrListPtr)->RTOSTmrPrev = new_timer;
        FreeTmrListPtr = new_timer;
        FreeTmrCount++;
    }

    return RTOS_SUCCESS;
}

INT32U hashCode(RTOS_TMR* key) {
    return (key->RTOSTmrMatch) % HASH_TABLE_SIZE;
}

// Initialize the Hash Table
void init_hash_table(void)
{
    for(INT8 i = 0; i < HASH_TABLE_SIZE; i++){
        hash_table[i].list_ptr = NULL;
        hash_table[i].timer_count = 0;
    }

}

// Insert a Timer Object in the Hash Table
void insert_hash_entry(RTOS_TMR *timer_obj)
{
    // Calculate the index using Hash Function
    INT32U index = hashCode(timer_obj);
    // Lock the Resources
    pthread_mutex_lock(&hash_table_mutex);
    // Add the Entry
    timer_obj->RTOSTmrNext = hash_table[index].list_ptr;
    timer_obj->RTOSTmrPrev = NULL;

    if(hash_table[index].list_ptr != NULL)
        hash_table[index].list_ptr->RTOSTmrPrev = timer_obj;
    hash_table[index].list_ptr = timer_obj;
    hash_table[index].timer_count++;

    // Unlock the Resources
    pthread_mutex_unlock(&hash_table_mutex);

}

// Remove the Timer Object entry from the Hash Table
void remove_hash_entry(RTOS_TMR *timer_obj)
{
    // Calculate the index using Hash Function
    INT32U index = hashCode(timer_obj);
    // Lock the Resources
    pthread_mutex_lock(&hash_table_mutex);
    // Remove the Timer Obj
    if (hash_table[index].list_ptr == NULL || timer_obj == NULL)
        return;

    if (hash_table[index].list_ptr == timer_obj) {
        //if its the head
        hash_table[index].list_ptr = timer_obj->RTOSTmrNext;
        if(hash_table[index].list_ptr != NULL)
            hash_table[index].list_ptr->RTOSTmrPrev = NULL;
        //remove timer_obj
        timer_obj->RTOSTmrNext = NULL;
        timer_obj->RTOSTmrPrev = NULL;
        hash_table[index].timer_count--;
    }
    else {
        //if its not the head
        RTOS_TMR *temp = hash_table[index].list_ptr->RTOSTmrNext;
        while (temp != NULL) {
            if (temp == timer_obj) {
                temp->RTOSTmrPrev->RTOSTmrNext = temp->RTOSTmrNext;
                if(temp->RTOSTmrNext)
                    temp->RTOSTmrNext->RTOSTmrPrev = temp->RTOSTmrPrev;
                //remove temp
                temp->RTOSTmrNext = NULL;
                temp->RTOSTmrPrev = NULL;
                hash_table[index].timer_count--;
            }

            temp = temp->RTOSTmrNext;
        }
    }


    // Unlock the Resources
    pthread_mutex_unlock(&hash_table_mutex);
}

// Timer Task to Manage the Running Timers
void *RTOSTmrTask(void* temp)
{
    INT8U *perr;
    RTOS_TMR *task_timer;
    while(1) {
        // Wait for the signal from RTOSTmrSignal()
        sem_wait(&timer_task_sem);
        // Once got the signal, Increment the Timer Tick Counter
        RTOSTmrTickCtr++;
        // Check the whole List associated with the index of the Hash Table

        // Compare each obj of linked list for Timer Completion
        // If the Timer is completed, Call its Callback Function, Remove the entry from Hash table
        // If the Timer is Periodic then again insert it in the hash table
        // Change the State
        INT32U index = RTOSTmrTickCtr % HASH_TABLE_SIZE;
        task_timer = hash_table[index].list_ptr;
        while(task_timer != NULL){
            if(RTOSTmrRemainGet(task_timer,perr) == 0){
                RTOSTmrStop(task_timer, RTOS_TMR_OPT_CALLBACK, NULL, perr);
                task_timer->RTOSTmrState = RTOS_TMR_STATE_COMPLETED;
                if(task_timer->RTOSTmrOpt == RTOS_TMR_PERIODIC)
                    RTOSTmrStart(task_timer, perr);
                if(task_timer->RTOSTmrOpt == RTOS_TMR_ONE_SHOT)
                    RTOSTmrDel(task_timer, perr);
            }
            task_timer = task_timer->RTOSTmrNext;
        }
    }

}

// Timer Initialization Function
void RTOSTmrInit(void)
{
    INT32U timer_count = 0;
    INT8	retVal;
    pthread_attr_t attr;

    fprintf(stdout,"\n\nPlease Enter the number of Timers required in the Pool for the OS ");
    scanf("%d", &timer_count);

    // Create Timer Pool
    retVal = Create_Timer_Pool(timer_count);

    // Check the return Value
    if(retVal != RTOS_SUCCESS){
        return;
    }
    // Init Hash Table
    init_hash_table();

    fprintf(stdout, "\n\nHash Table Initialized Successfully\n");

    // Initialize Semaphore for Timer Task
    retVal = sem_init(&timer_task_sem,0,0);
    if(retVal == -1){
        perror("Error: ");
        exit(RTOS_ERR_TSK_SEM_INIT_FAILED);
    }

    // Initialize Mutex if any
    retVal = pthread_mutex_init(&hash_table_mutex, NULL);
    if(retVal != 0){
        perror("Error: ");
        exit(RTOS_ERR_MUTEX_INIT_FAILED);
    }
    retVal = pthread_mutex_init(&timer_pool_mutex, NULL);
    if(retVal != 0){
        perror("Error: ");
        exit(RTOS_ERR_MUTEX_INIT_FAILED);
    }
    // Create any Thread if required for Timer Task
    retVal = pthread_create(&thread, NULL, RTOSTmrTask, NULL);
    if(retVal != 0)
        perror("pthread_create");


    fprintf(stdout,"\nRTOS Initialization Done...\n");
}

// Allocate a timer object from free timer pool
RTOS_TMR* alloc_timer_obj(void)
{

    // Lock the Resources
    pthread_mutex_lock(&timer_pool_mutex);
    // Check for Availability of Timers
    if(FreeTmrCount == 0) {
        pthread_mutex_unlock(&timer_pool_mutex);
        printf("couldnt allocate a timer object");
        return NULL;
    }
    // Assign the Timer Object
    RTOS_TMR* temp = FreeTmrListPtr;

    FreeTmrListPtr = FreeTmrListPtr->RTOSTmrNext;
    if(FreeTmrListPtr)
        FreeTmrListPtr -> RTOSTmrPrev = NULL;
    FreeTmrCount--;

    // Unlock the Resources
    pthread_mutex_unlock(&timer_pool_mutex);
    return temp;
}

// Free the allocated timer object and put it back into free pool
void free_timer_obj(RTOS_TMR *ptmr)
{
    // Lock the Resources
    pthread_mutex_lock(&timer_pool_mutex);
    // Clear the Timer Fields
    ptmr -> RTOSTmrPeriod = 0;
    ptmr -> RTOSTmrDelay = 0;
    // Change the State
    ptmr -> RTOSTmrState = RTOS_TMR_STATE_UNUSED;
    // Return the Timer to Free Timer Pool
    ptmr -> RTOSTmrNext = FreeTmrListPtr;
    if(FreeTmrListPtr != NULL)
        FreeTmrListPtr -> RTOSTmrPrev = ptmr;
    FreeTmrListPtr = ptmr;
    FreeTmrCount++;
    // Unlock the Resources
    pthread_mutex_unlock(&timer_pool_mutex);
}

// Function to Setup the Timer of Linux which will provide the Clock Tick Interrupt to the Timer Manager Module
void OSTickInitialize(void) {
    timer_t timer_id;
    struct itimerspec time_value;

    // Setup the time of the OS Tick as 100 ms after 3 sec of Initial Delay
    time_value.it_interval.tv_sec = 0;
    time_value.it_interval.tv_nsec = RTOS_CFG_TMR_TASK_RATE;

    time_value.it_value.tv_sec = 0;
    time_value.it_value.tv_nsec = RTOS_CFG_TMR_TASK_RATE;

    // Change the Action of SIGALRM to call a function RTOSTmrSignal()
    signal(SIGALRM, &RTOSTmrSignal);

    // Create the Timer Object
    timer_create(CLOCK_REALTIME, NULL, &timer_id);

    // Start the Timer
    timer_settime(timer_id, 0, &time_value, NULL);
}
