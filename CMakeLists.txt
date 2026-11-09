cmake_minimum_required(VERSION 3.21)
project(ryu)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_AUTOMOC ON)

if(PROJECT_IS_TOP_LEVEL)
    option(RYU_STATIC "Build the QML modules as static libraries" OFF)
    option(RYU_INSTALL "Generate install rules" ON)
else()
    option(RYU_STATIC "Build the QML modules as static libraries" ON)
    option(RYU_INSTALL "Generate install rules" OFF)
endif()


set(RYU_ALL_MODULES "Cleave;Clipsh;Sqliter;Warsa;Shapes;Extraa;Widgets;Discord;Network;Mpris;Power;Notifications;Io;Pam;Audio;Hyprland;Polkit;Ipc;Desktop;Ui")
set(RYU_MODULES "${RYU_ALL_MODULES}" CACHE STRING "Modules to build")

foreach(m IN LISTS RYU_MODULES)
    if(NOT m IN_LIST RYU_ALL_MODULES)
        message(FATAL_ERROR "Unknown Ryu module: ${m}")
    endif()
endforeach()

set(RYU_QT_COMPONENTS Core Qml Gui Quick)
if("Sqliter" IN_LIST RYU_MODULES)
    list(APPEND RYU_QT_COMPONENTS Sql)
endif()
if("Discord" IN_LIST RYU_MODULES OR "Hyprland" IN_LIST RYU_MODULES OR "Polkit" IN_LIST RYU_MODULES OR "Ipc" IN_LIST RYU_MODULES)
    list(APPEND RYU_QT_COMPONENTS Network)
endif()
if("Network" IN_LIST RYU_MODULES OR "Mpris" IN_LIST RYU_MODULES OR "Power" IN_LIST RYU_MODULES OR "Notifications" IN_LIST RYU_MODULES OR "Polkit" IN_LIST RYU_MODULES)
    list(APPEND RYU_QT_COMPONENTS DBus)
endif()
find_package(Qt6 REQUIRED COMPONENTS ${RYU_QT_COMPONENTS})

if("Pam" IN_LIST RYU_MODULES)
    find_library(PAM_LIBRARY pam REQUIRED)
    find_path(PAM_INCLUDE_DIR security/pam_appl.h REQUIRED)
endif()

if("Cleave" IN_LIST RYU_MODULES OR "Audio" IN_LIST RYU_MODULES)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(PIPEWIRE REQUIRED libpipewire-0.3)
endif()

qt_policy(SET QTP0001 NEW)

qt_add_qml_module(RyuCore
    STATIC
    URI Ryu.Core
    VERSION 1.0
    NO_PLUGIN
    SOURCES
        Core/paintutil.cpp
        Core/paintutil.hpp
        Core/sgitem.cpp
        Core/sgitem.hpp
        Core/sizes.hpp
        Core/spring.hpp
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/Ryu/Core"
)
set_target_properties(RyuCore PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    AUTOMOC ON
)
target_include_directories(RyuCore PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/Core
)
target_link_libraries(RyuCore PUBLIC Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick)

function(ryu_module name)
    if(NOT name IN_LIST RYU_MODULES)
        return()
    endif()

    cmake_parse_arguments(ARG "" "" "SOURCES;QML_FILES;LIBS;DEPS" ${ARGN})

    set(dep_args "")
    if(ARG_DEPS)
        set(dep_args DEPENDENCIES ${ARG_DEPS})
    endif()

    set(kind "")
    if(RYU_STATIC)
        set(kind STATIC)
    endif()

    qt_add_qml_module(${name}
        ${kind}
        URI Ryu.${name}
        VERSION 1.0
        SOURCES ${ARG_SOURCES}
        QML_FILES ${ARG_QML_FILES}
        ${dep_args}
        NO_PLUGIN_OPTIONAL
        OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/Ryu/${name}"
    )

    set_target_properties(${name} PROPERTIES POSITION_INDEPENDENT_CODE ON)

    target_include_directories(${name} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/${name})
    target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

    target_link_libraries(${name} PUBLIC
        Qt6::Core
        Qt6::Qml
        Qt6::Gui
        ${ARG_LIBS}
    )

    add_library(Ryu::${name} ALIAS ${name})
    add_library(Ryu::${name}plugin ALIAS ${name}plugin)

    if(RYU_INSTALL AND NOT RYU_STATIC)
        set(install_dir "lib/qt6/qml/Ryu/${name}")

        install(TARGETS ${name}
            LIBRARY DESTINATION lib
        )

        install(TARGETS ${name}plugin
            LIBRARY DESTINATION ${install_dir}
        )

        install(DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/Ryu/${name}/"
            DESTINATION ${install_dir}
            FILES_MATCHING
            PATTERN "*.qmltypes"
            PATTERN "qmldir"
            PATTERN "*.qrc"
        )
    endif()
endfunction()

ryu_module(Cleave
    SOURCES
        Cleave/cleave.cpp
        Cleave/cleave.hpp
    LIBS
        ${PIPEWIRE_LIBRARIES}
)

if(TARGET Cleave)
    target_include_directories(Cleave PRIVATE ${PIPEWIRE_INCLUDE_DIRS})
    target_compile_options(Cleave PRIVATE ${PIPEWIRE_CFLAGS_OTHER})
endif()

ryu_module(Audio
    SOURCES
        Audio/audio.cpp
        Audio/audio.hpp
    LIBS
        ${PIPEWIRE_LIBRARIES}
)

if(TARGET Audio)
    target_include_directories(Audio PRIVATE ${PIPEWIRE_INCLUDE_DIRS})
    target_compile_options(Audio PRIVATE ${PIPEWIRE_CFLAGS_OTHER})
endif()

