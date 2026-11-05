# Windows/ATL/WTL checks and the same precompiled codecs as JPEGView.vcxproj.
find_path(JPEGVIEW_WTL_INCLUDE_DIR atlapp.h
    HINTS "${PROJECT_SOURCE_DIR}/deps/WTL-sf/Include")
if(NOT JPEGVIEW_WTL_INCLUDE_DIR)
    message(FATAL_ERROR
        "WTL headers are missing. Run: git submodule update --init deps/WTL-sf "
        "or set JPEGVIEW_WTL_INCLUDE_DIR to the WTL Include directory.")
endif()

get_filename_component(_jpegview_compiler_dir "${CMAKE_CXX_COMPILER}" DIRECTORY)
get_filename_component(_jpegview_toolset_root "${_jpegview_compiler_dir}/../../.." ABSOLUTE)
find_path(JPEGVIEW_ATL_INCLUDE_DIR atlbase.h
    HINTS "$ENV{VCToolsInstallDir}/atlmfc/include" "${_jpegview_toolset_root}/atlmfc/include"
    PATHS ${CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES})
if(NOT JPEGVIEW_ATL_INCLUDE_DIR)
    message(FATAL_ERROR
        "ATL headers (atlbase.h) are missing. Install the C++ ATL component for your MSVC toolset "
        "in Visual Studio Installer, then reload/reset the CMake profile.")
endif()

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(JPEGVIEW_CODEC_LIB_SUBDIR lib64)
    set(JPEGVIEW_CODEC_BIN_SUBDIR bin64)
elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
    set(JPEGVIEW_CODEC_LIB_SUBDIR lib)
    set(JPEGVIEW_CODEC_BIN_SUBDIR bin)
else()
    message(FATAL_ERROR "Unsupported pointer size for the bundled JPEGView codecs.")
endif()

function(jpegview_import_codec name directory filename)
    set(_library "${PROJECT_SOURCE_DIR}/src/JPEGView/${directory}/${JPEGVIEW_CODEC_LIB_SUBDIR}/${filename}")
    if(NOT EXISTS "${_library}")
        message(FATAL_ERROR "Missing bundled codec library: ${_library}. Restore the repository's codec files.")
    endif()
    add_library(JPEGViewCodec::${name} UNKNOWN IMPORTED GLOBAL)
    set_target_properties(JPEGViewCodec::${name} PROPERTIES IMPORTED_LOCATION "${_library}")
endfunction()

jpegview_import_codec(turbojpeg libjpeg-turbo turbojpeg-static.lib)
jpegview_import_codec(png libpng-apng libpng16.lib)
jpegview_import_codec(zlib libpng-apng zlib.lib)
jpegview_import_codec(jxl libjxl jxl_dec.lib)
jpegview_import_codec(jxl_threads libjxl jxl_threads.lib)
jpegview_import_codec(heif libheif heif.lib)
jpegview_import_codec(webp libwebp libwebp.lib)
jpegview_import_codec(webpdemux libwebp libwebpdemux.lib)
jpegview_import_codec(avif libavif avif.lib)
jpegview_import_codec(lcms2 lcms2 lcms2.lib)
jpegview_import_codec(raw libraw libraw.lib)
