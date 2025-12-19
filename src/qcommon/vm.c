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
// vm.c -- virtual machine

/*


intermix code and data
symbol table

a dll has one imported function: VM_SystemCall
and one exported function: Perform


*/

#include "vm_local.h"

opcode_info_t ops[OP_MAX] = 
{
  {0, 0, 0, 0}, //undef
  {0, 0, 0, 0}, //ignore
  {0, 0, 0, 0}, //break

  {4, 0, 0, 0}, //enter
  {4,-4, 0, 0}, //leave
  {0, 0, 1, 0}, //call
  {0, 4, 0, 0}, //push
  {0,-4, 1, 0}, //pop

  {4, 4, 0, 0}, //const
  {4, 4, 0, 0}, //local
  {0,-4, 1, 0}, //jump

  {4,-8, 2, JUMP}, //eq
  {4,-8, 2, JUMP}, //ne

  {4,-8, 2, JUMP}, //lti
  {4,-8, 2, JUMP}, //lei
  {4,-8, 2, JUMP}, //gti
  {4,-8, 2, JUMP}, //gei

  {4,-8, 2, JUMP}, //ltu
  {4,-8, 2, JUMP}, //leu
  {4,-8, 2, JUMP}, //gtu
  {4,-8, 2, JUMP}, //geu

  {4,-8, 2, JUMP}, //eqf
  {4,-8, 2, JUMP}, //nef

  {4,-8, 2, JUMP}, //ltf
  {4,-8, 2, JUMP}, //lef
  {4,-8, 2, JUMP}, //gtf
  {4,-8, 2, JUMP}, //gef

  {0, 0, 1, 0}, //load1
  {0, 0, 1, 0}, //load2
  {0, 0, 1, 0}, //load4
  {0,-8, 2, 0}, //store1
  {0,-8, 2, 0}, //store2
  {0,-8, 2, 0}, //store4
  {1,-4, 1, 0}, //arg
  {4,-8, 2, 0}, //bcopy

  {0, 0, 1, 0}, //sex8
  {0, 0, 1, 0}, //sex16

  {0, 0, 1, 0}, //negi
  {0,-4, 3, 0}, //add
  {0,-4, 3, 0}, //sub
  {0,-4, 3, 0}, //divi
  {0,-4, 3, 0}, //divu
  {0,-4, 3, 0}, //modi
  {0,-4, 3, 0}, //modu
  {0,-4, 3, 0}, //muli
  {0,-4, 3, 0}, //mulu

  {0,-4, 3, 0}, //band
  {0,-4, 3, 0}, //bor
  {0,-4, 3, 0}, //bxor
  {0, 0, 1, 0}, //bcom

  {0,-4, 3, 0}, //lsh
  {0,-4, 3, 0}, //rshi
  {0,-4, 3, 0}, //rshu

  {0, 0, 1, 0}, //negf
  {0,-4, 3, 0}, //addf
  {0,-4, 3, 0}, //subf
  {0,-4, 3, 0}, //divf
  {0,-4, 3, 0}, //mulf

  {0, 0, 1, 0}, //cvif
  {0, 0, 1, 0} //cvfi
};

vm_t *currentVM = NULL;
vm_t *lastVM = NULL;
qint		vm_debugLevel;

// used by Com_Error to get rid of running vm's before longjmp
static qint forced_unload;

struct vm_s vmTable[VM_COUNT];

static const qchar *vmName[VM_COUNT] =
{
  "game",
  "cgame",
  "ui"
};


void VM_VmInfo_f( void );
void VM_VmProfile_f( void );



#if 0 // 64bit!
// converts a VM pointer to a C pointer and
// checks to make sure that the range is acceptable
void	*VM_VM2C( vmptr_t p, qint length ) {
	return (void *)p;
}
#endif

void VM_Debug( qint level ) {
	vm_debugLevel = level;
}

/*
==============
VM_Init
==============
*/
void VM_Init( void ) {
	Cvar_Get( "vm_cgame", "2", CVAR_ROM );	// !@# SHIP WITH SET TO 2
	Cvar_Get( "vm_game", "2", CVAR_ROM );	// !@# SHIP WITH SET TO 2
	Cvar_Get( "vm_ui", "2", CVAR_ROM );		// !@# SHIP WITH SET TO 2

	Cmd_AddCommand ("vmprofile", VM_VmProfile_f );
	Cmd_AddCommand ("vminfo", VM_VmInfo_f );

	Com_Memset( vmTable, 0, sizeof( vmTable ) );
}


/*
===============
VM_ValueToSymbol

Assumes a program counter value
===============
*/
const qchar *VM_ValueToSymbol( vm_t *vm, qint value ) {
	vmSymbol_t	*sym;
	static qchar		text[MAX_TOKEN_CHARS];

	sym = vm->symbols;
	if ( !sym ) {
		return "NO SYMBOLS";
	}

	// find the symbol
	while ( sym->next && sym->next->symValue <= value ) {
		sym = sym->next;
	}

	if ( value == sym->symValue ) {
		return sym->symName;
	}

	Com_sprintf( text, sizeof( text ), "%s+%i", sym->symName, value - sym->symValue );

	return text;
}

/*
===============
VM_ValueToFunctionSymbol

For profiling, find the symbol behind this value
===============
*/
vmSymbol_t *VM_ValueToFunctionSymbol( vm_t *vm, qint value ) {
	vmSymbol_t	*sym;
	static vmSymbol_t	nullSym;

	sym = vm->symbols;
	if ( !sym ) {
		return &nullSym;
	}

	while ( sym->next && sym->next->symValue <= value ) {
		sym = sym->next;
	}

	return sym;
}


/*
===============
VM_SymbolToValue
===============
*/
qint VM_SymbolToValue( vm_t *vm, const qchar *symbol ) {
	vmSymbol_t	*sym;

	for ( sym = vm->symbols ; sym ; sym = sym->next ) {
		if ( !strcmp( symbol, sym->symName ) ) {
			return sym->symValue;
		}
	}
	return 0;
}


