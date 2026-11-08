cmake_minimum_required(VERSION 3.16)

project(AIGallery VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/utf-8)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
endif()

set(THIRD_PARTY_DIR "${CMAKE_SOURCE_DIR}/third_party")

set(ONNXRUNTIME_ROOT "${THIRD_PARTY_DIR}/onnxruntime" CACHE PATH "Путь к ONNX Runtime")
if(NOT EXISTS "${ONNXRUNTIME_ROOT}/include/onnxruntime_cxx_api.h")
    message(FATAL_ERROR "ONNX Runtime не найден: ${ONNXRUNTIME_ROOT}")
endif()
set(ONNXRUNTIME_INCLUDE_DIR "${ONNXRUNTIME_ROOT}/include")
set(ONNXRUNTIME_LIBRARY     "${ONNXRUNTIME_ROOT}/lib/onnxruntime.lib")

set(ONNXRUNTIME_GENAI_ROOT "${THIRD_PARTY_DIR}/onnxruntime-genai" CACHE PATH "Путь к ONNX Runtime GenAI")
if(NOT EXISTS "${ONNXRUNTIME_GENAI_ROOT}/include/ort_genai.h")
    message(FATAL_ERROR "ONNX Runtime GenAI не найден: ${ONNXRUNTIME_GENAI_ROOT}")
endif()

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets LinguistTools)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets LinguistTools)

set(TS_FILES AIGallery_ru_RU.ts)

set(PROJECT_SOURCES
        main.cpp
        aigallery.cpp
        aigallery.h
        aigallery.ui
        genaiengine.cpp
        genaiengine.h
        genaiworker.cpp
        genaiworker.h
        descriptionmanager.cpp
        descriptionmanager.h
        descriptiondialog.cpp
        descriptiondialog.h
        promptdialog.cpp
        promptdialog.h
        batchreportdialog.cpp
        batchreportdialog.h
        themedialog.cpp
        themedialog.h
        embeddingmanager.cpp
        embeddingmanager.h
        toggleswitch.cpp
        toggleswitch.h
        ${TS_FILES}
)


if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(AIGallery MANUAL_FINALIZATION ${PROJECT_SOURCES})
    qt_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TS_FILES})
else()
    if(ANDROID)
        add_library(AIGallery SHARED ${PROJECT_SOURCES})
    else()
        add_executable(AIGallery ${PROJECT_SOURCES})
    endif()
    qt5_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TS_FILES})
endif()

target_include_directories(AIGallery PRIVATE
    ${ONNXRUNTIME_INCLUDE_DIR}
    ${ONNXRUNTIME_GENAI_ROOT}/include
)

target_link_libraries(AIGallery PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    ${ONNXRUNTIME_LIBRARY}
    ${ONNXRUNTIME_GENAI_ROOT}/lib/onnxruntime-genai.lib
)

add_custom_command(TARGET AIGallery POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${ONNXRUNTIME_ROOT}/lib/onnxruntime.dll"
        "$<TARGET_FILE_DIR:AIGallery>"
    COMMENT "Копирование onnxruntime.dll"
)

add_custom_command(TARGET AIGallery POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${ONNXRUNTIME_GENAI_ROOT}/lib/onnxruntime-genai.dll"
        "$<TARGET_FILE_DIR:AIGallery>"
    COMMENT "Копирование onnxruntime-genai.dll"
)
# Копируем UForm-модель рядом с exe при каждой сборке
add_custom_command(TARGET AIGallery POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/third_party/uform-multilingual"
        "$<TARGET_FILE_DIR:AIGallery>/uform-multilingual"
    COMMENT "Копирование UForm-модели рядом с AIGallery.exe"
)

if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.AIGallery)
endif()
set_target_properties(AIGallery PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS AIGallery
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(AIGallery)
endif()