#include "NESCartrige.h"

#include "NESMapper_000.h"
#include "NESMapper_001.h"
#include "NESMapper_002.h"
#include "NESMapper_007.h"

NESCartrige::NESCartrige()
{
	m_IsCartrigeReady = false;
	m_ROMName = "undefined";

	m_MapperID = 0;
	m_MirroringMode = 0;

	m_PRGChunksCount = 0;
	m_CHRChunksCount = 0;

	m_IsRAMPresent = false;
	m_IsCHRPresent = false;

	m_RAMMemory.clear();
	m_PRGMemory.clear();
	m_CHRMemory.clear();


	m_CHRMemory.resize(m_CHRChunksCount * 0x2000);
}

NESCartrige::~NESCartrige()
{
}

void NESCartrige::ClearCartrige()
{
	if (m_IsCartrigeReady)
	{
		m_IsCartrigeReady = false;
		m_ROMName = "undefined";

		m_MapperID = 0;
		m_MirroringMode = 0;

		m_PRGChunksCount = 0;
		m_CHRChunksCount = 0;

		m_IsRAMPresent = false;
		m_IsCHRPresent = false;

		m_RAMMemory.clear();
		m_PRGMemory.clear();
		m_CHRMemory.clear();

		m_CHRMemory.resize(m_CHRChunksCount * 0x2000);

		m_MapperPtr.release();
	}
}

bool NESCartrige::LoadCartrige(std::string file_name)
{
	m_IsCartrigeReady = true;

	//loading dummy cartrige
	if (file_name.empty()) return LoadDummyCartrige();
	else if(file_name == "dummy")  return LoadDummyCartrige();

	//ines file header
	struct INESFileHeader
	{
		char name[4];
		uint8_t prg_chunks;
		uint8_t chr_chunks;
		uint8_t flags6;
		uint8_t flags7;
		uint8_t flags8;
		uint8_t flags9;
		uint8_t flags10;
		char unused[5];
	} ines_header;
	
	std::ifstream ifs(file_name, std::ifstream::binary);
	if (ifs.is_open())
	{
		ifs.read((char*)&ines_header, sizeof(INESFileHeader));
		
		//   FLAG 6
		//  76543210
		//	||||||||
		//	|||||||+-Mirroring: 0 : horizontal(vertical arrangement) (CIRAM A10 = PPU A11)
		//	|||||||              1 : vertical(horizontal arrangement) (CIRAM A10 = PPU A10)
		//	||||||+-- 1 : Cartridge contains battery - backed PRG RAM($6000 - 7FFF) or other persistent memory
		//	|||||+-- - 1 : 512 - byte trainer at $7000 - $71FF(stored before PRG data)
		//	||||+---- 1 : Ignore mirroring control or above mirroring bit; instead provide four - screen VRAM
		//	++++---- - Lower nybble of mapper number

		//   FLAG 7
		//  76543210
		//	||||||||
		//	|||||||+-VS Unisystem
		//	||||||+--PlayChoice - 10 (8 KB of Hint Screen data stored after CHR data)
		//	||||++-- - If equal to 2, flags 8 - 15 are in NES 2.0 format
		//	++++---- - Upper nybble of mapper number

		m_MapperID = (ines_header.flags7 & 0xF0) | (ines_header.flags6 >> 4);


		m_MirroringMode = (ines_header.flags6 & 0x01);
		
		//Seek over trainer data if present
		if (ines_header.flags6 & 0x04) ifs.seekg(512, std::ios_base::cur);


		if ((ines_header.flags7 & 0x0C) == 0x08)
			printf("Detected NES 2.0 compatible ROM image...\n");

		if ((ines_header.flags6 & 0x02))
		{
			printf("ROM Require 2kb for RAM\n");
			m_RAMMemory.resize(m_PRGChunksCount * 0x2000);
			m_IsRAMPresent = true;
		}
		else
		{
			m_IsRAMPresent = false;
		}

		m_PRGChunksCount = ines_header.prg_chunks;
		m_PRGMemory.resize(m_PRGChunksCount * 0x4000);
		ifs.read((char*)m_PRGMemory.data(), m_PRGMemory.size());

		m_CHRChunksCount = ines_header.chr_chunks;
		m_CHRMemory.resize(m_CHRChunksCount * 0x2000);
		ifs.read((char*)m_CHRMemory.data(), m_CHRMemory.size());

		//Initialize mapper
		switch (m_MapperID)
		{
		case 0:	m_MapperPtr = std::make_unique<NESMapper_000>(m_PRGChunksCount, m_CHRChunksCount, m_MirroringMode);	break;
		case 1:	m_MapperPtr = std::make_unique<NESMapper_001>(m_PRGChunksCount, m_CHRChunksCount);					break;
		case 2:	m_MapperPtr = std::make_unique<NESMapper_002>(m_PRGChunksCount, m_CHRChunksCount, m_MirroringMode);	break;
		case 7:	m_MapperPtr = std::make_unique<NESMapper_007>(m_PRGChunksCount, m_CHRChunksCount);					break;
		default:
			printf("Unknown mapper %.3d\n", m_MapperID);
			printf("Unable to load \"%s\"\n", file_name.c_str());
			ifs.close();
			return LoadDummyCartrige();
		}
		m_MapperPtr->Reset();

		printf("ROM \"%s\" %s\n", file_name.c_str(), m_IsCartrigeReady ? "Loaded" : "Failed to load");
		printf("\tPRG Banks : %d [%d bytes]\n", ines_header.prg_chunks, (int)m_PRGMemory.size());
		printf("\tCHR Banks : %d [%d bytes]\n", ines_header.chr_chunks, (int)m_CHRMemory.size());
		printf("\tTrainer : %d\n", ines_header.flags6 & 0x04);
		printf("\tMapper : %d\n", m_MapperID);
		printf("\tMirroring : %d\n", m_MirroringMode);

		if (ines_header.chr_chunks == 0)
		{
			printf("CHR Tables not present...\nAllocating RAM for CHR\n");
			m_CHRChunksCount = 1;
			m_CHRMemory.resize(m_CHRChunksCount * 0x2000);
			m_IsCHRPresent = false;
		}
		else
		{
			m_IsCHRPresent = true;
		}

		ifs.close();

		m_ROMName = std::filesystem::path(file_name).stem().string();
	}
	else
	{
		printf("File \"%s\" not found\n", file_name.c_str());
		m_IsCartrigeReady = false;
	}

	return m_IsCartrigeReady;
}

