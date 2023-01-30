cmake_minimum_required (VERSION 3.8)
#----------------------------------------------------------------
project ("NES_Emulator")
#----------------------------------------------------------------
# compiler flags
set(CMAKE_CXX_STANDARD 17)
#----------------------------------------------------------------
set(PROJECT_SOURCE_DIR ${PROJECT_SOURCE_DIR}/src)
#----------------------------------------------------------------
# main source
set(SOURCES_CPP
    "${PROJECT_SOURCE_DIR}/Emulator.cpp"
    "${PROJECT_SOURCE_DIR}/Debugger.cpp"
    "${PROJECT_SOURCE_DIR}/GLDisplay.cpp"
    "${PROJECT_SOURCE_DIR}/NESDevice.cpp"
    "${PROJECT_SOURCE_DIR}/NESCPU.cpp"
    "${PROJECT_SOURCE_DIR}/NESPPU.cpp"
    "${PROJECT_SOURCE_DIR}/NESCartrige.cpp"
    "${PROJECT_SOURCE_DIR}/NESController.cpp"  
)
#----------------------------------------------------------------
# main include dirs
set(INCLUDE_DIR
    ${PROJECT_SOURCE_DIR}
    "${PROJECT_SOURCE_DIR}/mappers"
)
#----------------------------------------------------------------
#third party
include("${PROJECT_SOURCE_DIR}/external/CMakeLists.txt")

#################################################################
#################################################################
#################################################################

add_executable (${PROJECT_NAME} ${SOURCES_CPP} ${SOURCES_C} ${EXT_SOURCES_CPP} ${EXT_SOURCES_C})
#----------------------------------------------------------------
target_link_libraries(${PROJECT_NAME} PUBLIC ${EXT_LIBRARIES})
target_link_libraries(${PROJECT_NAME} PUBLIC ${LIBRARIES})
#----------------------------------------------------------------
target_include_directories(${PROJECT_NAME} PUBLIC ${EXT_INCLUDE_DIRS})
target_include_directories(${PROJECT_NAME} PUBLIC ${INCLUDE_DIR})
#----------------------------------------------------------------