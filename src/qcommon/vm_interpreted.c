/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.
Copyright (C) 2000-2006 Tim Angus

This file is part of Tremulous.

Tremulous is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Tremulous is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Tremulous; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
#include "vm_local.h"

//#define	DEBUG_VM
#ifdef DEBUG_VM
static qchar	*opnames[256] = {
	"OP_UNDEF", 

	"OP_IGNORE", 

	"OP_BREAK",

	"OP_ENTER",
	"OP_LEAVE",
	"OP_CALL",
	"OP_PUSH",
	"OP_POP",

	"OP_CONST",

	"OP_LOCAL",

	"OP_JUMP",

	//-------------------

	"OP_EQ",
	"OP_NE",

	"OP_LTI",
	"OP_LEI",
	"OP_GTI",
	"OP_GEI",

	"OP_LTU",
	"OP_LEU",
	"OP_GTU",
	"OP_GEU",

	"OP_EQF",
	"OP_NEF",

	"OP_LTF",
	"OP_LEF",
	"OP_GTF",
	"OP_GEF",

	//-------------------

	"OP_LOAD1",
	"OP_LOAD2",
	"OP_LOAD4",
	"OP_STORE1",
	"OP_STORE2",
	"OP_STORE4",
	"OP_ARG",

	"OP_BLOCK_COPY",

	//-------------------

	"OP_SEX8",
	"OP_SEX16",

	"OP_NEGI",
	"OP_ADD",
	"OP_SUB",
	"OP_DIVI",
	"OP_DIVU",
	"OP_MODI",
	"OP_MODU",
	"OP_MULI",
	"OP_MULU",

	"OP_BAND",
	"OP_BOR",
	"OP_BXOR",
	"OP_BCOM",

	"OP_LSH",
	"OP_RSHI",
	"OP_RSHU",

	"OP_NEGF",
	"OP_ADDF",
	"OP_SUBF",
	"OP_DIVF",
	"OP_MULF",

	"OP_CVIF",
	"OP_CVFI"
};
#endif

#if idppc

//FIXME: these, um... look the same to me
#if defined(__GNUC__)
static ID_INLINE unsigned qint loadWord(void *addr) {
	unsigned qint word;

	asm("lwbrx %0,0,%1" : "=r" (word) : "r" (addr));
	return word;
}
#else
static ID_INLINE unsigned qint __lwbrx(register void *addr,
		register qint offset) {
	register unsigned qint word;

	asm("lwbrx %0,%2,%1" : "=r" (word) : "r" (addr), "b" (offset));
	return word;
}
#define loadWord(addr) __lwbrx(addr,0)
#endif

#else
    static ID_INLINE qint loadWord(void *addr) {
	qint word;
	memcpy(&word, addr, 4);
	return LittleLong(word);
    }
#endif

qchar *VM_Indent( vm_t *vm ) {
	static qchar	*string = "                                        ";
	if ( vm->callLevel > 20 ) {
		return string;
	}
	return string + 2 * ( 20 - vm->callLevel );
}

void VM_StackTrace( vm_t *vm, qint programCounter, qint programStack ) {
	qint		count;

	count = 0;
	do {
		Com_Printf( "%s\n", VM_ValueToSymbol( vm, programCounter ) );
		programStack =  *(qint *)&vm->dataBase[programStack+4];
		programCounter = *(qint *)&vm->dataBase[programStack];
	} while ( programCounter != -1 && ++count < 32 );

}


