#include "NESDevice.h"

NESDevice::NESDevice() :
	m_CPU(this),
	m_PPU(this)
{

	//NTSC : 12/4 [3/1]
	//PAL  : 16/5
	CPUCycleDivider = 3;
	PPUCycleDivider = 1;

	this->Reset();
	DeviceMode = DeviceMode::Pause;
}

void NESDevice::SetCPUCycleDivider(uint32_t divider)
{
	CPUCycleDivider = divider;
}
void NESDevice::SetPPUCycleDivider(uint32_t divider)
{
	PPUCycleDivider = divider;
}

void NESDevice::Reset()
{
	DeviceCycle = 0;
	CPUMasterCycle = 0;
	PPUMasterCycle = 0;

	//Clear memory
	//cpu bus
	memset(m_RAM, 0, 0x0800);
	memset(m_AuxRegisters, 0, 0x0020);
	//ppu bus
	memset(m_VRAM, 0, 0x0800);

	m_CPU.Reset();
	m_PPU.Reset();
	m_Cartrige.Reset();
	m_Controller.Reset();

	//Debug palette
	PPUWrite(0x3F00, 0x00);
	PPUWrite(0x3F01, 0x03);
	PPUWrite(0x3F02, 0x06);
	PPUWrite(0x3F03, 0x0A);
}

NESCPU& NESDevice::GetCPU()
{
	return m_CPU;
}

NESPPU& NESDevice::GetPPU()
{
	return m_PPU;
}

NESCartrige& NESDevice::GetCartrige()
{
	return m_Cartrige;
}

NESController& NESDevice::GetController()
{
	return m_Controller;
}

uint8_t NESDevice::CPURead(uint16_t address)
{
	// $0000–$07FF : 2KB Internal RAM
	// $0800-$1FFF : Mirror of $0000–$07FF
	if (address <= 0x1FFF)
	{
		return m_RAM[address & 0x07FF];
	}

	// $2000-$2007 : NES PPU registers
	// $2008-$3FFF : Mirrors of $2000–$2007 (repeats every 8 bytes) 
	if (address <= 0x3FFF)
	{
		return m_PPU.CPURead(address);
	}

	// $4000-$4017 : NES APU and I/O registers
	// $4018-$401F : APU and I/O functionality [unused]
	if (address <= 0x401F)
	{
		//Controllers
		if (address == 0x4016 || address == 0x4017)
		{
			return m_Controller.CPURead(address);
		}

		return m_AuxRegisters[address & 0x001F];
	}

	// $4020-$FFFF : Cartrige space
	return m_Cartrige.CPURead(address);
}

void NESDevice::CPUWrite(uint16_t address, uint8_t data)
{
	// $0000–$07FF : 2KB Internal RAM
	// $0800-$1FFF : Mirror of $0000–$07FF
	if (address <= 0x1FFF)
	{
		m_RAM[address & 0x07FF] = data;
		return;
	}

	// $2000-$2007 : NES PPU registers
	// $2008-$3FFF : Mirrors of $2000–$2007 (repeats every 8 bytes) 
	if (address <= 0x3FFF)
	{
		m_PPU.CPUWrite(address, data);
		return;
	}

	// $4000-$4017 : NES APU and I/O registers
	// $4018-$401F : APU and I/O functionality [unused]
	if (address <= 0x401F)
	{	
		//OAMDMA
		if (address == 0x4014)
		{
			m_CPU.State.DMARequest = true;
			m_CPU.DMA.Address = (uint16_t)data << 8;
		}
		//Controllers
		if (address == 0x4016)
		{
			m_Controller.CPUWrite(address, data);
			return;
		}

		m_AuxRegisters[address & 0x001F] = data;
		return;
	}

	// $4020-$FFFF : Cartrige space
	return m_Cartrige.CPUWrite(address, data);
}

