#define _CRT_SECURE_NO_WARNINGS

#include <string>

#include "NESCPU.h"
#include "NESDevice.h"

#define READ_CYCLE __READ_CYCLE = true;
#define WRITE_CYCLE __WRITE_CYCLE = true;

NESCPU::NESCPU(NESDevice* nesDevice)
{
	this->m_NESDevicePtr = nesDevice;
	State.CyclesTotal = 0;
	State.CycleSkip = 0;
	State.Halted = false;
	State.DMATransfer = false;
	
	prepare_table();
}

void NESCPU::Reset()
{
	Registers.PC = ReadBus(0xFFFC) | (ReadBus(0xFFFD) << 8);
	Registers.AC = 0;
	Registers.XR = 0;
	Registers.YR = 0;
	Registers.SR = 0x20;
	Registers.SP = 0xFD;

	

	State.IRQRequest = false;
	State.NMIRequest = false;
	State.DMARequest = false;

	State.Ready = false;
	State.Halted = false;
	State.NMIActive = false;
	State.IRQActive = false;
	State.DMATransfer = false;

	State.CyclesTotal = 0;
	State.CycleCounter = 0;
	State.CycleInternal = 0;
	State.CycleSkip = 8;
	State.Ready = true;
}

void NESCPU::Update()
{
	if (State.Halted) return;

	if (!State.DMATransfer) //Normal mode
	{
		if (State.CycleSkip == 0)
		{
			if (State.Ready)
			{
				if (State.DMARequest)
				{
					State.DMARequest = false;
					State.DMATransfer = true;
				}
				else
				{
					State.CycleCounter = 0;
					State.CycleInternal = 0;
					State.CycleSkip = 0;
					State.Ready = false;

					if (State.NMIRequest)
					{
						State.NMIRequest = false;
						State.NMIActive = true;
						execute_nmi();
					}
					else if (State.IRQRequest && !getFlag(SRFlag::InterruptBit))
					{
						State.IRQRequest = false;
						State.IRQActive = true;
						execute_irq();
					}
					else
					{
						for (uint32_t i = 1; i < 8; i++)
							State.LastOperations[i - 1] = State.LastOperations[i];
						State.LastOperations[7] = Registers.PC;

						uint8_t opcode = ReadBus(Registers.PC++);
						auto& inst = m_InstructionLookup[opcode];

						State.CurrentOpCode = opcode;
						State.CurrentAddrMode = inst.AddressMode;
					}
				}
			}
			else
			{
				if (State.NMIActive)
				{
					execute_nmi();
				}
				else if (State.IRQActive)
				{
					execute_irq();
				}
				else
				{
					auto& inst = m_InstructionLookup[State.CurrentOpCode];
					while ((this->*inst.fn)() == false)
						State.CycleInternal++;
				}
			}

			if (!State.DMATransfer)
				State.CycleInternal++;
		}
		else
		{
			State.CycleSkip--;
		}
		State.CycleCounter++;
	}
	else //Suspended - DMA in progress
	{
		if (DMA.SkipCycle)
		{
			DMA.SkipCycle = (State.CyclesTotal & 0x01);
		}
		else
		{
			if (State.CyclesTotal & 0x01)
			{
				DMA.Buffer = ReadBus(DMA.Address);
				DMA.Address++;
			}
			else
			{
				WriteBus(0x2004, DMA.Buffer);
				if ((DMA.Address & 0x00FF) == 0)
				{
					State.DMATransfer = false;
					DMA.SkipCycle = true;
					State.Ready = true;
				}
			}
		}
	}

	State.CyclesTotal++;
}

bool NESCPU::IsReady()
{
	return State.Ready && State.CycleSkip == 0;
}

bool NESCPU::SaveState(NESState& state)
{
	state.Write(&State,		sizeof(NESCPU::State));
	state.Write(&DMA,		sizeof(NESCPU::DMA));
	state.Write(&Registers,	sizeof(NESCPU::Registers));
	state.Write(&DataBus,	sizeof(NESCPU::DataBus));

	return true;
}

bool NESCPU::LoadState(NESState& state)
{
	state.Read(&State,		sizeof(NESCPU::State));
	state.Read(&DMA,		sizeof(NESCPU::DMA));
	state.Read(&Registers,	sizeof(NESCPU::Registers));
	state.Read(&DataBus,	sizeof(NESCPU::DataBus));

	return true;
}

void NESCPU::setFlag(SRFlag flag, bool val)
{
	if (val)
		Registers.SR |= static_cast<uint8_t>(flag);
	else
		Registers.SR &= ~static_cast<uint8_t>(flag);
}

bool NESCPU::getFlag(SRFlag flag)
{
	return (Registers.SR & static_cast<uint8_t>(flag));
}

uint8_t NESCPU::ReadBus(uint16_t address)
{
	return  m_NESDevicePtr->CPURead(address);
}

void NESCPU::WriteBus(uint16_t address, uint8_t byte)
{
	m_NESDevicePtr->CPUWrite(address, byte);
}

std::vector<std::string> NESCPU::Disassemble(uint16_t address, uint32_t count, bool include_previous)
{
	std::vector<std::string> listing;

	if (include_previous)
	{
		listing.reserve(count + 8);
		for(uint32_t i = 0; i < 8; i++)
			listing.push_back(disassemble_op(State.LastOperations[i], nullptr));
	}
	else
	{
		listing.reserve(count);
	}

	uint16_t next_opcode_address = address;
	for (uint32_t i = 0; i < count; i++)
	{
		listing.push_back(disassemble_op(next_opcode_address,&next_opcode_address));
	}

	return listing;
}

std::string NESCPU::disassemble_op(uint16_t address, uint16_t* next_address)
{
	uint8_t opcode = m_NESDevicePtr->CPUPeek(address);
	Instruction& instruction = m_InstructionLookup[opcode];

	uint8_t bytes[2] = { 0 };
	for (size_t i = 0; i < instruction.Size - 1; i++)
		bytes[i] = m_NESDevicePtr->CPUPeek(address + 1 + (uint16_t)i);

	char buffer[48] = { 0 };

	if (instruction.AddressMode == State::AddrMode::XXX)
		sprintf(buffer, ":%0.4X  %.2x        %s ", address, opcode, instruction.Name);
	else if (instruction.AddressMode == State::AddrMode::IMP)
		sprintf(buffer, ":%0.4X  %.2x        %s ", address, opcode, instruction.Name);
	else if (instruction.AddressMode == State::AddrMode::ACC)
		sprintf(buffer, ":%0.4X  %.2x        %s ", address, opcode, instruction.Name);
	else if (instruction.AddressMode == State::AddrMode::IMM)
		sprintf(buffer, ":%0.4X  %.2x %.2x     %s #$%0.2X", address, opcode, bytes[0], instruction.Name, bytes[0]);
	else if (instruction.AddressMode == State::AddrMode::REL)
		sprintf(buffer, ":%0.4X  %.2x %.2x     %s *%0.3d", address, opcode, bytes[0], instruction.Name, (int8_t)bytes[0]);
	else if (instruction.AddressMode == State::AddrMode::ZPG)
		sprintf(buffer, ":%0.4X  %.2x %.2x     %s $%0.2X", address, opcode, bytes[0], instruction.Name, bytes[0]);
	else if (instruction.AddressMode == State::AddrMode::ZPX)
		sprintf(buffer, ":%0.4X  %.2x %.2x     %s $%0.2X,X", address, opcode, bytes[0], instruction.Name, bytes[0]);
	else if (instruction.AddressMode == State::AddrMode::ZPY)
		sprintf(buffer, ":%0.4X  %.2x %.2x     %s $%0.2X,Y", address, opcode, bytes[0], instruction.Name, bytes[0]);
	else if (instruction.AddressMode == State::AddrMode::ABS)
		sprintf(buffer, ":%0.4X  %.2x %.2x %.2x  %s $%0.4X", address, opcode, bytes[0], bytes[1], instruction.Name, (uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8));
	else if (instruction.AddressMode == State::AddrMode::ABX)
		sprintf(buffer, ":%0.4X  %.2x %.2x %.2x  %s $%0.4X,X", address, opcode, bytes[0], bytes[1], instruction.Name, (uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8));
	else if (instruction.AddressMode == State::AddrMode::ABY)
		sprintf(buffer, ":%0.4X  %.2x %.2x %.2x  %s $%0.4X,Y", address, opcode, bytes[0], bytes[1], instruction.Name, (uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8));
	else if (instruction.AddressMode == State::AddrMode::IND)
		sprintf(buffer, ":%0.4X  %.2x %.2x %.2x  %s ($%0.4X)", address, opcode, bytes[0], bytes[1], instruction.Name, (uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8));
	else if (instruction.AddressMode == State::AddrMode::XIN)
		sprintf(buffer, ":%0.4X  %.2x %.2x     %s ($%0.2X,X)", address, opcode, bytes[0], instruction.Name, bytes[0]);
	else if (instruction.AddressMode == State::AddrMode::INY)
		sprintf(buffer, ":%0.4X  %.2x %.2x     %s ($%0.2X),Y", address, opcode, bytes[0], instruction.Name, bytes[0]);

	if (next_address != nullptr) *next_address = address + instruction.Size;
	return std::string(buffer);
}

