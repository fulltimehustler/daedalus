/*
Copyright (C) 2001 StrmnNrmn

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "Base/Daedalus.h"
#include "Core/ROM.h"

#include <stdio.h>

#include "Base/Macros.h"
#include "Base/MathUtil.h"
#include "Config/Preferences.h"
#include "Core/Cheats.h"
#include "Core/CPU.h"
#include "Core/PIF.h"
#include "Core/R4300.h"
#include "Core/ROMBuffer.h"
#include "Core/ROMImage.h"
#include "Core/RomSettings.h"
#include "Debug/DBGConsole.h"
#include "Debug/DebugLog.h"
#include "Debug/Synchroniser.h"
#include "HLEAudio/AudioPlugin.h"
#include "OSHLE/OSHLE.h"
#include "RomFile/RomFile.h"
#include "System/Paths.h"
#include "Ultra/ultra_os.h"
#include "Ultra/ultra_R4300.h"
#include "Utility/CRC.h"
#include "Utility/Stream.h"

#if defined(DAEDALUS_ENABLE_DYNAREC_PROFILE) || defined(DAEDALUS_W32)
// This isn't really the most appropriate place. Need to check with
// the graphics plugin really
u32 g_dwNumFrames = 0;
#endif

RomInfo g_ROM;

static void DumpROMInfo( const ROMHeader & header )
{
	// The "Header" is actually something to do with the PI_DOM_*_OFS values...
	Console_Print("Header:          0x%02x%02x%02x%02x", header.x1, header.x2, header.x3, header.x4);
	Console_Print("Clockrate:       0x%08x", header.ClockRate);
	Console_Print("BootAddr:        0x%08x", SwapEndian(header.BootAddress));
	Console_Print("Release:         0x%08x", header.Release);
	Console_Print("CRC1:            0x%08x", header.CRC1);
	Console_Print("CRC2:            0x%08x", header.CRC2);
	Console_Print("Unknown0:        0x%08x", header.Unknown0);
	Console_Print("Unknown1:        0x%08x", header.Unknown1);
	Console_Print("ImageName:       '%s'",   header.Name);
	Console_Print("Unknown2:        0x%08x", header.Unknown2);
	Console_Print("Unknown3:        0x%04x", header.Unknown3);
	Console_Print("Unknown4:        0x%02x", header.Unknown4);
	Console_Print("Manufacturer:    0x%02x", header.Manufacturer);
	Console_Print("CartID:          0x%04x", header.CartID);
	Console_Print("CountryID:       0x%02x - '%c'", header.CountryID, (char)header.CountryID);
	Console_Print("Unknown5:        0x%02x", header.Unknown5);
}

static void ROM_SimulatePIFBoot( ECicType cic_chip, u32 Country )
{
	// Copy low 1000 bytes to MEM_SP_MEM
	RomBuffer::GetRomBytesRaw( (u8*)g_pMemoryBuffers[MEM_SP_MEM] + RAMROM_BOOTSTRAP_OFFSET,
		   RAMROM_BOOTSTRAP_OFFSET,
		   RAMROM_GAME_OFFSET - RAMROM_BOOTSTRAP_OFFSET );

	// Need to copy crap to SP_IMEM for CIC-6105 boot.
	u8 * pIMemBase = (u8*)g_pMemoryBuffers[ MEM_SP_MEM ] + 0x1000;

	//FIX ME: Some of these are redundant, see CPU_RomOpen
	//
	// gCPUState.CPUControl[C0_SR]		= 0x34000000;	//*SR_FR |*/ SR_ERL | SR_CU2|SR_CU1|SR_CU0;
	R4300_SetSR(0x34000000);
	gCPUState.CPUControl[C0_CONFIG]._u32	= 0x0006E463;	// 0x00066463;

	gCPUState.CPUControl[C0_COUNT]._u32 = 0x5000;
	gCPUState.CPUControl[C0_CAUSE]._u32 = 0x0000005C;
	//ENTRYHI_REGISTER	  = 0xFFFFE0FF;
	gCPUState.CPUControl[C0_CONTEXT]._u32 = 0x007FFFF0;
	gCPUState.CPUControl[C0_EPC]._u32 = 0xFFFFFFFF;
	gCPUState.CPUControl[C0_BADVADDR]._u32 = 0xFFFFFFFF;
	gCPUState.CPUControl[C0_ERROR_EPC]._u32= 0xFFFFFFFF;
	gCPUState.CPUControl[C0_CONFIG]._u32= 0x0006E463;
	//

	gGPR[0]._u64=0x0000000000000000LL;
	gGPR[1]._u64=0x0000000000000000LL;
	gGPR[2]._u64=0xFFFFFFFFD1731BE9LL;
	gGPR[3]._u64=0xFFFFFFFFD1731BE9LL;
	gGPR[4]._u64=0x0000000000001BE9LL;
	gGPR[5]._u64=0xFFFFFFFFF45231E5LL;
	gGPR[6]._u64=0xFFFFFFFFA4001F0CLL;
	gGPR[7]._u64=0xFFFFFFFFA4001F08LL;
	gGPR[8]._u64=0x00000000000000C0LL;
	gGPR[9]._u64=0x0000000000000000LL;
	gGPR[10]._u64=0x0000000000000040LL;
	gGPR[11]._u64=0xFFFFFFFFA4000040LL;
	gGPR[16]._u64=0x0000000000000000LL;
	gGPR[17]._u64=0x0000000000000000LL;
	gGPR[18]._u64=0x0000000000000000LL;
	gGPR[19]._u64=0x0000000000000000LL;
	gGPR[20]._u64=g_ROM.TvType;
	gGPR[21]._u64=0x0000000000000000LL;
	gGPR[23]._u64=0x0000000000000006LL;
	gGPR[24]._u64=0x0000000000000000LL;
	gGPR[25]._u64=0xFFFFFFFFD73f2993LL;
	gGPR[26]._u64=0x0000000000000000LL;
	gGPR[27]._u64=0x0000000000000000LL;
	gGPR[28]._u64=0x0000000000000000LL;
	gGPR[29]._u64=0xFFFFFFFFA4001FF0LL;
	gGPR[30]._u64=0x0000000000000000LL;
	gGPR[31]._u64=0xFFFFFFFFA4001554LL;

	switch (Country) {
		case 0x44: //Germany
		case 0x46: //french
		case 0x49: //Italian
		case 0x50: //Europe
		case 0x53: //Spanish
		case 0x55: //Australia
		case 0x58: // ????
		case 0x59: // X (PAL)
			switch (cic_chip) {
				case CIC_6102:
					gGPR[5]._u64=0xFFFFFFFFC0F1D859LL;
					gGPR[14]._u64=0x000000002DE108EALL;
					gGPR[24]._u64=0x0000000000000000LL;
					break;
				case CIC_6103:
					gGPR[5]._u64=0xFFFFFFFFD4646273LL;
					gGPR[14]._u64=0x000000001AF99984LL;
					gGPR[24]._u64=0x0000000000000000LL;
					break;
				case CIC_6105:
					*(u32 *)&pIMemBase[0x04] = 0xBDA807FC;
					gGPR[5]._u64=0xFFFFFFFFDECAAAD1LL;
					gGPR[14]._u64=0x000000000CF85C13LL;
					gGPR[24]._u64=0x0000000000000002LL;
					break;
				case CIC_6106:
					gGPR[5]._u64=0xFFFFFFFFB04DC903LL;
					gGPR[14]._u64=0x000000001AF99984LL;
					gGPR[24]._u64=0x0000000000000002LL;
					break;
				default:
					break;
			}

			gGPR[20]._u64=0x0000000000000000LL;
			gGPR[23]._u64=0x0000000000000006LL;
			gGPR[31]._u64=0xFFFFFFFFA4001554LL;
			break;
		case 0x37: // 7 (Beta)
		case 0x41: // ????
		case 0x45: //USA
		case 0x4A: //Japan
		default:
			switch (cic_chip) {
				case CIC_6102:
					gGPR[5]._u64=0xFFFFFFFFC95973D5LL;
					gGPR[14]._u64=0x000000002449A366LL;
					break;
				case CIC_6103:
					gGPR[5]._u64=0xFFFFFFFF95315A28LL;
					gGPR[14]._u64=0x000000005BACA1DFLL;
					break;
				case CIC_6105:
					*(u32  *)&pIMemBase[0x04] = 0x8DA807FC;
					gGPR[5]._u64=0x000000005493FB9ALL;
					gGPR[14]._u64=0xFFFFFFFFC2C20384LL;
					break;
				case CIC_6106:
					gGPR[5]._u64=0xFFFFFFFFE067221FLL;
					gGPR[14]._u64=0x000000005CD2B70FLL;
					break;
				default:
					break;
			}
			gGPR[20]._u64=0x0000000000000001LL;
			gGPR[23]._u64=0x0000000000000000LL;
			gGPR[24]._u64=0x0000000000000003LL;
			gGPR[31]._u64=0xFFFFFFFFA4001550LL;
	}

	switch (cic_chip) {
		case CIC_6101:
			gGPR[22]._u64=0x000000000000003FLL;
			break;
		case CIC_6102:
			gGPR[1]._u64=0x0000000000000001LL;
			gGPR[2]._u64=0x000000000EBDA536LL;
			gGPR[3]._u64=0x000000000EBDA536LL;
			gGPR[4]._u64=0x000000000000A536LL;
			gGPR[12]._u64=0xFFFFFFFFED10D0B3LL;
			gGPR[13]._u64=0x000000001402A4CCLL;
			gGPR[15]._u64=0x000000003103E121LL;
			gGPR[22]._u64=0x000000000000003FLL;
			gGPR[25]._u64=0xFFFFFFFF9DEBB54FLL;
			break;
		case CIC_6103:
			gGPR[1]._u64=0x0000000000000001LL;
			gGPR[2]._u64=0x0000000049A5EE96LL;
			gGPR[3]._u64=0x0000000049A5EE96LL;
			gGPR[4]._u64=0x000000000000EE96LL;
			gGPR[12]._u64=0xFFFFFFFFCE9DFBF7LL;
			gGPR[13]._u64=0xFFFFFFFFCE9DFBF7LL;
			gGPR[15]._u64=0x0000000018B63D28LL;
			gGPR[22]._u64=0x0000000000000078LL;
			gGPR[25]._u64=0xFFFFFFFF825B21C9LL;
			break;
		case CIC_6105:
			*(u32  *)&pIMemBase[0x00] = 0x3C0DBFC0;
			*(u32  *)&pIMemBase[0x08] = 0x25AD07C0;
			*(u32  *)&pIMemBase[0x0C] = 0x31080080;
			*(u32  *)&pIMemBase[0x10] = 0x5500FFFC;
			*(u32  *)&pIMemBase[0x14] = 0x3C0DBFC0;
			*(u32  *)&pIMemBase[0x18] = 0x8DA80024;
			*(u32  *)&pIMemBase[0x1C] = 0x3C0BB000;
			gGPR[1]._u64=0x0000000000000000LL;
			gGPR[2]._u64=0xFFFFFFFFF58B0FBFLL;
			gGPR[3]._u64=0xFFFFFFFFF58B0FBFLL;
			gGPR[4]._u64=0x0000000000000FBFLL;
			gGPR[12]._u64=0xFFFFFFFF9651F81ELL;
			gGPR[13]._u64=0x000000002D42AAC5LL;
			gGPR[15]._u64=0x0000000056584D60LL;
			gGPR[22]._u64=0x0000000000000091LL;
			gGPR[25]._u64=0xFFFFFFFFCDCE565FLL;
			break;
		case CIC_6106:
			gGPR[1]._u64=0x0000000000000000LL;
			gGPR[2]._u64=0xFFFFFFFFA95930A4LL;
			gGPR[3]._u64=0xFFFFFFFFA95930A4LL;
			gGPR[4]._u64=0x00000000000030A4LL;
			gGPR[12]._u64=0xFFFFFFFFBCB59510LL;
			gGPR[13]._u64=0xFFFFFFFFBCB59510LL;
			gGPR[15]._u64=0x000000007A3C07F4LL;
			gGPR[22]._u64=0x0000000000000085LL;
			gGPR[25]._u64=0x00000000465E3F72LL;
			break;
		default:
			break;
	}

	// Also need to set up PI_BSD_DOM1 regs etc!
	CPU_SetPC(0xA4000040);
}

