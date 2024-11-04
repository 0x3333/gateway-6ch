#include <stdio.h>

#include <FreeRTOS.h>
#include <task.h>
#include <inttypes.h>

#include "cpu_usage.h"

#define STATS_TICKS pdMS_TO_TICKS(5000)
#define ARRAY_SIZE_OFFSET 10

static void task_cpu(void *arg);

void init_cpu_usage_task(void)
{
#if defined(DEBUG_BUILD) || defined(SHOW_CPU_USAGE)
    xTaskCreate(task_cpu, "stats", 2048, NULL, tskIDLE_PRIORITY + 2, NULL);
#endif
}

static void task_cpu(void *arg)
{
    (void)arg;

    while (true)
    {
        TaskStatus_t *start_array = NULL, *end_array = NULL;
        UBaseType_t start_array_size, end_array_size;
        configRUN_TIME_COUNTER_TYPE start_run_time, end_run_time;
        HeapStats_t *heap_stat = NULL;

        // Allocate array to store current task states
        start_array_size = uxTaskGetNumberOfTasks() + ARRAY_SIZE_OFFSET;
        start_array = pvPortMalloc(sizeof(TaskStatus_t) * start_array_size);
        if (start_array == NULL)
        {
            goto exit;
        }
        // Get current task states
        start_array_size = uxTaskGetSystemState(start_array, start_array_size, &start_run_time);
        if (start_array_size == 0)
        {
            goto exit;
        }

        vTaskDelay(STATS_TICKS);

        // Allocate array to store tasks states post delay
        end_array_size = uxTaskGetNumberOfTasks() + ARRAY_SIZE_OFFSET;
        end_array = pvPortMalloc(sizeof(TaskStatus_t) * end_array_size);
        if (end_array == NULL)
        {
            goto exit;
        }
        // Get post delay task states
        end_array_size = uxTaskGetSystemState(end_array, end_array_size, &end_run_time);
        if (end_array_size == 0)
        {
            goto exit;
        }

        // Calculate total_elapsed_time in units of run time stats clock period.
        uint32_t total_elapsed_time = (end_run_time - start_run_time);
        if (total_elapsed_time == 0)
        {
            goto exit;
        }

        printf("| %-10s | %8s | %10s\n", "Task", "Run Time", "Percentage");
        printf("| %-10s | %8s | %10s\n", "----------", "--------", "----------");
        // Match each task in start_array to those in the end_array
        for (UBaseType_t i = 0; i < start_array_size; i++)
        {
            int k = -1;
            for (UBaseType_t j = 0; j < end_array_size; j++)
            {
                if (start_array[i].xHandle == end_array[j].xHandle)
                {
                    k = j;
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
                uint32_t percentage_time = (task_elapsed_time * 200UL) / (total_elapsed_time * configNUMBER_OF_CORES);
                printf("| %-10s | %8" PRIu32 " | %9" PRIu32 "%%\n", start_array[i].pcTaskName, task_elapsed_time, percentage_time);
            }
        }

        // Print unmatched tasks
        for (UBaseType_t i = 0; i < start_array_size; i++)
        {
            if (start_array[i].xHandle != NULL)
            {
                printf("| %-10s | Deleted\n", start_array[i].pcTaskName);
            }
        }
        for (UBaseType_t i = 0; i < end_array_size; i++)
        {
            if (end_array[i].xHandle != NULL)
            {
                printf("| %-10s | Created\n", end_array[i].pcTaskName);
            }
        }
        printf("\n");

        pvPortMalloc(sizeof(HeapStats_t));
        vPortGetHeapStats(heap_stat);
        printf("Heap memory: %d (%dK)\n"
               " available bytes total: %d (%dK)\n"
               "         largets block: %d (%dK)\n",
               configTOTAL_HEAP_SIZE, configTOTAL_HEAP_SIZE >> 10,
               heap_stat->xAvailableHeapSpaceInBytes, heap_stat->xAvailableHeapSpaceInBytes >> 10,
               heap_stat->xSizeOfLargestFreeBlockInBytes, heap_stat->xSizeOfLargestFreeBlockInBytes >> 10);
        printf("        smallest block: %d (%dK)\n"
               "           free blocks: %d\n"
               "    min free remaining: %d (%dK)\n"
               "           allocations: %d\n"
               "                 frees: %d\n",
               heap_stat->xSizeOfSmallestFreeBlockInBytes, heap_stat->xSizeOfSmallestFreeBlockInBytes >> 10,
               heap_stat->xNumberOfFreeBlocks,
               heap_stat->xMinimumEverFreeBytesRemaining, heap_stat->xMinimumEverFreeBytesRemaining >> 10,
               heap_stat->xNumberOfSuccessfulAllocations, heap_stat->xNumberOfSuccessfulFrees);

    exit: // Common return path
        vPortFree(start_array);
        vPortFree(end_array);
        vPortFree(heap_stat);
    }
}
