# This CMakeLists is a template for new ports. It provides the minimal
# configuration for building, but nothing more.

# Warn that the template port only builds. It will not create usable libraries.
message( STATUS "Configurint AWS to use the Icon HAL")

# Template platform sources. A network implementation (not listed below) is also needed.
set( PLATFORM_SOURCES
     ${PORTS_DIRECTORY}/${IOT_PLATFORM_NAME}/src/iot_clock_${IOT_PLATFORM_NAME}.c
     ${PORTS_DIRECTORY}/${IOT_PLATFORM_NAME}/src/iot_threads_${IOT_PLATFORM_NAME}.c
     ${PORTS_DIRECTORY}/${IOT_PLATFORM_NAME}/src/iot_port_mem_helper.c)

# Set the types header for this port.
set( PORT_TYPES_HEADER ${PORTS_DIRECTORY}/${IOT_PLATFORM_NAME}/include/iot_platform_types_${IOT_PLATFORM_NAME}.h)