/*
=====================
VM_SymbolForCompiledPointer
=====================
*/
#if 0 // 64bit!
const qchar *VM_SymbolForCompiledPointer( vm_t *vm, void *code ) {
	qint			i;

	if ( code < (void *)vm->codeBase.ptr ) {
		return "Before code block";
	}
	if ( code >= (void *)(vm->codeBase.ptr + vm->codeLength) ) {
		return "After code block";
	}

	// find which original instruction it is after
	for ( i = 0 ; i < vm->codeLength ; i++ ) {
		if ( (void *)vm->instructionPointers[i] > code ) {
			break;
		}
	}
	i--;

	// now look up the bytecode instruction pointer
	return VM_ValueToSymbol( vm, i );
}
#endif



/*
===============
ParseHex
===============
*/
qint	ParseHex( const qchar *text ) {
	qint		value;
	qint		c;

	value = 0;
	while ( ( c = *text++ ) != 0 ) {
		if ( c >= '0' && c <= '9' ) {
			value = value * 16 + c - '0';
			continue;
		}
		if ( c >= 'a' && c <= 'f' ) {
			value = value * 16 + 10 + c - 'a';
			continue;
		}
		if ( c >= 'A' && c <= 'F' ) {
			value = value * 16 + 10 + c - 'A';
			continue;
		}
	}

	return value;
}

/*
===============
VM_LoadSymbols
===============
*/
void VM_LoadSymbols( vm_t *vm ) {
        union
        {
          qchar *c;
          void *v;
        }
        mapfile;
        const qchar *text_p;
        const qchar *token;
	qchar	name[MAX_QPATH];
	qchar	symbols[MAX_QPATH];
	vmSymbol_t	**prev, *sym;
	qint		count;
	qint		value;
	qint		chars;
	qint		segment;
	qint		numInstructions;

	// don't load symbols if not developer
	if ( !com_developer->integer ) {
		return;
	}

	COM_StripExtension(vm->name, name, sizeof(name));
	Com_sprintf( symbols, sizeof( symbols ), "vm/%s.map", name );
	FS_ReadFile( symbols, &mapfile.v );
	if ( !mapfile.c ) {
		Com_Printf( "Couldn't load symbol file: %s\n", symbols );
		return;
	}

	numInstructions = vm->instructionCount;

	// parse the symbols
	text_p = mapfile.c;
	prev = &vm->symbols;
	count = 0;

	while ( 1 ) {
		token = COM_Parse( &text_p );
		if ( !token[0] ) {
			break;
		}
		segment = ParseHex( token );
		if ( segment ) {
			COM_Parse( &text_p );
			COM_Parse( &text_p );
			continue;		// only load code segment values
		}

		token = COM_Parse( &text_p );
		if ( !token[0] ) {
			Com_Printf( "WARNING: incomplete line at end of file\n" );
			break;
		}
		value = ParseHex( token );

		token = COM_Parse( &text_p );
		if ( !token[0] ) {
			Com_Printf( "WARNING: incomplete line at end of file\n" );
			break;
		}
		chars = strlen( token );
		sym = Hunk_Alloc( sizeof( *sym ) + chars, h_high );
		*prev = sym;
		prev = &sym->next;
		sym->next = NULL;

		// convert value from an instruction number to a code offset
		if ( value >= 0 && value < numInstructions ) {
			value = vm->instructionPointers[value];
		}

		sym->symValue = value;
		Q_strncpyz( sym->symName, token, chars + 1 );

		count++;
	}

	vm->numSymbols = count;
	Com_Printf( "%i symbols parsed from %s\n", count, symbols );
	FS_FreeFile( mapfile.v );
}

/*
============
VM_DllSyscall

Dlls will call this directly

 rcg010206 The horror; the horror.

  The syscall mechanism relies on stack manipulation to get its args.
   This is likely due to C's inability to pass "..." parameters to
   a function in one clean chunk. On PowerPC Linux, these parameters
   are not necessarily passed on the stack, so while (&arg[0] == arg)
   is true, (&arg[1] == 2nd function parameter) is not necessarily
   accurate, as arg's value might have been stored to the stack or
   other piece of scratch memory to give it a valid address, but the
   next parameter might still be sitting in a register.

  Quake's syscall system also assumes that the stack grows downward,
   and that any needed types can be squeezed, safely, into a signed qint.

  This hack below copies all needed values for an argument to a
   array in memory, so that Quake can get the correct values. This can
   also be used on systems where the stack grows upwards, as the
   presumably standard and safe stdargs.h macros are used.

  As for having enough space in a signed qint for your datatypes, well,
   it might be better to wait for DOOM 3 before you start porting.  :)

  The original code, while probably still inherently dangerous, seems
   to work well enough for the platforms it already works on. Rather
   than add the performance hit for those platforms, the original code
   is still in use there.

  For speed, we just grab 15 arguments, and don't worry about exactly
   how many the syscall actually needs; the extra is thrown away.
 
============
*/
#if 0 //- disabled because now is different for each module
intptr_t QDECL __attribute__((no_sanitize_address))
VM_DllSyscall(intptr_t arg, ...)
{
#if !id386
  // rcg010206 - see commentary above
  intptr_t args[VM_SYSCALL_ARGS];
  qint i;
  va_list ap;
  
  args[0] = arg;
  
  va_start(ap, arg);
  for (i = 1;i < ARRAY_LEN(args); i++)
  {
    args[i] = va_arg(ap, intptr_t);
  }

  va_end(ap);
  
  return currentVM->systemCall(args);
#else // original id code
  return currentVM->systemCall(&arg);
#endif
}
#endif

/*
==================
crc32 routines
==================
*/
static unsigned qint crc32_table[256];
static qbool crc32_inited = qfalse;

static void
crc32_init(unsigned *crc)
{
  unsigned c;
  qint i;
  qint j;

  if (!crc32_inited)
  {
    for(i = 0;i < 256;i++)
    {
      c = i;

      for(j = 0;j < 8;j++)
      {
        c = c & 1 ? (c >> 1) ^ 0xEDB88320UL:c >> 1;
      }

      crc32_table[i] = c;
    }

    crc32_inited = qtrue;
  }

  *crc = 0xFFFFFFFFUL;
}

