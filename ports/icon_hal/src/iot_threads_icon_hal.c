/*
 * Copyright (C) 2019 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/**
 * @file iot_threads_template.c
 * @brief Implimentation of iot threads for Icon Health and fitness.  Note that I would prefer to use the HAL,  However it does not facilitate tasks dying, as such the FreeRTOS API is  used here to implement the iot_threads api.
 */

/* The config header is always included first. */
#include "iot_config.h"
#include "iot_port_mem_helper.h"

/* Platform threads include. */
#include "platform/iot_threads.h"
#include <FreeRTOS.h>
#include <task.h>
#include <IH-Core.h>

/***********************************************************************************/
/***************************** Defines and Macros **********************************/
/***********************************************************************************/

/**
 * @brief Macro that gets the number of elements supported by the array
 */
#define ARRAY_MAX_COUNT(x) ((sizeof(x)/sizeof(0[x])) / ((size_t)(!(sizeof(x) % sizeof(0[x])))))

/* Configure logs for the functions in this file. */
#ifdef IOT_LOG_LEVEL_PLATFORM
#define LIBRARY_LOG_LEVEL        IOT_LOG_LEVEL_PLATFORM
#else
#ifdef IOT_LOG_LEVEL_GLOBAL
#define LIBRARY_LOG_LEVEL    IOT_LOG_LEVEL_GLOBAL
#else
#define LIBRARY_LOG_LEVEL    IOT_LOG_NONE
#endif
#endif

#define LIBRARY_LOG_NAME    ( "THREAD" )
#include "iot_logging_setup.h"

/**
 * @brief Macro for entering a critical section for the iot thread port
 */
#define IOT_THREAD_ENTER_CRITICAL()      portENTER_CRITICAL_SAFE(&iot_thread_master_mux);

/**
 * @brief Macro for exiting a critical section for the iot thread port
 */
#define IOT_THREAD_EXIT_CRITICAL()       portEXIT_CRITICAL_SAFE(&iot_thread_master_mux);

/**
 * @brief Helper macro for stringizing a token
 */
#define IOT_THREAD_STRING_VAL_1(X) #X

/**
 * @brief Macro that stringizes an expanded token
 */
#define IOT_THREAD_STRING_VAL(X) IOT_THREAD_STRING_VAL_1(X)

/**
 * @brief Macro that concatenates two values without expanding them
 */
#define IOT_THREAD_CAT_NX(A, B) A ## B

/**
 * @brief Macro that concatenates two tokens after expanding them
 */
#define IOT_THREAD_CAT(A, B) IOT_THREAD_CAT_NX(A, B)

/**
 * @brief Macro that creates a thread name string based on the value passed into X
 */
#define IOT_THREAD_NAME(X) IOT_THREAD_STRING_VAL(IOT_THREAD_CAT(IOT_TASK_, X))

/**
 * @brief Macro that Helps with initializing table elements
 */
#define IOT_THREAD_TABLE_INIT_ELEMENT(X) \
{ .threadRoutine=NULL, .pArgument = NULL, .priority = 0, .stackSize=0, .name= IOT_THREAD_NAME(X) }

/***********************************************************************************/
/***************************** Type Defs *******************************************/
/***********************************************************************************/

struct iot_thread_data_entry_s
{
    IotThreadRoutine_t threadRoutine; //!< The routine to run with the task
    void *pArgument; //!< The arguments to use with the task
    int32_t priority; //!< The task's priority
    size_t stackSize; //!< The task's stack size
    const char *name; //!< The name of the task
    TaskHandle_t handle; //!< The handle for the task
    StaticTask_t TaskBuffer; //!< Task Buffer used to hold the tasks TCB
    StackType_t   *Stack; //!< The stack to use with the task
}; //!< Structure that holds data for each task to create and run

struct iot_fifo_cleanup_data_s
{
    struct iot_thread_data_entry_s *entry;  //!< Pointer to the entry to clean up
}; //!< Structure used with the cleanup function called by the HAL Fifo task

/***********************************************************************************/
/***************************** Function Declarations *******************************/
/***********************************************************************************/

/***********************************************************************************/
/***************************** Static Variables ************************************/
/***********************************************************************************/