bool NESCartrige::LoadDummyCartrige()
{
	printf("Loading dummy cartrige...\n");

	m_IsCartrigeReady = true;

	m_ROMName = "dummy";
	m_MapperID = 0;
	m_MirroringMode = 0;

	m_RAMMemory.clear();
	m_RAMMemory.resize(0x2000);

	m_PRGChunksCount = 1;
	m_PRGMemory.clear();
	m_PRGMemory.resize(m_PRGChunksCount * 0x4000);

	m_CHRChunksCount = 1;
	m_CHRMemory.clear();
	m_CHRMemory.resize(m_CHRChunksCount * 0x2000);

	m_IsRAMPresent = true;
	m_IsCHRPresent = false;

	m_MapperPtr = std::make_unique<NESMapper_000>(m_PRGChunksCount, m_CHRChunksCount, m_MirroringMode);

	return true;
}

void NESCartrige::Reset()
{
	if(!m_IsCartrigeReady) return;
	m_MapperPtr->Reset();
}

void NESCartrige::Update()
{
	if(!m_IsCartrigeReady) return;
	m_MapperPtr->Update();
}

bool NESCartrige::IsCartrigeReady()
{
	return m_IsCartrigeReady;
}

bool NESCartrige::SaveState(NESState& state)
{
	bool result = true;

	state.Write(&m_MapperID,		sizeof(uint8_t));
	state.Write(&m_MirroringMode,	sizeof(uint8_t));
	state.Write(&m_PRGChunksCount,	sizeof(uint32_t));
	state.Write(&m_CHRChunksCount,	sizeof(uint32_t));
	state.Write(&m_IsRAMPresent,    sizeof(bool));
	state.Write(&m_IsCHRPresent,	sizeof(bool));



	if (m_MapperPtr != nullptr)
	{
		result = m_MapperPtr->SaveState(state);
	}

	if (m_IsRAMPresent)
		state.Write(m_RAMMemory.data(), 0x2000);
	if (!m_IsCHRPresent)
		state.Write(m_CHRMemory.data(), sizeof(uint8_t) * 0x2000);

	return result;
}

