cmake_minimum_required(VERSION 3.16)

project(ChargingPlatform LANGUAGES CXX)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Widgets Network Sql)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Widgets Network Sql)

add_subdirectory(protocol)
add_subdirectory(db)
add_subdirectory(server)
add_subdirectory(client)