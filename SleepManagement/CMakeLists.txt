cmake_minimum_required(VERSION 3.19)
project(SleepManagement LANGUAGES CXX)

# 在工程配置里把 Network 模块加进来
find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets Network)

qt_standard_project_setup()

qt_add_executable(SleepManagement
    WIN32 MACOSX_BUNDLE
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    sleep_core.h
)

target_link_libraries(SleepManagement
    PRIVATE
        Qt::Core
        Qt::Widgets
        Qt::Network # 加入网络模块
)

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
