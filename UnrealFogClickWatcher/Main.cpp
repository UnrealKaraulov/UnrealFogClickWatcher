#define _WIN32_WINNT 0x0501 
#define WINVER 0x0501 
#define NTDDI_VERSION 0x05010000
//#define BOTDEBUG
#define WIN32_LEAN_AND_MEAN
#define PSAPI_VERSION 1

#include <Windows.h>
#include <string>
#include <vector>
#include <time.h>
#include <eh.h>
#include <string>

#include <filesystem>
#include <TlHelp32.h>

#include <fstream>

#include "verinfo.h"
#include "IniReader.h"
#include "IniWriter.h"


std::string detectorConfigPath = "./UnrealFogClickWatcher.ini";

std::string logFileName = "./UnrealFogClickWatcher.log";

#define IsKeyPressed(CODE) (GetAsyncKeyState(CODE) & 0x8000) > 0

typedef LONG(WINAPI* pNtQIT)(HANDLE, LONG, PVOID, ULONG, PULONG);

#define STATUS_SUCCESS    ((NTSTATUS)0x00000000L)

#define ThreadQuerySetWin32StartAddress 9

BOOL IsReplayFound = FALSE;

BOOL WatcherNewDebugEnabled = TRUE;

char PrintBuffer[2048];

long long CurTickCount = 0;

long long LastGameUpdate = 0;

long long LastActionProcess = 0;

long long LastClickWatcher = 0;

long long StartGameTime = 0;

int LoggingType = 0;

void WatcherLog(const char* format, ...)
{
	if (LoggingType == -1)
		return;
	char buffer[1024];
	va_list args;
	va_start(args, format);
	vsprintf_s(buffer, format, args);
	std::ofstream outfile(logFileName, std::ios_base::app);
	if (!outfile.bad() && outfile.is_open())
		outfile << buffer;
	outfile.close();
	va_end(args);
}

void WatcherLogForce(const char* format, ...)
{
	char buffer[1024];
	va_list args;
	va_start(args, format);
	vsprintf_s(buffer, format, args);
	std::ofstream outfile(logFileName, std::ios_base::app);
	if (!outfile.bad() && outfile.is_open())
		outfile << buffer;
	outfile.close();
	va_end(args);
}


union DWFP
{
	unsigned int dw;
	float fl;
};

const int MAX_PLAYERS = 12;

int GameID = 0;

int GameDllVer = 0;

unsigned char* GameDll = 0;

HMODULE MainModule = 0;

const char* MapFileName = "";

BOOL GameStarted = FALSE;

unsigned char* pJassEnvAddress = 0;

unsigned char* pW3XGlobalClass = 0;

unsigned char* pPrintText2 = 0;

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

BOOL DebugState = FALSE;

BOOL DisplayFalse = TRUE;

BOOL ICCUP_DOTA_SUPPORT = FALSE;

BOOL ReplayPauseOnDetect = TRUE;

int DetectQuality = 2;

BOOL ExceptionFilterHooked = FALSE;

float DefaultCircleScale = 72.0f;

struct UnitSelectedStruct
{
	int PlayerNum;
	int UnitAddr;
	int SelectCount;
	long long LatestTime;
	BOOL initialVisibled;
};

struct FogHelper
{
	int UnitAddr;
	long long LatestTime;
	BOOL FogState[MAX_PLAYERS][3];
};

std::vector<UnitSelectedStruct> UnitClickList;

std::vector<FogHelper> FogHelperList;

DWORD MainThread = NULL;

unsigned int GetCurrentLocalTime()
{
	time_t rawtime;
	struct tm timeinfo;

	time(&rawtime);
	localtime_s(&timeinfo, &rawtime);
	return (timeinfo.tm_hour * 3600 + timeinfo.tm_min * 60 + timeinfo.tm_sec) * 1000;
}

typedef unsigned int(__cdecl* pGetPlayerColor)(int whichPlayer);
pGetPlayerColor GetPlayerColor;

typedef BOOL(__cdecl* pIsReplayMode)();
pIsReplayMode IsReplayMode;

typedef LRESULT(__fastcall* pWarcraftRealWNDProc)(HWND hWnd, unsigned int Msg, WPARAM wParam, LPARAM lParam);
pWarcraftRealWNDProc WarcraftRealWNDProc;

void SendPause()
{
	if (IsReplayFound && ReplayPauseOnDetect)
	{
		WarcraftRealWNDProc(0, WM_KEYDOWN, VK_PAUSE, 0);
		WarcraftRealWNDProc(0, WM_KEYUP, VK_PAUSE, 0);
	}
}

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


const char* GetPlayerColorString(int player)
{
	if (player <= 0)
	{
		return "|c00AAAAAA";
	}

	unsigned int c = GetPlayerColor(player);

	if (c == PLAYER_COLOR_RED)
		return "|c00FF0202";
	else if (c == PLAYER_COLOR_BLUE)
		return "|c000041FF";
	else if (c == PLAYER_COLOR_CYAN)
		return "|c001BE5B8";
	else if (c == PLAYER_COLOR_PURPLE)
		return "|c00530080";
	else if (c == PLAYER_COLOR_YELLOW)
		return "|c00FFFC00";
	else if (c == PLAYER_COLOR_ORANGE)
		return "|c00FE890D";
	else if (c == PLAYER_COLOR_GREEN)
		return "|c001FBF00";
	else if (c == PLAYER_COLOR_PINK)
		return "|c00E45AAF";
	else if (c == PLAYER_COLOR_LIGHT_GRAY)
		return "|c00949596";
	else if (c == PLAYER_COLOR_LIGHT_BLUE)
		return "|c007DBEF1";
	else if (c == PLAYER_COLOR_AQUA)
		return "|c000F6145";
	else if (c == PLAYER_COLOR_BROWN)
		return "|c004D2903";

	return "|c004B4B4B";
}

unsigned int GetPlayerColorUINT(int player)
{
	unsigned int c = GetPlayerColor(player);
	if (player <= 0)
	{
		return 0xffAAAAAA;
	}

	if (c == PLAYER_COLOR_RED)
		return 0xffFF0202;
	else if (c == PLAYER_COLOR_BLUE)
		return 0xff0041FF;
	else if (c == PLAYER_COLOR_CYAN)
		return 0xff1BE5B8;
	else if (c == PLAYER_COLOR_PURPLE)
		return 0xff530080;
	else if (c == PLAYER_COLOR_YELLOW)
		return 0xffFFFC00;
	else if (c == PLAYER_COLOR_ORANGE)
		return 0xffFE890D;
	else if (c == PLAYER_COLOR_GREEN)
		return 0xff1FBF00;
	else if (c == PLAYER_COLOR_PINK)
		return 0xffE45AAF;
	else if (c == PLAYER_COLOR_LIGHT_GRAY)
		return 0xff949596;
	else if (c == PLAYER_COLOR_LIGHT_BLUE)
		return 0xff7DBEF1;
	else if (c == PLAYER_COLOR_AQUA)
		return 0xff0F6145;
	else if (c == PLAYER_COLOR_BROWN)
		return 0xff4D2903;
	return 0xff4B4B4B;
}

const char* DefaultString = "DefaultString\0\xa..........................................................................................................";

int GetObjectClassID(int unit_or_item_addr)
{
	if (unit_or_item_addr)
		return *(int*)(unit_or_item_addr + 0x30);
	return 0;
}

int GetUnitOwnerSlot(int unitaddr)
{
	if (unitaddr)
		return *(int*)(unitaddr + 88);
	return -1;
}

unsigned char* pGlobalPlayerData;

void* GetGlobalPlayerData()
{
	if (*(int*)(pGlobalPlayerData) > 0)
	{
		return (void*)*(int*)(pGlobalPlayerData);
	}
	return 0;
}

int GetPlayerByNumber(int number)
{
	void* arg1 = GetGlobalPlayerData();
	int result = 0;
	if (arg1 != 0)
	{
		result = (int)arg1 + (number * 4) + 0x58;

		if (result)
		{
			result = *(int*)result;
		}
		else
		{
			return 0;
		}
	}
	return result;
}

unsigned char* _GameTimeOffset = 0;

long long CurGameTime = 0;

long long GetGameTime()
{
	long long tmpGameTime = (long long)*(unsigned int*)_GameTimeOffset;

	if (tmpGameTime == 0 || tmpGameTime == -1)
		return (long long)GetTickCount();

	return tmpGameTime;
}


BOOL PrintOrderName = FALSE;

