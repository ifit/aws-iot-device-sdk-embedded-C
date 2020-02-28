/**
 * @file iot_port_mem_helper.h
 * @author Kade Cox
 * @date Created: Feb 25, 2020
 * @details
 * Tool that helps with memory allocation and deallocation at the aws port layer
 */

#ifndef AWS_IOT_PORTS_ICON_HAL_SRC_IOT_PORT_MEM_HELPER_H_
#define AWS_IOT_PORTS_ICON_HAL_SRC_IOT_PORT_MEM_HELPER_H_

/**
 * @brief Function that allocates data
 * @param size how many bytes to allocate
 * @return returns NULL on error
 */
void *iot_port_malloc(unsigned int size);

/**
 * @brief Function that frees previously allocated data
 * @param ptr the ptr to free
 */
void iot_port_free(void *ptr);

#endif /* AWS_IOT_PORTS_ICON_HAL_SRC_IOT_PORT_MEM_HELPER_H_ */