static void
crc32_update(unsigned *crc, unsigned qchar *buf, unsigned len)
{
  while(len--)
  {
    *crc = crc32_table[(*crc ^ *buf++) & 0xFF] ^ (*crc >> 8);
  }
}

static void
crc32_final(unsigned *crc)
{
  *crc = *crc ^ 0xFFFFFFFFUL;
}

static qint
Load_JTS(vm_t *vm, unsigned crc32, void *data, qint vmPakIndex)
{
  qchar filename[MAX_QPATH];
  qint header[2];
  qint length;
  qint i;
  fileHandle_t fh;

  //load the image
  Com_sprintf(filename, sizeof(filename), "vm/%s.jts", vm->name);

  if (data)
  {
    Com_Printf("Loading jts file %s...\n", filename);
  }

  length = FS_FOpenFileRead(filename, &fh, qtrue);

  if (fh == FS_INVALID_HANDLE)
  {
    if (data)
    {
      Com_Printf(" not found.\n");
    }

    return -1;
  }

  if (fs_lastPakIndex != vmPakIndex)
  {
    Com_Printf(" bad pak index %i (expecting %i) for %s.\n", fs_lastPakIndex, vmPakIndex, filename);
    FS_FCloseFile(fh);
    return -1;
  }

  if (length < sizeof(header))
  {
    if (data)
    {
      Com_Printf(" bad filesize %i for %s.\n", length, filename);
    }

    FS_FCloseFile(fh);
    return -1;
  }

  if (FS_Read(header, sizeof(header), fh) != sizeof(header))
  {
    if (data)
    {
      Com_Printf(" error reading header of %s.\n", filename);
    }

    FS_FCloseFile(fh);
    return -1;
  }

  //byte swap the header
  for(i = 0;i < sizeof(header) / sizeof(qint);i++)
  {
    ((qint *)header)[i] = LittleLong(((qint *)header)[i]);
  }

  if ((unsigned)header[0] != crc32)
  {
    if (data)
    {
      Com_Printf(" crc32 mismatch: %08X <-> %08X.\n", header[0], crc32);
    }

    FS_FCloseFile(fh);
    return -1;
  }

  if (header[1] < 0 || header[1] != (length - (qint)sizeof(header)))
  {
    if (data)
    {
      Com_Printf(" bad file header.\n");
    }

    FS_FCloseFile(fh);
    return -1;
  }

  length -= sizeof(header); //skip header and filesize

  //we need just filesize
  if (!data)
  {
    FS_FCloseFile(fh);
    return length;
  }

  FS_Read(data, length, fh);
  FS_FCloseFile(fh);

  //byte swap the data
  for(i = 0;i < length / sizeof(qint);i++)
  {
    ((qint *)data)[i] = LittleLong(((qint *)data)[i]);
  }

  return length;
}

/*
=================
VM_ValidateHeader
=================
*/
static qchar *
VM_ValidateHeader(vmHeader_t *header, qint fileSize)
{
  static qchar errMsg[128];
  qint i;
  qint n;

  //truncated
  if (fileSize < (sizeof(vmHeader_t) - sizeof(qint)))
  {
    sprintf(errMsg, "truncated image header (%i bytes long)", fileSize);
    return errMsg;
  }

  //bad magic
  if (LittleLong(header->vmMagic) != VM_MAGIC && LittleLong(header->vmMagic) != VM_MAGIC_VER2)
  {
    sprintf(errMsg, "bad file magic %08x", LittleLong(header->vmMagic));
    return errMsg;
  }

  //truncated
  if (fileSize < sizeof(vmHeader_t) && LittleLong(header->vmMagic) != VM_MAGIC_VER2)
  {
    sprintf(errMsg, "truncated image header (%i bytes long)", fileSize);
    return errMsg;
  }

  if (LittleLong(header->vmMagic) == VM_MAGIC_VER2)
  {
    n = sizeof(vmHeader_t) / sizeof(qint);
  }
  else
  {
    n = (sizeof(vmHeader_t) - sizeof(qint)) / sizeof(qint);
  }

  //byte swap the header
  for(i = 0;i < n;i++)
  {
    ((qint *)header)[i] = LittleLong(((qint *)header)[i]);
  }

  //bad code offset
  if (header->codeOffset >= fileSize)
  {
    sprintf(errMsg, "bad code segment offset %i", header->codeOffset);
    return errMsg;
  }

  //bad code length
  if (header->codeLength <= 0 || header->codeOffset + header->codeLength > fileSize)
  {
    sprintf(errMsg, "bad code segment length %i", header->codeLength);
    return errMsg;
  }

  //bad data offset
  if (header->dataOffset >= fileSize || header->dataOffset != header->codeOffset + header->codeLength)
  {
    sprintf(errMsg, "bad data segment offset %i", header->dataOffset);
    return errMsg;
  }

  //bad data length
  if (header->dataOffset + header->dataLength > fileSize)
  {
    sprintf(errMsg, "bad data segment length %i", header->dataLength);
    return errMsg;
  }

  if (header->vmMagic == VM_MAGIC_VER2)
  {
    //bad lit/jtrg length
    if (header->dataOffset + header->dataLength + header->litLength + header->jtrgLength != fileSize)
    {
      sprintf(errMsg, "bad lit/jtrg segment length");
      return errMsg;
    }
  }
  //bad lit length
  else if (header->dataOffset + header->dataLength + header->litLength != fileSize)
  {
    sprintf(errMsg, "bad lit segment length %i", header->litLength);
    return errMsg;
  }

  return NULL;
}

