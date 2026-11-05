add_library(WICLoader SHARED
    dllmain.cpp
    stdafx.cpp
    WICLoader.cpp
    WICLoader.rc
    stdafx.h
    targetver.h
    MaxImageDef.h)
target_include_directories(WICLoader PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_definitions(WICLoader PRIVATE
    WIN32 _WINDOWS _USRDLL WICLOADER_EXPORTS UNICODE _UNICODE
    "$<$<CONFIG:Debug>:_DEBUG>")
target_compile_options(WICLoader PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:/W3>")
target_precompile_headers(WICLoader PRIVATE stdafx.h)
target_link_libraries(WICLoader PRIVATE windowscodecs ole32 uuid)
set_target_properties(WICLoader PROPERTIES DEBUG_POSTFIX "" PREFIX "")
