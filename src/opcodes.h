/**
 * @file opcodes.h
 *
 * @brief The Opcodes for 6502
 *
 * @author E Warren (ethanwarren768@gmail.com)
 * @date 2025-10-04
 */
#ifndef OPCODES_H
#define OPCODES_H

typedef enum {
    // Load/Store Operations
    OC_LDA_IMM =  0xA9,
    OC_LDA_ZP  =  0xA5,
    OC_LDA_ZPX =  0xB5,
    OC_LDA_ABS =  0xAD,
    OC_LDA_ABSX = 0xBD,
    OC_LDA_ABSY = 0xB9,
    OC_LDA_INDX = 0xA1,
    OC_LDA_INDY = 0xB1,
     
    OC_LDX_IMM =  0xA2,
    OC_LDX_ZP  =  0xA6,
    OC_LDX_ZPY =  0xB6,
    OC_LDX_ABS =  0xAE,
    OC_LDX_ABSY = 0xBE,
    
    OC_LDY_IMM =  0xA0,
    OC_LDY_ZP  =  0xA4,
    OC_LDY_ZPX =  0xB4,
    OC_LDY_ABS =  0xAC,
    OC_LDY_ABSX = 0xBC,
    
    OC_STA_ZP  =  0x85,
    OC_STA_ZPX =  0x95,
    OC_STA_ABS =  0x8D,
    OC_STA_ABSX = 0x9D,
    OC_STA_ABSY = 0x99,
    OC_STA_INDX = 0x81,
    OC_STA_INDY = 0x91,
    
    OC_STX_ZP  =  0x86,
    OC_STX_ZPY =  0x96,
    OC_STX_ABS =  0x8E,
    
    OC_STY_ZP  =  0x84,
    OC_STY_ZPX =  0x94,
    OC_STY_ABS =  0x8C,
    
    // Register Transfers
    OC_TAX =      0xAA,
    OC_TAY =      0xA8,
    OC_TXA =      0x8A,
    OC_TYA =      0x98,
    OC_TSX =      0xBA,
    OC_TXS =      0x9A,
    
    // Stack Operations  
    OC_PHA =      0x48,
    OC_PHP =      0x08,
    OC_PLA =      0x68,
    OC_PLP =      0x28,
    
    // Logical Operations
    OC_AND_IMM =  0x29,
    OC_AND_ZP  =  0x25,
    OC_AND_ZPX =  0x35,
    OC_AND_ABS =  0x2D,
    OC_AND_ABSX = 0x3D,
    OC_AND_ABSY = 0x39,
    OC_AND_INDX = 0x21,
    OC_AND_INDY = 0x31,
    
	//ORA
	OC_ORA_IMM =  0x09,
	OC_ORA_ZP =   0x05,
	OC_ORA_ZPX =  0x15,
	OC_ORA_ABS =  0x0D,
	OC_ORA_ABSX = 0x1D,
	OC_ORA_ABSY = 0x19,
	OC_ORA_INDX = 0x01,
	OC_ORA_INDY = 0x11,

	//EOR	
	OC_EOR_IMM =  0x49,
	OC_EOR_ZP =   0x45,
	OC_EOR_ZPX =  0x55,
	OC_EOR_ABS =  0x4D,
	OC_EOR_ABSX = 0x5D,
	OC_EOR_ABSY = 0x59,
	OC_EOR_INDX = 0x41,
	OC_EOR_INDY = 0x51,

	//BIT
	OC_BIT_ZP =   0x24,
	OC_BIT_ABS =  0x2C,

	//ADC
	OC_ADC_IMM =  0x69,
	OC_ADC_ZP =   0x65,
	OC_ADC_ZPX =  0x75,
	OC_ADC_ABS =  0x6D,
	OC_ADC_ABSX = 0x7D,
	OC_ADC_ABSY = 0x79,
	OC_ADC_INDX = 0x61,
	OC_ADC_INDY = 0x71,

	//SBC
	OC_SBC_IMM =  0xE9,
	OC_SBC_ZP =   0xE5,
	OC_SBC_ZPX =  0xF5,
	OC_SBC_ABS =  0xED,
	OC_SBC_ABSX = 0xFD,
	OC_SBC_ABSY = 0xF9,
	OC_SBC_INDX = 0xE1,
	OC_SBC_INDY = 0xF1,

    //CMP,
    OC_CMP_IMM =  0xC9,
    OC_CMP_ZP =   0xC5,
    OC_CMP_ZPX =  0xD5,
    OC_CMP_ABS =  0xCD,
    OC_CMP_ABSX = 0xDD,
    OC_CMP_ABSY = 0xD9,
    OC_CMP_INDX = 0xC1,
    OC_CMP_INDY = 0xD1,

    //CPX,
    OC_CPX_IMM =  0xE0,
    OC_CPX_ZP =   0xE4,
    OC_CPX_ABS =  0xEC,

    //CPY,
    OC_CPY_IMM =  0xC0,
    OC_CPY_ZP =   0xC4,
    OC_CPY_ABS =  0xCC,

    //Increments & Dec,rements
    //INC,
    OC_INC_ZP =   0xE6,
    OC_INC_ZPX =  0xF6,
    OC_INC_ABS =  0xEE,
    OC_INC_ABSX = 0xFE,

    OC_INX =      0xE8,
    OC_INY =      0xC8,

    //DEC,
    OC_DEC_ZP =   0xC6,
    OC_DEC_ZPX =  0xD6,
    OC_DEC_ABS =  0xCE,
    OC_DEC_ABSX = 0xDE,

    OC_DEX =      0xCA,
    OC_DEY =      0x88,

    //Shifts,
    //ASL,
    OC_ASL_ACC =  0x0A,
    OC_ASL_ZP =   0x06,
    OC_ASL_ZPX =  0x16,
    OC_ASL_ABS =  0x0E,
    OC_ASL_ABSX = 0x1E,

    //LSR,
    OC_LSR_ACC =  0x4A,
    OC_LSR_ZP =   0x46,
    OC_LSR_ZPX =  0x56,
    OC_LSR_ABS =  0x4E,
    OC_LSR_ABSX = 0x5E,

    //ROL,
    OC_ROL_ACC =  0x2A,
    OC_ROL_ZP =   0x26,
    OC_ROL_ZPX =  0x36,
    OC_ROL_ABS =  0x2E,
    OC_ROL_ABSX = 0x3E,

    //ROR,
    OC_ROR_ACC = 0x6A,
    OC_ROR_ZP =   0x66,
    OC_ROR_ZPX =  0x76,
    OC_ROR_ABS =  0x6E,
    OC_ROR_ABSX = 0x7E,

    //Jumps & Calls,
    OC_JMP_ABS =  0x4C,
    OC_JMP_IND =  0x6C,
    OC_JSR_ABS =  0x20,
    OC_RTS =      0x60,

    //Branches,
    OC_BCC =      0x90,
    OC_BCS =      0xB0,
    OC_BEQ =      0xF0,
    OC_BMI =      0x30,
    OC_BNE =      0xD0,
    OC_BPL =      0x10,
    OC_BVC =      0x50,
    OC_BVS =      0x70,

    //Status Flag Channges
    OC_CLC =      0x18,
    OC_CLD =      0xD8,
    OC_CLI =      0x58,
    OC_CLV =      0xB8,
    OC_SEC =      0x38,
    OC_SED =      0xF8,
    OC_SEI =      0x78,

    //System Functions,
    OC_BRK =      0x00,
    OC_NOP =      0xEA,
    OC_RTI =      0x40
} Opcode;

#endif //OPCODES_H
