// Absol (d3scene.ru), Abs0l (d3scene.com) 
// 2016 All Right Reserved :)
#define INI_USE_STACK 0

#include <Windows.h>
#include <string>
#include <vector>
#include <time.h>
#include <eh.h>
#include <Psapi.h>
#include <string>

#include <TlHelp32.h>

#include "ini.h"
#include "verinfo.h"
#include "INIReader.h"
#include "ExceptionDump.h"



const char * absolfogclicker = "./AbsolFogClickWatcher.ini";


#define IsKeyPressed(CODE) (GetAsyncKeyState(CODE) & 0x8000) > 0

using namespace std;


typedef LONG( WINAPI *pNtQIT )( HANDLE, LONG, PVOID, ULONG, PULONG );

#define STATUS_SUCCESS    ((NTSTATUS)0x00000000L)

#define ThreadQuerySetWin32StartAddress 9


BOOL WatcherNewDebugEnabled = TRUE;

LPVOID TlsValue;
DWORD TlsIndex;
DWORD _W3XTlsIndex;


char PrintBuffer[ 2048 ];

DWORD GAME_PrintToScreen = 0;

ULONGLONG StartGameTime = 0;

BOOL LogEnabled = FALSE;

char LogBuffer[ 2048 ];

FILE * logfile;


void WatcherLog( const char * format, ... )
{
	if ( !LogEnabled )
		return;
	if ( !logfile )
		return;
	/*char buffer[ 256 ]; va_list args; va_start( args , format );
	vsprintf_s( buffer , 256 , format , args ); va_end( args );
	int lentowrite = strlen( buffer );
	fwrite( &buffer , lentowrite , 1 , logfile ); fflush( logfile );*/
	va_list args = va_list( );
	va_start( args, format );
	vfprintf_s( logfile, format, args );
	va_end( args );
	fflush( logfile );
}

DWORD GetIndex( )
{
	return *( DWORD* )( _W3XTlsIndex );
}

DWORD GetW3TlsForIndex( DWORD index )
{
	DWORD pid = GetCurrentProcessId( );
	THREADENTRY32 te32;
	HANDLE hSnap = CreateToolhelp32Snapshot( TH32CS_SNAPTHREAD, pid );
	te32.dwSize = sizeof( THREADENTRY32 );

	if ( Thread32First( hSnap, &te32 ) )
	{
		do
		{
			if ( te32.th32OwnerProcessID == pid )
			{
				HANDLE hThread = OpenThread( THREAD_ALL_ACCESS, false, te32.th32ThreadID );
				if ( hThread )
				{
					CONTEXT ctx = { CONTEXT_SEGMENTS };
					LDT_ENTRY ldt;
					GetThreadContext( hThread, &ctx );
					GetThreadSelectorEntry( hThread, ctx.SegFs, &ldt );
					DWORD dwThreadBase = ldt.BaseLow | ( ldt.HighWord.Bytes.BaseMid <<
						16u ) | ( ldt.HighWord.Bytes.BaseHi << 24u );
					CloseHandle( hThread );
					if ( dwThreadBase == NULL )
						continue;
					DWORD* dwTLS = *( DWORD** )( dwThreadBase + 0xE10 + 4 * index );
					if ( dwTLS == NULL )
						continue;

					CloseHandle( hSnap );
					return ( DWORD )dwTLS;
				}
			}
		} while ( Thread32Next( hSnap, &te32 ) );
	}

	CloseHandle( hSnap );

	return NULL;
}

void SetTlsForMe( )
{
	TlsIndex = GetIndex( );
	LPVOID tls = ( LPVOID )GetW3TlsForIndex( TlsIndex );
	TlsValue = tls;
}



union DWFP
{
	DWORD dw;
	float fl;
};

int GameID = 0;

int GameDll = 0;

HMODULE MainModule = 0;

BOOL FirstInitialized = FALSE;

int IsInGame = 0;

BOOL GameStarted = FALSE;

int pJassEnvAddress = 0;

int pW3XGlobalClass = 0;

int pPrintText2 = 0;

int GameVersion = 0x126a;

BOOL DetectRightClickOnlyHeroes = TRUE;

BOOL SkipIllusions = TRUE;

BOOL MinimapPingFogClick = FALSE;

BOOL DetectImpossibleClicks = FALSE;

BOOL DetectItemDestroyer = FALSE;

BOOL DetectOwnItems = FALSE;

BOOL DetectPointClicks = FALSE;

BOOL DetectMeepoKey = TRUE;

BOOL DebugLog = FALSE;

int DetectQuality = 2;

BOOL ExceptionFilterHooked = FALSE;

struct UnitSelected
{
	ULONGLONG LatestTime[ 20 ];
	int SelectCount;
	int LatestPlayer;
	int UnitAddr;
};


LONG __stdcall DotaVectoredToSehHandler( _EXCEPTION_POINTERS *ExceptionInfo )
{
	if ( !ExceptionInfo )
	{
		ExceptionInfo = new	_EXCEPTION_POINTERS( );
	}
	if ( !ExceptionInfo->ContextRecord )
	{
		ExceptionInfo->ContextRecord = new CONTEXT( );
	}

	if ( !ExceptionInfo->ExceptionRecord )
	{
		ExceptionInfo->ExceptionRecord = new EXCEPTION_RECORD( );
	}

	PEXCEPTION_RECORD ex = ExceptionInfo->ExceptionRecord;

	DWORD exceptionCode = ExceptionInfo->ExceptionRecord->ExceptionCode;

	if ( ( exceptionCode & ERROR_SEVERITY_ERROR ) != ERROR_SEVERITY_ERROR ) {
		return ExceptionContinueSearch;
	}


	if ( exceptionCode & APPLICATION_ERROR_MASK ) {
		return ExceptionContinueSearch;
	}

	if ( ( ex->ExceptionFlags & EXCEPTION_NONCONTINUABLE ) == 0 )
	{
		if ( *( BOOL* )IsInGame )
		{
			throw logic_error( "Error found!" );
		}
		return ExceptionContinueSearch;
	}

	throw logic_error( "Error found!" );
	return ExceptionContinueSearch;
}



struct FogHelper
{
	int UnitAddr;
	BOOL FogState[ 20 ][ 2 ];
};

vector<UnitSelected> ClickCount;

vector<FogHelper> FogHelperList;

HANDLE MainThread = NULL;

DWORD GetCurrentLocalTime( )
{
	time_t rawtime;
	struct tm timeinfo;

	time( &rawtime );
	localtime_s( &timeinfo, &rawtime );
	return ( timeinfo.tm_hour * 3600 + timeinfo.tm_min * 60 + timeinfo.tm_sec ) * 1000;
}

typedef unsigned int( __cdecl * pGetPlayerColor )( int whichPlayer );
pGetPlayerColor GetPlayerColor;


unsigned int         PLAYER_COLOR_RED = 0;
unsigned int         PLAYER_COLOR_BLUE = 1;
unsigned int         PLAYER_COLOR_CYAN = 2;
unsigned int         PLAYER_COLOR_PURPLE = 3;
unsigned int         PLAYER_COLOR_YELLOW = 4;
unsigned int         PLAYER_COLOR_ORANGE = 5;
unsigned int         PLAYER_COLOR_GREEN = 6;
unsigned int         PLAYER_COLOR_PINK = 7;
unsigned int         PLAYER_COLOR_LIGHT_GRAY = 8;
unsigned int         PLAYER_COLOR_LIGHT_BLUE = 9;
unsigned int         PLAYER_COLOR_AQUA = 10;
unsigned int         PLAYER_COLOR_BROWN = 11;


const char * GetPlayerColorString( int player )
{
	unsigned int c = GetPlayerColor( player );
	if ( c == PLAYER_COLOR_RED )
		return "|cffFF0202";
	else if ( c == PLAYER_COLOR_BLUE )
		return "|cff0041FF";
	else if ( c == PLAYER_COLOR_CYAN )
		return "|cff1BE5B8";
	else if ( c == PLAYER_COLOR_PURPLE )
		return "|cff530080";
	else if ( c == PLAYER_COLOR_YELLOW )
		return "|cffFFFC00";
	else if ( c == PLAYER_COLOR_ORANGE )
		return "|cffFE890D";
	else if ( c == PLAYER_COLOR_GREEN )
		return "|cff1FBF00";
	else if ( c == PLAYER_COLOR_PINK )
		return "|cffE45AAF";
	else if ( c == PLAYER_COLOR_LIGHT_GRAY )
		return "|cff949596";
	else if ( c == PLAYER_COLOR_LIGHT_BLUE )
		return "|cff7DBEF1";
	else if ( c == PLAYER_COLOR_AQUA )
		return "|cff0F6145";
	else if ( c == PLAYER_COLOR_BROWN )
		return "|cff4D2903";
	else
		return "|cffFFFFFF";

}

unsigned int GetPlayerColorUINT( int player )
{
	unsigned int c = GetPlayerColor( player );
	if ( c == PLAYER_COLOR_RED )
		return 0xffFF0202;
	else if ( c == PLAYER_COLOR_BLUE )
		return 0xff0041FF;
	else if ( c == PLAYER_COLOR_CYAN )
		return 0xff1BE5B8;
	else if ( c == PLAYER_COLOR_PURPLE )
		return 0xff530080;
	else if ( c == PLAYER_COLOR_YELLOW )
		return 0xffFFFC00;
	else if ( c == PLAYER_COLOR_ORANGE )
		return 0xffFE890D;
	else if ( c == PLAYER_COLOR_GREEN )
		return 0xff1FBF00;
	else if ( c == PLAYER_COLOR_PINK )
		return 0xffE45AAF;
	else if ( c == PLAYER_COLOR_LIGHT_GRAY )
		return 0xff949596;
	else if ( c == PLAYER_COLOR_LIGHT_BLUE )
		return 0xff7DBEF1;
	else if ( c == PLAYER_COLOR_AQUA )
		return 0xff0F6145;
	else if ( c == PLAYER_COLOR_BROWN )
		return 0xff4D2903;
	else
		return 0xffFFFFFF;
}

char * DefaultString = "DefaultString\0\xa..........................................................................................................";

int GetObjectClassID( int unit_or_item_addr )
{
	if ( unit_or_item_addr )
		return *( int* )( unit_or_item_addr + 0x30 );
	return 0;
}

int GetUnitOwnerSlot( int unitaddr )
{
	if ( unitaddr )
		return *( int* )( unitaddr + 88 );
	return 15;
}

int pGlobalPlayerData;

void * GetGlobalPlayerData( )
{
	if ( *( int * )( pGlobalPlayerData ) > 0 )
	{
		return ( void * ) *( int* )( pGlobalPlayerData );
	}
	return 0;
}



int GetPlayerByNumber( int number )
{
	void * arg1 = GetGlobalPlayerData( );
	int result = 0;
	if ( arg1 != 0 )
	{
		result = ( int )arg1 + ( number * 4 ) + 0x58;

		if ( result )
		{
			result = *( int* )result;
		}
		else
		{
			return 0;
		}
	}
	return result;
}

int GameTimeOffset = 0;

DWORD GetGameTime( )
{
	return *( DWORD* )GameTimeOffset;
}


__inline void SleepGameTime( int sleeptime )
{
	int currentgametime = *( DWORD* )GameTimeOffset;

	if ( currentgametime > 0 )
	{
		while ( ( DWORD )( currentgametime + sleeptime ) > *( DWORD* )GameTimeOffset )
		{
			Sleep( 10 );
			if ( *( DWORD* )GameTimeOffset == 0 )
			{
				Sleep( sleeptime );
				break;
			}
		}
	}
	else
	{
		Sleep( sleeptime );
	}
}

BOOL PrintOrderName = FALSE;

char convitostr[ 126 ];

