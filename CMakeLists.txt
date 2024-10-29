cmake_minimum_required(VERSION 3.16)

project(ids LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(ids main.cpp
	all_attack_detection.cpp all_attack_detection.h CANStats.cpp CANStats.h check_clock_error.cpp check_clock_error.h cQueue.cpp cQueue.h header.h main.cpp periodic.cpp periodic.h)

include(GNUInstallDirs)
install(TARGETS ids
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
