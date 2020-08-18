# This CMakeLists is a template for new ports. It provides the minimal
# configuration for building, but nothing more.

# Warn that the template port only builds. It will not create usable libraries.
message( STATUS "Configurint AWS to use the Icon HAL")

# Template platform sources. A network implementation (not listed below) is also needed.
set( PLATFORM_SOURCES
     ${PORTS_DIRECTORY}/${IOT_PLATFORM_NAME}/src/iot_clock_${IOT_PLATFORM_NAME}.c
     ${PORTS_DIRECTORY}/${IOT_PLATFORM_NAME}/src/iot_threads_${IOT_PLATFORM_NAME}.c
     ${PORTS_DIRECTORY}/${IOT_PLATFORM_NAME}/src/iot_port_mem_helper.c
     ${PORTS_DIRECTORY}/${IOT_PLATFORM_NAME}/src/iot_network_mbedtls.c
     )

set( NETWORK_HEADER ${PORTS_DIRECTORY}/common/include/iot_network_mbedtls.h )

list( APPEND PLATFORM_COMMON_HEADERS
     ${NETWORK_HEADER} )

# Set the types header for this port.
set( PORT_TYPES_HEADER ${PORTS_DIRECTORY}/${IOT_PLATFORM_NAME}/include/iot_platform_types_${IOT_PLATFORM_NAME}.h)

if (AWS_MBED_CONFIG_OVERRIDE )
	message( STATUS "AWS_MBED_CONFIG_OVERRIDE: ${AWS_MBED_CONFIG_OVERRIDE}")
	set_source_files_properties(  ${PORTS_DIRECTORY}/${IOT_PLATFORM_NAME}/src/iot_network_mbedtls.c 
	PROPERTIES 
	COMPILE_DEFINITIONS MBEDTLS_CONFIG_FILE="${AWS_MBED_CONFIG_OVERRIDE}"
	)
endif() #AWS_MBED_CONFIG_OVERRIDE
