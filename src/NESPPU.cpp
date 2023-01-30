#include "NESPPU.h"
#include "NESPPUHelpers.h"
#include "NESDevice.h"

NESPPU::NESPPU(NESDevice* nesDevice)
{
	this->m_NESDevicePtr = nesDevice;

	this->m_RGB_Framebuffer = new RGBPixel[256 * 256];
	this->m_RGB_Patterntable[0] = new RGBPixel[128 * 128];
	this->m_RGB_Patterntable[1] = new RGBPixel[128 * 128];
	this->m_RGB_Nametables = new RGBPixel[512 * 512];

	memset(this->m_RGB_Framebuffer, 0x20, 256 * 256 * sizeof(RGBPixel));

	m_IsFrameReady = false;
	m_IsLineReady = false;

	this->Reset();
}

NESPPU::~NESPPU()
{
	delete[] this->m_RGB_Framebuffer;
	delete[] this->m_RGB_Patterntable[0];
	delete[] this->m_RGB_Patterntable[1];
	delete[] this->m_RGB_Nametables;

	m_IsFrameReady = false;
}

uint8_t NESPPU::CPURead(uint16_t address)
{
	//No idea how to explain this
	// better to look here : https://www.nesdev.org/wiki/PPU_registers
	//						 https://www.nesdev.org/wiki/PPU_scrolling

	//readonly allows to read writeonly registers
	// and doesn't alter the states of other
	uint8_t result_data = 0x00;
	switch (address)
	{
	case 0x2000: //PPUCTRL (writeonly)
		break;
	case 0x2001: //PPUMASK (writeonly)
		break;
	case 0x2002: //PPUSTATUS
		//Reading 1 PPU clock before VBL should suppress setting
		if (PPUScanline == 241 && PPUCycle == 0) //V-Blank
			SupressVBL = true;
		//NOTE: LO 5 bits supposed to be part of internal data buffer...yea?
		result_data = (PPURegisters[PPURegister::PPUSTATUS] & 0xE0) | (InternalReadBuffer & 0x1F);
		//Clear VBlank flag
		CLEAR_BIT_FIELD(PPURegisters[PPURegister::PPUSTATUS], PPUSTATUS::vblank_started);
		//Reset PPUADDR latch
		InternalWriteLatch = false;
		break;
	case 0x2003: //OAMADDR (writeonly)
		break; 
	case 0x2004: //OAMDATA
		result_data = OAMData[PPURegisters[PPURegister::OAMADDR]];
		break;
	case 0x2005: //PPUSCROLL
		break;
	case 0x2006: //PPUADDR
		break;
	case 0x2007: //PPUDATA
		//Buffer vram read and return previous buffer
		if (VRAMRegister & 0xFFE0) 
		{
			PPURegisters[PPURegister::PPUDATA] = InternalReadBuffer;
			result_data = InternalReadBuffer;
			InternalReadBuffer = m_NESDevicePtr->PPURead(VRAMRegister);

		}
		else //if vram address located in palette memory - update register and return value
		{
			InternalReadBuffer = m_NESDevicePtr->PPURead(VRAMRegister);
			PPURegisters[PPURegister::PPUDATA] = InternalReadBuffer;
			result_data = InternalReadBuffer;
		}
		//Increment address based on I flag 0x2000 
		//if 0: increment by 1 [across]; if 1: increment by 32 [down]
		VRAMRegister += GET_BIT_FIELD(PPURegisters[PPURegister::PPUCTRL],PPUCTRL::increment_addr) ? 0x20 : 0x01;
		break;
	case 0x4014: //OAMDMA
		break;
	}

	return result_data; 
}

