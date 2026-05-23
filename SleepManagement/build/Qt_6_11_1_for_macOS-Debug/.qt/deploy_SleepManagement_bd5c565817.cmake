include("/Users/sunny_cy/Desktop/大一下/大一下课程实记/程序设计实习/作业/大作业/程序包/SleepManagement/build/Qt_6_11_1_for_macOS-Debug/.qt/QtDeploySupport.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/SleepManagement-plugins.cmake" OPTIONAL)
set(__QT_DEPLOY_I18N_CATALOGS "qtbase")

qt6_deploy_runtime_dependencies(
    EXECUTABLE "SleepManagement.app"
)
