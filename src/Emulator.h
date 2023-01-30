#pragma once

#include <cstdio>
#include <chrono>
#include <filesystem>
//----------------------------------------
#pragma warning(push, 0)
#include "GL/gl3w.h"

#define SDL_MAIN_HANDLED
#include "SDL.h"

#include "imgui.h"
#include "backends/imgui_impl_sdl.h"
#include "backends/imgui_impl_opengl3.h"

#include "ImGuiFileDialog.h"
#pragma warning(pop)
//----------------------------------------

#include "Debugger.h"
#include "GLDisplay.h"

#include "NESState.h"
#include "NESCPU.h"
#include "NESPPU.h"
#include "NESCartrige.h"
#include "NESDevice.h"

#define DEFAULT_CFG_WINDOW_WIDTH 800
#define DEFAULT_CFG_WINDOW_HEIGHT 600
#define DEFAULT_CFG_REWIND_BUFFER 182


using chrono_time = std::chrono::microseconds;
using chrono_clock = std::chrono::steady_clock;

class Emulator
{
public:
	Emulator();
	~Emulator();

	int Initialize(int argc, char** argv);
	int MainLoop();

	void ProcessInput();
	void ProcessViewport();
	void ProcessWindows();
	void ProcessFrameTime();
	void ProcessRewindUpdates();

	void LoadConfigFile();
	void UpdateConfigFile();
	void LoadStates();
	void SaveStates();

protected:
	//--------------------------------
	SDL_Window*		m_SDLWindow;
	SDL_GLContext	m_SDLGlContext;
	uint32_t		m_WindowWidth;
	uint32_t		m_WindowHeight;
	//--------------------------------
	bool			m_IsEmulatorOpen;
	//--------------------------------
	ImGuiFileDialog m_FileDialog;
	std::string		m_LastFile;
	std::string     m_LastDirectory;
	//--------------------------------
	//Aux systems
	Debugger		m_Debugger;
	GLDisplay		m_GLDisplay;
	//Emulator soul
	NESDevice		m_NESDevice;
	//State slots
	NESState		m_NESState[8];
	bool		    m_NESSaveStateLatch;
	//--------------------------------
	//Rewind feature
	NESState*		m_RewindBuffer;
	uint32_t		m_RewindBufferSize;
	uint32_t		m_RewindBufferBegin;
	uint32_t		m_RewindBufferLength;
	uint32_t		m_RewindBufferEnd;
	uint32_t		m_RewindBufferIndex;
	bool			m_RewindRecording;
	bool			m_RewindControlLatch;
	bool			m_RewindAdvanceLatch;
	chrono_time::rep		 m_RewindAdvanceLatchCooldown;
	//--------------------------------
	//For update timing and fps calculation
	chrono_clock::time_point m_EmulatorFrameStartTimestamp;
	static const size_t	     m_EmulatorFrameTimeCacheSize = 256;
	size_t					 m_EmulatorFrameTimeCacheIndex;
	chrono_time::rep		 m_EmulatorFrameTimeCache[m_EmulatorFrameTimeCacheSize];
	chrono_time::rep		 m_EmulatorTimeAccumulator;
	chrono_time::rep		 m_EmulatorFrameTimeRaw;
	double					 m_EmulatorFrameTimeAverage;
	
	double					 m_DeviceUpdateTargetTiming;
	double					 m_DeviceUpdateResedueTime ;
	uint32_t				 m_DeviceFramesAccumulator;
	uint32_t				 m_DeviceFramesPerSecond;
	double					 m_DeviceFrameTime;
	//--------------------------------
	//Viewport
	bool    m_IsViewportSelected;
	bool    m_ViewportWindowedMode;
	uint8_t m_ViewportVerticalCutoff = 255;
	//--------------------------------
	//Debug windows
	bool		 m_ShowCPUMemoryViewer;
	bool		 m_ShowPPUMemoryViewer;
	bool		 m_ShowCPUControls;
	bool		 m_ShowPPUData = true;
	bool		 m_ShowImGuiStyleEditor;
	//--------------------------------	
};