/*
=================
VM_LoadQVM

Load a .qvm file

if (alloc)
{
  - Validate header, swap data
  - Alloc memory for data/instructions
  - Alloc memory for instructionPointers
  - Load instructions
  - Clear/load data
}
else
{
  - Check for header changes
  - Clear/load data
}
=================
*/
vmHeader_t *VM_LoadQVM( vm_t *vm, qbool alloc ) {
        qint length;
        qint previousNumJumpTableTargets;
	qint					dataLength;
	qint dataAlloc;
	qint					i;
	qchar				filename[MAX_QPATH];
	qchar *errorMsg;
	unsigned crc32sum;
	qbool tryjts;
	qint vmPakIndex;
	union
	{
	  vmHeader_t *h;
	  void *v;
	}
	header;

	// load the image
	Com_sprintf( filename, sizeof(filename), "vm/%s.qvm", vm->name );
	Com_Printf( "Loading vm file %s...\n", filename );
	length = FS_ReadFile( filename, &header.v );
	if ( !header.h ) {
		Com_Printf( "Failed.\n" );
		VM_Free( vm );
		return NULL;
	}

        vmPakIndex = fs_lastPakIndex;

        crc32_init(&crc32sum);
        crc32_update(&crc32sum, header.v, length);
        crc32_final(&crc32sum);

        //will also swap header
        errorMsg = VM_ValidateHeader(header.h, length);

        if (errorMsg)
        {
          VM_Free(vm);
          FS_FreeFile(header.h);
          Com_Error(ERR_FATAL, "%s", errorMsg);
          return NULL;
        }

        vm->crc32sum = crc32sum;
        tryjts = qfalse;

        //show where the qvm has landed from
        Cmd_ExecuteString(va(NULL, "which %s\n", filename));

	if (header.h->vmMagic == VM_MAGIC_VER2)
	{
          Com_Printf("...which has vmMagic VM_MAGIC_VER2\n");
	}
	else
	{
          tryjts = qtrue;
	}

        dataLength = header.h->dataLength + header.h->litLength + header.h->bssLength;
        vm->dataLength = dataLength;

	// round up to next power of 2 so all data operations can
	// be mask protected
	for ( i = 0 ; dataLength > ( BIT(i) ) ; i++ );

	dataLength = BIT(i);

        //reserve some space for effective LOCAL+LOAD* checks
        dataAlloc = dataLength + 256;

	if( alloc ) {
		// allocate zero filled space for initialized and uninitialized data
		//leave some space beyond data mask so we can secure all mask operations
		//vm->dataAlloc = dataLength + 4;
		vm->dataBase = Hunk_Alloc(dataAlloc, h_high);
		vm->dataMask = dataLength - 1;
		vm->dataAlloc = dataAlloc;
	} else {
		// clear the data, but make sure we're not clearing more than allocated
		//if (vm->dataAlloc != dataLength + 4)
		if (vm->dataAlloc != dataAlloc)
		{
		  VM_Free(vm);
		  FS_FreeFile(header.v);
		  Com_Printf(S_COLOR_YELLOW "WARNING: Data region size of %s not matching after VM_Restart()\n", filename);
		  return NULL;
		}

                Com_Memset(vm->dataBase, 0, vm->dataAlloc);
	}

	// copy the intialized data
	Com_Memcpy( vm->dataBase, (byte *)header.h + header.h->dataOffset, header.h->dataLength + header.h->litLength );

	// byte swap the longs
	for ( i = 0 ; i < header.h->dataLength ; i += 4 ) {
		*(qint *)(vm->dataBase + i) = LittleLong( *(qint *)(vm->dataBase + i ) );
	}

	if( header.h->vmMagic == VM_MAGIC_VER2 ) {

	        previousNumJumpTableTargets = vm->numJumpTableTargets;
	        header.h->jtrgLength &= ~0x03;

		vm->numJumpTableTargets = header.h->jtrgLength >> 2;
		Com_Printf( "Loading %d jump table targets\n", vm->numJumpTableTargets );

		if (alloc)
		{
                  vm->jumpTableTargets = Hunk_Alloc(header.h->jtrgLength, h_high);
		}
		else
		{
		  if (vm->numJumpTableTargets != previousNumJumpTableTargets)
		  {
		    VM_Free(vm);
		    FS_FreeFile(header.h);

                    Com_Printf(S_COLOR_YELLOW "Warning: Jump table size of %s not matching after VM_Restart()\n", filename);
                    return NULL;
                  }

                  Com_Memset(vm->jumpTableTargets, 0, header.h->jtrgLength);
		}

		Com_Memcpy( vm->jumpTableTargets, (byte *)header.h + header.h->dataOffset +
				header.h->dataLength + header.h->litLength, header.h->jtrgLength );

		// byte swap the longs
		for ( i = 0 ; i < header.h->jtrgLength ; i += 4 ) {
			*(qint *)(vm->jumpTableTargets + i) = LittleLong( *(qint *)(vm->jumpTableTargets + i ) );
		}
	}

        if (tryjts == qtrue && (length = Load_JTS(vm, crc32sum, NULL, vmPakIndex)) >= 0)
        {
          //we are trying to load newer file?
          if (vm->jumpTableTargets && vm->numJumpTableTargets != length >> 2)
          {
            Com_Printf(S_COLOR_YELLOW "Reload jts file\n");
            vm->jumpTableTargets = NULL;
            alloc = qtrue;
          }

          vm->numJumpTableTargets = length >> 2;

          Com_Printf("Loading %d external jump table targets\n", vm->numJumpTableTargets);

          if (alloc == qtrue)
          {
            vm->jumpTableTargets = Hunk_Alloc(length, h_high);
          }
          else
          {
            Com_Memset(vm->jumpTableTargets, 0, length);
          }

          Load_JTS(vm, crc32sum, vm->jumpTableTargets, vmPakIndex);
        }

	return header.h;
}

/*
=================
VM_LoadInstructions

loads instructions in structured format
=================
*/
const qchar *
VM_LoadInstructions(const vmHeader_t *header, instruction_t *buf)
{
  static qchar errBuf[128];
  byte *code_pos;
  byte *code_start;
  byte *code_end;
  qint i;
  qint n;
  qint op0;
  qint op1;
  qint opStack;
  instruction_t *ci;

  code_pos = (byte *)header + header->codeOffset;
  code_start = code_pos; //for printing
  code_end = (byte *)header + header->codeOffset + header->codeLength;

  ci = buf;
  opStack = 0;
  op1 = OP_UNDEF;

  //load instructions and perform some initial calculations/checks
  for(i = 0;i < header->instructionCount;i++, ci++, op1 = op0)
  {
    op0 = *code_pos;

    if (op0 < 0 || op0 >= OP_MAX)
    {
      sprintf(errBuf, "bad opcode %02X at offset %ld", op0, code_pos - code_start);
      return errBuf;
    }

    n = ops[op0].size;

    if (code_pos + 1 + n > code_end)
    {
      sprintf(errBuf, "code_pos > code_end");
      return errBuf;
    }

    code_pos++;
    ci->op = op0;

    if (n == 4)
    {
      ci->value = LittleLong(*((qint *)code_pos));
      code_pos += 4;
    }
    else if (n == 1)
    {
      ci->value = *((unsigned qchar *)code_pos);
      code_pos += 1;
    }
    else
    {
      ci->value = 0;
    }

    //setup jump value from previous const
    if (op0 == OP_JUMP && op1 == OP_CONST)
    {
      ci->value = (ci - 1)->value;
    }

    ci->opStack = opStack;
    opStack += ops[op0].stack;
  }

  return NULL;
}