bool ROM_ReBoot()
{
	//
	// Find out the CIC type and initialise various systems based on the CIC type
	//
	u8	rom_base[ RAMROM_GAME_OFFSET ];
	RomBuffer::GetRomBytesRaw( rom_base, 0, RAMROM_GAME_OFFSET );

	g_ROM.cic_chip = ROM_GenerateCICType( rom_base );

#ifdef DAEDALUS_DEBUG_CONSOLE
	if (g_ROM.cic_chip == CIC_UNKNOWN)
	{
		//DAEDALUS_ERROR("Unknown CIC CRC: 0x%08x\nAssuming CIC-6102", crc);
		//Console_Print("[MUnknown CIC CRC: 0x%08x]", crc);
		Console_Print("[MUnknown CIC]");
	}
	else
	{
		Console_Print("[MRom uses %s]", ROM_GetCicName( g_ROM.cic_chip ));
	}
#endif
	// XXXX Update this rom's boot info
#ifdef DAEDALUS_ENABLE_DYNAREC_PROFILE
	g_dwNumFrames = 0;
#endif

#ifdef DAEDALUS_ENABLE_OS_HOOKS
	Patch_Reset();
#endif

	// This will always return false since we always simulate boot code instead of loading from file
	//
	/*if ( ROM_LoadPIF( g_ROM.TvType ) )
	{
		ROM_RunPIFBoot( g_ROM.cic_chip );
	}
	else
	{
		ROM_SimulatePIFBoot( g_ROM.cic_chip, g_ROM.rh.CountryID );
	}*/
	ROM_SimulatePIFBoot( g_ROM.cic_chip, g_ROM.rh.CountryID );

	return true;
}