/*
====================
VM_PrepareInterpreter
====================
*/
void VM_PrepareInterpreter( vm_t *vm, vmHeader_t *header ) {
	qint		op;
	qint byte_pc;
	qint int_pc;
	byte	*code;
	qint		instruction;
	qint		*codeBase;

	vm->codeBase.ptr = Hunk_Alloc( vm->codeLength*4, h_high );			// we're now qint aligned
//	memcpy( vm->codeBase.ptr, (byte *)header + header->codeOffset, vm->codeLength );

	// we don't need to translate the instructions, but we still need
	// to find each instructions starting point for jumps
	int_pc = byte_pc = 0;
	instruction = 0;
	code = (byte *)header + header->codeOffset;
	codeBase = (qint *)vm->codeBase.ptr;

        //copy and expand instructions to words while building instruction table
	while ( instruction < header->instructionCount ) {
		vm->instructionPointers[ instruction ] = int_pc;
		instruction++;

		op = (qint)code[ byte_pc ];
		codeBase[int_pc] = op;
		if ( byte_pc > header->codeLength ) {
			Com_Error( ERR_FATAL, "VM_PrepareInterpreter: pc > header->codeLength" );
		}

		byte_pc++;
		int_pc++;

		// these are the only opcodes that aren't a single byte
		switch ( op ) {
		case OP_ENTER:
		case OP_CONST:
		case OP_LOCAL:
		case OP_LEAVE:
		case OP_EQ:
		case OP_NE:
		case OP_LTI:
		case OP_LEI:
		case OP_GTI:
		case OP_GEI:
		case OP_LTU:
		case OP_LEU:
		case OP_GTU:
		case OP_GEU:
		case OP_EQF:
		case OP_NEF:
		case OP_LTF:
		case OP_LEF:
		case OP_GTF:
		case OP_GEF:
		case OP_BLOCK_COPY:
			codeBase[int_pc] = loadWord(&code[byte_pc]);
			byte_pc += 4;
			int_pc++;
			break;
		case OP_ARG:
			codeBase[int_pc] = (qint)code[byte_pc];
			byte_pc++;
			int_pc++;
			break;
		default:
			break;
		}

	}
	int_pc = 0;
	instruction = 0;
	code = (byte *)header + header->codeOffset;

        //now that the code has been expanded to int-sized opcodes, we'll translate instruction index
        //int an index into codeBase[], which contains opcodes and operands
	while ( instruction < header->instructionCount ) {
		op = codeBase[ int_pc ];
		instruction++;
		int_pc++;
		switch ( op ) {
		//these ops need to translate addresses in jumps from instruction index to int index
		case OP_EQ:
		case OP_NE:
		case OP_LTI:
		case OP_LEI:
		case OP_GTI:
		case OP_GEI:
		case OP_LTU:
		case OP_LEU:
		case OP_GTU:
		case OP_GEU:
		case OP_EQF:
		case OP_NEF:
		case OP_LTF:
		case OP_LEF:
		case OP_GTF:
		case OP_GEF:
		        //codeBase[pc] is the instruction index, convert that into an offset into
		        //the int-aligned codeBase[] by the lookup table
		        codeBase[int_pc] = vm->instructionPointers[codeBase[int_pc]];
		        int_pc++;
			break;
		//these opcodes have an operand that isn't an instruction index
		case
		OP_ENTER:

                case
                OP_CONST:

                case
                OP_LOCAL:

                case
                OP_LEAVE:

                case
                OP_BLOCK_COPY:

		case OP_ARG:
			int_pc++;
			break;
		default:
			break;
		}

	}
}

/*
==============
VM_PrepareInterpreter2
==============
*/
qbool
VM_PrepareInterpreter2(vm_t *vm, vmHeader_t *header)
{
  const qchar *errMsg;
  instruction_t *buf;

  buf = (instruction_t *)Hunk_Alloc((vm->instructionCount + 8) * sizeof(instruction_t), h_high);

  errMsg = VM_LoadInstructions(header, buf);

  if (!errMsg)
  {
    errMsg = VM_CheckInstructions(buf, vm->instructionCount, vm->jumpTableTargets, vm->numJumpTableTargets, vm->dataLength);
  }

  if (errMsg)
  {
    Com_Printf("VM_PrepareInterpreter2 error: %s\n", errMsg);
    return qfalse;
  }

  vm->codeBase.ptr = (void *)buf;
  return qtrue;
}

/*
==============
VM_Call


Upon a system call, the stack will look like:

sp+32	parm1
sp+28	parm0
sp+24	return stack
sp+20	return address
sp+16	local1
sp+14	local0
sp+12	arg1
sp+8	arg0
sp+4	return stack
sp		return address

An interpreted function will immediately execute
an OP_ENTER instruction, which will subtract space for
locals from sp
==============
*/
#define	DEBUGSTR va(NULL, "%s%i", VM_Indent(vm), opStack-stack )

