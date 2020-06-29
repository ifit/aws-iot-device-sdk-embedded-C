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

/***********************************************************************************/
/***************************** Type Defs *******************************************/
/***********************************************************************************/

struct mem_helper_data_s
{
    bool initialized; //!< Tells if we are initialized
    ih_mempool_handle_t mempool; //!< The memory pool handle
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
    .mempool = NULL
}; //!< Variable that holds the module data

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
        mem_helper.mempool = IH_MEM_POOL_CREATE(iot_helper_pool, CONFIG_ICON_AWS_PORT_MEMPOOL_SIZE, CONFIG_ICON_AWS_PORT_MEMPOOL_MAX_CHUNKS);
    }
    IH_ASSERT(IH_ERR_LEVEL_ERROR, NULL != mem_helper.mempool);
}

/**
 * @brief Function that allocates data
 * @param size how many bytes to allocate
 * @return returns NULL on error
 */
void *iot_port_malloc(unsigned int size)
{
	void * rv;
    init_as_needed();
    rv = ih_mempool_malloc(mem_helper.mempool, size);
    IH_ASSERT(IH_ERR_LEVEL_ERROR, NULL != rv);
    return rv;
}

/**
 * @brief Function that frees previously allocated data
 * @param ptr the ptr to free
 */
void iot_port_free(void *ptr)
{
    init_as_needed();
    ih_mempool_free(mem_helper.mempool, ptr);
}