void NESPPU::CPUWrite(uint16_t address, uint8_t data)
{
	//No idea how to explain this
	// better to look here : https://www.nesdev.org/wiki/PPU_registers
	//						 https://www.nesdev.org/wiki/PPU_scrolling

	switch (address)
	{
	case 0x2000: //PPUCTRL
		PPURegisters[PPURegister::PPUCTRL] = data;
		{
			uint8_t nametable_select = READ_BIT_FIELD(PPURegisters[PPURegister::PPUCTRL], PPUCTRL::nametable_addr);
			WRITE_BIT_FIELD(TRAMRegister, nametable_select, PPUInternalRegister::nametable_select);
		}
		break;
	case 0x2001: //PPUMASK
		PPURegisters[PPURegister::PPUMASK] = data;
		break;
	case 0x2002: //PPUSTATUS (readonly)
		break;
	case 0x2003: //OAMADDR
		PPURegisters[PPURegister::OAMADDR] = data;
		break;
	case 0x2004: //OAMDATA
		OAMData[PPURegisters[PPURegister::OAMADDR]] = data;
		PPURegisters[PPURegister::OAMADDR]++;
		break;
	case 0x2005: //PPUSCROLL
		PPURegisters[PPURegister::PPUSCROLL] = data;
		if (!InternalWriteLatch)
		{
			//t: ....... ...ABCDE <- d : ABCDE...
			//x :			  FGH <- d : .....FGH
			//w :				  <- 1
			uint8_t coarse_x = READ_BIT_FIELD(PPURegisters[PPURegister::PPUSCROLL], PPUSCROLL::coarse);
			WRITE_BIT_FIELD(TRAMRegister, coarse_x, PPUInternalRegister::coarse_x);
			FineX = READ_BIT_FIELD(PPURegisters[PPURegister::PPUSCROLL], PPUSCROLL::fine);
			InternalWriteLatch = true;
		}
		else
		{
			//t: FGH..AB CDE..... <- d : ABCDEFGH
			//w :				  <- 1
			uint8_t coarse_y = READ_BIT_FIELD(PPURegisters[PPURegister::PPUSCROLL], PPUSCROLL::coarse);
			uint8_t fine_y = READ_BIT_FIELD(PPURegisters[PPURegister::PPUSCROLL], PPUSCROLL::fine);
			WRITE_BIT_FIELD(TRAMRegister, coarse_y, PPUInternalRegister::coarse_y);
			WRITE_BIT_FIELD(TRAMRegister, fine_y, PPUInternalRegister::fine_y);
			InternalWriteLatch = false;
		}
		break;
	case 0x2006: //PPUADDR
		PPURegisters[PPURegister::PPUADDR] = data;
		if (!InternalWriteLatch)
		{
			//t: .CDEFGH ........ <- d: ..CDEFGH
			//       <unused>     <- d: AB......
			//t: Z...... ........ <- 0 (bit Z is cleared)
			//w:                  <- 1
			TRAMRegister = (TRAMRegister & 0x00FF) | ((data & 0x3F) << 8);
			InternalWriteLatch = true;
		}
		else
		{
			//t: ....... ABCDEFGH <- d: ABCDEFGH
			//v: <...all bits...> <- t: <...all bits...>
			//w:                  <- 0
			TRAMRegister = (TRAMRegister & 0xFF00) | data;
			VRAMRegister = TRAMRegister;
			InternalWriteLatch = false;
		}
		break;
	case 0x2007: //PPUDATA
		PPURegisters[PPURegister::PPUDATA] = data;
		m_NESDevicePtr->PPUWrite(VRAMRegister, PPURegisters[PPURegister::PPUDATA]);

		//Increment address based on I flag 0x2000 
		//if 0: increment by 1 [across]; if 1: increment by 32 [down]
		VRAMRegister += GET_BIT_FIELD(PPURegisters[PPURegister::PPUCTRL], PPUCTRL::increment_addr) ? 0x20 : 0x01;
		break;
	}

}

uint8_t NESPPU::PPURead(uint16_t address)
{
	// $3F00-$3F1F : Palette RAM indexes 
	// $3F20-$3FFF : Mirrors of $3F00-$3F1F
	if (address <= 0x3FFF)
	{
		address &= 0x001F; 
		
		switch (address) {
		case 0x0000: address = 0x0010; break;
		case 0x0004: address = 0x0014; break;
		case 0x0008: address = 0x0018; break;
		case 0x000C: address = 0x001C; break;
		} return Palettes[address];

		switch (address) {
		case 0x0010: address = 0x0000; break;
		case 0x0014: address = 0x0004; break;
		case 0x0018: address = 0x0008; break;
		case 0x001C: address = 0x000C; break;
		} return Palettes[address];
	}
	return 0xCC;
}

void NESPPU::PPUWrite(uint16_t address, uint8_t data)
{
	// $3F00-$3F1F : Palette RAM indexes 
	// $3F20-$3FFF : Mirrors of $3F00-$3F1F
	if (address <= 0x3FFF)
	{
		address &= 0x001F; 
		
		switch (address) {
		case 0x0000: address = 0x0010; break;
		case 0x0004: address = 0x0014; break;
		case 0x0008: address = 0x0018; break;
		case 0x000C: address = 0x001C; break;
		} Palettes[address] = data;

		switch (address) {
		case 0x0010: address = 0x0000; break;
		case 0x0014: address = 0x0004; break;
		case 0x0018: address = 0x0008; break;
		case 0x001C: address = 0x000C; break;
		} Palettes[address] = data;

		return;
	}
}