bool NESCPU::execute_type_0(std::function<void(void)> op_lambda)
{
	switch (State.CurrentAddrMode)
	{
		//--------------------------------
		case AddrMode::IMM:
		{
			switch (State.CycleInternal)
			{
			case 1:	
					DataBus.Data = ReadBus(Registers.PC++);	
					op_lambda(); 
					State.Ready = true; 
					return true;
			default: printf("cycle assert: %s %d\n", __FILE__, __LINE__); return true;
			}
		}
		break;
		//--------------------------------
		case AddrMode::ACC: break;
		//--------------------------------
		case AddrMode::ZPG:
		{
			switch (State.CycleInternal)
			{
			case 1:	
					DataBus.Address = ReadBus(Registers.PC++);	
					return true;
			case 2:	
					DataBus.Data = ReadBus(DataBus.Address); 
					op_lambda(); 
					State.Ready = true; 
					return true;
			default: printf("cycle assert: %s %d\n", __FILE__, __LINE__); return true;
			}
		}
		//--------------------------------
		case AddrMode::ZPX:
		{
			switch (State.CycleInternal)
			{
			case 1:	
					DataBus.Address = ReadBus(Registers.PC++); 
					return true;
			case 2:	
					DataBus.Address = (DataBus.Address + Registers.XR) & 0x00FF;
					return true;
			case 3:	
					DataBus.Data = ReadBus(DataBus.Address);
					op_lambda(); 
					State.Ready = true; 
					return true;
			default: printf("cycle assert: %s %d\n", __FILE__, __LINE__); return true;
			}
		}
		//--------------------------------
		case AddrMode::ZPY:
		{
			switch (State.CycleInternal)
			{
			case 1:	
					DataBus.Address = ReadBus(Registers.PC++); 
					return true;
			case 2:	
					DataBus.Address = (DataBus.Address + Registers.YR) & 0x00FF;
					return true;
			case 3:	
					DataBus.Data = ReadBus(DataBus.Address);
					op_lambda(); 
					State.Ready = true; 
					return true;
			default: printf("cycle assert: %s %d\n", __FILE__, __LINE__); return true;
			}
		}
		//--------------------------------
		case AddrMode::ABS:
		{
			switch (State.CycleInternal)
			{
			case 1:	
					DataBus.Address = ReadBus(Registers.PC++); 
					return true;
			case 2:	
					DataBus.Address |= ReadBus(Registers.PC++) << 8; 
					return true;
			case 3:	
					DataBus.Data = ReadBus(DataBus.Address);
					op_lambda(); 
					State.Ready = true; 
					return true;
			default: printf("cycle assert: %s %d\n", __FILE__, __LINE__); return true;
			}
		}
		//--------------------------------
		case AddrMode::ABX:
		{
			switch (State.CycleInternal)
			{
			case 1:	
					DataBus.Address = ReadBus(Registers.PC++);	
					return true;
			case 2:	
					DataBus.Address |= ReadBus(Registers.PC++) << 8; 
					return true;
			case 3:
					return (DataBus.Address & 0xFF00) != ((DataBus.Address + Registers.XR) & 0xFF00);
			case 4:	
					DataBus.Data = ReadBus(DataBus.Address + Registers.XR); 
					op_lambda(); 
					State.Ready = true;  
					return true;
			default: printf("cycle assert: %s %d\n", __FILE__, __LINE__); return true;
			}
		}
		//--------------------------------
		case AddrMode::ABY:
			switch (State.CycleInternal)
			{
			case 1:	
					DataBus.Address = ReadBus(Registers.PC++);	return true;
			case 2:	
					DataBus.Address |= ReadBus(Registers.PC++) << 8; return true;
			case 3:	
					return (DataBus.Address & 0xFF00) != ((DataBus.Address + Registers.YR) & 0xFF00);
			case 4:	
					DataBus.Data = ReadBus(DataBus.Address + Registers.YR); 
					op_lambda(); 
					State.Ready = true; 
					return true;
			default: printf("cycle assert: %s %d\n", __FILE__, __LINE__); return true;
			}
		//--------------------------------
		case AddrMode::XIN:
			switch (State.CycleInternal)
			{
			case 1:
					DataBus.Address = ReadBus(Registers.PC++);	
					return true;
			case 2:	
					DataBus.Buffer = DataBus.Address + Registers.XR; 
					return true;
			case 3: 
					DataBus.Address = ReadBus(DataBus.Buffer & 0x00FF);
					return true;
			case 4: 
					DataBus.Address |= ReadBus((DataBus.Buffer + 1) & 0x00FF) << 8;
					return true;
			case 5:	
					DataBus.Data = ReadBus(DataBus.Address); 
					op_lambda(); 
					State.Ready = true; 
					return true;
			default: printf("cycle assert: %s %d\n", __FILE__, __LINE__); return true;
			}
		//--------------------------------
		case AddrMode::INY:
			switch (State.CycleInternal)
			{
			case 1:	
					DataBus.Buffer = ReadBus(Registers.PC++);	
					return true;
			case 2:	
					DataBus.Address = ReadBus(DataBus.Buffer & 0x00FF);
					return true;
			case 3: 
					DataBus.Address |= ReadBus((DataBus.Buffer + 1) & 0x00FF) << 8;
					return true;
			case 4: 
					return (DataBus.Address & 0xFF00) != ((DataBus.Address + Registers.YR) & 0xFF00);
			case 5:	
					DataBus.Data = ReadBus(DataBus.Address + Registers.YR); 
					op_lambda(); 
					State.Ready = true; 
					return true;
			default: printf("cycle assert: %s:%d\n", __FILE__, __LINE__); return true;
			}
		//--------------------------------
	default: printf("addressing assert: %s:%d op:%.2X ad:%2d\n", __FILE__, __LINE__, State.CurrentOpCode, State.CurrentAddrMode); return true;
	}
	return true;
}

bool NESCPU::execute_type_1(std::function<void(void)> op_lambda)
{
	switch (State.CurrentAddrMode)
	{
	//--------------------------------
	case AddrMode::ACC:
	{
		switch (State.CycleInternal)
		{
		case 1:	
				DataBus.Data = Registers.AC; 
				op_lambda(); 
				Registers.AC = DataBus.Data;
				State.Ready = true; 
				return true;
		default: printf("cycle assert: %s %d\n", __FILE__, __LINE__); return true;
		}
	}
	break;
	//--------------------------------
	case AddrMode::ZPG:
	{
		switch (State.CycleInternal)
		{
		case 1:	
				DataBus.Address = ReadBus(Registers.PC++);	
				return true;
		case 2:	
				DataBus.Data = ReadBus(DataBus.Address); 
				return true;
		case 3: 
				op_lambda(); 
				return true;
		case 4: 
				WriteBus(DataBus.Address, DataBus.Data); 
				State.Ready = true; 
				return true;
		default: printf("cycle assert: %s %d\n", __FILE__, __LINE__); return true;
		}
	}
	//--------------------------------
	case AddrMode::ZPX:
	{
		switch (State.CycleInternal)
		{
		case 1:	
				DataBus.Address = ReadBus(Registers.PC++);	
				return true;
		case 2:	
				DataBus.Address = (DataBus.Address + Registers.XR) & 0x00FF;
				return true;
		case 3:	
				DataBus.Data = ReadBus(DataBus.Address);
				return true;
		case 4: 
				op_lambda(); 
				return true;
		case 5: 
				WriteBus(DataBus.Address, DataBus.Data); 
				State.Ready = true; 
				return true;
		default: printf("cycle assert: %s %d\n", __FILE__, __LINE__); return true;
		}
	}
	//--------------------------------
	case AddrMode::ZPY:
	{
		switch (State.CycleInternal)
		{
		case 1:	
				DataBus.Address = ReadBus(Registers.PC++);	
				return true;
		case 2:	
				DataBus.Address = (DataBus.Address + Registers.YR) & 0x00FF; 
				return true;
		case 3:	
				DataBus.Data = ReadBus(DataBus.Address);
				return true;
		case 4: 
				op_lambda(); 
				return true;
		case 5: 
				WriteBus(DataBus.Address, DataBus.Data); 
				State.Ready = true; 
				return true;
		default: printf("cycle assert: %s %d\n", __FILE__, __LINE__); return true;
		}
	}
	//--------------------------------
	case AddrMode::ABS:
	{
		switch (State.CycleInternal)
		{
		case 1:	
				DataBus.Address = ReadBus(Registers.PC++); 
				return true;
		case 2:	
				DataBus.Address |= ReadBus(Registers.PC++) << 8; 
				return true;
		case 3:	
				DataBus.Data = ReadBus(DataBus.Address); 
				return true;
		case 4: 
				op_lambda(); 
				return true;
		case 5: 
				WriteBus(DataBus.Address, DataBus.Data); 
				State.Ready = true; 
				return true;
		default: printf("cycle assert: %s %d\n", __FILE__, __LINE__); return true;
		}
	}
	//--------------------------------
	case AddrMode::ABX:
	{
		switch (State.CycleInternal)
		{
		case 1:	
				DataBus.Address = ReadBus(Registers.PC++); 
				return true;
		case 2:	
				DataBus.Address |= ReadBus(Registers.PC++) << 8; 
				return true;
		case 3: 
				DataBus.Address += Registers.XR;
				return true;
		case 4:	
				DataBus.Data = ReadBus(DataBus.Address); 
				return true;
		case 5: 
				op_lambda(); 
				return true;
		case 6: 
				WriteBus(DataBus.Address, DataBus.Data); 
				State.Ready = true; 
				return true;
		default: printf("cycle assert: %s %d\n", __FILE__, __LINE__); return true;
		}
	}
	//--------------------------------
	case AddrMode::ABY:
		switch (State.CycleInternal)
		{
		case 1:	
				DataBus.Address = ReadBus(Registers.PC++); 
				return true;
		case 2:	
				DataBus.Address |= ReadBus(Registers.PC++) << 8; 
				return true;
		case 3: 
				DataBus.Address += Registers.YR; 
				return true;
		case 4:	
				DataBus.Data = ReadBus(DataBus.Address); 
				return true;
		case 5: 
				op_lambda(); 
				return true;
		case 6: 
				WriteBus(DataBus.Address, DataBus.Data); 
				State.Ready = true; 
				return true;
		default: printf("cycle assert: %s %d\n", __FILE__, __LINE__); return true;
		}
	//--------------------------------
	default: printf("addressing assert: %s %d\n", __FILE__, __LINE__); return true;
	}
}