const char * ConvertIdToString( int id )
{
	if ( !PrintOrderName )
	{
		return "-ORDER-";
	}

	if ( id == 0xD000F ) { return "-ATTACK-"; }
	if ( id == 0xD0010 ) { return "-attackground-"; }
	if ( id == 0xD0011 ) { return "-attackonce-"; }
	if ( id == 0xD01F2 ) { return "-awaken-"; }
	if ( id == 0xD0012 || id == 0xD0003 ) { return "-MOVE-"; }
	if ( id == 0xD0014 ) { return "-AImove-"; }
	if ( id == 0xD0016 ) { return "-patrol-"; }
	if ( id == 0xD0019 ) { return "-holdposition-"; }
	if ( id == 0xD001A ) { return "-build-"; }
	if ( id == 0xD001B ) { return "-humanbuild-"; }
	if ( id == 0xD001C ) { return "-orcbuild-"; }
	if ( id == 0xD001D ) { return "-nightelfbuild-"; }
	if ( id == 0xD001E ) { return "-undeadbuild-"; }
	if ( id == 0xD01F3 ) { return "-nagabuild-"; }
	if ( id == 0xD001F ) { return "-resumebuild-"; }
	if ( id == 0xD0021 ) { return "-dropitem-"; }
	if ( id == 0xD004B ) { return "-board-"; }
	if ( id == 0xD002F ) { return "-detectaoe-"; }
	if ( id == 0xD000D ) { return "-getitem-"; }
	if ( id == 0xD0032 ) { return "-harvest-"; }
	if ( id == 0xD0035 ) { return "-autoharvestgold-"; }
	if ( id == 0xD0036 ) { return "-autoharvestlumber-"; }
	if ( id == 0xD0031 ) { return "-resumeharvesting-"; }
	if ( id == 0xD0034 ) { return "-returnresources-"; }
	if ( id == 0xD004C ) { return "-forceboard-"; }
	if ( id == 0xD004E ) { return "-load-"; }
	if ( id == 0xD004F ) { return "-unload-"; }
	if ( id == 0xD0050 ) { return "-unloadall-"; }
	if ( id == 0xD0051 ) { return "-unloadallinstant-"; }
	if ( id == 0xD0052 ) { return "-loadcorpse-"; }
	if ( id == 0xD0056 ) { return "-unloadallcorpses-"; }
	if ( id == 0xD0055 ) { return "-loadcorpseinstant-"; }
	if ( id == 0xD01F5 ) { return "-mount-"; }
	if ( id == 0xD01F6 ) { return "-dismount-"; }
	if ( id == 0xD0037 ) { return "-neutraldetectaoe-"; }
	if ( id == 0xD00BD ) { return "-recharge-"; }
	if ( id == 0xD0038 ) { return "-repair-"; }
	if ( id == 0xD0039 ) { return "-repairon-"; }
	if ( id == 0xD003A ) { return "-repairoff-"; }
	if ( id == 0xD0047 ) { return "-revive-"; }
	if ( id == 0xD0048 ) { return "-selfdestruct-"; }
	if ( id == 0xD0049 ) { return "-selfdestructon-"; }
	if ( id == 0xD004A ) { return "-selfdestructoff-"; }
	if ( id == 0xD000C ) { return "-setrally-"; }
	if ( id == 0xD0004 ) { return "-stop-"; }
	if ( id == 0xD01F9 ) { return "-cloudoffog-"; }
	if ( id == 0xD01FA ) { return "-controlmagic-"; }
	if ( id == 0xD0057 ) { return "-defend-"; }
	if ( id == 0xD0058 ) { return "-undefend-"; }
	if ( id == 0xD0059 ) { return "-dispel-"; }
	if ( id == 0xD005C ) { return "-flare-"; }
	if ( id == 0xD005F ) { return "-heal-"; }
	if ( id == 0xD0060 ) { return "-healon-"; }
	if ( id == 0xD0061 ) { return "-healoff-"; }
	if ( id == 0xD0062 ) { return "-innerfire-"; }
	if ( id == 0xD0063 ) { return "-innerfireon-"; }
	if ( id == 0xD0064 ) { return "-innerfireoff-"; }
	if ( id == 0xD0065 ) { return "-invisibility-"; }
	if ( id == 0xD01FE ) { return "-magicdefense-"; }
	if ( id == 0xD01FF ) { return "-magicundefense-"; }
	if ( id == 0xD0200 ) { return "-magicleash-"; }
	if ( id == 0xD0067 ) { return "-militiaconvert-"; }
	if ( id == 0xD02AB ) { return "-militiaunconvert-"; }
	if ( id == 0xD0068 ) { return "-militia-"; }
	if ( id == 0xD0069 ) { return "-militiaoff-"; }
	if ( id == 0xD0201 ) { return "-phoenixfire-"; }
	if ( id == 0xD0202 ) { return "-phoenixmorph-"; }
	if ( id == 0xD006A ) { return "-polymorph-"; }
	if ( id == 0xD006B ) { return "-slow-"; }
	if ( id == 0xD006C ) { return "-slowon-"; }
	if ( id == 0xD006D ) { return "-slowoff-"; }
	if ( id == 0xD0203 ) { return "-spellsteal-"; }
	if ( id == 0xD0204 ) { return "-spellstealon-"; }
	if ( id == 0xD0205 ) { return "-spellstealoff-"; }
	if ( id == 0xD006F ) { return "-tankdroppilot-"; }
	if ( id == 0xD0070 ) { return "-tankloadpilot-"; }
	if ( id == 0xD0071 ) { return "-tankpilot-"; }
	if ( id == 0xD0072 ) { return "-townbellon-"; }
	if ( id == 0xD0073 ) { return "-townbelloff-"; }
	if ( id == 0xD0076 ) { return "-avatar-"; }
	if ( id == 0xD0077 ) { return "-unavatar-"; }
	if ( id == 0xD0206 ) { return "-banish-"; }
	if ( id == 0xD0079 ) { return "-blizzard-"; }
	if ( id == 0xD007A ) { return "-divineshield-"; }
	if ( id == 0xD007B ) { return "-undivineshield-"; }
	if ( id == 0xD0208 ) { return "-flamestrike-"; }
	if ( id == 0xD007C ) { return "-holybolt-"; }
	if ( id == 0xD026D ) { return "-manashieldon-"; }
	if ( id == 0xD026E ) { return "-manashieldoff-"; }
	if ( id == 0xD007D ) { return "-massteleport-"; }
	if ( id == 0xD0209 ) { return "-summonphoenix-"; }
	if ( id == 0xD007E ) { return "-resurrection-"; }
	if ( id == 0xD007F ) { return "-thunderbolt-"; }
	if ( id == 0xD0080 ) { return "-thunderclap-"; }
	if ( id == 0xD0081 ) { return "-waterelemental-"; }
	if ( id == 0xD020A ) { return "-ancestralspirit-"; }
	if ( id == 0xD020B ) { return "-ancestralspirittarget-"; }
	if ( id == 0xD0083 ) { return "-battlestations-"; }
	if ( id == 0xD0084 ) { return "-berserk-"; }
	if ( id == 0xD0085 ) { return "-bloodlust-"; }
	if ( id == 0xD0086 ) { return "-bloodluston-"; }
	if ( id == 0xD0087 ) { return "-bloodlustoff-"; }
	if ( id == 0xD020D ) { return "-corporealform-"; }
	if ( id == 0xD020E ) { return "-uncorporealform-"; }
	if ( id == 0xD0088 ) { return "-devour-"; }
	if ( id == 0xD020F ) { return "-disenchant-"; }
	if ( id == 0xD008A ) { return "-ensnare-"; }
	if ( id == 0xD008B ) { return "-ensnareon-"; }
	if ( id == 0xD008C ) { return "-ensnareoff-"; }
	if ( id == 0xD0210 ) { return "-etherealform-"; }
	if ( id == 0xD0211 ) { return "-unetherealform-"; }
	if ( id == 0xD0089 ) { return "-evileye-"; }
	if ( id == 0xD008D ) { return "-healingward-"; }
	if ( id == 0xD008E ) { return "-lightningshield-"; }
	if ( id == 0xD008F ) { return "-purge-"; }
	if ( id == 0xD0213 ) { return "-spiritlink-"; }
	if ( id == 0xD0091 ) { return "-standdown-"; }
	if ( id == 0xD0092 ) { return "-stasistrap-"; }
	if ( id == 0xD0214 ) { return "-unstableconcoction-"; }
	if ( id == 0xD0097 ) { return "-chainlightning-"; }
	if ( id == 0xD0099 ) { return "-earthquake-"; }
	if ( id == 0xD009A ) { return "-farsight-"; }
	if ( id == 0xD0215 ) { return "-healingwave-"; }
	if ( id == 0xD0216 ) { return "-hex-"; }
	if ( id == 0xD009B ) { return "-mirrorimage-"; }
	if ( id == 0xD009D ) { return "-shockwave-"; }
	if ( id == 0xD009E ) { return "-spiritwolf-"; }
	if ( id == 0xD009F ) { return "-stomp-"; }
	if ( id == 0xD0217 ) { return "-voodoo-"; }
	if ( id == 0xD0218 ) { return "-ward-"; }
	if ( id == 0xD00A0 ) { return "-whirlwind-"; }
	if ( id == 0xD00A1 ) { return "-windwalk-"; }
	if ( id == 0xD00A2 ) { return "-unwindwalk-"; }
	if ( id == 0xD00A3 ) { return "-ambush-"; }
	if ( id == 0xD00A4 ) { return "-autodispel-"; }
	if ( id == 0xD00A5 ) { return "-autodispelon-"; }
	if ( id == 0xD00A6 ) { return "-autodispeloff-"; }
	if ( id == 0xD00A7 ) { return "-barkskin-"; }
	if ( id == 0xD00A8 ) { return "-barkskinon-"; }
	if ( id == 0xD00A9 ) { return "-barkskinoff-"; }
	if ( id == 0xD00AA ) { return "-bearform-"; }
	if ( id == 0xD00AB ) { return "-unbearform-"; }
	if ( id == 0xD00AC ) { return "-corrosivebreath-"; }
	if ( id == 0xD00AE ) { return "-loadarcher-"; }
	if ( id == 0xD00AF ) { return "-mounthippogryph-"; }
	if ( id == 0xD021C ) { return "-coupleinstant-"; }
	if ( id == 0xD021B ) { return "-coupletarget-"; }
	if ( id == 0xD00B0 ) { return "-cyclone-"; }
	if ( id == 0xD021D ) { return "-decouple-"; }
	if ( id == 0xD00B1 ) { return "-detonate-"; }
	if ( id == 0xD00B2 ) { return "-eattree-"; }
	if ( id == 0xD00B3 ) { return "-entangle-"; }
	if ( id == 0xD00B4 ) { return "-entangleinstant-"; }
	if ( id == 0xD0219 ) { return "-autoentangle-"; }
	if ( id == 0xD021A ) { return "-autoentangleinstant-"; }
	if ( id == 0xD00B5 ) { return "-faeriefire-"; }
	if ( id == 0xD00B6 ) { return "-faeriefireon-"; }
	if ( id == 0xD00B7 ) { return "-faeriefireoff-"; }
	if ( id == 0xD021F ) { return "-grabtree-"; }
	if ( id == 0xD0220 ) { return "-manaflareon-"; }
	if ( id == 0xD0221 ) { return "-manaflareoff-"; }
	if ( id == 0xD0222 ) { return "-phaseshift-"; }
	if ( id == 0xD0223 ) { return "-phaseshifton-"; }
	if ( id == 0xD0224 ) { return "-phaseshiftoff-"; }
	if ( id == 0xD0225 ) { return "-phaseshiftinstant-"; }
	if ( id == 0xD00BB ) { return "-ravenform-"; }
	if ( id == 0xD00BC ) { return "-unravenform-"; }
	if ( id == 0xD00BE ) { return "-rechargeon-"; }
	if ( id == 0xD00BF ) { return "-rechargeoff-"; }
	if ( id == 0xD00C0 ) { return "-rejuvination-"; }
	if ( id == 0xD00C1 ) { return "-renew-"; }
	if ( id == 0xD00C2 ) { return "-renewon-"; }
	if ( id == 0xD00C3 ) { return "-renewoff-"; }
	if ( id == 0xD00C4 ) { return "-roar-"; }
	if ( id == 0xD00C5 ) { return "-root-"; }
	if ( id == 0xD00C6 ) { return "-unroot-"; }
	if ( id == 0xD00D6 ) { return "-sentinel-"; }
	if ( id == 0xD0228 ) { return "-taunt-"; }
	if ( id == 0xD0229 ) { return "-vengeance-"; }
	if ( id == 0xD022A ) { return "-vengeanceon-"; }
	if ( id == 0xD022B ) { return "-vengeanceoff-"; }
	if ( id == 0xD022C ) { return "-vengeanceinstant-"; }
	if ( id == 0xD00F6 ) { return "-wispharvest-"; }
	if ( id == 0xD022D ) { return "-blink-"; }
	if ( id == 0xD00CB ) { return "-entanglingroots-"; }
	if ( id == 0xD022E ) { return "-fanofknives-"; }
	if ( id == 0xD00CD ) { return "-flamingarrowstarg-"; }
	if ( id == 0xD00CE ) { return "-flamingarrows-"; }
	if ( id == 0xD00CF ) { return "-unflamingarrows-"; }
	if ( id == 0xD00D0 ) { return "-forceofnature-"; }
	if ( id == 0xD00D1 ) { return "-immolation-"; }
	if ( id == 0xD00D2 ) { return "-unimmolation-"; }
	if ( id == 0xD00D3 ) { return "-manaburn-"; }
	if ( id == 0xD00D4 ) { return "-metamorphosis-"; }
	if ( id == 0xD00D5 ) { return "-scout-"; }
	if ( id == 0xD022F ) { return "-shadowstrike-"; }
	if ( id == 0xD0230 ) { return "-spiritofvengeance-"; }
	if ( id == 0xD00D7 ) { return "-starfall-"; }
	if ( id == 0xD00D8 ) { return "-tranquility-"; }
	if ( id == 0xD0231 ) { return "-absorb-"; }
	if ( id == 0xD00D9 ) { return "-acolyteharvest-"; }
	if ( id == 0xD00DA ) { return "-antimagicshell-"; }
	if ( id == 0xD0233 ) { return "-avengerform-"; }
	if ( id == 0xD0234 ) { return "-unavengerform-"; }
	if ( id == 0xD00DB ) { return "-blight-"; }
	if ( id == 0xD0235 ) { return "-burrow-"; }
	if ( id == 0xD0236 ) { return "-unburrow-"; }
	if ( id == 0xD00DC ) { return "-cannibalize-"; }
	if ( id == 0xD00DD ) { return "-cripple-"; }
	if ( id == 0xD00DE ) { return "-curse-"; }
	if ( id == 0xD00DF ) { return "-curseon-"; }
	if ( id == 0xD00E0 ) { return "-curseoff-"; }
	if ( id == 0xD0238 ) { return "-devourmagic-"; }
	if ( id == 0xD023B ) { return "-flamingattacktarg-"; }
	if ( id == 0xD023C ) { return "-flamingattack-"; }
	if ( id == 0xD023D ) { return "-unflamingattack-"; }
	if ( id == 0xD00E3 ) { return "-freezingbreath-"; }
	if ( id == 0xD00E4 ) { return "-possession-"; }
	if ( id == 0xD00E5 ) { return "-raisedead-"; }
	if ( id == 0xD00E6 ) { return "-raisedeadon-"; }
	if ( id == 0xD00E7 ) { return "-raisedeadoff-"; }
	if ( id == 0xD00E8 ) { return "-instant-"; }
	if ( id == 0xD023E ) { return "-replenish-"; }
	if ( id == 0xD023F ) { return "-replenishon-"; }
	if ( id == 0xD0240 ) { return "-replenishoff-"; }
	if ( id == 0xD0241 ) { return "-replenishlife-"; }
	if ( id == 0xD0242 ) { return "-replenishlifeon-"; }
	if ( id == 0xD0243 ) { return "-replenishlifeoff-"; }
	if ( id == 0xD0244 ) { return "-replenishmana-"; }
	if ( id == 0xD0245 ) { return "-replenishmanaon-"; }
	if ( id == 0xD0246 ) { return "-replenishmanaoff-"; }
	if ( id == 0xD00E9 ) { return "-requestsacrifice-"; }
	if ( id == 0xD00EA ) { return "-restoration-"; }
	if ( id == 0xD00EB ) { return "-restorationon-"; }
	if ( id == 0xD00EC ) { return "-restorationoff-"; }
	if ( id == 0xD00ED ) { return "-sacrifice-"; }
	if ( id == 0xD00EE ) { return "-stoneform-"; }
	if ( id == 0xD00EF ) { return "-unstoneform-"; }
	if ( id == 0xD00F1 ) { return "-unholyfrenzy-"; }
	if ( id == 0xD00F2 ) { return "-unsummon-"; }
	if ( id == 0xD00F3 ) { return "-web-"; }
	if ( id == 0xD00F4 ) { return "-webon-"; }
	if ( id == 0xD00F5 ) { return "-weboff-"; }
	if ( id == 0xD00F9 ) { return "-animatedead-"; }
	if ( id == 0xD00F7 ) { return "-auraunholy-"; }
	if ( id == 0xD00F8 ) { return "-auravampiric-"; }
	if ( id == 0xD0247 ) { return "-carrionscarabs-"; }
	if ( id == 0xD0248 ) { return "-carrionscarabson-"; }
	if ( id == 0xD0249 ) { return "-carrionscarabsoff-"; }
	if ( id == 0xD024A ) { return "-carrionscarabsinstant-"; }
	if ( id == 0xD00FA ) { return "-carrionswarm-"; }
	if ( id == 0xD00FB ) { return "-darkritual-"; }
	if ( id == 0xD00FC ) { return "-darksummoning-"; }
	if ( id == 0xD00FD ) { return "-deathanddecay-"; }
	if ( id == 0xD00FE ) { return "-deathcoil-"; }
	if ( id == 0xD00FF ) { return "-deathpact-"; }
	if ( id == 0xD0100 ) { return "-dreadlordinferno-"; }
	if ( id == 0xD0101 ) { return "-frostarmor-"; }
	if ( id == 0xD01EA ) { return "-frostarmoron-"; }
	if ( id == 0xD01EB ) { return "-frostarmoroff-"; }
	if ( id == 0xD0102 ) { return "-frostnova-"; }
	if ( id == 0xD024B ) { return "-impale-"; }
	if ( id == 0xD024C ) { return "-locustswarm-"; }
	if ( id == 0xD0103 ) { return "-sleep-"; }
	if ( id == 0xD0250 ) { return "-breathoffrost-"; }
	if ( id == 0xD0116 ) { return "-creepanimatedead-"; }
	if ( id == 0xD0117 ) { return "-creepdevour-"; }
	if ( id == 0xD0118 ) { return "-creepheal-"; }
	if ( id == 0xD0119 ) { return "-creephealon-"; }
	if ( id == 0xD011A ) { return "-creephealoff-"; }
	if ( id == 0xD011C ) { return "-creepthunderbolt-"; }
	if ( id == 0xD011D ) { return "-creepthunderclap-"; }
	if ( id == 0xD0251 ) { return "-frenzy-"; }
	if ( id == 0xD0252 ) { return "-frenzyon-"; }
	if ( id == 0xD0253 ) { return "-frenzyoff-"; }
	if ( id == 0xD0254 ) { return "-mechanicalcritter-"; }
	if ( id == 0xD0255 ) { return "-mindrot-"; }
	if ( id == 0xD0109 ) { return "-gold2lumber-"; }
	if ( id == 0xD010A ) { return "-lumber2gold-"; }
	if ( id == 0xD0256 ) { return "-neutralinteract-"; }
	if ( id == 0xD010B ) { return "-spies-"; }
	if ( id == 0xD0258 ) { return "-preservation-"; }
	if ( id == 0xD010F ) { return "-request_hero-"; }
	if ( id == 0xD0259 ) { return "-sanctuary-"; }
	if ( id == 0xD025A ) { return "-shadowsight-"; }
	if ( id == 0xD025B ) { return "-spellshield-"; }
	if ( id == 0xD025C ) { return "-spellshieldaoe-"; }
	if ( id == 0xD025D ) { return "-spirittroll-"; }
	if ( id == 0xD025E ) { return "-steal-"; }
	if ( id == 0xD0260 ) { return "-attributemodskill-"; }
	if ( id == 0xD0261 ) { return "-blackarrow-"; }
	if ( id == 0xD0262 ) { return "-blackarrowon-"; }
	if ( id == 0xD0263 ) { return "-blackarrowoff-"; }
	if ( id == 0xD0264 ) { return "-breathoffire-"; }
	if ( id == 0xD0265 ) { return "-charm-"; }
	if ( id == 0xD0113 ) { return "-coldarrowstarg-"; }
	if ( id == 0xD0114 ) { return "-coldarrows-"; }
	if ( id == 0xD0115 ) { return "-uncoldarrows-"; }
	if ( id == 0xD0267 ) { return "-doom-"; }
	if ( id == 0xD0207 ) { return "-drain-"; }
	if ( id == 0xD0269 ) { return "-drunkenhaze-"; }
	if ( id == 0xD026A ) { return "-elementalfury-"; }
	if ( id == 0xD026B ) { return "-forkedlightning-"; }
	if ( id == 0xD026C ) { return "-howlofterror-"; }
	if ( id == 0xD0108 ) { return "-inferno-"; }
	if ( id == 0xD026F ) { return "-monsoon-"; }
	if ( id == 0xD011E ) { return "-poisonarrowstarg-"; }
	if ( id == 0xD011F ) { return "-poisonarrows-"; }
	if ( id == 0xD0120 ) { return "-unpoisonarrows-"; }
	if ( id == 0xD0270 ) { return "-silence-"; }
	if ( id == 0xD0271 ) { return "-stampede-"; }
	if ( id == 0xD0272 ) { return "-summongrizzly-"; }
	if ( id == 0xD0273 ) { return "-summonquillbeast-"; }
	if ( id == 0xD0274 ) { return "-summonwareagle-"; }
	if ( id == 0xD0275 ) { return "-tornado-"; }
	if ( id == 0xD0276 ) { return "-wateryminion-"; }
	if ( id == 0xD02B6 ) { return "-acidbomb-"; }
	if ( id == 0xD02B7 ) { return "-chemicalrage-"; }
	if ( id == 0xD02B8 ) { return "-healingspray-"; }
	if ( id == 0xD02B9 ) { return "-transmute-"; }
	if ( id == 0xD02AC ) { return "-clusterrockets-"; }
	if ( id == 0xD02B0 ) { return "-robogoblin-"; }
	if ( id == 0xD02B1 ) { return "-unrobogoblin-"; }
	if ( id == 0xD02B2 ) { return "-summonfactory-"; }
	if ( id == 0xD02BE ) { return "-incineratearrow-"; }
	if ( id == 0xD02BF ) { return "-incineratearrowon-"; }
	if ( id == 0xD02C0 ) { return "-incineratearrowoff-"; }
	if ( id == 0xD02BB ) { return "-lavamonster-"; }
	if ( id == 0xD02BC ) { return "-soulburn-"; }
	if ( id == 0xD02BD ) { return "-volcano-"; }
	if ( id == 0xD0277 ) { return "-battleroar-"; }
	if ( id == 0xD0278 ) { return "-channel-"; }
	if ( id == 0xD0104 ) { return "-darkconversion-"; }
	if ( id == 0xD0105 ) { return "-darkportal-"; }
	if ( id == 0xD0106 ) { return "-fingerofdeath-"; }
	if ( id == 0xD0107 ) { return "-firebolt-"; }
	if ( id == 0xD0279 ) { return "-parasite-"; }
	if ( id == 0xD027A ) { return "-parasiteon-"; }
	if ( id == 0xD027B ) { return "-parasiteoff-"; }
	if ( id == 0xD010D ) { return "-rainofchaos-"; }
	if ( id == 0xD010E ) { return "-rainoffire-"; }
	if ( id == 0xD0111 ) { return "-revenge-"; }
	if ( id == 0xD0112 ) { return "-soulpreservation-"; }
	if ( id == 0xD027C ) { return "-submerge-"; }
	if ( id == 0xD027D ) { return "-unsubmerge-"; }
	if ( id == 0xD0110 ) { return "-disassociate-"; }
	if ( id == 0xD0296 ) { return "-neutralspell-"; }

	if ( id >= 0xD0022 && id <= 0xD0028 )
		return "-dropitem-";
	if ( id >= 0xD0028 && id <= 0xd002d )
		return "-inventory-";
	_itoa_s( id, convitostr, 126, 16 );
	return convitostr;
}


