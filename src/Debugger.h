#pragma once

#include <cmath>

#include "imgui.h"
#include "imgui_memory_editor.h"

#include "NESDevice.h"
#include "GLDisplay.h"

class Debugger
{
public:
	Debugger();

	void SetGLDisplay(GLDisplay* glDisplay);
	void SetNESDevice(NESDevice* nesDevice);

	void Initialize();
	void Destroy();

	bool ShowCPUMemory();
	bool ShowPPUMemory();
	bool ShowCPUControls();
	bool ShowPPUData();

	void Update();

	bool IsCycleHijackActive();

private:
	//Helper function for ShowPPUData pallete subwindow
	void DrawPalette(uint16_t address);

	GLDisplay* m_GLDisplayPtr;
	NESDevice* m_NESDevicePtr;

	MemoryEditor m_CPUMemoryEditor;
	MemoryEditor m_PPUMemoryEditor;
	MemoryEditor m_OAMMemoryEditor;

	bool	 m_EnableAutomaticAdvance;
	uint32_t m_MaxInstructionsQueued;
	uint32_t m_InstructionsQueued;

	//CPU Controls stuff
	bool m_EnableAsmListings;
	bool m_EnableRegistersTampering;
	//PPU Data stuff
	uint8_t m_PaletteCache[0x20] = { 0x00 };
	uint8_t m_SelectedPalette;
	uint8_t m_Patterntable0UpdateMode;
	uint8_t m_Patterntable1UpdateMode;
	uint8_t m_PalettesUpdateMode;
	uint8_t m_NametableUpdateMode;


	//Internal
	uint32_t m_InternalClock = 0;
	uint32_t m_StoredPPUFrameCounter1 = 0;
	uint32_t m_StoredPPUFrameCounter2 = 0;

};