bool NESCPU::execute_type_2(std::function<void(void)> op_lambda)
{
	switch (State.CurrentAddrMode)
	{
		//--------------------------------
	case AddrMode::ZPG:
	{
		switch (State.CycleInternal)
		{
		case 1:
			DataBus.Address = ReadBus(Registers.PC++);
			return true;
		case 2:
			op_lambda();
			WriteBus(DataBus.Address, DataBus.Data);
			State.Ready = true;
			return true;
		default: printf("cycle assert: %s %d\n", __FILE__, __LINE__); return true;
		}
	}
	//--------------------------------
	case AddrMode::ZPX:
	{
		switch (State.CycleInternal)
		{
		case 1:
			DataBus.Address = ReadBus(Registers.PC++);
			return true;
		case 2:
			DataBus.Address = (DataBus.Address + Registers.XR) & 0x00FF;
			return true;
		case 3:
			op_lambda();
			WriteBus(DataBus.Address, DataBus.Data);
			State.Ready = true;
			return true;
		default: printf("cycle assert: %s %d\n", __FILE__, __LINE__); return true;
		}
	}
	//--------------------------------
	case AddrMode::ZPY:
	{
		switch (State.CycleInternal)
		{
		case 1:
			DataBus.Address = ReadBus(Registers.PC++);
			return true;
		case 2:
			DataBus.Address = (DataBus.Address + Registers.YR) & 0x00FF;
			return true;
		case 3:
			op_lambda();
			WriteBus(DataBus.Address, DataBus.Data);
			State.Ready = true;
			return true;
		default: printf("cycle assert: %s %d\n", __FILE__, __LINE__); return true;
		}
	}
	//--------------------------------
	case AddrMode::ABS:
	{
		switch (State.CycleInternal)
		{
		case 1:
			DataBus.Address = ReadBus(Registers.PC++);
			return true;
		case 2:
			DataBus.Address |= ((uint16_t)ReadBus(Registers.PC++)) << 8;
			return true;
		case 3:
			op_lambda();
			WriteBus(DataBus.Address, DataBus.Data);
			State.Ready = true;
			return true;
		default: printf("cycle assert: %s %d\n", __FILE__, __LINE__); return true;
		}
	}
	//--------------------------------
	case AddrMode::ABX:
	{
		switch (State.CycleInternal)
		{
		case 1:
			DataBus.Address = ReadBus(Registers.PC++);
			return true;
		case 2:
			DataBus.Address |= ((uint16_t)ReadBus(Registers.PC++)) << 8;
			return true;
		case 3:
			DataBus.Address += Registers.XR;
			return true;
		case 4:
			op_lambda();
			WriteBus(DataBus.Address, DataBus.Data);
			State.Ready = true;
			return true;
		default: printf("cycle assert: %s %d\n", __FILE__, __LINE__); return true;
		}
	}
	//--------------------------------
	case AddrMode::ABY:
		switch (State.CycleInternal)
		{
		case 1:
			DataBus.Address = ReadBus(Registers.PC++);
			return true;
		case 2:
			DataBus.Address |= ((uint16_t)ReadBus(Registers.PC++)) << 8;
			return true;
		case 3:
			DataBus.Address += Registers.YR;
			return true;
		case 4:
			op_lambda();
			WriteBus(DataBus.Address, DataBus.Data);
			State.Ready = true;
			return true;
		default: printf("cycle assert: %s %d\n", __FILE__, __LINE__); return true;
		}
		//--------------------------------
	case AddrMode::XIN:
		switch (State.CycleInternal)
		{
		case 1:
			DataBus.Address = ReadBus(Registers.PC++);
			return true;
		case 2:
			DataBus.Buffer = DataBus.Address + Registers.XR;
			return true;
		case 3:
			DataBus.Address = ReadBus(DataBus.Buffer & 0x00FF);
			return true;
		case 4:
			DataBus.Address |= ReadBus((DataBus.Buffer + 1) & 0x00FF) << 8;
			return true;
		case 5:
			op_lambda();
			WriteBus(DataBus.Address, DataBus.Data);
			State.Ready = true;
			return true;
		default: printf("cycle assert: %s %d\n", __FILE__, __LINE__); return true;
		}
		//--------------------------------
	case AddrMode::INY:
		switch (State.CycleInternal)
		{
		case 1:
			DataBus.Address = ReadBus(Registers.PC++);
			return true;
		case 2:
			DataBus.Buffer = DataBus.Address;
			return true;
		case 3:
			DataBus.Address = ReadBus(DataBus.Buffer & 0x00FF);
			return true;
		case 4:
			DataBus.Address |= ReadBus((DataBus.Buffer + 1) & 0x00FF) << 8;
			return true;
		case 5:
			op_lambda();
			WriteBus(DataBus.Address + Registers.YR, DataBus.Data);
			State.Ready = true;
			return true;
		default: printf("cycle assert: %s %d\n", __FILE__, __LINE__); return true;
		}
		//--------------------------------
	default: printf("addressing assert: %s %d\n", __FILE__, __LINE__); return true;
	}
}

