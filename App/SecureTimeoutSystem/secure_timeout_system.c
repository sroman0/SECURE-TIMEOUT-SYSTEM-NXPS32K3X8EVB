/* FreeRTOS includes */
#include "FreeRTOS.h"
#include "task.h"

/* Application includes */
#include "secure_timeout_system.h"
#include "globals.h"

/* Peripheral includes */
#include "uart.h"
#include "IntTimer.h"
#include "printf-stdarg.h"

/* MPU includes */
// #include "mpu_wrappers.h" /* Uncomment this line to include MPU wrappers */

/* Task priorities */
#define MONITOR_TASK_PRIORITY (tskIDLE_PRIORITY + 2)
#define ALERT_TASK_PRIORITY   (tskIDLE_PRIORITY + 3)
#define EVENT_TASK_PRIORITY   (tskIDLE_PRIORITY + 4)

/* Task functions */
static void vMonitorTask(void *pvParameters);
static void vAlertTask(void *pvParameters);
static void vEventTask(void *pvParameters);

/* Global variables */
volatile int userActivity = 0;
volatile int userActivityDetection = 0;
volatile int suspiciousActivity = 0;
volatile int suspiciousActivityDetection = 0;

/* Activity Detection counters */
static int userADCount = 0;
static int suspiciousADCount = 0;

/* Seed used to generate pseudo random numbers */
static uint32_t seed = 14536;

/* Custom function to generate a pseudo-random number */
uint32_t simpleRandom() 
{
    /* Linear Congruential Generator (LCG) parameters */
    seed = (seed * 1664525 + 1013904223); // Modulus is implicitly 2^32
    return seed;
}

void initSecureTimeoutSystem( void ) 
{
    userActivity = 0;
    userActivityDetection = 0;
    suspiciousActivity = 0;
    suspiciousActivityDetection = 0;
}

/*--------------------------------------------------------------------------------*/

void vStartSecureTimeoutSystem( my_bool verbose ) 
{
    /* Initialise the secure timeout system */
    initSecureTimeoutSystem();

    /* Hardware initialisation */
    vInitialiseTimers( verbose );

    /* Create the tasks */
    xTaskCreate(vMonitorTask, "MonitorTask", configMINIMAL_STACK_SIZE, NULL, MONITOR_TASK_PRIORITY, NULL);
    xTaskCreate(vAlertTask,   "AlertTask",   configMINIMAL_STACK_SIZE, NULL, ALERT_TASK_PRIORITY,   NULL);
    xTaskCreate(vEventTask,   "EventTask",   configMINIMAL_STACK_SIZE, NULL, EVENT_TASK_PRIORITY,   NULL);  
}

static void vMonitorTask(void *pvParameters) 
{
    (void) pvParameters;

    vTaskDelay(pdMS_TO_TICKS(500));

    for (;;) 
    {
        if (userActivityDetection == 1) 
        {
            userActivityDetection = 0;
            printf("[USER MONITOR] Activity detected              | Status: ACTIVE\n");
            /* Possible extra implementation */
        } 
        else 
        {
            printf("[USER MONITOR] No activity                    | Status: IDLE\n");
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static void vAlertTask(void *pvParameters) 
{
    (void) pvParameters;

    vTaskDelay(pdMS_TO_TICKS(500));

    for (;;) 
    {
        if (suspiciousActivityDetection == 1) 
        {
            suspiciousActivityDetection = 0;
            printf("[SECURITY ALERT] Suspicious activity detected | Status: ALARM\n");
            printf("[SECURITY ALERT] Initiating security protocols...\n");
            /* Possible extra implementation */
        } 
        else 
        {
            printf("[SECURITY ALERT] System secure                | Status: NORMAL\n");
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static void vEventTask(void *pvParameters) 
{
    (void) pvParameters;

    for (;;) 
    {
        printf("\n[EVENT SIMULATOR] ------ New Cycle Started -------------------\n");
                  
        /* Reset Activities */
        userActivity = 0;
        suspiciousActivity = 0;

        if (simpleRandom() % 2 == 1) 
        {
            userActivity = 1;
            userADCount++;
            printf("[EVENT SIMULATOR] Generated: User Activity    | Count: %d\n\n", userADCount);
        } 
        else 
        {
            suspiciousActivity = 1;
            suspiciousADCount++;
            printf("[EVENT SIMULATOR] Generated: Security Event   | Count: %d\n\n", suspiciousADCount);
        }

        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