qint	VM_CallInterpreted( vm_t *vm, qint *args ) {
	qint		stack[OPSTACK_SIZE];
	qint		*opStack;
	qint		programCounter;
	qint		programStack;
	qint		stackOnEntry;
	byte	*image;
	qint		*codeImage;
	qint		v1;
	qint		dataMask;
#ifdef DEBUG_VM
	vmSymbol_t	*profileSymbol;
#endif

	// interpret the code
	vm->currentlyInterpreting = qtrue;

	// we might be called recursively, so this might not be the very top
	programStack = stackOnEntry = vm->programStack;

#ifdef DEBUG_VM
	profileSymbol = VM_ValueToFunctionSymbol( vm, 0 );
	// uncomment this for debugging breakpoints
	vm->breakFunction = 0;
#endif
	// set up the stack frame 

	image = vm->dataBase;
	codeImage = (qint *)vm->codeBase.ptr;
	dataMask = vm->dataMask;
	
	// leave a free spot at start of stack so
	// that as long as opStack is valid, opStack-1 will
	// not corrupt anything
	opStack = stack;
	programCounter = 0;

	programStack -= VM_CALL_PSTACK;

	*(qint *)&image[ programStack + 44] = args[9];
	*(qint *)&image[ programStack + 40] = args[8];
	*(qint *)&image[ programStack + 36] = args[7];
	*(qint *)&image[ programStack + 32] = args[6];
	*(qint *)&image[ programStack + 28] = args[5];
	*(qint *)&image[ programStack + 24] = args[4];
	*(qint *)&image[ programStack + 20] = args[3];
	*(qint *)&image[ programStack + 16] = args[2];
	*(qint *)&image[ programStack + 12] = args[1];
	*(qint *)&image[ programStack + 8 ] = args[0];
	*(qint *)&image[ programStack + 4 ] = 0;	// return stack
	*(qint *)&image[ programStack ] = -1;	// will terminate the loop on return

	VM_Debug(0);

//	vm_debugLevel=2;
	// main interpreter loop, will exit when a LEAVE instruction
	// grabs the -1 program counter

#define r2 codeImage[programCounter]

	while ( 1 ) {
		qint		opcode,	r0, r1;
//		unsigned qint	r2;

nextInstruction:
		r0 = ((qint *)opStack)[0];
		r1 = ((qint *)opStack)[-1];
nextInstruction2:
#ifdef DEBUG_VM
		if ( (unsigned)programCounter >= vm->codeLength ) {
			Com_Error( ERR_DROP, "VM pc out of range" );
		}

		if ( opStack < stack ) {
			Com_Error( ERR_DROP, "VM opStack underflow" );
		}
		if ( opStack >= stack + OPSTACK_SIZE ) {
			Com_Error( ERR_DROP, "VM opStack overflow" );
		}

		if ( programStack <= vm->stackBottom ) {
			Com_Error( ERR_DROP, "VM stack overflow" );
		}

		if ( programStack & 3 ) {
			Com_Error( ERR_DROP, "VM program stack misaligned" );
		}

		if ( vm_debugLevel > 1 ) {
			Com_Printf( "%s %s\n", DEBUGSTR, opnames[opcode] );
		}
		profileSymbol->profileCount++;
#endif
		opcode = codeImage[ programCounter++ ];

		switch ( opcode ) {
#ifdef DEBUG_VM
		default:
			Com_Error( ERR_DROP, "Bad VM instruction" );  // this should be scanned on load!
#endif
		case OP_BREAK:
			vm->breakCount++;
			goto nextInstruction2;
		case OP_CONST:
			opStack++;
			r1 = r0;
			r0 = *opStack = r2;
			
			programCounter += 1;
			goto nextInstruction2;
		case OP_LOCAL:
			opStack++;
			r1 = r0;
			r0 = *opStack = r2+programStack;

			programCounter += 1;
			goto nextInstruction2;

		case OP_LOAD4:
#ifdef DEBUG_VM
			if ( *opStack & 3 ) {
				Com_Error( ERR_DROP, "OP_LOAD4 misaligned" );
			}
#endif
			r0 = *opStack = *(qint *)&image[ r0&dataMask ];
			goto nextInstruction2;
		case OP_LOAD2:
			r0 = *opStack = *(unsigned short *)&image[ r0&dataMask&~1 ];
			goto nextInstruction2;
		case OP_LOAD1:
			r0 = *opStack = image[ r0&dataMask ];
			goto nextInstruction2;

		case OP_STORE4:
			*(qint *)&image[ r1&(dataMask & ~3) ] = r0;
			opStack -= 2;
			goto nextInstruction;
		case OP_STORE2:
			*(short *)&image[ r1&(dataMask & ~1) ] = r0;
			opStack -= 2;
			goto nextInstruction;
		case OP_STORE1:
			image[ r1&dataMask ] = r0;
			opStack -= 2;
			goto nextInstruction;

		case OP_ARG:
			// single byte offset from programStack
			*(qint *)&image[ (codeImage[programCounter] + programStack) & dataMask&~3 ] = r0;
			opStack--;
			programCounter += 1;
			goto nextInstruction;

		case OP_BLOCK_COPY:
			{
				qint		*src, *dest;
				qint		count, srci, desti;

				count = r2;
				// MrE: copy range check
				srci = r0 & dataMask;
				desti = r1 & dataMask;
				count = ((srci + count) & dataMask) - srci;
				count = ((desti + count) & dataMask) - desti;

				src = (qint *)&image[ srci ];
				dest = (qint *)&image[ desti ];
				
				memcpy(dest, src, count);
				programCounter += 1;
				opStack -= 2;
			}
			goto nextInstruction;

		case OP_CALL:
			// save current program counter
			*(qint *)&image[ programStack ] = programCounter;
			
			// jump to the location on the stack
			programCounter = r0;
			opStack--;
			if ( programCounter < 0 ) {
				// system call
				qint		r;
//				qint		temp;
#ifdef DEBUG_VM
				qint		stomped;

				if ( vm_debugLevel ) {
					Com_Printf( "%s---> systemcall(%i)\n", DEBUGSTR, -1 - programCounter );
				}
#endif
				// save the stack to allow recursive VM entry
//				temp = vm->callLevel;
				vm->programStack = programStack - 4;
#ifdef DEBUG_VM
				stomped = *(qint *)&image[ programStack + 4 ];
#endif
				*(qint *)&image[ programStack + 4 ] = -1 - programCounter;

//VM_LogSyscalls( (qint *)&image[ programStack + 4 ] );
				{
					//the vm has ints on the stack, we expect
					//pointers so we might have to convert it
					if (sizeof(intptr_t) != sizeof(qint))
					{
					  intptr_t argarr[16];
					  qint *imagePtr = (qint *)&image[programStack];
					  qint i;

                                          for(i = 0;i < 16;++i)
                                          {
                                            argarr[i] = *(++imagePtr);
                                          }

                                          r = vm->systemCall(argarr);
                                        }
                                        else
                                        {
                                          intptr_t *argptr = (intptr_t *)&image[programStack + 4];
                                          r = vm->systemCall(argptr);
					}
				}

#ifdef DEBUG_VM
				// this is just our stack frame pointer, only needed
				// for debugging
				*(qint *)&image[ programStack + 4 ] = stomped;
#endif

				// save return value
				opStack++;
				*opStack = r;
				programCounter = *(qint *)&image[ programStack ];
//				vm->callLevel = temp;
#ifdef DEBUG_VM
				if ( vm_debugLevel ) {
					Com_Printf( "%s<--- %s\n", DEBUGSTR, VM_ValueToSymbol( vm, programCounter ) );
				}
#endif
			} else if ( (unsigned)programCounter >= vm->instructionCount ) {
				Com_Error( ERR_DROP, "VM program counter out of range in OP_CALL" );
			} else {
				programCounter = vm->instructionPointers[ programCounter ];
			}
			goto nextInstruction;

		// push and pop are only needed for discarded or bad function return values
		case OP_PUSH:
			opStack++;
			goto nextInstruction;
		case OP_POP:
			opStack--;
			goto nextInstruction;

		case OP_ENTER:
#ifdef DEBUG_VM
			profileSymbol = VM_ValueToFunctionSymbol( vm, programCounter );
#endif
			// get size of stack frame
			v1 = r2;

			programCounter += 1;
			programStack -= v1;
#ifdef DEBUG_VM
			// save old stack frame for debugging traces
			*(qint *)&image[programStack+4] = programStack + v1;
			if ( vm_debugLevel ) {
				Com_Printf( "%s---> %s\n", DEBUGSTR, VM_ValueToSymbol( vm, programCounter - 5 ) );
				if ( vm->breakFunction && programCounter - 5 == vm->breakFunction ) {
					// this is to allow setting breakpoints here in the debugger
					vm->breakCount++;
//					vm_debugLevel = 2;
//					VM_StackTrace( vm, programCounter, programStack );
				}
//				vm->callLevel++;
			}
#endif
			goto nextInstruction;
		case OP_LEAVE:
			// remove our stack frame
			v1 = r2;

			programStack += v1;

			// grab the saved program counter
			programCounter = *(qint *)&image[ programStack ];
#ifdef DEBUG_VM
			profileSymbol = VM_ValueToFunctionSymbol( vm, programCounter );
			if ( vm_debugLevel ) {
//				vm->callLevel--;
				Com_Printf( "%s<--- %s\n", DEBUGSTR, VM_ValueToSymbol( vm, programCounter ) );
			}
#endif
			// check for leaving the VM
			if ( programCounter == -1 ) {
				goto done;
			} else if ( (unsigned)programCounter >= vm->codeLength ) {
				Com_Error( ERR_DROP, "VM program counter out of range in OP_LEAVE" );
			}
			goto nextInstruction;

		/*
		===================================================================
		BRANCHES
		===================================================================
		*/

		case OP_JUMP:
			if ((unsigned)r0 >= vm->instructionCount)
			{
			  Com_Error(ERR_DROP, "VM program counter out of range in OP_JUMP");
			}

                        programCounter = vm->instructionPointers[r0];
			opStack--;
			goto nextInstruction;

		case OP_EQ:
			opStack -= 2;
			if ( r1 == r0 ) {
				programCounter = r2;	//vm->instructionPointers[r2];
				goto nextInstruction;
			} else {
				programCounter += 1;
				goto nextInstruction;
			}

		case OP_NE:
			opStack -= 2;
			if ( r1 != r0 ) {
				programCounter = r2;	//vm->instructionPointers[r2];
				goto nextInstruction;
			} else {
				programCounter += 1;
				goto nextInstruction;
			}

		case OP_LTI:
			opStack -= 2;
			if ( r1 < r0 ) {
				programCounter = r2;	//vm->instructionPointers[r2];
				goto nextInstruction;
			} else {
				programCounter += 1;
				goto nextInstruction;
			}

		case OP_LEI:
			opStack -= 2;
			if ( r1 <= r0 ) {
				programCounter = r2;	//vm->instructionPointers[r2];
				goto nextInstruction;
			} else {
				programCounter += 1;
				goto nextInstruction;
			}

		case OP_GTI:
			opStack -= 2;
			if ( r1 > r0 ) {
				programCounter = r2;	//vm->instructionPointers[r2];
				goto nextInstruction;
			} else {
				programCounter += 1;
				goto nextInstruction;
			}

		case OP_GEI:
			opStack -= 2;
			if ( r1 >= r0 ) {
				programCounter = r2;	//vm->instructionPointers[r2];
				goto nextInstruction;
			} else {
				programCounter += 1;
				goto nextInstruction;
			}

		case OP_LTU:
			opStack -= 2;
			if ( ((unsigned)r1) < ((unsigned)r0) ) {
				programCounter = r2;	//vm->instructionPointers[r2];
				goto nextInstruction;
			} else {
				programCounter += 1;
				goto nextInstruction;
			}

		case OP_LEU:
			opStack -= 2;
			if ( ((unsigned)r1) <= ((unsigned)r0) ) {
				programCounter = r2;	//vm->instructionPointers[r2];
				goto nextInstruction;
			} else {
				programCounter += 1;
				goto nextInstruction;
			}

		case OP_GTU:
			opStack -= 2;
			if ( ((unsigned)r1) > ((unsigned)r0) ) {
				programCounter = r2;	//vm->instructionPointers[r2];
				goto nextInstruction;
			} else {
				programCounter += 1;
				goto nextInstruction;
			}

		case OP_GEU:
			opStack -= 2;
			if ( ((unsigned)r1) >= ((unsigned)r0) ) {
				programCounter = r2;	//vm->instructionPointers[r2];
				goto nextInstruction;
			} else {
				programCounter += 1;
				goto nextInstruction;
			}

		case OP_EQF:
			if ( ((float *)opStack)[-1] == *(float *)opStack ) {
				programCounter = r2;	//vm->instructionPointers[r2];
				opStack -= 2;
				goto nextInstruction;
			} else {
				programCounter += 1;
				opStack -= 2;
				goto nextInstruction;
			}

		case OP_NEF:
			if ( ((float *)opStack)[-1] != *(float *)opStack ) {
				programCounter = r2;	//vm->instructionPointers[r2];
				opStack -= 2;
				goto nextInstruction;
			} else {
				programCounter += 1;
				opStack -= 2;
				goto nextInstruction;
			}

		case OP_LTF:
			if ( ((float *)opStack)[-1] < *(float *)opStack ) {
				programCounter = r2;	//vm->instructionPointers[r2];
				opStack -= 2;
				goto nextInstruction;
			} else {
				programCounter += 1;
				opStack -= 2;
				goto nextInstruction;
			}

		case OP_LEF:
			if ( ((float *)opStack)[-1] <= *(float *)opStack ) {
				programCounter = r2;	//vm->instructionPointers[r2];
				opStack -= 2;
				goto nextInstruction;
			} else {
				programCounter += 1;
				opStack -= 2;
				goto nextInstruction;
			}

		case OP_GTF:
			if ( ((float *)opStack)[-1] > *(float *)opStack ) {
				programCounter = r2;	//vm->instructionPointers[r2];
				opStack -= 2;
				goto nextInstruction;
			} else {
				programCounter += 1;
				opStack -= 2;
				goto nextInstruction;
			}

		case OP_GEF:
			if ( ((float *)opStack)[-1] >= *(float *)opStack ) {
				programCounter = r2;	//vm->instructionPointers[r2];
				opStack -= 2;
				goto nextInstruction;
			} else {
				programCounter += 1;
				opStack -= 2;
				goto nextInstruction;
			}


		//===================================================================

		case OP_NEGI:
			*opStack = -r0;
			goto nextInstruction;
		case OP_ADD:
			opStack[-1] = r1 + r0;
			opStack--;
			goto nextInstruction;
		case OP_SUB:
			opStack[-1] = r1 - r0;
			opStack--;
			goto nextInstruction;
		case OP_DIVI:
			opStack[-1] = r1 / r0;
			opStack--;
			goto nextInstruction;
		case OP_DIVU:
			opStack[-1] = ((unsigned)r1) / ((unsigned)r0);
			opStack--;
			goto nextInstruction;
		case OP_MODI:
			opStack[-1] = r1 % r0;
			opStack--;
			goto nextInstruction;
		case OP_MODU:
			opStack[-1] = ((unsigned)r1) % (unsigned)r0;
			opStack--;
			goto nextInstruction;
		case OP_MULI:
			opStack[-1] = r1 * r0;
			opStack--;
			goto nextInstruction;
		case OP_MULU:
			opStack[-1] = ((unsigned)r1) * ((unsigned)r0);
			opStack--;
			goto nextInstruction;

		case OP_BAND:
			opStack[-1] = ((unsigned)r1) & ((unsigned)r0);
			opStack--;
			goto nextInstruction;
		case OP_BOR:
			opStack[-1] = ((unsigned)r1) | ((unsigned)r0);
			opStack--;
			goto nextInstruction;
		case OP_BXOR:
			opStack[-1] = ((unsigned)r1) ^ ((unsigned)r0);
			opStack--;
			goto nextInstruction;
		case OP_BCOM:
			*opStack = ~ ((unsigned)r0);
			goto nextInstruction;

		case OP_LSH:
			opStack[-1] = r1 << r0;
			opStack--;
			goto nextInstruction;
		case OP_RSHI:
			opStack[-1] = r1 >> r0;
			opStack--;
			goto nextInstruction;
		case OP_RSHU:
			opStack[-1] = ((unsigned)r1) >> r0;
			opStack--;
			goto nextInstruction;

		case OP_NEGF:
			*(float *)opStack =  -*(float *)opStack;
			goto nextInstruction;
		case OP_ADDF:
			*(float *)(opStack-1) = *(float *)(opStack-1) + *(float *)opStack;
			opStack--;
			goto nextInstruction;
		case OP_SUBF:
			*(float *)(opStack-1) = *(float *)(opStack-1) - *(float *)opStack;
			opStack--;
			goto nextInstruction;
		case OP_DIVF:
			*(float *)(opStack-1) = *(float *)(opStack-1) / *(float *)opStack;
			opStack--;
			goto nextInstruction;
		case OP_MULF:
			*(float *)(opStack-1) = *(float *)(opStack-1) * *(float *)opStack;
			opStack--;
			goto nextInstruction;

		case OP_CVIF:
			*(float *)opStack =  (float)*opStack;
			goto nextInstruction;
		case OP_CVFI:
			*opStack = (qint) *(float *)opStack;
			goto nextInstruction;
		case OP_SEX8:
			*opStack = (signed qchar)*opStack;
			goto nextInstruction;
		case OP_SEX16:
			*opStack = (short)*opStack;
			goto nextInstruction;
		}
	}

done:
	vm->currentlyInterpreting = qfalse;

	if ( opStack != &stack[1] ) {
		Com_Error( ERR_DROP, "Interpreter error: opStack = %ld", (long qint) (opStack - stack) );
	}

	vm->programStack = stackOnEntry;

	// return the result
	return *opStack;
}