bool NESCPU::execute_irq()
{
	//WriteData(0x0100 + Registers.SP--, (Registers.PC & 0xFF00) >> 8);
	//WriteData(0x0100 + Registers.SP--, (Registers.PC & 0x00FF));
	//WriteData(0x0100 + Registers.SP--, (Registers.SR));
	
	//setFlag(SRFlag::InterruptBit, true);
	//setFlag(SRFlag::BreakBit, false);

	//Registers.PC = (uint16_t)ReadBus(0xFFFE) | ((uint16_t)ReadBus(0xFFFF) << 8);
	////State.CyclesQueued += 7;
	switch (State.CycleInternal)
	{
	case 0:
			WriteBus(0x0100 + Registers.SP--, (Registers.PC & 0xFF00) >> 8);
			return true;
	case 1:
			WriteBus(0x0100 + Registers.SP--, (Registers.PC & 0x00FF));
			return true;
	case 2:
			WriteBus(0x0100 + Registers.SP--, (Registers.SR));
			return true;
	case 3:
			setFlag(SRFlag::InterruptBit, true);
			setFlag(SRFlag::BreakBit, false);
			return true;
	case 4:
			DataBus.Address = ReadBus(0xFFFE);
			return true;
	case 5:
			DataBus.Address |= ReadBus(0xFFFF) << 8;
			return true;
	case 6:
			Registers.PC = DataBus.Address;
			State.IRQActive = false;
			State.Ready = true;
			return true;
	default: printf("cycle assert: %s %d\n", __FILE__, __LINE__); return true;
	}
}
bool NESCPU::execute_nmi()
{
	//WriteData(0x0100 + Registers.SP--, (Registers.PC & 0xFF00) >> 8);
	//WriteData(0x0100 + Registers.SP--, (Registers.PC & 0x00FF));
	//WriteData(0x0100 + Registers.SP--, (Registers.SR));

	//setFlag(SRFlag::InterruptBit, true);
	//setFlag(SRFlag::BreakBit, false);

	//Registers.PC = (uint16_t)ReadBus(0xFFFA) | ((uint16_t)ReadBus(0xFFFB) << 8);
	//State.CyclesQueued += 8;
	switch (State.CycleInternal)
	{
	case 0:
		WriteBus(0x0100 + Registers.SP--, (Registers.PC & 0xFF00) >> 8);
		return true;
	case 1:
		WriteBus(0x0100 + Registers.SP--, (Registers.PC & 0x00FF));
		return true;
	case 2:
		WriteBus(0x0100 + Registers.SP--, (Registers.SR));
		return true;
	case 3:
		setFlag(SRFlag::InterruptBit, true);
		setFlag(SRFlag::BreakBit, false);
		return true;
	case 4:
		DataBus.Address = ReadBus(0xFFFA);
		return true;
	case 5:
		DataBus.Address |= ((uint16_t)ReadBus(0xFFFB)) << 8;
		return true;
	case 6:
		Registers.PC = DataBus.Address;
		State.NMIActive = false;
		State.Ready = true;
		return true;
	default: printf("cycle assert: %s %d\n", __FILE__, __LINE__); return true;
	}
}
bool NESCPU::execute_adc()
{
	auto OpLambda = [this]()
	{
		uint16_t adc_intermediate = (uint16_t)Registers.AC + (uint16_t)DataBus.Data + (getFlag(SRFlag::CarryBit) ? 1 : 0);

		setFlag(SRFlag::ZeroBit, (adc_intermediate & 0xFF) == 0);
		setFlag(SRFlag::OverflowBit, !((Registers.AC ^ DataBus.Data) & 0x80) && ((Registers.AC ^ adc_intermediate) & 0x80));
		setFlag(SRFlag::CarryBit, (adc_intermediate > 0xFF));
		setFlag(SRFlag::NegativeBit, adc_intermediate & 0x80);

		Registers.AC = (adc_intermediate & 0x00FF);
	};

	return execute_type_0(OpLambda);
}
bool NESCPU::execute_and() // and (with accumulator)
{
	auto OpLambda = [this]()
	{
		Registers.AC &= DataBus.Data;
		setFlag(SRFlag::ZeroBit, Registers.AC == 0);
		setFlag(SRFlag::NegativeBit, Registers.AC & 0x80);
	};

	return execute_type_0(OpLambda);
}
bool NESCPU::execute_asl() // arithmetic shift left
{
	auto OpLambda = [this]()
	{
		setFlag(SRFlag::CarryBit, (DataBus.Data & 0x80));
		DataBus.Data = (DataBus.Data << 1);
		setFlag(SRFlag::ZeroBit, DataBus.Data == 0);
		setFlag(SRFlag::NegativeBit, DataBus.Data & 0x80);
	};

	return execute_type_1(OpLambda);
}
bool NESCPU::execute_bcc() // branch on carry clear
{
	DataBus.Data = ReadBus(Registers.PC++);
	if (getFlag(SRFlag::CarryBit) == false)
	{
		State.CycleSkip++;
		if ((Registers.PC & 0xFF00) != ((Registers.PC + static_cast<int8_t>(DataBus.Data)) & 0xFF00))
			State.CycleSkip++;

		Registers.PC += static_cast<int8_t>(DataBus.Data);
	}
	State.Ready = true;
	return true;
}
bool NESCPU::execute_bcs() // branch on carry set
{
	DataBus.Data = ReadBus(Registers.PC++);
	if (getFlag(SRFlag::CarryBit) == true)
	{
		State.CycleSkip++;
		if ((Registers.PC & 0xFF00) != ((Registers.PC + static_cast<int8_t>(DataBus.Data)) & 0xFF00))
			State.CycleSkip++;

		Registers.PC += static_cast<int8_t>(DataBus.Data);
	}
	State.Ready = true;
	return true;
}
bool NESCPU::execute_beq() // branch on equal (zero set)
{
	DataBus.Data = ReadBus(Registers.PC++);
	if (getFlag(SRFlag::ZeroBit) == true)
	{
		State.CycleSkip++;
		if ((Registers.PC & 0xFF00) != ((Registers.PC + static_cast<int8_t>(DataBus.Data)) & 0xFF00))
			State.CycleSkip++;

		Registers.PC += static_cast<int8_t>(DataBus.Data);
	}
	State.Ready = true;
	return true;
}
bool NESCPU::execute_bit() // bit test
{
	auto OpLambda = [this]()
	{
		uint8_t bit_intermediate = Registers.AC & DataBus.Data;

		Registers.SR = (Registers.SR & 0x3F) | (DataBus.Data & 0xC0);
		setFlag(SRFlag::ZeroBit, bit_intermediate == 0);
	};

	return execute_type_0(OpLambda);
}
bool NESCPU::execute_bmi() // branch on minus (negative set)
{
	DataBus.Data = ReadBus(Registers.PC++);
	if (getFlag(SRFlag::NegativeBit) == true)
	{
		State.CycleSkip++;
		if ((Registers.PC & 0xFF00) != ((Registers.PC + static_cast<int8_t>(DataBus.Data)) & 0xFF00))
			State.CycleSkip++;

		Registers.PC += static_cast<int8_t>(DataBus.Data);
	}
	State.Ready = true;
	return true;
}
bool NESCPU::execute_bne() // branch on not equal (zero clear)
{
	DataBus.Data = ReadBus(Registers.PC++);
	if (getFlag(SRFlag::ZeroBit) == false)
	{
		State.CycleSkip++;
		if ((Registers.PC & 0xFF00) != ((Registers.PC + static_cast<int8_t>(DataBus.Data)) & 0xFF00))
			State.CycleSkip++;

		Registers.PC += static_cast<int8_t>(DataBus.Data);
	}
	State.Ready = true;
	return true;
}
bool NESCPU::execute_bpl() // branch on plus (negative clear)
{
	DataBus.Data = ReadBus(Registers.PC++);
	if (getFlag(SRFlag::NegativeBit) == false)
	{
		State.CycleSkip++;
		if ((Registers.PC & 0xFF00) != ((Registers.PC + static_cast<int8_t>(DataBus.Data)) & 0xFF00))
			State.CycleSkip++;

		Registers.PC += static_cast<int8_t>(DataBus.Data);
	}
	State.Ready = true;
	return true;
}
bool NESCPU::execute_brk() // break / interrupt
{
	switch (State.CycleInternal)
	{
	case 1:
		return true;
	case 2:
		WriteBus(0x0100 + Registers.SP--, (Registers.PC & 0xFF00) >> 8);
		return true;
	case 3:
		WriteBus(0x0100 + Registers.SP--, (Registers.PC & 0x00FF));
		return true;
	case 4:
		WriteBus(0x0100 + Registers.SP--, (Registers.SR));
		return true;
	case 5:
		DataBus.Address = ReadBus(0xFFFE);
		setFlag(SRFlag::InterruptBit, true);
		setFlag(SRFlag::BreakBit, false);
		return true;
	case 6:
		DataBus.Address |= ReadBus(0xFFFF) << 8;
		Registers.PC = DataBus.Address;
		State.IRQActive = false;
		State.Ready = true;
		return true;

	default: printf("cycle assert: %s %d\n", __FILE__, __LINE__); return true;
	}
	return true;
}
bool NESCPU::execute_bvc() // branch on overflow clear
{
	DataBus.Data = ReadBus(Registers.PC++);
	if (getFlag(SRFlag::OverflowBit) == false)
	{
		State.CycleSkip++;
		if ((Registers.PC & 0xFF00) != ((Registers.PC + static_cast<int8_t>(DataBus.Data)) & 0xFF00))
			State.CycleSkip++;

		Registers.PC += static_cast<int8_t>(DataBus.Data);
	}
	State.Ready = true;
	return true;
}
bool NESCPU::execute_bvs() // branch on overflow set
{
	DataBus.Data = ReadBus(Registers.PC++);
	if (getFlag(SRFlag::OverflowBit) == true)
	{
		State.CycleSkip++;
		if ((Registers.PC & 0xFF00) != ((Registers.PC + static_cast<int8_t>(DataBus.Data)) & 0xFF00))
			State.CycleSkip++;

		Registers.PC += static_cast<int8_t>(DataBus.Data);
	}
	State.Ready = true;
	return true;
}
bool NESCPU::execute_clc() // clear carry
{
	setFlag(SRFlag::CarryBit, false);
	State.Ready = true;
	return true;
}
bool NESCPU::execute_cld() // clear decimal
{
	setFlag(SRFlag::DecimalBit, false);
	State.Ready = true;
	return true;
}
bool NESCPU::execute_cli() // clear interrupt disable
{
	setFlag(SRFlag::InterruptBit, false);
	State.Ready = true;
	return true;
}
bool NESCPU::execute_clv() // clear overflow
{
	setFlag(SRFlag::OverflowBit, false);
	State.Ready = true;
	return true;
}
bool NESCPU::execute_cmp() // compare (with accumulator)
{
	auto OpLambda = [this]()
	{
		uint16_t cmp_intermediate = (uint16_t)Registers.AC - (uint16_t)DataBus.Data;

		setFlag(SRFlag::CarryBit, Registers.AC >= DataBus.Data);
		setFlag(SRFlag::ZeroBit, (cmp_intermediate & 0xFF) == 0x0000);
		setFlag(SRFlag::NegativeBit, cmp_intermediate & 0x80);
	};
	return execute_type_0(OpLambda);
}
bool NESCPU::execute_cpx() // compare with x
{
	auto OpLambda = [this]()
	{
		uint16_t cmp_intermediate = (uint16_t)Registers.XR - (uint16_t)DataBus.Data;

		setFlag(SRFlag::CarryBit, Registers.XR >= DataBus.Data);
		setFlag(SRFlag::ZeroBit, (cmp_intermediate & 0xFF) == 0x0000);
		setFlag(SRFlag::NegativeBit, cmp_intermediate & 0x80);
	};

	return execute_type_0(OpLambda);
}
bool NESCPU::execute_cpy() // compare with y
{
	auto OpLambda = [this]()
	{
		uint16_t cmp_intermediate = (uint16_t)Registers.YR - (uint16_t)DataBus.Data;

		setFlag(SRFlag::CarryBit, Registers.YR >= DataBus.Data);
		setFlag(SRFlag::ZeroBit, (cmp_intermediate & 0xFF) == 0x0000);
		setFlag(SRFlag::NegativeBit, cmp_intermediate & 0x80);
	};

	return execute_type_0(OpLambda);
}
bool NESCPU::execute_dec() // decrement
{
	auto OpLambda = [this]()
	{
		DataBus.Data--;

		setFlag(SRFlag::ZeroBit, DataBus.Data == 0);
		setFlag(SRFlag::NegativeBit, DataBus.Data & 0x80);
	};

	return execute_type_1(OpLambda);
}
bool NESCPU::execute_dex() // decrement x
{
	Registers.XR--;
	setFlag(SRFlag::ZeroBit, Registers.XR == 0);
	setFlag(SRFlag::NegativeBit, Registers.XR & 0x80);
	State.Ready = true;
	return true;
}
bool NESCPU::execute_dey() // decrement y
{
	Registers.YR--;
	setFlag(SRFlag::ZeroBit, Registers.YR == 0);
	setFlag(SRFlag::NegativeBit, Registers.YR & 0x80);
	State.Ready = true;
	return true;
}
bool NESCPU::execute_eor() // exclusive or (with accumulator)
{
	auto OpLambda = [this]()
	{
		Registers.AC ^= DataBus.Data;

		setFlag(SRFlag::ZeroBit, Registers.AC == 0);
		setFlag(SRFlag::NegativeBit, Registers.AC & 0x80);
	};
	return execute_type_0(OpLambda);
}
bool NESCPU::execute_inc() // increment
{
	auto OpLambda = [this]()
	{
		DataBus.Data++;
	
		setFlag(SRFlag::ZeroBit, DataBus.Data == 0);
		setFlag(SRFlag::NegativeBit, DataBus.Data & 0x80);
	};

	return execute_type_1(OpLambda);
}
bool NESCPU::execute_inx() // increment x
{
	Registers.XR++;
	setFlag(SRFlag::ZeroBit, Registers.XR == 0);
	setFlag(SRFlag::NegativeBit, Registers.XR & 0x80);
	State.Ready = true;
	return true;
}
bool NESCPU::execute_iny() // increment y
{
	Registers.YR++;
	setFlag(SRFlag::ZeroBit, Registers.YR == 0);
	setFlag(SRFlag::NegativeBit, Registers.YR & 0x80);
	State.Ready = true;
	return true;
}
bool NESCPU::execute_jmp() // jump
{
	switch (State.CurrentAddrMode)
	{
		case AddrMode::ABS:
		{
			switch (State.CycleInternal)
			{
				case 1:	
					DataBus.Address = ReadBus(Registers.PC++); 
					return true;
				case 2:	
					DataBus.Address |= ReadBus(Registers.PC++) << 8; 
					Registers.PC = DataBus.Address;
					State.Ready = true;
					return true;
			}
		}
		break;
		case AddrMode::IND:
		{
			switch (State.CycleInternal)
			{
			case 1:
				DataBus.Buffer = ReadBus(Registers.PC++);
				return true;
			case 2:
				DataBus.Buffer |= ReadBus(Registers.PC++) << 8;
				return true;
			case 3:
				DataBus.Address = ReadBus(DataBus.Buffer);
				return true;
			case 4:
				//Simulate indirect mode page boundary bug
				if ((DataBus.Buffer & 0x00FF) == 0xFF)
					DataBus.Address |= ReadBus(DataBus.Buffer & 0xFF00) << 8;
				else
					DataBus.Address |= ReadBus(DataBus.Buffer + 1) << 8;

				Registers.PC = DataBus.Address;
				State.Ready = true;
				return true;
			}
		}
		break;
	}
	return true;
}
bool NESCPU::execute_jsr() // jump subroutine
{
	switch (State.CycleInternal)
	{
	case 1:
		WriteBus(0x0100 + Registers.SP--, ((Registers.PC+1) >> 8) & 0x00FF);
		return true;
	case 2:
		WriteBus(0x0100 + Registers.SP--, (Registers.PC+1) & 0x00FF);
		return true;
	case 3:
		DataBus.Address = ReadBus(Registers.PC++);
		return true;
	case 4:
		DataBus.Address |= ReadBus(Registers.PC++) << 8;
		return true;
	case 5:
		Registers.PC = DataBus.Address;
		State.Ready = true;
		return true;
	}
}
bool NESCPU::execute_lda() // load accumulator
{
	auto OpLambda = [this]()
	{
		Registers.AC = DataBus.Data;
		setFlag(SRFlag::ZeroBit, Registers.AC == 0);
		setFlag(SRFlag::NegativeBit, Registers.AC & 0x80);
	};
	return execute_type_0(OpLambda);
}
bool NESCPU::execute_ldx() // load x
{
	auto OpLambda = [this]()
	{
		Registers.XR = DataBus.Data;
		setFlag(SRFlag::ZeroBit, Registers.XR == 0);
		setFlag(SRFlag::NegativeBit, Registers.XR & 0x80);
	};
	return execute_type_0(OpLambda);
}
bool NESCPU::execute_ldy() // load y
{
	auto OpLambda = [this]()
	{
		Registers.YR = DataBus.Data;
		setFlag(SRFlag::ZeroBit, Registers.YR == 0);
		setFlag(SRFlag::NegativeBit, Registers.YR & 0x80);
	};
	return execute_type_0(OpLambda);
}
bool NESCPU::execute_lsr() // logical shift right
{
	auto OpLambda = [this]()
	{
		setFlag(SRFlag::CarryBit, (DataBus.Data & 0x01));
		DataBus.Data = (DataBus.Data >> 1);
		setFlag(SRFlag::ZeroBit, DataBus.Data == 0);
		setFlag(SRFlag::NegativeBit, false);
	};

	return execute_type_1(OpLambda);
}
bool NESCPU::execute_nop() // no operation
{
	State.Ready = true;
	return true;
}
bool NESCPU::execute_ora() // or with accumulator
{
	auto OpLambda = [this]()
	{
		Registers.AC |= DataBus.Data;
		setFlag(SRFlag::ZeroBit, Registers.AC == 0);
		setFlag(SRFlag::NegativeBit, Registers.AC & 0x80);
	};
	return execute_type_0(OpLambda);
}
bool NESCPU::execute_pha() // push accumulator
{
	WriteBus(0x0100 + Registers.SP--, Registers.AC);
	State.CycleSkip++;
	State.Ready = true;
	return true;
}
bool NESCPU::execute_php() // push processor status (sr)
{
	WriteBus(0x0100 + Registers.SP--, Registers.SR | 0x20);
	State.CycleSkip++;
	State.Ready = true;
	return true;
}
bool NESCPU::execute_pla() // pull accumulator
{
	Registers.AC = ReadBus(0x0100 + (++Registers.SP));

	setFlag(SRFlag::ZeroBit, Registers.AC == 0);
	setFlag(SRFlag::NegativeBit, Registers.AC & 0x80);

	State.CycleSkip += 2;
	State.Ready = true;
	return true;
	
}
bool NESCPU::execute_plp() // pull processor status (sr)
{
	Registers.SR = ReadBus(0x0100 + (++Registers.SP));

	State.CycleSkip += 2;
	State.Ready = true;
	return true;
	
}
bool NESCPU::execute_rol() // rotate left
{
	auto OpLambda = [this]()
	{
		uint16_t rol_intermediate = DataBus.Data << 1 | (getFlag(SRFlag::CarryBit) ? 0x01 : 0x00);
		DataBus.Data = (rol_intermediate & 0x00FF);

		setFlag(SRFlag::CarryBit, rol_intermediate & 0xFF00);
		setFlag(SRFlag::ZeroBit, DataBus.Data == 0);
		setFlag(SRFlag::NegativeBit, DataBus.Data & 0x80);
	};

	return execute_type_1(OpLambda);
}
bool NESCPU::execute_ror() // rotate right
{
	auto OpLambda = [this]()
	{
		uint16_t ror_intermediate = DataBus.Data >> 1 | (getFlag(SRFlag::CarryBit) ? 0x80 : 0x00);
		setFlag(SRFlag::CarryBit, (DataBus.Data & 0x01));
		DataBus.Data = (ror_intermediate & 0x00FF);

		setFlag(SRFlag::ZeroBit, DataBus.Data == 0);
		setFlag(SRFlag::NegativeBit, DataBus.Data & 0x80);
	};

	return execute_type_1(OpLambda);
}
bool NESCPU::execute_rti() // return from interrupt
{
	switch (State.CycleInternal)
	{
	case 1:
			Registers.SR = ReadBus(0x0100 + (++Registers.SP));
			return true;
	case 2:
			DataBus.Address = (uint16_t)ReadBus(0x0100 + (++Registers.SP));
			return true;
	case 3:
			DataBus.Address |= (uint16_t)ReadBus(0x0100 + (++Registers.SP)) << 8;
			return true;
	case 4:
			//dummy cycle
			return true;
	case 5:
			Registers.PC = DataBus.Address;
			State.Ready = true;
			return true;
	}	
	return true;
}
bool NESCPU::execute_rts() // return from subroutine
{
	switch (State.CycleInternal)
	{
	case 1:
			DataBus.Address = (uint16_t)ReadBus(0x0100 + (++Registers.SP));
			return true;
	case 2:
			DataBus.Address |= (uint16_t)ReadBus(0x0100 + (++Registers.SP)) << 8;
			return true;
	case 3:
			DataBus.Address++;
			return true;
	case 4:
			//dummy cycle
			return true;
	case 5:
			Registers.PC = DataBus.Address;
			State.Ready = true;
			return true;
	}
	return true;
}
bool NESCPU::execute_sbc() // subtract with carry
{
	auto OpLambda = [this]()
	{
		uint16_t sbc_xored = ((uint16_t)DataBus.Data) ^ 0x00FF;
		uint16_t sbc_intermediate = ((uint16_t)Registers.AC + (uint16_t)sbc_xored) + (getFlag(SRFlag::CarryBit) ? 1 : 0);

		setFlag(SRFlag::ZeroBit, (sbc_intermediate & 0xFF) == 0);
		setFlag(SRFlag::NegativeBit, sbc_intermediate & 0x80);
		setFlag(SRFlag::OverflowBit, (sbc_intermediate ^ (uint16_t)Registers.AC) & (sbc_intermediate ^ sbc_xored) & 0x80);
		setFlag(SRFlag::CarryBit, sbc_intermediate & 0xFF00);

		Registers.AC = (sbc_intermediate & 0x00FF);
	};

	return execute_type_0(OpLambda);
}
bool NESCPU::execute_sec() // set carry
{
	setFlag(SRFlag::CarryBit, true);
	State.Ready = true;
	return true;
}
bool NESCPU::execute_sed() // set decimal
{
	setFlag(SRFlag::DecimalBit, true);
	State.Ready = true;
	return true;
}
bool NESCPU::execute_sei() // set interrupt disable
{
	setFlag(SRFlag::InterruptBit, true);
	State.Ready = true;
	return true;
}
bool NESCPU::execute_sta() // store accumulator
{
	auto OpLambda = [this]()
	{
		DataBus.Data = Registers.AC;
	};

	return execute_type_2(OpLambda);
	
}
bool NESCPU::execute_stx() // store x
{
	auto OpLambda = [this]()
	{
		DataBus.Data = Registers.XR;
	};

	return execute_type_2(OpLambda);
}
bool NESCPU::execute_sty() // store y
{
	auto OpLambda = [this]()
	{
		DataBus.Data = Registers.YR;
	};

	return execute_type_2(OpLambda);
}
bool NESCPU::execute_tax() // transfer accumulator to x
{
	Registers.XR = Registers.AC;
	setFlag(SRFlag::ZeroBit, Registers.XR == 0);
	setFlag(SRFlag::NegativeBit, Registers.XR & 0x80);
	State.Ready = true;
	return true;
}
bool NESCPU::execute_tay() // transfer accumulator to y
{
	Registers.YR = Registers.AC;
	setFlag(SRFlag::ZeroBit, Registers.YR == 0);
	setFlag(SRFlag::NegativeBit, Registers.YR & 0x80);
	State.Ready = true;
	return true;
}
bool NESCPU::execute_tsx() // transfer stack pointer to x
{
	Registers.XR = Registers.SP;
	setFlag(SRFlag::ZeroBit, Registers.XR == 0);
	setFlag(SRFlag::NegativeBit, Registers.XR & 0x80);
	State.Ready = true;
	return true;
}
bool NESCPU::execute_txa() // transfer x to accumulator
{
	Registers.AC = Registers.XR;
	setFlag(SRFlag::ZeroBit, Registers.AC == 0);
	setFlag(SRFlag::NegativeBit, Registers.AC & 0x80);
	State.Ready = true;
	return true;
}
bool NESCPU::execute_txs() // transfer x to stack pointer
{
	Registers.SP = Registers.XR;
	State.Ready = true;
	return true;
}
bool NESCPU::execute_tya() // transfer y to accumulator 
{
	Registers.AC = Registers.YR;
	setFlag(SRFlag::ZeroBit, Registers.AC == 0);
	setFlag(SRFlag::NegativeBit, Registers.AC & 0x80);
	State.Ready = true;
	return true;
}

