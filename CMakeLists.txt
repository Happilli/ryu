cmake_minimum_required(VERSION 3.20)
project(ryu)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Qml Gui Quick Sql ShaderTools)
find_package(PkgConfig REQUIRED)
pkg_check_modules(PIPEWIRE REQUIRED libpipewire-0.3)

qt_policy(SET QTP0001 NEW)

function(ryu_module name)
    cmake_parse_arguments(ARG "" "" "SOURCES;LIBS" ${ARGN})

    qt_add_qml_module(${name}
        URI Ryu.${name}
        VERSION 1.0
        SOURCES ${ARG_SOURCES}
        NO_PLUGIN_OPTIONAL
        OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/Ryu/${name}"
    )

    target_include_directories(${name} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/${name})
    target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

    target_link_libraries(${name} PRIVATE
        Qt6::Core
        Qt6::Qml
        Qt6::Gui
        ${ARG_LIBS}
    )

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
endfunction()

ryu_module(Cleave
    SOURCES
        Cleave/cleave.cpp
        Cleave/cleave.hpp
    LIBS
        ${PIPEWIRE_LIBRARIES}
)

target_include_directories(Cleave PRIVATE ${PIPEWIRE_INCLUDE_DIRS})
target_compile_options(Cleave PRIVATE ${PIPEWIRE_CFLAGS_OTHER})

ryu_module(Clipsh
    SOURCES
        Clipsh/clipsh.cpp
        Clipsh/clipsh.hpp
)

ryu_module(Drawness
    SOURCES
        Drawness/drawness.cpp
        Drawness/drawness.hpp
    LIBS
        Qt6::Quick
)

qt_add_shaders(Drawness "drawness_shaders"
    PREFIX
        "/qt/qml/Ryu"
    FILES
        "Drawness/jiggle/jiggle.vert"
        "Drawness/jiggle/jiggle.frag"
)

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
        Shapes/springdriver.hpp
        Shapes/svgpath.cpp
        Shapes/svgpath.hpp
        Shapes/morph.cpp
        Shapes/morph.hpp
    LIBS
        Qt6::Quick
)

target_sources(Shapes PRIVATE Shapes/shapedata.inc)

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
    LIBS
        Qt6::Quick
)
