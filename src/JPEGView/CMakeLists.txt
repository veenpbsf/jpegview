# Only this directory's application sources, not the codec/submodule trees.
# CONFIGURE_DEPENDS also brings newly added source files into CLion targets.
file(GLOB _jpegview_sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
file(GLOB _jpegview_headers CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.h")
list(REMOVE_ITEM _jpegview_sources "${CMAKE_CURRENT_SOURCE_DIR}/ExplorerOrder.cpp")

add_executable(JPEGView WIN32 ${_jpegview_sources} ${_jpegview_headers}
    JPEGView.rc res/JPEGView.ico "${PROJECT_SOURCE_DIR}/cmake/JPEGView.manifest")
target_include_directories(JPEGView PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${JPEGVIEW_WTL_INCLUDE_DIR}"
    "${JPEGVIEW_ATL_INCLUDE_DIR}"
    libjpeg-turbo/include libpng-apng/include libjxl/include libheif/include
    libwebp/include libavif/include lcms2/include libraw/include)
target_compile_definitions(JPEGView PRIVATE
    WIN32 _WINDOWS STRICT UNICODE _UNICODE
    "$<$<CONFIG:Debug>:_DEBUG>")
target_compile_options(JPEGView PRIVATE
    "$<$<COMPILE_LANGUAGE:CXX>:/W3>"
    "$<$<COMPILE_LANGUAGE:CXX>:/EHa>")
target_precompile_headers(JPEGView PRIVATE stdafx.h)

# dcraw orders Winsock headers before stdafx.h; forced inclusion would change
# that order. The AVX source also needs different compiler flags from the PCH.
set_source_files_properties(dcraw_mod.cpp ApplyFilterAVX.cpp
    PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set_property(SOURCE ApplyFilterAVX.cpp APPEND PROPERTY COMPILE_OPTIONS /arch:AVX)
endif()

target_link_libraries(JPEGView PRIVATE
    WICLoader
    JPEGViewCodec::turbojpeg JPEGViewCodec::png JPEGViewCodec::zlib
    JPEGViewCodec::jxl JPEGViewCodec::jxl_threads JPEGViewCodec::heif
    JPEGViewCodec::webp JPEGViewCodec::webpdemux JPEGViewCodec::avif
    JPEGViewCodec::lcms2 JPEGViewCodec::raw
    gdiplus comctl32 comdlg32 gdi32 user32 shell32 shlwapi advapi32
    ole32 oleaut32 uuid ws2_32 delayimp
    "$<$<CONFIG:Debug>:dbghelp>")
if(TARGET ExplorerOrder)
    target_link_libraries(JPEGView PRIVATE ExplorerOrder)
endif()
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    target_link_libraries(JPEGView PRIVATE legacy_stdio_definitions legacy_stdio_wide_specifiers)
endif()

target_link_options(JPEGView PRIVATE
    /DELAYLOAD:WICLoader.dll
    /DELAYLOAD:jxl_dec.dll /DELAYLOAD:jxl_threads.dll /DELAYLOAD:heif.dll
    /DELAYLOAD:avif.dll /DELAYLOAD:lcms2.dll /DELAYLOAD:libraw.dll
    /IGNORE:4099
    "$<$<CONFIG:Debug>:/NODEFAULTLIB:LIBCMT>")
set_target_properties(JPEGView PROPERTIES DEBUG_POSTFIX "" VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:JPEGView>")

# Keep configuration/templates, translations, documentation and all transitive
# codec DLLs next to the exe. Existing *.ini files are never overwritten.
add_custom_command(TARGET JPEGView POST_BUILD
    COMMAND "${CMAKE_COMMAND}"
        "-DJPEGVIEW_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
        "-DJPEGVIEW_OUTPUT_DIR=$<TARGET_FILE_DIR:JPEGView>"
        "-DJPEGVIEW_CODEC_BIN_SUBDIR=${JPEGVIEW_CODEC_BIN_SUBDIR}"
        "-DJPEGVIEW_MSVC_RUNTIME_DLLS=${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS}"
        -P "${PROJECT_SOURCE_DIR}/cmake/StageRuntime.cmake"
    COMMENT "Stage JPEGView configuration, translations and codec DLLs"
    VERBATIM)
