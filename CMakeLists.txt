cmake_minimum_required(VERSION 3.5)

# set(PNG_PNG_INCLUDE_DIR  /usr/local/opt/libpng/include)
# set(PNG_LIBRARY       /usr/local/opt/libpng/lib/libpng.dylib)

project(JPEG-decoder)

find_package(ZLIB)
find_package(PNG)

add_executable(JPEG-decoder
    src/decoder.cpp 
    src/fft.cpp 
    src/huffman.cpp 
    src/jpg_to_png.cpp 
    src/png_encoder.cpp
    src/main.cpp
)

target_include_directories(JPEG-decoder PRIVATE src)

if (FFTW_INCLUDES)
  # Already in cache, be silent
  set (FFTW_FIND_QUIETLY TRUE)
endif (FFTW_INCLUDES)

find_path (FFTW_INCLUDES fftw3.h)

find_library (FFTW_LIBRARIES NAMES fftw3)

mark_as_advanced (FFTW_LIBRARIES FFTW_INCLUDES)

# handle the QUIETLY and REQUIRED arguments and set FFTW_FOUND to TRUE if
# all listed variables are TRUE
include (FindPackageHandleStandardArgs)
find_package_handle_standard_args (FFTW DEFAULT_MSG FFTW_LIBRARIES FFTW_INCLUDES)

target_include_directories(JPEG-decoder PUBLIC
            ${PNG_INCLUDE_DIRS}
            ${FFTW_INCLUDES})

    target_link_libraries(JPEG-decoder PUBLIC
            ${PNG_LIBRARY}
            ${FFTW_LIBRARIES})
