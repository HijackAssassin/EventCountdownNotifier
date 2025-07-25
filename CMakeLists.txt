cmake_minimum_required(VERSION 3.16)

project(EventCountdownsNotifier LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Required for tray, timer, file access, etc
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)

add_executable(EventCountdownsNotifier
    main.cpp

)

target_link_libraries(EventCountdownsNotifier PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)

include(GNUInstallDirs)
install(TARGETS EventCountdownsNotifier
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
