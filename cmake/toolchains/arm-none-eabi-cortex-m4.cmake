# Toolchain file for cross-compiling the footprint programs to a Cortex-M4F
# (for example STM32F4, nRF52, or a Raspberry-Pi-class M4 part).
#
# Use:  cmake -S . -B build-arm \
#           -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/arm-none-eabi-cortex-m4.cmake \
#           -DCMAKE_BUILD_TYPE=MinSizeRel \
#           -DECL_BUILD_TESTS=OFF -DECL_BUILD_EXAMPLES=OFF -DECL_BUILD_FOOTPRINT=ON
#
# Needs the "arm-none-eabi" GCC toolchain on PATH
# (Ubuntu: sudo apt install gcc-arm-none-eabi libstdc++-arm-none-eabi-newlib).

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(CMAKE_C_COMPILER   arm-none-eabi-gcc)
set(CMAKE_CXX_COMPILER arm-none-eabi-g++)
set(CMAKE_ASM_COMPILER arm-none-eabi-gcc)

# There is no OS to run test programs on, so only compile them when CMake
# probes the compiler.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# Cortex-M4 with single-precision FPU (hard float ABI).
set(ECL_MCU_FLAGS "-mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16")
set(CMAKE_C_FLAGS_INIT   "${ECL_MCU_FLAGS}")
set(CMAKE_CXX_FLAGS_INIT "${ECL_MCU_FLAGS}")

# newlib-nano (small C library) and nosys (empty system-call stubs): the
# result links without a board support package.
# The nosys stubs make the linker print "_close is not implemented" for
# every program; they are expected here, so they are silenced.
set(CMAKE_EXE_LINKER_FLAGS_INIT
    "${ECL_MCU_FLAGS} --specs=nano.specs --specs=nosys.specs -Wl,--no-warnings")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