ryu_module(Clipsh
    SOURCES
        Clipsh/clipsh.cpp
        Clipsh/clipsh.hpp
)

ryu_module(Discord
    SOURCES
        Discord/discord.cpp
        Discord/discord.hpp
    LIBS
        Qt6::Network
)

ryu_module(Mpris
    SOURCES
        Mpris/mpris.cpp
        Mpris/mpris.hpp
    LIBS
        Qt6::DBus
)

ryu_module(Power
    SOURCES
        Power/upower.cpp
        Power/upower.hpp
        Power/powerprofiles.cpp
        Power/powerprofiles.hpp
    LIBS
        Qt6::DBus
)
ryu_module(Notifications
    SOURCES
        Notifications/notifications.cpp
        Notifications/notifications.hpp
    LIBS
        Qt6::DBus
        Qt6::Quick
)
ryu_module(Network
    SOURCES
        Network/iwd.cpp
        Network/iwd.hpp
    LIBS
        Qt6::DBus
)
ryu_module(Io
    SOURCES
        Io/process.cpp
        Io/process.hpp
        Io/exec.cpp
        Io/exec.hpp
        Io/fileview.cpp
        Io/fileview.hpp
)

ryu_module(Desktop
    SOURCES
        Desktop/desktop.cpp
        Desktop/desktop.hpp
    LIBS
        Qt6::Quick
)

ryu_module(Ui
    SOURCES
        Ui/scriptmodel.cpp
        Ui/scriptmodel.hpp
    QML_FILES
        Ui/ClippingRectangle.qml
        Ui/IconImage.qml
    LIBS
        Qt6::Quick
    DEPS
        QtQuick
        QtQuick.Effects
)

ryu_module(Pam
    SOURCES
        Pam/pam.cpp
        Pam/pam.hpp
    LIBS
        ${PAM_LIBRARY}
)

if(TARGET Pam)
    target_include_directories(Pam PRIVATE ${PAM_INCLUDE_DIR})
endif()
ryu_module(Hyprland
    SOURCES
        Hyprland/hyprland.cpp
        Hyprland/hyprland.hpp
    LIBS
        Qt6::Network
)

ryu_module(Polkit
    SOURCES
        Polkit/polkit.cpp
        Polkit/polkit.hpp
    LIBS
        Qt6::DBus
        Qt6::Network
)

ryu_module(Ipc
    SOURCES
        Ipc/ipc.cpp
        Ipc/ipc.hpp
    LIBS
        Qt6::Network
)

if("Ipc" IN_LIST RYU_MODULES)
    add_executable(ryuctl Ipc/ryuctl.cpp)
    if(RYU_INSTALL)
        install(TARGETS ryuctl RUNTIME DESTINATION bin)
    endif()
endif()

ryu_module(Sqliter
    SOURCES
        Sqliter/eventsservice.cpp
        Sqliter/eventsservice.h
        Sqliter/sqliterdb.cpp
        Sqliter/sqliterdb.h
    LIBS
        Qt6::Sql
)

ryu_module(Warsa
    SOURCES
        Warsa/warsa.cpp
        Warsa/warsa.hpp
)

ryu_module(Shapes
    SOURCES
        Shapes/materialshape.cpp
        Shapes/materialshape.hpp
        Shapes/materialshapeborder.hpp
        Shapes/svgpath.cpp
        Shapes/svgpath.hpp
        Shapes/morph.cpp
        Shapes/morph.hpp
    LIBS
        Qt6::Quick
        RyuCore
    DEPS
        QtQuick
        Ryu.Core
)

if(TARGET Shapes)
    target_sources(Shapes PRIVATE Shapes/shapedata.inc)
endif()

ryu_module(Extraa
    SOURCES
        Extraa/wavyprogress.cpp
        Extraa/wavyprogress.hpp
        Extraa/materialslider.cpp
        Extraa/materialslider.hpp
        Extraa/materialbuttongroup.cpp
        Extraa/materialbuttongroup.hpp
        Extraa/materialiconbutton.cpp
        Extraa/materialiconbutton.hpp
        Extraa/materialsplitbutton.cpp
        Extraa/materialsplitbutton.hpp
        Extraa/materialswitch.cpp
        Extraa/materialswitch.hpp
        Extraa/materialsearchbox.cpp
        Extraa/materialsearchbox.hpp
        Extraa/materialsearchview.cpp
        Extraa/materialsearchview.hpp
        Extraa/materialmenu.cpp
        Extraa/materialmenu.hpp
        Extraa/materiallist.cpp
        Extraa/materiallist.hpp
        Extraa/materialcheckbox.hpp
        Extraa/materialcheckbox.cpp
        Extraa/materialradiobutton.hpp
        Extraa/materialradiobutton.cpp
        Extraa/materialsegmentedbutton.hpp
        Extraa/materialsegmentedbutton.cpp
        Extraa/materialbutton.hpp
        Extraa/materialbutton.cpp
        Extraa/materialchip.hpp
        Extraa/materialchip.cpp
        Extraa/materialcarousel.hpp
        Extraa/materialcarousel.cpp
    LIBS
        Qt6::Quick
        RyuCore
    DEPS
        QtQuick
        Ryu.Core
)

ryu_module(Widgets
    SOURCES
        Widgets/digitalclock.cpp
        Widgets/digitalclock.hpp
    LIBS
        Qt6::Quick
        RyuCore
    DEPS
        QtQuick
        Ryu.Core
)

if(RYU_INSTALL AND NOT RYU_STATIC)
    install(DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/Ryu/Core/"
        DESTINATION lib/qt6/qml/Ryu/Core
        FILES_MATCHING
        PATTERN "*.qmltypes"
        PATTERN "qmldir"
    )
endif()
