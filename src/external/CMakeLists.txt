#----------------------------------------------------------------
set(EXTERNAL_DIR ${CMAKE_SOURCE_DIR}/src/external)
set(SDL2_DISABLE_INSTALL ON CACHE BOOL "" FORCE)
set(BUILD_STATIC_LIBS OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
#----------------------------------------------------------------
#SDL
set(SDL_PATH "${EXTERNAL_DIR}/SDL2" CACHE INTERNAL "" FORCE) 
	add_subdirectory(${SDL_PATH} "${SDL_PATH}/out")

set(SDL_LIBRARY 
	"SDL2main"
	"SDL2-static"
)

set(SDL_INCLUDE_DIR 
	"${SDL_PATH}/include"
	CACHE INTERNAL "" FORCE)
#----------------------------------------------------------------
#GL3W
set(GL3W_PATH "${EXTERNAL_DIR}/gl3w" CACHE INTERNAL "" FORCE) 
set(GL3W_SOURCES_C 
	"${GL3W_PATH}/src/gl3w.c"
)

set(GL3W_INCLUDE_DIR
	"${GL3W_PATH}/include"
)
#----------------------------------------------------------------
#IMGUI
set(IMGUI_PATH "${EXTERNAL_DIR}/imgui" CACHE INTERNAL "" FORCE) 
set(IMGUI_SOURCES_CPP
	"${IMGUI_PATH}/imgui.cpp"
	"${IMGUI_PATH}/imgui_demo.cpp"
	"${IMGUI_PATH}/imgui_draw.cpp"
	"${IMGUI_PATH}/imgui_tables.cpp"
	"${IMGUI_PATH}/imgui_widgets.cpp"
	"${IMGUI_PATH}/backends/imgui_impl_sdl.cpp"
	"${IMGUI_PATH}/backends/imgui_impl_opengl3.cpp"
)
set(IMGUI_INCLUDE_DIR
	"${IMGUI_PATH}"
	"${IMGUI_PATH}/backends"
)
#----------------------------------------------------------------
#IMGUI_WIDGETS
set(IMGUI_WIDGETS_SOURCES_CPP
	"${EXTERNAL_DIR}/ImGuiFileDialog/ImGuiFileDialog.cpp"
)

set(IMGUI_WIDGETS_INCLUDE_DIRS
	"${EXTERNAL_DIR}/imgui_widgets/imgui_memory_editor"
	"${EXTERNAL_DIR}/ImGuiFileDialog"
)
#----------------------------------------------------------------
#concat all data
set(EXT_SOURCES_CPP 
	${EXT_SOURCES_CPP}
	${IMGUI_SOURCES_CPP}
	${IMGUI_WIDGETS_SOURCES_CPP}
)

set(EXT_SOURCES_C 
	${EXT_SOURCES_C}
	${GL3W_SOURCES_C}
)

set(EXT_INCLUDE_DIRS 
	${EXT_INCLUDE_DIR}
	${SDL_INCLUDE_DIR}
	${GL3W_INCLUDE_DIR} 
	${IMGUI_INCLUDE_DIR}
	${IMGUI_WIDGETS_INCLUDE_DIRS}
)

set(EXT_LIBRARIES
	${EXT_LIBRARY}
	${SDL_LIBRARY}
)
#----------------------------------------------------------------
message(STATUS "EXT_SOURCES_CPP = ${EXT_SOURCES_CPP}")
message(STATUS "EXT_SOURCES_C = ${EXT_SOURCES_C}")
message(STATUS "EXT_INCLUDE_DIRS = ${EXT_INCLUDE_DIRS}")
message(STATUS "EXT_LIBRARIES = ${EXT_LIBRARIES}")
#----------------------------------------------------------------