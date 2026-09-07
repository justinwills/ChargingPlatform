add_library(navigation STATIC
    navigationwidget.cpp
    navigationwidget.h
)

set_target_properties(navigation PROPERTIES
    AUTOMOC ON
)

target_include_directories(navigation PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(navigation PUBLIC
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::WebEngineWidgets
)