uint8_t NESPPU::CPUPeek(uint16_t address)
{
	//Modified CPURead
	return PPURegisters[address & 0x07];
}


void NESPPU::Reset()
{
	memset(Palettes, 0, 32);
	memset(PPURegisters, 0, 8);
	memset(OAMData, 0xFF, 256);
	memset(SecondOAMData, 0xFF, 32);

	//Internal registers
	VRAMRegister = 0x0000;
	TRAMRegister = 0x0000;
	FineX = 0x00;
	InternalWriteLatch = false;
	InternalReadBuffer = 0x00;

	//Background internal rendering data 
	BackgroundLOShiftRegister = 0x0000;
	BackgroundHIShiftRegister = 0x0000;
	BackgroundAttribRegister = 0x00;

	NextPatternLOByte = 0x00;
	NextPatternHIByte = 0x00;
	NextTile = 0x00;
	NextAttrib = 0x00;

	SecondOAMSprites = 0;

	//Signals
	IsEmitingNMI = false;

	//Spacial cases
	SupressVBL = false;

	m_IsFrameReady = false;
	m_IsLineReady = false;

	DBG_ScrollX = 0;
	DBG_ScrollY = 0;
	DBG_GlobalCycle = 0;

	PPUCycle = 0;
	PPUScanline = 0;
	PPUFrameCycle = 0;
	PPUFrameCounter = 0;
}

