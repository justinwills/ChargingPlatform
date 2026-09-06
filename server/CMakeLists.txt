add_library(server STATIC
    clientthread.cpp
    clientthread.h
    requestdispatcher.cpp
    requestdispatcher.h
    serverlistener.cpp
    serverlistener.h
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