add_library(database STATIC
    database.cpp
    database.h
)

target_include_directories(database PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(database PUBLIC
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Sql
)