cmake_minimum_required(VERSION 3.18)
project(VkFFTAudio CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

# ── vkFFT — header-only, OpenCL backend (VKFFT_BACKEND=3) ────────────────────
# WHY FetchContent: keeps the module self-contained; no system vkFFT install required.
FetchContent_Declare(vkfft
  GIT_REPOSITORY https://github.com/DTolm/VkFFT.git
  GIT_TAG        master
)
FetchContent_MakeAvailable(vkfft)

# ── CLI11 + OpenCL + stb via common.cmake ─────────────────────────────────────
# common.cmake is 2 levels up: 06_Bonus/vkFFT_Audio → root
include(${CMAKE_CURRENT_SOURCE_DIR}/../../common/common.cmake)

# ── Optional FFTW3 for CPU reference timing ───────────────────────────────────
# WHY QUIET: FFTW3 is optional. The binary runs and skips the CPU reference path
# when FFTW3 is absent; a missing package must not be a configure-time error.
find_package(FFTW3 QUIET)
if(FFTW3_FOUND)
    message(STATUS "FFTW3 cmake config found — CPU reference enabled")
    set(HAVE_FFTW3 TRUE)
else()
    # Fallback: try pkg-config for fftw3f (float variant preferred for audio)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(FFTW3F QUIET fftw3f)
        if(FFTW3F_FOUND)
            message(STATUS "FFTW3 (fftw3f via pkg-config) found — CPU reference enabled")
            set(HAVE_FFTW3 TRUE)
        else()
            message(STATUS "FFTW3 not found — CPU reference path disabled")
        endif()
    else()
        message(STATUS "FFTW3 and PkgConfig not found — CPU reference path disabled")
    endif()
endif()

add_executable(vkfft_audio main.cpp)

# ── Common wiring (OpenCL + CLI11 + stb includes) ────────────────────────────
opencl_lab_target(vkfft_audio)

# ── vkFFT: VKFFT_BACKEND=3 selects the OpenCL backend inside vkFFT.h ─────────
target_compile_definitions(vkfft_audio PRIVATE VKFFT_BACKEND=3)

target_include_directories(vkfft_audio PRIVATE
    ${vkfft_SOURCE_DIR}/vkFFT   # provides vkFFT.h
)

# ── Link FFTW3 if available ───────────────────────────────────────────────────
if(HAVE_FFTW3)
    target_compile_definitions(vkfft_audio PRIVATE HAVE_FFTW3)
    if(FFTW3_FOUND)
        # cmake config provides FFTW3::fftw3f (float) or FFTW3::fftw3 (double)
        if(TARGET FFTW3::fftw3f)
            target_link_libraries(vkfft_audio PRIVATE FFTW3::fftw3f)
        elseif(TARGET FFTW3::fftw3)
            target_link_libraries(vkfft_audio PRIVATE FFTW3::fftw3)
        endif()
    elseif(FFTW3F_FOUND)
        # pkg-config path: use raw flags
        target_include_directories(vkfft_audio PRIVATE ${FFTW3F_INCLUDE_DIRS})
        target_link_libraries(vkfft_audio PRIVATE ${FFTW3F_LIBRARIES})
    endif()
endif()

# ── Kernel copy (magnitude.cl → binary dir/kernels/) ─────────────────────────
copy_kernels(vkfft_audio)
symlink_assets(vkfft_audio)

message(STATUS "vkfft_audio configured. Run: ./build/vkfft_audio --input <path>.wav")