std::string ConvertIdToString(int id)
{
	if (!PrintOrderName)
	{
		return "-ORDER-";
	}

	if (id == 0xD000F) { return "-ATTACK-"; }
	if (id == 0xD0010) { return "-attackground-"; }
	if (id == 0xD0011) { return "-attackonce-"; }
	if (id == 0xD01F2) { return "-awaken-"; }
	if (id == 0xD0012 || id == 0xD0003) { return "-MOVE-"; }
	if (id == 0xD0014) { return "-AImove-"; }
	if (id == 0xD0016) { return "-patrol-"; }
	if (id == 0xD0019) { return "-holdposition-"; }
	if (id == 0xD001A) { return "-build-"; }
	if (id == 0xD001B) { return "-humanbuild-"; }
	if (id == 0xD001C) { return "-orcbuild-"; }
	if (id == 0xD001D) { return "-nightelfbuild-"; }
	if (id == 0xD001E) { return "-undeadbuild-"; }
	if (id == 0xD01F3) { return "-nagabuild-"; }
	if (id == 0xD001F) { return "-resumebuild-"; }
	if (id == 0xD0021) { return "-dropitem-"; }
	if (id == 0xD004B) { return "-board-"; }
	if (id == 0xD002F) { return "-detectaoe-"; }
	if (id == 0xD000D) { return "-getitem-"; }
	if (id == 0xD0032) { return "-harvest-"; }
	if (id == 0xD0035) { return "-autoharvestgold-"; }
	if (id == 0xD0036) { return "-autoharvestlumber-"; }
	if (id == 0xD0031) { return "-resumeharvesting-"; }
	if (id == 0xD0034) { return "-returnresources-"; }
	if (id == 0xD004C) { return "-forceboard-"; }
	if (id == 0xD004E) { return "-load-"; }
	if (id == 0xD004F) { return "-unload-"; }
	if (id == 0xD0050) { return "-unloadall-"; }
	if (id == 0xD0051) { return "-unloadallinstant-"; }
	if (id == 0xD0052) { return "-loadcorpse-"; }
	if (id == 0xD0056) { return "-unloadallcorpses-"; }
	if (id == 0xD0055) { return "-loadcorpseinstant-"; }
	if (id == 0xD01F5) { return "-mount-"; }
	if (id == 0xD01F6) { return "-dismount-"; }
	if (id == 0xD0037) { return "-neutraldetectaoe-"; }
	if (id == 0xD00BD) { return "-recharge-"; }
	if (id == 0xD0038) { return "-repair-"; }
	if (id == 0xD0039) { return "-repairon-"; }
	if (id == 0xD003A) { return "-repairoff-"; }
	if (id == 0xD0047) { return "-revive-"; }
	if (id == 0xD0048) { return "-selfdestruct-"; }
	if (id == 0xD0049) { return "-selfdestructon-"; }
	if (id == 0xD004A) { return "-selfdestructoff-"; }
	if (id == 0xD000C) { return "-setrally-"; }
	if (id == 0xD0004) { return "-stop-"; }
	if (id == 0xD01F9) { return "-cloudoffog-"; }
	if (id == 0xD01FA) { return "-controlmagic-"; }
	if (id == 0xD0057) { return "-defend-"; }
	if (id == 0xD0058) { return "-undefend-"; }
	if (id == 0xD0059) { return "-dispel-"; }
	if (id == 0xD005C) { return "-flare-"; }
	if (id == 0xD005F) { return "-heal-"; }
	if (id == 0xD0060) { return "-healon-"; }
	if (id == 0xD0061) { return "-healoff-"; }
	if (id == 0xD0062) { return "-innerfire-"; }
	if (id == 0xD0063) { return "-innerfireon-"; }
	if (id == 0xD0064) { return "-innerfireoff-"; }
	if (id == 0xD0065) { return "-invisibility-"; }
	if (id == 0xD01FE) { return "-magicdefense-"; }
	if (id == 0xD01FF) { return "-magicundefense-"; }
	if (id == 0xD0200) { return "-magicleash-"; }
	if (id == 0xD0067) { return "-militiaconvert-"; }
	if (id == 0xD02AB) { return "-militiaunconvert-"; }
	if (id == 0xD0068) { return "-militia-"; }
	if (id == 0xD0069) { return "-militiaoff-"; }
	if (id == 0xD0201) { return "-phoenixfire-"; }
	if (id == 0xD0202) { return "-phoenixmorph-"; }
	if (id == 0xD006A) { return "-polymorph-"; }
	if (id == 0xD006B) { return "-slow-"; }
	if (id == 0xD006C) { return "-slowon-"; }
	if (id == 0xD006D) { return "-slowoff-"; }
	if (id == 0xD0203) { return "-spellsteal-"; }
	if (id == 0xD0204) { return "-spellstealon-"; }
	if (id == 0xD0205) { return "-spellstealoff-"; }
	if (id == 0xD006F) { return "-tankdroppilot-"; }
	if (id == 0xD0070) { return "-tankloadpilot-"; }
	if (id == 0xD0071) { return "-tankpilot-"; }
	if (id == 0xD0072) { return "-townbellon-"; }
	if (id == 0xD0073) { return "-townbelloff-"; }
	if (id == 0xD0076) { return "-avatar-"; }
	if (id == 0xD0077) { return "-unavatar-"; }
	if (id == 0xD0206) { return "-banish-"; }
	if (id == 0xD0079) { return "-blizzard-"; }
	if (id == 0xD007A) { return "-divineshield-"; }
	if (id == 0xD007B) { return "-undivineshield-"; }
	if (id == 0xD0208) { return "-flamestrike-"; }
	if (id == 0xD007C) { return "-holybolt-"; }
	if (id == 0xD026D) { return "-manashieldon-"; }
	if (id == 0xD026E) { return "-manashieldoff-"; }
	if (id == 0xD007D) { return "-massteleport-"; }
	if (id == 0xD0209) { return "-summonphoenix-"; }
	if (id == 0xD007E) { return "-resurrection-"; }
	if (id == 0xD007F) { return "-thunderbolt-"; }
	if (id == 0xD0080) { return "-thunderclap-"; }
	if (id == 0xD0081) { return "-waterelemental-"; }
	if (id == 0xD020A) { return "-ancestralspirit-"; }
	if (id == 0xD020B) { return "-ancestralspirittarget-"; }
	if (id == 0xD0083) { return "-battlestations-"; }
	if (id == 0xD0084) { return "-berserk-"; }
	if (id == 0xD0085) { return "-bloodlust-"; }
	if (id == 0xD0086) { return "-bloodluston-"; }
	if (id == 0xD0087) { return "-bloodlustoff-"; }
	if (id == 0xD020D) { return "-corporealform-"; }
	if (id == 0xD020E) { return "-uncorporealform-"; }
	if (id == 0xD0088) { return "-devour-"; }
	if (id == 0xD020F) { return "-disenchant-"; }
	if (id == 0xD008A) { return "-ensnare-"; }
	if (id == 0xD008B) { return "-ensnareon-"; }
	if (id == 0xD008C) { return "-ensnareoff-"; }
	if (id == 0xD0210) { return "-etherealform-"; }
	if (id == 0xD0211) { return "-unetherealform-"; }
	if (id == 0xD0089) { return "-evileye-"; }
	if (id == 0xD008D) { return "-healingward-"; }
	if (id == 0xD008E) { return "-lightningshield-"; }
	if (id == 0xD008F) { return "-purge-"; }
	if (id == 0xD0213) { return "-spiritlink-"; }
	if (id == 0xD0091) { return "-standdown-"; }
	if (id == 0xD0092) { return "-stasistrap-"; }
	if (id == 0xD0214) { return "-unstableconcoction-"; }
	if (id == 0xD0097) { return "-chainlightning-"; }
	if (id == 0xD0099) { return "-earthquake-"; }
	if (id == 0xD009A) { return "-farsight-"; }
	if (id == 0xD0215) { return "-healingwave-"; }
	if (id == 0xD0216) { return "-hex-"; }
	if (id == 0xD009B) { return "-mirrorimage-"; }
	if (id == 0xD009D) { return "-shockwave-"; }
	if (id == 0xD009E) { return "-spiritwolf-"; }
	if (id == 0xD009F) { return "-stomp-"; }
	if (id == 0xD0217) { return "-voodoo-"; }
	if (id == 0xD0218) { return "-ward-"; }
	if (id == 0xD00A0) { return "-whirlwind-"; }
	if (id == 0xD00A1) { return "-windwalk-"; }
	if (id == 0xD00A2) { return "-unwindwalk-"; }
	if (id == 0xD00A3) { return "-ambush-"; }
	if (id == 0xD00A4) { return "-autodispel-"; }
	if (id == 0xD00A5) { return "-autodispelon-"; }
	if (id == 0xD00A6) { return "-autodispeloff-"; }
	if (id == 0xD00A7) { return "-barkskin-"; }
	if (id == 0xD00A8) { return "-barkskinon-"; }
	if (id == 0xD00A9) { return "-barkskinoff-"; }
	if (id == 0xD00AA) { return "-bearform-"; }
	if (id == 0xD00AB) { return "-unbearform-"; }
	if (id == 0xD00AC) { return "-corrosivebreath-"; }
	if (id == 0xD00AE) { return "-loadarcher-"; }
	if (id == 0xD00AF) { return "-mounthippogryph-"; }
	if (id == 0xD021C) { return "-coupleinstant-"; }
	if (id == 0xD021B) { return "-coupletarget-"; }
	if (id == 0xD00B0) { return "-cyclone-"; }
	if (id == 0xD021D) { return "-decouple-"; }
	if (id == 0xD00B1) { return "-detonate-"; }
	if (id == 0xD00B2) { return "-eattree-"; }
	if (id == 0xD00B3) { return "-entangle-"; }
	if (id == 0xD00B4) { return "-entangleinstant-"; }
	if (id == 0xD0219) { return "-autoentangle-"; }
	if (id == 0xD021A) { return "-autoentangleinstant-"; }
	if (id == 0xD00B5) { return "-faeriefire-"; }
	if (id == 0xD00B6) { return "-faeriefireon-"; }
	if (id == 0xD00B7) { return "-faeriefireoff-"; }
	if (id == 0xD021F) { return "-grabtree-"; }
	if (id == 0xD0220) { return "-manaflareon-"; }
	if (id == 0xD0221) { return "-manaflareoff-"; }
	if (id == 0xD0222) { return "-phaseshift-"; }
	if (id == 0xD0223) { return "-phaseshifton-"; }
	if (id == 0xD0224) { return "-phaseshiftoff-"; }
	if (id == 0xD0225) { return "-phaseshiftinstant-"; }
	if (id == 0xD00BB) { return "-ravenform-"; }
	if (id == 0xD00BC) { return "-unravenform-"; }
	if (id == 0xD00BE) { return "-rechargeon-"; }
	if (id == 0xD00BF) { return "-rechargeoff-"; }
	if (id == 0xD00C0) { return "-rejuvination-"; }
	if (id == 0xD00C1) { return "-renew-"; }
	if (id == 0xD00C2) { return "-renewon-"; }
	if (id == 0xD00C3) { return "-renewoff-"; }
	if (id == 0xD00C4) { return "-roar-"; }
	if (id == 0xD00C5) { return "-root-"; }
	if (id == 0xD00C6) { return "-unroot-"; }
	if (id == 0xD00D6) { return "-sentinel-"; }
	if (id == 0xD0228) { return "-taunt-"; }
	if (id == 0xD0229) { return "-vengeance-"; }
	if (id == 0xD022A) { return "-vengeanceon-"; }
	if (id == 0xD022B) { return "-vengeanceoff-"; }
	if (id == 0xD022C) { return "-vengeanceinstant-"; }
	if (id == 0xD00F6) { return "-wispharvest-"; }
	if (id == 0xD022D) { return "-blink-"; }
	if (id == 0xD00CB) { return "-entanglingroots-"; }
	if (id == 0xD022E) { return "-fanofknives-"; }
	if (id == 0xD00CD) { return "-flamingarrowstarg-"; }
	if (id == 0xD00CE) { return "-flamingarrows-"; }
	if (id == 0xD00CF) { return "-unflamingarrows-"; }
	if (id == 0xD00D0) { return "-forceofnature-"; }
	if (id == 0xD00D1) { return "-immolation-"; }
	if (id == 0xD00D2) { return "-unimmolation-"; }
	if (id == 0xD00D3) { return "-manaburn-"; }
	if (id == 0xD00D4) { return "-metamorphosis-"; }
	if (id == 0xD00D5) { return "-scout-"; }
	if (id == 0xD022F) { return "-shadowstrike-"; }
	if (id == 0xD0230) { return "-spiritofvengeance-"; }
	if (id == 0xD00D7) { return "-starfall-"; }
	if (id == 0xD00D8) { return "-tranquility-"; }
	if (id == 0xD0231) { return "-absorb-"; }
	if (id == 0xD00D9) { return "-acolyteharvest-"; }
	if (id == 0xD00DA) { return "-antimagicshell-"; }
	if (id == 0xD0233) { return "-avengerform-"; }
	if (id == 0xD0234) { return "-unavengerform-"; }
	if (id == 0xD00DB) { return "-blight-"; }
	if (id == 0xD0235) { return "-burrow-"; }
	if (id == 0xD0236) { return "-unburrow-"; }
	if (id == 0xD00DC) { return "-cannibalize-"; }
	if (id == 0xD00DD) { return "-cripple-"; }
	if (id == 0xD00DE) { return "-curse-"; }
	if (id == 0xD00DF) { return "-curseon-"; }
	if (id == 0xD00E0) { return "-curseoff-"; }
	if (id == 0xD0238) { return "-devourmagic-"; }
	if (id == 0xD023B) { return "-flamingattacktarg-"; }
	if (id == 0xD023C) { return "-flamingattack-"; }
	if (id == 0xD023D) { return "-unflamingattack-"; }
	if (id == 0xD00E3) { return "-freezingbreath-"; }
	if (id == 0xD00E4) { return "-possession-"; }
	if (id == 0xD00E5) { return "-raisedead-"; }
	if (id == 0xD00E6) { return "-raisedeadon-"; }
	if (id == 0xD00E7) { return "-raisedeadoff-"; }
	if (id == 0xD00E8) { return "-instant-"; }
	if (id == 0xD023E) { return "-replenish-"; }
	if (id == 0xD023F) { return "-replenishon-"; }
	if (id == 0xD0240) { return "-replenishoff-"; }
	if (id == 0xD0241) { return "-replenishlife-"; }
	if (id == 0xD0242) { return "-replenishlifeon-"; }
	if (id == 0xD0243) { return "-replenishlifeoff-"; }
	if (id == 0xD0244) { return "-replenishmana-"; }
	if (id == 0xD0245) { return "-replenishmanaon-"; }
	if (id == 0xD0246) { return "-replenishmanaoff-"; }
	if (id == 0xD00E9) { return "-requestsacrifice-"; }
	if (id == 0xD00EA) { return "-restoration-"; }
	if (id == 0xD00EB) { return "-restorationon-"; }
	if (id == 0xD00EC) { return "-restorationoff-"; }
	if (id == 0xD00ED) { return "-sacrifice-"; }
	if (id == 0xD00EE) { return "-stoneform-"; }
	if (id == 0xD00EF) { return "-unstoneform-"; }
	if (id == 0xD00F1) { return "-unholyfrenzy-"; }
	if (id == 0xD00F2) { return "-unsummon-"; }
	if (id == 0xD00F3) { return "-web-"; }
	if (id == 0xD00F4) { return "-webon-"; }
	if (id == 0xD00F5) { return "-weboff-"; }
	if (id == 0xD00F9) { return "-animatedead-"; }
	if (id == 0xD00F7) { return "-auraunholy-"; }
	if (id == 0xD00F8) { return "-auravampiric-"; }
	if (id == 0xD0247) { return "-carrionscarabs-"; }
	if (id == 0xD0248) { return "-carrionscarabson-"; }
	if (id == 0xD0249) { return "-carrionscarabsoff-"; }
	if (id == 0xD024A) { return "-carrionscarabsinstant-"; }
	if (id == 0xD00FA) { return "-carrionswarm-"; }
	if (id == 0xD00FB) { return "-darkritual-"; }
	if (id == 0xD00FC) { return "-darksummoning-"; }
	if (id == 0xD00FD) { return "-deathanddecay-"; }
	if (id == 0xD00FE) { return "-deathcoil-"; }
	if (id == 0xD00FF) { return "-deathpact-"; }
	if (id == 0xD0100) { return "-dreadlordinferno-"; }
	if (id == 0xD0101) { return "-frostarmor-"; }
	if (id == 0xD01EA) { return "-frostarmoron-"; }
	if (id == 0xD01EB) { return "-frostarmoroff-"; }
	if (id == 0xD0102) { return "-frostnova-"; }
	if (id == 0xD024B) { return "-impale-"; }
	if (id == 0xD024C) { return "-locustswarm-"; }
	if (id == 0xD0103) { return "-sleep-"; }
	if (id == 0xD0250) { return "-breathoffrost-"; }
	if (id == 0xD0116) { return "-creepanimatedead-"; }
	if (id == 0xD0117) { return "-creepdevour-"; }
	if (id == 0xD0118) { return "-creepheal-"; }
	if (id == 0xD0119) { return "-creephealon-"; }
	if (id == 0xD011A) { return "-creephealoff-"; }
	if (id == 0xD011C) { return "-creepthunderbolt-"; }
	if (id == 0xD011D) { return "-creepthunderclap-"; }
	if (id == 0xD0251) { return "-frenzy-"; }
	if (id == 0xD0252) { return "-frenzyon-"; }
	if (id == 0xD0253) { return "-frenzyoff-"; }
	if (id == 0xD0254) { return "-mechanicalcritter-"; }
	if (id == 0xD0255) { return "-mindrot-"; }
	if (id == 0xD0109) { return "-gold2lumber-"; }
	if (id == 0xD010A) { return "-lumber2gold-"; }
	if (id == 0xD0256) { return "-neutralinteract-"; }
	if (id == 0xD010B) { return "-spies-"; }
	if (id == 0xD0258) { return "-preservation-"; }
	if (id == 0xD010F) { return "-request_hero-"; }
	if (id == 0xD0259) { return "-sanctuary-"; }
	if (id == 0xD025A) { return "-shadowsight-"; }
	if (id == 0xD025B) { return "-spellshield-"; }
	if (id == 0xD025C) { return "-spellshieldaoe-"; }
	if (id == 0xD025D) { return "-spirittroll-"; }
	if (id == 0xD025E) { return "-steal-"; }
	if (id == 0xD0260) { return "-attributemodskill-"; }
	if (id == 0xD0261) { return "-blackarrow-"; }
	if (id == 0xD0262) { return "-blackarrowon-"; }
	if (id == 0xD0263) { return "-blackarrowoff-"; }
	if (id == 0xD0264) { return "-breathoffire-"; }
	if (id == 0xD0265) { return "-charm-"; }
	if (id == 0xD0113) { return "-coldarrowstarg-"; }
	if (id == 0xD0114) { return "-coldarrows-"; }
	if (id == 0xD0115) { return "-uncoldarrows-"; }
	if (id == 0xD0267) { return "-doom-"; }
	if (id == 0xD0207) { return "-drain-"; }
	if (id == 0xD0269) { return "-drunkenhaze-"; }
	if (id == 0xD026A) { return "-elementalfury-"; }
	if (id == 0xD026B) { return "-forkedlightning-"; }
	if (id == 0xD026C) { return "-howlofterror-"; }
	if (id == 0xD0108) { return "-inferno-"; }
	if (id == 0xD026F) { return "-monsoon-"; }
	if (id == 0xD011E) { return "-poisonarrowstarg-"; }
	if (id == 0xD011F) { return "-poisonarrows-"; }
	if (id == 0xD0120) { return "-unpoisonarrows-"; }
	if (id == 0xD0270) { return "-silence-"; }
	if (id == 0xD0271) { return "-stampede-"; }
	if (id == 0xD0272) { return "-summongrizzly-"; }
	if (id == 0xD0273) { return "-summonquillbeast-"; }
	if (id == 0xD0274) { return "-summonwareagle-"; }
	if (id == 0xD0275) { return "-tornado-"; }
	if (id == 0xD0276) { return "-wateryminion-"; }
	if (id == 0xD02B6) { return "-acidbomb-"; }
	if (id == 0xD02B7) { return "-chemicalrage-"; }
	if (id == 0xD02B8) { return "-healingspray-"; }
	if (id == 0xD02B9) { return "-transmute-"; }
	if (id == 0xD02AC) { return "-clusterrockets-"; }
	if (id == 0xD02B0) { return "-robogoblin-"; }
	if (id == 0xD02B1) { return "-unrobogoblin-"; }
	if (id == 0xD02B2) { return "-summonfactory-"; }
	if (id == 0xD02BE) { return "-incineratearrow-"; }
	if (id == 0xD02BF) { return "-incineratearrowon-"; }
	if (id == 0xD02C0) { return "-incineratearrowoff-"; }
	if (id == 0xD02BB) { return "-lavamonster-"; }
	if (id == 0xD02BC) { return "-soulburn-"; }
	if (id == 0xD02BD) { return "-volcano-"; }
	if (id == 0xD0277) { return "-battleroar-"; }
	if (id == 0xD0278) { return "-channel-"; }
	if (id == 0xD0104) { return "-darkconversion-"; }
	if (id == 0xD0105) { return "-darkportal-"; }
	if (id == 0xD0106) { return "-fingerofdeath-"; }
	if (id == 0xD0107) { return "-firebolt-"; }
	if (id == 0xD0279) { return "-parasite-"; }
	if (id == 0xD027A) { return "-parasiteon-"; }
	if (id == 0xD027B) { return "-parasiteoff-"; }
	if (id == 0xD010D) { return "-rainofchaos-"; }
	if (id == 0xD010E) { return "-rainoffire-"; }
	if (id == 0xD0111) { return "-revenge-"; }
	if (id == 0xD0112) { return "-soulpreservation-"; }
	if (id == 0xD027C) { return "-submerge-"; }
	if (id == 0xD027D) { return "-unsubmerge-"; }
	if (id == 0xD0110) { return "-disassociate-"; }
	if (id == 0xD0296) { return "-neutralspell-"; }

	if (id >= 0xD0022 && id <= 0xD0028)
		return "-dropitem-";
	if (id >= 0xD0028 && id <= 0xd002d)
		return "-inventory-";

	return std::to_string(id);
}


BOOL DetectLocalPlayer = FALSE;

bool FileExists(const std::string& fname)
{
	return std::filesystem::exists(fname) && !std::filesystem::is_directory(fname);
}

int GetLocalPlayerNumber()
{
	if (!DetectLocalPlayer)
		return -2;

	void* gldata = GetGlobalPlayerData();
	if (gldata != 0)
	{
		int playerslotaddr = (int)gldata + 0x28;
		return (int)*(short*)(playerslotaddr);
	}
	else
		return 16;
}



int GetItemCountAndItemArray(int** itemarray)
{
	int GlobalClassOffset = *(int*)(pW3XGlobalClass);
	if (GlobalClassOffset > 0)
	{
		int ItemOffset1 = *(int*)(GlobalClassOffset + 0x3BC) + 0x10;
		int ItemCount = *(int*)(ItemOffset1 + 0x604);
		if (ItemCount > 0 && ItemOffset1 > 0)
		{
			*itemarray = (int*)*(int*)(ItemOffset1 + 0x608);
			return ItemCount;
		}
	}
	*itemarray = 0;
	return 0;
}

int SafeUnitCount = 1;
int* SafeUnitArray = new int[1] {0};
void FillUnitCountAndUnitArray()
{
	if (SafeUnitCount > 0)
	{
		SafeUnitCount = 0;
		delete[] SafeUnitArray;
	}

	int GlobalClassOffset = *(int*)(pW3XGlobalClass);
	if (GlobalClassOffset > 0)
	{
		int UnitsOffset1 = *(int*)(GlobalClassOffset + 0x3BC);
		if (UnitsOffset1 > 0)
		{
			int* UnitsCount = (int*)(UnitsOffset1 + 0x604);
			if (UnitsCount
				&& *UnitsCount > 0)
			{
				int* unitarray = (int*)*(int*)(UnitsOffset1 + 0x608);
				SafeUnitCount = *UnitsCount;
				SafeUnitArray = new int[SafeUnitCount];
				memcpy(SafeUnitArray, unitarray, 4 * SafeUnitCount);
			}
		}
	}
}


BOOL IsUnitDead(int unitaddr)
{
	if (unitaddr > 0)
	{
		unsigned int unitflag = *(unsigned int*)(unitaddr + 0x5C);
		BOOL UnitNotDead = ((unitflag & 0x100u) == 0);
		return UnitNotDead == FALSE;
	}
	return TRUE;
}

BOOL IsUnitTower(int unitaddr)
{
	if (unitaddr)
	{
		unsigned int unitflag = *(unsigned int*)(unitaddr + 0x5C);
		BOOL UnitNotTower = ((unitflag & 0x10000u) == 0);
		return UnitNotTower == FALSE;
	}
	return TRUE;
}

BOOL UnitHaveItems(int unitaddr)
{
	if (unitaddr)
	{
		return *(int*)(unitaddr + 0x1F8) > 0;
	}
	return FALSE;
}

BOOL UnitHaveAttack(int unitaddr)
{
	if (unitaddr)
	{
		return *(int*)(unitaddr + 0x1e8) > 0;
	}
	return FALSE;
}


BOOL IsHero(int unitaddr)
{
	if (unitaddr)
	{
		unsigned int ishero = *(unsigned int*)(unitaddr + 48);
		ishero = ishero >> 24;
		ishero = ishero - 64;
		return ishero < 0x19;
	}
	return FALSE;
}


BOOL IsDetectedTower(int unitaddr)
{
	if (unitaddr)
	{
		//unsigned int unitflag = *(unsigned int*)(unitaddr + 0x5C);
		unsigned int unitflag2 = *(unsigned int*)(unitaddr + 32);
		if (IsUnitTower(unitaddr))
		{
			//BOOL UnitNotVulnerable = ((unitflag2 & 0x8u) == 0);
			BOOL UnitClickable = ((unitflag2 & 0x4u) == 0);
			if (/*!UnitNotVulnerable && */!UnitClickable)
			{
				return !IsHero(unitaddr)/* && !UnitHaveItems(unitaddr) && UnitHaveAttack(unitaddr)*/;
			}
		}
	}
	return FALSE;
}


