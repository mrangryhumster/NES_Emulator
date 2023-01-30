#pragma once

#include <cstdint>

#include "NESMapper.h"

class NESMapper_002 : public NESMapper
{
public:
	NESMapper_002(uint32_t prg_chunks, uint32_t chr_chunks, uint8_t mirroring_mode)
	{
		m_PRGChunksCount = prg_chunks;
		m_CHRChunksCount = chr_chunks;
		m_MirroringMode = mirroring_mode;

		m_PRGActiveBank = 0;
	}

	void Reset()
	{
		m_PRGActiveBank = 0;
	}

	void Update()
	{
		// ...same for update
	}

	bool SaveState(NESState& state)
	{
		state.Write(&m_PRGActiveBank, sizeof(uint8_t));
		return true;
	}

	bool LoadState(NESState& state)
	{
		state.Read(&m_PRGActiveBank, sizeof(uint8_t));
		return true;
	}

	//CPU Bus RW operations
	bool CPUReadIntercept(uint16_t address, uint32_t* out_address)
	{
		if (address < 0xC000)
		{
			*out_address = (address & 0x3FFF) | ((m_PRGActiveBank) * 0x4000);
		}
		else
		{
			*out_address = (address & 0x3FFF) | ((m_PRGChunksCount - 1) * 0x4000);
		}
		return false;
	}

	bool CPUWriteIntercept(uint16_t address, uint32_t* out_address, uint8_t data)
	{
		data &= 0x0F;
		if (data < m_PRGChunksCount-1)
			m_PRGActiveBank = data;
		else
			m_PRGActiveBank = 0;
		*out_address = address;
		return true; //intercept write
	}

	//PPU Buss RW operations
	bool PPUReadIntercept(uint16_t address, uint32_t* out_address)
	{
		*out_address = address & 0x1FFF; //No mapping required
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
		if (m_MirroringMode == 0) // Horizontal
		{
			if (address <= 0x07FF)
				*out_address = address & 0x03FF;
			else
				*out_address = 0x03FF + (address & 0x03FF);
		}
		else // Vertical
		{
			//Simplest version - 1 AND it's all what we need
			*out_address = address & 0x07FF;
		}

		return false;
	}

private:
	uint8_t		m_PRGActiveBank;

	uint32_t	m_PRGChunksCount;
	uint32_t	m_CHRChunksCount;
	uint8_t		m_MirroringMode;
};