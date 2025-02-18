#include <stdio.h>

#include <FreeRTOS.h>
#include <task.h>
#include <inttypes.h>
#include "macrologger.h"

#include "res_usage.h"
#include "config.h"

#if (SHOW_CPU_USAGE == 1 || SHOW_HEAP_USAGE == 1)
#warning This Resource Usage code has some issues, the code can crash !Use only if really necessary.

_Noreturn static void task_res(void *arg)
{
    (void)arg;

    static const size_t task_size = RES_USAGE_TASKS_ARRAY_SIZE;

    for (;;) // Task infinite loop
    {
#if SHOW_CPU_USAGE == 1
        static TaskStatus_t start_array[task_size], end_array[task_size];
        static configRUN_TIME_COUNTER_TYPE start_run_time, end_run_time;

        // Get current task states
        if (uxTaskGetSystemState(start_array, task_size, &start_run_time) == 0)
        {
            LOG_ERROR("Failed CPU Usage Start!");
            continue;
        }

        vTaskDelay(pdMS_TO_TICKS(RES_USAGE_TATS_TICKS));

        // Get post delay task states
        if (uxTaskGetSystemState(end_array, task_size, &end_run_time) == 0)
        {
            LOG_ERROR("Failed CPU Usage End!");
            continue;
        }

        // Calculate total_elapsed_time in units of run time stats clock period.
        uint32_t total_elapsed_time = (end_run_time - start_run_time);
        if (total_elapsed_time == 0)
        {
            LOG_ERROR("Failed CPU Usage Time!");
            continue;
        }

        LOG_INFO("| %-20s | %8s | %10s | %20s", "        Task        ", "Run Time", "Percentage", "Stack High Water Mark");
        LOG_INFO("| %-20s | %8s | %10s | %20s", "--------------------", "--------", "----------", "---------------------");
        // Match each task in start_array to those in the end_array
        for (UBaseType_t i = 0; i < task_size; i++)
        {
            int k = -1;
            uint32_t highWaterMark;
            for (UBaseType_t j = 0; j < task_size; j++)
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
                LOG_INFO("| %-20s | %8" PRIu32 " | %9" PRIu32 "%% | %-20" PRIu32 " | \n",
                         start_array[i].pcTaskName, task_elapsed_time, percentage_time, highWaterMark);
            }
        }

        // Print unmatched tasks
        for (UBaseType_t i = 0; i < task_size; i++)
        {
            if (start_array[i].xHandle != NULL)
            {
                LOG_INFO("| %-20s | Deleted", start_array[i].pcTaskName);
            }
        }
        for (UBaseType_t i = 0; i < task_size; i++)
        {
            if (end_array[i].xHandle != NULL)
            {
                LOG_INFO("| %-20s | Created", end_array[i].pcTaskName);
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

        LOG_INFO("           Heap memory: %d (%dK)", configTOTAL_HEAP_SIZE, configTOTAL_HEAP_SIZE >> 10);
        LOG_INFO(" Available bytes total: %d (%dK)", heap_stat.xAvailableHeapSpaceInBytes, heap_stat.xAvailableHeapSpaceInBytes >> 10);
        LOG_INFO("         Largets block: %d (%dK)", heap_stat.xSizeOfLargestFreeBlockInBytes, heap_stat.xSizeOfLargestFreeBlockInBytes >> 10);
        LOG_INFO("        Smallest block: %d (%dK)", heap_stat.xSizeOfSmallestFreeBlockInBytes, heap_stat.xSizeOfSmallestFreeBlockInBytes >> 10);
        LOG_INFO("           Free blocks: %d", heap_stat.xNumberOfFreeBlocks);
        LOG_INFO("    Min free remaining: %d (%dK)", heap_stat.xMinimumEverFreeBytesRemaining, heap_stat.xMinimumEverFreeBytesRemaining >> 10);
        LOG_INFO("           Allocations: %d", heap_stat.xNumberOfSuccessfulAllocations);
        LOG_INFO("                 Frees: %d", heap_stat.xNumberOfSuccessfulFrees);
#endif
    }
}
#endif

void res_usage_init(void)
{
#if (SHOW_CPU_USAGE == 1 || SHOW_HEAP_USAGE == 1)
    LOG_DEBUG("Initializing Resource Usage");

    xTaskCreateAffinitySet(task_res,
                           "Res Stats",
                           configMINIMAL_STACK_SIZE * 2,
                           NULL,
                           tskIDLE_PRIORITY + 1,
                           RESOURCE_USAGE_TASK_CORE_AFFINITY,
                           NULL);
#endif
}
