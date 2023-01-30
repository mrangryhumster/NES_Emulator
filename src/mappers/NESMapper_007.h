#pragma once

#include <cstdint>

#include "NESMapper.h"

class NESMapper_007 : public NESMapper
{
public:
	NESMapper_007(uint32_t prg_chunks, uint32_t chr_chunks)
	{
		m_PRGChunksCount = prg_chunks;
		m_CHRChunksCount = chr_chunks;

		m_PRGActiveBank = 0;
		m_VRAMTable = 0;
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
		state.Write(&m_VRAMTable, sizeof(uint8_t));
		return true;
	}

	bool LoadState(NESState& state)
	{
		state.Read(&m_PRGActiveBank, sizeof(uint8_t));
		state.Read(&m_VRAMTable, sizeof(uint8_t));
		return true;
	}

	//CPU Bus RW operations
	bool CPUReadIntercept(uint16_t address, uint32_t* out_address)
	{
		*out_address = (address & 0x7FFF) | (m_PRGActiveBank << 15);
		return false;
	}

	bool CPUWriteIntercept(uint16_t address, uint32_t* out_address, uint8_t data)
	{
		//VRAM table switching
		m_VRAMTable = data & 0x10 >> 4;

		//PRG bank switching
		data &= 0x07;
		if ( (uint32_t)(data << 1) < m_PRGChunksCount)
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
		return false; //intercept write
	}

	//Universal function to intercept VRAM access
	bool PPUInterceptVRAM(uint16_t address, uint16_t* out_address)
	{
		*out_address = (address & 0x03FF) | (m_VRAMTable << 10);
		return false;
	}

private:
	uint8_t		m_PRGActiveBank;
	uint8_t		m_VRAMTable;

	uint32_t	m_PRGChunksCount;
	uint32_t	m_CHRChunksCount;
	
};