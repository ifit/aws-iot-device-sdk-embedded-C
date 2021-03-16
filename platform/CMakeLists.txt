# For static library builds, link against static library of OpenSSL
if(NOT BUILD_SHARED_LIBS)
    set( OPENSSL_USE_STATIC_LIBS TRUE )
endif()

if(NOT DEFINED IOT_PLATFORM_NAME)
find_package(OpenSSL)
# Verify the minimum OpenSSL version required
if( ${OPENSSL_FOUND} AND ( OPENSSL_VERSION MATCHES "0.9$" OR OPENSSL_VERSION MATCHES "1.0$" ) )
    message( WARNING "OpenSSL 1.1.0 or later required: OpenSSL ${OPENSSL_VERSION} found." )
endif()
endif(NOT DEFINED IOT_PLATFORM_NAME)

find_library(LIB_RT rt)

if(NOT DEFINED IOT_PLATFORM_NAME)
    set(IOT_PLATFORM_NAME posix)
    set(THREADS_PREFER_PTHREAD_FLAG ON)
    find_package(Threads)
    set(OpenSSL_FOUND ${OpenSSL_FOUND} CACHE INTERNAL "Indicates whether OpenSSL library was found.")
    set(Threads_FOUND ${Threads_FOUND} CACHE INTERNAL "Indicates whether Threads library was found.")
endif(NOT DEFINED IOT_PLATFORM_NAME)

# Add the platform targets
message(STATUS "Platform: ${PLATFORM_DIR}/${IOT_PLATFORM_NAME}")
add_subdirectory( ${PLATFORM_DIR}/${IOT_PLATFORM_NAME} )

# Add ota porting targets
add_subdirectory( ${PLATFORM_DIR}/${IOT_PLATFORM_NAME}/ota_pal )

if(DEFINED AWS_HAL_PORT_SRCS)
    set(AWS_HAL_PORT_SRCS ${AWS_HAL_PORT_SRCS} PARENT_SCOPE)
endif(DEFINED AWS_HAL_PORT_SRCS)