uint8_t NESDevice::PPURead(uint16_t address)
{
	// $0000–$0FFF : Pattern table 0 
	// $1000–$1FFF : Pattern table 1 
	if (address <= 0x1FFF)
	{
		return m_Cartrige.PPURead(address);
	}

	// $2000-$23FF : Nametable 0
	// $2400-$27FF : Nametable 1
	// $2800-$2BFF : Nametable 2
	// $2C00-$2FFF : Nametable 3
	// $2008-$3EFF : Mirrors of $2000-$2EFF
	if (address <= 0x3EFF)
	{
		uint16_t new_address;

		//Check if operation intercepted by cartrige mapper
		// if true - operation redirects to cartrige
		// if false - operation will access onboard VRAM via new address (mirroring)
		if (m_Cartrige.PPUInterceptVRAM(address & 0x0FFF, &new_address))
		{
			return m_Cartrige.PPURead(address);
		}
		else
		{
			return m_VRAM[new_address];
		}
	}

	// $3F00-$3F1F : Palette RAM indexes 
	// $3F20-$3FFF : Mirrors of $3F00-$3F1F
	if (address <= 0x3FFF)
	{
		return m_PPU.PPURead(address);
	}

	return 0x0F;
}

void NESDevice::PPUWrite(uint16_t address, uint8_t data)
{
	// $0000–$0FFF : Pattern table 0 
	// $1000–$1FFF : Pattern table 1 
	if (address <= 0x1FFF)
	{
		m_Cartrige.PPUWrite(address, data);
		return;
	}

	// $2000-$23FF : Nametable 0
	// $2400-$27FF : Nametable 1
	// $2800-$2BFF : Nametable 2
	// $2C00-$2FFF : Nametable 3
	// $2008-$3EFF : Mirrors of $2000-$3EFF
	if (address <= 0x3EFF)
	{
		uint16_t new_address;
		//Check if operation intercepted by cartrige mapper
		// if true - operation redirects to cartrige
		// if false - operation will access onboard VRAM via new address (mirroring)
		if (m_Cartrige.PPUInterceptVRAM(address & 0x0FFF, &new_address))
		{
			m_Cartrige.PPUWrite(address, data);
		}
		else
		{
			m_VRAM[new_address] = data;
		}
		return;
	}

	// $3F00-$3F1F : Palette RAM indexes 
	// $3F20-$3FFF : Mirrors of $3F00-$3F1F
	if (address <= 0x3FFF)
	{
		m_PPU.PPUWrite(address, data);
		return;
	}
}

uint8_t NESDevice::CPUPeek(uint16_t address)
{
	//Modified CPURead
	if (address <= 0x1FFF)  
		return m_RAM[address & 0x07FF];

	if (address <= 0x3FFF)	
		return m_PPU.CPUPeek(address);

	if (address <= 0x401F)
	{
		//OAMDMA (Read works for DEBUG propourses)
		if (address == 0x4014)
		{
			return (m_CPU.DMA.Address >> 8);
		}
		//Controllers
		if (address == 0x4016 || address == 0x4017)
		{
			return m_Controller.CPUPeek(address);
		}

		return m_AuxRegisters[address & 0x001F];
	}

	// $4020-$FFFF : Cartrige space
	return m_Cartrige.CPURead(address);
}

uint8_t NESDevice::PPUPeek(uint16_t address)
{
	if (address <= 0x1FFF)
		return m_Cartrige.PPURead(address);

	if (address <= 0x3EFF)
	{
		uint16_t new_address;
		if (m_Cartrige.PPUInterceptVRAM(address & 0x0FFF, &new_address))
			return m_Cartrige.PPURead(address);
		else
			return m_VRAM[new_address];
	}

	if (address <= 0x3FFF)
		return m_PPU.PPURead(address);

	return 0x0F;
}

