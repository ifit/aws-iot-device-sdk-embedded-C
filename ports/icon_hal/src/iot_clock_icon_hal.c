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
 * @file iot_clock_template.c
 * @brief Template implementation of the functions in iot_clock.h
 */

/* The config header is always included first. */
#include "iot_config.h"
#include "iot_port_mem_helper.h"

/* Platform clock include. */
#include "platform/iot_clock.h"

#include <IH-Core.h>
#include <IH-Time.h>
#include <FreeRTOS.h>

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

#define LIBRARY_LOG_NAME    ( "CLOCK" )
#include "iot_logging_setup.h"

struct iot_port_timer_data_s
{
    IotThreadRoutine_t expirationRoutine;
    void *pArgument ;
    TimerHandle_t rtos_timer;
    uint32_t relativeTimeoutMs;
    uint32_t periodMs;
}; //Data used by the iot timer port

/*-----------------------------------------------------------*/

bool IotClock_GetTimestring(char *pBuffer,
                            size_t bufferSize,
                            size_t *pTimestringLength)
{
    /* Implement this function as specified here:
     * https://docs.aws.amazon.com/freertos/latest/lib-ref/c-sdk/platform/platform_clock_function_gettimestring.html
     */
    ih_time_date_object_s current_time;
    if(NULL == pBuffer || 0 >= bufferSize || NULL == pTimestringLength)
    {
        return false;
    }
    current_time = ih_time_get_time();
    //Format to YYYY/MM/DD HH:MM:SS
    pTimestringLength[0] = snprintf(pBuffer, bufferSize, "%04u/%02u/%02u %02u:%02u:%02u",
                                    current_time.Date.Year, current_time.Date.Month, current_time.Date.Day,
                                    current_time.Time.Hour, current_time.Time.Min, current_time.Time.Second);
    if(pTimestringLength[0] < bufferSize)
    {
        return true;
    }
    return false;
}

/*-----------------------------------------------------------*/

uint64_t IotClock_GetTimeMs(void)
{
    /* Implement this function as specified here:
     * https://docs.aws.amazon.com/freertos/latest/lib-ref/c-sdk/platform/platform_clock_function_gettimems.html
     */
    return 0;
}

/*-----------------------------------------------------------*/

void IotClock_SleepMs(uint32_t sleepTimeMs)
{
    /* Implement this function as specified here:
     * https://docs.aws.amazon.com/freertos/latest/lib-ref/c-sdk/platform/platform_clock_function_sleepms.html
     */
}

/*-----------------------------------------------------------*/

bool IotClock_TimerCreate(IotTimer_t *pNewTimer,
                          IotThreadRoutine_t expirationRoutine,
                          void *pArgument)
{
    struct iot_port_timer_data_s *worker;
    /* Implement this function as specified here:
     * https://docs.aws.amazon.com/freertos/latest/lib-ref/c-sdk/platform/platform_clock_function_timercreate.html
     */
    if(NULL == pNewTimer || NULL == expirationRoutine)
    {
        return false;
    }
    worker = iot_port_malloc(sizeof(struct iot_port_timer_data_s));
    if(NULL == worker)
    {
        return false;
    }
    worker->expirationRoutine = expirationRoutine;
    worker->pArgument = pArgument;
    worker->rtos_timer = NULL;
    worker->periodMs = 0;
    worker->relativeTimeoutMs = 0;
    pNewTimer[0] = worker;
    return true;
}

/*-----------------------------------------------------------*/

void IotClock_TimerDestroy(IotTimer_t *pTimer)
{
    struct iot_port_timer_data_s *worker;
    /* Implement this function as specified here:
     * https://docs.aws.amazon.com/freertos/latest/lib-ref/c-sdk/platform/platform_clock_function_timerdestroy.html
     */
    if(NULL == pTimer)
    {
        return;
    }
    worker = (struct iot_port_timer_data_s *)pTimer[0];
    if(NULL == worker)
    {
        return;
    }
    if(NULL != worker->rtos_timer)
    {
        if(xPortInIsrContext())
        {
            IH_ASSERT(IH_ERR_LEVEL_ERROR, pdPASS == xTimerDelete(worker->rtos_timer, 0));
        }
        else
        {
            IH_ASSERT(IH_ERR_LEVEL_ERROR, pdPASS == xTimerDelete(worker->rtos_timer, portMAX_DELAY));
        }
        worker->rtos_timer = NULL;
    }
    iot_port_free(worker);
}

/**
 * @brief the generic timer callback function
 * @param xTimer
 */
static void timer_callback_function(TimerHandle_t xTimer)
{
    struct iot_port_timer_data_s *worker;
    IH_ASSERT(IH_ERR_LEVEL_ERROR, false == xPortInIsrContext());
    worker = pvTimerGetTimerID(xTimer);
    IH_ASSERT(IH_ERR_LEVEL_ERROR, NULL != worker);

    worker->relativeTimeoutMs = 0;
    if(0 != worker->periodMs)
    {
        IH_ASSERT(IH_ERR_LEVEL_ERROR, pdPASS == xTimerChangePeriod(worker->rtos_timer, worker->periodMs, portMAX_DELAY));
        IH_ASSERT(IH_ERR_LEVEL_ERROR, pdPASS == xTimerStart(worker->rtos_timer, portMAX_DELAY));
    }
    //TODO Need to run the task here:
    IH_ASSERT_NOT_IMPLEMENTED;
}
/*-----------------------------------------------------------*/

bool IotClock_TimerArm(IotTimer_t *pTimer,
                       uint32_t relativeTimeoutMs,
                       uint32_t periodMs)
{
    struct iot_port_timer_data_s *worker;
    uint32_t initial_period = periodMs;

    /* Implement this function as specified here:
     * https://docs.aws.amazon.com/freertos/latest/lib-ref/c-sdk/platform/platform_clock_function_timerarm.html
     */
    if(NULL == pTimer)
    {
        return false;
    }
    worker = pTimer[0];
    if(NULL == pTimer)
    {
        return false;
    }
    worker->periodMs = periodMs;
    worker->relativeTimeoutMs = relativeTimeoutMs;
    if(NULL == worker->rtos_timer)
    {
        worker->rtos_timer = xTimerCreate("Clock_Timer_Interface", 500, pdFALSE, worker, timer_callback_function);
        assert(NULL != worker->rtos_timer);
    }
    if(0 != relativeTimeoutMs)
    {
        initial_period = relativeTimeoutMs;
    }
    if(xPortInIsrContext())
    {
        IH_ASSERT(IH_ERR_LEVEL_ERROR, pdPASS == xTimerChangePeriod(worker->rtos_timer, initial_period, 0));
        IH_ASSERT(IH_ERR_LEVEL_ERROR, pdPASS == xTimerStart(worker->rtos_timer, 0));
    }
    else
    {
        IH_ASSERT(IH_ERR_LEVEL_ERROR, pdPASS == xTimerChangePeriod(worker->rtos_timer, initial_period, portMAX_DELAY));
        IH_ASSERT(IH_ERR_LEVEL_ERROR, pdPASS == xTimerStart(worker->rtos_timer, portMAX_DELAY));
    }

    return false;
}

/*-----------------------------------------------------------*/