/*
==============
VM_CallInterpreted2
==============
*/
qint
VM_CallInterpreted2(vm_t *vm, qint *args)
{
  //qint stack[MAX_OPSTACK_SIZE];
  qint stack[OPSTACK_SIZE];
  qint *opStack;
  qint programStack;
  qint stackOnEntry;
  byte *image;
  qint v1;
  qint v0;
  qint dataMask;
  instruction_t *inst;
  instruction_t *ci;
  floatint_t r0;
  floatint_t r1;
  qint opcode;

  //interpret the code
  vm->currentlyInterpreting = qtrue;

  //we might be called recursively, so this might not be the very top
  programStack = stackOnEntry = vm->programStack;

  //set up the stack frame
  image = vm->dataBase;
  inst = (instruction_t *)vm->codeBase.ptr;
  dataMask = vm->dataMask;

  //leave a free spot at start of stack so
  //that as long as opStack is valid, opStack - 1 will
  //not corrupt anything
  opStack = stack;

  programStack -= VM_CALL_PSTACK;

  //*(qint *)&image[programStack + 44] = args[9];
  //*(qint *)&image[programStack + 40] = args[8];
  //*(qint *)&image[programStack + 36] = args[7];
  *(qint *)&image[programStack + 32] = args[6];
  *(qint *)&image[programStack + 28] = args[5];
  *(qint *)&image[programStack + 24] = args[4];
  *(qint *)&image[programStack + 20] = args[3];
  *(qint *)&image[programStack + 16] = args[2];
  *(qint *)&image[programStack + 12] = args[1];
  *(qint *)&image[programStack + 8] = args[0];
  *(qint *)&image[programStack + 4] = 0; //return stack
  *(qint *)&image[programStack] = -1; //will terminate loop on return

  ci = inst;

  while(1)
  {
    r0.i = opStack[0];
    r1.i = opStack[-1];

nextInstruction2:

    v0 = ci->value;
    opcode = ci->op;
    ci++;

    switch(opcode)
    {
      case
      OP_BREAK:
        vm->breakCount++;
        goto nextInstruction2;

      case
      OP_ENTER:
        //get size of stack frame
        programStack -= v0;

        if (programStack <= vm->stackBottom)
        {
          Com_Error(ERR_DROP, "VM stack overflow");
        }

        break;

      case
      OP_LEAVE:
        //remove our stack frame
        programStack += v0;

        //grab the saved program counter
        v1 = *(qint *)&image[programStack];

        //check for leaving the VM
        if (v1 == -1)
        {
          goto done;
        }
        else if ((unsigned)v1 >= vm->codeLength)
        {
          Com_Error(ERR_DROP, "VM program counter out of range in OP_LEAVE");
        }

        ci = inst + v1;
        break;


      case
      OP_CALL:
        //save current program counter
        *(qint *)&image[programStack] = ci - inst;

        //jump to the location on the stack
        if (r0.i < 0)
        {
          //system call
          //save the stack to allow recursive VM entry
          vm->programStack = programStack - 4;
          *(qint *)&image[programStack + 4] = ~r0.i;
          {
            intptr_t *argptr = (intptr_t *)&image[programStack + 4];

#if idx64 //__WORDSIZE == 64
            //the vm has qints on the stack, we expect
            //longs so we have to convert it
            intptr_t argarr[16];
            qint i;

            for(i = 0;i < 16;++i)
            {
              argarr[i] = *(qint *)&image[programStack + 4 + 4*i];
            }

            argptr = argarr;
#endif
            v0 = vm->systemCall(argptr);
          }

          //save return value
          //opStack++;
          ci = inst + *(qint *)&image[programStack];
          *opStack = v0;
        }
        else if (r0.u < vm->instructionCount)
        {
          ci = inst + r0.i;
          opStack--;
        }
        else
        {
          Com_Error(ERR_DROP, "VM program counter out of range in OP_CALL");
        }

        break;

      //push and pop are only needed for discarded or bad function return values
      case
      OP_PUSH:
        opStack++;
        break;

      case
      OP_POP:
        opStack--;
        break;

      case
      OP_CONST:
        opStack++;
        r1.i = r0.i;
        r0.i = *opStack = v0;
        goto nextInstruction2;

      case
      OP_LOCAL:
        opStack++;
        r1.i = r0.i;
        r0.i = *opStack = v0 + programStack;
        goto nextInstruction2;

      case
      OP_JUMP:
        if (r0.u >= vm->instructionCount)
        {
          Com_Error(ERR_DROP, "VM program counter out of range in OP_JUMP");
        }

        ci = inst + r0.i;
        opStack--;
        break;

      /*
      ===================================================================
      BRANCHES
      ===================================================================
      */

      case
      OP_EQ:
        opStack -= 2;

        if (r1.i == r0.i)
        {
          ci = inst + v0;
        }

        break;

      case
      OP_NE:
        opStack -= 2;

        if (r1.i != r0.i)
        {
          ci = inst + v0;
        }

        break;

      case
      OP_LTI:
        opStack -= 2;

        if (r1.i < r0.i)
        {
          ci = inst + v0;
        }

        break;

      case
      OP_LEI:
        opStack -= 2;

        if (r1.i <= r0.i)
        {
          ci = inst + v0;
        }

        break;

      case
      OP_GTI:
        opStack -= 2;

        if (r1.i > r0.i)
        {
          ci = inst + v0;
        }

        break;

      case
      OP_GEI:
        opStack -= 2;

        if (r1.i >= r0.i)
        {
          ci = inst + v0;
        }

        break;

      case
      OP_LTU:
        opStack -= 2;

        if (r1.u < r0.u)
        {
          ci = inst + v0;
        }

        break;

      case
      OP_LEU:
        opStack -= 2;

        if (r1.u <= r0.u)
        {
          ci = inst + v0;
        }

        break;

      case
      OP_GTU:
        opStack -= 2;

        if (r1.u > r0.u)
        {
          ci = inst + v0;
        }

        break;

      case
      OP_GEU:
        opStack -= 2;

        if (r1.u >= r0.u)
        {
          ci = inst + v0;
        }

        break;

      case
      OP_EQF:
        opStack -= 2;

        if (r1.f == r0.f)
        {
          ci = inst + v0;
        }

        break;

      case
      OP_NEF:
        opStack -= 2;

        if (r1.f != r0.f)
        {
          ci = inst + v0;
        }

        break;

      case
      OP_LTF:
        opStack -= 2;

        if (r1.f < r0.f)
        {
          ci = inst + v0;
        }

        break;

      case
      OP_LEF:
        opStack -= 2;

        if (r1.f <= r0.f)
        {
          ci = inst + v0;
        }

        break;

      case
      OP_GEF:
        opStack -= 2;

        if (r1.f >= r0.f)
        {
          ci = inst + v0;
        }

        break;

      //===================================================================

      case
      OP_LOAD1:
        r0.i = *opStack = image[r0.i & dataMask];
        goto nextInstruction2;

      case
      OP_LOAD2:
        r0.i = *opStack = *(unsigned short *)&image[r0.i & (dataMask & ~1)];
        goto nextInstruction2;

      case
      OP_LOAD4:
        r0.i = *opStack = *(qint *)&image[r0.i & (dataMask & ~3)];
        goto nextInstruction2;

      case
      OP_STORE1:
        image[r1.i & dataMask] = r0.i;
        opStack -= 2;
        break;

      case
      OP_STORE2:
        *(short *)&image[r1.i & (dataMask & ~1)] = r0.i;
        opStack -= 2;
        break;

      case
      OP_STORE4:
        *(qint *)&image[r1.i & (dataMask & ~3)] = r0.i;
        opStack -= 2;
        break;

      case
      OP_ARG:
        //single byte offset from programStack
        *(qint *)&image[(v0 + programStack) /*& (dataMask & ~3)*/] = r0.i;
        opStack--;
        break;

      case
      OP_BLOCK_COPY:
        qint *src;
        qint *dest;
        qint count;
        qint srci;
        qint desti;

        count = v0;

        //MrE: copy range check
        srci = r0.i & dataMask;
        desti = r1.i & dataMask;
        count = ((srci + count) & dataMask) - srci;
        count = ((desti + count) & dataMask) - desti;

        src = (qint *)&image[srci];
        dest = (qint *)&image[desti];

        memcpy(dest, src, count);
        opStack -= 2;
        break;

      case
      OP_SEX8:
        *opStack = (signed qchar)*opStack;
        break;

      case
      OP_SEX16:
        *opStack = (short)*opStack;
        break;

      case
      OP_NEGI:
        *opStack = -r0.i;
        break;

      case
      OP_ADD:
        opStack[-1] = r1.i + r0.i;
        opStack--;
        break;

      case
      OP_SUB:
        opStack[-1] = r1.i - r0.i;
        opStack--;
        break;

      case
      OP_DIVI:
        opStack[-1] = r1.i / r0.i;
        opStack--;
        break;

      case
      OP_DIVU:
        opStack[-1] = r1.u / r0.u;
        opStack--;
        break;

      case
      OP_MODI:
        opStack[-1] = r1.i % r0.i;
        opStack--;
        break;

      case
      OP_MODU:
        opStack[-1] = r1.u % r0.u;
        opStack--;
        break;

      case
      OP_MULI:
        opStack[-1] = r1.i * r0.i;
        opStack--;
        break;

      case
      OP_MULU:
        opStack[-1] = r1.u * r0.u;
        opStack--;
        break;

      case
      OP_BAND:
        opStack[-1] = r1.u & r0.u;
        opStack--;
        break;

      case
      OP_BOR:
        opStack[-1] = r1.u | r0.u;
        opStack--;
        break;

      case
      OP_BXOR:
        opStack[-1] = r1.u ^ r0.u;
        opStack--;
        break;

      case
      OP_BCOM:
        *opStack = ~r0.u;
        break;

      case
      OP_LSH:
        opStack[-1] = r1.i << r0.i;
        opStack--;
        break;

      case
      OP_RSHI:
        opStack[-1] = r1.i >> r0.i;
        opStack--;
        break;

      case
      OP_RSHU:
        opStack[-1] = r1.u >> r0.i;
        opStack--;
        break;

      case
      OP_NEGF:
        *(float *)opStack = -r0.f;
        break;

      case
      OP_ADDF:
        *(float *)(opStack - 1) = r1.f + r0.f;
        opStack--;
        break;

      case
      OP_SUBF:
        *(float *)(opStack - 1) = r1.f - r0.f;
        opStack--;
        break;

      case
      OP_DIVF:
        *(float *)(opStack - 1) = r1.f / r0.f;
        opStack--;
        break;

      case
      OP_MULF:
        *(float *)(opStack - 1) = r1.f * r0.f;
        opStack--;
        break;

      case
      OP_CVIF:
        *(float *)opStack = (float)r0.i;
        break;

      case
      OP_CVFI:
        *opStack = (qint)r0.f;
        break;
    }
  }

done:
  vm->currentlyInterpreting = qfalse;

  if (opStack != &stack[1])
  {
    Com_Error(ERR_DROP, "Interpreter error: opStack = %ld", (long)(opStack - stack));
  }

  vm->programStack = stackOnEntry;

  //return the result
  return *opStack;
}