void NESDevice::Update()
{
	bool IsRunning = true;
	while (IsRunning)
	{
		switch (DeviceMode)
		{
			case DeviceMode::Pause: // ---------------------------------------- Pause
				IsRunning = false;
				break;

			case DeviceMode::Running: // --------------------------------- Normal mode
				this->MasterCycle();
				if (m_PPU.IsFrameReady())
				{
					//Drain resedue cycles
					while (PPUMasterCycle) { this->MasterCycle(); }
					IsRunning = false;
				}
				break;
	
			case DeviceMode::AdvanceCycle: // ---------------------- Advance one cycle
				this->MasterCycle();
				DeviceMode = DeviceMode::Pause; //Fallback to pause
				IsRunning = false;
				break;

			case DeviceMode::AdvanceCPUInstruction: // -------- Operation advance mode 
				this->MasterCycle();
				if (m_CPU.IsReady())
				{
					DeviceMode = DeviceMode::Pause; //Fallback to pause
					//Drain resedue cycles
					while (CPUMasterCycle) { this->MasterCycle(); }
					IsRunning = false;
				}
				break;

			case DeviceMode::AdvancePPUFrame: // ------------------ Frame advance mode
				//Same as normal but pause after frame is ready
				this->MasterCycle();
				if (m_PPU.IsFrameReady())
				{
					DeviceMode = DeviceMode::Pause; //Fallback to pause
					//Drain resedue cycles
					while (PPUMasterCycle) { this->MasterCycle(); }
					IsRunning = false;
				}
				break;
			case DeviceMode::AdvancePPULine: // -------------------- Line advance mode
				//Same as normal but pause after frame is ready
				this->MasterCycle();
				if (m_PPU.IsLineReady())
				{
					DeviceMode = DeviceMode::Pause; //Fallback to pause
					//Drain resedue cycles
					while (PPUMasterCycle) { this->MasterCycle(); }
					IsRunning = false;
				}
				break;	
			default:  // ------------------------------------------------------- Pause
				IsRunning = false;
				break;
		}
	}
}

bool NESDevice::SaveState(NESState& state)
{
	state.Seek(0);

	state.Write(m_RAM,			sizeof(uint8_t) * 0x0800);
	state.Write(m_AuxRegisters, sizeof(uint8_t) * 0x0020);
	state.Write(m_VRAM,			sizeof(uint8_t) * 0x0800);

	if (!m_CPU.SaveState(state)) return false;
	if (!m_PPU.SaveState(state)) return false;
	if (!m_Cartrige.SaveState(state)) return false;

	state.Write(nullptr, 0);

	return true;
}

bool NESDevice::LoadState(NESState& state)
{
	if (!state.IsValid()) return false;

	state.Seek(0);

	state.Read(m_RAM, sizeof(uint8_t) * 0x0800);
	state.Read(m_AuxRegisters, sizeof(uint8_t) * 0x0020);
	state.Read(m_VRAM, sizeof(uint8_t) * 0x0800);

	if (!m_CPU.LoadState(state)) return false;
	if (!m_PPU.LoadState(state)) return false;
	if (!m_Cartrige.LoadState(state)) return false;

	return true;
}

void NESDevice::MasterCycle()
{

	// ******** CPU ********
	if (CPUMasterCycle == 0)
	{
		m_CPU.Update();
	}

	// ******** PPU ********
	if (PPUMasterCycle == 0)
	{
		m_PPU.Update();
	}
	// ******** APU ********
		/* TODO */

	// ******** PERIPHERALS ********
	m_Controller.Update();
	m_Cartrige.Update(); //CARTRIGE (Mapper and stuff)


	// ******** INTERNAL COMMUNICATIONS ********

	//Check if ppu prepared interrupt for cpu
	if (m_PPU.IsEmitingNMI)
	{
		//Add request to cpu
		m_CPU.State.NMIRequest = true;
		//NMI is scheduled - disabling it
		m_PPU.IsEmitingNMI = false;
	}
	
	// ******** DEBUG ********
	//Force pause if cpu is halted
	if (m_CPU.State.Halted)
	{
		DeviceMode = DeviceMode::Pause; //Fallback to pause
	}

	//Advance Cycles
	if (CPUMasterCycle == 0)
		CPUMasterCycle = CPUCycleDivider;
	CPUMasterCycle--;

	if (PPUMasterCycle == 0)
		PPUMasterCycle = PPUCycleDivider;
	PPUMasterCycle--;

	DeviceCycle++;
}