BOOL LocalPlayerDisabled = FALSE;

BOOL FileExists( LPCTSTR fname )
{
	return ::GetFileAttributes( fname ) != DWORD( -1 );
}

int GetLocalPlayerNumber( )
{
	if ( LocalPlayerDisabled )
		return -2;

	void * gldata = GetGlobalPlayerData( );
	if ( gldata != 0 )
	{
		int playerslotaddr = ( int )gldata + 0x28;
		return ( int ) *( short * )( playerslotaddr );
	}
	else
		return 16;
}



int GetItemCountAndItemArray( int ** itemarray )
{
	int GlobalClassOffset = *( int* )( pW3XGlobalClass );
	if ( GlobalClassOffset > 0 )
	{
		int ItemOffset1 = *( int* )( GlobalClassOffset + 0x3BC ) + 0x10;
		int ItemCount = *( int* )( ItemOffset1 + 0x604 );
		if ( ItemCount > 0 && ItemOffset1 > 0 )
		{
			*itemarray = ( int * ) *( int* )( ItemOffset1 + 0x608 );
			return ItemCount;
		}
	}
	*itemarray = 0;
	return 0;
}

int lUnitsCount = 1;
int* TempUnitArray = new int[ 0 ];
BOOL SkipUpdate = FALSE;
void FillUnitCountAndUnitArray( )
{
	if ( SkipUpdate )
		return;

	SkipUpdate = TRUE;

	if ( lUnitsCount > 0 )
	{
		lUnitsCount = 0;
		delete[ ] TempUnitArray;
	}

	int GlobalClassOffset = *( int* )( pW3XGlobalClass );
	if ( GlobalClassOffset > 0 )
	{
		int UnitsOffset1 = *( int* )( GlobalClassOffset + 0x3BC );
		if ( UnitsOffset1 > 0 )
		{
			int * UnitsCount = ( int * )( UnitsOffset1 + 0x604 );
			if ( UnitsCount > 0
				&& *UnitsCount > 0 )
			{
				int *unitarray = ( int * ) *( int* )( UnitsOffset1 + 0x608 );
				lUnitsCount = *UnitsCount;
				TempUnitArray = new int[ lUnitsCount ];
				memcpy( TempUnitArray, unitarray, 4 * lUnitsCount );
			}
		}
	}
	SkipUpdate = FALSE;
}


BOOL IsUnitDead( int unitaddr )
{
	if ( unitaddr > 0 )
	{
		unsigned int unitflag = *( unsigned int* )( unitaddr + 0x5C );
		BOOL UnitNotDead = ( ( unitflag & 0x100u ) == 0 );
		return UnitNotDead == FALSE;
	}
	return TRUE;
}

BOOL IsUnitTower( int unitaddr )
{
	if ( unitaddr )
	{
		unsigned int unitflag = *( unsigned int* )( unitaddr + 0x5C );
		BOOL UnitNotTower = ( ( unitflag & 0x10000u ) == 0 );
		return UnitNotTower == FALSE;
	}
	return TRUE;
}

BOOL UnitHaveItems( int unitaddr )
{
	if ( unitaddr )
	{
		return *( int* )( unitaddr + 0x1F8 ) > 0;
	}
	return FALSE;
}

BOOL UnitHaveAttack( int unitaddr )
{
	if ( unitaddr )
	{
		return *( int* )( unitaddr + 0x1e8 ) > 0;
	}
	return FALSE;
}


BOOL IsHero( int unitaddr )
{
	if ( unitaddr )
	{
		unsigned int ishero = *( unsigned int* )( unitaddr + 48 );
		ishero = ishero >> 24;
		ishero = ishero - 64;
		return ishero < 0x19;
	}
	return FALSE;
}


BOOL IsDetectedTower( int unitaddr )
{
	if ( unitaddr )
	{
		unsigned int unitflag = *( unsigned int* )( unitaddr + 0x5C );
		unsigned int unitflag2 = *( unsigned int* )( unitaddr + 32 );
		BOOL UnitNotTower = ( ( unitflag & 0x10000u ) == 0 );
		if ( !UnitNotTower )
		{
			BOOL UnitNotVulnerable = ( ( unitflag2 & 0x8u ) == 0 );
			BOOL UnitClickable = ( ( unitflag2 & 0x4u ) == 0 );
			if ( !UnitNotVulnerable && !UnitClickable )
			{
				return !IsHero( unitaddr ) && !UnitHaveItems( unitaddr ) && UnitHaveAttack( unitaddr );
			}
		}
	}
	return FALSE;
}


int UnitVtable = 0;
int ItemVtable = 0;

BOOL IsOkayPtr( void * addr, unsigned int size )
{
	return !IsBadReadPtr( addr, size );
}

float GetUnitTimer( int unitaddr )
{
	int unitdataddr = *( int* )( unitaddr + 0x28 );
	if ( unitdataddr <= 0 )
		return 0.0f;
#ifdef BOTDEBUG
	PrintDebugInfo( "CheckBadUnit - ENDCHExCK5" );
#endif
	if ( IsOkayPtr( ( void* )( unitdataddr + 0xA4 ), 4 ) )
		return *( float* )( unitdataddr + 0xA0 );
	return 0.0f;
}
int GetSomeAddr_Addr = 0;
#pragma optimize("",off)

__declspec( naked ) int __fastcall GetSomeAddr( UINT a1, UINT a2 )
{
	__asm
	{
		JMP GetSomeAddr_Addr;
	}
}
#pragma optimize("",on)

BOOL __stdcall IsNotBadUnit( int unitaddr )
{
	if ( unitaddr > 0 )
	{
		int xaddraddr = ( int )&UnitVtable;

		if ( *( BYTE* )xaddraddr != *( BYTE* )unitaddr )
			return FALSE;
		else if ( *( BYTE* )( xaddraddr + 1 ) != *( BYTE* )( unitaddr + 1 ) )
			return FALSE;
		else if ( *( BYTE* )( xaddraddr + 2 ) != *( BYTE* )( unitaddr + 2 ) )
			return FALSE;
		else if ( *( BYTE* )( xaddraddr + 3 ) != *( BYTE* )( unitaddr + 3 ) )
			return FALSE;

		unsigned int unitflag = *( unsigned int* )( unitaddr + 0x20 );
		unsigned int unitflag2 = *( unsigned int* )( unitaddr + 0x5C );


		if ( unitflag & 1u )
			return FALSE;

		if ( !( unitflag & 2u ) )
			return FALSE;

		if ( unitflag2 & 0x100u )
			return FALSE;



		return TRUE;
	}

	return FALSE;
}

BOOL IsNotBadItem( int itemaddr, BOOL onlymemcheck = FALSE )
{
	if ( itemaddr > 0 )
	{
		int xaddraddr = ( int )&ItemVtable;

		if ( *( BYTE* )xaddraddr != *( BYTE* )itemaddr )
			return FALSE;
		else if ( *( BYTE* )( xaddraddr + 1 ) != *( BYTE* )( itemaddr + 1 ) )
			return FALSE;
		else if ( *( BYTE* )( xaddraddr + 2 ) != *( BYTE* )( itemaddr + 2 ) )
			return FALSE;
		else if ( *( BYTE* )( xaddraddr + 3 ) != *( BYTE* )( itemaddr + 3 ) )
			return FALSE;



		if ( *( int * )( itemaddr + 0x20 ) & 1 )
			return FALSE;

		float hitpoint = *( float * )( itemaddr + 0x58 );

		return hitpoint != 0.0f;
	}

	return FALSE;
}




typedef int( __cdecl * pPlayer )( int number );
pPlayer PlayerReal;

int Player( int number )
{
	if ( number >= 0 && number <= 15 )
	{
		return PlayerReal( number );
	}
	return 0;
}


int IsUnitVisibledAddr = 0;
#pragma optimize("",off)

BOOL __declspec( naked ) __cdecl IsUnitVisibled126a( int unitaddr, int player )
{
	__asm
	{
		MOV ECX, [ ESP + 0x4 ];
		PUSH ESI;
		MOV EAX, ECX;
		JMP IsUnitVisibledAddr;
	}
}

BOOL __declspec( naked ) __cdecl IsUnitVisibled127a_128a( int unitaddr, int player )
{
	__asm
	{
		PUSH EBP;
		MOV EBP, ESP;
		MOV ECX, [ EBP + 0x8 ];
		PUSH ESI;
		MOV EAX, ECX;
		JMP IsUnitVisibledAddr;
	}
}

BOOL __cdecl IsUnitVisibled( int unitaddr, int player )
{
	if ( !unitaddr || !player )
		return TRUE;
	if ( GameVersion == 0x126a )
		return IsUnitVisibled126a( unitaddr, player );
	else if ( GameVersion == 0x127a || GameVersion == 0x128a )
		return IsUnitVisibled127a_128a( unitaddr, player );
	return TRUE;
}

int IsUnitSelectedAddr = 0;


BOOL __declspec( naked ) __cdecl IsUnitSelected126a( int unitaddr, int player )
{
	__asm
	{
		MOV ECX, [ ESP + 0x4 ];
		PUSH EDI;
		MOV EAX, ECX;
		JMP IsUnitSelectedAddr;
	}
}

BOOL __declspec( naked ) __cdecl IsUnitSelected127a_128a( int unitaddr, int player )
{
	__asm
	{
		PUSH EBP;
		MOV EBP, ESP;
		MOV ECX, [ EBP + 0x8 ];
		PUSH ESI;
		MOV EAX, ECX;
		JMP IsUnitSelectedAddr;
	}
}


BOOL __cdecl IsUnitSelected( int unitaddr, int player )
{
	if ( !unitaddr || !player )
		return FALSE;
	if ( GameVersion == 0x126a )
		return IsUnitSelected126a( unitaddr, player );
	else if ( GameVersion == 0x127a || GameVersion == 0x128a )
		return IsUnitSelected127a_128a( unitaddr, player );
	return FALSE;
}
#pragma optimize("",on)


ULONGLONG ActionTime = GetTickCount64( );

void AddStringToLogFile( const char * line )
{
	int seconds = ( int )( ActionTime / 1000 ) % 60;
	int minutes = ( int )( ( ActionTime / ( 1000 * 60 ) ) % 60 );
	int hours = ( int )( ( ActionTime / ( 1000 * 60 * 60 ) ) % 24 );
	WatcherLog( "[%.2d:%.2d:%.2d] : %s\n", hours, minutes, seconds, line );
}
#pragma optimize("",off)

void DisplayText( char *szText, float fDuration )
{
	DWORD dwDuration = *( ( DWORD * )&fDuration );

	if ( LogEnabled )
	{
		AddStringToLogFile( szText );
	}

	if ( GameStarted && *( BOOL* )IsInGame )
	{
		__asm
		{
			PUSH 0xFFFFFFFF;
			PUSH dwDuration;
			PUSH szText;
			MOV		ECX, [ pW3XGlobalClass ];
			MOV		ECX, [ ECX ];
			MOV		EAX, pPrintText2;
			CALL	EAX;
		}
	}
}
#pragma optimize("",on)


ULONGLONG latestcheck = GetTickCount64( );

#pragma optimize("",off)

int GetSelectedItem( int slot )
{
	int plr = GetPlayerByNumber( slot );
	if ( plr != 0 )
	{
		int itemaddr;
		__asm
		{
			MOV EAX, plr;
			MOV ECX, DWORD PTR DS : [EAX + 0x34];
			CMP ECX, 0;
			JNE NORET;
			MOV EAX, 0;
			ret;
		NORET:
			MOV EAX, DWORD PTR DS : [ECX + 0x1A8];
			MOV itemaddr, EAX;
		}

		return itemaddr;
	}

	return NULL;
}
#pragma optimize( "", on )


int UnitDetectionMethod = 3;

int GetSelectedUnitCount( int slot, BOOL Smaller = TRUE )
{
	int plr = GetPlayerByNumber( slot );
	if ( plr != 0 )
	{
		int PlayerData1 = *( int* )( plr + 0x34 );
		if ( PlayerData1 )
		{
			int unit1 = *( int* )( PlayerData1 + 0x1E0 );
			int unit2 = *( int* )( PlayerData1 + 0x1A4 );
			int unitcount = *( int * )( PlayerData1 + 0x10 );
			int unitcount2 = *( int * )( PlayerData1 + 0x1D4 );

			if ( unit1 != NULL || unit2 != NULL )
			{

				if ( !Smaller )
				{
					if ( unitcount > unitcount2 )
						return unitcount;
					return unitcount2;
				}

				if ( unitcount == unitcount2 )
				{
					return unitcount;
				}
				return unitcount2;
			}

		}
	}

	return NULL;
}

int GetSelectedUnit( int slot )
{
	int plr = GetPlayerByNumber( slot );
	if ( plr != 0 )
	{
		int PlayerData1 = *( int* )( plr + 0x34 );
		if ( PlayerData1 )
		{
			int unit1 = *( int* )( PlayerData1 + 0x1E0 );
			int unit2 = *( int* )( PlayerData1 + 0x1A4 );
			int unitcount = *( int * )( PlayerData1 + 0x10 );
			int unitcount2 = *( int * )( PlayerData1 + 0x1D4 );

			if ( UnitDetectionMethod == 1 )
			{
				int unitaddr = unit1;
				if ( unitaddr > 0 && unitcount == 1 && unitcount2 == 1 )
				{
					if ( IsNotBadUnit( unitaddr ) && IsUnitSelected( unitaddr, Player( slot ) ) )
					{
						return unitaddr;
					}
				}
			}
			else if ( UnitDetectionMethod == 2 )
			{
				int unitaddr = unit2;
				if ( unitaddr > 0 && unitcount == 1 && unitcount2 == 1 )
				{
					if ( IsNotBadUnit( unitaddr ) && IsUnitSelected( unitaddr, Player( slot ) ) )
					{
						return unitaddr;
					}
				}
			}
			else if ( UnitDetectionMethod == 3 )
			{
				if ( unit1 == unit2 )
				{
					int unitaddr = unit1;
					if ( unitaddr > 0 && unitcount == 1 && unitcount2 == 1 )
					{
						if ( IsNotBadUnit( unitaddr ) && IsUnitSelected( unitaddr, Player( slot ) ) )
						{
							return unitaddr;
						}
					}
				}
			}

		}
		return NULL;
	}
	return NULL;
}




//sub_6F32C880 126a
//sub_6F327020 127a
typedef int( __fastcall * p_GetTypeInfo )( int unit_item_code, int unused );
p_GetTypeInfo GetTypeInfo = NULL;

//sub_6F2F8F90 126a
//sub_6F34F730 127a
typedef char *( __fastcall * p_GetPlayerName )( int a1, int a2 );
p_GetPlayerName GetPlayerName = NULL;


char * GetObjectNameByID( int clid )
{
	if ( clid > 0 )
	{
		int v3 = GetTypeInfo( clid, 0 );
		int v4, v5;
		if ( v3 && ( v4 = *( int * )( v3 + 40 ) ) != 0 )
		{
			v5 = v4 - 1;
			if ( v5 >= ( unsigned int )0 )
				v5 = 0;
			return ( char * ) *( int * )( *( int * )( v3 + 44 ) + 4 * v5 );
		}
		else
		{
			return DefaultString;
		}
	}
	return DefaultString;
}





char * GetObjectName( int objaddress )
{
	return GetObjectNameByID( GetObjectClassID( objaddress ) );
}



int CreateJassNativeHook( int oldaddress, int newaddress )
{
	int FirstAddress = *( int * )pJassEnvAddress;
	if ( FirstAddress > 0 )
	{
		FirstAddress = *( int * )( FirstAddress + 20 );
		if ( FirstAddress > 0 )
		{
			FirstAddress = *( int * )( FirstAddress + 32 );
			if ( FirstAddress > 0 )
			{

				int NextAddress = FirstAddress;

				while ( TRUE )
				{
					if ( *( int * )( NextAddress + 12 ) == oldaddress )
					{
						*( int * )( NextAddress + 12 ) = newaddress;

						return NextAddress + 12;
					}

					NextAddress = *( int* )NextAddress;

					if ( NextAddress == FirstAddress || NextAddress <= 0 )
						break;
				}
			}
		}

	}
	return 0;
}

typedef int( __cdecl * pGetSpellAbilityId )( );
pGetSpellAbilityId GetSpellAbilityId_real;
int pRecoveryJassNative1 = 0;

typedef int( __cdecl * pGetTriggerEventId )( );
pGetTriggerEventId GetTriggerEventId_real;
int pRecoveryJassNative2 = 0;

typedef int( __cdecl * pGetIssuedOrderId )( );
pGetIssuedOrderId GetIssuedOrderId_real;
int pRecoveryJassNative3 = 0;

typedef int( __cdecl * pGetSpellAbilityUnit )( );
pGetSpellAbilityUnit GetSpellAbilityUnit_real;
int pRecoveryJassNative4 = 0;

typedef int( __cdecl * pGetAttacker )( );
pGetAttacker GetAttacker_real;
int pRecoveryJassNative5 = 0;

typedef int( __cdecl * pGetSpellTargetUnit )( );
pGetSpellTargetUnit GetSpellTargetUnit;

typedef DWFP( __cdecl * pGetSpellTargetX )( );
pGetSpellTargetX _GetSpellTargetX;

typedef DWFP( __cdecl * pGetSpellTargetY )( );
pGetSpellTargetY _GetSpellTargetY;

typedef int( __fastcall * pGetHandleUnitAddress ) ( int HandleID, int unused );
pGetHandleUnitAddress GetHandleUnitAddress;
pGetHandleUnitAddress GetHandleItemAddress;

