cmake_minimum_required(VERSION 3.0)

project(simple-player)

find_package(PkgConfig REQUIRED)
pkg_search_module(GLIB REQUIRED glib-2.0) 
pkg_check_modules(GSTREAMER REQUIRED gstreamer-1.0)
pkg_check_modules(GST_APP REQUIRED gstreamer-app-1.0)
pkg_check_modules(GST_VIDEO REQUIRED gstreamer-video-1.0)
pkg_check_modules(GTK3 REQUIRED gtk+-3.0)

message ("GSTREAMER_LIBRARIES = " ${GSTREAMER_INCLUDE_DIRS})
message ("GSTREAMER_INCLUDE_DIRS = " ${GSTREAMER_LIBRARY_DIRS})

add_executable(
    ${PROJECT_NAME}
    basic-tutorial-5.c
)

target_include_directories(
    ${PROJECT_NAME}
    PRIVATE 
    ${GSTREAMER_INCLUDE_DIRS}
    ${GTK3_INCLUDE_DIRS}
)

target_link_libraries(
    ${PROJECT_NAME}
    ${GTK3_LIBRARIES}
    ${GSTREAMER_LIBRARY_DIRS}
    ${GST_APP_LIBRARIES}
    ${GST_VIDEO_LIBRARIES}
)