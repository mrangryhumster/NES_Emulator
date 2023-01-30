#pragma once

#include <cstdint>
#include <functional>
#include <vector>

#include "NESState.h"


class NESDevice;

class NESCPU
{
public:
	NESCPU(NESDevice* nesDevice);

	void Reset();
	void Update();
	bool IsReady();

	bool SaveState(NESState& state);
	bool LoadState(NESState& state);

	std::vector<std::string> Disassemble(uint16_t address, uint32_t count,bool include_previous = false);
	// **************** CPU State and statistics ****************
	struct State
	{
		enum class AddrMode : uint8_t
		{
			XXX, //Undefined,
			
			IMM, //Immediate,
			IMP, //Implied,
			ACC, //Accumulator,
			ZPG, //Zeropage,
			ZPX, //ZeropageXIndexed,
			ZPY, //ZeropageYIndexed,
			ABS, //Absolute,
			ABX, //AbsoluteXIndexed,
			ABY, //AbsoluteYIndexed,
			
			XIN, //XIndexedIndirect,
			INY, //IndirectYIndexed,
			IND, //Indirect,
			REL, //Relative,
		};

		//For debugger to show previous opcodes disassembly
		uint16_t LastOperations[8];
		uint16_t CurrentOpPosition = 0;
		AddrMode CurrentAddrMode = AddrMode::XXX;
		uint8_t  CurrentOpCode = 0;
		uint8_t  CycleInternal = 0;
		uint8_t  CycleSkip = 0;

		uint8_t  CycleCounter = 0;
		uint32_t CyclesTotal = 0;

		bool IRQRequest = false;
		bool NMIRequest = false;
		bool DMARequest = false;

		bool Ready;
		bool Halted;
		bool NMIActive;
		bool IRQActive;
		bool DMATransfer;

	}State;

	struct DMA
	{
		bool	 SkipCycle = true;
		uint16_t Address = 0;
		uint8_t  Buffer = 0;
	}DMA;

	// **************** Registers ****************

	struct Registers
	{
		enum class StatusFlags : uint8_t
		{
			NegativeBit = 0b10000000,
			OverflowBit = 0b01000000,
			UnusedBit = 0b00100000,
			BreakBit = 0b00010000,
			DecimalBit = 0b00001000,
			InterruptBit = 0b00000100,
			ZeroBit = 0b00000010,
			CarryBit = 0b00000001,
		};

		uint16_t PC;	//	 program counter(16 bit)
		uint8_t	 AC;	//	 accumulator(8 bit)
		uint8_t  XR;	// X register	(8 bit)
		uint8_t  YR;	// Y register	(8 bit)
		uint8_t  SR;	//	 status register[NV - BDIZC](8 bit)
		uint8_t  SP;	//	 stack pointer(8 bit)


	} Registers;
	using SRFlag = Registers::StatusFlags;

	//Just for convenience
	void setFlag(SRFlag flag, bool val);
	bool getFlag(SRFlag flag);

	// **************** IO Operations ****************

	struct DataBus
	{
		uint16_t	Address;
		uint16_t	Buffer;
		uint8_t		Data;
		
	}DataBus;
	using AddrMode = State::AddrMode;

	//Direct R/W Operations (Proxy functions for NESDevice CPURead/Write)
	uint8_t ReadBus(uint16_t address);
	void    WriteBus(uint16_t address, uint8_t byte);

	//Internal R/W functions utilazing addressing mode
	void	FetchData();
	void	StoreData();
	
protected:
	//Ptr to main device for io operations
	NESDevice* m_NESDevicePtr;

	//Special table for instructions lookup
	struct Instruction
	{
		char		Name[8];
		uint8_t		Size;
		AddrMode    AddressMode;
		bool(NESCPU::* fn)(void) = nullptr;
	};
	std::vector<Instruction> m_InstructionLookup;

	//Dissassemble single operation
	std::string disassemble_op(uint16_t address, uint16_t* next_address);

	// ADC, AND, CMP, EOR, LDA, ORA, SBC, LDX, LDY
	// BIT, CPX, CPY
	bool execute_type_0(std::function<void(void)> op_lambda);
	// ASL, DEC, INC, LSR, ROL, ROR
	bool execute_type_1(std::function<void(void)> op_lambda);
	// STX, STA, STY
	bool execute_type_2(std::function<void(void)> op_lambda);


	/// *** OP CODES *** ///
	bool execute_irq();
	bool execute_nmi();
	bool execute_adc(); // add with carry
	bool execute_and(); // and (with accumulator)
	bool execute_asl(); // arithmetic shift left
	bool execute_bcc(); // branch on carry clear
	bool execute_bcs(); // branch on carry set
	bool execute_beq(); // branch on equal (zero set)
	bool execute_bit(); // bit test
	bool execute_bmi(); // branch on minus (negative set)
	bool execute_bne(); // branch on not equal (zero clear)
	bool execute_bpl(); // branch on plus (negative clear)
	bool execute_brk(); // break / interrupt
	bool execute_bvc(); // branch on overflow clear
	bool execute_bvs(); // branch on overflow set
	bool execute_clc(); // clear carry
	bool execute_cld(); // clear decimal
	bool execute_cli(); // clear interrupt disable
	bool execute_clv(); // clear overflow
	bool execute_cmp(); // compare (with accumulator)
	bool execute_cpx(); // compare with x
	bool execute_cpy(); // compare with y
	bool execute_dec(); // decrement
	bool execute_dex(); // decrement x
	bool execute_dey(); // decrement y
	bool execute_eor(); // exclusive or (with accumulator)
	bool execute_inc(); // increment
	bool execute_inx(); // increment x
	bool execute_iny(); // increment y
	bool execute_jmp(); // jump
	bool execute_jsr(); // jump subroutine
	bool execute_lda(); // load accumulator
	bool execute_ldx(); // load x
	bool execute_ldy(); // load y
	bool execute_lsr(); // logical shift right
	bool execute_nop(); // no operation
	bool execute_ora(); // or with accumulator
	bool execute_pha(); // push accumulator
	bool execute_php(); // push processor status (sr)
	bool execute_pla(); // pull accumulator
	bool execute_plp(); // pull processor status (sr)
	bool execute_rol(); // rotate left
	bool execute_ror(); // rotate right
	bool execute_rti(); // return from interrupt
	bool execute_rts(); // return from subroutine
	bool execute_sbc(); // subtract with carry
	bool execute_sec(); // set carry
	bool execute_sed(); // set decimal
	bool execute_sei(); // set interrupt disable
	bool execute_sta(); // store accumulator
	bool execute_stx(); // store x
	bool execute_sty(); // store y
	bool execute_tax(); // transfer accumulator to x
	bool execute_tay(); // transfer accumulator to y
	bool execute_tsx(); // transfer stack pointer to x
	bool execute_txa(); // transfer x to accumulator
	bool execute_txs(); // transfer x to stack pointer
	bool execute_tya(); // transfer y to accumulator 
	bool execute_xxx(); //unknown opcode

	void prepare_table();
};