static struct iot_thread_data_entry_s iot_threads_table[15] =
{
    IOT_THREAD_TABLE_INIT_ELEMENT(1),
    IOT_THREAD_TABLE_INIT_ELEMENT(2),
    IOT_THREAD_TABLE_INIT_ELEMENT(3),
    IOT_THREAD_TABLE_INIT_ELEMENT(4),
    IOT_THREAD_TABLE_INIT_ELEMENT(5),
    IOT_THREAD_TABLE_INIT_ELEMENT(6),
    IOT_THREAD_TABLE_INIT_ELEMENT(7),
    IOT_THREAD_TABLE_INIT_ELEMENT(8),
    IOT_THREAD_TABLE_INIT_ELEMENT(9),
    IOT_THREAD_TABLE_INIT_ELEMENT(10),
    IOT_THREAD_TABLE_INIT_ELEMENT(11),
    IOT_THREAD_TABLE_INIT_ELEMENT(12),
    IOT_THREAD_TABLE_INIT_ELEMENT(13),
    IOT_THREAD_TABLE_INIT_ELEMENT(14),
    IOT_THREAD_TABLE_INIT_ELEMENT(15)
}; //!< Table that is used to keep track of the allocated tasks

static portMUX_TYPE iot_thread_master_mux = portMUX_INITIALIZER_UNLOCKED; //!< Mutex that protects the modules data

/***********************************************************************************/
/***************************** Function Definitions ********************************/
/***********************************************************************************/

/**
 * @brief Function that fetches the next available table entry.  Returns NULL on error
 * @return Pointer to the table entry, or NULL otherwise
 */
static struct iot_thread_data_entry_s *iot_threads_fetch_available_table_entry(void)
{
    struct iot_thread_data_entry_s *rv;
    rv = NULL;
    for(int i = 0; i < ARRAY_MAX_COUNT(iot_threads_table) && NULL == rv; i++)
    {
        if(NULL == iot_threads_table[i].threadRoutine)
        {
            rv = &iot_threads_table[i];
        }
    }
    return rv;
}

/**
 * @brief Function that cleans up a task as requested.
 * @param data pointer to data containing a struct iot_fifo_cleanup_data_s
 * @param data_size should be the sizeof an iot_fifo_cleanup_data_s structure
 */
static void iot_thread_cleanup_task(void *data, uint16_t data_size)
{
    struct iot_fifo_cleanup_data_s *typed;
    IH_ASSERT(IH_ERR_LEVEL_ERROR, NULL != data);
    IH_ASSERT(IH_ERR_LEVEL_ERROR, sizeof(struct iot_fifo_cleanup_data_s) == data_size);

    typed = data;

    vTaskDelete(typed->entry->handle);

    IOT_THREAD_ENTER_CRITICAL()
    iot_port_free(typed->entry->Stack);
    typed->entry->threadRoutine = NULL;
    typed->entry->pArgument = NULL;
    typed->entry->priority = 0;
    typed->entry->stackSize = 0;
    IOT_THREAD_EXIT_CRITICAL()

}

/**
 * @brief Function that runs the detached IOT Threads
 * @param table_entry pointer to the table entry in use
 */
static void iot_thread_woker(void *table_entry)
{
    struct iot_fifo_cleanup_data_s worker_data;
    worker_data.entry = table_entry;

    //Run the threads routine
    worker_data.entry->threadRoutine(worker_data.entry->pArgument);

    //The thread returned.  We will need to tell the fifo to clean us up and wait
    IH_ASSERT(IH_ERR_LEVEL_ERROR, IH_SUCCESS == ih_sched_add(iot_thread_cleanup_task, &worker_data, sizeof(worker_data)));

    //Spin Sleep so that the fifo can kill this task
    while(1)
    {
        vTaskDelay(200);
    }
}


