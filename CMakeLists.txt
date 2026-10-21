cmake_minimum_required(VERSION 3.16)

project(Gamecharacterlike_desktoppet VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets)

qt_standard_project_setup(REQUIRES 6.5)

qt_add_executable(Gamecharacterlike_desktoppet
    WIN32
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    desktopicons.cpp
    desktopicons.h
    speechbubble.cpp
    speechbubble.h
    windowframe.cpp
    windowframe.h
    resources/resources.qrc


)

set_target_properties(Gamecharacterlike_desktoppet PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
)

target_link_libraries(Gamecharacterlike_desktoppet
    PRIVATE Qt6::Widgets
)

if(WIN32)
    # dwmapi：关掉系统给窗口额外画的阴影和圆角。
    # oleacc + oleaut32：读桌面图标信息用的无障碍接口（MSAA）。
    # shell32 + ole32：打开图标，以及 COM 初始化。
    target_link_libraries(Gamecharacterlike_desktoppet
        PRIVATE dwmapi oleacc oleaut32 shell32 ole32
    )
endif()

install(TARGETS Gamecharacterlike_desktoppet
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION bin
)