void NESPPU::Update()
{
	//ohhh.... yeah... this... thing...
	//for better understanding that happening here...
	// https://www.nesdev.org/wiki/PPU_rendering

	if (PPUCycle == 0)
	{
		m_IsLineReady = false;
		if (PPUScanline == -1)
			m_IsFrameReady = false;
	}

	//First cycle of visible range
	if (PPUScanline == 0 && PPUCycle == 0)
	{
		//Skip odd frame if rendering enabled
		if (GET_BIT_FIELD(PPURegisters[PPURegister::PPUMASK], PPUMASK::rendering_enabled) && (PPUFrameCounter & 0x01))
		{
			PPUCycle = 1;
		}
	}

	if (PPUScanline == -1 && PPUCycle == 1)
	{
		//printf("VBLANK clear at %d %d %d\n", PPUCycle, PPUScanline, PPUFrameCycle);
		CLEAR_BIT_FIELD(PPURegisters[PPURegister::PPUSTATUS], PPUSTATUS::vblank_started);
		CLEAR_BIT_FIELD(PPURegisters[PPURegister::PPUSTATUS], PPUSTATUS::sprite_zero_hit);
		CLEAR_BIT_FIELD(PPURegisters[PPURegister::PPUSTATUS], PPUSTATUS::sprite_overflow);
	}

	if (PPUScanline == 241 && PPUCycle == 1) //V-Blank
	{
		if (!SupressVBL)
		{
			//printf("VBLANK set at %d %d %d\n", PPUCycle, PPUScanline, PPUFrameCycle);
			SET_BIT_FIELD(PPURegisters[PPURegister::PPUSTATUS], PPUSTATUS::vblank_started);
			if (GET_BIT_FIELD(PPURegisters[PPURegister::PPUCTRL], PPUCTRL::enable_nmi))
			{
				IsEmitingNMI = true;
			}
		}
		else
		{
			SupressVBL = false;
		}
	}

	//Only visible scanlines and rendering enabled
	if (PPUScanline < 240 && GET_BIT_FIELD(PPURegisters[PPURegister::PPUMASK], PPUMASK::rendering_enabled))
	{
		if ((PPUCycle >= 1 && PPUCycle <= 256) || (PPUCycle >= 321 && PPUCycle <= 336))
		{
			// **************** Shift registers operations ****************
			//Shift registers advancing
			BackgroundLOShiftRegister <<= 1;
			BackgroundHIShiftRegister <<= 1;

			// Thanks to : https://www.nesdev.org/wiki/PPU_scrolling#Tile_and_attribute_fetching
			// tile address      = 0x2000 | (v & 0x0FFF)
			// attribute address = 0x23C0 | (v & 0x0C00) | ((v >> 4) & 0x38) | ((v >> 2) & 0x07)
			switch (PPUCycle & 0x07)
			{
			case 1: //Fetch nametable byte
				{
					//Upload tile planes to shift registers
					BackgroundLOShiftRegister = (BackgroundLOShiftRegister & 0xFF00) | NextPatternLOByte;
					BackgroundHIShiftRegister = (BackgroundHIShiftRegister & 0xFF00) | NextPatternHIByte;
					//Upload attrib to shift register
					BackgroundAttribRegister = (BackgroundAttribRegister << 2) | NextAttrib;


					//Construct fetch address
					const uint16_t fetch_addr =
						0x2000					  |	//Nametables offset on ppu bus
						(VRAMRegister & 0x0FFF);	//Taking only 12 significant bits
					//Fetch data
					NextTile = m_NESDevicePtr->PPURead(fetch_addr);
				}
				break;
			case 3: //Fetch attribute table byte
				{
					//Construct fetch address
					const uint16_t fetch_addr =
						0x23C0						 |	//Attribs offset on the ppu bus
						(VRAMRegister & 0x0C00)      |	//Nametables offset
						((VRAMRegister >> 4) & 0x38) |	//Divide coarse y by 4 (>>2) and append it ((>>2)&0x38)
						((VRAMRegister >> 2) & 0x07);	//Divide coarse x by 4 (>>2) and append it (&0x07)
					//Contruct attrib offset
					const uint8_t shift =
						(VRAMRegister >> 4) & 0x04 |// V: [........ .B....A.] -> SHIFT: [.....BA.]
						(VRAMRegister >> 0) & 0x02;	// Clever way to get shift for tile in attrib byte 
					//Fetch data and instantly isolate required attrib
					// also shift result 2 left, just for convenience in next calc 
					NextAttrib = ((m_NESDevicePtr->PPURead(fetch_addr) >> shift) & 0x03) << 2;
				}
				break;
			case 5: //Fetch pattern table tile low
				{
					//Construct fetch address
					const uint16_t fetch_addr = 
						(uint16_t)	(PPURegisters[PPURegister::PPUCTRL] & 0x10) << 8 |  // Background patterntable 0x0000 or 0x1000
									(VRAMRegister & 0x7000) >> 12					 |  // y offset (fine_y from V register)
									(NextTile << 4) + 0;								  // tileId multiplied by 16
					//Fetch data
					NextPatternLOByte = m_NESDevicePtr->PPURead(fetch_addr);
				}
				break;
			case 7: //Fetch pattern table tile high 
				{
					//Construct fetch address
					const uint16_t fetch_addr =
						(uint16_t)	(PPURegisters[PPURegister::PPUCTRL] & 0x10) << 8 |  // Background patterntable 0x0000 or 0x1000
									(VRAMRegister & 0x7000) >> 12					 |  // y offset (fine_y from V register)
									(NextTile << 4) + 8;							    // tileId multiplied by 16 (+ HI byte offset)
					//Fetch data
					NextPatternHIByte = m_NESDevicePtr->PPURead(fetch_addr);
				}
				break;
			}
			// ******************************** Scroll operations ********************************
			//Scrolling X
			if ((PPUCycle % 8) == 0)
			{
				//if coarse_x == 31
				if ((VRAMRegister & 0x001F) == 31)
				{
					// coarse_x = 0
					CLEAR_BIT_FIELD(VRAMRegister, PPUInternalRegister::coarse_x);
					// switch horizontal nametables
					TOGGLE_BIT_FIELD(VRAMRegister, PPUInternalRegister::nametable_x);
				}
				else
				{
					VRAMRegister += 1;
				}
			}
			//Scrolling Y
			if (PPUCycle == 256)
			{
				//If fine_y of V != 7 - increment fine_y
				if ((VRAMRegister & 0x7000) != 0x7000)
				{
					VRAMRegister += 0x1000;	
				}
				else
				{
					//set fine_y = 0
					VRAMRegister &= ~0x7000;
					uint8_t coarse_y = READ_BIT_FIELD(VRAMRegister, PPUInternalRegister::coarse_y);
					if (coarse_y == 29)
					{
						// coarse_y = 0
						CLEAR_BIT_FIELD(VRAMRegister, PPUInternalRegister::coarse_y);
						// switch vertical nametables
						TOGGLE_BIT_FIELD(VRAMRegister, PPUInternalRegister::nametable_y);
					}
					else if (coarse_y == 31)
					{
						// coarse_y = 0
						CLEAR_BIT_FIELD(VRAMRegister, PPUInternalRegister::coarse_y);
					}
					else
					{
						// increment coarse_y and store it in register
						coarse_y++;
						WRITE_BIT_FIELD(VRAMRegister, coarse_y, PPUInternalRegister::coarse_y);
					}
				}
			}
			// ******************************** Sprites Evaluation ********************************
			//Clear Secondary OAM (Cycles 1 - 64)
			// no need to do it every cycle, we will clear everything at once at the last cycle
			if (PPUScanline >= 0 && PPUCycle == 64)
			{ 
				memset(SecondOAMData, 0xFF, 32);
			}
			//Sprite evaluation (Cycles 65 - 256)
			// again... we will do it at once at 256
			if (PPUScanline >= 0 && PPUCycle == 256) 
			{
				SecondOAMSprites = 0; //Reset sprite counter
				for (uint32_t address = 0; address < 256; address += 4)
				{
					int16_t distance_y = PPUScanline - (int16_t)OAMData[address];
					uint8_t sprite_size = GET_BIT_FIELD(PPURegisters[PPURegister::PPUCTRL], PPUCTRL::sprite_size) ? 16 : 8;
					if (distance_y >= 0 && distance_y < sprite_size)
					{
						if (SecondOAMSprites < 8)
						{
							memcpy(&SecondOAMData[SecondOAMSprites*4], &OAMData[address], sizeof(uint8_t) * 4);

							//Using 'unused' bit in byte 2 to indicate sprite 0
							if (address == 0) OAMData[address + 2] |= 0x1C;
						}
						else
						{
							SET_BIT_FIELD(PPURegisters[PPURegister::PPUSTATUS], PPUSTATUS::sprite_overflow);
							break; //overflow detected, we done here
						}
						SecondOAMSprites++;
					}
				}
			}
			//Sprite fetches (Cycles 257-320)
			// and again... we will do it at once at 321
			if (PPUScanline >= 0 && PPUCycle == 321)
			{
				memset(SpriteOutputUnits, 0xFF, sizeof(SpriteOutputUnit) * 8);
				for (uint8_t sprite = 0; sprite < SecondOAMSprites; sprite++)
				{
					uint8_t address = (sprite << 2);
					uint16_t pattern_addres = 0x00;
					uint16_t distance_w = PPUScanline - SecondOAMData[address];
					uint16_t pattern_line = (SecondOAMData[address + 2] & 0x80) ? 
												(7 - (distance_w) & 0x07) :
													 (distance_w) & 0x07  ;

					if (!GET_BIT_FIELD(PPURegisters[PPURegister::PPUCTRL], PPUCTRL::sprite_size))
					{
						// sprite size 8x8
						pattern_addres =
							((PPURegisters[PPURegister::PPUCTRL] & 0x08) << 9) | //Pattern table offset
							((SecondOAMData[address + 1])			     << 4) | //Tile offset
							pattern_line									   ; //Offset to required byte acording Y and HFlip (if present)
					}
					else
					{
						//sprite size 8x16

						//If V flip is required - inverse distance bit 4
						if (SecondOAMData[address + 2] & 0x80)
							distance_w ^= 0x08;

						pattern_addres =
							((SecondOAMData[address + 1] & 0x01) << 12)	| //Pattern table offset
							((SecondOAMData[address + 1] & 0xFE) << 4)	| //First tile offset
							(distance_w & 0x08) << 1					| //Offset to second half if ditance >= 8
							pattern_line								; //Offset to required byte acording inside tile
					}
					
					//Transfer info
					SpriteOutputUnits[sprite].x_offset = SecondOAMData[address + 3];
					SpriteOutputUnits[sprite].attrib = SecondOAMData[address + 2];
					//Fetch bytes
					SpriteOutputUnits[sprite].pattern_lo = m_NESDevicePtr->PPURead(pattern_addres);
					SpriteOutputUnits[sprite].pattern_hi = m_NESDevicePtr->PPURead(pattern_addres + 8);
				}
			}
		}

		if (PPUScanline == -1 && PPUCycle >= 280 && PPUCycle <= 304)
		{
			//Transfer y components
			//v: GHIA.BC DEF..... <- t: GHIA.BC DEF.....
			VRAMRegister = (VRAMRegister & ~0x7BE0) | (TRAMRegister & 0x7BE0);

			//Save scroll values for debug propurses
			DBG_ScrollY = ((VRAMRegister & 0x0800) >> 3) | ((VRAMRegister & 0x03E0) >> 2) | ((VRAMRegister & 0x7000) >> 12);
		}

		if (PPUCycle == 257)
		{
			//Transfer x components
			//v: ....A.. ...BCDEF <- t: ....A.. ...BCDEF
			VRAMRegister = (VRAMRegister & ~0x041F) | (TRAMRegister & 0x041F);

			//Save scroll values for debug propurses
			DBG_ScrollX = ((VRAMRegister & 0x0400) >> 2) | ((VRAMRegister & 0x001F) << 3) | FineX;

			BackgroundLOShiftRegister = (BackgroundLOShiftRegister & 0xFF00) | NextPatternLOByte;
			BackgroundHIShiftRegister = (BackgroundHIShiftRegister & 0xFF00) | NextPatternHIByte;
		}
	}

	//'Actual' rendering
	if (PPUScanline >= 0 && PPUScanline < 240 && PPUCycle >= 1 && PPUCycle < 257)
	{
		uint8_t screen_color = 0x00;
		uint8_t screen_palette = 0x00;

		//If background enabled - get it color
		if (GET_BIT_FIELD(PPURegisters[PPURegister::PPUMASK], PPUMASK::render_background))
		{
			const uint8_t attrib_offset = ((FineX + ((PPUCycle-1) & 0x07) < 8) ? 2 : 0);
			screen_color = (
				(((BackgroundLOShiftRegister << FineX) & 0x8000) >> 15) |
				(((BackgroundHIShiftRegister << FineX) & 0x8000) >> 14)
			);
			screen_palette = ((BackgroundAttribRegister >> attrib_offset) & 0x0C);
		}

		//Check sprites
		if (GET_BIT_FIELD(PPURegisters[PPURegister::PPUMASK], PPUMASK::render_sprites))
		{
			for (uint8_t sprite = 0; sprite < SecondOAMSprites; sprite++)
			{
				uint8_t sprite_color = 0x00;
				int16_t offset = (PPUCycle - 1) - SpriteOutputUnits[sprite].x_offset;
				if (offset >= 0 && offset < 8)
				{
					if (SpriteOutputUnits[sprite].attrib & 0x40)
					{
						sprite_color = (
							(((SpriteOutputUnits[sprite].pattern_hi >> offset) & 0x01) << 1) |
							(((SpriteOutputUnits[sprite].pattern_lo >> offset) & 0x01) << 0)
							);
					}
					else
					{
						sprite_color = (
							(((SpriteOutputUnits[sprite].pattern_hi << offset) & 0x80) >> 6) |
							(((SpriteOutputUnits[sprite].pattern_lo << offset) & 0x80) >> 7)
							);
					}
				}

				//If sprite with not transparent color 
				if (sprite_color != 0x00)
				{
					//Check zero sprite hit
					if (screen_color != 0x00 && (SpriteOutputUnits[sprite].attrib & 0x1C))
						SET_BIT_FIELD(PPURegisters[PPURegister::PPUSTATUS], PPUSTATUS::sprite_zero_hit);

					//If screen color not present or current sprite has priority
					if (screen_color == 0x00 || !(SpriteOutputUnits[sprite].attrib & 0x20))
					{
						screen_color = sprite_color;
						screen_palette = 0x10 | ((SpriteOutputUnits[sprite].attrib & 0x03) << 2);
					}
					break;
				}
			}
		}

		if(GET_BIT_FIELD(PPURegisters[PPURegister::PPUMASK], PPUMASK::rendering_enabled))
			m_RGB_Framebuffer[((PPUScanline * 256) + (PPUCycle-1))] = 
			m_RGB_Palette[
				Palettes[
					(screen_color & 0x03) ? (screen_palette + screen_color) : 0x00
				]
			];
	}

	//Advance PPU Cycles
	PPUCycle++;
	PPUFrameCycle++;
	if (PPUCycle > 340) //Scanline completed
	{
		PPUCycle = 0;
		PPUScanline++;
		m_IsLineReady = true;

		if (PPUScanline > 260) //Frame completed
		{
			PPUScanline = -1;
			PPUFrameCounter++;
			PPUFrameCycle = 0;	

			m_IsFrameReady = true;
		}
	}
	DBG_GlobalCycle++;
}

