cmake_minimum_required(VERSION 3.21)

project(DolphinContextMenuManager
    VERSION 0.1.0
    DESCRIPTION "Create and manage Dolphin context-menu actions"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets)

add_library(dcm-core STATIC
    src/servicemenu.cpp
    src/servicemenu.h
)
target_link_libraries(dcm-core PUBLIC Qt6::Core)
target_include_directories(dcm-core PUBLIC src)

add_executable(dolphin-context-menu-manager
    src/main.cpp
    src/mainwindow.cpp
    src/mainwindow.h
)
target_link_libraries(dolphin-context-menu-manager PRIVATE
    dcm-core
    Qt6::Gui
    Qt6::Widgets
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(dcm-core PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(dolphin-context-menu-manager PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(GNUInstallDirs)
install(TARGETS dolphin-context-menu-manager RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES data/io.github.dolphincontextmenumanager.desktop
    DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
)
install(FILES data/io.github.dolphincontextmenumanager.svg
    DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps
)

include(CTest)
if(BUILD_TESTING)
    add_executable(servicemenu-tests tests/servicemenu_tests.cpp)
    target_link_libraries(servicemenu-tests PRIVATE dcm-core Qt6::Core)
    add_test(NAME servicemenu-tests COMMAND servicemenu-tests)
endif()
