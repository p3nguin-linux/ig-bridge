cmake_minimum_required(VERSION 3.5)
project(instagrambridge CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(APP_NAME instagrambridge)
set(FULL_NAME instagrambridge.aprilpixelrain)

find_package(Qt5 REQUIRED COMPONENTS Core Gui Qml Quick WebEngine)

if(NOT DEFINED CLICK_ARCH)
    execute_process(
        COMMAND dpkg-architecture -qDEB_HOST_ARCH
        OUTPUT_VARIABLE CLICK_ARCH
        OUTPUT_STRIP_TRAILING_WHITESPACE)
endif()

add_executable(${APP_NAME}
    src/main.cpp
    src/bridge.cpp
    src/bridge.h)

target_link_libraries(${APP_NAME}
    Qt5::Core Qt5::Gui Qt5::Qml Qt5::Quick Qt5::WebEngine)

configure_file(manifest.json.in ${CMAKE_CURRENT_BINARY_DIR}/manifest.json @ONLY)
configure_file(instagrambridge.desktop.in ${CMAKE_CURRENT_BINARY_DIR}/${APP_NAME}.desktop @ONLY)

install(TARGETS ${APP_NAME} RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX})
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/manifest.json
    ${CMAKE_CURRENT_BINARY_DIR}/${APP_NAME}.desktop
    apparmor.json
    DESTINATION ${CMAKE_INSTALL_PREFIX})
install(DIRECTORY qml DESTINATION ${CMAKE_INSTALL_PREFIX})
install(DIRECTORY assets DESTINATION ${CMAKE_INSTALL_PREFIX})
