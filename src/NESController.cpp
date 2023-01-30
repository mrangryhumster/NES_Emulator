#include <stdio.h>
#include "NESController.h"


uint8_t NESController::CPURead(uint16_t address)
{
	address &= 0x0001;

	uint8_t btn_state = (m_ControllerRegisters[address] & 0x80) != 0;
	m_ControllerRegisters[address] <<= 1;
	//All subsequent reads will return 1 on official Nintendo brand controllers
	if (m_ControllerReads[address] == 8) btn_state = 1;
	else m_ControllerReads[address]++;
	

	return btn_state;
}

uint8_t NESController::CPUPeek(uint16_t address)
{
	return (m_ControllerRegisters[address & 0x0001] & 0x80) != 0;
}

void NESController::CPUWrite(uint16_t address, uint8_t data)
{
	m_ControllerRegisters[0] = m_ControllerState[0];
	m_ControllerReads[0] = 0;
	m_ControllerRegisters[1] = m_ControllerState[1];
	m_ControllerReads[1] = 0;
}

void NESController::Reset()
{
	m_ControllerRegisters[0] = 0;
	m_ControllerRegisters[1] = 0;
	m_ControllerReads[0] = 0;
	m_ControllerReads[1] = 0;

	m_ControllerState[0] = 0;
	m_ControllerState[1] = 0;
}

void NESController::Update() 
{

}

void NESController::PushButton(uint8_t controller, NESButtons button)
{
	m_ControllerState[controller] |= (uint8_t)button;
}

void NESController::ResetButtons(uint8_t controller)
{
	m_ControllerState[controller] = 0;
}