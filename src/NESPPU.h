#pragma once

#include <cstdint>
#include <cstdio>
#include <memory>

#include "NESState.h"

class NESDevice;

class NESPPU
{
public:
	NESPPU(NESDevice* nesDevice);
	~NESPPU();

	//CPU Bus RW operations
	uint8_t CPURead(uint16_t address);
	void    CPUWrite(uint16_t address, uint8_t data);

	//PPU Bus RW operations
	uint8_t PPURead(uint16_t address);
	void    PPUWrite(uint16_t address, uint8_t data);

	//Debug operation used to 'peek' into the memory without modifying it
	uint8_t CPUPeek(uint16_t address);


	void Reset();
	void Update();
	bool IsLineReady();
	bool IsFrameReady();

	bool SaveState(NESState& state);
	bool LoadState(NESState& state);

	//Simple structure for rgb pixel
	struct RGBPixel	{ uint8_t r, g, b; };

	//Palette operations
	void      SetRGBPalette(RGBPixel* newPalette);
	RGBPixel* GetRGBPalette();
	const RGBPixel& GetRGBColor(uint8_t nesColor);

	uint8_t* GetFramebuffer();
	//Function for requesting resterization of PPU memory chunks
	uint8_t* ResterizePatterntable(uint8_t id,uint8_t palette = 0);
	//If background_table > 1 setting from PPUCTRL will be used to determine it
	uint8_t* ResterizeNametables(uint8_t background_table = 0xFF);

//All things below should be in class::private space but... 
//	im lazy to write get/set for every register
//	and thay are usefull for debuggingstuff 
//	so everything is public now...

	//Internal palette memory
	uint8_t		Palettes[32];
	//Internal object attrib memory
	uint8_t		OAMData[256];
	uint8_t		SecondOAMData[32];
	uint8_t		SecondOAMSprites;
	//Mapped registers
	uint8_t		PPURegisters[8];
	//Internal registers
	uint16_t	VRAMRegister;
	uint16_t	TRAMRegister;
	uint8_t		FineX;
	bool		InternalWriteLatch;
	uint8_t		InternalReadBuffer;

	//Background internal rendering data 
	uint16_t    BackgroundLOShiftRegister;
	uint16_t    BackgroundHIShiftRegister;
	uint8_t		BackgroundAttribRegister;

	uint8_t		NextPatternLOByte;
	uint8_t		NextPatternHIByte;
	uint8_t     NextTile;
	uint8_t     NextAttrib;

	//Sprites internal rendering data
	struct SpriteOutputUnit
	{
		uint8_t x_offset;
		uint8_t attrib;
		uint8_t pattern_lo;
		uint8_t pattern_hi;
	} SpriteOutputUnits[8];

	//Signals
	bool	 IsEmitingNMI;


	//Special cases
	bool	SupressVBL;

	//State trackers
	int16_t  PPUCycle;
	int16_t  PPUScanline;
	uint32_t PPUFrameCycle;
	uint32_t PPUFrameCounter;

	//Debug
	uint32_t DBG_ScrollX;
	uint32_t DBG_ScrollY;
	uint32_t DBG_GlobalCycle;

protected:

	//Ptr to main device for io operations
	NESDevice* m_NESDevicePtr;

	bool	 m_IsLineReady;
	bool	 m_IsFrameReady;
	
	//Frame buffer for main Viewport
	RGBPixel* m_RGB_Framebuffer;
	//Debug buffers for PPU data viewer
	RGBPixel* m_RGB_Patterntable[2];
	RGBPixel* m_RGB_Nametables;

	//Predefined palette for color conversion
	RGBPixel m_RGB_Palette[64] = {
		{0x55, 0x55, 0x55 }, {0x00, 0x17, 0x73 }, {0x00, 0x07, 0x86 }, {0x2e, 0x05, 0x78 },
		{0x59, 0x02, 0x4d }, {0x72, 0x00, 0x11 }, {0x6e, 0x00, 0x00 }, {0x4c, 0x08, 0x00 },
		{0x17, 0x1b, 0x00 }, {0x00, 0x2a, 0x00 }, {0x00, 0x31, 0x00 }, {0x00, 0x2e, 0x08 },
		{0x00, 0x26, 0x45 }, {0x00, 0x00, 0x00 }, {0x00, 0x00, 0x00 }, {0x00, 0x00, 0x00 },
		{0xa5, 0xa5, 0xa5 }, {0x00, 0x57, 0xc6 }, {0x22, 0x3f, 0xe5 }, {0x6e, 0x28, 0xd9 },
		{0xae, 0x1a, 0xa6 }, {0xd2, 0x17, 0x59 }, {0xd1, 0x21, 0x07 }, {0xa7, 0x37, 0x00 },
		{0x63, 0x51, 0x00 }, {0x18, 0x67, 0x00 }, {0x00, 0x72, 0x00 }, {0x00, 0x73, 0x31 },
		{0x00, 0x6a, 0x84 }, {0x00, 0x00, 0x00 }, {0x00, 0x00, 0x00 }, {0x00, 0x00, 0x00 },
		{0xfe, 0xff, 0xff }, {0x2f, 0xa8, 0xff }, {0x5d, 0x81, 0xff }, {0x9c, 0x70, 0xff },
		{0xf7, 0x72, 0xff }, {0xff, 0x77, 0xbd }, {0xff, 0x7e, 0x75 }, {0xff, 0x8a, 0x2b },
		{0xcd, 0xa0, 0x00 }, {0x81, 0xb8, 0x02 }, {0x3d, 0xc8, 0x30 }, {0x12, 0xcd, 0x7b },
		{0x0d, 0xc5, 0xd0 }, {0x3c, 0x3c, 0x3c }, {0x00, 0x00, 0x00 }, {0x00, 0x00, 0x00 },
		{0xfe, 0xff, 0xff }, {0xa4, 0xde, 0xff }, {0xb1, 0xc8, 0xff }, {0xcc, 0xbe, 0xff },
		{0xf4, 0xc2, 0xff }, {0xff, 0xc5, 0xea }, {0xff, 0xc7, 0xc9 }, {0xff, 0xcd, 0xaa },
		{0xef, 0xd6, 0x96 }, {0xd0, 0xe0, 0x95 }, {0xb3, 0xe7, 0xa5 }, {0x9f, 0xea, 0xc3 },
		{0x9a, 0xe8, 0xe6 }, {0xaf, 0xaf, 0xaf }, {0x00, 0x00, 0x00 }, {0x00, 0x00, 0x00 },
	};

};