/*
===============================
VM_CheckInstructions

performs additional consistency and security checks
===============================
*/
const qchar *
VM_CheckInstructions(instruction_t *buf, qint instructionCount, const byte *jumpTableTargets, qint numJumpTableTargets, qint dataLength)
{
  static qchar errBuf[128];
  qint i;
  qint n;
  qint v;
  qint op0;
  qint op1;
  qint opStack;
  qint pstack;
  instruction_t *ci;
  instruction_t *proc;
  qint startp;
  qint endp;

  ci = buf;
  opStack = 0;

  //opstack checks
  for(i = 0;i < instructionCount;i++, ci++)
  {
    opStack += ops[ci->op].stack;

    if (opStack < 0)
    {
      sprintf(errBuf, "opStack underflow at %i", i);
      return errBuf;
    }

    if (opStack >= PROC_OPSTACK_SIZE * 4)
    {
      sprintf(errBuf, "opStack overflow at %i", i);
      return errBuf;
    }
  }

  ci = buf;
  pstack = 0;
  op1 = OP_UNDEF;
  proc = NULL;

  startp = 0;
  endp = instructionCount - 1;

  //additional security checks
  for(i = 0;i < instructionCount;i++, ci++, op1 = op0)
  {
    op0 = ci->op;

    //function entry
    if (op0 == OP_ENTER)
    {
      //missing block end
      if (proc || (pstack && op1 != OP_LEAVE))
      {
        sprintf(errBuf, "missing proc end before %i", i);
        return errBuf;
      }

      if (ci->opStack != 0)
      {
        v = ci->opStack;
        sprintf(errBuf, "bad entry opstack %i at %i", v, i);
        return errBuf;
      }

      v = ci->value;

      if (v < 0 || v >= PROGRAM_STACK_SIZE || (v & 3))
      {
        sprintf(errBuf, "bad entry programStack %i at %i", v, i);
        return errBuf;
      }

      pstack = ci->value;

      //mark jump target
      ci->jused = 1;
      proc = ci;
      startp = i + 1;

      //locate endproc
      for(endp = 0, n = i + 1;n < instructionCount;n++)
      {
        if (buf[n].op == OP_PUSH && buf[n + 1].op == OP_LEAVE)
        {
          endp = n;
          break;
        }
      }

      if (endp == 0)
      {
        sprintf(errBuf, "missing end proc for %i", i);
        return errBuf;
      }

      continue;
    }

    //proc opstack will carry max.possible opstack value
    if (proc && ci->opStack > proc->opStack)
    {
      proc->opStack = ci->opStack;
    }

    //function return
    if (op0 == OP_LEAVE)
    {
      //bad return programStack
      if (pstack != ci->value)
      {
        v = ci->value;
        sprintf(errBuf, "bad programStack %i at %i", v, i);
        return errBuf;
      }

      //bad opStack before return
      if (ci->opStack != 4)
      {
        v = ci->opStack;
        sprintf(errBuf, "bad opStack %i at %i", v, i);
        return errBuf;
      }

      v = ci->value;

      if (v < 0 || v >= PROGRAM_STACK_SIZE || (v & 3))
      {
        sprintf(errBuf, "bad return programStack %i at %i", v, i);
        return errBuf;
      }

      if (op1 == OP_PUSH)
      {
        if (proc == NULL)
        {
          sprintf(errBuf, "unexpected proc end at %i", i);
          return errBuf;
        }

        proc = NULL;
        startp = i + 1; //next instruction
        endp = instructionCount - 1; //end of the image
      }

      continue;
    }

    //conditional jumps
    if (ops[ci->op].flags & JUMP)
    {
      v = ci->value;

      //conditional jumps should have opStack == 8
      if (ci->opStack != 8)
      {
        sprintf(errBuf, "bad jump opStack %i at %i", ci->opStack, i);
        return errBuf;
      }

      //if (v >= header->instructionCount)
      //allow only local proc jumps
      if (v < startp || v > endp)
      {
        sprintf(errBuf, "jump target %i at %i is out of range (%i, %i)", v, i - 1, startp, endp);
        return errBuf;
      }

      if (buf[v].opStack != 0)
      {
        n = buf[v].opStack;
        sprintf(errBuf, "jump target %i has bad opStack %i", v, n);
        return errBuf;
      }

      //mark jump target
      buf[v].jused = 1;
      continue;
    }

    //unconditional jumps
    if (op0 == OP_JUMP)
    {
      //jumps should have opStack == 4
      if (ci->opStack != 4)
      {
        sprintf(errBuf, "bad jump opStack %i at %i", ci->opStack, i);
        return errBuf;
      }

      if (op1 == OP_CONST)
      {
        v = buf[i - 1].value;

        //allow only local jumps
        if (v < startp || v > endp)
        {
          sprintf(errBuf, "jump target %i at %i is out of range (%i, %i)", v, i - 1, startp, endp);
          return errBuf;
        }

        if (buf[v].opStack != 0)
        {
          n = buf[v].opStack;
          sprintf(errBuf, "jump target %i has bad opStack %i", v, n);
          return errBuf;
        }

        if (buf[v].op == OP_ENTER)
        {
          n = buf[v].op;
          sprintf(errBuf, "jump target %i has bad opcode %i", v, n);
          return errBuf;
        }

        //mark jump target
        buf[v].jused = 1;
      }
      else
      {
        if (proc)
        {
          proc->swtch = 1;
        }
        else
        {
          ci->swtch = 1;
        }
      }

      continue;
    }

    if (op0 == OP_CALL)
    {
      if (ci->opStack < 4)
      {
        sprintf(errBuf, "bad call opStack at %i", i);
        return errBuf;
      }

      if (op1 == OP_CONST)
      {
        v = buf[i - 1].value;

        //analyze only local function calls
        if (v >= 0)
        {
          if (v >= instructionCount)
          {
            sprintf(errBuf, "call target %i is out of range", v);
            return errBuf;
          }

          if (buf[v].op != OP_ENTER)
          {
            n = buf[v].op;
            sprintf(errBuf, "call target %i has bad opcode %i", v, n);
            return errBuf;
          }

          if (v == 0)
          {
            sprintf(errBuf, "explicit vmMain call inside VM");
            return errBuf;
          }

          //mark jump target
          buf[v].jused = 1;
        }
      }

      continue;
    }

    if (ci->op == OP_ARG)
    {
      v = ci->value & 255;

      //argument can't exceed programStack frame
      if (v < 8 || v > pstack - 4 || (v & 3))
      {
        sprintf(errBuf, "bad argument address %i at %i", v, i);
        return errBuf;
      }

      continue;
    }

    if (ci->op == OP_LOCAL)
    {
      v = ci->value;

      if (proc == NULL)
      {
        sprintf(errBuf, "missing proc frame for local %i at %i", v, i);
        return errBuf;
      }

      if ((ci + 1)->op == OP_LOAD1 || (ci + 1)->op == OP_LOAD2 || (ci + 1)->op == OP_LOAD4)
      {
        //FIXME: alloc 256 bytes of programStack in VM_CallCompiled()?
        if (v < 8 || v >= proc->value + 256)
        {
          sprintf(errBuf, "bad local address %i at %i", v, i);
          return errBuf;
        }
      }
    }

    if (ci->op == OP_LOAD4 && op1 == OP_CONST)
    {
      v = (ci - 1)->value;

      if (v < 0 || v > dataLength - 4)
      {
        sprintf(errBuf, "bad load4 address %i at %i", v, i - 1);
        return errBuf;
      }
    }

    if (ci->op == OP_LOAD2 && op1 == OP_CONST)
    {
      v = (ci - 1)->value;

      if (v < 0 || v > dataLength - 2)
      {
        sprintf(errBuf, "bad load2 address %i at %i", v, i - 1);
        return errBuf;
      }
    }

    if (ci->op == OP_LOAD1 && op1 == OP_CONST)
    {
      v = (ci - 1)->value;

      if (v < 0 || v > dataLength - 1)
      {
        sprintf(errBuf, "bad load1 address %i at %i", v, i - 1);
        return errBuf;
      }
    }

    //op1 = op0;
    //ci++;
  }

  if (op1 != OP_UNDEF && op1 != OP_LEAVE)
  {
    sprintf(errBuf, "missing return instruction at the end of the image");
    return errBuf;
  }

  //ensure that the optimization pass knows about all the jump table targets
  if (jumpTableTargets)
  {
    for(i = 0;i < numJumpTableTargets;i++)
    {
      n = *(qint *)(jumpTableTargets + (i * sizeof(qint)));

      if (n < 0 || n >= instructionCount)
      {
        sprintf(errBuf, "jump target %i at %i is out of range [0..%i]", n, i, instructionCount - 1);
        return errBuf;
      }

      if (buf[n].opStack != 0)
      {
        opStack = buf[n].opStack;
        sprintf(errBuf, "jump target set on instruction %i with bad opStack %i", n, opStack);
        return errBuf;
      }

      buf[n].jused = 1;
    }
  }
  else
  {
    v = 0;

    //instructions with opStack > 0 can't be jump labels so its safe to optimize/merge
    for(i = 0, ci = buf;i < instructionCount;i++, ci++)
    {
      if (ci->op == OP_ENTER)
      {
        v = ci->swtch;
        continue;
      }

      //if there is a switch statement in function -
      //mark all potential jump labels
      if (ci->swtch)
      {
        v = ci->swtch;
      }

      if (ci->opStack > 0)
      {
        ci->jused = 0;
      }
      else if (v)
      {
        ci->jused = 1;
      }
    }
  }

  return NULL;
}

