add_library(server STATIC
    clientthread.cpp
    clientthread.h
    requestdispatcher.cpp
    requestdispatcher.h
    serverlistener.cpp
    serverlistener.h
)

set_target_properties(server PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
)

target_include_directories(server PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(server PUBLIC
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Network
    protocol
    database
)

add_executable(ChargingServer
    main.cpp
)

target_link_libraries(ChargingServer PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Network
    server
)