void ROM_Unload()
{
}

//Most hacks are for the PSP, due the limitations of the hardware, and because we prefer speed over accuracy
void SpecificGameHacks( const ROMHeader & id )
{
	Console_Print("ROM ID[%04X]\n", id.CartID);

	g_ROM.HACKS_u32 = 0;	//Default to no game hacks

	switch( id.CartID )
	{
	case 0x324a: g_ROM.GameHacks = WONDER_PROJECTJ2;	break;
	case 0x4547: g_ROM.GameHacks = GOLDEN_EYE;			break;
	case 0x5742: g_ROM.GameHacks = SUPER_BOWLING;		break;
	case 0x514D: g_ROM.GameHacks = PMARIO;				break;
	case 0x5632: g_ROM.GameHacks = CHAMELEON_TWIST_2;	break;
	case 0x4154: g_ROM.GameHacks = TARZAN;				break;
	case 0x4643: g_ROM.GameHacks = CLAY_FIGHTER_63;		break;
	case 0x504A: g_ROM.GameHacks = ISS64;				break;
	case 0x5944: g_ROM.GameHacks = DKR;					break;
	case 0x3247: g_ROM.GameHacks = EXTREME_G2;			break;
	case 0x5359: g_ROM.GameHacks = YOSHI;				break;
	case 0x4C42: g_ROM.GameHacks = BUCK_BUMBLE;			break;
	case 0x4441: g_ROM.GameHacks = WORMS_ARMAGEDDON;	break;

	case 0x464A:	// Jet Force Geminy
	case 0x5647:	// Glover
		g_ROM.SET_ROUND_MODE = true;
		break;
	case 0x4B42:	//Banjo-Kazooie
		g_ROM.TLUT_HACK = true;
		g_ROM.DISABLE_LBU_OPT = true;
		break;
	//case 0x5750:	//PilotWings64
	case 0x4450:	//Perfect Dark
		g_ROM.DISABLE_LBU_OPT = true;
		break;
	case 0x5941:	//AIDYN_CRONICLES
		g_ROM.ALPHA_HACK = true;
		g_ROM.GameHacks = AIDYN_CRONICLES;
		break;
	case 0x424C:	//Mario Party 1
		g_ROM.DISABLE_SIM_CVT_D_S = true;
		break;
	case 0x4A54:	//Tom and Jerry
	case 0x4d4a:	//Earthworm Jim
	case 0x5150:	//PowerPuff Girls
		g_ROM.DISABLE_SIM_CVT_D_S = true;
		break;
	case 0x5144:	//Donald Duck
	case 0x3259:	//Rayman2
		g_ROM.SET_ROUND_MODE = true;
		break;
	case 0x3358:	//GEX3
	case 0x3258:	//GEX64
		g_ROM.GameHacks = GEX_GECKO;
		break;
	case 0x4c5a:	//ZELDA_OOT
		g_ROM.ZELDA_HACK = true;
		g_ROM.GameHacks = ZELDA_OOT;
		break;
	case 0x4F44:	//DK64
		g_ROM.SET_ROUND_MODE = true;
		g_ROM.GameHacks = DK64;
		break;
	case 0x535a:	//ZELDA_MM
		g_ROM.TLUT_HACK = true;
		g_ROM.ZELDA_HACK = true;
		g_ROM.GameHacks = ZELDA_MM;
		break;
	case 0x5653:	//SSV
		g_ROM.TLUT_HACK = true;
		break;
	case 0x5547:	//Sin and punishment
		g_ROM.TLUT_HACK = true;
		g_ROM.GameHacks = SIN_PUNISHMENT;
		break;
	case 0x3742:	//Banjo Tooie
		g_ROM.GameHacks = BANJO_TOOIE;
		g_ROM.TLUT_HACK = true;
		break;
	case 0x5544:	//Duck Dodgers
	case 0x3653:	//Star soldier - vanishing earth
	case 0x324C:	//Top Gear Rally 2
	case 0x5247:	//Top Gear Rally
	case 0x4552:	//Resident Evil 2
	case 0x4446:	//Flying Dragon
	case 0x534E:	//Beetle Racing
		g_ROM.TLUT_HACK = true;
		break;
	case 0x4641:	//Animal crossing
		g_ROM.TLUT_HACK = true;
		g_ROM.GameHacks = ANIMAL_CROSSING;
		break;
	case 0x4842:	//Body Harvest
	case 0x434E:	//Nightmare Creatures
	case 0x5543:	//Cruisn' USA
		g_ROM.GameHacks = BODY_HARVEST;
		break;
	default:
		break;
	}
}

