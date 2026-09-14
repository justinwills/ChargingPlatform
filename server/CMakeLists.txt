add_library(server STATIC
    clientthread.cpp
    clientthread.h
    requestdispatcher.cpp
    requestdispatcher.h
    dispatcher_account.cpp
    dispatcher_admin.cpp
    dispatcher_stations.cpp
    dispatcher_navigation.cpp
    dispatcher_navigation_helpers.cpp
    dispatcher_orders.cpp
    dispatchernavhelpers.h
    dispatchernavhelpers.cpp
    dispatcherorderhelpers.h
    dispatcherorderhelpers.cpp
    serverlistener.cpp
    serverlistener.h
    httpdashboard.cpp
    httpdashboard.h
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

add_custom_command(TARGET ChargingServer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/dashboard
        $<TARGET_FILE_DIR:ChargingServer>/dashboard
    COMMENT "Copying dashboard/ next to ChargingServer executable"
)