cmake_minimum_required(VERSION 3.16)

project(ChargingClient LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Widgets Network Charts)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Widgets Network Charts)
find_package(Qt${QT_VERSION_MAJOR} QUIET COMPONENTS WebEngineWidgets)

if(NOT TARGET protocol)
    add_subdirectory(
        ${CMAKE_CURRENT_SOURCE_DIR}/../protocol
        ${CMAKE_CURRENT_BINARY_DIR}/protocol-build
    )
endif()

add_executable(ChargingClient
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    mainwindow_chargeui.cpp
    mainwindow_chargeselectors.cpp
    mainwindow_homenav.cpp
    mainwindow_stationdetail.cpp
    mainwindow_accountpage.cpp
    mainwindow_chargingsession.cpp
    mainwindow_settingspage.cpp
    mainwindow_rechargepage.cpp
    mainwindow_response.cpp
    mainwindow_response_charging.cpp
    mainwindow_response_account.cpp
    chargering.cpp
    chargering.h
    flowlayout.cpp
    flowlayout.h
    paymentpreview.cpp
    paymentpreview.h
    resources.qrc
    admin/adminwindow.cpp
    admin/adminwindow.h
    admin/admindesign.h
    admin/adminwindow_style.cpp
    admin/adminwindow_loginpage.cpp
    admin/adminwindow_dashboardshell.cpp
    admin/adminwindow_statstab.cpp
    admin/adminwindow_userstab.cpp
    admin/adminwindow_stationstab.cpp
    admin/adminwindow_orderstab.cpp
    admin/adminwindow_login.cpp
    admin/adminwindow_datarefresh.cpp
    admin/adminwindow_statscards.cpp
    admin/adminwindow_pilestable.cpp
    admin/adminwindow_response.cpp
    clientconnection.cpp
    clientconnection.h
    navigationpage.cpp
    navigationpage.h
)

target_include_directories(ChargingClient PRIVATE
    ../protocol
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(ChargingClient PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Network
    Qt${QT_VERSION_MAJOR}::Charts
    protocol
)

if(TARGET Qt${QT_VERSION_MAJOR}::WebEngineWidgets)
    target_link_libraries(ChargingClient PRIVATE Qt${QT_VERSION_MAJOR}::WebEngineWidgets)
    target_compile_definitions(ChargingClient PRIVATE CHARGING_HAS_WEBENGINE)
else()
    message(WARNING "Qt WebEngineWidgets was not found; navigation uses an external-browser fallback")
endif()
