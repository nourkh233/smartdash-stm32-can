cmake_minimum_required(VERSION 3.19)
project(SmartDashQt LANGUAGES CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets SerialPort)

qt_standard_project_setup()

qt_add_executable(SmartDashQt
    WIN32 MACOSX_BUNDLE
    main.cpp
    mainwindow.cpp
    mainwindow.h
    framedecoder.h
    framedecoder.cpp
    dashboardmodel.h
    dashboardmodel.cpp
    serialworker.h
    serialworker.cpp
    simsource.h
    simsource.cpp


    simsource.h
    gaugewidget.cpp
    gaugewidget.h
)

target_link_libraries(SmartDashQt
    PRIVATE
        Qt::Core
        Qt::Widgets
        Qt::SerialPort
)

install(TARGETS SmartDashQt
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
find_package(Qt6 REQUIRED COMPONENTS Test)
enable_testing()
add_executable(tst_framedecoder tests/tst_framedecoder.cpp framedecoder.cpp framedecoder.h)
target_include_directories(tst_framedecoder PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(tst_framedecoder PRIVATE Qt6::Core Qt6::Test)
add_test(NAME framedecoder COMMAND tst_framedecoder)

