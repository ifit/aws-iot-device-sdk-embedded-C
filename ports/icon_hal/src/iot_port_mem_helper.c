/**
 * @file iot_port_mem_helper.c
 * @author Kade Cox
 * @date Created: Feb 25, 2020
 * @details
 * Helper file for allocating and deallocating a memory helper
 */

#include "iot_port_mem_helper.h"

#include <IH-Core.h>
#include <stdbool.h>
#include <multi_heap.h>
#include "iot_config.h"

#ifndef CONFIG_ICON_AWS_PORT_MEMPOOL_SIZE
#error "CONFIG_ICON_AWS_PORT_MEMPOOL_SIZE must be defined"
#endif //CONFIG_ICON_AWS_PORT_MEMPOOL_SIZE

#ifndef CONFIG_ICON_AWS_PORT_MEMPOOL_MAX_CHUNKS
#error "CONFIG_ICON_AWS_PORT_MEMPOOL_MAX_CHUNKS must be defined"
#endif //CONFIG_ICON_AWS_PORT_MEMPOOL_MAX_CHUNKS


/***********************************************************************************/
/***************************** Defines and Macros **********************************/
/***********************************************************************************/

//Macro that gets the number of elements supported by the array
#define ARRAY_MAX_COUNT(x) ((sizeof(x)/sizeof(0[x])) / ((size_t)(!(sizeof(x) % sizeof(0[x])))))

/**
 * @brief Macro for entering a critical section for the iot thread port
 */
#define IOT_MEM_ENTER_CRITICAL()      portENTER_CRITICAL_SAFE(&iot_mem_master_mux);

/**
 * @brief Macro for exiting a critical section for the iot thread port
 */
#define IOT_MEM_EXIT_CRITICAL()       portEXIT_CRITICAL_SAFE(&iot_mem_master_mux);

#define IOT_TASK_POOL_SIZE (CONFIG_AWS_TASK_POOL_TASK_SIZE * CONFIG_AWS_TASK_POOL_MAX_TASKS)

#if 0 != (CONFIG_ICON_AWS_PORT_MEMPOOL_SIZE % 4)
#define AWS_HEAP_SIZE (CONFIG_ICON_AWS_PORT_MEMPOOL_SIZE + (4-(CONFIG_ICON_AWS_PORT_MEMPOOL_SIZE % 4)))
#else
#define AWS_HEAP_SIZE CONFIG_ICON_AWS_PORT_MEMPOOL_SIZE
#endif //0 != (CONFIG_ICON_AWS_PORT_MEMPOOL_SIZE % 4)

/***********************************************************************************/
/***************************** Type Defs *******************************************/
/***********************************************************************************/

struct mem_helper_data_s
{
    bool initialized; //!< Tells if we are initialized
//    ih_mempool_handle_t mempool; //!< The memory pool handle
    multi_heap_handle_t mempool; //!< The Multi Heap Handle Memory Pool
    ih_mempool_handle_t taskpool; //!< The memory pool handle for the task pool
};

/***********************************************************************************/
/***************************** Function Declarations *******************************/
/***********************************************************************************/

/***********************************************************************************/
/***************************** Static Variables ************************************/
/***********************************************************************************/

static struct mem_helper_data_s mem_helper =
{
    .initialized = false,
    .mempool = NULL,
    .taskpool = NULL
}; //!< Variable that holds the module data

static portMUX_TYPE iot_mem_master_mux = portMUX_INITIALIZER_UNLOCKED; //!< Mutex that protects the modules data
static uint8_t aws_heap[AWS_HEAP_SIZE] __attribute__((aligned))  __attribute__((section(".bss.icon_hal.aws_heap")));    //!< The Heap to use with AWS


/***********************************************************************************/
/***************************** Function Definitions ********************************/
/***********************************************************************************/

/**
 * @brief Function that ensures we are initialized
 */
static void init_as_needed(void)
{
    if(false == mem_helper.initialized)
    {
        mem_helper.initialized = true;
//        mem_helper.mempool = IH_MEM_POOL_CREATE(iot_helper_pool, CONFIG_ICON_AWS_PORT_MEMPOOL_SIZE, CONFIG_ICON_AWS_PORT_MEMPOOL_MAX_CHUNKS);
        mem_helper.mempool = multi_heap_register(aws_heap, AWS_HEAP_SIZE);
        mem_helper.taskpool = IH_MEM_POOL_CREATE(iot_task_pool, IOT_TASK_POOL_SIZE, CONFIG_AWS_TASK_POOL_MAX_TASKS);
    }
    IH_ASSERT(IH_ERR_LEVEL_ERROR, NULL != mem_helper.mempool);
    IH_ASSERT(IH_ERR_LEVEL_ERROR, NULL != mem_helper.taskpool);
}

/**
 * @brief Function that allocates data
 * @param size how many bytes to allocate
 * @return returns NULL on error
 */
void *iot_port_malloc(unsigned int size)
{

    void *rv;
    unsigned int free_mem = 0;
    rv = NULL;
    IOT_MEM_ENTER_CRITICAL()
    init_as_needed();
    rv = multi_heap_malloc(mem_helper.mempool, size);
    if(NULL == rv)
    {
        free_mem = multi_heap_free_size(mem_helper.mempool);
    }
    IOT_MEM_EXIT_CRITICAL()
    if(0 != free_mem)
    {
        IH_PRINT_DEBUG_MESSAGE("Failed allocate %u bytes of memory. (%u available)\r\n", size, free_mem);
    }
    return rv;
}

/**
 * @brief Function that frees previously allocated data
 * @param ptr the ptr to free
 */
void iot_port_free(void *ptr)
{
    IOT_MEM_ENTER_CRITICAL()
    init_as_needed();
//    ih_mempool_free(mem_helper.mempool, ptr);
    multi_heap_free(mem_helper.mempool, ptr);
    IOT_MEM_EXIT_CRITICAL()
}

/**
 * @brief Function that allocates data for taskpool tasks
 * @param size how many bytes to allocate
 * @return returns NULL on error
 */
void *iot_port_taskpool_malloc(unsigned int size)
{
    void *rv;
    rv = NULL;
    IOT_MEM_ENTER_CRITICAL()
    init_as_needed();
    rv = ih_mempool_malloc(mem_helper.taskpool, size);
    IOT_MEM_EXIT_CRITICAL()

    return rv;
}

/**
 * @brief Function that frees previously allocated data for taskpool tasks
 * @param ptr the ptr to free
 */
void iot_port_taskpool_free(void *ptr)
{
    IOT_MEM_ENTER_CRITICAL()
    init_as_needed();
    ih_mempool_free(mem_helper.taskpool, ptr);
    IOT_MEM_EXIT_CRITICAL()
}