bool NESCartrige::LoadState(NESState& state)
{
	bool result = true;

	uint8_t		state_MapperID;
	uint8_t		state_MirroringMode;
	uint32_t	state_PRGChunksCount;
	uint32_t	state_CHRChunksCount;
	bool		state_IsRAMPresent;
	bool		state_IsCHRPresent;

	state.Read(&state_MapperID, sizeof(uint8_t));
	state.Read(&state_MirroringMode, sizeof(uint8_t));
	state.Read(&state_PRGChunksCount, sizeof(uint32_t));
	state.Read(&state_CHRChunksCount, sizeof(uint32_t));
	state.Read(&state_IsRAMPresent, sizeof(bool));
	state.Read(&state_IsCHRPresent, sizeof(bool));

	bool missmatch =
		(state_MapperID != m_MapperID) ||
		(state_MirroringMode != m_MirroringMode) ||
		(state_PRGChunksCount != m_PRGChunksCount) ||
		(state_CHRChunksCount != m_CHRChunksCount) ||
		(state_IsRAMPresent != m_IsRAMPresent) ||
		(state_IsCHRPresent != m_IsCHRPresent)
		;

	if (missmatch) 
	{
		printf("Loaded state incompatible with current loaded ROM\n");
		printf("MapperID		S:%d / C:%d\n", state_MapperID,m_MapperID);
		printf("MirroringMode	S:%d / C:%d\n", state_MirroringMode, m_MirroringMode);
		printf("PRGChunksCount	S:%d / C:%d\n", state_PRGChunksCount, m_PRGChunksCount);
		printf("CHRChunksCount	S:%d / C:%d\n", state_CHRChunksCount, m_CHRChunksCount);
		printf("\tbut who cares... HERE WE GOOO!\n");
	}

	if (m_MapperPtr != nullptr)
	{
		result = m_MapperPtr->LoadState(state);
	}

	if (m_IsRAMPresent)
		state.Read(m_RAMMemory.data(), 0x2000);
	if (!m_IsCHRPresent)
		state.Read(m_CHRMemory.data(), sizeof(uint8_t) * 0x2000);

	return result;
}

const std::string& NESCartrige::GetROMName()
{
	return m_ROMName;
}

uint8_t NESCartrige::GetMapperID()
{
	return m_MapperID;
}

uint8_t NESCartrige::GetMirroringMode()
{
	return m_MirroringMode;
}

uint32_t NESCartrige::GetPRGChunksCount()
{
	return m_PRGChunksCount;
}

uint32_t NESCartrige::GetPRGSize()
{
	return (uint32_t)m_PRGMemory.size();
}

uint32_t NESCartrige::GetCHRChunksCount()
{
	return m_CHRChunksCount;
}

uint32_t NESCartrige::GetCHRSize()
{
	return (uint32_t)m_PRGMemory.size();
}

uint8_t NESCartrige::CPURead(uint16_t address)
{
	if (!m_IsCartrigeReady || m_PRGChunksCount == 0) return 0x00;

	uint32_t local_address = 0;

	//Check for interception 
	//(in case if mapper wants to return smth - it will be stored in LO byte of address)
	if (m_MapperPtr->CPUReadIntercept(address, &local_address)) return (uint8_t)local_address;

	return m_PRGMemory[local_address];
}

void NESCartrige::CPUWrite(uint16_t address, uint8_t data)
{
	if (!m_IsCartrigeReady || m_PRGChunksCount == 0) return;

	uint32_t local_address;

	//Check for interception
	if (m_MapperPtr->CPUWriteIntercept(address, &local_address, data)) return;

	m_PRGMemory[local_address] = data;
}

uint8_t NESCartrige::PPURead(uint16_t address)
{
	if (!m_IsCartrigeReady || m_CHRChunksCount == 0) return 0x00;

	uint32_t local_address = 0;

	//Check for interception 
	//(in case if mapper wants to return smth - it will be stored in LO byte of address)
	if (m_MapperPtr->PPUReadIntercept(address, &local_address)) return (uint8_t)local_address;

	if (address <= 0x1FFF)
	{
		return m_CHRMemory[local_address];
	}

	if (address <= 0x3FFF) //Ext. VRAM
	{
		return 0x00; //TODO 
	}

	return 0x00;
}

void NESCartrige::PPUWrite(uint16_t address, uint8_t data)
{
	if (!m_IsCartrigeReady || m_CHRChunksCount == 0) return;

	uint32_t local_address; 

	//Check for interception
	if (m_MapperPtr->PPUWriteIntercept(address, &local_address, data)) return;

	if (address <= 0x1FFF) //CHR
	{
		m_CHRMemory[address] = data;
	}

	if (address <= 0x3FFF) //Ext. VRAM
	{
		//TODO
	}
}

bool NESCartrige::PPUInterceptVRAM(uint16_t address, uint16_t* out_address)
{
	*out_address = 0;
	if (!m_IsCartrigeReady)	return false;
	return m_MapperPtr->PPUInterceptVRAM(address, out_address);
}