#if 0
/*
=================
VM_FindMOps
=================
*/
void
VM_FindMOps(vmHeader_t *header, instruction_t *buf)
{
  qint i;
  qint v;
  qint op0;
  instruction_t *ci;

  ci = buf;

  //search for known macro-op sequences
  i = 0;

  while(i < header->instructionCount)
  {
    op0 = ci->op;

    if (op0 == OP_LOCAL)
    {
      //OP_LOCAL + OP_LOCAL + OP_LOAD4 + OP_CONST + OP_XXX + OP_STORE4
      if ((ci + 1)->op == OP_LOCAL && ci->value == (ci + 1)->value && (ci + 2)->op == OP_LOAD4 && (ci + 3)->op == OP_CONST && (ci + 4)->op != OP_UNDEF && (ci + 5)->op == OP_STORE4)
      {
        v = (ci + 4)->op;

        if (v == OP_ADD)
        {
          ci->mop = MOP_ADD4;
          ci += 6;
          i += 6;
          continue;
        }

        if (v == OP_SUB)
        {
          ci->mop = MOP_SUB4;
          ci += 6;
          i += 6;
          continue;
        }

        if (v == OP_BAND)
        {
          ci->mop = MOP_BAND4;
          ci += 6;
          i += 6;
          continue;
        }

        if (v == OP_BOR)
        {
          ci->mop = MOP_BOR4;
          ci += 6;
          i += 6;
          continue;
        }
      }

      //skip useless sequences
      if ((ci + 1)->op == OP_LOCAL && (ci + 0)->value == (ci + 1)->value && (ci + 2)->op == OP_LOAD4 && (ci + 3)->op == OP_STORE4)
      {
        ci->mop = MOP_IGNORE4;
        ci += 4;
        i += 4;
        continue;
      }
    }

    if ((ops[ci->op].flags & CALC) && (ops[(ci + 1)->op].flags & CALC) && !(ci + 1)->jused)
    {
      ci->mop = MOP_CALCF4;
      ci += 2;
      i += 2;
      continue;
    }

    ci++;
    i++;
  }
}
#endif

