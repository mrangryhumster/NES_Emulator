#pragma once


// **************** MAGIC MACROSES BEGIN ****************

//If defined - ppu wont access device to request palette data
// disabling is usefull for debugging memory access
// because in that case all request will go through main device IO methods
#define PPU_ISOLATE_PALETTES

#if defined(PPU_ISOLATE_PALETTES) 
#define PPU_READ_PALETTE(ADDR) Palettes[ADDR & 0x1F]
#else
#define PPU_READ_PALETTE(ADDR) m_NESDevicePtr->PPURead(0x3F00 + ADDR)
#endif

#define WRITE_BIT_FIELD(REGISTER,VALUE,FIELD) REGISTER = (REGISTER & ~FIELD##_mask) | (((VALUE) & FIELD) << FIELD##_padding)
#define READ_BIT_FIELD(REGISTER,FIELD) ((REGISTER & FIELD##_mask) >> FIELD##_padding)

#define SET_BIT_FIELD(REGISTER,FIELD) REGISTER |= FIELD##_mask
#define CLEAR_BIT_FIELD(REGISTER,FIELD) REGISTER &= ~FIELD##_mask
#define TOGGLE_BIT_FIELD(REGISTER,FIELD) REGISTER ^= FIELD##_mask

#define GET_BIT_FIELD(REGISTER,FIELD) (REGISTER & FIELD##_mask)


// **************** MAGIC MACROSES ENDS ****************



// *****************************************************
//Well... i forced to use manual bitwise operations instead of bitfields
// because in some cases my compiller produce strange aligment
// and this lead to VERY unpredictable errors 
// i spent whole day to spot such error
// so now im relying on preprocessor instead of bitfields


struct PPURegister
{
	enum Addresses
	{
		PPUCTRL   = 0x00,
		PPUMASK   = 0x01,
		PPUSTATUS = 0x02,
		OAMADDR   = 0x03,
		OAMDATA   = 0x04,
		PPUSCROLL = 0x05,
		PPUADDR   = 0x06,
		PPUDATA   = 0x07,
	};
};

struct PPUCTRL
{
	enum Layout
	{
		nametable_addr		= 0x03, nametable_addr_padding		= 0, nametable_addr_mask	= 0x03,
		increment_addr		= 0x01, increment_addr_padding		= 2, increment_addr_mask	= 0x04,
		sprite_table		= 0x01, sprite_table_padding		= 3, sprite_table_mask		= 0x08,
		background_table	= 0x01, background_table_padding	= 4, background_table_mask	= 0x10,
		sprite_size			= 0x01, sprite_size_padding			= 5, sprite_size_mask		= 0x20,
		slave_mode			= 0x01, slave_mode_padding			= 6, slave_mode_mask		= 0x40,
		enable_nmi			= 0x01, enable_nmi_padding			= 7, enable_nmi_mask		= 0x80,
	};
};

struct PPUMASK
{;
	enum Layout
	{
		grayscale			= 0x01, grayscale_padding				= 0, grayscale_mask				= 0x01,
		render_tl_background= 0x01, render_tl_background_padding	= 1, render_tl_background_mask	= 0x02,
		render_tl_sprites   = 0x01, render_tl_sprites_padding		= 2, render_tl_sprites_mask		= 0x04,
		render_background	= 0x01, render_background_padding		= 3, render_background_mask		= 0x08,
		render_sprites		= 0x01, render_sprites_padding			= 4, render_sprites_mask		= 0x10,
		emphasize_red		= 0x01, emphasize_red_padding			= 5, emphasize_red_mask			= 0x20,
		emphasize_green		= 0x01, emphasize_green_padding			= 6, emphasize_green_mask		= 0x40,
		emphasize_blue		= 0x01, emphasize_blue_padding			= 7, emphasize_blue_mask		= 0x80,
		//Additional difinitions for convinience
		rendering_enabled   = 0x02, rendering_enabled_padding       = 3, rendering_enabled_mask     = 0x18,
	};
};

struct PPUSTATUS
{
	enum Layout
	{
		open_bus			= 0x1F, open_bus_padding		= 0, open_bus_mask			 = 0x1F,
		sprite_overflow		= 0x01, sprite_overflow_padding	= 5, sprite_overflow_mask	 = 0x20,
		sprite_zero_hit		= 0x01, sprite_zero_hit_padding	= 6, sprite_zero_hit_mask	 = 0x40,
		vblank_started		= 0x01, vblank_started_padding	= 7, vblank_started_mask	 = 0x80,
	};
};

struct PPUSCROLL
{
	enum Layout
	{
		fine				= 3, fine_padding   = 0, fine_mask	 = 0x07,
		coarse				= 5, coarse_padding = 3, coarse_mask = 0xF8,
	};
};

struct PPUInternalRegister
{
	enum Layout
	{
		coarse_x			= 0x1F, coarse_x_padding			=  0, coarse_x_mask			= 0x001F,
		coarse_y			= 0x1F, coarse_y_padding			=  5, coarse_y_mask			= 0x03E0,
		nametable_select    = 0x03, nametable_select_padding	= 10, nametable_select_mask = 0x0C00,
		fine_y				= 0x07, fine_y_padding				= 12, fine_y_mask			= 0x7000,
		//Additional difinitions for convinience
		nametable_x = 0x01, nametable_x_padding = 10, nametable_x_mask = 0x0400,
		nametable_y = 0x01, nametable_y_padding = 11, nametable_y_mask = 0x0800,
	};
};