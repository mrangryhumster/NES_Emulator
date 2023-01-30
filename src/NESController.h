#pragma once

#include <cstdint>

class NESController
{

public:

	//CPU Bus RW operations
	uint8_t CPURead(uint16_t address);
	void    CPUWrite(uint16_t address, uint8_t data);

	//Debug operation used to 'peek' into the memory without modifying it
	uint8_t CPUPeek(uint16_t address);
	
	void Reset();
	void Update();

	//Classic Controls
	enum class NESButtons : uint8_t
	{
		BTN_A		= 0x80,
		BTN_B		= 0x40,
		BTN_SELECT	= 0x20,
		BTN_START   = 0x10,
		BTN_UP      = 0x08,
		BTN_DOWN    = 0x04,
		BTN_LEFT    = 0x02,
		BTN_RIGHT   = 0x01,
	};

	void PushButton(uint8_t controller, NESButtons btn);
	void ResetButtons(uint8_t controller);

protected:
	uint8_t m_ControllerRegisters[2];
	uint8_t m_ControllerReads[2];

	//Classic controller
	uint8_t m_ControllerState[2];
};