bool NESPPU::IsLineReady()
{
	return m_IsLineReady;
}

bool NESPPU::IsFrameReady()
{
	return m_IsFrameReady;
}

bool NESPPU::SaveState(NESState& state)
{
	state.Write(Palettes, sizeof(uint8_t) * 32);
	state.Write(OAMData, sizeof(uint8_t) * 256);
	state.Write(SecondOAMData, sizeof(uint8_t) * 32);
	state.Write(&SecondOAMSprites, sizeof(uint8_t));

	state.Write(PPURegisters, sizeof(uint8_t) * 8);
	state.Write(&VRAMRegister, sizeof(uint16_t));
	state.Write(&TRAMRegister, sizeof(uint16_t));
	state.Write(&FineX, sizeof(uint8_t));
	state.Write(&InternalWriteLatch, sizeof(bool));
	state.Write(&InternalReadBuffer, sizeof(uint8_t));

	state.Write(&BackgroundLOShiftRegister, sizeof(uint16_t));
	state.Write(&BackgroundHIShiftRegister, sizeof(uint16_t));
	state.Write(&BackgroundAttribRegister, sizeof(uint8_t));

	state.Write(&NextPatternLOByte, sizeof(uint8_t));
	state.Write(&NextPatternHIByte, sizeof(uint8_t));
	state.Write(&NextTile,			sizeof(uint8_t));
	state.Write(&NextAttrib,		sizeof(uint8_t));

	state.Write(SpriteOutputUnits, sizeof(NESPPU::SpriteOutputUnit) * 8);

	state.Write(&IsEmitingNMI, sizeof(bool));

	state.Write(&PPUCycle, sizeof(int16_t));
	state.Write(&PPUScanline, sizeof(int16_t));
	state.Write(&PPUFrameCycle, sizeof(uint32_t));
	state.Write(&PPUFrameCounter, sizeof(uint32_t));

	return true;
}

