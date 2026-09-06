add_library(protocol STATIC
    protocolcodec.cpp
    protocolcodec.h
)

target_include_directories(protocol PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(protocol PUBLIC
    Qt${QT_VERSION_MAJOR}::Core
)