/*
=================
VM_Restart

Reload the data, but leave everything else in place
This allows a server to do a map_restart without changing memory allocation
=================
*/
vm_t *
VM_Restart(vm_t *vm)
{
  vmHeader_t *header;

  //DLL's can't be restarted in place
  if (vm->dllHandle)
  {
    syscall_t systemCall;
    dllSyscall_t dllSyscall;
    const qint *vmMainArgs;
    vmIndex_t index;

    index = vm->index;
    systemCall = vm->systemCall;
    dllSyscall = vm->dllSyscall;
    vmMainArgs = vm->vmMainArgs;

    VM_Free(vm);

    vm = VM_Create(index, systemCall, dllSyscall, vmMainArgs, VMI_NATIVE);
    return vm;
  }

  //load the image
  Com_Printf("VM_Restart()\n");

  if (!(header = VM_LoadQVM(vm, qfalse)))
  {
    Com_Error(ERR_DROP, "VM_Restart failed.");
    return NULL;
  }

  //free the original file
  FS_FreeFile(header);

  return vm;
}

/*
================
VM_Create

If image ends in .qvm it will be interpreted, otherwise
it will attempt to load as a system dll
================
*/
vm_t *
VM_Create(vmIndex_t index, syscall_t systemCalls, dllSyscall_t dllSyscalls, const qint *vmMainArgs, vmInterpret_t interpret)
{
  qint remaining;
  const qchar *name;
  vmHeader_t *header;
  vm_t *vm;

  if (!systemCalls)
  {
    Com_Error(ERR_FATAL, "VM_Create: bad parms");
  }

  if ((unsigned)index >= VM_COUNT)
  {
    Com_Error(ERR_FATAL, "VM_Create: bad vm index %i", index);
  }

  remaining = Hunk_MemoryRemaining();

  vm = &vmTable[index];

  //see if we already have the VM
  if (vm->name)
  {
    if (vm->index != index)
    {
      Com_Error(ERR_FATAL, "VM_Create: bad allocated vm index %i", vm->index);
      return NULL;
    }

    return vm;
  }

  name = vmName[index];

  vm->name = name;
  vm->index = index;
  vm->systemCall = systemCalls;
  vm->dllSyscall = dllSyscalls;
  vm->vmMainArgs = vmMainArgs;

  //never allow dll loading with a demo
  if (interpret == VMI_NATIVE)
  {
    if (Cvar_VariableValue("fs_restrict"))
    {
      interpret = VMI_COMPILED;
    }
  }

  if (interpret == VMI_NATIVE)
  {
    //try to load as a system dll
    Com_Printf("Loading dll file %s.\n", name);
    vm->dllHandle = Sys_LoadDll(name, &vm->entryPoint, dllSyscalls);

    if (vm->dllHandle)
    {
      return vm;
    }

    Com_Printf( "Failed to load dll, looking for qvm.\n" );
    interpret = VMI_COMPILED;
  }

  //load the image
  if ((header = VM_LoadQVM(vm, qtrue)) == NULL)
  {
    return NULL;
  }

  //allocate space for the jump targets, which will be filled in by the compile/prep functions
  vm->instructionCount = header->instructionCount;
  vm->instructionPointers = Hunk_Alloc(vm->instructionCount * sizeof(intptr_t), h_high);

  //copy or compile the instructions
  vm->codeLength = header->codeLength;

  //the stack is implicitly at the end of the image
  vm->programStack = vm->dataMask + 1;
  vm->stackBottom = vm->programStack - PROGRAM_STACK_SIZE;

  vm->compiled = qfalse;

#if defined(NO_VM_COMPILED)
  if (interpret >= VMI_COMPILED)
  {
    Com_Printf("Architecture doesn't have a bytecode compiler, using interpreter\n");
    interpret = VMI_BYTECODE;
  }
#else
  if (interpret >= VMI_COMPILED)
  {
    vm->compiled = qtrue;
    VM_Compile(vm, header);
  }
#endif
  //VM_Compile may have reset vm->compiled if compilation failed
  if (!vm->compiled)
  {
    if (!VM_PrepareInterpreter2(vm, header))
    {
      FS_FreeFile(header); //free the original file
      VM_Free(vm);
      return NULL;
    }
  }

  //free the original file
  FS_FreeFile(header);

  //load the map file
  VM_LoadSymbols(vm);

  Com_Printf("%s loaded in %d bytes on the hunk\n", vm->name, remaining - Hunk_MemoryRemaining());

  return vm;
}

/*
==============
VM_Free
==============
*/
void VM_Free( vm_t *vm ) {

	if(!vm) {
		return;
	}

	if(vm->callLevel) {
		if(!forced_unload) {
			Com_Error( ERR_FATAL, "VM_Free(%s) on running vm", vm->name );
			return;
		} else {
			Com_Printf( "forcefully unloading %s vm\n", vm->name );
		}
	}

	if(vm->destroy)
		vm->destroy(vm);

	if ( vm->dllHandle ) {
		Sys_UnloadDll( vm->dllHandle );
	}
#if 0	// now automatically freed by hunk
	if ( vm->codeBase.ptr ) {
		Z_Free( vm->codeBase.ptr );
	}
	if ( vm->dataBase ) {
		Z_Free( vm->dataBase );
	}
	if ( vm->instructionPointers ) {
		Z_Free( vm->instructionPointers );
	}
#endif
	Com_Memset( vm, 0, sizeof( *vm ) );

        currentVM = NULL;
        lastVM = NULL;
}

void
VM_Clear(void)
{
  qint i;

  for(i = 0;i < VM_COUNT;i++)
  {
    VM_Free(&vmTable[i]);
  }
}

void VM_Forced_Unload_Start(void) {
	forced_unload = 1;
}

void VM_Forced_Unload_Done(void) {
	forced_unload = 0;
}