unsigned char* UnitVtable = 0;
unsigned char* ItemVtable = 0;

float GetUnitTimer(int unitaddr)
{
	int unitdataddr = *(int*)(unitaddr + 0x28);
	if (unitdataddr <= 0)
		return 0.0f;
#ifdef BOTDEBUG
	PrintDebugInfo("CheckBadUnit - ENDCHExCK5");
#endif
	return *(float*)(unitdataddr + 0xA0);
}

unsigned char* GetSomeAddr_Addr = 0;
__declspec(naked) int __fastcall GetSomeAddr(UINT a1, UINT a2)
{
	__asm
	{
		JMP GetSomeAddr_Addr;
	}
}


//
//LPVOID ConvertHandle(unsigned int handle) {
//	return handle > 0x100000 ? *(LPVOID*)(*(int*)(*(int*)(*(int*)pGlobalPlayerData + 0x1c) + 0x19c) + handle * 0xc - 0x2fffff * 4) : NULL;
//}


BOOL __stdcall IsNotBadUnit(int _unitaddr)
{
	if (_unitaddr > 0)
	{
		unsigned char* unitaddr = (unsigned char*)_unitaddr;
		unsigned char* realvtbladdr = (unsigned char*)&UnitVtable;

		if (realvtbladdr[0] != unitaddr[0])
			return 0;
		else if (realvtbladdr[1] != unitaddr[1])
			return 0;
		else if (realvtbladdr[2] != unitaddr[2])
			return 0;
		else if (realvtbladdr[3] != unitaddr[3])
			return 0;


		//unsigned int unitflag = *(unsigned int*)(unitaddr + 0x20);
		//unsigned int unitflag2 = *(unsigned int*)(unitaddr + 0x5C);


		//if (unitflag & 1u)
		//	return FALSE;

		//if (!(unitflag & 2u))
		//	return FALSE;

		//if (unitflag2 & 0x100u)
		//	return FALSE;

		bool UnitFoundInArray = false;

		for (int i = 0; i < SafeUnitCount; i++)
		{
			if (SafeUnitArray[i] == _unitaddr)
				UnitFoundInArray = true;
		}


		return UnitFoundInArray && !IsUnitDead(_unitaddr);
	}

	return FALSE;
}

BOOL IsNotBadItem(int itemaddr, BOOL onlymemcheck = FALSE)
{
	if (itemaddr > 0)
	{
		int xaddraddr = (int)&ItemVtable;

		if (*(BYTE*)xaddraddr != *(BYTE*)itemaddr)
			return FALSE;
		else if (*(BYTE*)(xaddraddr + 1) != *(BYTE*)(itemaddr + 1))
			return FALSE;
		else if (*(BYTE*)(xaddraddr + 2) != *(BYTE*)(itemaddr + 2))
			return FALSE;
		else if (*(BYTE*)(xaddraddr + 3) != *(BYTE*)(itemaddr + 3))
			return FALSE;



		if (*(int*)(itemaddr + 0x20) & 1)
			return FALSE;

		float hitpoint = *(float*)(itemaddr + 0x58);

		return hitpoint != 0.0f;
	}

	return FALSE;
}


typedef int(__cdecl* pPlayer)(int number);
pPlayer PlayerReal;

int Player(int number)
{
	if (number >= 0 && number <= 15)
	{
		return PlayerReal(number);
	}
	return 0;
}

int __cdecl IsUnitVisibleToPlayer(unsigned char* unitaddr, unsigned char* player)
{
	if (player && player)
	{
		__asm
		{
			mov esi, unitaddr;
			mov eax, player;
			movzx eax, byte ptr[eax + 0x30];
			mov edx, [esi];
			push 0x04;
			push 0x00;
			push eax;
			mov eax, [edx + 0x000000FC];
			mov ecx, esi;
			call eax;
		}
	}
	else
		return 0;
}

typedef int(__cdecl* pIsUnitSelected)(int unitaddr, int playerdataaddr);
pIsUnitSelected IsUnitSelectedReal;


BOOL __fastcall IsUnitSelected(int unitaddr, int playerdataaddr)
{
	return IsUnitSelectedReal(unitaddr, playerdataaddr);
}


int IsGame()
{
	if (!GameDll)
		return 0;

	unsigned char* _GameUI = GameDll + 0x93631C;
	unsigned char* InGame = GameDll + 0xACE66C;

	if (!InGame)
		return 0;

	return *(unsigned char**)InGame && **(unsigned char***)InGame == _GameUI;
}


long long ActionTime = 0;

void DisplayText(const std::string& szText, float fDuration)
{
	if (IsReplayFound)
		fDuration = 60.0f;

	unsigned int dwDuration = *((unsigned int*)&fDuration);


	unsigned char* GAME_PrintToScreen = GameDll + 0x2F8E40;

	int seconds = (int)(ActionTime / 1000) % 60;
	int minutes = (int)((ActionTime / (1000 * 60)) % 60);
	int hours = (int)((ActionTime / (1000 * 60 * 60)) % 24);

	char timeBuff[64];
	sprintf_s(timeBuff, "[%.2d:%.2d:%.2d] : ", hours, minutes, seconds);

	std::string outLineStr = (timeBuff + szText);
	const char* outLinePointer = outLineStr.c_str();

	if (LoggingType != -1)
	{
		WatcherLog((outLineStr + "\n").c_str());
	}

	if (!GameDll || !*(unsigned char**)pW3XGlobalClass || LoggingType == 0)
		return;

	__asm
	{
		PUSH	0xFFFFFFFF;
		PUSH	dwDuration;
		PUSH	outLinePointer;
		PUSH	0x0;
		PUSH	0x0;
		MOV		ECX, [pW3XGlobalClass];
		MOV		ECX, [ECX];
		CALL	GAME_PrintToScreen;
	}
}

long long latestcheck = 0;

int GetSelectedItem(int slot)
{
	int plr = GetPlayerByNumber(slot);
	if (plr != 0)
	{
		int itemaddr;
		__asm
		{
			MOV EAX, plr;
			MOV ECX, DWORD PTR DS : [EAX + 0x34] ;
			CMP ECX, 0;
			JNE NORET;
			MOV EAX, 0;
			ret;
		NORET:
			MOV EAX, DWORD PTR DS : [ECX + 0x1A8] ;
			MOV itemaddr, EAX;
		}

		return itemaddr;
	}

	return NULL;
}

int UnitDetectionMethod = 3;

int GetSelectedUnitCount(int slot, BOOL Smaller = TRUE)
{
	int plr = GetPlayerByNumber(slot);
	if (plr != 0)
	{
		int PlayerData1 = *(int*)(plr + 0x34);
		if (PlayerData1)
		{
			int unit1 = *(int*)(PlayerData1 + 0x1E0);
			int unit2 = *(int*)(PlayerData1 + 0x1A4);
			int unitcount = *(int*)(PlayerData1 + 0x10);
			int unitcount2 = *(int*)(PlayerData1 + 0x1D4);

			if (unit1 != NULL || unit2 != NULL)
			{

				if (!Smaller)
				{
					if (unitcount > unitcount2)
						return unitcount;
					return unitcount2;
				}

				if (unitcount == unitcount2)
				{
					return unitcount;
				}
				return unitcount2;
			}

		}
	}

	return NULL;
}

