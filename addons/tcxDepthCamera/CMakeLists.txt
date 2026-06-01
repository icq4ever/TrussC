# =============================================================================
# tcxDepthCamera - Unified depth-camera interface for TrussC
# =============================================================================
#
# Header-only, interface-only addon: it defines the DepthCamera base contract
# and capability interfaces. It ships no device drivers and links no SDKs, so
# the target is an INTERFACE library (just an include path + a link to TrussC).
#
# Concrete camera addons (e.g. tcxOrbbec) depend on this one.
# =============================================================================

cmake_minimum_required(VERSION 3.16)

set(ADDON_NAME tcxDepthCamera)

add_library(${ADDON_NAME} INTERFACE)

target_include_directories(${ADDON_NAME} INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(${ADDON_NAME} INTERFACE TrussC)