bool NESPPU::LoadState(NESState& state)
{
	state.Read(Palettes, sizeof(uint8_t) * 32);
	state.Read(OAMData, sizeof(uint8_t) * 256);
	state.Read(SecondOAMData, sizeof(uint8_t) * 32);
	state.Read(&SecondOAMSprites, sizeof(uint8_t));

	state.Read(PPURegisters, sizeof(uint8_t) * 8);
	state.Read(&VRAMRegister, sizeof(uint16_t));
	state.Read(&TRAMRegister, sizeof(uint16_t));
	state.Read(&FineX, sizeof(uint8_t));
	state.Read(&InternalWriteLatch, sizeof(bool));
	state.Read(&InternalReadBuffer, sizeof(uint8_t));

	state.Read(&BackgroundLOShiftRegister, sizeof(uint16_t));
	state.Read(&BackgroundHIShiftRegister, sizeof(uint16_t));
	state.Read(&BackgroundAttribRegister, sizeof(uint8_t));

	state.Read(&NextPatternLOByte, sizeof(uint8_t));
	state.Read(&NextPatternHIByte, sizeof(uint8_t));
	state.Read(&NextTile, sizeof(uint8_t));
	state.Read(&NextAttrib, sizeof(uint8_t));

	state.Read(SpriteOutputUnits, sizeof(NESPPU::SpriteOutputUnit) * 8);

	state.Read(&IsEmitingNMI, sizeof(bool));

	state.Read(&PPUCycle, sizeof(int16_t));
	state.Read(&PPUScanline, sizeof(int16_t));
	state.Read(&PPUFrameCycle, sizeof(uint32_t));
	state.Read(&PPUFrameCounter, sizeof(uint32_t));

	return true;
}