int GetSelectedUnit(int slot)
{
	int plr = GetPlayerByNumber(slot);
	if (plr > 0)
	{
		int PlayerData1 = *(int*)(plr + 0x34);
		if (PlayerData1 > 0)
		{
			int unit1 = *(int*)(PlayerData1 + 0x1E0);
			int unit2 = *(int*)(PlayerData1 + 0x1A4);
			int unitcount = *(int*)(PlayerData1 + 0x10);
			int unitcount2 = *(int*)(PlayerData1 + 0x1D4);

			if (UnitDetectionMethod == 1)
			{
				if (unit1 == unit2
					&& unit1 > 0
					&& IsNotBadUnit(unit1)
					&& (unitcount > 0 || unitcount2 > 0)
					)
				{
					if (IsUnitSelected(unit1, PlayerData1))
						return unit1;
				}

				if (unit1 > 0
					&& unit2 > 0
					&& IsNotBadUnit(unit2)
					&& (unitcount > 0 || unitcount2 > 0))
				{
					if (IsUnitSelected(unit2, PlayerData1))
						return unit2;
				}

				//if (IsNotBadUnit(unitaddr) && (unitcount || unitcount2)/* && IsUnitSelected(unitaddr, PlayerData1)*/)
				//{
				//	return unitaddr;
				//}
			}
			else if (UnitDetectionMethod == 2)
			{
				int unitaddr = unit2;
				if (unitaddr > 0 && unitcount == 1 && unitcount2 == 1)
				{
					if (IsNotBadUnit(unitaddr) && IsUnitSelected(unitaddr, PlayerData1))
					{
						return unitaddr;
					}
				}
			}
			else if (UnitDetectionMethod == 3)
			{
				if (unit1 == unit2)
				{
					int unitaddr = unit1;
					if (unitaddr > 0 && unitcount == 1 && unitcount2 == 1)
					{
						if (IsNotBadUnit(unitaddr))
						{
							return unitaddr;
						}
					}
				}
			}
			else if (UnitDetectionMethod == 4)
			{
				int unitaddr = unit1;
				if (unitaddr > 0 && (unitcount == 1 || unitcount2 == 1))
				{
					if (IsNotBadUnit(unitaddr) && IsUnitSelected(unitaddr, PlayerData1))
					{
						return unitaddr;
					}
				}
				unitaddr = unit2;
				if (unitaddr > 0 && (unitcount == 1 || unitcount2 == 1))
				{
					if (IsNotBadUnit(unitaddr) && IsUnitSelected(unitaddr, PlayerData1))
					{
						return unitaddr;
					}
				}
			}
			else
			{
				int unitaddr = unit1;
				if (unitaddr > 0 && unitcount == 1)
				{
					if (IsNotBadUnit(unitaddr))
					{
						return unitaddr;
					}
				}
				unitaddr = unit2;
				if (unitaddr > 0 && unitcount2 == 1)
				{
					if (IsNotBadUnit(unitaddr))
					{
						return unitaddr;
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
typedef int(__fastcall* p_GetTypeInfo)(int unit_item_code, int unused);
p_GetTypeInfo GetTypeIdInfoAddr = NULL;

typedef float(__fastcall* pGetMiscDataFloat)(const char* section, const char* key, unsigned int addr);
pGetMiscDataFloat GetMiscDataFloat = NULL;



int GetObjectTypeId(int unit_or_item_addr)
{
	return *(int*)(unit_or_item_addr + 0x30);
}

float GetSafeObjectSelectionCircle(int unit_addr)
{
	float retval = 0.0f;
	if (unit_addr > 0)
	{
		int unitData = GetTypeIdInfoAddr(GetObjectTypeId(unit_addr), 0);
		if (unitData > 0)
		{
			retval = (*(float*)(unitData + 0x54) * DefaultCircleScale) / 4.8f;
		}
	}

	if (retval < 16.0f)
		return 16.0f;
	return retval;
}

//sub_6F2F8F90 126a
//sub_6F34F730 127a
typedef char* (__fastcall* p_GetPlayerName)(int a1, int a2);
p_GetPlayerName GetPlayerName = NULL;


const char* GetObjectNameByID(int clid)
{
	if (clid > 0)
	{
		int v3 = GetTypeIdInfoAddr(clid, 0);
		int v4, v5;
		if (v3 && (v4 = *(int*)(v3 + 40)) != 0)
		{
			v5 = v4 - 1;
			if (v5 >= (unsigned int)0)
				v5 = 0;
			return (char*)*(int*)(*(int*)(v3 + 44) + 4 * v5);
		}
		else
		{
			return DefaultString;
		}
	}
	return DefaultString;
}





const char* GetObjectName(int objaddress)
{
	return GetObjectNameByID(GetObjectClassID(objaddress));
}


int CreateJassNativeHook(int oldaddress, int newaddress)
{
	unsigned char* module = (unsigned char*)GetModuleHandle("war3map.dll");

	// ������ Jass ������� � Dota 6.83s fukkei
	if (module && *(int*)(module + 0xb2eb28) == (int)(GameDll + 0x3D1550))
	{
		WatcherLog("[HOOK JASS NATIVE IN 6.83s]");
		unsigned char* StartAddr = module + 0xb2eb28;
		while (*(int*)StartAddr != 0)
		{
			if (*(int*)StartAddr == oldaddress)
			{
				*(int*)StartAddr = newaddress;
				return oldaddress;
			}
			StartAddr += 4;
		}
		return 0;
	}

	// ������ JASS ������� � iCCup Dota 390
	if (module && *(int*)(module + 0x10434C4) == (int)(GameDll + 0x3C83E0))
	{
		WatcherLog("[HOOK JASS NATIVE IN iCCup Dota 390]");
		unsigned char* StartAddr = module + 0x10434C4;
		while (*(int*)StartAddr != 0)
		{
			if (*(int*)StartAddr == oldaddress)
			{
				*(int*)StartAddr = newaddress;
				return oldaddress;
			}
			StartAddr += 4;
		}
		return 0;
	}

	// ������ JASS ������� � iCCup Dota 396
	if (module && *(int*)(module + 0x104571C) == (int)(GameDll + 0x3B3E50))
	{
		WatcherLog("[HOOK JASS NATIVE IN iCCup Dota 396]");
		unsigned char* StartAddr = module + 0x104571C;
		while (*(int*)StartAddr != 0)
		{
			if (*(int*)StartAddr == oldaddress)
			{
				*(int*)StartAddr = newaddress;
				return oldaddress;
			}
			StartAddr += 4;
		}
	}

	int FirstAddress = *(int*)pJassEnvAddress;
	if (FirstAddress > 0)
	{
		FirstAddress = *(int*)(FirstAddress + 20);
		if (FirstAddress > 0)
		{
			FirstAddress = *(int*)(FirstAddress + 32);
			if (FirstAddress > 0)
			{
				int NextAddress = FirstAddress;
				while (TRUE)
				{
					if (*(int*)(NextAddress + 12) == oldaddress)
					{
						int oldaddr = *(int*)(NextAddress + 12);
						*(int*)(NextAddress + 12) = newaddress;
						return oldaddr;
					}

					NextAddress = *(int*)NextAddress;

					if (NextAddress == FirstAddress || NextAddress <= 0)
						break;
				}
			}
		}
	}
	return 0;
}


int CreateJassNativeHook(std::string name, int newaddress)
{
	int FirstAddress = *(int*)pJassEnvAddress;
	if (FirstAddress > 0)
	{
		FirstAddress = *(int*)(FirstAddress + 20);
		if (FirstAddress > 0)
		{
			FirstAddress = *(int*)(FirstAddress + 32);
			if (FirstAddress > 0)
			{
				int NextAddress = FirstAddress;
				while (TRUE)
				{
					if (*(char**)(NextAddress + 8) == name)
					{
						int oldaddr = *(int*)(NextAddress + 12);
						*(int*)(NextAddress + 12) = newaddress;
						return oldaddr;
					}

					NextAddress = *(int*)NextAddress;

					if (NextAddress == FirstAddress || NextAddress <= 0)
						break;
				}
			}
		}
	}
	return 0;
}

typedef int(__cdecl* pGetSpellAbilityId)();
pGetSpellAbilityId GetSpellAbilityId_real;

typedef int(__cdecl* pGetTriggerEventId)();
pGetTriggerEventId GetTriggerEventId_real;

typedef int(__cdecl* pClearTextMessages)();
pClearTextMessages ClearTextMessages_real;

int __cdecl ClearTextMessages_hooked()
{
	if (IsReplayFound)
		return 0;
	return ClearTextMessages_real();
}

typedef int(__cdecl* pGetIssuedOrderId)();
pGetIssuedOrderId GetIssuedOrderId_real;

typedef int(__cdecl* pGetSpellAbilityUnit)();
pGetSpellAbilityUnit GetSpellAbilityUnit_real;

typedef int(__cdecl* pGetAttacker)();
pGetAttacker GetAttacker_real;
int pRecoveryJassNative5 = 0;

typedef int(__cdecl* pGetSpellTargetUnit)();
pGetSpellTargetUnit GetSpellTargetUnit;

typedef DWFP(__cdecl* pGetSpellTargetX)();
pGetSpellTargetX _GetSpellTargetX;

typedef DWFP(__cdecl* pGetSpellTargetY)();
pGetSpellTargetY _GetSpellTargetY;

typedef int(__fastcall* pGetHandleUnitAddress) (int HandleID, int unused);
pGetHandleUnitAddress GetHandleUnitAddress;
pGetHandleUnitAddress GetHandleItemAddress;

typedef BOOL(__cdecl* pIsPlayerEnemy)(int whichPlayer, int otherPlayer);
pIsPlayerEnemy IsPlayerEnemyReal;

typedef int(__cdecl* pGetPlayerController)(int whichPlayer);
pGetPlayerController GetPlayerController;

typedef int(__cdecl* pGetPlayerSlotState)(int whichPlayer);
pGetPlayerSlotState GetPlayerSlotState;

BOOL __cdecl IsPlayerEnemy(int hplayer1, int hplayer2)
{
	if (hplayer1 != hplayer2)
	{
		if (hplayer1 > 0 && hplayer2 > 0)
		{
			return IsPlayerEnemyReal(hplayer1, hplayer2) && IsPlayerEnemyReal(hplayer2, hplayer1);
		}
	}
	return FALSE;
}

//int GetPlayerTeam(unsigned char* playeraddr)
//{
//	if (!playeraddr)
//		return 0;
//	return *(int*)(playeraddr + 0x278);
//}
//
//int IsPlayerEnemy(unsigned char* unitaddr)
//{
//	int teamplayer1 = GetPlayerTeam(GetLocalPlayer());
//	int teamplayer2 = GetPlayerTeam(GetPlayerByNumber(GetUnitOwnerSlot(unitaddr)));
//
//	return teamplayer1 != teamplayer2;
//}

typedef int(__cdecl* pGetTriggerUnit)();
pGetTriggerUnit GetTriggerUnit;

typedef int(__cdecl* pGetOrderTargetUnit)();
pGetOrderTargetUnit GetOrderTargetUnit;

typedef int(__cdecl* pGetOrderTargetItem)();
pGetOrderTargetItem GetOrderTargetItem;

typedef DWFP(__cdecl* pGetOrderPointX)();
pGetOrderPointX _GetOrderPointX;

typedef DWFP(__cdecl* pGetOrderPointY)();
pGetOrderPointY _GetOrderPointY;

typedef BOOL(__cdecl* pIsFoggedToPlayer)(float* x, float* y, int whichPlayer);
pIsFoggedToPlayer IsFoggedToPlayerReal;

int GetItemOwner(int itemaddr)
{
	if (itemaddr)
		return  *(int*)(itemaddr + 0x74);
	return 0;
}

BOOL __cdecl IsFoggedToPlayerMy(float* pX, float* pY, int player)
{
	if (player <= 0)
	{
		return FALSE;
	}
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



	BOOL CheckCenter = IsFoggedToPlayerReal(&x1, &y1, player);

	if (DetectQuality >= 2)
	{
		BOOL CheckRight = IsFoggedToPlayerReal(&x2, &y2, player);
		BOOL CheckLeft = IsFoggedToPlayerReal(&x3, &y3, player);
		BOOL CheckTop = IsFoggedToPlayerReal(&x4, &y4, player);
		BOOL CheckBot = IsFoggedToPlayerReal(&x5, &y5, player);
		if (DetectQuality >= 3)
		{
			return CheckCenter && CheckRight && CheckLeft && CheckTop && CheckBot;
		}
		return CheckCenter && CheckRight && CheckLeft && CheckTop && CheckBot;
	}
	return CheckCenter;
}

long long LatestAbilSpell = 0;


void GetUnitLocation3D(int unitaddr, float* x, float* y, float* z)
{
	if (unitaddr)
	{
		*x = *(float*)(unitaddr + 0x284);
		*y = *(float*)(unitaddr + 0x288);
		*z = *(float*)(unitaddr + 0x28C);
	}
	else
	{
		*x = 0.0f;
		*y = 0.0f;
		*z = 0.0f;
	}
}


void GetItemLocation3D(int itemaddr, float* x, float* y, float* z)
{
	if (itemaddr)
	{
		int iteminfo = *(int*)(itemaddr + 0x28);
		if (iteminfo)
		{
			*x = *(float*)(iteminfo + 0x88);
			*y = *(float*)(iteminfo + 0x8C);
			*z = *(float*)(iteminfo + 0x90);
		}
		else
		{
			*x = 0.0f;
			*y = 0.0f;
			*z = 0.0f;
		}
	}
	else
	{
		*x = 0.0f;
		*y = 0.0f;
		*z = 0.0f;
	}
}



/*void GetUnitLocation2D(int unitaddr, float* x, float* y)
{
	if (unitaddr)
	{
		*x = *(float*)(unitaddr + 0x284);
		*y = *(float*)(unitaddr + 0x288);
	}
	else
	{
		*x = 0.0;
		*y = 0.0;
	}
}


void GetItemLocation2D(int itemaddr, float* x, float* y)
{
	if (itemaddr)
	{
		int iteminfo = *(int*)(itemaddr + 0x28);
		if (iteminfo)
		{
			*x = *(float*)(iteminfo + 0x88);
			*y = *(float*)(iteminfo + 0x8C);
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
}*/


float Distance3D(float x1, float y1, float z1, float x2, float y2, float z2)
{
	double d[] = { abs((double)x1 - (double)x2), abs((double)y1 - (double)y2), abs((double)z1 - (double)z2) };
	if (d[0] < d[1]) std::swap(d[0], d[1]);
	if (d[0] < d[2]) std::swap(d[0], d[2]);
	return (float)(d[0] * sqrt(1.0 + d[1] / d[0] + d[2] / d[0]));
}

float Distance2D(float x1, float y1, float x2, float y2)
{
	return Distance3D(x1, y1, 1.0f, x2, y2, 1.0f);
}


float pingduration = 1.5;

typedef void(__cdecl* pPingMinimapEx)(float* x, float* y, float* duration, int red, int green, int blue, BOOL extraEffects);
pPingMinimapEx PingMinimapEx;

void __cdecl PingMinimapMy(float* x, float* y, float* duration, int red, int green, int blue, BOOL extraEffects)
{
	float newx = *x + (float)(-200 + rand() % 200);
	float newy = *y + (float)(-200 + rand() % 200);

	return PingMinimapEx(&newx, &newy, &pingduration, red, green, blue, extraEffects);
}

int GetItemByXY(float x, float y, int player)
{
	int* itemsarray = 0;
	int itemsCount = GetItemCountAndItemArray(&itemsarray);
	if (itemsCount > 0 && itemsarray)
	{
		for (int i = 0; i < itemsCount; i++)
		{
			if (itemsarray[i])
			{
				if (IsNotBadItem(itemsarray[i]))
				{
					float itemx = 0.0f, itemy = 0.0f, itemz = 0.0f;
					GetItemLocation3D(itemsarray[i], &itemx, &itemy, &itemz);
					if (Distance2D(itemx, itemy, x, y) < GetSafeObjectSelectionCircle(itemsarray[i]))
						return itemsarray[i];
				}
			}
		}
	}
	return 0;
}


BOOL ImpossibleClick = FALSE;

int GetUnitByXY(float x, float y, int playerid, BOOL onlyunits = FALSE)
{
	if (!DetectLocalPlayer && playerid == GetLocalPlayerNumber())
		return 0;

	FillUnitCountAndUnitArray();
	/*int SafeUnitCount = 1;
	int* TempUnitArray = new int[ 0 ];*/

	int CurrentUnit = 0;

	if (SafeUnitCount > 0)
	{
		for (int i = 0; i < SafeUnitCount; i++)
		{
			CurrentUnit = SafeUnitArray[i];

			if (IsNotBadUnit(CurrentUnit))
			{
				int unitowner = GetUnitOwnerSlot(CurrentUnit);

				float selectionCircle = GetSafeObjectSelectionCircle(CurrentUnit);

				if (playerid == unitowner)
					continue;
				bool IsValidTowerDetect = false;
				if (!IsHero(CurrentUnit))
				{
					IsValidTowerDetect = (DetectImpossibleClicks && IsDetectedTower(CurrentUnit));
					if (!IsValidTowerDetect)
					{
						if (!(!DetectRightClickOnlyHeroes && !IsUnitTower(CurrentUnit)))
						{
							continue;
						}
					}
				}
				if (playerid != unitowner)
				{
					int hPlayerFinder = Player(playerid);
					int hPlayerOwner = Player(unitowner);

					if (hPlayerFinder && hPlayerOwner)
					{
						if (IsValidTowerDetect || IsPlayerEnemy(hPlayerFinder, hPlayerOwner))
						{
							ImpossibleClick = FALSE;
							float unitx = 0.0f, unity = 0.0f, unitz = 0.0f;
							GetUnitLocation3D(CurrentUnit, &unitx, &unity, &unitz);

							if (IsValidTowerDetect)
							{
								if (Distance2D(unitx, unity, x, y) < selectionCircle && Distance2D(unitx, unity, x, y) > 0.1)
								{
									ImpossibleClick = TRUE;
									return CurrentUnit;
								}
							}

							if (IsDetectedTower(CurrentUnit))
							{
								if (Distance2D(unitx, unity, x, y) <= 0.1)
									continue;
							}

							if (Distance2D(unitx, unity, x, y) < selectionCircle)
							{
								return CurrentUnit;
							}
						}
					}
				}
			}
		}
	}
	if (onlyunits)
	{
		return 0;
	}
	return GetItemByXY(x, y, playerid);
}

int LastEventID, LastSkillID, LastCasterID;
long long LastEventTime = 0;

struct PlayerEvent
{
	int EventID;
	int SkillID;
	int Caster;
	int SelectedUnits;
	long long Time;
};

PlayerEvent PlayerEventList[MAX_PLAYERS][20];
int MeepoPoofID = 0x41304E38;
//BOOL PlayerMeepoDetect[ 20 ];

void ShiftLeftAndAddNewActionScanForBot(int PlayerID, PlayerEvent NewPlayerEvent)
{
	if (PlayerID >= 0 && PlayerID < MAX_PLAYERS && (GetLocalPlayerNumber() != PlayerID || DetectLocalPlayer))
	{
		if (DebugLog)
			WatcherLog("[DEBUG][+%u ms][LogActions] : Event:%i - Skill:%X - Caster:%X \n", NewPlayerEvent.Time, NewPlayerEvent.EventID, NewPlayerEvent.SkillID, NewPlayerEvent.Caster);

		if (NewPlayerEvent.EventID == 272)
		{

			for (int i = 0; i < 19; i++)
			{
				PlayerEventList[PlayerID][i] = PlayerEventList[PlayerID][i + 1];
			}

			PlayerEventList[PlayerID][19] = NewPlayerEvent;

			PlayerEvent Event1 = PlayerEventList[PlayerID][19];
			PlayerEvent Event2 = PlayerEventList[PlayerID][18];
			PlayerEvent Event3 = PlayerEventList[PlayerID][17];
			PlayerEvent Event4 = PlayerEventList[PlayerID][16];


			/*WatcherLog( "[event1][+%u ms][LogActions] : Event:%i - Skill:%X - Caster:%X - units %i \n", Event1.Time, Event1.EventID, Event1.SkillID, Event1.Caster,Event1.SelectedUnits );
			WatcherLog( "[event2][+%u ms][LogActions] : Event:%i - Skill:%X - Caster:%X - units %i\n", Event2.Time, Event2.EventID, Event2.SkillID, Event2.Caster, Event1.SelectedUnits );
			WatcherLog( "[event3][+%u ms][LogActions] : Event:%i - Skill:%X - Caster:%X - units %i\n", Event3.Time, Event3.EventID, Event3.SkillID, Event3.Caster, Event1.SelectedUnits );
			WatcherLog( "[event4][+%u ms][LogActions] : Event:%i - Skill:%X - Caster:%X - units %i\n", Event4.Time, Event4.EventID, Event4.SkillID, Event4.Caster, Event1.SelectedUnits );
			*/

			if (DetectMeepoKey)
			{
				if (Event1.SkillID == MeepoPoofID &&
					Event2.SkillID == MeepoPoofID &&
					Event3.SkillID == MeepoPoofID &&
					Event4.SkillID == MeepoPoofID)
				{
					if (Event1.Caster != 0 &&
						Event2.Caster != 0 &&
						Event3.Caster != 0 &&
						Event4.Caster != 0)
					{
						if (Event1.Caster != Event2.Caster &&
							Event1.Caster != Event3.Caster &&
							Event1.Caster != Event4.Caster &&
							//	 Event2.Caster != Event3.Caster &&
							Event3.Caster != Event2.Caster &&
							Event3.Caster != Event4.Caster)
						{
							if (DebugLog)
							{
								WatcherLog("[event1][+%u ms][LogActions] : Event:%i - Skill:%X - Caster:%X - units %i\n", Event1.Time, Event1.EventID, Event1.SkillID, Event1.Caster, Event1.SelectedUnits);
								WatcherLog("[event2][+%u ms][LogActions] : Event:%i - Skill:%X - Caster:%X - units %i\n", Event2.Time, Event2.EventID, Event2.SkillID, Event2.Caster, Event1.SelectedUnits);
								WatcherLog("[event3][+%u ms][LogActions] : Event:%i - Skill:%X - Caster:%X - units %i\n", Event3.Time, Event3.EventID, Event3.SkillID, Event3.Caster, Event1.SelectedUnits);
								WatcherLog("[event4][+%u ms][LogActions] : Event:%i - Skill:%X - Caster:%X - units %i\n", Event4.Time, Event4.EventID, Event4.SkillID, Event4.Caster, Event1.SelectedUnits);
							}
							if (
								Event2.Time < 120 &&
								Event3.Time < 120 &&
								Event4.Time < 120)
							{
								if (Event1.SelectedUnits == 1 &&
									Event2.SelectedUnits == 1 &&
									Event3.SelectedUnits == 1 &&
									Event4.SelectedUnits == 1
									)
								{
									//if ( !PlayerMeepoDetect[ PlayerID ] )
									//{
									//	PlayerMeepoDetect[ PlayerID ] = TRUE;
									LatestAbilSpell = CurGameTime;

									sprintf_s(PrintBuffer, 2048, "|c00EF4000[FogCW v16.1]|r: Player %s%s|r use MeepoKey!!\0",
										GetPlayerColorString(PlayerID),
										GetPlayerName(PlayerID, 0));

									ActionTime = CurGameTime;
									DisplayText(PrintBuffer, 15.0f);
									SendPause();
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

void BotDetector(int SkillID, int EventID, int CasterID)
{
	// THE BOT DETECTOR!

	LastEventID = EventID;
	LastSkillID = SkillID;
	LastCasterID = CasterID;
	if (CasterID > 0)
	{
		int CasterAddr = GetHandleUnitAddress(CasterID, 0);
		if (CasterAddr > 0)
		{
			int CasterSlot = GetUnitOwnerSlot(CasterAddr);
			if (CasterSlot < 15 && CasterSlot >= 0)
			{
				if (EventID == 272)
				{
					PlayerEvent NewPlayerEvent;
					NewPlayerEvent.Caster = CasterAddr;
					NewPlayerEvent.EventID = EventID;
					NewPlayerEvent.SkillID = SkillID;
					NewPlayerEvent.Time = llabs(CurGameTime - LastEventTime);
					NewPlayerEvent.SelectedUnits = GetSelectedUnitCount(CasterSlot, FALSE);
					ShiftLeftAndAddNewActionScanForBot(CasterSlot, NewPlayerEvent);
					LastEventTime = CurGameTime;
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

std::vector<ProcessNewAction> ProcessNewActionList;

void ProcessGetSpellAbilityIdAction(int EventID, int TargetUnitHandle, int CasterUnitHandle, int TargetItemHandle, int GetIssueOrderId, float GetSpellTargetX, float GetSpellTargetY)
{
	if (CasterUnitHandle > 0)
	{
		if (TargetUnitHandle > 0)
		{
			int CasterAddr = GetHandleUnitAddress(CasterUnitHandle, 0);
			int TargetAddr = GetHandleUnitAddress(TargetUnitHandle, 0);
			if (CasterAddr > 0 && IsNotBadUnit(CasterAddr) && TargetAddr > 0 && IsNotBadUnit(TargetAddr))
			{
				int CasterSlot = GetUnitOwnerSlot(CasterAddr);
				int TargetSlot = GetUnitOwnerSlot(TargetAddr);

				if (CasterSlot >= 0 && CasterSlot <= 15 && TargetSlot >= 0 && TargetSlot <= 15 && (IsHero(TargetAddr) || (!DetectRightClickOnlyHeroes && !IsUnitTower(TargetAddr))) && (CasterSlot != GetLocalPlayerNumber() || DetectLocalPlayer) && TargetSlot != CasterSlot && IsPlayerEnemy(Player(CasterSlot), Player(TargetSlot)))
				{
					if (GetPlayerController(Player(CasterSlot)) == 0 && GetPlayerSlotState(Player(CasterSlot)) == 1)
					{
						BOOL needcontinue = FALSE;

						if (DetectQuality >= 3)
						{
							for (unsigned int z = 0; z < FogHelperList.size(); z++)
							{
								if (TargetAddr == FogHelperList[z].UnitAddr)
								{
									if (DetectQuality == 3)
									{
										if (!(FogHelperList[z].FogState[CasterSlot][0] && FogHelperList[z].FogState[CasterSlot][1]))
										{
											needcontinue = TRUE;
										}
									}
									else
									{
										if (!(FogHelperList[z].FogState[CasterSlot][0] && FogHelperList[z].FogState[CasterSlot][1] && FogHelperList[z].FogState[CasterSlot][2]))
										{
											needcontinue = TRUE;
										}
									}
								}
							}
						}


						if (!needcontinue)
						{
							float unitx = 0.0f, unity = 0.0f, unitz = 0.0f;
							GetUnitLocation3D(TargetAddr, &unitx, &unity, &unitz);
							if (IsFoggedToPlayerMy(&unitx, &unity, Player(CasterSlot)))
							{
								LatestAbilSpell = CurGameTime;
								if (MinimapPingFogClick && IsReplayMode)
								{
									unsigned int PlayerColorInt = GetPlayerColorUINT(Player(CasterSlot));
									PingMinimapMy(&unitx, &unity, &pingduration, PlayerColorInt & 0x00FF0000, PlayerColorInt & 0x0000FF00, PlayerColorInt & 0x000000FF, false);
								}

								sprintf_s(PrintBuffer, 2048, "|c00EF4000[FogCW v16.1]|r: Player %s%s|r use ability(%s) in fogged %s%s|r|r[TARGET]\0",
									GetPlayerColorString(Player(CasterSlot)),
									GetPlayerName(CasterSlot, 0),
									ConvertIdToString(GetIssueOrderId).c_str(),
									GetPlayerColorString(Player(GetUnitOwnerSlot(TargetAddr))),
									GetObjectName(TargetAddr));

								ActionTime = CurGameTime;
								DisplayText(PrintBuffer, 15.0f);
								SendPause();
							}
							else if (!IsUnitVisibleToPlayer((unsigned char*)TargetAddr, (unsigned char*)GetPlayerByNumber(CasterSlot)))
							{
								LatestAbilSpell = CurGameTime;
								if (MinimapPingFogClick && IsReplayMode)
								{
									unsigned int PlayerColorInt = GetPlayerColorUINT(Player(CasterSlot));
									PingMinimapMy(&unitx, &unity, &pingduration, PlayerColorInt & 0x00FF0000, PlayerColorInt & 0x0000FF00, PlayerColorInt & 0x000000FF, false);
								}

								sprintf_s(PrintBuffer, 2048, "|c00EF4000[FogCW v16.1]|r: Player %s%s|r use ability(%s) in invisibled %s%s|r|r[TARGET]\0",
									GetPlayerColorString(Player(CasterSlot)),
									GetPlayerName(CasterSlot, 0),
									ConvertIdToString(GetIssueOrderId).c_str(),
									GetPlayerColorString(Player(GetUnitOwnerSlot(TargetAddr))),
									GetObjectName(TargetAddr));

								ActionTime = CurGameTime;
								DisplayText(PrintBuffer, 15.0f);
								SendPause();
							}
						}
					}
				}
			}
		}
		else if (DetectPointClicks && CasterUnitHandle > 0 && GetSpellTargetX != 0.0f && GetSpellTargetY != 0.0f)
		{
			int CasterAddr = GetHandleUnitAddress(CasterUnitHandle, 0);

			if (CasterAddr > 0)
			{
				int CasterOwner = GetUnitOwnerSlot(CasterAddr);

				float x = GetSpellTargetX;
				float y = GetSpellTargetY;
				int TargetAddr = GetUnitByXY(x, y, CasterOwner, TRUE);

				if (TargetAddr > 0 && (IsNotBadUnit(TargetAddr) && (IsHero(TargetAddr) || (!DetectRightClickOnlyHeroes && !IsUnitTower(TargetAddr)))))
				{
					BOOL needcontinue = FALSE;
					if (DetectQuality >= 3)
					{
						for (unsigned int z = 0; z < FogHelperList.size(); z++)
						{
							if (TargetAddr == FogHelperList[z].UnitAddr)
							{
								if (DetectQuality == 3)
								{
									if (!(FogHelperList[z].FogState[CasterOwner][0] && FogHelperList[z].FogState[CasterOwner][1]))
									{
										needcontinue = TRUE;
									}
								}
								else
								{
									if (!(FogHelperList[z].FogState[CasterOwner][0] && FogHelperList[z].FogState[CasterOwner][1]
										&& FogHelperList[z].FogState[CasterOwner][2]))
									{
										needcontinue = TRUE;
									}
								}
							}
						}
					}

					if (!needcontinue)
					{
						if (GetPlayerController(Player(CasterOwner)) == 0 && GetPlayerSlotState(Player(CasterOwner)) == 1)
						{
							float unitx = 0.0f, unity = 0.0f, unitz = 0.0f;
							GetUnitLocation3D(TargetAddr, &unitx, &unity, &unitz);
							if (IsFoggedToPlayerMy(&unitx, &unity, Player(CasterOwner)))
							{
								LatestAbilSpell = CurGameTime;
								if (MinimapPingFogClick && IsReplayMode)
								{
									unsigned int PlayerColorInt = GetPlayerColorUINT(Player(CasterOwner));
									PingMinimapMy(&unitx, &unity, &pingduration, PlayerColorInt & 0x00FF0000, PlayerColorInt & 0x0000FF00, PlayerColorInt & 0x000000FF, false);
								}

								sprintf_s(PrintBuffer, 2048, "|c00EF4000[FogCW v16.1]|r: Player %s%s|r use ability(%s) in fogged %s%s|r|r[POINT]\0",
									GetPlayerColorString(Player(CasterOwner)),
									GetPlayerName(CasterOwner, 0),
									ConvertIdToString(GetIssueOrderId).c_str(),
									GetPlayerColorString(Player(GetUnitOwnerSlot(TargetAddr))),
									GetObjectName(TargetAddr));

								ActionTime = CurGameTime;
								DisplayText(PrintBuffer, 15.0f);
								SendPause();
							}
							else if (!IsUnitVisibleToPlayer((unsigned char*)TargetAddr, (unsigned char*)GetPlayerByNumber(CasterOwner)))
							{
								LatestAbilSpell = CurGameTime;
								if (MinimapPingFogClick && IsReplayMode)
								{
									unsigned int PlayerColorInt = GetPlayerColorUINT(Player(CasterOwner));
									PingMinimapMy(&unitx, &unity, &pingduration, PlayerColorInt & 0x00FF0000, PlayerColorInt & 0x0000FF00, PlayerColorInt & 0x000000FF, false);
								}

								sprintf_s(PrintBuffer, 2048, "|c00EF4000[FogCW v16.1]|r: Player %s%s|r use ability(%s) in invisibled %s%s|r|r[POINT]\0",
									GetPlayerColorString(Player(CasterOwner)),
									GetPlayerName(CasterOwner, 0),
									ConvertIdToString(GetIssueOrderId).c_str(),
									GetPlayerColorString(Player(GetUnitOwnerSlot(TargetAddr))),
									GetObjectName(TargetAddr));

								ActionTime = CurGameTime;
								DisplayText(PrintBuffer, 15.0f);
								SendPause();
							}
						}
					}
				}
			}
		}
	}
}

void ProcessGetTriggerEventAction(int EventID, int TargetUnitHandle_, int CasterUnitHandle, int TargetItemHandle, int GetIssuedOrderId, float GetOrderPointX, float GetOrderPointY)
{
	if (EventID == 77 || EventID == 40)
	{
		BOOL IsItem = FALSE;
		int TargetUnitHandle = TargetUnitHandle_;
		if (TargetUnitHandle <= 0)
		{
			TargetUnitHandle = TargetItemHandle;
			if (TargetUnitHandle > 0)
				IsItem = TRUE;
			else
			{
				return;
			}
		}

		if (CasterUnitHandle > 0 && TargetUnitHandle > 0)
		{
			int CasterAddr = GetHandleUnitAddress(CasterUnitHandle, 0);
			int TargetAddr = (IsItem ? GetHandleItemAddress(TargetUnitHandle, 0) : GetHandleUnitAddress(TargetUnitHandle, 0));
			if (CasterAddr > 0 && TargetAddr > 0)
			{
				int CasterSlot = GetUnitOwnerSlot(CasterAddr);
				int TargetSlot = (IsItem ? 15 : GetUnitOwnerSlot(TargetAddr));

				if ((CasterSlot >= 0 && CasterSlot <= 15) && (TargetSlot >= 0 && TargetSlot <= 15) && Player(CasterSlot) > 0 && (IsItem || Player(TargetSlot) > 0))
				{
					BOOL IsOkay = IsItem ? (IsNotBadItem(TargetAddr) && !(GetIssuedOrderId >= 0xD0022 && GetIssuedOrderId <= 0xD0028))
						: (IsNotBadUnit(TargetAddr) && (IsHero(TargetAddr) || (!DetectRightClickOnlyHeroes && !IsUnitTower(TargetAddr))));

					if (TargetSlot != CasterSlot && (CasterSlot != GetLocalPlayerNumber() || DetectLocalPlayer) && IsOkay && (TargetSlot >= MAX_PLAYERS || IsPlayerEnemy(Player(CasterSlot), Player(TargetSlot))))
					{
						if (GetPlayerController(Player(CasterSlot)) == 0 && GetPlayerSlotState(Player(CasterSlot)) == 1)
						{
							BOOL needcontinue = FALSE;

							if (!IsItem)
							{
								if (DetectQuality >= 3)
								{
									for (unsigned int z = 0; z < FogHelperList.size(); z++)
									{
										if (TargetAddr == FogHelperList[z].UnitAddr)
										{
											if (DetectQuality == 3)
											{
												if (!(FogHelperList[z].FogState[CasterSlot][0] && FogHelperList[z].FogState[CasterSlot][1]))
												{
													needcontinue = TRUE;
												}
											}
											else
											{
												if (!(FogHelperList[z].FogState[CasterSlot][0] && FogHelperList[z].FogState[CasterSlot][1]
													&& FogHelperList[z].FogState[CasterSlot][2]))
												{
													needcontinue = TRUE;
												}
											}
										}
									}
								}
							}


							if (!needcontinue)
							{
								float unitx = 0.0f, unity = 0.0f, unitz = 0.0f;

								if (IsItem)
								{
									GetItemLocation3D(TargetAddr, &unitx, &unity, &unitz);
								}
								else
								{
									GetUnitLocation3D(TargetAddr, &unitx, &unity, &unitz);
								}

								if (IsFoggedToPlayerMy(&unitx, &unity, Player(CasterSlot)))
								{
									LatestAbilSpell = CurGameTime;
									if (MinimapPingFogClick && IsReplayMode)
									{
										unsigned int PlayerColorInt = GetPlayerColorUINT(Player(CasterSlot));
										PingMinimapMy(&unitx, &unity, &pingduration, PlayerColorInt & 0x00FF0000, PlayerColorInt & 0x0000FF00, PlayerColorInt & 0x000000FF, false);
									}
									sprintf_s(PrintBuffer, 2048, "|c00EF4000[FogCW v16.1]|r: Player %s%s|r use %s in fogged %s %s%s|r|r[TARGET]\0",
										GetPlayerColorString(Player(CasterSlot)),
										GetPlayerName(CasterSlot, 0),
										ConvertIdToString(GetIssuedOrderId).c_str(),
										IsItem ? "[item]" : "[unit]",
										IsItem ? "|c004B4B4B" : GetPlayerColorString(Player(TargetSlot)),
										GetObjectName(TargetAddr));

									ActionTime = CurGameTime;
									DisplayText(PrintBuffer, 15.0f);
									SendPause();
								}
								else if (!IsItem && !IsUnitVisibleToPlayer((unsigned char*)TargetAddr, (unsigned char*)GetPlayerByNumber(CasterSlot)))
								{
									LatestAbilSpell = CurGameTime;
									if (MinimapPingFogClick && IsReplayMode)
									{
										unsigned int PlayerColorInt = GetPlayerColorUINT(Player(CasterSlot));
										PingMinimapMy(&unitx, &unity, &pingduration, PlayerColorInt & 0x00FF0000, PlayerColorInt & 0x0000FF00, PlayerColorInt & 0x000000FF, false);
									}
									sprintf_s(PrintBuffer, 2048, "|c00EF4000[FogCW v16.1]|r: Player %s%s|r use %s in invisibled %s%s|r|r[TARGET]\0",
										GetPlayerColorString(Player(CasterSlot)),
										GetPlayerName(CasterSlot, 0),
										ConvertIdToString(GetIssuedOrderId).c_str(),
										GetPlayerColorString(Player(TargetSlot)),
										GetObjectName(TargetAddr));

									ActionTime = CurGameTime;
									DisplayText(PrintBuffer, 15.0f);
									SendPause();
								}
								else if (IsItem && (DetectItemDestroyer || DetectOwnItems) && GetIssuedOrderId == 0xD000F)
								{
									if ((GetItemOwner(TargetAddr) != CasterSlot && DetectItemDestroyer) || DetectOwnItems)
									{
										if (!IsPlayerEnemy(Player(CasterSlot), Player(GetItemOwner(TargetAddr))))
										{
											if (MinimapPingFogClick && IsReplayMode)
											{
												unsigned int PlayerColorInt = GetPlayerColorUINT(Player(CasterSlot));
												PingMinimapMy(&unitx, &unity, &pingduration, PlayerColorInt & 0x00FF0000, PlayerColorInt & 0x0000FF00, PlayerColorInt & 0x000000FF, false);
											}
											LatestAbilSpell = CurGameTime;
											sprintf_s(PrintBuffer, 2048, "|c00EF4000[FogCW v16.1]|r: Player %s%s|r try to destroy item %s%s|r|r\0",
												GetPlayerColorString(Player(CasterSlot)),
												GetPlayerName(CasterSlot, 0),
												GetPlayerColorString(Player(TargetSlot)),
												GetObjectName(TargetAddr));

											ActionTime = CurGameTime;
											DisplayText(PrintBuffer, 15.0f);
											SendPause();
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

	if (EventID == 76 || EventID == 39)
	{
		if (CasterUnitHandle > 0)
		{
			int CasterAddr = GetHandleUnitAddress(CasterUnitHandle, 0);

			float x = GetOrderPointX;
			float y = GetOrderPointY;
			float unitx = 0.0f, unity = 0.0f, unitz = 0.0f;
			if (CasterAddr > 0 && GetOrderPointX != 0.0f && GetOrderPointY != 0.0f)
			{
				int CasterSlot = GetUnitOwnerSlot(CasterAddr);

				if ((CasterSlot >= 0 && CasterSlot <= 15) && Player(CasterSlot) > 0)
				{
					if (GetPlayerController(Player(GetUnitOwnerSlot(CasterAddr))) == 0 && GetPlayerSlotState(Player(GetUnitOwnerSlot(CasterAddr))) == 1)
					{
						int TargetAddr = GetUnitByXY(x, y, GetUnitOwnerSlot(CasterAddr));
						BOOL DetectionImpossibleClick = FALSE;

						if (TargetAddr > 0 && (IsNotBadUnit(TargetAddr) && (IsHero(TargetAddr) || (DetectImpossibleClicks && IsDetectedTower(TargetAddr)) || (!DetectRightClickOnlyHeroes && !IsUnitTower(TargetAddr)))))
						{
							int TargetSlot = GetUnitOwnerSlot(TargetAddr);
							if ((TargetSlot >= 0 && TargetSlot <= 15) && Player(TargetSlot) > 0)
							{

								BOOL needcontinue = FALSE;

								if (DetectQuality >= 3)
								{
									for (unsigned int z = 0; z < FogHelperList.size(); z++)
									{
										if (TargetAddr == FogHelperList[z].UnitAddr)
										{
											if (DetectQuality == 3)
											{
												if (!(FogHelperList[z].FogState[GetUnitOwnerSlot(CasterAddr)][0] && FogHelperList[z].FogState[GetUnitOwnerSlot(CasterAddr)][1]))
												{
													needcontinue = TRUE;
												}
											}
											else
											{
												if (!(FogHelperList[z].FogState[GetUnitOwnerSlot(CasterAddr)][0] && FogHelperList[z].FogState[GetUnitOwnerSlot(CasterAddr)][1]
													&& FogHelperList[z].FogState[GetUnitOwnerSlot(CasterAddr)][2]))
												{
													needcontinue = TRUE;
												}
											}
										}
									}
								}

								if (!needcontinue)
								{
									GetUnitLocation3D(TargetAddr, &unitx, &unity, &unitz);
									if (DetectImpossibleClicks && IsDetectedTower(TargetAddr))
									{
										DetectionImpossibleClick = TRUE;
									}

									if ((DetectPointClicks || DetectionImpossibleClick) && IsFoggedToPlayerMy(&unitx, &unity, Player(GetUnitOwnerSlot(CasterAddr))))
									{
										LatestAbilSpell = CurGameTime;
										if (MinimapPingFogClick && IsReplayMode)
										{
											unsigned int PlayerColorInt = GetPlayerColorUINT(Player(GetUnitOwnerSlot(CasterAddr)));
											PingMinimapMy(&unitx, &unity, &pingduration, PlayerColorInt & 0x00FF0000, PlayerColorInt & 0x0000FF00, PlayerColorInt & 0x000000FF, false);
										}

										sprintf_s(PrintBuffer, 2048, "|c00EF4000[FogCW v16.1]|r: Player %s%s|r use %s in fogged [unit] %s%s|r|r[POINT]\0",
											GetPlayerColorString(Player(GetUnitOwnerSlot(CasterAddr))),
											GetPlayerName(GetUnitOwnerSlot(CasterAddr), 0),
											ImpossibleClick ? "-HACKCLICK-" : ConvertIdToString(GetIssuedOrderId).c_str(),
											GetPlayerColorString(Player(GetUnitOwnerSlot(TargetAddr))),
											GetObjectName(TargetAddr));

										ActionTime = CurGameTime;
										DisplayText(PrintBuffer, 15.0f);
										ImpossibleClick = FALSE;
										SendPause();
									}
									else if (DetectPointClicks && !DetectionImpossibleClick && !IsUnitVisibleToPlayer((unsigned char*)TargetAddr, (unsigned char*)GetPlayerByNumber(GetUnitOwnerSlot(CasterAddr))))
									{
										LatestAbilSpell = CurGameTime;
										if (MinimapPingFogClick && IsReplayMode)
										{
											unsigned int PlayerColorInt = GetPlayerColorUINT(Player(GetUnitOwnerSlot(CasterAddr)));
											PingMinimapMy(&unitx, &unity, &pingduration, PlayerColorInt & 0x00FF0000, PlayerColorInt & 0x0000FF00, PlayerColorInt & 0x000000FF, false);
										}

										sprintf_s(PrintBuffer, 2048, "|c00EF4000[FogCW v16.1]|r: Player %s%s|r use %s in invisibled %s%s|r|r[POINT]\0",
											GetPlayerColorString(Player(GetUnitOwnerSlot(CasterAddr))),
											GetPlayerName(GetUnitOwnerSlot(CasterAddr), 0),
											ConvertIdToString(GetIssuedOrderId).c_str(),
											GetPlayerColorString(Player(GetUnitOwnerSlot(TargetAddr))),
											GetObjectName(TargetAddr));

										ActionTime = CurGameTime;
										DisplayText(PrintBuffer, 15.0f);
										SendPause();
									}
								}
							}
							else if (DetectPointClicks && TargetAddr > 0 && IsNotBadItem(TargetAddr) && !(GetIssuedOrderId >= 0xD0022 && GetIssuedOrderId <= 0xD0028))
							{
								float xunitx = 0.0f, xunity = 0.0f, xunitz = 0.0f;
								GetItemLocation3D(TargetAddr, &xunitx, &xunity, &xunitz);
								if (IsFoggedToPlayerMy(&xunitx, &xunity, Player(GetUnitOwnerSlot(CasterAddr))))
								{
									LatestAbilSpell = CurGameTime;
									if (MinimapPingFogClick && IsReplayMode)
									{
										unsigned int PlayerColorInt = GetPlayerColorUINT(Player(GetUnitOwnerSlot(CasterAddr)));
										PingMinimapMy(&xunitx, &xunity, &pingduration, PlayerColorInt & 0x00FF0000, PlayerColorInt & 0x0000FF00, PlayerColorInt & 0x000000FF, false);
									}


									sprintf_s(PrintBuffer, 2048, "|c00EF4000[FogCW v16.1]|r: Player %s%s|r use %s in fogged [item] %s%s|r|r[POINT]\0",
										GetPlayerColorString(Player(GetUnitOwnerSlot(CasterAddr))),
										GetPlayerName(GetUnitOwnerSlot(CasterAddr), 0),
										ConvertIdToString(GetIssuedOrderId).c_str(),
										"|c004B4B4B",
										GetObjectName(TargetAddr));

									ActionTime = CurGameTime;
									DisplayText(PrintBuffer, 15.0f);
									SendPause();
								}
							}
						}
					}
				}
			}
			else if (CasterAddr > 0 && (GetUnitOwnerSlot(CasterAddr) != GetLocalPlayerNumber() || DetectLocalPlayer) && x == 0.0f && y == 0.0f)
			{
				if (GetIssuedOrderId == 851971)
				{
					LatestAbilSpell = CurGameTime;

					sprintf_s(PrintBuffer, 2048, "|c00EF4000[FogCW v16.1]|r: Player %s%s|r activate GuAI Maphack!!\0",
						GetPlayerColorString(Player(GetUnitOwnerSlot(CasterAddr))),
						GetPlayerName(GetUnitOwnerSlot(CasterAddr), 0));

					ActionTime = CurGameTime;
					DisplayText(PrintBuffer, 15.0f);
					SendPause();
				}
			}
		}

	}
}

// Every 10ms
void ActionProcessFunction()
{
	for (const ProcessNewAction& CurrentAction : ProcessNewActionList)
	{
		if (CurrentAction.IsGetSpellAbilityId)
		{
			ProcessGetSpellAbilityIdAction(CurrentAction.EventID, CurrentAction.TargetUnitHandle, CurrentAction.CasterUnitHandle, CurrentAction.TargetItemHandle, CurrentAction.GetIssuedOrderId, CurrentAction.GetSpellOrderTargetX, CurrentAction.GetSpellOrderTargetY);
		}
		else
		{
			ProcessGetTriggerEventAction(CurrentAction.EventID, CurrentAction.TargetUnitHandle, CurrentAction.CasterUnitHandle, CurrentAction.TargetItemHandle, CurrentAction.GetIssuedOrderId, CurrentAction.GetSpellOrderTargetX, CurrentAction.GetSpellOrderTargetY);
		}
	}
	ProcessNewActionList.clear();
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

long long GetTriggerEventIdTime = 0;

int __cdecl GetTriggerEventId_hooked()
{
	if (!GameStarted || GetTriggerEventIdCalled)
		return GetTriggerEventId_real();

	GetTriggerEventIdCalled = TRUE;

	int TriggerEventId = GetTriggerEventId_real();

	ProcessNewAction tmpProcessNewAction;
	tmpProcessNewAction.CasterUnitHandle = GetTriggerUnit();
	tmpProcessNewAction.EventID = TriggerEventId;
	tmpProcessNewAction.GetIssuedOrderId = GetIssuedOrderId_real();
	tmpProcessNewAction.IsGetSpellAbilityId = FALSE;
	tmpProcessNewAction.SkillID = GetSpellAbilityId_real();
	tmpProcessNewAction.TargetItemHandle = GetOrderTargetItem();
	tmpProcessNewAction.TargetUnitHandle = GetOrderTargetUnit();
	tmpProcessNewAction.GetSpellOrderTargetX = _GetOrderPointX().fl;
	tmpProcessNewAction.GetSpellOrderTargetY = _GetOrderPointY().fl;

	if (GetTriggerEventIdTime != CurGameTime)
	{
		ProcessNewActionList.push_back(tmpProcessNewAction);
	}

	GetTriggerEventIdTime = CurGameTime;

	GetTriggerEventId_CasterUnitHandle = tmpProcessNewAction.CasterUnitHandle;
	GetTriggerEventId_EventID = tmpProcessNewAction.EventID;
	GetTriggerEventId_GetIssuedOrderId = tmpProcessNewAction.GetIssuedOrderId;
	GetTriggerEventId_SkillID = tmpProcessNewAction.SkillID;
	GetTriggerEventId_TargetItemHandle = tmpProcessNewAction.TargetItemHandle;
	GetTriggerEventId_TargetUnitHandle = tmpProcessNewAction.TargetUnitHandle;
	GetTriggerEventId_x = tmpProcessNewAction.GetSpellOrderTargetX;
	GetTriggerEventId_y = tmpProcessNewAction.GetSpellOrderTargetY;

	BotDetector(tmpProcessNewAction.SkillID, tmpProcessNewAction.EventID, tmpProcessNewAction.CasterUnitHandle);

	GetTriggerEventIdCalled = FALSE;
	return TriggerEventId;
}


int GetSpellAbilityId_CasterUnitHandle = 0;
int GetSpellAbilityId_EventID = 0;
int GetSpellAbilityId_GetIssuedOrderId = 0;
int GetSpellAbilityId_SkillID = 0;
int GetSpellAbilityId_TargetItemHandle = 0;
int GetSpellAbilityId_TargetUnitHandle = 0;

long long GetSpellAbilityIdTime = 0;

BOOL GetSpellAbilityIdCalled = FALSE;
int __cdecl GetSpellAbilityId_hooked()
{
	if (!GameStarted || GetSpellAbilityIdCalled)
		return GetSpellAbilityId_real();

	GetSpellAbilityIdCalled = TRUE;

	int SpellAbilityId = GetSpellAbilityId_real();

	ProcessNewAction tmpProcessNewAction;
	tmpProcessNewAction.CasterUnitHandle = GetSpellAbilityUnit_real();
	tmpProcessNewAction.EventID = GetTriggerEventId_real();
	tmpProcessNewAction.GetIssuedOrderId = GetIssuedOrderId_real();
	tmpProcessNewAction.IsGetSpellAbilityId = TRUE;
	tmpProcessNewAction.SkillID = SpellAbilityId;
	tmpProcessNewAction.TargetItemHandle = GetOrderTargetItem();
	tmpProcessNewAction.TargetUnitHandle = GetSpellTargetUnit();
	tmpProcessNewAction.GetSpellOrderTargetX = _GetSpellTargetX().fl;
	tmpProcessNewAction.GetSpellOrderTargetY = _GetSpellTargetY().fl;

	if (GetSpellAbilityIdTime != CurGameTime)
	{
		ProcessNewActionList.push_back(tmpProcessNewAction);
	}

	GetSpellAbilityIdTime = CurGameTime;

	GetSpellAbilityId_CasterUnitHandle = tmpProcessNewAction.CasterUnitHandle;
	GetSpellAbilityId_EventID = tmpProcessNewAction.EventID;
	GetSpellAbilityId_GetIssuedOrderId = tmpProcessNewAction.GetIssuedOrderId;
	GetSpellAbilityId_SkillID = tmpProcessNewAction.SkillID;
	GetSpellAbilityId_TargetItemHandle = tmpProcessNewAction.TargetItemHandle;
	GetSpellAbilityId_TargetUnitHandle = tmpProcessNewAction.TargetUnitHandle;
	GetTriggerEventId_x = tmpProcessNewAction.GetSpellOrderTargetX;
	GetTriggerEventId_y = tmpProcessNewAction.GetSpellOrderTargetY;

	GetSpellAbilityIdCalled = FALSE;
	return SpellAbilityId;
}

BOOL GetIssuedOrderIdCalled = FALSE;

int __cdecl GetIssuedOrderId_hooked()
{
	if (GetIssuedOrderIdCalled)
	{
		return GetIssuedOrderId_real();
	}

	GetIssuedOrderIdCalled = TRUE;
	GetTriggerEventId_hooked();
	GetIssuedOrderIdCalled = FALSE;
	return GetIssuedOrderId_real();
}

BOOL GetSpellAbilityUnitCalled = FALSE;

int __cdecl GetSpellAbilityUnit_hooked()
{
	if (GetSpellAbilityUnitCalled)
		return GetSpellAbilityUnit_real();

	GetSpellAbilityUnitCalled = TRUE;
	if (GetSpellAbilityId_hooked() > 0)
	{
		GetSpellAbilityUnitCalled = FALSE;
		return GetSpellAbilityUnit_real();
	}
	GetTriggerEventId_hooked();
	GetSpellAbilityUnitCalled = FALSE;
	return GetSpellAbilityUnit_real();
}

int __cdecl GetAttacker_hooked()
{
	GetTriggerEventId_hooked();
	//GetSpellAbilityId_hooked( );
	return GetAttacker_real();
}


//FogHelperListTemp.clear( );

long long LatestFogCheck = 0;

void UpdateFogHelper()
{
	if (llabs(CurGameTime - LatestFogCheck) > 150)
	{
		LatestFogCheck = CurGameTime;
		for (int n = 0; n < MAX_PLAYERS; n++)
		{
			if (!DetectLocalPlayer && GetLocalPlayerNumber() == n)
				continue;

			if (!Player(n))
				continue;

			if (GetPlayerController(Player(n)) != 0 || GetPlayerSlotState(Player(n)) != 1)
				continue;

			int CurrentUnit = 0;
			if (SafeUnitCount > 0)
			{
				for (int i = 0; i < SafeUnitCount; i++)
				{
					CurrentUnit = SafeUnitArray[i];

					if (IsNotBadUnit(CurrentUnit))
					{
						if (IsHero(CurrentUnit) || !DetectRightClickOnlyHeroes)
						{
							float unitx, unity, unitz;

							GetUnitLocation3D(CurrentUnit, &unitx, &unity, &unitz);

							BOOL FoundUnit = FALSE;
							for (auto& UnitFogHelper : FogHelperList)
							{
								if (CurrentUnit == UnitFogHelper.UnitAddr
									&& llabs(CurGameTime - UnitFogHelper.LatestTime) > 400)
								{
									UnitFogHelper.LatestTime = CurGameTime;
									FoundUnit = TRUE;
									if (DetectQuality == 3)
									{
										UnitFogHelper.FogState[n][0] = UnitFogHelper.FogState[n][1];
										UnitFogHelper.FogState[n][1] = (IsFoggedToPlayerMy(&unitx, &unity, Player(n)) || !IsUnitVisibleToPlayer((unsigned char*)CurrentUnit, (unsigned char*)GetPlayerByNumber(n)));
									}
									else
									{
										UnitFogHelper.FogState[n][0] = UnitFogHelper.FogState[n][1];
										UnitFogHelper.FogState[n][1] = UnitFogHelper.FogState[n][2];
										UnitFogHelper.FogState[n][2] = (IsFoggedToPlayerMy(&unitx, &unity, Player(n)) || !IsUnitVisibleToPlayer((unsigned char*)CurrentUnit, (unsigned char*)GetPlayerByNumber(n)));
									}
								}
							}
							if (!FoundUnit)
							{
								FogHelper tmpghelp;
								for (int z = 0; z < MAX_PLAYERS; z++)
								{
									tmpghelp.FogState[z][0] = TRUE;
									tmpghelp.FogState[z][1] = TRUE;
									tmpghelp.FogState[z][2] = TRUE;
								}
								tmpghelp.UnitAddr = CurrentUnit;
								FogHelperList.push_back(tmpghelp);


								if (FogHelperList.size() >= 1500)
								{
									FogHelperList.erase(FogHelperList.begin());
								}
							}

						}
					}
				}
			}
		}
	}
}


int PlayerSelectedItems[MAX_PLAYERS];

// Units initially visibled

// 1 is new visible unit
int LatestVisibledUnits[MAX_PLAYERS];
// 2 is any visible unit
int LatestVisibledUnits2[MAX_PLAYERS];

long long LatestVisibleUnitTime = 0;

int LatestSelectedUnits[MAX_PLAYERS];
int PrevSelectedUnits[MAX_PLAYERS];

int PossibleStrike[MAX_PLAYERS] = { 0 };

void SearchPlayersFogSelect()
{
	FillUnitCountAndUnitArray();
	if (DetectQuality >= 3)
		UpdateFogHelper();

	for (int i = 0; i < MAX_PLAYERS; i++)
	{
		if (!DetectLocalPlayer && GetLocalPlayerNumber() == i)
			continue;

		int hCurrentPlayer = Player(i);
		if (hCurrentPlayer <= 0)
			continue;

		if (GetPlayerController(Player(i)) != 0 || GetPlayerSlotState(Player(i)) != 1)
			continue;

		int selectedunit = GetSelectedUnit(i);

	/*	if (IsKeyPressed('X'))
		{
			int plr = GetPlayerByNumber(i);
			if (plr > 0)
			{
				int PlayerData1 = *(int*)(plr + 0x34);
				if (PlayerData1 > 0)
				{
					int unit1 = *(int*)(PlayerData1 + 0x1E0);
					int unit2 = *(int*)(PlayerData1 + 0x1A4);
					int unitcount = *(int*)(PlayerData1 + 0x10);
					int unitcount2 = *(int*)(PlayerData1 + 0x1D4);

					float scale = 1.0f;
					int unitData = 0;

					if (selectedunit)
					{
						unitData = GetTypeIdInfoAddr(GetObjectTypeId(selectedunit), 0);
						if (unitData > 0)
						{
							scale = *(float*)(unitData + 0x54) * 50.0f;
						}
					}

					int somedata = 0;
					int somedata2 = 0;

					if (selectedunit)
					{
						somedata = GetSomeAddr(*(unsigned int*)(selectedunit + 0xA0), *(unsigned int*)(selectedunit + 0xA4));
						somedata2 = GetSomeAddr(*(unsigned int*)(selectedunit + 0xC0), *(unsigned int*)(selectedunit + 0xC4));
					}

					char debug[256];
					sprintf_s(debug, "Name:[%s] id=%i poffs=%X pdata=%X U1:%X U22: %X & %i=%i DA:%X SC:%f SD:%X/%X\n", GetPlayerName(i, 0), i, plr, PlayerData1, unit1
						, unit2, unitcount, unitcount2, unitData, scale, somedata, somedata2);
					DisplayText(debug, 20.0f);
				}
			}
		}*/


		if (selectedunit <= 0)
			continue;

		if (!IsNotBadUnit(selectedunit))
			continue;

		int OwnedPlayerSlot = GetUnitOwnerSlot(selectedunit);
		if (OwnedPlayerSlot < 0 || OwnedPlayerSlot > 15)
			continue;

		for (auto& UnitClick : UnitClickList)
		{
			if (UnitClick.PlayerNum == i)
			{
				if (UnitClick.SelectCount >= 0 && UnitClick.LatestTime != 0 && IsNotBadUnit(UnitClick.UnitAddr))
				{
					if (llabs(CurGameTime - UnitClick.LatestTime) > 200 * DetectQuality)
					{
						BOOL possible = UnitClick.initialVisibled || selectedunit == LatestVisibledUnits[i];

						sprintf_s(PrintBuffer, 2048, "|c00EF4000[FogCW v16.1]|r: Player %s%s|r select invisibled %s%s|r|r [%s]\0",
							GetPlayerColorString(Player(i)),
							GetPlayerName(i, 0),
							GetPlayerColorString(Player(OwnedPlayerSlot)),
							GetObjectName(UnitClick.UnitAddr),
							possible ? "FALSE" : "POSSIBLE");

						ActionTime = UnitClick.LatestTime;
						if (possible && !DisplayFalse)
						{
							int seconds = (int)(ActionTime / 1000) % 60;
							int minutes = (int)((ActionTime / (1000 * 60)) % 60);
							int hours = (int)((ActionTime / (1000 * 60 * 60)) % 24);

							char timeBuff[64];
							sprintf_s(timeBuff, "[%.2d:%.2d:%.2d] : ", hours, minutes, seconds);

							std::string outLineStr = (timeBuff + std::string(PrintBuffer));
							if (LoggingType != -1)
							{
								WatcherLog((outLineStr + "\n").c_str());
							}
						}
						else
						{
							DisplayText(PrintBuffer, 15.0f);
							SendPause();
						}

						UnitClick.SelectCount = -1;
						if (LatestVisibledUnits[i] == -1)
							LatestVisibledUnits[i] = UnitClick.UnitAddr;
					}
				}
			}
		}

		// ���� ���� � ���������� ��� ��� ������� - ����������
		if (selectedunit == LatestVisibledUnits2[i])
			continue;

		// ���� ��� ��������� ����� ��������� ���� �� ����������.
		if (selectedunit == LatestVisibledUnits[i])
			continue;

		LatestVisibledUnits2[i] = -1;
		LatestVisibledUnits[i] = -1;

		int hOwnerPlayer = Player(OwnedPlayerSlot);
		if (hOwnerPlayer <= 0)
			continue;

		if (hOwnerPlayer == hCurrentPlayer)
			continue;

		if (IsPlayerEnemy(hOwnerPlayer, hCurrentPlayer) || OwnedPlayerSlot >= MAX_PLAYERS)
		{
			if (LatestSelectedUnits[i] != PrevSelectedUnits[i])
				PrevSelectedUnits[i] = LatestSelectedUnits[i];
			LatestSelectedUnits[i] = selectedunit;

			float unitx = 0.0f, unity = 0.0f, unitz = 0.0f;
			GetUnitLocation3D(selectedunit, &unitx, &unity, &unitz);


			if (IsFoggedToPlayerMy(&unitx, &unity, hCurrentPlayer) || !IsUnitVisibleToPlayer((unsigned char*)selectedunit, (unsigned char*)GetPlayerByNumber(i)))
			{
				BOOL found = FALSE;
				BOOL needcontinue = FALSE;

				UnitSelectedStruct* tmpunitselected = NULL;

				for (auto& UnitClick : UnitClickList)
				{
					if (UnitClick.PlayerNum == i)
					{
						if ((UnitClick.SelectCount >= 0 || UnitClick.SelectCount == -5) && UnitClick.UnitAddr == selectedunit)
						{
							found = TRUE;
							tmpunitselected = &UnitClick;
							if (UnitClick.SelectCount == -5)
								UnitClick.SelectCount = 0;
						}
					}
				}

				for (auto& UnitClick : UnitClickList)
				{
					if (UnitClick.PlayerNum == i)
					{
						if (UnitClick.LatestTime != 0 && llabs(CurGameTime - UnitClick.LatestTime) < 5000 && UnitClick.SelectCount == -1 && UnitClick.UnitAddr == selectedunit)
						{
							needcontinue = TRUE;
							break;
						}
					}
				}

				if (DetectQuality >= 3 && !needcontinue)
				{
					for (unsigned int z = 0; z < FogHelperList.size(); z++)
					{
						if (selectedunit == FogHelperList[z].UnitAddr)
						{
							if (DetectQuality == 3)
							{
								if (!(FogHelperList[z].FogState[i][0] && FogHelperList[z].FogState[i][1]))
								{
									needcontinue = TRUE;
								}
							}
							else
							{
								if (!(FogHelperList[z].FogState[i][0] && FogHelperList[z].FogState[i][1]
									&& FogHelperList[z].FogState[i][2]))
								{
									needcontinue = TRUE;
								}
							}
						}
					}
				}

				if (needcontinue)
				{
					continue;
				}

				if (!found)
				{
					UnitSelectedStruct tmpus;
					tmpus.PlayerNum = i;
					tmpus.UnitAddr = selectedunit;
					tmpus.LatestTime = CurGameTime;
					tmpus.SelectCount = 0;
					tmpus.initialVisibled = FALSE;

					UnitClickList.push_back(tmpus);
					tmpunitselected = &UnitClickList[UnitClickList.size() - 1];

					if (UnitClickList.size() >= 1000)
					{
						UnitClickList.erase(UnitClickList.begin());
					}
				}

				if (tmpunitselected && tmpunitselected->SelectCount >= 0)
				{
					if (tmpunitselected->SelectCount < 2)
					{
						if (llabs(CurGameTime - tmpunitselected->LatestTime) > 10 * DetectQuality)
						{
							tmpunitselected->SelectCount++;
							tmpunitselected->LatestTime = CurGameTime;
						}
					}
					else
					{
						if (llabs(CurGameTime - tmpunitselected->LatestTime) > 100 * DetectQuality)
						{
							BOOL possible = tmpunitselected->initialVisibled || selectedunit == LatestVisibledUnits[i];

							if (!IsFoggedToPlayerMy(&unitx, &unity, hCurrentPlayer))
							{
								if (MinimapPingFogClick && IsReplayMode)
								{
									unsigned int PlayerColorInt = GetPlayerColorUINT(Player(i));
									PingMinimapMy(&unitx, &unity, &pingduration, PlayerColorInt & 0x00FF0000, PlayerColorInt & 0x0000FF00, PlayerColorInt & 0x000000FF, false);
								}
								sprintf_s(PrintBuffer, 2048, "|c00EF4000[FogCW v16.1]|r: Player %s%s|r select invisibled %s%s|r|r [%s]",
									GetPlayerColorString(Player(i)),
									GetPlayerName(i, 0),
									GetPlayerColorString(Player(OwnedPlayerSlot)),
									GetObjectName(selectedunit),
									possible ? "FALSE" : "DETECTED");

								if (PrevSelectedUnits[i] != LatestVisibledUnits[i] &&
									LatestSelectedUnits[i] != LatestVisibledUnits[i])
								{
									LatestVisibledUnits[i] = -1;
								}

								if (possible)
								{
									PossibleStrike[i]++;
								}
								else
								{
									PossibleStrike[i] = 0;
								}

								if (DebugState)
								{
									char detectUnitAndPlayer[64];
									sprintf_s(detectUnitAndPlayer, "%X=%X", GetPlayerByNumber(i), selectedunit);
									DisplayText(detectUnitAndPlayer, 15.0f);
								}
							}
							else
							{
								if (MinimapPingFogClick && IsReplayMode)
								{
									unsigned int PlayerColorInt = GetPlayerColorUINT(Player(i));
									PingMinimapMy(&unitx, &unity, &pingduration, PlayerColorInt & 0x00FF0000, PlayerColorInt & 0x0000FF00, PlayerColorInt & 0x000000FF, false);
								}

								sprintf_s(PrintBuffer, 2048, "|c00EF4000[FogCW v16.1]|r: Player %s%s|r select fogged %s%s|r|r [%s]\0",
									GetPlayerColorString(Player(i)),
									GetPlayerName(i, 0),
									GetPlayerColorString(Player(OwnedPlayerSlot)),
									GetObjectName(selectedunit),
									possible ? "FALSE" : "DETECTED");


								if (PrevSelectedUnits[i] != LatestVisibledUnits[i] &&
									LatestSelectedUnits[i] != LatestVisibledUnits[i])
								{
									LatestVisibledUnits[i] = -1;
								}

								if (possible)
								{
									PossibleStrike[i]++;
								}
								else
								{
									PossibleStrike[i] = 0;
								}

								if (DebugState)
								{
									char detectUnitAndPlayer[64];
									sprintf_s(detectUnitAndPlayer, "%X=%X", GetPlayerByNumber(i), selectedunit);
									DisplayText(detectUnitAndPlayer, 15.0f);
								}
							}
							tmpunitselected->SelectCount = -1;

							ActionTime = tmpunitselected->LatestTime;

							if (PossibleStrike[i] <= 1)
							{
								if (possible && !DisplayFalse)
								{
									int seconds = (int)(ActionTime / 1000) % 60;
									int minutes = (int)((ActionTime / (1000 * 60)) % 60);
									int hours = (int)((ActionTime / (1000 * 60 * 60)) % 24);

									char timeBuff[64];
									sprintf_s(timeBuff, "[%.2d:%.2d:%.2d] : ", hours, minutes, seconds);

									std::string outLineStr = (timeBuff + std::string(PrintBuffer));
									if (LoggingType != -1)
									{
										WatcherLog((outLineStr + "\n").c_str());
									}
								}
								else
								{
									DisplayText(PrintBuffer, 15.0f);
									SendPause();
								}
							}
						}
					}
				}
			}
			else
			{
				LatestVisibledUnits2[i] = selectedunit;

				BOOL found = FALSE;
				for (auto& UnitClick : UnitClickList)
				{
					if (UnitClick.PlayerNum == i)
					{
						if (UnitClick.UnitAddr == selectedunit)
						{
							LatestVisibledUnits[i] = selectedunit;
							UnitClick.initialVisibled = TRUE;
						}
						if ((UnitClick.SelectCount >= 0 || UnitClick.SelectCount == -5) && UnitClick.UnitAddr == selectedunit)
						{
							found = TRUE;
						}
					}
				}

				if (!found)
				{
					UnitSelectedStruct tmpus;
					tmpus.PlayerNum = i;
					tmpus.UnitAddr = selectedunit;
					tmpus.LatestTime = CurGameTime;
					tmpus.SelectCount = -5;
					tmpus.initialVisibled = TRUE;
					LatestVisibleUnitTime = CurGameTime;
					LatestVisibledUnits[i] = selectedunit;
					UnitClickList.push_back(tmpus);
					if (UnitClickList.size() >= 1000)
					{
						UnitClickList.erase(UnitClickList.begin());
					}
				}
			}
		}
	}
}

void CreateFogClickWatcherConfig()
{
	/*FILE* f = NULL;
	fopen_s(&f, detectorConfigPath.c_str(), "w");
	if (f != NULL)
	{
		fclose(f);
	}*/
	CIniWriter fogwatcherconf(detectorConfigPath.c_str());
	fogwatcherconf.WriteInt("FogClickWatcher", "LoggingType", 1);
	fogwatcherconf.WriteBool("FogClickWatcher", "LocalPlayerEnable", TRUE);
	fogwatcherconf.WriteBool("FogClickWatcher", "GetTriggerEventId", TRUE);
	fogwatcherconf.WriteBool("FogClickWatcher", "GetSpellAbilityId", TRUE);
	fogwatcherconf.WriteBool("FogClickWatcher", "GetIssuedOrderId", TRUE);
	fogwatcherconf.WriteBool("FogClickWatcher", "GetAttacker", TRUE);
	fogwatcherconf.WriteBool("FogClickWatcher", "GetSpellAbilityUnit", TRUE);
	fogwatcherconf.WriteInt("FogClickWatcher", "UnitDetectionMethod", 1);
	fogwatcherconf.WriteInt("FogClickWatcher", "MeepoPoofID", 0x41304E38);
	fogwatcherconf.WriteBool("FogClickWatcher", "DetectRightClickOnlyHeroes", FALSE);
	//fogwatcherconf.WriteBool("FogClickWatcher", "MinimapPingFogClick", FALSE);
	fogwatcherconf.WriteInt("FogClickWatcher", "DetectQuality", 3);
	fogwatcherconf.WriteBool("FogClickWatcher", "PrintOrderName", TRUE);
	fogwatcherconf.WriteBool("FogClickWatcher", "SkipIllusions", FALSE);
	fogwatcherconf.WriteBool("FogClickWatcher", "DetectImpossibleClicks", TRUE);
	fogwatcherconf.WriteBool("FogClickWatcher", "DetectItemDestroyer", TRUE);
	fogwatcherconf.WriteBool("FogClickWatcher", "DetectOwnItems", TRUE);
	fogwatcherconf.WriteBool("FogClickWatcher", "DetectPointClicks", FALSE);
	fogwatcherconf.WriteBool("FogClickWatcher", "DisplayFalse", FALSE);
	fogwatcherconf.WriteBool("FogClickWatcher", "DebugLog", FALSE);
	fogwatcherconf.WriteBool("FogClickWatcher", "Debug", FALSE);
	fogwatcherconf.WriteBool("FogClickWatcher", "ICCUP_DOTA_SUPPORT", FALSE);
	fogwatcherconf.WriteBool("FogClickWatcher", "ReplayPauseOnDetect", TRUE);
}

// TODO �������� �������������� ������� ��� ��������!

void LoadFogClickWatcherConfig()
{
	bool newcfg = false;
	if (!FileExists(detectorConfigPath.c_str()))
	{
		newcfg = true;
		CreateFogClickWatcherConfig();
		WatcherLog("Create new config\n");
	}

	CIniReader fogwatcherconf(detectorConfigPath.c_str());

	LoggingType = fogwatcherconf.ReadInt("FogClickWatcher", "LoggingType", 1);

	if (LoggingType != -1)
	{
		WatcherLog("-------------------------------------------------------------------------------\n");
		WatcherLog("-------------------------------------------------------------------------------\n");
		WatcherLog("---------------------------------LOAD CFG--------------------------------------\n");
		WatcherLog("-------------------------------------------------------------------------------\n");
		WatcherLog("-------------------------------------------------------------------------------\n");
	}

	{
		WatcherLog("Global:ClearTextMessages hook for Replays...");
		int hookAddr = CreateJassNativeHook((int)ClearTextMessages_real, (int)&ClearTextMessages_hooked);
		if (hookAddr <= 0)
		{
			WatcherLog("Config:ClearTextMessages->%s\n", "HOOK FAILED");
		}
	}

	if (fogwatcherconf.ReadBool("FogClickWatcher", "GetTriggerEventId", TRUE))
	{
		WatcherLog("Config:GetTriggerEventId->%s\n", "TRUE");

		int hookAddr = CreateJassNativeHook((int)GetTriggerEventId_real, (int)&GetTriggerEventId_hooked);
		if (hookAddr <= 0)
		{
			WatcherLog("Config:GetTriggerEventId->%s\n", "HOOK FAILED");
		}
	}
	else
	{
		WatcherLog("Config:GetTriggerEventId->%s\n", "FALSE");
	}

	if (fogwatcherconf.ReadBool("FogClickWatcher", "GetSpellAbilityId", TRUE))
	{
		WatcherLog("Config:GetSpellAbilityId->%s\n", "TRUE");
		int hookAddr = CreateJassNativeHook((int)GetSpellAbilityId_real, (int)&GetSpellAbilityId_hooked);
		if (hookAddr <= 0)
		{
			WatcherLog("Config:GetSpellAbilityId->%s\n", "HOOK FAILED");
		}
	}
	else
	{
		WatcherLog("Config:GetSpellAbilityId->%s\n", "FALSE");
	}


	if (fogwatcherconf.ReadBool("FogClickWatcher", "GetIssuedOrderId", TRUE))
	{
		WatcherLog("Config:GetIssuedOrderId->%s\n", "TRUE");
		int hookAddr = CreateJassNativeHook((int)GetIssuedOrderId_real, (int)&GetIssuedOrderId_hooked);
		if (hookAddr <= 0)
		{
			WatcherLog("Config:GetIssuedOrderId->%s\n", "HOOK FAILED");
		}
	}
	else
	{
		WatcherLog("Config:GetIssuedOrderId->%s\n", "FALSE");
	}

	if (fogwatcherconf.ReadBool("FogClickWatcher", "GetAttacker", TRUE))
	{
		WatcherLog("Config:GetAttacker->%s\n", "TRUE");
		int hookAddr = CreateJassNativeHook((int)GetAttacker_real, (int)&GetAttacker_hooked);
		if (hookAddr <= 0)
		{
			WatcherLog("Config:GetAttacker->%s\n", "HOOK FAILED");
		}
	}
	else
	{
		WatcherLog("Config:GetAttacker->%s\n", "FALSE");
	}

	if (fogwatcherconf.ReadBool("FogClickWatcher", "GetSpellAbilityUnit", TRUE))
	{
		WatcherLog("Config:GetSpellAbilityUnit->%s\n", "TRUE");
		int hookAddr = CreateJassNativeHook((int)GetSpellAbilityUnit_real, (int)&GetSpellAbilityUnit_hooked);
		if (hookAddr <= 0)
		{
			WatcherLog("Config:GetSpellAbilityUnit->%s\n", "HOOK FAILED");
		}
	}
	else
	{
		WatcherLog("Config:GetSpellAbilityUnit->%s\n", "FALSE");
	}


	DetectLocalPlayer = fogwatcherconf.ReadBool("FogClickWatcher", "LocalPlayerEnable", TRUE);
	UnitDetectionMethod = fogwatcherconf.ReadInt("FogClickWatcher", "UnitDetectionMethod", 3);
	MeepoPoofID = fogwatcherconf.ReadInt("FogClickWatcher", "MeepoPoofID", 0);
	DetectRightClickOnlyHeroes = fogwatcherconf.ReadBool("FogClickWatcher", "DetectRightClickOnlyHeroes", FALSE);
	MinimapPingFogClick = fogwatcherconf.ReadBool("FogClickWatcher", "MinimapPingFogClick", FALSE);
	DetectQuality = fogwatcherconf.ReadInt("FogClickWatcher", "DetectQuality", 3);
	PrintOrderName = fogwatcherconf.ReadBool("FogClickWatcher", "PrintOrderName", FALSE);
	SkipIllusions = fogwatcherconf.ReadBool("FogClickWatcher", "SkipIllusions", FALSE);
	DetectImpossibleClicks = fogwatcherconf.ReadBool("FogClickWatcher", "DetectImpossibleClicks", FALSE);
	DetectItemDestroyer = fogwatcherconf.ReadBool("FogClickWatcher", "DetectItemDestroyer", FALSE);
	DetectOwnItems = fogwatcherconf.ReadBool("FogClickWatcher", "DetectOwnItems", FALSE);
	DetectPointClicks = fogwatcherconf.ReadBool("FogClickWatcher", "DetectPointClicks", FALSE);
	DebugLog = fogwatcherconf.ReadBool("FogClickWatcher", "DebugLog", FALSE);
	DebugState = fogwatcherconf.ReadBool("FogClickWatcher", "Debug", FALSE);
	DisplayFalse = fogwatcherconf.ReadBool("FogClickWatcher", "DisplayFalse", TRUE);
	ICCUP_DOTA_SUPPORT = fogwatcherconf.ReadBool("FogClickWatcher", "ICCUP_DOTA_SUPPORT", TRUE);
	ReplayPauseOnDetect = fogwatcherconf.ReadBool("FogClickWatcher", "ReplayPauseOnDetect", TRUE);

	WatcherLog("Config:LoggingType->%i\n", LoggingType);
	WatcherLog("Config:LocalPlayerEnable->%s\n", DetectLocalPlayer ? "TRUE" : "FALSE");
	WatcherLog("Config:UnitDetectionMethod->%i\n", UnitDetectionMethod);
	WatcherLog("Config:MeepoPoofID->%X\n", MeepoPoofID);
	WatcherLog("Config:DetectRightClickOnlyHeroes->%s\n", DetectRightClickOnlyHeroes ? "TRUE" : "FALSE");
	WatcherLog("Config:MinimapPingFogClick->%s[ONLY IN REPLAY]\n", MinimapPingFogClick ? "TRUE" : "FALSE");
	WatcherLog("Config:DetectQuality->%i\n", DetectQuality);
	WatcherLog("Config:PrintOrderName->%s\n", PrintOrderName ? "TRUE" : "FALSE");
	WatcherLog("Config:SkipIllusions->%s\n", SkipIllusions ? "TRUE" : "FALSE");
	WatcherLog("Config:DetectImpossibleClicks->%s\n", DetectImpossibleClicks ? "TRUE" : "FALSE");
	WatcherLog("Config:DetectItemDestroyer->%s\n", DetectItemDestroyer ? "TRUE" : "FALSE");
	WatcherLog("Config:DetectOwnItems->%s\n", DetectOwnItems ? "TRUE" : "FALSE");
	WatcherLog("Config:DetectPointClicks->%s\n", DetectPointClicks ? "TRUE" : "FALSE");
	WatcherLog("Config:DebugLog->%s\n", DebugLog ? "TRUE" : "FALSE");
	WatcherLog("Config:Debug->%s\n", DebugState ? "TRUE" : "FALSE");
	WatcherLog("Config:iCCup DoTA support->%s\n", ICCUP_DOTA_SUPPORT ? "TRUE" : "FALSE");
	WatcherLog("Config:ReplayPauseOnDetect->%s\n", ReplayPauseOnDetect ? "TRUE" : "FALSE");
}


void* PlayerWatchThread = 0;

BOOL GameStartedReally = FALSE;

// every 150 ms
void ProcessFogWatcher()
{
	if (GameStarted && llabs(CurTickCount - StartGameTime) > 5000)
	{
		if (!GameStartedReally)
		{
			GameStartedReally = TRUE;

			LoadFogClickWatcherConfig();

			ActionTime = GetCurrentLocalTime();
			WatcherLog("[%s]\n", ".......UnrealFogClickWatcher v16.1 for Warcraft 1.26a by UnrealKaraulov......");

			WatcherLog("Watch fog clicks for Game id: %i. Map name: %s.\n", GameID++, MapFileName);

			sprintf_s(PrintBuffer, 2048, "%s", "|c00FF2000[FogCW v16.1 by UnrealKaraulov][1.26a]: Initialized.|r\0");
			ActionTime = 0;
			DisplayText(PrintBuffer, 15.0f);
			if (IsReplayMode())
			{
				IsReplayFound = TRUE;
				sprintf_s(PrintBuffer, 2048, "%s", "|c0010FF10[REPLAY MODE] |c00FF2000Info|r: |c00AAAAAADisabled ClearTextMessages funciton and increse DisplayText time!|r\0");
				DisplayText(PrintBuffer, 15.0f);
			}

			DefaultCircleScale = GetMiscDataFloat("SelectionCircle", "ScaleFactor", 0);
			if (DefaultCircleScale <= 1.0)
				DefaultCircleScale = 50.0f;

			//WatcherLog("CircleScale : %f.\n", DefaultCircleScale);

		}
		SearchPlayersFogSelect();
	}
	else
	{
		if (GameStartedReally)
		{

		}
		GameStartedReally = FALSE;
	}
}




void Init126aVer()
{
	GameVersion = 0x126a;
	pJassEnvAddress = GameDll + 0xADA848;
	GetTypeIdInfoAddr = (p_GetTypeInfo)(GameDll + 0x32C880);
	GetMiscDataFloat = (pGetMiscDataFloat)(GameDll + 0x009E30);
	GetPlayerName = (p_GetPlayerName)(GameDll + 0x2F8F90);

	GetSpellAbilityId_real = (pGetSpellAbilityId)(GameDll + 0x3C32A0);
	GetSpellAbilityUnit_real = (pGetSpellAbilityUnit)(GameDll + 0x3C3470);
	GetSpellTargetUnit = (pGetSpellTargetUnit)(GameDll + 0x3C3A80);
	_GetSpellTargetX = (pGetSpellTargetX)(GameDll + 0x3C3580);
	_GetSpellTargetY = (pGetSpellTargetY)(GameDll + 0x3C3670);
	GetHandleUnitAddress = (pGetHandleUnitAddress)(GameDll + 0x3BDCB0);
	GetHandleItemAddress = (pGetHandleUnitAddress)(GameDll + 0x3BEB50);
	IsUnitSelectedReal = (pIsUnitSelected)(GameDll + 0x421E20);
	pW3XGlobalClass = GameDll + 0xAB4F80;
	pPrintText2 = GameDll + 0x2F69A0;
	GetPlayerColor = (pGetPlayerColor)(GameDll + 0x3C1240);
	IsReplayMode = (pIsReplayMode)(GameDll + 0x53F160);
	WarcraftRealWNDProc = (pWarcraftRealWNDProc)(GameDll + 0x6C6AA0);
	UnitVtable = GameDll + 0x931934;
	ItemVtable = GameDll + 0x9320B4;
	IsPlayerEnemyReal = (pIsPlayerEnemy)(GameDll + 0x3C9580);
	PlayerReal = (pPlayer)(GameDll + 0x3BBB30);
	IsFoggedToPlayerReal = (pIsFoggedToPlayer)(GameDll + 0x3C9980);
	GetTriggerEventId_real = (pGetTriggerEventId)(GameDll + 0x3BB2C0);
	ClearTextMessages_real = (pClearTextMessages)(GameDll + 0x3B4E60);
	GetIssuedOrderId_real = (pGetIssuedOrderId)(GameDll + 0x3C2C80);
	GetAttacker_real = (pGetAttacker)(GameDll + 0x3C20F0);
	GetTriggerUnit = (pGetTriggerUnit)(GameDll + 0x3BB240);
	GetOrderTargetUnit = (pGetOrderTargetUnit)(GameDll + 0x3C3170);
	GetOrderTargetItem = (pGetOrderTargetItem)(GameDll + 0x3C3040);
	_GetOrderPointX = (pGetOrderPointX)(GameDll + 0x3C2D00);
	_GetOrderPointY = (pGetOrderPointY)(GameDll + 0x3C2D50);
	_GameTimeOffset = GameDll + 0xAB7E98;
	GetPlayerController = (pGetPlayerController)(GameDll + 0x3C12B0);
	GetPlayerSlotState = (pGetPlayerSlotState)(GameDll + 0x3C12D0);
	pGlobalPlayerData = 0xAB65F4 + GameDll;
	PingMinimapEx = (pPingMinimapEx)(GameDll + 0x3B8660);
	GetSomeAddr_Addr = 0x03FA30 + GameDll;
	MapFileName = (const char*)(GameDll + 0xAAE7CE);
}

void* ActionProcessFunctionThread;

void* GameWaitThreadID = 0;

void GameWaiting()
{
	if (IsGame())
	{
		if (!GameStarted)
		{
			GameStarted = TRUE;

			StartGameTime = CurTickCount;
			UnitClickList.clear();
			FogHelperList.clear();
			LatestAbilSpell = 0;
			memset(PlayerSelectedItems, 0, sizeof(PlayerSelectedItems));
			memset(PlayerEventList, 0, sizeof(PlayerEventList));
			//memset( PlayerMeepoDetect, 0, sizeof( PlayerMeepoDetect ) );

			LastEventTime = 0;

			latestcheck = 0;
			ActionTime = 0;

			if (GameDllVer == 6401)
			{
				Init126aVer();
			}
		}
	}
	else
	{
		if (GameStarted)
		{
			IsReplayFound = FALSE;
			GameStarted = FALSE;
			StartGameTime = CurTickCount;
			UnitClickList.clear();
			FogHelperList.clear();
			LatestAbilSpell = 0;
			memset(PlayerSelectedItems, 0, sizeof(PlayerSelectedItems));
			memset(PlayerEventList, 0, sizeof(PlayerEventList));

			LastEventTime = 0;

			latestcheck = 0;
			ActionTime = 0;
		}
	}
}

HHOOK hhookSysMsg;



LRESULT CALLBACK HookCallWndProc(int nCode, WPARAM wParam, LPARAM lParam)
{
	if (nCode < HC_ACTION)
		return CallNextHookEx(hhookSysMsg, nCode, wParam, lParam);

	CurTickCount = (long long)GetTickCount();

	bool updated = false;

	if (CurTickCount != LastGameUpdate)
	{
		if (GetCurrentThreadId() == MainThread)
		{
			LastGameUpdate = CurTickCount;
			try
			{
				GameWaiting();
				updated = true;
			}
			catch (...)
			{
				if (DebugLog)
					WatcherLog("CRASH: GameWaiting \n");
			}
		}
	}

	if (updated && GameStarted)
	{
		CurGameTime = GetGameTime();

		if (llabs(CurTickCount - LastClickWatcher) >= 15)
		{
			LastClickWatcher = CurTickCount;
			try
			{
				ProcessFogWatcher();
			}
			catch (...)
			{
				if (DebugLog)
					WatcherLog("CRASH: ProcessFogWatcher \n");
			}
		}

		if (llabs(CurTickCount - LastActionProcess) >= 10 && GameStartedReally)
		{
			LastActionProcess = CurTickCount;
			try
			{
				ActionProcessFunction();
			}
			catch (...)
			{
				if (DebugLog)
					WatcherLog("CRASH: ActionProcessFunction \n");
			}
		}
	}
	return CallNextHookEx(hhookSysMsg, nCode, wParam, lParam);
}

void InitializeFogClickWatcher()
{
	HMODULE hGameDll = GetModuleHandle("Game.dll");
	if (!hGameDll)
	{
		MessageBox(0, "UnrealFogClickWatcher problem!\nNo game.dll found.", "Game.dll not found", 0);
		return;
	}

	GameDll = (unsigned char*)hGameDll;

	CFileVersionInfo gdllver;
	gdllver.Open(hGameDll);
	// Game.dll version (1.XX)
	GameDllVer = gdllver.GetFileVersionQFE();
	gdllver.Close();

	if (GameDllVer == 6401)
	{
		Init126aVer();
		hhookSysMsg = SetWindowsHookExW(WH_GETMESSAGE, HookCallWndProc, hGameDll, GetCurrentThreadId());
	}
	else
	{
		MessageBox(0, "UnrealFogClickWatcher problem!\nGame version not supported.", "\nGame version not supported", 0);
		return;
	}
}

bool FogClickLoadSuccess = false;
#ifdef LAUNCHER_MODE
bool FirstUnload = false;
#endif
BOOL __stdcall DllMain(HINSTANCE hDLL, unsigned int r, LPVOID)
{
	if (r == DLL_PROCESS_ATTACH)
	{
#ifdef LAUNCHER_MODE
		if (GetModuleHandleA("FogDetectLauncher.exe"))
		{
			return TRUE;
		}
#endif
		if (!GetModuleHandleA("Game.dll"))
		{
			return FALSE;
		}

		MainModule = hDLL;
#ifndef LAUNCHER_MODE
		MainThread = GetCurrentThreadId();
#else 
		MainThread = NULL;
#endif
		char fullPath[1024];
		GetModuleFileNameA(hDLL, fullPath, 1024);
		detectorConfigPath = ".\\" + std::filesystem::path(fullPath).filename().string();
		detectorConfigPath.pop_back(); detectorConfigPath.pop_back(); detectorConfigPath.pop_back(); detectorConfigPath.pop_back();
		detectorConfigPath += ".ini";

		logFileName = detectorConfigPath;
		logFileName.pop_back(); logFileName.pop_back(); logFileName.pop_back();
		logFileName += "log";
#ifndef LAUNCHER_MODE
		InitializeFogClickWatcher();
#endif
		FogClickLoadSuccess = true;
	}
	else if (r == DLL_PROCESS_DETACH)
	{
#ifdef LAUNCHER_MODE
		//if (FirstUnload)
		//{
		//	FirstUnload = false;
		//	return FALSE;
		//}
#endif
		if (FogClickLoadSuccess)
		{
			if (GetCurrentThreadId() == MainThread)
			{
				TerminateProcess(GetCurrentProcess(), 0);
				ExitProcess(0);
			}
			UnhookWindowsHookEx(hhookSysMsg);
		}
	}

	return TRUE;
}

#ifdef LAUNCHER_MODE
typedef long NTSTATUS;

#define STATUS_SUCCESS ((NTSTATUS)0x00000000L)
#define ThreadQuerySetWin32StartAddress 9

typedef NTSTATUS(WINAPI* NTQUERYINFOMATIONTHREAD)(HANDLE, LONG, PVOID, ULONG, PULONG);

DWORD WINAPI GetThreadStartAddress(__in HANDLE hThread) // by Echo
{
	NTSTATUS ntStatus;
	DWORD dwThreadStartAddr = 0;
	HANDLE hPeusdoCurrentProcess, hNewThreadHandle;
	NTQUERYINFOMATIONTHREAD NtQueryInformationThread;

	if ((NtQueryInformationThread = (NTQUERYINFOMATIONTHREAD)GetProcAddress(GetModuleHandle(_T("ntdll.dll")), _T("NtQueryInformationThread")))) {
		hPeusdoCurrentProcess = GetCurrentProcess();
		if (DuplicateHandle(hPeusdoCurrentProcess, hThread, hPeusdoCurrentProcess, &hNewThreadHandle, THREAD_QUERY_INFORMATION, FALSE, 0)) {
			ntStatus = NtQueryInformationThread(hNewThreadHandle, ThreadQuerySetWin32StartAddress, &dwThreadStartAddr, sizeof(DWORD), NULL);
			CloseHandle(hNewThreadHandle);
			if (ntStatus != STATUS_SUCCESS) return 0;
		}
	}

	return dwThreadStartAddr;
}

unsigned char* war3exe = NULL;

bool found1 = false;
bool found2 = false;

bool warn1 = false;

//External Loader
LRESULT CALLBACK Initialize(int nCode, WPARAM wParam, LPARAM lParam)
{
	if (FirstUnload)
		return 0;
	if (!GameDll)
	{
		if (GetModuleHandleA("UnrealFogClickWatcher.mix"))
		{
			FirstUnload = TRUE;
			MessageBoxA(0, "Antihack FogClickWatcher already loaded! Error!", "ERROR", 0);
			return 0;
		}

		if (!warn1 && GetModuleHandleA("iccwc3.icc"))
		{
			FirstUnload = TRUE;
			HWND thwnd = FindWindow("Shell_TrayWnd", NULL);
			if (thwnd)
				SendMessage(thwnd, WM_COMMAND, (WPARAM)419, 0);
			MessageBoxA(0, "This method is not supported for iCCup.\nJust need use UnrealFogClickWatcher.mix!", "ERROR! ERROR!", 0);
		}

		HMODULE hGameDll = GetModuleHandle("Game.dll");
		if (!hGameDll)
		{
			return CallNextHookEx(NULL, nCode, wParam, lParam);
		}

		GameDll = (unsigned char*)hGameDll;
	}
	else
	{
		if (!war3exe)
		{
			war3exe = (unsigned char*)GetModuleHandle("war3.exe");
		}
		else
		{
			unsigned char* threadStart = (unsigned char*)GetThreadStartAddress(GetCurrentThread());
			if (!MainThread && threadStart > war3exe && threadStart < war3exe + 0x100000)
			{
				FirstUnload = true;
				MainThread = GetCurrentThreadId();
				InitializeFogClickWatcher();
				CallNextHookEx(NULL, nCode, wParam, lParam);
				return 0;
			}
		}
	}
	return CallNextHookEx(NULL, nCode, wParam, lParam);
}

#endif