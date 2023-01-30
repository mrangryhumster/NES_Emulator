#pragma once

#include <cstdint>

#include "NESMapper.h"

///////////////////////////////////////////////////////////////////////////////////////
//							Placeholder for mapper 001								 //
///////////////////////////////////////////////////////////////////////////////////////

class NESMapper_001 : public NESMapper
{
public:
	NESMapper_001(uint32_t prg_chunks, uint32_t chr_chunks)
	{
		m_PRGChunksCount = prg_chunks;
		m_CHRChunksCount = chr_chunks;
	}

	void Reset()
	{
		m_ControlRegister = 0x0C;

		m_CHRActiveLOBank = 0;
		m_CHRActiveHIBank = 0;
		m_CHRActiveFullBank = 0;

		m_PRGActiveLOBank = 0;
		m_PRGActiveHIBank = m_PRGChunksCount - 1;
		m_PRGActiveFullBank = 0;
	}

	void Update()
	{

	}

	bool SaveState(NESState& state)
	{
		state.Write(&m_ShiftRegister, sizeof(uint8_t));
		state.Write(&m_ShiftPosition, sizeof(uint8_t));

		state.Write(&m_ControlRegister, sizeof(uint8_t));

		state.Write(&m_CHRActiveLOBank, sizeof(uint8_t));
		state.Write(&m_CHRActiveHIBank, sizeof(uint8_t));
		state.Write(&m_CHRActiveFullBank, sizeof(uint8_t));

		state.Write(&m_PRGActiveLOBank, sizeof(uint8_t));
		state.Write(&m_PRGActiveHIBank, sizeof(uint8_t));
		state.Write(&m_PRGActiveFullBank, sizeof(uint8_t));
		return true;
	}

	bool LoadState(NESState& state)
	{
		state.Read(&m_ShiftRegister, sizeof(uint8_t));
		state.Read(&m_ShiftPosition, sizeof(uint8_t));

		state.Read(&m_ControlRegister, sizeof(uint8_t));

		state.Read(&m_CHRActiveLOBank, sizeof(uint8_t));
		state.Read(&m_CHRActiveHIBank, sizeof(uint8_t));
		state.Read(&m_CHRActiveFullBank, sizeof(uint8_t));

		state.Read(&m_PRGActiveLOBank, sizeof(uint8_t));
		state.Read(&m_PRGActiveHIBank, sizeof(uint8_t));
		state.Read(&m_PRGActiveFullBank, sizeof(uint8_t));
		return true;
	}

	//CPU Bus RW operations
	bool CPUReadIntercept(uint16_t address, uint32_t* out_address)
	{
		if (address >= 0x8000)
		{
			if (m_ControlRegister & 0x08)
			{
				if (address < 0xC000)
				{
					*out_address = m_PRGActiveLOBank * 0x4000 + (address & 0x3FFF);
				}
				else
				{
					*out_address = m_PRGActiveHIBank * 0x4000 + (address & 0x3FFF);
				}
			}
			else
			{
				*out_address = m_PRGActiveFullBank * 0x8000 + (address & 0x7FFF);
			}
		}
		return false;
	}

	bool CPUWriteIntercept(uint16_t address, uint32_t* out_address, uint8_t data)
	{
		if (address >= 0x8000)
		{
			if (data & 0x80)
			{
				m_ShiftRegister = 0;
				m_ShiftPosition = 0;
			}
			else
			{
				m_ShiftRegister >>= 1;
				m_ShiftRegister |= (data & 0x01) << 4;
				m_ShiftPosition++;
				if (m_ShiftPosition == 5)
				{
					switch (address & 0xE000)
					{
					case 0x8000://---------------- Control (internal, $8000-$9FFF)
						m_ControlRegister = m_ShiftRegister & 0x1F;
						break;

					case 0xA000://---------------- CHR bank 0 (internal, $A000-$BFFF)
						if (m_ControlRegister & 0x10)
						{
							m_CHRActiveLOBank = m_ShiftRegister & 0x1F;
						}
						else
						{
							m_CHRActiveFullBank = m_ShiftRegister & 0x1E;
						}
						break;
					
					case 0xC000://---------------- CHR bank 1 (internal, $C000-$DFFF)
						if (m_ControlRegister & 0x10)
						{
							m_CHRActiveHIBank = m_ShiftRegister & 0x1F;
						}
						break;

					case 0xE000://---------------- PRG bank (internal, $E000-$FFFF)
						switch (m_ControlRegister & 0x0C)
						{
						case 0x00:// 0x00, 0x04: switch 32 KB at $8000, ignoring low bit of bank number 
						case 0x04: 
								m_PRGActiveFullBank = (m_ShiftRegister & 0x0E) >> 1;
							break;
						case 0x08:// 0x08: fix first bank at $8000 and switch 16 KB bank at $C000
							m_PRGActiveLOBank = 0;
							m_PRGActiveHIBank = m_ShiftRegister & 0x0F;
							break;
						case 0x0C:// 0x0C: fix last bank at $C000 and switch 16 KB bank at $8000
							m_PRGActiveLOBank = m_ShiftRegister & 0x0F;
							m_PRGActiveHIBank = m_PRGChunksCount - 1;
							break;
						}
						break;
					}
					m_ShiftRegister = 0x10;
					m_ShiftPosition = 0;
				}
			}
		}
		return true;
	}

	//PPU Buss RW operations
	bool PPUReadIntercept(uint16_t address, uint32_t* out_address)
	{
		if (m_CHRChunksCount)
		{
			if (m_ControlRegister & 0x10)
			{
				if (address < 0x1000)
				{
					*out_address = (m_CHRActiveLOBank * 0x1000) + (address & 0x0FFF);
				}
				else
				{
					*out_address = (m_CHRActiveHIBank * 0x1000) + (address & 0x0FFF);
				}
			}
			else
			{
				*out_address = m_CHRActiveFullBank * 0x2000 + (address & 0x1FFF);
			}
		}
		else
		{
			*out_address = address & 0x1FFF; //No banks - no mapping
		}
		return false;
	}

	bool PPUWriteIntercept(uint16_t address, uint32_t* out_address, uint8_t data)
	{
		*out_address = address & 0x1FFF;
		return false;
	}

	//Universal function to intercept VRAM access
	bool PPUInterceptVRAM(uint16_t address, uint16_t* out_address)
	{
		switch (m_ControlRegister & 0x03)
		{
		case 0: //0: one-screen, lower bank
			*out_address = (address & 0x03FF);
			break;
		case 1: //1: one-screen, upper bank
			*out_address = (address & 0x03FF) + 0x0400;
			break;
		case 2: //2: vertical
			*out_address = address & 0x07FF;
			break;
		case 3: //3: horizontal
			if (address <= 0x07FF)
				*out_address = address & 0x03FF;
			else
				*out_address = 0x03FF + (address & 0x03FF);
			break;
		}

		return false;
	}

private:
	uint8_t     m_ShiftRegister;
	uint8_t		m_ShiftPosition;

	uint8_t		m_ControlRegister;

	uint8_t		m_CHRActiveLOBank;
	uint8_t		m_CHRActiveHIBank;
	uint8_t		m_CHRActiveFullBank;

	uint8_t		m_PRGActiveLOBank;
	uint8_t		m_PRGActiveHIBank;
	uint8_t		m_PRGActiveFullBank;

	uint32_t	m_PRGChunksCount;
	uint32_t	m_CHRChunksCount;
};