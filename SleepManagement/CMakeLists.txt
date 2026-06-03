cmake_minimum_required(VERSION 3.19)
project(SleepManagement LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(GNUInstallDirs)
# 在工程配置里把 Network 模块加进来
find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets Network PrintSupport)

qt_standard_project_setup()
set(CMAKE_AUTORCC ON) # 确保QT资源系统自动打开RCC，读取说明书.html

set(PROJECT_SOURCES
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    sleep_core.h
    achievement_manager.h
    sleep_charts.h
    sleep_charts.cpp
    settings_dialog.h
    settings_dialog.cpp
    notification_manager.h
    notification_manager.cpp
    key_monitor.h
    key_monitor.cpp
    keyboard_chart.h
    keyboard_chart.cpp
    resources.qrc
)

if(APPLE)#这里注意写了对于系统的判断，关键改动，否则windows跑不了
    enable_language(OBJCXX)

    list(APPEND PROJECT_SOURCES
        key_monitor_mac.mm
    )
# 为 .mm 文件开启 Objective-C ARC（自动引用计数）
    set_source_files_properties(key_monitor_mac.mm PROPERTIES
        COMPILE_FLAGS "-fobjc-arc"
    )
endif()

qt_add_executable(SleepManagement
    WIN32 MACOSX_BUNDLE
    ${PROJECT_SOURCES}
)

target_link_libraries(SleepManagement
    PRIVATE
        Qt::Core
        Qt::Widgets
        Qt::Network
        Qt::PrintSupport
)
# macOS 需要链接 Cocoa 和 CoreGraphics 框架
if(APPLE)
    find_library(COCOA_LIBRARY Cocoa)
    target_link_libraries(SleepManagement PRIVATE ${COCOA_LIBRARY})
endif()

install(TARGETS SleepManagement
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET SleepManagement
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)

install(SCRIPT ${deploy_script})