/**
 * @brief Create a new detached thread, i.e. a thread that cleans up after itself.
 *
 * This function creates a new thread. Threads created by this function exit
 * upon returning from the thread routine. Any resources taken must be freed
 * by the exiting thread.
 *
 * @param[in] threadRoutine The function this thread should run.
 * @param[in] pArgument The argument passed to `threadRoutine`.
 * @param[in] priority Represents the priority of the new thread, as defined by
 * the system. The value #IOT_THREAD_DEFAULT_PRIORITY (i.e. `0`) must be used to
 * represent the system default for thread priority. #IOT_THREAD_IGNORE_PRIORITY
 * should be passed if this parameter is not relevant for the port implementation.
 * @param[in] stackSize Represents the stack size of the new thread, as defined
 * by the system. The value #IOT_THREAD_DEFAULT_STACK_SIZE (i.e. `0`) must be used
 * to represent the system default for stack size. #IOT_THREAD_IGNORE_STACK_SIZE
 * should be passed if this parameter is not relevant for the port implementation.
 *
 * @return `true` if the new thread was successfully created; `false` otherwise.
 *
 * @code{c}
 * // Thread routine.
 * void threadRoutine( void * pArgument );
 *
 * // Run threadRoutine in a detached thread, using default priority and stack size.
 * if( Iot_CreateDetachedThread( threadRoutine,
 *                               NULL,
 *                               IOT_THREAD_DEFAULT_PRIORITY,
 *                               IOT_THREAD_DEFAULT_STACK_SIZE ) == true )
 * {
 *     // Success
 * }
 * else
 * {
 *     // Failure, no thread was created.
 * }
 * @endcode
 */
bool Iot_CreateDetachedThread(IotThreadRoutine_t threadRoutine,
                              void *pArgument,
                              int32_t priority,
                              size_t stackSize)
{
    /* Implement this function as specified here:
     * https://docs.aws.amazon.com/freertos/latest/lib-ref/c-sdk/platform/platform_threads_function_createdetachedthread.html
     */
    struct iot_thread_data_entry_s *table_entry;

    if(NULL == threadRoutine || 150 > stackSize || tskIDLE_PRIORITY > priority ||  configMAX_PRIORITIES < priority)
    {
        return false;
    }

    IOT_THREAD_ENTER_CRITICAL()
    table_entry = iot_threads_fetch_available_table_entry();
    if(NULL != table_entry)
    {
        table_entry->threadRoutine = threadRoutine;
        table_entry->pArgument = pArgument;
        table_entry->priority = priority;
        table_entry->stackSize = stackSize;
    }
    IOT_THREAD_EXIT_CRITICAL()
    if(NULL == table_entry)
    {
        return false;
    }

    table_entry->Stack = iot_port_malloc(stackSize);
    if(NULL == table_entry->Stack)
    {
        IOT_THREAD_ENTER_CRITICAL()
        table_entry->threadRoutine = NULL;
        table_entry->pArgument = NULL;
        table_entry->priority = 0;
        table_entry->stackSize = 0;
        IOT_THREAD_EXIT_CRITICAL()
        return false;
    }

    table_entry->handle = xTaskCreateStatic(iot_thread_woker, table_entry->name, table_entry->stackSize, table_entry, table_entry->priority,
                                            table_entry->Stack, &table_entry->TaskBuffer);
    if(NULL == table_entry->handle)
    {
        IOT_THREAD_ENTER_CRITICAL()
        table_entry->threadRoutine = NULL;
        table_entry->pArgument = NULL;
        table_entry->priority = 0;
        table_entry->stackSize = 0;
        IOT_THREAD_EXIT_CRITICAL()
        return false;
    }

    return true;
}

/**
 * @brief Create a new mutex.
 *
 * This function creates a new, unlocked mutex. It must be called on an uninitialized
 * #IotMutex_t. This function must not be called on an already-initialized #IotMutex_t.
 *
 * @param[in] pNewMutex Pointer to the memory that will hold the new mutex.
 * @param[in] recursive Set to `true` to create a recursive mutex, i.e. a mutex that
 * may be locked multiple times by the same thread. If the system does not support
 * recursive mutexes, this function should do nothing and return `false`.
 *
 * @return `true` if mutex creation succeeds; `false` otherwise.
 *
 * @see @ref platform_threads_function_mutexdestroy
 *
 * <b>Example</b>
 * @code{c}
 * IotMutex_t mutex;
 *
 * // Create non-recursive mutex.
 * if( IotMutex_Create( &mutex, false ) == true )
 * {
 *     // Lock and unlock the mutex...
 *
 *     // Destroy the mutex when it's no longer needed.
 *     IotMutex_Destroy( &mutex );
 * }
 * @endcode
 */
