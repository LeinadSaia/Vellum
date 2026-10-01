cmake_minimum_required(VERSION 3.16)
project(EnsinadorDeIngles VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Qt6 com os módulos necessários
find_package(Qt6 REQUIRED COMPONENTS
    Core
    Widgets
    Pdf
    PdfWidgets
    Network
)

set(SOURCES
    src/main.cpp
    src/MainWindow.cpp
    src/NetworkManager.cpp
)

set(HEADERS
    src/MainWindow.h
    src/NetworkManager.h
)

qt_add_executable(EnsinadorDeIngles
    ${SOURCES}
    ${HEADERS}
)

target_link_libraries(EnsinadorDeIngles PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Pdf
    Qt6::PdfWidgets
    Qt6::Network
)

# Windows: oculta o console e usa ícone nativo
set_target_properties(EnsinadorDeIngles PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
)

# Copia as DLLs do Qt para a pasta de build (Windows)
if(WIN32)
    add_custom_command(TARGET EnsinadorDeIngles POST_BUILD
        COMMAND Qt6::windeployqt --no-translations $<TARGET_FILE:EnsinadorDeIngles>
        COMMENT "Copiando dependências Qt (windeployqt)..."
    )
endif()