bool ROM_LoadFile(const RomID & rom_id, const RomSettings & settings, const SRomPreferences & preferences )
{
	Console_Print("Reading rom image: [C%s]", g_ROM.FileName.c_str());

	// Get information about the rom header
	RomBuffer::GetRomBytesRaw( &g_ROM.rh, 0, sizeof(ROMHeader) );

	//	Swap into native format
	ROMFile::ByteSwap_3210( &g_ROM.rh, sizeof(ROMHeader) );

	DAEDALUS_ASSERT( RomID( g_ROM.rh ) == rom_id, "Why is the rom id incorrect?" );

	// Copy across various bits
	g_ROM.mRomID   = rom_id;
	g_ROM.settings = settings;
	g_ROM.TvType   = ROM_GetTvTypeFromID( g_ROM.rh.CountryID );

	// Game specific hacks..
	SpecificGameHacks( g_ROM.rh );

	DumpROMInfo( g_ROM.rh );

	// Read and apply preferences from preferences.ini
	preferences.Apply(g_ROM.settings);

	// Parse cheat file this rom, if cheat feature is enabled
	// This is also done when accessing the cheat menu
	// But we do this when ROM is loaded too, to allow any forced enabled cheats to work.
	if (gCheatsEnabled)
	{
		CheatCodes_Read( g_ROM.settings.GameName.c_str(), GetDataFilename("Daedalus.cht"), g_ROM.rh.CountryID );
	}

	Console_Print("[G%s]", g_ROM.settings.GameName.c_str());
	Console_Print("This game has been certified as [G%s] (%s)", g_ROM.settings.Comment.c_str(), g_ROM.settings.Info.c_str());
	Console_Print("SaveType: [G%s]", ROM_GetSaveTypeName(g_ROM.settings.SaveType));
	Console_Print("ApplyPatches: [G%s]", gOSHooksEnabled ? "on" : "off");
	Console_Print("SpeedSync: [G%d]", gSpeedSyncEnabled);
	Console_Print("DynaRec: [G%s]", gDynarecEnabled ? "on" : "off");
	Console_Print("Cheats: [G%s]", gCheatsEnabled ? "on" : "off");

	//Patch_ApplyPatches();

	return true;
}