typedef BOOL( __cdecl * pIsPlayerEnemy )( int whichPlayer, int otherPlayer );
pIsPlayerEnemy IsPlayerEnemyReal;

typedef int( __cdecl * pGetPlayerController )( int whichPlayer );
pGetPlayerController GetPlayerController;

typedef int( __cdecl * pGetPlayerSlotState )( int whichPlayer );
pGetPlayerSlotState GetPlayerSlotState;


BOOL __cdecl IsPlayerEnemy( int player1, int player2 )
{
	if ( player1 != player2 )
	{
		if ( player1 > 0 && player2 > 0 )
		{
			return IsPlayerEnemyReal( player1, player2 ) && IsPlayerEnemyReal( player2, player1 );
		}
	}
	return FALSE;
}

typedef int( __cdecl * pGetTriggerUnit )( );
pGetTriggerUnit GetTriggerUnit;

typedef int( __cdecl * pGetOrderTargetUnit )( );
pGetOrderTargetUnit GetOrderTargetUnit;

typedef int( __cdecl * pGetOrderTargetItem )( );
pGetOrderTargetItem GetOrderTargetItem;

typedef DWFP( __cdecl * pGetOrderPointX )( );
pGetOrderPointX _GetOrderPointX;

typedef DWFP( __cdecl * pGetOrderPointY )( );
pGetOrderPointY _GetOrderPointY;




typedef BOOL( __cdecl * pIsFoggedToPlayer )( float *x, float *y, int whichPlayer );
pIsFoggedToPlayer IsFoggedToPlayerReal;

int GetItemOwner( int itemaddr )
{
	if ( itemaddr )
		return  *( int* )( itemaddr + 0x74 );
	return 0;
}

BOOL __cdecl IsFoggedToPlayerMy( float * pX, float * pY, int player )
{
	// CENTER
	float x1 = *pX;
	float y1 = *pY;
	// RIGHT
	float x2 = x1 + 128;
	float y2 = y1;
	// LEFT
	float x3 = x1 - 128;
	float y3 = y1;
	// TOP
	float x4 = x1;
	float y4 = y1 + 128;
	// BOT
	float x5 = x1;
	float y5 = y1 - 128;



	BOOL CheckCenter = IsFoggedToPlayerReal( &x1, &y1, player );

	if ( DetectQuality >= 2 )
	{
		BOOL CheckRight = IsFoggedToPlayerReal( &x2, &y2, player );
		BOOL CheckLeft = IsFoggedToPlayerReal( &x3, &y3, player );
		BOOL CheckTop = IsFoggedToPlayerReal( &x4, &y4, player );
		BOOL CheckBot = IsFoggedToPlayerReal( &x5, &y5, player );
		if ( DetectQuality >= 3 )
		{
			return CheckCenter && CheckRight && CheckLeft && CheckTop && CheckBot;
		}
		return CheckCenter && CheckRight && CheckLeft && CheckTop && CheckBot;
	}
	return CheckCenter;
}

ULONGLONG LatestAbilSpell = GetTickCount64( );


void GetUnitLocation2D( int unitaddr, float * x, float * y )
{
	if ( unitaddr )
	{
		*x = *( float* )( unitaddr + 0x284 );
		*y = *( float* )( unitaddr + 0x288 );
	}
	else
	{
		*x = 0.0;
		*y = 0.0;
	}
}


void GetItemLocation2D( int itemaddr, float * x, float * y )
{
	if ( itemaddr )
	{
		int iteminfo = *( int * )( itemaddr + 0x28 );
		if ( iteminfo )
		{
			*x = *( float* )( iteminfo + 0x88 );
			*y = *( float* )( iteminfo + 0x8C );
		}
		else
		{
			*x = 0.0;
			*y = 0.0;
		}
	}
	else
	{
		*x = 0.0;
		*y = 0.0;
	}
}


float Distance( float x1, float y1, float x2, float y2 )
{
	return ( float )sqrt( ( ( double )x2 - ( double )x1 )*( ( double )x2 - ( double )x1 ) + ( ( double )y2 - ( double )y1 )*( ( double )y2 - ( double )y1 ) );
}

float pingduration = 1.5;

typedef void( __cdecl * pPingMinimapEx )( float *x, float *y, float *duration, int red, int green, int blue, BOOL extraEffects );
pPingMinimapEx PingMinimapEx;

void __cdecl PingMinimapMy( float *x, float *y, float *duration, int red, int green, int blue, BOOL extraEffects )
{
	float newx = *x + ( float )( -200 + rand( ) % 200 );
	float newy = *y + ( float )( -200 + rand( ) % 200 );

	return PingMinimapEx( &newx, &newy, &pingduration, red, green, blue, extraEffects );
}

int GetItemByXY( float x, float y, int player )
{
	int * itemsarray = 0;
	int itemsCount = GetItemCountAndItemArray( &itemsarray );
	if ( itemsCount > 0 && itemsarray )
	{
		for ( int i = 0; i < itemsCount; i++ )
		{
			if ( itemsarray[ i ] )
			{
				if ( IsNotBadItem( itemsarray[ i ] ) )
				{
					float itemx = 0.0f, itemy = 0.0f;
					GetItemLocation2D( itemsarray[ i ], &itemx, &itemy );
					if ( Distance( itemx, itemy, x, y ) < 19 )
						return itemsarray[ i ];
				}
			}
		}
	}
	return 0;
}


BOOL ImpossibleClick = FALSE;

int GetUnitByXY( float x, float y, int player, BOOL onlyunits = FALSE )
{
	if ( player == GetLocalPlayerNumber( ) )
		return 0;

	FillUnitCountAndUnitArray( );
	SkipUpdate = TRUE;
	/*int lUnitsCount = 1;
	int* TempUnitArray = new int[ 0 ];*/

	int CurrentUnit = 0;

	if ( lUnitsCount > 0 )
	{
		for ( int i = 0; i < lUnitsCount; i++ )
		{
			CurrentUnit = TempUnitArray[ i ];

			if ( IsNotBadUnit( CurrentUnit ) )
			{
				if ( IsHero( CurrentUnit ) || ( DetectImpossibleClicks && IsDetectedTower( CurrentUnit ) ) || ( !DetectRightClickOnlyHeroes && !IsUnitTower( CurrentUnit ) ) )
				{
					if ( player != GetUnitOwnerSlot( CurrentUnit ) )
					{
						int hPlayerFinder = Player( player );
						int hPlayerOwner = Player( GetUnitOwnerSlot( CurrentUnit ) );

						if ( hPlayerFinder && hPlayerOwner )
						{
							if ( IsPlayerEnemy( hPlayerFinder, hPlayerOwner ) )
							{
								ImpossibleClick = FALSE;
								float unitx = 0.0f, unity = 0.0f;
								GetUnitLocation2D( CurrentUnit, &unitx, &unity );
								if ( DetectImpossibleClicks && IsDetectedTower( CurrentUnit ) )
								{
									if ( Distance( unitx, unity, x, y ) < 22 && Distance( unitx, unity, x, y ) > 1 )
									{
										ImpossibleClick = TRUE;
										SkipUpdate = FALSE;
										return CurrentUnit;
									}
								}

								if ( IsDetectedTower( CurrentUnit ) )
								{
									if ( Distance( unitx, unity, x, y ) <= 1 )
										continue;
								}
								if ( Distance( unitx, unity, x, y ) < 19 )
								{
									SkipUpdate = FALSE;
									return CurrentUnit;
								}
							}
						}
					}
				}
			}
		}
	}
	SkipUpdate = FALSE;
	if ( onlyunits )
	{

		return 0;
	}
	return GetItemByXY( x, y, player );
}

int LastEventID, LastSkillID, LastCasterID;
ULONGLONG LastEventTime = 0;

struct PlayerEvent
{
	int EventID;
	int SkillID;
	int Caster;
	int SelectedUnits;
	ULONGLONG Time;
};

PlayerEvent PlayerEventList[ 20 ][ 20 ];
int MeepoPoofID = 0x41304E38;
//BOOL PlayerMeepoDetect[ 20 ];

void ShiftLeftAndAddNewActionScanForBot( int PlayerID, PlayerEvent NewPlayerEvent )
{
	if ( GetLocalPlayerNumber( ) != PlayerID )
	{
		if ( DebugLog )
			WatcherLog( "[DEBUG][+%u ms][LogActions] : Event:%i - Skill:%X - Caster:%X \n", NewPlayerEvent.Time, NewPlayerEvent.EventID, NewPlayerEvent.SkillID, NewPlayerEvent.Caster );

		if ( NewPlayerEvent.EventID == 272 )
		{

			for ( int i = 0; i < 19; i++ )
			{
				PlayerEventList[ PlayerID ][ i ] = PlayerEventList[ PlayerID ][ i + 1 ];
			}

			PlayerEventList[ PlayerID ][ 19 ] = NewPlayerEvent;

			PlayerEvent Event1 = PlayerEventList[ PlayerID ][ 19 ];
			PlayerEvent Event2 = PlayerEventList[ PlayerID ][ 18 ];
			PlayerEvent Event3 = PlayerEventList[ PlayerID ][ 17 ];
			PlayerEvent Event4 = PlayerEventList[ PlayerID ][ 16 ];


			/*WatcherLog( "[event1][+%u ms][LogActions] : Event:%i - Skill:%X - Caster:%X - units %i \n", Event1.Time, Event1.EventID, Event1.SkillID, Event1.Caster,Event1.SelectedUnits );
			WatcherLog( "[event2][+%u ms][LogActions] : Event:%i - Skill:%X - Caster:%X - units %i\n", Event2.Time, Event2.EventID, Event2.SkillID, Event2.Caster, Event1.SelectedUnits );
			WatcherLog( "[event3][+%u ms][LogActions] : Event:%i - Skill:%X - Caster:%X - units %i\n", Event3.Time, Event3.EventID, Event3.SkillID, Event3.Caster, Event1.SelectedUnits );
			WatcherLog( "[event4][+%u ms][LogActions] : Event:%i - Skill:%X - Caster:%X - units %i\n", Event4.Time, Event4.EventID, Event4.SkillID, Event4.Caster, Event1.SelectedUnits );
			*/

			if ( DetectMeepoKey )
			{


				if ( Event1.SkillID == MeepoPoofID &&
					Event2.SkillID == MeepoPoofID &&
					Event3.SkillID == MeepoPoofID &&
					Event4.SkillID == MeepoPoofID )
				{
					if ( Event1.Caster != 0 &&
						Event2.Caster != 0 &&
						Event3.Caster != 0 &&
						Event4.Caster != 0 )
					{
						if ( Event1.Caster != Event2.Caster &&
							Event1.Caster != Event3.Caster &&
							Event1.Caster != Event4.Caster &&
							//	 Event2.Caster != Event3.Caster &&
							Event3.Caster != Event2.Caster &&
							Event3.Caster != Event4.Caster )
						{
							if ( DebugLog )
							{
								WatcherLog( "[event1][+%u ms][LogActions] : Event:%i - Skill:%X - Caster:%X - units %i\n", Event1.Time, Event1.EventID, Event1.SkillID, Event1.Caster, Event1.SelectedUnits );
								WatcherLog( "[event2][+%u ms][LogActions] : Event:%i - Skill:%X - Caster:%X - units %i\n", Event2.Time, Event2.EventID, Event2.SkillID, Event2.Caster, Event1.SelectedUnits );
								WatcherLog( "[event3][+%u ms][LogActions] : Event:%i - Skill:%X - Caster:%X - units %i\n", Event3.Time, Event3.EventID, Event3.SkillID, Event3.Caster, Event1.SelectedUnits );
								WatcherLog( "[event4][+%u ms][LogActions] : Event:%i - Skill:%X - Caster:%X - units %i\n", Event4.Time, Event4.EventID, Event4.SkillID, Event4.Caster, Event1.SelectedUnits );
							}
							if (
								Event2.Time < 120 &&
								Event3.Time < 120 &&
								Event4.Time < 120 )
							{
								if ( Event1.SelectedUnits == 1 &&
									Event2.SelectedUnits == 1 &&
									Event3.SelectedUnits == 1 &&
									Event4.SelectedUnits == 1
									)
								{
									//if ( !PlayerMeepoDetect[ PlayerID ] )
									//{
									//	PlayerMeepoDetect[ PlayerID ] = TRUE;
									LatestAbilSpell = GetTickCount64( );


									sprintf_s( PrintBuffer, 2048, "|c00EF4000[FogCW v12.1a]|r: Player %s%s|r use MeepoKey!!\0",
										GetPlayerColorString( PlayerID ),
										GetPlayerName( PlayerID, 0 ) );
									ActionTime = GetGameTime( );
									DisplayText( PrintBuffer, 15.0f );
									//}
								}
							}
						}
					}
				}
			}
		}

	}
}

void BotDetector( int SkillID, int EventID, int CasterID )
{
	// THE BOT DETECTOR!

	LastEventID = EventID;
	LastSkillID = SkillID;
	LastCasterID = CasterID;
	if ( CasterID > 0 )
	{
		int CasterAddr = GetHandleUnitAddress( CasterID, 0 );
		if ( CasterAddr > 0 )
		{
			int CasterSlot = GetUnitOwnerSlot( CasterAddr );
			if ( CasterSlot < 15 && CasterSlot >= 0 )
			{
				if ( EventID == 272 )
				{
					PlayerEvent NewPlayerEvent;
					NewPlayerEvent.Caster = CasterAddr;
					NewPlayerEvent.EventID = EventID;
					NewPlayerEvent.SkillID = SkillID;
					NewPlayerEvent.Time = GetTickCount64( ) - LastEventTime;
					NewPlayerEvent.SelectedUnits = GetSelectedUnitCount( CasterSlot, FALSE );
					ShiftLeftAndAddNewActionScanForBot( CasterSlot, NewPlayerEvent );
					LastEventTime = GetTickCount64( );
				}
			}
		}
	}



	//WatcherLog( "[+%ims][LogActions] : Event:%i - Skill:%X - Caster:%X \n", GetGameTime( ) - LastEventTime, EventID, SkillID, CasterID );

}


struct ProcessNewAction
{
	BOOL IsGetSpellAbilityId;
	int CasterUnitHandle;
	int TargetUnitHandle;
	int TargetItemHandle;
	int GetIssuedOrderId;
	int SkillID;
	int EventID;
	float GetSpellOrderTargetX;
	float GetSpellOrderTargetY;
};

vector<ProcessNewAction> ProcessNewActionList;

