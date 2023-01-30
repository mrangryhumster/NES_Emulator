#pragma once

#include <cstdint>

#include "NESState.h"
#include "NESCPU.h"
#include "NESPPU.h"
#include "NESCartrige.h"
#include "NESController.h"

class NESDevice
{
public:
	NESDevice();

	void SetCPUCycleDivider(uint32_t divider);
	void SetPPUCycleDivider(uint32_t divider);

	void Reset();

	NESCPU& GetCPU();
	NESPPU& GetPPU();
	NESCartrige& GetCartrige();
	NESController& GetController();

	//CPU Bus RW operations
	uint8_t CPURead(uint16_t address);
	void    CPUWrite(uint16_t address, uint8_t data);

	//PPU Bus RW operations
	uint8_t PPURead(uint16_t address);
	void    PPUWrite(uint16_t address, uint8_t data);

	//Debug operations used to 'peek' into the memory without modifying it
	uint8_t CPUPeek(uint16_t address);
	uint8_t PPUPeek(uint16_t address);

	void Update();

	bool SaveState(NESState& state);
	bool LoadState(NESState& state);

	//Debugging modes
	enum class DeviceMode : uint32_t
	{
		Pause,
		Running,
		AdvanceCycle,
		AdvanceCPUInstruction,
		AdvancePPUFrame,
		AdvancePPULine
	};

	DeviceMode DeviceMode;

	uint32_t DeviceCycle;
	uint32_t CPUCycleDivider, CPUMasterCycle;
	uint32_t PPUCycleDivider, PPUMasterCycle;

protected:

	void MasterCycle();

	//SubSystems
	NESCPU m_CPU;
	NESPPU m_PPU;
	NESCartrige m_Cartrige;
	NESController m_Controller;

	//cpu bus
	uint8_t m_RAM[0x0800];			//2KB internal RAM
	uint8_t m_AuxRegisters[0x0020]; //NES APU and I/O registers
	//ppu bus
	uint8_t m_VRAM[0x0800];			//Namatables VRAM

};