add_library(protocol STATIC
    protocolcodec.cpp
    protocolcodec.h
)

set_target_properties(protocol PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
)

target_include_directories(protocol PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(protocol PUBLIC
    Qt${QT_VERSION_MAJOR}::Core
)