void ProcessGetSpellAbilityIdAction( int EventID, int TargetUnitHandle, int CasterUnitHandle, int TargetItemHandle, int GetIssueOrderId, float GetSpellTargetX, float GetSpellTargetY )
{

	if ( CasterUnitHandle > 0 )
	{
		if (  TargetUnitHandle > 0 )
		{
			int CasterAddr = GetHandleUnitAddress( CasterUnitHandle, 0 );
			int TargetAddr = GetHandleUnitAddress( TargetUnitHandle, 0 );
			if ( CasterAddr > 0 && IsNotBadUnit( CasterAddr ) && TargetAddr > 0 && IsNotBadUnit( TargetAddr ) )
			{
				int CasterSlot = GetUnitOwnerSlot( CasterAddr );
				int TargetSlot = GetUnitOwnerSlot( TargetAddr );


				if ( CasterSlot >= 0 && CasterSlot <= 15 && TargetSlot >= 0 && TargetSlot <= 15 && ( IsHero( TargetAddr ) || ( !DetectRightClickOnlyHeroes && !IsUnitTower( TargetAddr ) ) ) && CasterSlot != GetLocalPlayerNumber( ) && TargetSlot != CasterSlot && IsPlayerEnemy( Player( CasterSlot ), Player( TargetSlot ) ) )
				{
				if ( GetPlayerController( Player( CasterSlot ) ) == 0 && GetPlayerSlotState( Player( CasterSlot ) ) == 1 )
				{
					BOOL needcontinue = FALSE;

					if ( DetectQuality >= 3 )
					{
						for ( unsigned int z = 0; z < FogHelperList.size( ); z++ )
						{
							if ( TargetAddr == FogHelperList[ z ].UnitAddr )
							{
								if ( !( FogHelperList[ z ].FogState[ CasterSlot ][ 0 ] && FogHelperList[ z ].FogState[ CasterSlot ][ 1 ] ) )
								{
									needcontinue = TRUE;
								}
							}
						}
					}


					if ( !needcontinue )
					{
						float unitx = 0.0f, unity = 0.0f;
						GetUnitLocation2D( TargetAddr, &unitx, &unity );
						if ( IsFoggedToPlayerMy( &unitx, &unity, Player( GetUnitOwnerSlot( CasterAddr ) ) ) )
						{
							LatestAbilSpell = GetTickCount64( );
							if ( MinimapPingFogClick )
							{
								unsigned int PlayerColorInt = GetPlayerColorUINT( Player( GetUnitOwnerSlot( CasterAddr ) ) );
								PingMinimapMy( &unitx, &unity, &pingduration, PlayerColorInt & 0x00FF0000, PlayerColorInt & 0x0000FF00, PlayerColorInt & 0x000000FF, false );
							}

							sprintf_s( PrintBuffer, 2048, "|c00EF4000[FogCW v12.1a]|r: Player %s%s|r use ability(%s) in fogged %s%s|r|r[TARGET]\0",
								GetPlayerColorString( Player( GetUnitOwnerSlot( CasterAddr ) ) ),
								GetPlayerName( GetUnitOwnerSlot( CasterAddr ), 0 ),
								ConvertIdToString( GetIssuedOrderId_real( ) ),
								GetPlayerColorString( Player( GetUnitOwnerSlot( TargetAddr ) ) ),
								GetObjectName( TargetAddr ) );
							ActionTime = GetGameTime( );
							DisplayText( PrintBuffer, 6.0f );
						}
						else if ( !IsUnitVisibled( TargetAddr, Player( GetUnitOwnerSlot( CasterAddr ) ) ) )
						{
							LatestAbilSpell = GetTickCount64( );
							if ( MinimapPingFogClick )
							{
								unsigned int PlayerColorInt = GetPlayerColorUINT( Player( GetUnitOwnerSlot( CasterAddr ) ) );
								PingMinimapMy( &unitx, &unity, &pingduration, PlayerColorInt & 0x00FF0000, PlayerColorInt & 0x0000FF00, PlayerColorInt & 0x000000FF, false );
							}

							sprintf_s( PrintBuffer, 2048, "|c00EF4000[FogCW v12.1a]|r: Player %s%s|r use ability(%s) in invisibled %s%s|r|r[TARGET]\0",
								GetPlayerColorString( Player( GetUnitOwnerSlot( CasterAddr ) ) ),
								GetPlayerName( GetUnitOwnerSlot( CasterAddr ), 0 ),
								ConvertIdToString( GetIssuedOrderId_real( ) ),
								GetPlayerColorString( Player( GetUnitOwnerSlot( TargetAddr ) ) ),
								GetObjectName( TargetAddr ) );
							ActionTime = GetGameTime( );
							DisplayText( PrintBuffer, 6.0f );
						}
					}
				}
				}
			}
		}
		else if ( DetectPointClicks && CasterUnitHandle > 0 && GetSpellTargetX != 0.0f && GetSpellTargetY != 0.0f )
		{
			int CasterAddr = GetHandleUnitAddress( CasterUnitHandle, 0 );

			if ( CasterAddr > 0 )
			{
				float x = GetSpellTargetX;
				float y = GetSpellTargetY;
				int TargetAddr = GetUnitByXY( x, y, GetUnitOwnerSlot( CasterAddr ), TRUE );

				if ( TargetAddr > 0 && ( IsNotBadUnit( TargetAddr ) && ( IsHero( TargetAddr ) || ( !DetectRightClickOnlyHeroes && !IsUnitTower( TargetAddr ) ) ) ) )
				{
					BOOL needcontinue = FALSE;
					if ( DetectQuality >= 3 )
					{
						for ( unsigned int z = 0; z < FogHelperList.size( ); z++ )
						{
							if ( TargetAddr == FogHelperList[ z ].UnitAddr )
							{
								if ( !( FogHelperList[ z ].FogState[ GetUnitOwnerSlot( CasterAddr ) ][ 0 ] && FogHelperList[ z ].FogState[ GetUnitOwnerSlot( CasterAddr ) ][ 1 ] ) )
								{
									needcontinue = TRUE;
								}
							}
						}
					}



					if ( !needcontinue )
					{
						if ( GetPlayerController( Player( GetUnitOwnerSlot( CasterAddr ) ) ) == 0 && GetPlayerSlotState( Player( GetUnitOwnerSlot( CasterAddr ) ) ) == 1 )
						{
							float unitx = 0.0f, unity = 0.0f;
							GetUnitLocation2D( TargetAddr, &unitx, &unity );
							if ( IsFoggedToPlayerMy( &unitx, &unity, Player( GetUnitOwnerSlot( CasterAddr ) ) ) )
							{
								LatestAbilSpell = GetTickCount64( );
								if ( MinimapPingFogClick )
								{
									unsigned int PlayerColorInt = GetPlayerColorUINT( Player( GetUnitOwnerSlot( CasterAddr ) ) );
									PingMinimapMy( &unitx, &unity, &pingduration, PlayerColorInt & 0x00FF0000, PlayerColorInt & 0x0000FF00, PlayerColorInt & 0x000000FF, false );
								}

								sprintf_s( PrintBuffer, 2048, "|c00EF4000[FogCW v12.1a]|r: Player %s%s|r use ability(%s) in fogged %s%s|r|r[POINT]\0",
									GetPlayerColorString( Player( GetUnitOwnerSlot( CasterAddr ) ) ),
									GetPlayerName( GetUnitOwnerSlot( CasterAddr ), 0 ),
									ConvertIdToString( GetIssuedOrderId_real( ) ),
									GetPlayerColorString( Player( GetUnitOwnerSlot( TargetAddr ) ) ),
									GetObjectName( TargetAddr ) );
								ActionTime = GetGameTime( );
								DisplayText( PrintBuffer, 6.0f );
							}
							else if ( !IsUnitVisibled( TargetAddr, Player( GetUnitOwnerSlot( CasterAddr ) ) ) )
							{
								LatestAbilSpell = GetTickCount64( );
								if ( MinimapPingFogClick )
								{
									unsigned int PlayerColorInt = GetPlayerColorUINT( Player( GetUnitOwnerSlot( CasterAddr ) ) );
									PingMinimapMy( &unitx, &unity, &pingduration, PlayerColorInt & 0x00FF0000, PlayerColorInt & 0x0000FF00, PlayerColorInt & 0x000000FF, false );
								}

								sprintf_s( PrintBuffer, 2048, "|c00EF4000[FogCW v12.1a]|r: Player %s%s|r use ability(%s) in invisibled %s%s|r|r[POINT]\0",
									GetPlayerColorString( Player( GetUnitOwnerSlot( CasterAddr ) ) ),
									GetPlayerName( GetUnitOwnerSlot( CasterAddr ), 0 ),
									ConvertIdToString( GetIssuedOrderId_real( ) ),
									GetPlayerColorString( Player( GetUnitOwnerSlot( TargetAddr ) ) ),
									GetObjectName( TargetAddr ) );
								ActionTime = GetGameTime( );
								DisplayText( PrintBuffer, 6.0f );
							}
						}
					}
				}
			}
		}
	}

}

void ProcessGetTriggerEventAction( int EventID, int TargetUnitHandle_, int CasterUnitHandle, int TargetItemHandle, int GetIssuedOrderId, float GetOrderPointX, float GetOrderPointY )
{
	if ( EventID == 77 || EventID == 40 )
	{

		BOOL IsItem = FALSE;
		int TargetUnitHandle = TargetUnitHandle_;
		if ( TargetUnitHandle <= 0 )
		{
			TargetUnitHandle = TargetItemHandle;
			if ( TargetUnitHandle > 0 )
				IsItem = TRUE;
			else
			{
				return;
			}
		}

		if ( CasterUnitHandle > 0 && TargetUnitHandle > 0 )
		{
			int CasterAddr = GetHandleUnitAddress( CasterUnitHandle, 0 );
			int TargetAddr = ( IsItem ? GetHandleItemAddress( TargetUnitHandle, 0 ) : GetHandleUnitAddress( TargetUnitHandle, 0 ) );
			if ( CasterAddr > 0 && TargetAddr > 0 )
			{
				int CasterSlot = GetUnitOwnerSlot( CasterAddr );
				int TargetSlot = ( IsItem ? 15 : GetUnitOwnerSlot( TargetAddr ) );

				if ( ( CasterSlot >= 0 && CasterSlot <= 15 ) && ( TargetSlot >= 0 && TargetSlot <= 15 ) && Player( CasterSlot ) > 0 && Player( TargetSlot ) > 0 )
				{

					BOOL IsOkay = ( IsItem ? ( IsNotBadItem( TargetAddr ) && !( GetIssuedOrderId >= 0xD0022 && GetIssuedOrderId <= 0xD0028 ) )
						: ( IsNotBadUnit( TargetAddr ) && ( IsHero( TargetAddr ) || ( !DetectRightClickOnlyHeroes && !IsUnitTower( TargetAddr ) ) ) ) );

					if ( TargetSlot != CasterSlot && CasterSlot != GetLocalPlayerNumber( ) && IsOkay && ( TargetSlot > 11 || IsPlayerEnemy( Player( CasterSlot ), Player( TargetSlot ) ) ) )
					{
						if ( GetPlayerController( Player( CasterSlot ) ) == 0 && GetPlayerSlotState( Player( CasterSlot ) ) == 1 )
						{
							BOOL needcontinue = FALSE;

							if ( !IsItem )
							{
								if ( DetectQuality >= 3 )
								{

									for ( unsigned int z = 0; z < FogHelperList.size( ); z++ )
									{
										if ( TargetAddr == FogHelperList[ z ].UnitAddr )
										{
											if ( !( FogHelperList[ z ].FogState[ CasterSlot ][ 0 ] && FogHelperList[ z ].FogState[ CasterSlot ][ 1 ] ) )
											{
												needcontinue = TRUE;
											}
										}
									}
								}
							}


							if ( !needcontinue )
							{

								float unitx = 0.0f, unity = 0.0f;
								if ( IsItem )
								{
									GetItemLocation2D( TargetAddr, &unitx, &unity );
								}
								else
								{
									GetUnitLocation2D( TargetAddr, &unitx, &unity );
								}
								if ( IsFoggedToPlayerMy( &unitx, &unity, Player( CasterSlot ) ) )
								{
									LatestAbilSpell = GetTickCount64( );
									if ( MinimapPingFogClick )
									{
										unsigned int PlayerColorInt = GetPlayerColorUINT( Player( CasterSlot ) );
										PingMinimapMy( &unitx, &unity, &pingduration, PlayerColorInt & 0x00FF0000, PlayerColorInt & 0x0000FF00, PlayerColorInt & 0x000000FF, false );
									}
									sprintf_s( PrintBuffer, 2048, "|c00EF4000[FogCW v12.1a]|r: Player %s%s|r use %s in fogged %s %s%s|r|r[TARGET]\0",
										GetPlayerColorString( Player( CasterSlot ) ),
										GetPlayerName( CasterSlot, 0 ),
										ConvertIdToString( GetIssuedOrderId ),
										IsItem ? "[item]" : "[unit]",
										IsItem ? "|cFF4B4B4B" : GetPlayerColorString( Player( TargetSlot ) ),
										GetObjectName( TargetAddr ) );
									ActionTime = GetGameTime( );
									DisplayText( PrintBuffer, 6.0f );
								}
								else if ( !IsItem && !IsUnitVisibled( TargetAddr, Player( CasterSlot ) ) )
								{
									LatestAbilSpell = GetTickCount64( );
									if ( MinimapPingFogClick )
									{
										unsigned int PlayerColorInt = GetPlayerColorUINT( Player( CasterSlot ) );
										PingMinimapMy( &unitx, &unity, &pingduration, PlayerColorInt & 0x00FF0000, PlayerColorInt & 0x0000FF00, PlayerColorInt & 0x000000FF, false );
									}
									sprintf_s( PrintBuffer, 2048, "|c00EF4000[FogCW v12.1a]|r: Player %s%s|r use %s in invisibled %s%s|r|r[TARGET]\0",
										GetPlayerColorString( Player( CasterSlot ) ),
										GetPlayerName( CasterSlot, 0 ),
										ConvertIdToString( GetIssuedOrderId ),
										GetPlayerColorString( Player( TargetSlot ) ),
										GetObjectName( TargetAddr ) );
									ActionTime = GetGameTime( );
									DisplayText( PrintBuffer, 6.0f );
								}
								else if ( IsItem && DetectItemDestroyer && GetIssuedOrderId == 0xD000F )
								{

									if ( GetItemOwner( TargetAddr ) != CasterSlot || DetectOwnItems )
									{

										if ( !IsPlayerEnemy( Player( CasterSlot ), GetItemOwner( TargetAddr ) ) )
										{
											if ( MinimapPingFogClick )
											{
												unsigned int PlayerColorInt = GetPlayerColorUINT( Player( CasterSlot ) );
												PingMinimapMy( &unitx, &unity, &pingduration, PlayerColorInt & 0x00FF0000, PlayerColorInt & 0x0000FF00, PlayerColorInt & 0x000000FF, false );
											}
											LatestAbilSpell = GetTickCount64( );
											sprintf_s( PrintBuffer, 2048, "|c00EF4000[FogCW v12.1a]|r: Player %s%s|r try to destroy item %s%s|r|r\0",
												GetPlayerColorString( Player( CasterSlot ) ),
												GetPlayerName( CasterSlot, 0 ),
												GetPlayerColorString( Player( TargetSlot ) ),
												GetObjectName( TargetAddr ) );
											ActionTime = GetGameTime( );
											DisplayText( PrintBuffer, 6.0f );
										}
									}
								}
							}
						}
					}
				}
			}
		}

	}

	if ( EventID == 76 || EventID == 39 )
	{


		if ( CasterUnitHandle > 0 )
		{
			int CasterAddr = GetHandleUnitAddress( CasterUnitHandle, 0 );

			float x = GetOrderPointX;
			float y = GetOrderPointY;
			float unitx = 0.0f, unity = 0.0f;
			if ( CasterAddr > 0 && GetOrderPointX != 0.0f && GetOrderPointY != 0.0f )
			{

				int CasterSlot = GetUnitOwnerSlot( CasterAddr );

				if ( ( CasterSlot >= 0 && CasterSlot <= 15 ) && Player( CasterSlot ) > 0 )
				{
					if ( GetPlayerController( Player( GetUnitOwnerSlot( CasterAddr ) ) ) == 0 && GetPlayerSlotState( Player( GetUnitOwnerSlot( CasterAddr ) ) ) == 1 )
					{

						int TargetAddr = GetUnitByXY( x, y, GetUnitOwnerSlot( CasterAddr ) );
						BOOL DetectionImpossibleClick = FALSE;

						if ( TargetAddr > 0 && ( IsNotBadUnit( TargetAddr ) && ( IsHero( TargetAddr ) || ( DetectImpossibleClicks && IsDetectedTower( TargetAddr ) ) || ( !DetectRightClickOnlyHeroes && !IsUnitTower( TargetAddr ) ) ) ) )
						{
							int TargetSlot = GetUnitOwnerSlot( TargetAddr );
							if ( ( TargetSlot >= 0 && TargetSlot <= 15 ) && Player( TargetSlot ) > 0 )
							{

								BOOL needcontinue = FALSE;

								if ( DetectQuality >= 3 )
								{
									for ( unsigned int z = 0; z < FogHelperList.size( ); z++ )
									{
										if ( TargetAddr == FogHelperList[ z ].UnitAddr )
										{
											if ( !( FogHelperList[ z ].FogState[ GetUnitOwnerSlot( CasterAddr ) ][ 0 ] && FogHelperList[ z ].FogState[ GetUnitOwnerSlot( CasterAddr ) ][ 1 ] ) )
											{
												needcontinue = TRUE;
											}
										}
									}
								}

								if ( !needcontinue )
								{
									GetUnitLocation2D( TargetAddr, &unitx, &unity );
									if ( DetectImpossibleClicks && IsDetectedTower( TargetAddr ) )
									{
										DetectionImpossibleClick = TRUE;
									}

									if ( ( DetectPointClicks || DetectionImpossibleClick ) && IsFoggedToPlayerMy( &unitx, &unity, Player( GetUnitOwnerSlot( CasterAddr ) ) ) )
									{
										LatestAbilSpell = GetTickCount64( );
										if ( MinimapPingFogClick )
										{
											unsigned int PlayerColorInt = GetPlayerColorUINT( Player( GetUnitOwnerSlot( CasterAddr ) ) );
											PingMinimapMy( &unitx, &unity, &pingduration, PlayerColorInt & 0x00FF0000, PlayerColorInt & 0x0000FF00, PlayerColorInt & 0x000000FF, false );
										}

										sprintf_s( PrintBuffer, 2048, "|c00EF4000[FogCW v12.1a]|r: Player %s%s|r use %s in fogged [unit] %s%s|r|r[POINT]\0",
											GetPlayerColorString( Player( GetUnitOwnerSlot( CasterAddr ) ) ),
											GetPlayerName( GetUnitOwnerSlot( CasterAddr ), 0 ),
											ImpossibleClick ? "-IMPOSSIBLECLICK-" : ConvertIdToString( GetIssuedOrderId ),
											GetPlayerColorString( Player( GetUnitOwnerSlot( TargetAddr ) ) ),
											GetObjectName( TargetAddr ) );
										ActionTime = GetGameTime( );
										DisplayText( PrintBuffer, 6.0f );
										ImpossibleClick = FALSE;
									}
									else if ( DetectPointClicks && !DetectionImpossibleClick && !IsUnitVisibled( TargetAddr, Player( GetUnitOwnerSlot( CasterAddr ) ) ) )
									{
										LatestAbilSpell = GetTickCount64( );
										if ( MinimapPingFogClick )
										{
											unsigned int PlayerColorInt = GetPlayerColorUINT( Player( GetUnitOwnerSlot( CasterAddr ) ) );
											PingMinimapMy( &unitx, &unity, &pingduration, PlayerColorInt & 0x00FF0000, PlayerColorInt & 0x0000FF00, PlayerColorInt & 0x000000FF, false );
										}

										sprintf_s( PrintBuffer, 2048, "|c00EF4000[FogCW v12.1a]|r: Player %s%s|r use %s in invisibled %s%s|r|r[POINT]\0",
											GetPlayerColorString( Player( GetUnitOwnerSlot( CasterAddr ) ) ),
											GetPlayerName( GetUnitOwnerSlot( CasterAddr ), 0 ),
											ConvertIdToString( GetIssuedOrderId ),
											GetPlayerColorString( Player( GetUnitOwnerSlot( TargetAddr ) ) ),
											GetObjectName( TargetAddr ) );
										ActionTime = GetGameTime( );
										DisplayText( PrintBuffer, 6.0f );
									}
								}
							}
							else if ( DetectPointClicks && TargetAddr > 0 && IsNotBadItem( TargetAddr ) && !( GetIssuedOrderId >= 0xD0022 && GetIssuedOrderId <= 0xD0028 ) )
							{
								float xunitx = 0.0f, xunity = 0.0f;
								GetItemLocation2D( TargetAddr, &xunitx, &xunity );
								if ( IsFoggedToPlayerMy( &xunitx, &xunity, Player( GetUnitOwnerSlot( CasterAddr ) ) ) )
								{
									LatestAbilSpell = GetTickCount64( );
									if ( MinimapPingFogClick )
									{
										unsigned int PlayerColorInt = GetPlayerColorUINT( Player( GetUnitOwnerSlot( CasterAddr ) ) );
										PingMinimapMy( &xunitx, &xunity, &pingduration, PlayerColorInt & 0x00FF0000, PlayerColorInt & 0x0000FF00, PlayerColorInt & 0x000000FF, false );
									}


									sprintf_s( PrintBuffer, 2048, "|c00EF4000[FogCW v12.1a]|r: Player %s%s|r use %s in fogged [item] %s%s|r|r[POINT]\0",
										GetPlayerColorString( Player( GetUnitOwnerSlot( CasterAddr ) ) ),
										GetPlayerName( GetUnitOwnerSlot( CasterAddr ), 0 ),
										ConvertIdToString( GetIssuedOrderId ),
										"|cFF4B4B4B",
										GetObjectName( TargetAddr ) );
									ActionTime = GetGameTime( );
									DisplayText( PrintBuffer, 6.0f );
								}
							}
						}
					}
				}
			}
			else if ( CasterAddr > 0 && GetUnitOwnerSlot( CasterAddr ) != GetLocalPlayerNumber( ) && x == 0.0f && y == 0.0f )
			{
				if ( GetIssuedOrderId == 851971 )
				{
					LatestAbilSpell = GetTickCount64( );

					sprintf_s( PrintBuffer, 2048, "|c00EF4000[FogCW v12.1a]|r: Player %s%s|r activate GuAI Maphack!!\0",
						GetPlayerColorString( Player( GetUnitOwnerSlot( CasterAddr ) ) ),
						GetPlayerName( GetUnitOwnerSlot( CasterAddr ), 0 ) );
					ActionTime = GetGameTime( );
					DisplayText( PrintBuffer, 6.0f );

				}
			}
		}

	}


}