bool NESCPU::execute_xxx() // halt
{
	printf("Warning: illegal operand reached, cpu halted!\n");
	State.Halted = true;
	State.Ready = true;
	return true;
}

void NESCPU::prepare_table()
{
	using am = State::AddrMode;
	//Setup lookup table
	m_InstructionLookup = {
		{" BRK\0", 1, am::IMP, &NESCPU::execute_brk }, // 0x00 : BRK impl
		{" ORA\0", 2, am::XIN, &NESCPU::execute_ora }, // 0x01 : ORA X,ind
		{" ???\0", 1, am::XXX, &NESCPU::execute_xxx }, // 0x02 : ???
		{" ???\0", 1, am::XXX, &NESCPU::execute_xxx }, // 0x03 : ???
		{" ???\0", 1, am::XXX, &NESCPU::execute_xxx }, // 0x04 : ???
		{" ORA\0", 2, am::ZPG, &NESCPU::execute_ora }, // 0x05 : ORA zpg
		{" ASL\0", 2, am::ZPG, &NESCPU::execute_asl }, // 0x06 : ASL zpg
		{" ???\0", 1, am::XXX, &NESCPU::execute_xxx }, // 0x07 : ???
		{" PHP\0", 1, am::IMP, &NESCPU::execute_php }, // 0x08 : PHP impl
		{" ORA\0", 2, am::IMM, &NESCPU::execute_ora }, // 0x09 : ORA #
		{" ASL\0", 1, am::ACC, &NESCPU::execute_asl }, // 0x0a : ASL A
		{" ???\0", 1, am::XXX, &NESCPU::execute_xxx }, // 0x0b : ???
		{" ???\0", 1, am::XXX, &NESCPU::execute_xxx }, // 0x0c : ???
		{" ORA\0", 3, am::ABS, &NESCPU::execute_ora }, // 0x0d : ORA abs
		{" ASL\0", 3, am::ABS, &NESCPU::execute_asl }, // 0x0e : ASL abs
		{" ???\0", 1, am::XXX, &NESCPU::execute_xxx }, // 0x0f : ???
		{" BPL\0", 2, am::REL, &NESCPU::execute_bpl }, // 0x10 : BPL rel
		{" ORA\0", 2, am::INY, &NESCPU::execute_ora }, // 0x11 : ORA ind,Y
		{" ???\0", 1, am::XXX, &NESCPU::execute_xxx }, // 0x12 : ???
		{" ???\0", 1, am::XXX, &NESCPU::execute_xxx }, // 0x13 : ???
		{" ???\0", 1, am::XXX, &NESCPU::execute_xxx }, // 0x14 : ???
		{" ORA\0", 2, am::ZPX, &NESCPU::execute_ora }, // 0x15 : ORA zpg,X
		{" ASL\0", 2, am::ZPX, &NESCPU::execute_asl }, // 0x16 : ASL zpg,X
		{" ???\0", 1, am::XXX, &NESCPU::execute_xxx }, // 0x17 : ???
		{" CLC\0", 1, am::IMP, &NESCPU::execute_clc }, // 0x18 : CLC impl
		{" ORA\0", 3, am::ABY, &NESCPU::execute_ora }, // 0x19 : ORA abs,Y
		{" ???\0", 1, am::XXX, &NESCPU::execute_xxx }, // 0x1a : ???
		{" ???\0", 1, am::XXX, &NESCPU::execute_xxx }, // 0x1b : ???
		{" ???\0", 1, am::XXX, &NESCPU::execute_xxx }, // 0x1c : ???
		{" ORA\0", 3, am::ABX, &NESCPU::execute_ora }, // 0x1d : ORA abs,X
		{" ASL\0", 3, am::ABX, &NESCPU::execute_asl }, // 0x1e : ASL abs,X
		{" ???\0", 1, am::XXX, &NESCPU::execute_xxx }, // 0x1f : ???
		{" JSR\0", 3, am::ABS, &NESCPU::execute_jsr }, // 0x20 : JSR abs
		{" AND\0", 2, am::XIN, &NESCPU::execute_and }, // 0x21 : AND X,ind
		{" ???\0", 1, am::XXX, &NESCPU::execute_xxx }, // 0x22 : ???
		{" ???\0", 1, am::XXX, &NESCPU::execute_xxx }, // 0x23 : ???
		{" BIT\0", 2, am::ZPG, &NESCPU::execute_bit }, // 0x24 : BIT zpg
		{" AND\0", 2, am::ZPG, &NESCPU::execute_and }, // 0x25 : AND zpg
		{" ROL\0", 2, am::ZPG, &NESCPU::execute_rol }, // 0x26 : ROL zpg
		{" ???\0", 1, am::XXX, &NESCPU::execute_xxx }, // 0x27 : ???
		{" PLP\0", 1, am::IMP, &NESCPU::execute_plp }, // 0x28 : PLP impl
		{" AND\0", 2, am::IMM, &NESCPU::execute_and }, // 0x29 : AND #
		{" ROL\0", 1, am::ACC, &NESCPU::execute_rol }, // 0x2a : ROL A
		{" ???\0", 1, am::XXX, &NESCPU::execute_xxx }, // 0x2b : ???
		{" BIT\0", 3, am::ABS, &NESCPU::execute_bit }, // 0x2c : BIT abs
		{" AND\0", 3, am::ABS, &NESCPU::execute_and }, // 0x2d : AND abs
		{" ROL\0", 3, am::ABS, &NESCPU::execute_rol }, // 0x2e : ROL abs
		{" ???\0", 1, am::XXX, &NESCPU::execute_xxx }, // 0x2f : ???
		{" BMI\0", 2, am::REL, &NESCPU::execute_bmi }, // 0x30 : BMI rel
		{" AND\0", 2, am::INY, &NESCPU::execute_and }, // 0x31 : AND ind,Y
		{" ???\0", 1, am::XXX, &NESCPU::execute_xxx }, // 0x32 : ???
		{" ???\0", 1, am::XXX, &NESCPU::execute_xxx }, // 0x33 : ???
		{" ???\0", 1, am::XXX, &NESCPU::execute_xxx }, // 0x34 : ???
		{" AND\0", 2, am::ZPX, &NESCPU::execute_and }, // 0x35 : AND zpg,X
		{" ROL\0", 2, am::ZPX, &NESCPU::execute_rol }, // 0x36 : ROL zpg,X
		{" ???\0", 1, am::XXX, &NESCPU::execute_xxx }, // 0x37 : ???
		{" SEC\0", 1, am::IMP, &NESCPU::execute_sec }, // 0x38 : SEC impl
		{" AND\0", 3, am::ABY, &NESCPU::execute_and }, // 0x39 : AND abs,Y
		{" ???\0", 1, am::XXX, &NESCPU::execute_xxx }, // 0x3a : ???
		{" ???\0", 1, am::XXX, &NESCPU::execute_xxx }, // 0x3b : ???
		{" ???\0", 1, am::XXX, &NESCPU::execute_xxx }, // 0x3c : ???
		{" AND\0", 3, am::ABX, &NESCPU::execute_and }, // 0x3d : AND abs,X
		{" ROL\0", 3, am::ABX, &NESCPU::execute_rol }, // 0x3e : ROL abs,X
		{" ???\0", 1, am::XXX, &NESCPU::execute_xxx }, // 0x3f : ???
		{" RTI\0", 1, am::IMP, &NESCPU::execute_rti }, // 0x40 : RTI impl
		{" EOR\0", 2, am::XIN, &NESCPU::execute_eor }, // 0x41 : EOR X,ind
		{" ???\0", 1, am::XXX, &NESCPU::execute_xxx }, // 0x42 : ???
		{" ???\0", 1, am::XXX, &NESCPU::execute_xxx }, // 0x43 : ???
		{" ???\0", 1, am::XXX, &NESCPU::execute_xxx }, // 0x44 : ???
		{" EOR\0", 2, am::ZPG, &NESCPU::execute_eor }, // 0x45 : EOR zpg
		{" LSR\0", 2, am::ZPG, &NESCPU::execute_lsr }, // 0x46 : LSR zpg
		{" ???\0", 1, am::XXX, &NESCPU::execute_xxx }, // 0x47 : ???
		{" PHA\0", 1, am::IMP, &NESCPU::execute_pha }, // 0x48 : PHA impl
		{" EOR\0", 2, am::IMM, &NESCPU::execute_eor }, // 0x49 : EOR #
		{" LSR\0", 1, am::ACC, &NESCPU::execute_lsr }, // 0x4a : LSR A
		{" ???\0", 1, am::XXX, &NESCPU::execute_xxx }, // 0x4b : ???
		{" JMP\0", 3, am::ABS, &NESCPU::execute_jmp }, // 0x4c : JMP abs
		{" EOR\0", 3, am::ABS, &NESCPU::execute_eor }, // 0x4d : EOR abs
		{" LSR\0", 3, am::ABS, &NESCPU::execute_lsr }, // 0x4e : LSR abs
		{" ???\0", 1, am::XXX, &NESCPU::execute_xxx }, // 0x4f : ???
		{" BVC\0", 2, am::REL, &NESCPU::execute_bvc }, // 0x50 : BVC rel
		{" EOR\0", 2, am::INY, &NESCPU::execute_eor }, // 0x51 : EOR ind,Y
		{" ???\0", 1, am::XXX, &NESCPU::execute_xxx }, // 0x52 : ???
		{" ???\0", 1, am::XXX, &NESCPU::execute_xxx }, // 0x53 : ???
		{" ???\0", 1, am::XXX, &NESCPU::execute_xxx }, // 0x54 : ???
		{" EOR\0", 2, am::ZPX, &NESCPU::execute_eor }, // 0x55 : EOR zpg,X
		{" LSR\0", 2, am::ZPX, &NESCPU::execute_lsr }, // 0x56 : LSR zpg,X
		{" ???\0", 1, am::XXX, &NESCPU::execute_xxx }, // 0x57 : ???
		{" CLI\0", 1, am::IMP, &NESCPU::execute_cli }, // 0x58 : CLI impl
		{" EOR\0", 3, am::ABY, &NESCPU::execute_eor }, // 0x59 : EOR abs,Y
		{" ???\0", 1, am::XXX, &NESCPU::execute_xxx }, // 0x5a : ???
		{" ???\0", 1, am::XXX, &NESCPU::execute_xxx }, // 0x5b : ???
		{" ???\0", 1, am::XXX, &NESCPU::execute_xxx }, // 0x5c : ???
		{" EOR\0", 3, am::ABX, &NESCPU::execute_eor }, // 0x5d : EOR abs,X
		{" LSR\0", 3, am::ABX, &NESCPU::execute_lsr }, // 0x5e : LSR abs,X
		{" ???\0", 1, am::XXX, &NESCPU::execute_xxx }, // 0x5f : ???
		{" RTS\0", 1, am::IMP, &NESCPU::execute_rts }, // 0x60 : RTS impl
		{" ADC\0", 2, am::XIN, &NESCPU::execute_adc }, // 0x61 : ADC X,ind
		{" ???\0", 1, am::XXX, &NESCPU::execute_xxx }, // 0x62 : ???
		{" ???\0", 1, am::XXX, &NESCPU::execute_xxx }, // 0x63 : ???
		{" ???\0", 1, am::XXX, &NESCPU::execute_xxx }, // 0x64 : ???
		{" ADC\0", 2, am::ZPG, &NESCPU::execute_adc }, // 0x65 : ADC zpg
		{" ROR\0", 2, am::ZPG, &NESCPU::execute_ror }, // 0x66 : ROR zpg
		{" ???\0", 1, am::XXX, &NESCPU::execute_xxx }, // 0x67 : ???
		{" PLA\0", 1, am::IMP, &NESCPU::execute_pla }, // 0x68 : PLA impl
		{" ADC\0", 2, am::IMM, &NESCPU::execute_adc }, // 0x69 : ADC #
		{" ROR\0", 1, am::ACC, &NESCPU::execute_ror }, // 0x6a : ROR A
		{" ???\0", 1, am::XXX, &NESCPU::execute_xxx }, // 0x6b : ???
		{" JMP\0", 3, am::IND, &NESCPU::execute_jmp }, // 0x6c : JMP ind
		{" ADC\0", 3, am::ABS, &NESCPU::execute_adc }, // 0x6d : ADC abs
		{" ROR\0", 3, am::ABS, &NESCPU::execute_ror }, // 0x6e : ROR abs
		{" ???\0", 1, am::XXX, &NESCPU::execute_xxx }, // 0x6f : ???
		{" BVS\0", 2, am::REL, &NESCPU::execute_bvs }, // 0x70 : BVS rel
		{" ADC\0", 2, am::INY, &NESCPU::execute_adc }, // 0x71 : ADC ind,Y
		{" ???\0", 1, am::XXX, &NESCPU::execute_xxx }, // 0x72 : ???
		{" ???\0", 1, am::XXX, &NESCPU::execute_xxx }, // 0x73 : ???
		{" ???\0", 1, am::XXX, &NESCPU::execute_xxx }, // 0x74 : ???
		{" ADC\0", 2, am::ZPX, &NESCPU::execute_adc }, // 0x75 : ADC zpg,X
		{" ROR\0", 2, am::ZPX, &NESCPU::execute_ror }, // 0x76 : ROR zpg,X
		{" ???\0", 1, am::XXX, &NESCPU::execute_xxx }, // 0x77 : ???
		{" SEI\0", 1, am::IMP, &NESCPU::execute_sei }, // 0x78 : SEI impl
		{" ADC\0", 3, am::ABY, &NESCPU::execute_adc }, // 0x79 : ADC abs,Y
		{" ???\0", 1, am::XXX, &NESCPU::execute_xxx }, // 0x7a : ???
		{" ???\0", 1, am::XXX, &NESCPU::execute_xxx }, // 0x7b : ???
		{" ???\0", 1, am::XXX, &NESCPU::execute_xxx }, // 0x7c : ???
		{" ADC\0", 3, am::ABX, &NESCPU::execute_adc }, // 0x7d : ADC abs,X
		{" ROR\0", 3, am::ABX, &NESCPU::execute_ror }, // 0x7e : ROR abs,X
		{" ???\0", 1, am::XXX, &NESCPU::execute_xxx }, // 0x7f : ???
		{" ???\0", 1, am::XXX, &NESCPU::execute_xxx }, // 0x80 : ???
		{" STA\0", 2, am::XIN, &NESCPU::execute_sta }, // 0x81 : STA X,ind
		{" ???\0", 1, am::XXX, &NESCPU::execute_xxx }, // 0x82 : ???
		{" ???\0", 1, am::XXX, &NESCPU::execute_xxx }, // 0x83 : ???
		{" STY\0", 2, am::ZPG, &NESCPU::execute_sty }, // 0x84 : STY zpg
		{" STA\0", 2, am::ZPG, &NESCPU::execute_sta }, // 0x85 : STA zpg
		{" STX\0", 2, am::ZPG, &NESCPU::execute_stx }, // 0x86 : STX zpg
		{" ???\0", 1, am::XXX, &NESCPU::execute_xxx }, // 0x87 : ???
		{" DEY\0", 1, am::IMP, &NESCPU::execute_dey }, // 0x88 : DEY impl
		{" ???\0", 1, am::XXX, &NESCPU::execute_xxx }, // 0x89 : ???
		{" TXA\0", 1, am::IMP, &NESCPU::execute_txa }, // 0x8a : TXA impl
		{" ???\0", 1, am::XXX, &NESCPU::execute_xxx }, // 0x8b : ???
		{" STY\0", 3, am::ABS, &NESCPU::execute_sty }, // 0x8c : STY abs
		{" STA\0", 3, am::ABS, &NESCPU::execute_sta }, // 0x8d : STA abs
		{" STX\0", 3, am::ABS, &NESCPU::execute_stx }, // 0x8e : STX abs
		{" ???\0", 1, am::XXX, &NESCPU::execute_xxx }, // 0x8f : ???
		{" BCC\0", 2, am::REL, &NESCPU::execute_bcc }, // 0x90 : BCC rel
		{" STA\0", 2, am::INY, &NESCPU::execute_sta }, // 0x91 : STA ind,Y
		{" ???\0", 1, am::XXX, &NESCPU::execute_xxx }, // 0x92 : ???
		{" ???\0", 1, am::XXX, &NESCPU::execute_xxx }, // 0x93 : ???
		{" STY\0", 2, am::ZPX, &NESCPU::execute_sty }, // 0x94 : STY zpg,X
		{" STA\0", 2, am::ZPX, &NESCPU::execute_sta }, // 0x95 : STA zpg,X
		{" STX\0", 2, am::ZPY, &NESCPU::execute_stx }, // 0x96 : STX zpg,Y
		{" ???\0", 1, am::XXX, &NESCPU::execute_xxx }, // 0x97 : ???
		{" TYA\0", 1, am::IMP, &NESCPU::execute_tya }, // 0x98 : TYA impl
		{" STA\0", 3, am::ABY, &NESCPU::execute_sta }, // 0x99 : STA abs,Y
		{" TXS\0", 1, am::IMP, &NESCPU::execute_txs }, // 0x9a : TXS impl
		{" ???\0", 1, am::XXX, &NESCPU::execute_xxx }, // 0x9b : ???
		{" ???\0", 1, am::XXX, &NESCPU::execute_xxx }, // 0x9c : ???
		{" STA\0", 3, am::ABX, &NESCPU::execute_sta }, // 0x9d : STA abs,X
		{" ???\0", 1, am::XXX, &NESCPU::execute_xxx }, // 0x9e : ???
		{" ???\0", 1, am::XXX, &NESCPU::execute_xxx }, // 0x9f : ???
		{" LDY\0", 2, am::IMM, &NESCPU::execute_ldy }, // 0xa0 : LDY #
		{" LDA\0", 2, am::XIN, &NESCPU::execute_lda }, // 0xa1 : LDA X,ind
		{" LDX\0", 2, am::IMM, &NESCPU::execute_ldx }, // 0xa2 : LDX #
		{" ???\0", 1, am::XXX, &NESCPU::execute_xxx }, // 0xa3 : ???
		{" LDY\0", 2, am::ZPG, &NESCPU::execute_ldy }, // 0xa4 : LDY zpg
		{" LDA\0", 2, am::ZPG, &NESCPU::execute_lda }, // 0xa5 : LDA zpg
		{" LDX\0", 2, am::ZPG, &NESCPU::execute_ldx }, // 0xa6 : LDX zpg
		{" ???\0", 1, am::XXX, &NESCPU::execute_xxx }, // 0xa7 : ???
		{" TAY\0", 1, am::IMP, &NESCPU::execute_tay }, // 0xa8 : TAY impl
		{" LDA\0", 2, am::IMM, &NESCPU::execute_lda }, // 0xa9 : LDA #
		{" TAX\0", 1, am::IMP, &NESCPU::execute_tax }, // 0xaa : TAX impl
		{" ???\0", 1, am::XXX, &NESCPU::execute_xxx }, // 0xab : ???
		{" LDY\0", 3, am::ABS, &NESCPU::execute_ldy }, // 0xac : LDY abs
		{" LDA\0", 3, am::ABS, &NESCPU::execute_lda }, // 0xad : LDA abs
		{" LDX\0", 3, am::ABS, &NESCPU::execute_ldx }, // 0xae : LDX abs
		{" ???\0", 1, am::XXX, &NESCPU::execute_xxx }, // 0xaf : ???
		{" BCS\0", 2, am::REL, &NESCPU::execute_bcs }, // 0xb0 : BCS rel
		{" LDA\0", 2, am::INY, &NESCPU::execute_lda }, // 0xb1 : LDA ind,Y
		{" ???\0", 1, am::XXX, &NESCPU::execute_xxx }, // 0xb2 : ???
		{" ???\0", 1, am::XXX, &NESCPU::execute_xxx }, // 0xb3 : ???
		{" LDY\0", 2, am::ZPX, &NESCPU::execute_ldy }, // 0xb4 : LDY zpg,X
		{" LDA\0", 2, am::ZPX, &NESCPU::execute_lda }, // 0xb5 : LDA zpg,X
		{" LDX\0", 2, am::ZPY, &NESCPU::execute_ldx }, // 0xb6 : LDX zpg,Y
		{" ???\0", 1, am::XXX, &NESCPU::execute_xxx }, // 0xb7 : ???
		{" CLV\0", 1, am::IMP, &NESCPU::execute_clv }, // 0xb8 : CLV impl
		{" LDA\0", 3, am::ABY, &NESCPU::execute_lda }, // 0xb9 : LDA abs,Y
		{" TSX\0", 1, am::IMP, &NESCPU::execute_tsx }, // 0xba : TSX impl
		{" ???\0", 1, am::XXX, &NESCPU::execute_xxx }, // 0xbb : ???
		{" LDY\0", 3, am::ABX, &NESCPU::execute_ldy }, // 0xbc : LDY abs,X
		{" LDA\0", 3, am::ABX, &NESCPU::execute_lda }, // 0xbd : LDA abs,X
		{" LDX\0", 3, am::ABY, &NESCPU::execute_ldx }, // 0xbe : LDX abs,Y
		{" ???\0", 1, am::XXX, &NESCPU::execute_xxx }, // 0xbf : ???
		{" CPY\0", 2, am::IMM, &NESCPU::execute_cpy }, // 0xc0 : CPY #
		{" CMP\0", 2, am::XIN, &NESCPU::execute_cmp }, // 0xc1 : CMP X,ind
		{" ???\0", 1, am::XXX, &NESCPU::execute_xxx }, // 0xc2 : ???
		{" ???\0", 1, am::XXX, &NESCPU::execute_xxx }, // 0xc3 : ???
		{" CPY\0", 2, am::ZPG, &NESCPU::execute_cpy }, // 0xc4 : CPY zpg
		{" CMP\0", 2, am::ZPG, &NESCPU::execute_cmp }, // 0xc5 : CMP zpg
		{" DEC\0", 2, am::ZPG, &NESCPU::execute_dec }, // 0xc6 : DEC zpg
		{" ???\0", 1, am::XXX, &NESCPU::execute_xxx }, // 0xc7 : ???
		{" INY\0", 1, am::IMP, &NESCPU::execute_iny }, // 0xc8 : INY impl
		{" CMP\0", 2, am::IMM, &NESCPU::execute_cmp }, // 0xc9 : CMP #
		{" DEX\0", 1, am::IMP, &NESCPU::execute_dex }, // 0xca : DEX impl
		{" ???\0", 1, am::XXX, &NESCPU::execute_xxx }, // 0xcb : ???
		{" CPY\0", 3, am::ABS, &NESCPU::execute_cpy }, // 0xcc : CPY abs
		{" CMP\0", 3, am::ABS, &NESCPU::execute_cmp }, // 0xcd : CMP abs
		{" DEC\0", 3, am::ABS, &NESCPU::execute_dec }, // 0xce : DEC abs
		{" ???\0", 1, am::XXX, &NESCPU::execute_xxx }, // 0xcf : ???
		{" BNE\0", 2, am::REL, &NESCPU::execute_bne }, // 0xd0 : BNE rel
		{" CMP\0", 2, am::INY, &NESCPU::execute_cmp }, // 0xd1 : CMP ind,Y
		{" ???\0", 1, am::XXX, &NESCPU::execute_xxx }, // 0xd2 : ???
		{" ???\0", 1, am::XXX, &NESCPU::execute_xxx }, // 0xd3 : ???
		{" ???\0", 1, am::XXX, &NESCPU::execute_xxx }, // 0xd4 : ???
		{" CMP\0", 2, am::ZPX, &NESCPU::execute_cmp }, // 0xd5 : CMP zpg,X
		{" DEC\0", 2, am::ZPX, &NESCPU::execute_dec }, // 0xd6 : DEC zpg,X
		{" ???\0", 1, am::XXX, &NESCPU::execute_xxx }, // 0xd7 : ???
		{" CLD\0", 1, am::IMP, &NESCPU::execute_cld }, // 0xd8 : CLD impl
		{" CMP\0", 3, am::ABY, &NESCPU::execute_cmp }, // 0xd9 : CMP abs,Y
		{" ???\0", 1, am::XXX, &NESCPU::execute_xxx }, // 0xda : ???
		{" ???\0", 1, am::XXX, &NESCPU::execute_xxx }, // 0xdb : ???
		{" ???\0", 1, am::XXX, &NESCPU::execute_xxx }, // 0xdc : ???
		{" CMP\0", 3, am::ABX, &NESCPU::execute_cmp }, // 0xdd : CMP abs,X
		{" DEC\0", 3, am::ABX, &NESCPU::execute_dec }, // 0xde : DEC abs,X
		{" ???\0", 1, am::XXX, &NESCPU::execute_xxx }, // 0xdf : ???
		{" CPX\0", 2, am::IMM, &NESCPU::execute_cpx }, // 0xe0 : CPX #
		{" SBC\0", 2, am::XIN, &NESCPU::execute_sbc }, // 0xe1 : SBC X,ind
		{" ???\0", 1, am::XXX, &NESCPU::execute_xxx }, // 0xe2 : ???
		{" ???\0", 1, am::XXX, &NESCPU::execute_xxx }, // 0xe3 : ???
		{" CPX\0", 2, am::ZPG, &NESCPU::execute_cpx }, // 0xe4 : CPX zpg
		{" SBC\0", 2, am::ZPG, &NESCPU::execute_sbc }, // 0xe5 : SBC zpg
		{" INC\0", 2, am::ZPG, &NESCPU::execute_inc }, // 0xe6 : INC zpg
		{" ???\0", 1, am::XXX, &NESCPU::execute_xxx }, // 0xe7 : ???
		{" INX\0", 1, am::IMP, &NESCPU::execute_inx }, // 0xe8 : INX impl
		{" SBC\0", 2, am::IMM, &NESCPU::execute_sbc }, // 0xe9 : SBC #
		{" NOP\0", 1, am::IMP, &NESCPU::execute_nop }, // 0xea : NOP impl
		{" ???\0", 1, am::XXX, &NESCPU::execute_xxx }, // 0xeb : ???
		{" CPX\0", 3, am::ABS, &NESCPU::execute_cpx }, // 0xec : CPX abs
		{" SBC\0", 3, am::ABS, &NESCPU::execute_sbc }, // 0xed : SBC abs
		{" INC\0", 3, am::ABS, &NESCPU::execute_inc }, // 0xee : INC abs
		{" ???\0", 1, am::XXX, &NESCPU::execute_xxx }, // 0xef : ???
		{" BEQ\0", 2, am::REL, &NESCPU::execute_beq }, // 0xf0 : BEQ rel
		{" SBC\0", 2, am::INY, &NESCPU::execute_sbc }, // 0xf1 : SBC ind,Y
		{" ???\0", 1, am::XXX, &NESCPU::execute_xxx }, // 0xf2 : ???
		{" ???\0", 1, am::XXX, &NESCPU::execute_xxx }, // 0xf3 : ???
		{" ???\0", 1, am::XXX, &NESCPU::execute_xxx }, // 0xf4 : ???
		{" SBC\0", 2, am::ZPX, &NESCPU::execute_sbc }, // 0xf5 : SBC zpg,X
		{" INC\0", 2, am::ZPX, &NESCPU::execute_inc }, // 0xf6 : INC zpg,X
		{" ???\0", 1, am::XXX, &NESCPU::execute_xxx }, // 0xf7 : ???
		{" SED\0", 1, am::IMP, &NESCPU::execute_sed }, // 0xf8 : SED impl
		{" SBC\0", 3, am::ABY, &NESCPU::execute_sbc }, // 0xf9 : SBC abs,Y
		{" ???\0", 1, am::XXX, &NESCPU::execute_xxx }, // 0xfa : ???
		{" ???\0", 1, am::XXX, &NESCPU::execute_xxx }, // 0xfb : ???
		{" ???\0", 1, am::XXX, &NESCPU::execute_xxx }, // 0xfc : ???
		{" SBC\0", 3, am::ABX, &NESCPU::execute_sbc }, // 0xfd : SBC abs,X
		{" INC\0", 3, am::ABX, &NESCPU::execute_inc }, // 0xfe : INC abs,X
		{" ???\0", 1, am::XXX, &NESCPU::execute_xxx }, // 0xff : ???

	};
}