// Association between a country id value, tv type and name
struct CountryIDInfo
{
	s8				CountryID;
	const char *	CountryName;
	u32				TvType;
};

static const CountryIDInfo g_CountryCodeInfo[] =
{
	{  0,  "0",			OS_TV_NTSC },
	{ '7', "Beta",		OS_TV_NTSC },
	{ 'A', "NTSC",		OS_TV_NTSC },
	{ 'D', "Germany",	OS_TV_PAL },
	{ 'E', "USA",		OS_TV_NTSC },
	{ 'F', "France",	OS_TV_PAL },
	{ 'I', "Italy",		OS_TV_PAL },
	{ 'J', "Japan",		OS_TV_NTSC },
	{ 'P', "Europe",	OS_TV_PAL },
	{ 'S', "Spain",		OS_TV_PAL },
	{ 'U', "Australia", OS_TV_PAL },
	{ 'X', "PAL",		OS_TV_PAL },
	{ 'Y', "PAL",		OS_TV_PAL }
};

// Get a string representing the country name from an ID value
const char * ROM_GetCountryNameFromID( u8 country_id )
{
	for (u32 i = 0; i < ARRAYSIZE( g_CountryCodeInfo ); i++)
	{
		if (g_CountryCodeInfo[i].CountryID == country_id)
		{
			return g_CountryCodeInfo[i].CountryName;
		}
	}

	return "?";
}

u32 ROM_GetTvTypeFromID( u8 country_id )
{
	for (u32 i = 0; i < ARRAYSIZE( g_CountryCodeInfo ); i++)
	{
		if (g_CountryCodeInfo[i].CountryID == country_id)
		{
			return g_CountryCodeInfo[i].TvType;
		}
	}

	return OS_TV_NTSC;
}