DWORD WINAPI ActionProcessFunction( LPVOID )
{
	SetTlsForMe( );
	TlsSetValue( TlsIndex, TlsValue );

	while ( true )
	{
		Sleep( 10 );
		for ( ProcessNewAction CurrentAction : ProcessNewActionList )
		{

			if ( CurrentAction.IsGetSpellAbilityId )
			{
				ProcessGetSpellAbilityIdAction( CurrentAction.EventID, CurrentAction.TargetUnitHandle, CurrentAction.CasterUnitHandle, CurrentAction.TargetItemHandle, CurrentAction.GetIssuedOrderId, CurrentAction.GetSpellOrderTargetX, CurrentAction.GetSpellOrderTargetY );
			}
			else
			{
				ProcessGetTriggerEventAction( CurrentAction.EventID, CurrentAction.TargetUnitHandle, CurrentAction.CasterUnitHandle, CurrentAction.TargetItemHandle, CurrentAction.GetIssuedOrderId, CurrentAction.GetSpellOrderTargetX, CurrentAction.GetSpellOrderTargetY );
			}

		}
		ProcessNewActionList.clear( );
	}


	return 0;
}


BOOL GetTriggerEventIdCalled = FALSE;


int GetTriggerEventId_CasterUnitHandle = 0;
int GetTriggerEventId_EventID = 0;
int GetTriggerEventId_GetIssuedOrderId = 0;
int GetTriggerEventId_SkillID = 0;
int GetTriggerEventId_TargetItemHandle = 0;
int GetTriggerEventId_TargetUnitHandle = 0;

float GetTriggerEventId_x = 0;
float GetTriggerEventId_y = 0;


int GetTriggerEventId_hooked( )
{
	if ( !MainThread )
		MainThread = GetCurrentThread( );


	if ( !GameStarted || GetTriggerEventIdCalled )
		return GetTriggerEventId_real( );

	GetTriggerEventIdCalled = TRUE;



	ProcessNewAction tmpProcessNewAction;
	tmpProcessNewAction.CasterUnitHandle = GetTriggerUnit( );
	tmpProcessNewAction.EventID = GetTriggerEventId_real( );
	tmpProcessNewAction.GetIssuedOrderId = GetIssuedOrderId_real( );
	tmpProcessNewAction.IsGetSpellAbilityId = FALSE;
	tmpProcessNewAction.SkillID = GetSpellAbilityId_real( );
	tmpProcessNewAction.TargetItemHandle = GetOrderTargetItem( );
	tmpProcessNewAction.TargetUnitHandle = GetOrderTargetUnit( );
	tmpProcessNewAction.GetSpellOrderTargetX = _GetOrderPointX( ).fl;
	tmpProcessNewAction.GetSpellOrderTargetY = _GetOrderPointY( ).fl;


	if ( GetTriggerEventId_CasterUnitHandle != tmpProcessNewAction.CasterUnitHandle ||
		GetTriggerEventId_EventID != tmpProcessNewAction.EventID ||
		GetTriggerEventId_GetIssuedOrderId != tmpProcessNewAction.GetIssuedOrderId ||
		GetTriggerEventId_SkillID != tmpProcessNewAction.SkillID ||
		GetTriggerEventId_TargetItemHandle != tmpProcessNewAction.TargetItemHandle ||
		GetTriggerEventId_TargetUnitHandle != tmpProcessNewAction.TargetUnitHandle ||
		GetTriggerEventId_x != tmpProcessNewAction.GetSpellOrderTargetX ||
		GetTriggerEventId_y != tmpProcessNewAction.GetSpellOrderTargetY

		)
	{
		ProcessNewActionList.push_back( tmpProcessNewAction );


		if ( ProcessNewActionList.size( ) >= 1000 )
		{
			ProcessNewActionList.erase( ProcessNewActionList.begin( ) );
		}
	}



	GetTriggerEventId_CasterUnitHandle = tmpProcessNewAction.CasterUnitHandle;
	GetTriggerEventId_EventID = tmpProcessNewAction.EventID;
	GetTriggerEventId_GetIssuedOrderId = tmpProcessNewAction.GetIssuedOrderId;
	GetTriggerEventId_SkillID = tmpProcessNewAction.SkillID;
	GetTriggerEventId_TargetItemHandle = tmpProcessNewAction.TargetItemHandle;
	GetTriggerEventId_TargetUnitHandle = tmpProcessNewAction.TargetUnitHandle;
	GetTriggerEventId_x = tmpProcessNewAction.GetSpellOrderTargetX;
	GetTriggerEventId_y = tmpProcessNewAction.GetSpellOrderTargetY;


	GetTriggerEventIdCalled = FALSE;
	return GetTriggerEventId_real( );
}


int GetSpellAbilityId_CasterUnitHandle = 0;
int GetSpellAbilityId_EventID = 0;
int GetSpellAbilityId_GetIssuedOrderId = 0;
int GetSpellAbilityId_SkillID = 0;
int GetSpellAbilityId_TargetItemHandle = 0;
int GetSpellAbilityId_TargetUnitHandle = 0;


BOOL GetSpellAbilityIdCalled = FALSE;
int GetSpellAbilityId_hooked( )
{
	if ( !MainThread )
		MainThread = GetCurrentThread( );

	if ( !GameStarted || GetSpellAbilityIdCalled )
		return GetSpellAbilityId_real( );

	GetSpellAbilityIdCalled = TRUE;

	ProcessNewAction tmpProcessNewAction;
	tmpProcessNewAction.CasterUnitHandle = GetSpellAbilityUnit_real( );
	tmpProcessNewAction.EventID = GetTriggerEventId_real( );
	tmpProcessNewAction.GetIssuedOrderId = GetIssuedOrderId_real( );
	tmpProcessNewAction.IsGetSpellAbilityId = TRUE;
	tmpProcessNewAction.SkillID = GetSpellAbilityId_real( );
	tmpProcessNewAction.TargetItemHandle = GetOrderTargetItem( );
	tmpProcessNewAction.TargetUnitHandle = GetSpellTargetUnit( );
	tmpProcessNewAction.GetSpellOrderTargetX = _GetSpellTargetX( ).fl;
	tmpProcessNewAction.GetSpellOrderTargetY = _GetSpellTargetY( ).fl;


	if ( GetSpellAbilityId_CasterUnitHandle != tmpProcessNewAction.CasterUnitHandle ||
		GetSpellAbilityId_EventID != tmpProcessNewAction.EventID ||
		GetSpellAbilityId_GetIssuedOrderId != tmpProcessNewAction.GetIssuedOrderId ||
		GetSpellAbilityId_SkillID != tmpProcessNewAction.SkillID ||
		GetSpellAbilityId_TargetItemHandle != tmpProcessNewAction.TargetItemHandle ||
		GetSpellAbilityId_TargetUnitHandle != tmpProcessNewAction.TargetUnitHandle ||
		GetTriggerEventId_x != tmpProcessNewAction.GetSpellOrderTargetX ||
		GetTriggerEventId_y != tmpProcessNewAction.GetSpellOrderTargetY

		)
	{
		ProcessNewActionList.push_back( tmpProcessNewAction );
		if ( ProcessNewActionList.size( ) >= 1000 )
		{
			ProcessNewActionList.erase( ProcessNewActionList.begin( ) );
		}
	}



	GetSpellAbilityId_CasterUnitHandle = tmpProcessNewAction.CasterUnitHandle;
	GetSpellAbilityId_EventID = tmpProcessNewAction.EventID;
	GetSpellAbilityId_GetIssuedOrderId = tmpProcessNewAction.GetIssuedOrderId;
	GetSpellAbilityId_SkillID = tmpProcessNewAction.SkillID;
	GetSpellAbilityId_TargetItemHandle = tmpProcessNewAction.TargetItemHandle;
	GetSpellAbilityId_TargetUnitHandle = tmpProcessNewAction.TargetUnitHandle;
	GetTriggerEventId_x = tmpProcessNewAction.GetSpellOrderTargetX;
	GetTriggerEventId_y = tmpProcessNewAction.GetSpellOrderTargetY;


	GetSpellAbilityIdCalled = FALSE;
	return GetSpellAbilityId_real( );
}

BOOL GetIssuedOrderIdCalled = FALSE;

int GetIssuedOrderId_hooked( )
{
	if ( !MainThread )
		MainThread = GetCurrentThread( );


	if ( GetIssuedOrderIdCalled )
	{
		return GetIssuedOrderId_real( );
	}

	GetIssuedOrderIdCalled = TRUE;
	GetTriggerEventId_hooked( );
	GetIssuedOrderIdCalled = FALSE;
	return GetIssuedOrderId_real( );
}

BOOL GetSpellAbilityUnitCalled = FALSE;

int GetSpellAbilityUnit_hooked( )
{
	if ( !MainThread )
		MainThread = GetCurrentThread( );

	if ( GetSpellAbilityUnitCalled )
		return GetSpellAbilityUnit_real( );

	GetSpellAbilityUnitCalled = TRUE;

	if ( GetSpellAbilityId_hooked( ) > 0 )
	{
		GetSpellAbilityUnitCalled = FALSE;
		return GetSpellAbilityUnit_real( );
	}
	GetTriggerEventId_hooked( );
	GetSpellAbilityUnitCalled = FALSE;
	return GetSpellAbilityUnit_real( );
}

int GetAttacker_hooked( )
{
	if ( !MainThread )
		MainThread = GetCurrentThread( );
	GetTriggerEventId_hooked( );
	//GetSpellAbilityId_hooked( );
	return GetAttacker_real( );
}




int LatestUnit = 0;
int LatestPlayer = 0;
//FogHelperListTemp.clear( );

ULONGLONG LatestFogCheck = 0;

void UpdateFogHelper( )
{
	if ( GetTickCount64( ) - LatestFogCheck > 350 )
	{
		LatestFogCheck = GetTickCount64( );
		for ( int n = 0; n < 12; n++ )
		{
			if ( GetLocalPlayerNumber( ) == n )
				continue;

			if ( !Player( n ) )
				continue;

			if ( GetPlayerController( Player( n ) ) != 0 || GetPlayerSlotState( Player( n ) ) != 1 )
				continue;

			FillUnitCountAndUnitArray( );
			int CurrentUnit = 0;
			SkipUpdate = TRUE;
			if ( lUnitsCount > 0 )
			{
				for ( int i = 0; i < lUnitsCount; i++ )
				{
					CurrentUnit = TempUnitArray[ i ];

					if ( IsNotBadUnit( CurrentUnit ) )
					{
						if ( IsHero( CurrentUnit ) || !DetectRightClickOnlyHeroes )
						{
							float unitx, unity;

							GetUnitLocation2D( CurrentUnit, &unitx, &unity );

							BOOL FoundUnit = FALSE;

							for ( unsigned int m = 0; m < FogHelperList.size( ); m++ )
							{
								if ( CurrentUnit == FogHelperList[ m ].UnitAddr )
								{
									FoundUnit = TRUE;
									FogHelperList[ m ].FogState[ n ][ 1 ] = FogHelperList[ m ].FogState[ n ][ 0 ];
									FogHelperList[ m ].FogState[ n ][ 0 ] = ( IsFoggedToPlayerMy( &unitx, &unity, Player( n ) ) || !IsUnitVisibled( CurrentUnit, Player( n ) ) );
								}
							}

							if ( !FoundUnit )
							{
								FogHelper tmpghelp;
								for ( int z = 0; z < 20; z++ )
								{
									tmpghelp.FogState[ z ][ 0 ] = TRUE;
									tmpghelp.FogState[ z ][ 1 ] = TRUE;
								}
								tmpghelp.UnitAddr = CurrentUnit;
								FogHelperList.push_back( tmpghelp );


								if ( FogHelperList.size( ) >= 1000 )
								{
									FogHelperList.erase( FogHelperList.begin( ) );
								}
							}

						}
					}
				}
			}
			SkipUpdate = FALSE;
		}
	}
}


int PlayerSelectedItems[ 20 ];


void SearchPlayersFogSelect( )
{

	if ( DetectQuality >= 3 )
		UpdateFogHelper( );

	for ( int i = 0; i < 12; i++ )
	{

		if ( GetLocalPlayerNumber( ) == i )
			continue;

		if ( !Player( i ) )
			continue;

		if ( GetPlayerController( Player( i ) ) != 0 || GetPlayerSlotState( Player( i ) ) != 1 )
			continue;

		int selectedunit = 0;
		int hCurrentPlayer = Player( i );


		if ( hCurrentPlayer )
		{

			selectedunit = GetSelectedUnit( i );
			if ( !selectedunit || !IsNotBadUnit( selectedunit ) )
				continue;

			int OwnedPlayerSlot = GetUnitOwnerSlot( selectedunit );

			if ( OwnedPlayerSlot < 0 || OwnedPlayerSlot > 15 )
				continue;

			int hOwnerPlayer = Player( OwnedPlayerSlot );

			if ( !hOwnerPlayer || ( hCurrentPlayer == LatestPlayer && LatestUnit == selectedunit ) )
				continue;

			if ( hOwnerPlayer == hCurrentPlayer )
				continue;


			DWORD tmpActionTime = GetGameTime( );

			if ( IsPlayerEnemy( hOwnerPlayer, hCurrentPlayer ) || OwnedPlayerSlot > 11 )
			{

				float unitx = 0.0f, unity = 0.0f;
				GetUnitLocation2D( selectedunit, &unitx, &unity );
				if ( IsFoggedToPlayerMy( &unitx, &unity, hCurrentPlayer ) || !IsUnitVisibled( selectedunit, hCurrentPlayer ) )
				{

					BOOL found = FALSE;
					BOOL needcontinue = FALSE;

					UnitSelected * tmpunitselected = NULL;

					for ( unsigned int n = 0; n < ClickCount.size( ); n++ )
					{
						if ( ClickCount[ n ].UnitAddr == selectedunit )
						{
							found = TRUE;
							if ( GetTickCount64( ) - ClickCount[ n ].LatestTime[ i ] < 5000 )
							{
								needcontinue = TRUE;
							}
							else
							{
								tmpunitselected = &ClickCount[ n ];
							}
						}
					}

					if ( !found )
					{
						UnitSelected tmpus;
						tmpus.LatestPlayer = i;
						for ( int n = 0; n < 20; n++ )
							tmpus.LatestTime[ n ] = GetTickCount64( );
						tmpus.SelectCount = 0;
						tmpus.UnitAddr = selectedunit;
						ClickCount.push_back( tmpus );
						tmpunitselected = &ClickCount[ ClickCount.size( ) - 1 ];

						if ( ClickCount.size( ) >= 1000 )
						{
							ClickCount.erase( ClickCount.begin( ) );
						}
					}


					if ( DetectQuality >= 3 )
					{
						for ( unsigned int z = 0; z < FogHelperList.size( ); z++ )
						{
							if ( selectedunit == FogHelperList[ z ].UnitAddr )
							{
								if ( !( FogHelperList[ z ].FogState[ i ][ 0 ] && FogHelperList[ z ].FogState[ i ][ 1 ] ) )
								{
									needcontinue = TRUE;
								}
							}
						}
					}

					if ( needcontinue )
					{
						continue;
					}

					SleepGameTime( 50 * DetectQuality );
					if ( !*( BOOL* )IsInGame || !GetGameTime( ) )
					{

						return;
					}


					hCurrentPlayer = Player( i );
					hOwnerPlayer = Player( OwnedPlayerSlot );
					unitx = 0.0f, unity = 0.0f;
					if ( !hCurrentPlayer || !hOwnerPlayer || GetSelectedUnit( i ) != selectedunit )
					{
						continue;
					}
					GetUnitLocation2D( selectedunit, &unitx, &unity );
					if ( !( IsFoggedToPlayerMy( &unitx, &unity, hCurrentPlayer ) || !IsUnitVisibled( selectedunit, hCurrentPlayer ) ) )
					{
						continue;
					}


					SleepGameTime( 50 * DetectQuality );
					if ( !*( BOOL* )IsInGame || !GetGameTime( ) )
					{
						return;
					}

					hCurrentPlayer = Player( i );
					hOwnerPlayer = Player( OwnedPlayerSlot );
					unitx = 0.0f, unity = 0.0f;
					if ( !hCurrentPlayer || !hOwnerPlayer || GetSelectedUnit( i ) != selectedunit )
					{
						continue;
					}
					GetUnitLocation2D( selectedunit, &unitx, &unity );

					if ( IsFoggedToPlayerMy( &unitx, &unity, hCurrentPlayer ) || !IsUnitVisibled( selectedunit, hCurrentPlayer ) )
					{
						SleepGameTime( 300 );
						if ( !*( BOOL* )IsInGame || !GetGameTime( ) )
						{
							return;
						}

						hCurrentPlayer = Player( i );
						hOwnerPlayer = Player( OwnedPlayerSlot );
						unitx = 0.0f, unity = 0.0f;
						if ( !hCurrentPlayer || !hOwnerPlayer || GetSelectedUnit( i ) != selectedunit )
						{
							goto PossibledDetect;
						}

						GetUnitLocation2D( selectedunit, &unitx, &unity );
						if ( IsFoggedToPlayerMy( &unitx, &unity, hCurrentPlayer ) || !IsUnitVisibled( selectedunit, hCurrentPlayer ) )
						{
							if ( !IsFoggedToPlayerMy( &unitx, &unity, hCurrentPlayer ) )
							{
								if ( MinimapPingFogClick )
								{
									unsigned int PlayerColorInt = GetPlayerColorUINT( Player( i ) );
									PingMinimapMy( &unitx, &unity, &pingduration, PlayerColorInt & 0x00FF0000, PlayerColorInt & 0x0000FF00, PlayerColorInt & 0x000000FF, false );
								}

								tmpunitselected->LatestTime[ i ] = GetTickCount64( );

								sprintf_s( PrintBuffer, 2048, "|c00EF4000[FogCW v12.1a]|r: Player %s%s|r select invisibled %s%s|r|r [DETECTED]",

									GetPlayerColorString( Player( i ) ),
									GetPlayerName( i, 0 ),
									GetPlayerColorString( Player( OwnedPlayerSlot ) ),
									GetObjectName( selectedunit ) );
							}
							else
							{
								if ( MinimapPingFogClick )
								{
									unsigned int PlayerColorInt = GetPlayerColorUINT( Player( i ) );
									PingMinimapMy( &unitx, &unity, &pingduration, PlayerColorInt & 0x00FF0000, PlayerColorInt & 0x0000FF00, PlayerColorInt & 0x000000FF, false );
								}

								tmpunitselected->LatestTime[ i ] = GetTickCount64( );

								sprintf_s( PrintBuffer, 2048, "|c00EF4000[FogCW v12.1a]|r: Player %s%s|r select fogged %s%s|r|r [DETECTED]\0",

									GetPlayerColorString( Player( i ) ),
									GetPlayerName( i, 0 ),
									GetPlayerColorString( Player( OwnedPlayerSlot ) ),
									GetObjectName( selectedunit ) );
							}

							ActionTime = tmpActionTime;
							DisplayText( PrintBuffer, 6.0f );
							LatestPlayer = hCurrentPlayer;
							LatestUnit = selectedunit;

							break;
						}
						else
						{
						PossibledDetect:
							if ( !IsFoggedToPlayerMy( &unitx, &unity, hCurrentPlayer ) )
							{
								tmpunitselected->LatestTime[ i ] = GetTickCount64( );

								sprintf_s( PrintBuffer, 2048, "|c00EF4000[FogCW v12.1a]|r: Player %s%s|r select invisibled %s%s|r|r [POSSIBLE]\0",
									GetPlayerColorString( Player( i ) ),
									GetPlayerName( i, 0 ),
									GetPlayerColorString( Player( OwnedPlayerSlot ) ),
									GetObjectName( selectedunit ) );
							}
							else
							{
								tmpunitselected->LatestTime[ i ] = GetTickCount64( );

								sprintf_s( PrintBuffer, 2048, "|c00EF4000[FogCW v12.1a]|r: Player %s%s|r select fogged %s%s|r|r [POSSIBLE]\0",
									GetPlayerColorString( Player( i ) ),
									GetPlayerName( i, 0 ),
									GetPlayerColorString( Player( OwnedPlayerSlot ) ),
									GetObjectName( selectedunit ) );
							}
							ActionTime = tmpActionTime;
							DisplayText( PrintBuffer, 6.0f );
							LatestPlayer = hCurrentPlayer;
							LatestUnit = selectedunit;


							break;
						}

					}

				}
			}
		}
	}

}
char absbuf[ 100 ];


