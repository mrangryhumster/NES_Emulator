#pragma once

#include <cstdint>
#include <vector>
#include <filesystem>
#include <fstream>

#include "NESState.h"
#include "NESMapper.h"

class NESCartrige
{
public:
	NESCartrige();
	~NESCartrige();

	void ClearCartrige();
	bool LoadCartrige(std::string file_name);
	bool LoadDummyCartrige();

	void Reset();
	void Update();
	bool IsCartrigeReady();

	bool SaveState(NESState& state);
	bool LoadState(NESState& state);

	const std::string& GetROMName();
	uint8_t			   GetMapperID();
	uint8_t		       GetMirroringMode();

	uint32_t  GetPRGChunksCount();
	uint32_t  GetPRGSize();
	uint32_t  GetCHRChunksCount();
	uint32_t  GetCHRSize();
	

	//CPU Bus RW operations
	uint8_t CPURead(uint16_t address);
	void    CPUWrite(uint16_t address, uint8_t data);

	//PPU Buss RW operations
	uint8_t PPURead(uint16_t address);
	void    PPUWrite(uint16_t address, uint8_t data);

	//Universal function to intercept VRAM access
	// if true - PPURead required
	// if false - read from instead VRAM by out_address
	bool    PPUInterceptVRAM(uint16_t address,uint16_t* out_address);

private:
	bool m_IsCartrigeReady;
	std::string m_ROMName;

	uint8_t m_MapperID;
	uint8_t m_MirroringMode;

	uint32_t m_PRGChunksCount;
	uint32_t m_CHRChunksCount;

	bool m_IsRAMPresent;
	bool m_IsCHRPresent;

	std::vector<uint8_t> m_RAMMemory;
	std::vector<uint8_t> m_PRGMemory;
	std::vector<uint8_t> m_CHRMemory;

	std::unique_ptr<NESMapper> m_MapperPtr;
};