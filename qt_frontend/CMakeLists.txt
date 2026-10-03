cmake_minimum_required(VERSION 3.16)
project(Vellum VERSION 1.0.0 LANGUAGES CXX)

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
    resources.qrc
)

if(WIN32)
    list(APPEND SOURCES resources/app.rc)
endif()

set(HEADERS
    src/MainWindow.h
    src/NetworkManager.h
)

qt_add_executable(Vellum
    ${SOURCES}
    ${HEADERS}
)

target_link_libraries(Vellum PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Pdf
    Qt6::PdfWidgets
    Qt6::Network
)

# Windows: oculta o console e usa ícone nativo
set_target_properties(Vellum PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
    OUTPUT_NAME "Vellum"
)

# Compatibilidade no Linux com scripts anteriores
if(UNIX AND NOT APPLE)
    add_custom_command(TARGET Vellum POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E create_symlink $<TARGET_FILE_NAME:Vellum> ${CMAKE_BINARY_DIR}/EnsinadorDeIngles
        COMMENT "Criando link simbolico de compatibilidade EnsinadorDeIngles -> Vellum"
    )
endif()

# Copia as DLLs do Qt para a pasta de build (Windows)
if(WIN32)
    add_custom_command(TARGET Vellum POST_BUILD
        COMMAND Qt6::windeployqt --no-translations $<TARGET_FILE:Vellum>
        COMMENT "Copiando dependências Qt (windeployqt)..."
    )
endif()