void NESPPU::SetRGBPalette(NESPPU::RGBPixel* newPalette)
{
	memcpy(m_RGB_Palette, newPalette, 64 * sizeof(NESPPU::RGBPixel));
}

NESPPU::RGBPixel* NESPPU::GetRGBPalette()
{
	return m_RGB_Palette;
}

const NESPPU::RGBPixel& NESPPU::GetRGBColor(uint8_t nesColor)
{
	return m_RGB_Palette[nesColor & 0x3F];
}

uint8_t* NESPPU::GetFramebuffer()
{
	return (uint8_t*)m_RGB_Framebuffer;
}

uint8_t* NESPPU::ResterizePatterntable(uint8_t id,uint8_t palette)
{
	for (uint16_t tile = 0; tile < 256; tile++)
	{
		//This piece of cra.. code will convert tile index to target RGB buffer position
		// (tile & 0xF0) * 64) - 'vertical' offset
		// ((tile & 0x0F) * 8) - 'horizontal' offset
		uint32_t target_offset = ((tile & 0xF0) * 64) + ((tile & 0x0F) * 8);

		for (uint8_t row = 0; row < 8; row++)
		{
			uint16_t byte_address = (0x1000 * id) + (tile * 16) + row;
			uint8_t lo_plane = m_NESDevicePtr->PPUPeek(byte_address);
			uint8_t hi_plane = m_NESDevicePtr->PPUPeek(byte_address + 8);

			for (uint8_t column = 0; column < 8; column++)
			{
				uint8_t color_index = ((lo_plane & 0x80) >> 7) | (((hi_plane & 0x80) >> 6));
				uint8_t color_nes = Palettes[(palette << 2) + color_index];
				//Advance planes with shift operation
				lo_plane <<= 1; hi_plane <<= 1;
				
				m_RGB_Patterntable[id][target_offset + ((row * 128) + column)] =
					m_RGB_Palette[color_nes];
			}
		}
	}
	return (uint8_t*)m_RGB_Patterntable[id];
}

