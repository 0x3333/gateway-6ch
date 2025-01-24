#include <stdio.h>

#include <FreeRTOS.h>
#include <task.h>
#include <inttypes.h>

#include "res_usage.h"

#define STATS_TICKS pdMS_TO_TICKS(5000)
#define TASKS_ARRAY_SIZE 20

#if (SHOW_CPU_USAGE == 1 || SHOW_HEAP_USAGE == 1)
#warning This Resource Usage code has some issues, the code can crash !Use only if really necessary.

static void task_res(void *arg)
{
    (void)arg;

    while (true)
    {
#if SHOW_CPU_USAGE == 1
        static TaskStatus_t start_array[TASKS_ARRAY_SIZE], end_array[TASKS_ARRAY_SIZE];
        static configRUN_TIME_COUNTER_TYPE start_run_time, end_run_time;

        // Get current task states
        if (uxTaskGetSystemState(start_array, TASKS_ARRAY_SIZE, &start_run_time) == 0)
        {
            printf("Failed CPU Usage Start!\n");
            continue;
        }

        vTaskDelay(STATS_TICKS);

        // Get post delay task states
        if (uxTaskGetSystemState(end_array, TASKS_ARRAY_SIZE, &end_run_time) == 0)
        {
            printf("Failed CPU Usage End!\n");
            continue;
        }

        // Calculate total_elapsed_time in units of run time stats clock period.
        uint32_t total_elapsed_time = (end_run_time - start_run_time);
        if (total_elapsed_time == 0)
        {
            printf("Failed CPU Usage Time!\n");
            continue;
        }

        printf("| %-20s | %8s | %10s | %20s\n", "        Task        ", "Run Time", "Percentage", "Stack High Water Mark");
        printf("| %-20s | %8s | %10s | %20s\n", "--------------------", "--------", "----------", "---------------------");
        // Match each task in start_array to those in the end_array
        for (UBaseType_t i = 0; i < TASKS_ARRAY_SIZE; i++)
        {
            int k = -1;
            uint32_t highWaterMark;
            for (UBaseType_t j = 0; j < TASKS_ARRAY_SIZE; j++)
            {
                if (start_array[i].xHandle != NULL && start_array[i].xHandle == end_array[j].xHandle)
                {
                    k = j;
                    highWaterMark = uxTaskGetStackHighWaterMark(start_array[i].xHandle);
                    // Mark that task have been matched by overwriting their handles
                    start_array[i].xHandle = NULL;
                    end_array[j].xHandle = NULL;
                    break;
                }
            }
            // Check if matching task found
            if (k >= 0)
            {
                uint32_t task_elapsed_time = end_array[k].ulRunTimeCounter - start_array[i].ulRunTimeCounter;
                uint32_t percentage_time = (task_elapsed_time * 100UL) / (total_elapsed_time);
                printf("| %-20s | %8" PRIu32 " | %9" PRIu32 "%% | %-20" PRIu32 " | \n",
                       start_array[i].pcTaskName, task_elapsed_time, percentage_time, highWaterMark);
            }
        }

        // Print unmatched tasks
        for (UBaseType_t i = 0; i < TASKS_ARRAY_SIZE; i++)
        {
            if (start_array[i].xHandle != NULL)
            {
                printf("| %-20s | Deleted\n", start_array[i].pcTaskName);
            }
        }
        for (UBaseType_t i = 0; i < TASKS_ARRAY_SIZE; i++)
        {
            if (end_array[i].xHandle != NULL)
            {
                printf("| %-20s | Created\n", end_array[i].pcTaskName);
            }
        }
        printf("\n");
#endif
#if SHOW_HEAP_USAGE == 1
#if SHOW_CPU_USAGE != 1
        vTaskDelay(STATS_TICKS);
#endif
        static HeapStats_t heap_stat;

        // Get Heap stats
        vPortGetHeapStats(&heap_stat);

        printf("           Heap memory: %d (%dK)\n", configTOTAL_HEAP_SIZE, configTOTAL_HEAP_SIZE >> 10);
        printf(" Available bytes total: %d (%dK)\n", heap_stat.xAvailableHeapSpaceInBytes, heap_stat.xAvailableHeapSpaceInBytes >> 10);
        printf("         Largets block: %d (%dK)\n", heap_stat.xSizeOfLargestFreeBlockInBytes, heap_stat.xSizeOfLargestFreeBlockInBytes >> 10);
        printf("        Smallest block: %d (%dK)\n", heap_stat.xSizeOfSmallestFreeBlockInBytes, heap_stat.xSizeOfSmallestFreeBlockInBytes >> 10);
        printf("           Free blocks: %d\n", heap_stat.xNumberOfFreeBlocks);
        printf("    Min free remaining: %d (%dK)\n", heap_stat.xMinimumEverFreeBytesRemaining, heap_stat.xMinimumEverFreeBytesRemaining >> 10);
        printf("           Allocations: %d\n", heap_stat.xNumberOfSuccessfulAllocations);
        printf("                 Frees: %d\n", heap_stat.xNumberOfSuccessfulFrees);
#endif
    }
}
#endif

void res_usage_init(void)
{
#if (SHOW_CPU_USAGE == 1 || SHOW_HEAP_USAGE == 1)
    xTaskCreate(task_res, "Res Stats", configMINIMAL_STACK_SIZE * 2, NULL, tskIDLE_PRIORITY + 2, NULL);
#endif
}
