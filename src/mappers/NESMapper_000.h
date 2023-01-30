#pragma once

#include <cstdint>

#include "NESMapper.h"

class NESMapper_000 : public NESMapper
{
public:
	NESMapper_000(uint32_t prg_chunks, uint32_t chr_chunks, uint8_t mirroring_mode)
	{
		m_PRGChunksCount = prg_chunks;
		m_CHRChunksCount = chr_chunks;
		m_MirroringMode = mirroring_mode;
	}

	void Reset()
	{
		//Reset doesn't do anything in this mapper
	}

	void Update()
	{
		// ...same for update
	}

	bool SaveState(NESState& state)
	{
		// this mapper doesnt require to save its state
		return true;
	}

	bool LoadState(NESState& state)
	{
		// this mapper doesnt require to restore its state
		return true;
	}

	//CPU Bus RW operations
	bool CPUReadIntercept(uint16_t address, uint32_t* out_address)
	{
		if (m_PRGChunksCount == 1) 
			*out_address = address & 0x3FFF; // 16kb ROM
		else 
			*out_address = address & 0x7FFF; // 32kb ROM

		return false;
	}

	bool CPUWriteIntercept(uint16_t address, uint32_t* out_address, uint8_t data)
	{
		//TODO : Add support for Family BASIC
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
		return false; //intercept write
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
	uint32_t	m_PRGChunksCount;
	uint32_t	m_CHRChunksCount;
	uint8_t		m_MirroringMode;
};