void InitOtherFunctions( )
{
	if ( !FileExists( ".\\AbsolFogClickWatcher.ini" ) )
	{
		int pOldRecoveryJassNative1 = pRecoveryJassNative1;
		pRecoveryJassNative1 = CreateJassNativeHook( ( int )GetSpellAbilityId_real, ( int )&GetSpellAbilityId_hooked );
		if ( pRecoveryJassNative1 <= 0 )
			pRecoveryJassNative1 = pOldRecoveryJassNative1;

		int pOldRecoveryJassNative2 = pRecoveryJassNative2;
		pRecoveryJassNative2 = CreateJassNativeHook( ( int )GetTriggerEventId_real, ( int )&GetTriggerEventId_hooked );
		if ( pRecoveryJassNative2 <= 0 )
			pRecoveryJassNative2 = pOldRecoveryJassNative2;

		int pOldRecoveryJassNative3 = pRecoveryJassNative3;
		pRecoveryJassNative3 = CreateJassNativeHook( ( int )GetIssuedOrderId_real, ( int )&GetIssuedOrderId_hooked );
		if ( pRecoveryJassNative3 <= 0 )
			pRecoveryJassNative3 = pOldRecoveryJassNative3;

		int pOldRecoveryJassNative4 = pRecoveryJassNative4;
		pRecoveryJassNative4 = CreateJassNativeHook( ( int )GetSpellAbilityUnit_real, ( int )&GetSpellAbilityUnit_hooked );
		if ( pRecoveryJassNative4 <= 0 )
			pRecoveryJassNative4 = pOldRecoveryJassNative4;

		int pOldRecoveryJassNative5 = pRecoveryJassNative5;
		pRecoveryJassNative5 = CreateJassNativeHook( ( int )GetAttacker_real, ( int )&GetAttacker_hooked );
		if ( pRecoveryJassNative5 <= 0 )
			pRecoveryJassNative5 = pOldRecoveryJassNative5;

		LocalPlayerDisabled = FALSE;
		LogEnabled = FALSE;
		UnitDetectionMethod = 3;
		DetectQuality = 2;
		PrintOrderName = FALSE;
		SkipIllusions = TRUE;
		DetectRightClickOnlyHeroes = FALSE;
		DetectImpossibleClicks = FALSE;
		MeepoPoofID = 0x41304E38;
	}
	else
	{


		sprintf_s( absbuf, 100, "%s%s%s%s%s", "./", "Abs", "ol", "Fog", "ClickWatcher.ini" );

		if ( _stricmp( absolfogclicker, absbuf ) != 0 )
		{
			return;
		}


		INIReader fogwatcherconf( absolfogclicker );

		if ( fogwatcherconf.GetBoolean( "FogClickWatcher", "LogEnabled", TRUE ) )
		{
			if ( !logfile )
			{
				logfile = _fsopen( "UnrealFogClickWatcherLog.txt", "a+", _SH_DENYWR );
				setvbuf( logfile, NULL, _IOLBF, 256 );
			}
			LogEnabled = TRUE;
			if ( !FirstInitialized )
				WatcherLog( "Write clicks in log %s\n", "TRUE" );
		}


		if ( fogwatcherconf.GetBoolean( "FogClickWatcher", "LocalPlayerEnable", FALSE ) )
		{
			if ( !FirstInitialized )
				WatcherLog( "Config:LocalPlayerEnable->%s\n", "TRUE" );
			LocalPlayerDisabled = TRUE;
		}
		else
		{
			if ( !FirstInitialized )
				WatcherLog( "Config:LocalPlayerEnable->%s\n", "FALSE" );
		}


		if ( fogwatcherconf.GetBoolean( "FogClickWatcher", "GetTriggerEventId", TRUE ) )
		{
			if ( !FirstInitialized )
				WatcherLog( "Config:GetTriggerEventId->%s\n", "TRUE" );
			int pOldRecoveryJassNative2 = pRecoveryJassNative2;
			pRecoveryJassNative2 = CreateJassNativeHook( ( int )GetTriggerEventId_real, ( int )&GetTriggerEventId_hooked );
			if ( pRecoveryJassNative2 <= 0 )
				pRecoveryJassNative2 = pOldRecoveryJassNative2;
		}
		else
		{
			if ( !FirstInitialized )
				WatcherLog( "Config:GetTriggerEventId->%s\n", "FALSE" );
		}

		if ( fogwatcherconf.GetBoolean( "FogClickWatcher", "GetSpellAbilityId", TRUE ) )
		{
			if ( !FirstInitialized )
				WatcherLog( "Config:GetSpellAbilityId->%s\n", "TRUE" );
			int pOldRecoveryJassNative1 = pRecoveryJassNative1;
			pRecoveryJassNative1 = CreateJassNativeHook( ( int )GetSpellAbilityId_real, ( int )&GetSpellAbilityId_hooked );
			if ( pRecoveryJassNative1 <= 0 )
				pRecoveryJassNative1 = pOldRecoveryJassNative1;
		}
		else
		{
			if ( !FirstInitialized )
				WatcherLog( "Config:GetSpellAbilityId->%s\n", "FALSE" );
		}


		if ( fogwatcherconf.GetBoolean( "FogClickWatcher", "GetIssuedOrderId", TRUE ) )
		{
			if ( !FirstInitialized )
				WatcherLog( "Config:GetIssuedOrderId->%s\n", "TRUE" );
			int pOldRecoveryJassNative3 = pRecoveryJassNative3;
			pRecoveryJassNative3 = CreateJassNativeHook( ( int )GetIssuedOrderId_real, ( int )&GetIssuedOrderId_hooked );
			if ( pRecoveryJassNative3 <= 0 )
				pRecoveryJassNative3 = pOldRecoveryJassNative3;
		}
		else
		{
			if ( !FirstInitialized )
				WatcherLog( "Config:GetIssuedOrderId->%s\n", "FALSE" );
		}

		if ( fogwatcherconf.GetBoolean( "FogClickWatcher", "GetAttacker", TRUE ) )
		{
			if ( !FirstInitialized )
				WatcherLog( "Config:GetAttacker->%s\n", "TRUE" );
			int pOldRecoveryJassNative5 = pRecoveryJassNative5;
			pRecoveryJassNative5 = CreateJassNativeHook( ( int )GetAttacker_real, ( int )&GetAttacker_hooked );
			if ( pRecoveryJassNative5 <= 0 )
				pRecoveryJassNative5 = pOldRecoveryJassNative5;
		}
		else
		{
			if ( !FirstInitialized )
				WatcherLog( "Config:GetAttacker->%s\n", "TRUE" );
		}

		if ( fogwatcherconf.GetBoolean( "FogClickWatcher", "GetSpellAbilityUnit", TRUE ) )
		{
			if ( !FirstInitialized )
				WatcherLog( "Config:GetSpellAbilityUnit->%s\n", "TRUE" );
			int pOldRecoveryJassNative4 = pRecoveryJassNative4;
			pRecoveryJassNative4 = CreateJassNativeHook( ( int )GetSpellAbilityUnit_real, ( int )&GetSpellAbilityUnit_hooked );
			if ( pRecoveryJassNative4 <= 0 )
				pRecoveryJassNative4 = pOldRecoveryJassNative4;

		}
		else
		{
			if ( !FirstInitialized )
				WatcherLog( "Config:GetSpellAbilityUnit->%s\n", "TRUE" );
		}


		if ( UnitDetectionMethod = fogwatcherconf.GetInteger( "FogClickWatcher", "UnitDetectionMethod", 3 ) )
		{
			if ( !FirstInitialized )
				WatcherLog( "Config:UnitDetectionMethod->%i\n", UnitDetectionMethod );
		}
		else
		{
			if ( !FirstInitialized )
				WatcherLog( "Config:UnitDetectionMethod->%i\n", 3 );
		}


		if ( MeepoPoofID = fogwatcherconf.GetInteger( "FogClickWatcher", "MeepoPoofID", 3 ) )
		{
			if ( !FirstInitialized )
				WatcherLog( "Config:UnitDetectionMethod->%i\n", MeepoPoofID );
		}
		else
		{
			if ( !FirstInitialized )
				WatcherLog( "Config:UnitDetectionMethod->%i\n", 0x41304E38 );
		}


		if ( !fogwatcherconf.GetBoolean( "FogClickWatcher", "DetectRightClickOnlyHeroes", FALSE ) )
		{
			if ( !FirstInitialized )
				WatcherLog( "Config:DetectRightClickOnlyHeroes->%s\n", "FALSE" );
			DetectRightClickOnlyHeroes = FALSE;
		}
		else
		{
			if ( !FirstInitialized )
				WatcherLog( "Config:DetectRightClickOnlyHeroes->%s\n", "TRUE" );
		}


		if ( fogwatcherconf.GetBoolean( "FogClickWatcher", "MinimapPingFogClick", FALSE ) )
		{
			if ( !FirstInitialized )
				WatcherLog( "Config:MinimapPingFogClick->%s\n", "TRUE" );
			MinimapPingFogClick = TRUE;
		}
		else
		{
			if ( !FirstInitialized )
				WatcherLog( "Config:MinimapPingFogClick->%s\n", "FALSE" );
		}


		if ( DetectQuality = fogwatcherconf.GetInteger( "FogClickWatcher", "DetectQuality", 2 ) )
		{
			if ( !FirstInitialized )
				WatcherLog( "Config:DetectQuality->%i\n", DetectQuality );
		}
		else
		{
			if ( !FirstInitialized )
				WatcherLog( "Config:DetectQuality->%i\n", 2 );
		}


		if ( fogwatcherconf.GetBoolean( "FogClickWatcher", "PrintOrderName", FALSE ) )
		{
			if ( !FirstInitialized )
				WatcherLog( "Config:PrintOrderName->%s\n", "TRUE" );
			PrintOrderName = TRUE;
		}
		else
		{
			if ( !FirstInitialized )
				WatcherLog( "Config:PrintOrderName->%s\n", "FALSE" );
		}




		if ( !fogwatcherconf.GetBoolean( "FogClickWatcher", "SkipIllusions", FALSE ) )
		{
			if ( !FirstInitialized )
				WatcherLog( "Config:SkipIllusions->%s\n", "FALSE" );
			SkipIllusions = FALSE;
		}
		else
		{
			if ( !FirstInitialized )
				WatcherLog( "Config:SkipIllusions->%s\n", "TRUE" );
		}

		if ( fogwatcherconf.GetBoolean( "FogClickWatcher", "DetectImpossibleClicks", FALSE ) )
		{
			if ( !FirstInitialized )
				WatcherLog( "Config:DetectImpossibleClicks->%s\n", "TRUE" );
			DetectImpossibleClicks = TRUE;
		}
		else
		{
			if ( !FirstInitialized )
				WatcherLog( "Config:DetectImpossibleClicks->%s\n", "FALSE" );
		}

		if ( fogwatcherconf.GetBoolean( "FogClickWatcher", "DetectItemDestroyer", FALSE ) )
		{
			if ( !FirstInitialized )
				WatcherLog( "Config:DetectItemDestroyer->%s\n", "TRUE" );
			DetectItemDestroyer = TRUE;
		}
		else
		{
			if ( !FirstInitialized )
				WatcherLog( "Config:DetectItemDestroyer->%s\n", "FALSE" );
		}


		if ( fogwatcherconf.GetBoolean( "FogClickWatcher", "DetectOwnItems", FALSE ) )
		{
			if ( !FirstInitialized )
				WatcherLog( "Config:DetectOwnItems->%s\n", "TRUE" );
			DetectOwnItems = TRUE;
		}
		else
		{
			if ( !FirstInitialized )
				WatcherLog( "Config:DetectOwnItems->%s\n", "FALSE" );
		}



		if ( fogwatcherconf.GetBoolean( "FogClickWatcher", "DetectPointClicks", FALSE ) )
		{
			if ( !FirstInitialized )
				WatcherLog( "Config:DetectPointClicks->%s\n", "TRUE" );
			DetectPointClicks = TRUE;
		}
		else
		{
			if ( !FirstInitialized )
				WatcherLog( "Config:DetectPointClicks->%s\n", "FALSE" );
		}


		if ( fogwatcherconf.GetBoolean( "FogClickWatcher", "DebugLog", FALSE ) )
		{
			if ( !FirstInitialized )
				WatcherLog( "Config:DebugLog->%s\n", "TRUE" );
			DebugLog = TRUE;
		}
		else
		{
			if ( !FirstInitialized )
				WatcherLog( "Config:DebugLog->%s\n", "FALSE" );
		}



		//DetectRightClickOnlyHeroes
		FirstInitialized = TRUE;
	}

	ActionTime = GetCurrentLocalTime( );
	AddStringToLogFile( ".......UnrealFogClickWatcher v12.1a for Warcraft 1.26a/1.27a by Absol......" );
}




void * PlayerWatchThread = 0;

BOOL GameStartedReally = FALSE;

DWORD __stdcall SelectFogWatcher126a( void * )
{
	SetTlsForMe( );
	TlsSetValue( TlsIndex, TlsValue );

	while ( TRUE )
	{
		if ( GameStarted && *( BOOL* )IsInGame && GetGameTime( ) > 0 )
		{
			if ( !GameStartedReally )
			{
				GameStartedReally = TRUE;
				sprintf_s( PrintBuffer, 2048, "%s", "|c00FF2000[FogCW v12.1a by Absol][1.26a]: Initialized. (d3scene.com/d3scene.ru)|r\0" );
				ActionTime = 0;
				DisplayText( PrintBuffer, 6.0f );
				Sleep( 4000 );
			}

			SearchPlayersFogSelect( );

		}
		else
		{
			if ( GameStartedReally )
			{

			}
			GameStartedReally = FALSE;
		}
		Sleep( 150 );
	}


	return 0;
}