bool IotMutex_Create(IotMutex_t *pNewMutex, bool recursive)
{
    /* Implement this function as specified here:
     * https://docs.aws.amazon.com/freertos/latest/lib-ref/c-sdk/platform/platform_threads_function_mutexcreate.html
     */
    return false;
}

/*-----------------------------------------------------------*/

void IotMutex_Destroy(IotMutex_t *pMutex)
{
    /* Implement this function as specified here:
     * https://docs.aws.amazon.com/freertos/latest/lib-ref/c-sdk/platform/platform_threads_function_mutexdestroy.html
     */
}

/*-----------------------------------------------------------*/

void IotMutex_Lock(IotMutex_t *pMutex)
{
    /* Implement this function as specified here:
     * https://docs.aws.amazon.com/freertos/latest/lib-ref/c-sdk/platform/platform_threads_function_mutexlock.html
     */
}

/*-----------------------------------------------------------*/

bool IotMutex_TryLock(IotMutex_t *pMutex)
{
    /* Implement this function as specified here:
     * https://docs.aws.amazon.com/freertos/latest/lib-ref/c-sdk/platform/platform_threads_function_mutextrylock.html
     */
    return false;
}

/*-----------------------------------------------------------*/

void IotMutex_Unlock(IotMutex_t *pMutex)
{
    /* Implement this function as specified here:
     * https://docs.aws.amazon.com/freertos/latest/lib-ref/c-sdk/platform/platform_threads_function_mutexunlock.html
     */
}

/*-----------------------------------------------------------*/

bool IotSemaphore_Create(IotSemaphore_t *pNewSemaphore,
                         uint32_t initialValue,
                         uint32_t maxValue)
{
    /* Implement this function as specified here:
     * https://docs.aws.amazon.com/freertos/latest/lib-ref/c-sdk/platform/platform_threads_function_semaphorecreate.html
     */
    return false;
}

/*-----------------------------------------------------------*/

void IotSemaphore_Destroy(IotSemaphore_t *pSemaphore)
{
    /* Implement this function as specified here:
     * https://docs.aws.amazon.com/freertos/latest/lib-ref/c-sdk/platform/platform_threads_function_semaphoredestroy.html
     */
}

/*-----------------------------------------------------------*/

uint32_t IotSemaphore_GetCount(IotSemaphore_t *pSemaphore)
{
    /* Implement this function as specified here:
     * https://docs.aws.amazon.com/freertos/latest/lib-ref/c-sdk/platform/platform_threads_function_semaphoregetcount.html
     */
    return 0;
}

/*-----------------------------------------------------------*/

void IotSemaphore_Wait(IotSemaphore_t *pSemaphore)
{
    /* Implement this function as specified here:
     * https://docs.aws.amazon.com/freertos/latest/lib-ref/c-sdk/platform/platform_threads_function_semaphorewait.html
     */
}

/*-----------------------------------------------------------*/

bool IotSemaphore_TryWait(IotSemaphore_t *pSemaphore)
{
    /* Implement this function as specified here:
     * https://docs.aws.amazon.com/freertos/latest/lib-ref/c-sdk/platform/platform_threads_function_semaphoretrywait.html
     */
    return false;
}

/*-----------------------------------------------------------*/

bool IotSemaphore_TimedWait(IotSemaphore_t *pSemaphore,
                            uint32_t timeoutMs)
{
    /* Implement this function as specified here:
     * https://docs.aws.amazon.com/freertos/latest/lib-ref/c-sdk/platform/platform_threads_function_semaphoretimedwait.html
     */
    return false;
}

/*-----------------------------------------------------------*/

void IotSemaphore_Post(IotSemaphore_t *pSemaphore)
{
    /* Implement this function as specified here:
     * https://docs.aws.amazon.com/freertos/latest/lib-ref/c-sdk/platform/platform_threads_function_semaphorepost.html
     */
}

/*-----------------------------------------------------------*/
