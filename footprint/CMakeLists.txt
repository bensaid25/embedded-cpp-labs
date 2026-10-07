# Footprint programs: tiny "firmware-shaped" programs that each use one part
# of the library, so the flash and RAM cost of every part can be measured.
# Meant to be cross-compiled (see cmake/toolchains/), then measured with
# scripts/footprint.sh.
#
# These targets are built with the strict embedded rules ON:
#   -fno-exceptions -fno-rtti  : a throw / dynamic_cast / typeid anywhere in
#                                the library is a COMPILE ERROR here
# and scripts/footprint.sh then checks the linked program for heap and
# exception machinery.

add_library(ecl_embedded_options INTERFACE)
target_compile_options(ecl_embedded_options INTERFACE
    -Wall -Wextra -Wpedantic -Werror
    -fno-exceptions -fno-rtti -fno-threadsafe-statics
    -ffunction-sections -fdata-sections)
target_link_options(ecl_embedded_options INTERFACE -Wl,--gc-sections)

set(ECL_FOOTPRINT_SCENARIOS
    BASELINE RING_BUFFER SPSC SAMPLER_MOCK MOVING_AVERAGE STATISTICS THRESHOLD FULL)

foreach(scenario IN LISTS ECL_FOOTPRINT_SCENARIOS)
    string(TOLOWER "${scenario}" lower)
    add_executable(fp_${lower} footprint.cpp)
    target_compile_definitions(fp_${lower} PRIVATE FP_${scenario})
    target_link_libraries(fp_${lower} PRIVATE ecl ecl_embedded_options)
endforeach()
