cmake_minimum_required(VERSION 3.20)

project(OnionHENKernelLogPlugin VERSION 1.0.0 LANGUAGES C)

set(ONION_PLUGIN_TARGET klog)
set(ONION_PLUGIN_ID KLOG00001)
set(ONION_PLUGIN_VERSION 1.00)
set(ONION_PLUGIN_NAME "Kernel Log")

set(ONIONHEN_PLUGIN_SDK_SOURCE "" CACHE PATH
    "Use a local onionHEN-plugin-sdk checkout instead of downloading it")
set(ONIONHEN_PLUGIN_SDK_GIT_REPOSITORY
    "https://github.com/OnionBuddies/onionHEN-plugin-sdk.git" CACHE STRING
    "OnionHEN Plugin SDK repository")
set(ONIONHEN_PLUGIN_SDK_GIT_TAG
    "0cf4346536705fb271ba0eb2226d97ebcb23559c" CACHE STRING
    "Pinned OnionHEN Plugin SDK revision")

if(NOT ONIONHEN_PLUGIN_SDK_SOURCE AND
   DEFINED ENV{ONIONHEN_PLUGIN_SDK_SOURCE})
    set(ONIONHEN_PLUGIN_SDK_SOURCE "$ENV{ONIONHEN_PLUGIN_SDK_SOURCE}")
endif()

if(ONIONHEN_PLUGIN_SDK_SOURCE)
    if(NOT EXISTS "${ONIONHEN_PLUGIN_SDK_SOURCE}/CMakeLists.txt")
        message(FATAL_ERROR
            "ONIONHEN_PLUGIN_SDK_SOURCE is not an SDK checkout: "
            "${ONIONHEN_PLUGIN_SDK_SOURCE}")
    endif()
    add_subdirectory(
        "${ONIONHEN_PLUGIN_SDK_SOURCE}"
        "${CMAKE_BINARY_DIR}/_deps/onionhen-plugin-sdk-build"
        EXCLUDE_FROM_ALL)
else()
    include(FetchContent)
    FetchContent_Declare(onionhen_plugin_sdk
        GIT_REPOSITORY "${ONIONHEN_PLUGIN_SDK_GIT_REPOSITORY}"
        GIT_TAG "${ONIONHEN_PLUGIN_SDK_GIT_TAG}"
        GIT_SHALLOW FALSE)
    FetchContent_MakeAvailable(onionhen_plugin_sdk)
endif()

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/include/plugin_config.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/plugin_config.h"
    @ONLY)

set(KLOGSRV_PAYLOAD "${CMAKE_CURRENT_SOURCE_DIR}/assets/klogsrv-ps5.elf")
if(NOT EXISTS "${KLOGSRV_PAYLOAD}")
    message(FATAL_ERROR
        "Embedded payload not found: ${KLOGSRV_PAYLOAD}")
endif()
file(READ "${KLOGSRV_PAYLOAD}" KLOGSRV_MAGIC HEX LIMIT 4)
if(NOT KLOGSRV_MAGIC STREQUAL "7f454c46")
    message(FATAL_ERROR
        "assets/klogsrv-ps5.elf is not an ELF (magic 0x${KLOGSRV_MAGIC})")
endif()
file(SIZE "${KLOGSRV_PAYLOAD}" KLOGSRV_PAYLOAD_SIZE)
message(STATUS
    "Embedded klogsrv payload: ${KLOGSRV_PAYLOAD} "
    "(${KLOGSRV_PAYLOAD_SIZE} bytes)")

onion_add_plugin(
    NAME ${ONION_PLUGIN_TARGET}
    TITLE_ID ${ONION_PLUGIN_ID}
    VERSION ${ONION_PLUGIN_VERSION}
    SOURCES
        source/main.c
        source/klog_i18n.c
        source/klog_service.c
        source/elfldr_client.c
        source/embedded_payloads.c
        source/plugin_descriptor.c
        source/plugin_settings.c
        source/plugin_ui.c
    INCLUDE_DIRS
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
        "${CMAKE_CURRENT_BINARY_DIR}/generated"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets"
    LIBRARIES
        SceLibcInternal
        SceSystemService
        SceNotification
        kernel_sys)

set_target_properties(${ONION_PLUGIN_TARGET} PROPERTIES
    OUTPUT_NAME "${ONION_PLUGIN_ID}.elf")

# Post-build descriptor validation. The pinned SDK declares the same check
# inside onion_add_plugin(), but Python3_EXECUTABLE set in the function scope
# does not survive to command expansion on every CMake release, so the plugin
# runs it explicitly against the SDK tools.
find_package(Python3 COMPONENTS Interpreter REQUIRED)
set(KLOG_SDK_TOOLS_DIR "")
if(ONIONHEN_PLUGIN_SDK_SOURCE)
    set(KLOG_SDK_TOOLS_DIR "${ONIONHEN_PLUGIN_SDK_SOURCE}/tools")
elseif(DEFINED onionhen_plugin_sdk_SOURCE_DIR)
    set(KLOG_SDK_TOOLS_DIR "${onionhen_plugin_sdk_SOURCE_DIR}/tools")
endif()
if(KLOG_SDK_TOOLS_DIR AND EXISTS "${KLOG_SDK_TOOLS_DIR}/inspect_plugin.py")
    add_custom_command(TARGET ${ONION_PLUGIN_TARGET} POST_BUILD
        COMMAND "${Python3_EXECUTABLE}"
            "${KLOG_SDK_TOOLS_DIR}/inspect_plugin.py"
            "$<TARGET_FILE:${ONION_PLUGIN_TARGET}>"
            --expect-id "${ONION_PLUGIN_ID}"
            --expect-version "${ONION_PLUGIN_VERSION}"
            --quiet
        COMMENT "Validating ${ONION_PLUGIN_TARGET}.elf plugin descriptor"
        VERBATIM)
else()
    message(STATUS "inspect_plugin.py not found; skipping post-build validation")
endif()

# The .incbin directive resolves the payload through the -I include path
# (validated with the Prospero toolchain), so a bare filename keeps the build
# portable across Linux and Windows hosts.
target_compile_definitions(${ONION_PLUGIN_TARGET} PRIVATE
    VERSION_TAG="OnionHEN Kernel Log plugin ${ONION_PLUGIN_VERSION}"
    KLOGSRV_ELF_PATH="klogsrv-ps5.elf")