uint8_t* NESPPU::ResterizeNametables(uint8_t background_table)
{
	// WARNING : IT'S VERY SLLOOOOWWWWWWWW FUUNNNNCCCCCCCCTTTTTTTTTTTTTTTTIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIOO...

	//It's possible to make this function 2 times faster
	// for vertical or horizontal mirroring explicitly
	// but i wanna keep this piece of code more universal

	if (background_table > 1)
		background_table = READ_BIT_FIELD(PPURegisters[PPURegister::PPUCTRL], PPUCTRL::background_table);
	

	//Lookup tables for addresses
	const uint32_t nametables_offsets[4] = { 
		0x2000, // Nametable 0
		0x2400, // Nametable 1
		0x2800, // Nametable 2
		0x2C00  // Nametable 3
	};

	const uint32_t targetrgb_offsets[4] = {
		0x00000, // x =   0; y =   0;
		0x000FF, // x = 256; y =   0;
		0x1E000, // x =   0; y = 240;
		0x1E0FF  // x = 256; y = 240;
	};

	for (uint32_t nametable_id = 0; nametable_id < 4; nametable_id++)
	{
		//Loading metatiles for nametable
		uint8_t metatiles[64];
		for (uint32_t metatile_id = 0; metatile_id < 64; metatile_id++)
		{
			metatiles[metatile_id] = m_NESDevicePtr->PPUPeek(nametables_offsets[nametable_id] + 0x03C0 + metatile_id);
		}

		for (uint32_t tile = 0; tile < 32 * 30; tile++)
		{
			//Get pattern
			uint8_t pattern  = m_NESDevicePtr->PPUPeek(nametables_offsets[nametable_id] + tile);

			//Get metatile color
			uint8_t metatile_color = metatiles[((tile & 0x1C) >> 2) + (((tile & 0x380) >> 7) * 8)];
			metatile_color >>= ((((tile & 0x02) >> 1) + (((tile & 0x40) >> 6) * 2)) * 2);
			metatile_color &= 0x03;

			//Calc rgb buffer offset
			uint32_t targetrgb_offset = ((tile & 0x03E0) * 128) + ((tile & 0x001F) * 8) + targetrgb_offsets[nametable_id];

			//Read pattern and add it to RGB buffer
			for (uint32_t pattern_row = 0; pattern_row < 8; pattern_row++)
			{
				uint32_t pattern_offset   = (0x1000 * background_table) + (pattern * 16) + pattern_row;
				uint8_t pattern_lo_plane = m_NESDevicePtr->PPURead(pattern_offset    );
				uint8_t pattern_hi_plane = m_NESDevicePtr->PPURead(pattern_offset + 8);

				for (uint32_t pattern_column = 0; pattern_column < 8; pattern_column++)
				{
					uint8_t color_index = ((pattern_lo_plane & 0x80) >> 7) | (((pattern_hi_plane & 0x80) >> 6));
					//printf("%d %d\n", color_index);
					uint8_t color_nes = Palettes[color_index ? (metatile_color << 2) + color_index : 0x00];

					m_RGB_Nametables[targetrgb_offset + ((pattern_row * 512) + pattern_column)] = m_RGB_Palette[color_nes];

					pattern_lo_plane <<= 1; pattern_hi_plane <<= 1;
				}
				
			}
		}
	}
	return (uint8_t*) m_RGB_Nametables;
}
