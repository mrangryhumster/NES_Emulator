#pragma once

#include "NESState.h"

class NESMapper
{
public:

	virtual void Reset() = 0;
	virtual void Update() = 0;

	virtual bool SaveState(NESState& state) = 0;
	virtual bool LoadState(NESState& state) = 0;

	//if any of XXXIntercept (except InterceptVRAM) functions return true
	//  then no read/write operation should be performed on cartrige data

	virtual bool CPUReadIntercept(uint16_t address, uint32_t* out_address) = 0;
	virtual bool CPUWriteIntercept(uint16_t address, uint32_t* out_address, uint8_t data) = 0;

	virtual bool PPUReadIntercept(uint16_t address, uint32_t* out_address) = 0;
	virtual bool PPUWriteIntercept(uint16_t address, uint32_t* out_address, uint8_t data) = 0;

	//Universal function to intercept VRAM access
	// if true - PPURead required
	// if false - read from instead VRAM by out_address
	virtual bool PPUInterceptVRAM(uint16_t address, uint16_t* out_address) = 0;

protected:
};