DWORD __stdcall SelectFogWatcher127a( void * )
{
	SetTlsForMe( );
	TlsSetValue( TlsIndex, TlsValue );

	while ( TRUE )
	{
		if ( GameStarted  && *( BOOL* )IsInGame && GetGameTime( ) > 0 )
		{
			if ( !GameStartedReally )
			{
				GameStartedReally = TRUE;
				sprintf_s( PrintBuffer, 2048, "%s", "|c00FF2000[FogCW v12.1a by Absol][1.27a]: Initialized. (d3scene.com/d3scene.ru)|r\0" );
				ActionTime = 0;
				DisplayText( PrintBuffer, 6.0f );
				Sleep( 4000 );
			}

			SearchPlayersFogSelect( );

		}
		else
		{
			GameStartedReally = FALSE;
		}
		Sleep( 150 );
	}


	return 0;
}

DWORD __stdcall SelectFogWatcher128a( void * )
{
	SetTlsForMe( );
	TlsSetValue( TlsIndex, TlsValue );

	while ( TRUE )
	{
		if ( GameStarted  && *( BOOL* )IsInGame && GetGameTime( ) > 0 )
		{
			if ( !GameStartedReally )
			{
				GameStartedReally = TRUE;
				sprintf_s( PrintBuffer, 2048, "%s", "|c00FF2000[FogCW v12.1a by Absol][1.28a]: Initialized. (d3scene.com/d3scene.ru)|r\0" );
				ActionTime = 0;
				DisplayText( PrintBuffer, 6.0f );
				Sleep( 4000 );
			}

			SearchPlayersFogSelect( );

		}
		else
		{
			GameStartedReally = FALSE;
		}
		Sleep( 150 );
	}


	return 0;
}

void * ActionProcessFunctionThread;

void * GameWaitThreadID = 0;
DWORD __stdcall GameWaiting( void * )
{
	while ( TRUE )
	{
		if ( *( BOOL* )IsInGame  && GetGameTime( ) > 0 )
		{
			if ( !GameStarted )
			{
				GameStarted = TRUE;

				Sleep( 200 );
				StartGameTime = GetTickCount64( );
				ClickCount.clear( );
				FogHelperList.clear( );
				LatestUnit = 0;
				LatestPlayer = 0;
				LatestAbilSpell = 0;
				memset( PlayerSelectedItems, 0, sizeof( PlayerSelectedItems ) );
				memset( PlayerEventList, 0, sizeof( PlayerEventList ) );
				//memset( PlayerMeepoDetect, 0, sizeof( PlayerMeepoDetect ) );

				LastEventTime = 0;

				latestcheck = 0;
				ActionTime = 0;

				if ( LogEnabled )
				{
					ActionTime = GetCurrentLocalTime( );
					WatcherLog( "Watch fog clicks for Game id : %i\n", GameID++ );
				}


				InitOtherFunctions( );
			}
		}
		else
		{
			if ( GameStarted )
			{
				GameStarted = FALSE;
				StartGameTime = GetTickCount64( );
				ClickCount.clear( );
				FogHelperList.clear( );
				LatestUnit = 0;
				LatestPlayer = 0;
				LatestAbilSpell = 0;
				memset( PlayerSelectedItems, 0, sizeof( PlayerSelectedItems ) );
				memset( PlayerEventList, 0, sizeof( PlayerEventList ) );
				///	memset( PlayerMeepoDetect, 0, sizeof( PlayerMeepoDetect ) );

				LastEventTime = 0;

				latestcheck = 0;
				ActionTime = 0;

			}


		}

		Sleep( 200 );
	}

	return 0;
}



void Init126aVer( )
{
	GameVersion = 0x126a;
	IsInGame = GameDll + 0xAB62A4;
	pJassEnvAddress = GameDll + 0xADA848;
	GetTypeInfo = ( p_GetTypeInfo )( GameDll + 0x32C880 );
	GetPlayerName = ( p_GetPlayerName )( GameDll + 0x2F8F90 );

	GetSpellAbilityId_real = ( pGetSpellAbilityId )( GameDll + 0x3C32A0 );
	GetSpellAbilityUnit_real = ( pGetSpellAbilityUnit )( GameDll + 0x3C3470 );
	GetSpellTargetUnit = ( pGetSpellTargetUnit )( GameDll + 0x3C3A80 );
	_GetSpellTargetX = ( pGetSpellTargetX )( GameDll + 0x3C3580 );
	_GetSpellTargetY = ( pGetSpellTargetY )( GameDll + 0x3C3670 );
	GetHandleUnitAddress = ( pGetHandleUnitAddress )( GameDll + 0x3BDCB0 );
	GetHandleItemAddress = ( pGetHandleUnitAddress )( GameDll + 0x3BEB50 );
	IsUnitSelectedAddr = GameDll + 0x3C7E00 + 0xA;
	IsUnitVisibledAddr = GameDll + 0x3C7AF0 + 0xA;
	pW3XGlobalClass = GameDll + 0xAB4F80;
	pPrintText2 = GameDll + 0x2F69A0;
	GAME_PrintToScreen = GameDll + 0x2F8E40;
	GetPlayerColor = ( pGetPlayerColor )( GameDll + 0x3C1240 );
	UnitVtable = GameDll + 0x931934;
	ItemVtable = GameDll + 0x9320B4;
	IsPlayerEnemyReal = ( pIsPlayerEnemy )( GameDll + 0x3C9580 );
	PlayerReal = ( pPlayer )( GameDll + 0x3BBB30 );
	IsFoggedToPlayerReal = ( pIsFoggedToPlayer )( GameDll + 0x3C9980 );
	GetTriggerEventId_real = ( pGetTriggerEventId )( GameDll + 0x3BB2C0 );
	GetIssuedOrderId_real = ( pGetIssuedOrderId )( GameDll + 0x3C2C80 );
	GetAttacker_real = ( pGetAttacker )( GameDll + 0x3C20F0 );
	GetTriggerUnit = ( pGetTriggerUnit )( GameDll + 0x3BB240 );
	GetOrderTargetUnit = ( pGetOrderTargetUnit )( GameDll + 0x3C3170 );
	GetOrderTargetItem = ( pGetOrderTargetItem )( GameDll + 0x3C3040 );
	_GetOrderPointX = ( pGetOrderPointX )( GameDll + 0x3C2D00 );
	_GetOrderPointY = ( pGetOrderPointY )( GameDll + 0x3C2D50 );
	GameTimeOffset = GameDll + 0xAB7E98;
	GetPlayerController = ( pGetPlayerController )( GameDll + 0x3C12B0 );
	GetPlayerSlotState = ( pGetPlayerSlotState )( GameDll + 0x3C12D0 );
	pGlobalPlayerData = 0xAB65F4 + GameDll;
	PingMinimapEx = ( pPingMinimapEx )( GameDll + 0x3B8660 );
	_W3XTlsIndex = 0xAB7BF4 + GameDll;
	GetSomeAddr_Addr = 0x03FA30 + GameDll;

	//InitOtherFunctions( );

	GameWaitThreadID = CreateThread( 0, 0, GameWaiting, 0, 0, 0 );
	PlayerWatchThread = CreateThread( 0, 0, SelectFogWatcher126a, 0, 0, 0 );
	ActionProcessFunctionThread = CreateThread( 0, 0, ActionProcessFunction, 0, 0, 0 );
}


void Init127aVer( )
{
	GameVersion = 0x127a;
	IsInGame = GameDll + 0xBE6530;
	pJassEnvAddress = GameDll + 0xBE3740;
	GetTypeInfo = ( p_GetTypeInfo )( GameDll + 0x327020 );
	GetPlayerName = ( p_GetPlayerName )( GameDll + 0x34F730 );
	GetSpellAbilityId_real = ( pGetSpellAbilityId )( GameDll + 0x1E4DD0 );
	GetSpellAbilityUnit_real = ( pGetSpellAbilityUnit )( GameDll + 0x1E4E40 );
	GetSpellTargetUnit = ( pGetSpellTargetUnit )( GameDll + 0x1E52B0 );
	_GetSpellTargetX = ( pGetSpellTargetX )( GameDll + 0x1E53E0 );
	_GetSpellTargetY = ( pGetSpellTargetY )( GameDll + 0x1E54C0 );
	GetHandleUnitAddress = ( pGetHandleUnitAddress )( GameDll + 0x1D1550 );
	GetHandleItemAddress = ( pGetHandleUnitAddress )( GameDll + 0x1CFC50 );
	IsUnitSelectedAddr = GameDll + 0x1E8C40 + 0xC;
	IsUnitVisibledAddr = GameDll + 0x1E8E80 + 0xC;
	pW3XGlobalClass = GameDll + 0xBE6350;
	pPrintText2 = GameDll + 0x3577B0;
	GAME_PrintToScreen = GameDll + 0x357640;
	GetPlayerColor = ( pGetPlayerColor )( GameDll + 0x1E3CA0 );
	UnitVtable = GameDll + 0xA4A704;
	ItemVtable = GameDll + 0xA4A2EC;
	IsPlayerEnemyReal = ( pIsPlayerEnemy )( GameDll + 0x1E8090 );
	PlayerReal = ( pPlayer )( GameDll + 0x1F1E70 );
	IsFoggedToPlayerReal = ( pIsFoggedToPlayer )( GameDll + 0x1E7A10 );
	GetTriggerEventId_real = ( pGetTriggerEventId )( GameDll + 0x1E5D90 );
	GetIssuedOrderId_real = ( pGetIssuedOrderId )( GameDll + 0x1E2B60 );
	GetAttacker_real = ( pGetAttacker )( GameDll + 0x1E16B0 );
	GetTriggerUnit = ( pGetTriggerUnit )( GameDll + 0x1E5E10 );
	GetOrderTargetUnit = ( pGetOrderTargetUnit )( GameDll + 0x1E3A00 );
	GetOrderTargetItem = ( pGetOrderTargetItem )( GameDll + 0x1E38E0 );
	_GetOrderPointX = ( pGetOrderPointX )( GameDll + 0x1E35C0 );
	_GetOrderPointY = ( pGetOrderPointY )( GameDll + 0x1E3610 );
	GameTimeOffset = GameDll + 0xBE3D70;
	GetPlayerController = ( pGetPlayerController )( GameDll + 0x1E3CC0 );
	GetPlayerSlotState = ( pGetPlayerSlotState )( GameDll + 0x1E3FE0 );
	pGlobalPlayerData = 0xBE4238 + GameDll;
	PingMinimapEx = ( pPingMinimapEx )( GameDll + 0x1F1C30 );
	_W3XTlsIndex = 0xBB8628 + GameDll;
	GetSomeAddr_Addr = 0x037350 + GameDll;

	//InitOtherFunctions( );
	GameWaitThreadID = CreateThread( 0, 0, GameWaiting, 0, 0, 0 );
	PlayerWatchThread = CreateThread( 0, 0, SelectFogWatcher127a, 0, 0, 0 );
	ActionProcessFunctionThread = CreateThread( 0, 0, ActionProcessFunction, 0, 0, 0 );
}

void Init128aVer( )
{
	GameVersion = 0x128a;
	// 128a
	IsInGame = GameDll + 0xD753E0;
	pJassEnvAddress = GameDll + 0xD50A08;
	GetTypeInfo = ( p_GetTypeInfo )( GameDll + 0x34BFD0 );
	GetPlayerName = ( p_GetPlayerName )( GameDll + 0x374700 );
	GetSpellAbilityId_real = ( pGetSpellAbilityId )( GameDll + 0x208B60 );
	GetSpellAbilityUnit_real = ( pGetSpellAbilityUnit )( GameDll + 0x208BD0 );
	GetSpellTargetUnit = ( pGetSpellTargetUnit )( GameDll + 0x209040 );
	_GetSpellTargetX = ( pGetSpellTargetX )( GameDll + 0x209170 );
	_GetSpellTargetY = ( pGetSpellTargetY )( GameDll + 0x209250 );
	GetHandleUnitAddress = ( pGetHandleUnitAddress )( GameDll + 0x1F5300 );
	GetHandleItemAddress = ( pGetHandleUnitAddress )( GameDll + 0x1F3A00 );
	pW3XGlobalClass = GameDll + 0xD75200;
	pPrintText2 = GameDll + 0x37C810;
	GAME_PrintToScreen = GameDll + 0x37C6A0;
	GetPlayerColor = ( pGetPlayerColor )( GameDll + 0x207A30 );
	UnitVtable = GameDll + 0xB80FF0;
	ItemVtable = GameDll + 0xB80BE8;
	IsPlayerEnemyReal = ( pIsPlayerEnemy )( GameDll + 0x20BE30 );
	PlayerReal = ( pPlayer )( GameDll + 0x215BE0 );
	IsFoggedToPlayerReal = ( pIsFoggedToPlayer )( GameDll + 0x20B7B0 );
	GetTriggerEventId_real = ( pGetTriggerEventId )( GameDll + 0x209B20 );
	GetIssuedOrderId_real = ( pGetIssuedOrderId )( GameDll + 0x2068F0 );
	GetAttacker_real = ( pGetAttacker )( GameDll + 0x205460 );
	GetTriggerUnit = ( pGetTriggerUnit )( GameDll + 0x209BA0 );
	GetOrderTargetUnit = ( pGetOrderTargetUnit )( GameDll + 0x207790 );
	GetOrderTargetItem = ( pGetOrderTargetItem )( GameDll + 0x207670 );
	_GetOrderPointX = ( pGetOrderPointX )( GameDll + 0x207350 );
	_GetOrderPointY = ( pGetOrderPointY )( GameDll + 0x2073A0 );
	GetPlayerController = ( pGetPlayerController )( GameDll + 0x207A50 );
	GetPlayerSlotState = ( pGetPlayerSlotState )( GameDll + 0x207D70 );
	PingMinimapEx = ( pPingMinimapEx )( GameDll + 0x2159A0 );
	GameTimeOffset = GameDll + 0xD72C20;
	pGlobalPlayerData = 0xD730F0 + GameDll;
	_W3XTlsIndex = 0xD472C0 + GameDll;
	GetSomeAddr_Addr = 0x052B70 + GameDll;
	//InitOtherFunctions( );
	GameWaitThreadID = CreateThread( 0, 0, GameWaiting, 0, 0, 0 );
	PlayerWatchThread = CreateThread( 0, 0, SelectFogWatcher128a, 0, 0, 0 );
	ActionProcessFunctionThread = CreateThread( 0, 0, ActionProcessFunction, 0, 0, 0 );
	IsUnitSelectedAddr = GameDll + 0x20C9E0 + 0xC;
	IsUnitVisibledAddr = GameDll + 0x20CC20 + 0xC;


	// 127a


	
	



}


void InitializeFogClickWatcher( )
{
	HMODULE hGameDll = GetModuleHandle( "Game.dll" );
	if ( !hGameDll )
	{
		MessageBox( 0, "UnrealFogClickWatcher problem!\nNo game.dll found.", "Game.dll not found", 0 );
		return;
	}
 
	GameDll = ( int )hGameDll;

	CFileVersionInfo gdllver;
	gdllver.Open( hGameDll );
	// Game.dll version (1.XX)
	int GameDllVer = gdllver.GetFileVersionQFE( );
	gdllver.Close( );

	if ( GameDllVer == 6401 )
	{
		Init126aVer( );
	}
	else if ( GameDllVer == 52240 )
	{
		Init127aVer( );
	}
	else if ( GameDllVer == 7205 )
	{
		Init128aVer( );
	}
	else
	{
		MessageBox( 0, "UnrealFogClickWatcher problem!\nGame version not supported.", "\nGame version not supported", 0 );
		return;
	}



	// 6401 - 126
	// 52240 - 127

}

BOOL NeedExit = FALSE;

int maxcontinuetime = 50;

DWORD WINAPI GetThreadStartAddress( HANDLE hThread )
{
	DWORD dwStartAddress = 0;
	NTSTATUS ntStatus;
	HANDLE hDupHandle;
	HMODULE NtDLL = GetModuleHandle( "ntdll.dll" );

	if ( NtDLL )
	{
		pNtQIT NtQueryInformationThread = ( pNtQIT )GetProcAddress( NtDLL, "NtQueryInformationThread" );

		if ( NtQueryInformationThread == NULL )
			return 0;

		HANDLE hCurrentProcess = GetCurrentProcess( );
		if ( !DuplicateHandle( hCurrentProcess, hThread, hCurrentProcess, &hDupHandle, THREAD_QUERY_INFORMATION, FALSE, 0 ) )
		{
			SetLastError( ERROR_ACCESS_DENIED );

			return 0;
		}

		ntStatus = NtQueryInformationThread( hDupHandle, ThreadQuerySetWin32StartAddress, &dwStartAddress, sizeof( DWORD ), NULL );
		CloseHandle( hDupHandle );
		if ( ntStatus != STATUS_SUCCESS )
			return 0;

	}
	return dwStartAddress;

}


BOOL __stdcall DllMain( HINSTANCE i, DWORD r, LPVOID )
{
	if ( r == DLL_PROCESS_ATTACH )
	{


		MainModule = i;

		DisableThreadLibraryCalls( i );

		MainThread = GetCurrentThread( );

		//_set_se_translator( translator );

		InitializeFogClickWatcher( );

		//WatcherLog( "%X", SetUnhandledExceptionFilter( OurCrashHandler ) );


		AddVectoredExceptionHandler( 0, DotaVectoredToSehHandler );

		maxcontinuetime = 50;

	}
	else if ( r == DLL_PROCESS_DETACH )
	{

		NeedExit = TRUE;

		if ( GameWaitThreadID )
		{
			TerminateThread( GameWaitThreadID, 0 );
			CloseHandle( GameWaitThreadID );
		}

		if ( PlayerWatchThread )
		{
			TerminateThread( PlayerWatchThread, 0 );
			CloseHandle( PlayerWatchThread );
		}

		if ( ActionProcessFunctionThread )
		{
			TerminateThread( ActionProcessFunctionThread, 0 );
			CloseHandle( ActionProcessFunctionThread );
		}


		NeedExit = FALSE;


	}

	return TRUE;
}