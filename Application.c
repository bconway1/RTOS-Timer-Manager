// Application file to Demonstrate the Usage of Timer Manager
// Include Header Files
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "Include/TypeDefines.h"
#include "Include/TimerMgrHeader.h"
#include "Include/TimerAPI.h"

// Function to Print the Time
void print_time(void)
{
    time_t time_to_print;
    time(&time_to_print);
    printf("%s", ctime(&time_to_print));
}

// Function to Print the Time and Msg in Callback Function
void print_time_msg(int num)
{

    // Function to print the message for each Timer like
    // "This is Function 2 and UTC time and date: Thu Oct 27 20:53:27 2016
    printf("This is Function %d and UTC time and date: ", num);
    print_time();
}

void print_program_info(void)
{
    fprintf(stdout, "\n\n\n\nTimer Manager Project");
    fprintf(stdout, "\n=====================");

    fprintf(stdout, "\n\nCreated by: Brereton Conway");

    fprintf(stdout, "\n\n-> This Program will initialize the timers and creates Timer Task as Thread");
    fprintf(stdout, "\n-> It creates 3 Timers");
    fprintf(stdout, "\n\tTimer1 - Periodic 5 second");
    fprintf(stdout, "\n\tTimer2 - Periodic 3 second");
    fprintf(stdout, "\n\tTimer3 - One Shot 10 second");
    fprintf(stdout, "\n\nPress Enter to start the Program...\n");

    getchar();
}

void function1(void *arg)
{
    print_time_msg(1);
}

void function2(void *arg)
{
    print_time_msg(2);
}

void function3(void *arg)
{
    print_time_msg(3);
}

int main(void)
{
    INT8U err_val;

    RTOS_TMR *timer_obj1 = NULL;
    RTOS_TMR *timer_obj2 = NULL;
    RTOS_TMR *timer_obj3 = NULL;

    INT8 *timer_name[3] = {"Timer1", "Timer2", "Timer3"};

    // Display the Program Info
    print_program_info();

    // Initialize the OS Tick
    OSTickInitialize();

    fprintf(stdout, "OS Tick Initialization completed successfully");

    // Initialize the RTOS Timer
    RTOSTmrInit();

    fprintf(stdout, "\nApplication Started....... :-)\n");

    // ================================================================
    // Timer Creation
    // ================================================================

    // Create Timer1
    // Provide the required arguments in the function call
    //50 ticks = 5 secs
    timer_obj1 = RTOSTmrCreate(10, 50, RTOS_TMR_PERIODIC, function1, NULL, timer_name[0], &err_val);
    // Check the return value and determine if it created successfully or not
    if(err_val != RTOS_SUCCESS){
        printf("Timer1 Create Error: %d",err_val);
        return 0;
    }
    // Create Timer2
    // Provide the required arguments in the function call
    //30 ticks = 3 secs
    timer_obj2 = RTOSTmrCreate(10, 30, RTOS_TMR_PERIODIC, function2, NULL, timer_name[1], &err_val);
    // Check the return value and determine if it created successfully or not
    if(err_val != RTOS_SUCCESS){
        printf("Timer2 Create Error: %d",err_val);
        return 0;
    }
    // Create Timer3
    // Provide the required arguments in the function call
    //100 ticks = 10 secs
    timer_obj3 = RTOSTmrCreate(100, 0, RTOS_TMR_ONE_SHOT, function3, NULL, timer_name[2], &err_val);
    // Check the return value and determine if it created successfully or not
    if(err_val != RTOS_SUCCESS){
        printf("Timer3 Create Error: %d",err_val);
        return 0;
    }
    // ================================================================
    // Starting Timer
    // ================================================================
    // Start Timer1
    RTOSTmrStart(timer_obj1, &err_val);
    // Check the return value and determine if it started successfully or not
    if(err_val != RTOS_SUCCESS){
        printf("Timer1 Start Error: %d",err_val);
        return 0;
    }
    // Start Timer2
    RTOSTmrStart(timer_obj2, &err_val);
    // Check the return value and determine if it started successfully or not
    if(err_val != RTOS_SUCCESS){
        printf("Timer2 Start Error: %d",err_val);
        return 0;
    }
    // Start Timer3
    RTOSTmrStart(timer_obj3, &err_val);
    // Check the return value and determine if it started successfully or not
    if(err_val != RTOS_SUCCESS){
        printf("Timer3 Start Error: %d",err_val);
        return 0;
    }

    // Other Code if needed
    while(1);
    return 0;
}