/*
==============
VM_Call


Upon a system call, the stack will look like:

sp+32	parm1
sp+28	parm0
sp+24	return value
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
intptr_t	QDECL __attribute__((no_sanitize_address)) VM_Call( vm_t *vm, qint callnum, ... ) {
	vm_t	*oldVM;
	intptr_t r;
	qint nargs;
	qint i;

	if ( !vm ) {
		Com_Error( ERR_FATAL, "VM_Call with NULL vm" );
	}

        oldVM = currentVM;
        currentVM = vm;
        lastVM = vm;

	if ( vm_debugLevel ) {
	  Com_Printf( "VM_Call( %d )\n", callnum );
	}

        nargs = vm->vmMainArgs[callnum]; //counting callnum

	++vm->callLevel;
	// if we have a dll loaded, call it directly
	if ( vm->entryPoint ) {
		//rcg010207 -  see dissertation at top of VM_DllSyscall() in this file.
		intptr_t args[MAX_VMMAIN_CALL_ARGS - 1];
		va_list ap;
		va_start(ap, callnum);
		for (i = 0;i < nargs - 1;i++) {
			args[i] = va_arg(ap, intptr_t);
		}
		va_end(ap);

		//add more arguments if you've changed MAX_VMMAIN_CALL_ARGS:
		r = vm->entryPoint(callnum, args[0], args[1], args[2]);
	} else {
#if (id386 || idsparc) && !defined(__clang__) // i386/sparc calling convention doesn't need conversion in some cases
#ifndef NO_VM_COMPILED
		if ( vm->compiled )
			r = VM_CallCompiled( vm, nargs, (qint*)&callnum );
		else
#endif
			r = VM_CallInterpreted2( vm, nargs, (qint*)&callnum );
#else
		qint args[MAX_VMMAIN_CALL_ARGS];
		va_list ap;

		args[0] = callnum;
		va_start(ap, callnum);
		for (i = 1; i < nargs; i++) {
			args[i] = va_arg(ap, qint);
		}
		va_end(ap);
#ifndef NO_VM_COMPILED
		if ( vm->compiled )
			r = VM_CallCompiled( vm, nargs, &args[0] );
		else
#endif
			r = VM_CallInterpreted2( vm, nargs, &args[0] );
#endif
	}
	--vm->callLevel;

        if (oldVM != NULL)
        {
          currentVM = oldVM;
        }

	return r;
}

//=================================================================
static qint QDECL VM_ProfileSort( const void *a, const void *b ) {
	vmSymbol_t	*sa, *sb;

	sa = *(vmSymbol_t **)a;
	sb = *(vmSymbol_t **)b;

	if ( sa->profileCount < sb->profileCount ) {
		return -1;
	}
	if ( sa->profileCount > sb->profileCount ) {
		return 1;
	}
	return 0;
}

/*
==============
VM_NameToVM
==============
*/
static vm_t *
VM_NameToVM(const qchar *name)
{
  vmIndex_t index;

  if (!Q_stricmp(name, "game"))
  {
    index = VM_GAME;
  }
  else if (!Q_stricmp(name, "cgame"))
  {
    index = VM_CGAME;
  }
  else if (!Q_stricmp(name, "ui"))
  {
    index = VM_UI;
  }
  else
  {
    Com_Printf(" unknown VM name '%s'\n", name);
    return NULL;
  }

  if (!vmTable[index].name)
  {
    Com_Printf(" %s is not running.\n", name);
    return NULL;
  }

  return &vmTable[index];
}

/*
==============
VM_VmProfile_f

==============
*/
void VM_VmProfile_f( void ) {
	vm_t		*vm;
	vmSymbol_t	**sorted, *sym;
	qint			i;
	double		total;

	if ( !lastVM ) {
		return;
	}

        if (Cmd_Argc() < 2)
        {
          Com_Printf("usage: %s <game|cgame|ui>\n", Cmd_Argv(0));
          return;
        }

	vm = lastVM = VM_NameToVM(Cmd_Argv(1));

        if (vm == NULL)
        {
          return;
        }

	if ( !vm->numSymbols ) {
		return;
	}

	sorted = Z_Malloc( vm->numSymbols * sizeof( *sorted ) );
	sorted[0] = vm->symbols;
	total = sorted[0]->profileCount;
	for ( i = 1 ; i < vm->numSymbols ; i++ ) {
		sorted[i] = sorted[i-1]->next;
		total += sorted[i]->profileCount;
	}

	qsort( sorted, vm->numSymbols, sizeof( *sorted ), VM_ProfileSort );

	for ( i = 0 ; i < vm->numSymbols ; i++ ) {
		qint		perc;

		sym = sorted[i];

		perc = 100 * (float) sym->profileCount / total;
		Com_Printf( "%2i%% %9i %s\n", perc, sym->profileCount, sym->symName );
		sym->profileCount = 0;
	}

	Com_Printf("    %9.0f total\n", total );

	Z_Free( sorted );
}

/*
==============
VM_VmInfo_f

==============
*/
void VM_VmInfo_f( void ) {
	vm_t	*vm;
	qint		i;

	Com_Printf( "Registered virtual machines:\n" );
	for ( i = 0 ; i < VM_COUNT ; i++ ) {
		vm = &vmTable[i];
		if ( !vm->name ) {
			continue;
		}
		Com_Printf( "%s : ", vm->name );
		if ( vm->dllHandle ) {
			Com_Printf( "native\n" );
			continue;
		}
		if ( vm->compiled ) {
			Com_Printf( "compiled on load\n" );
		} else {
			Com_Printf( "interpreted\n" );
		}
		Com_Printf( "    code length : %7i\n", vm->codeLength );
		Com_Printf( "    table length: %7i\n", vm->instructionCount * 4 );
		Com_Printf( "    data length : %7i\n", vm->dataMask + 1 );
	}
}

/*
===============
VM_LogSyscalls

Insert calls to this while debugging the vm compiler
===============
*/
void VM_LogSyscalls( qint *args ) {
#if 0
	static	qint		callnum;
	static	FILE	*f;

	if ( !f ) {
		f = Sys_FOpen("syscalls.log", "w" );

		if (!f)
		{
		  return;
		}
	}
	callnum++;
	fprintf(f, "%i: %p (%i) = %i %i %i %i\n", callnum, (void*)(args - (qint *)currentVM->dataBase),
		args[0], args[1], args[2], args[3], args[4] );
#endif
}
