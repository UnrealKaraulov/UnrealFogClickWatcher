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

#include <chrono>

#include "GameStructs.h"

#include "JassStructs.h"

#include "StringStructs.h"

#include "fp_call.h"

std::string detectorConfigPath = "./UnrealFogClickWatcher.ini";

std::string logFileName = "./UnrealFogClickWatcher.log";

#define IsKeyPressed(CODE) (GetAsyncKeyState(CODE) & 0x8000) > 0

typedef LONG(WINAPI* pNtQIT)(HANDLE, LONG, PVOID, ULONG, PULONG);

#define STATUS_SUCCESS    ((NTSTATUS)0x00000000L)

#define ThreadQuerySetWin32StartAddress 9

BOOL IsReplayFound = FALSE;

char PrintBuffer[2048];

long long CurTickCount = 0;

long long LastGameUpdate = 0;

long long LastActionProcess = 0;

long long LastClickWatcher = 0;

long long StartGameTime = 0;

int LoggingType = 0;

BOOL BB_CODE_FORMAT_OUTPUT = TRUE;

void replaceAll(std::string& str, const std::string& from, const std::string& to)
{
	size_t start_pos = 0;
	while ((start_pos = str.find(from, start_pos)) != std::string::npos)
	{
		str.replace(start_pos, from.length(), to);
		start_pos += to.length(); // Handles case where 'to' is a substring of 'from'
	}
}


void WatcherLogAddLine(std::string line)
{
	if (BB_CODE_FORMAT_OUTPUT)
	{
		std::size_t ind = line.find("|r");

		while (ind != std::string::npos)
		{
			line.erase(ind, 2);
			//line.insert(ind, "[/b]");
			ind = line.find("|r");
		}

		ind = line.find("|c");
		while (ind != std::string::npos)
		{
			line.erase(ind, 2);

			if (line.size() - ind > 8)
			{
				line.erase(ind, 8);
			}

			//line.insert(ind, "[b]");
			ind = line.find("|c");
		}

		replaceAll(line, "[", "[b^^^[");
		replaceAll(line, "]", "][/b]");
		replaceAll(line, "^^^", "]");
	}

	std::ofstream outfile(logFileName, std::ios_base::app);
	if (!outfile.bad() && outfile.is_open())
		outfile << line;
	outfile.close();
}

void WatcherLog(const char* format, ...)
{
	if (LoggingType == -1)
		return;
	char buffer[1024];
	va_list args;
	va_start(args, format);
	vsprintf_s(buffer, format, args);
	WatcherLogAddLine(buffer);
	va_end(args);
}

void WatcherLogForce(const char* format, ...)
{
	char buffer[1024];
	va_list args;
	va_start(args, format);
	vsprintf_s(buffer, format, args);
	WatcherLogAddLine(buffer);
	va_end(args);
}


union DWFP
{
	unsigned int dw;
	float fl;
};

const int MAX_PLAYERS = 12;
const int TOTAL_MAX_PLAYERS_ARRAY = 16;

int GameID = 0;

int GameDllVer = 0;

unsigned char* GameDll = 0;

HMODULE MainModule = 0;

const char* MapFileName = "";

BOOL GameStarted = FALSE;

BOOL GameStartedReally = FALSE;

BOOL FogClickEnabled = TRUE;

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

int TechiesDetonateId = 0xD024C;

BOOL DebugLog = FALSE;

BOOL DebugModeEnabled = FALSE;

BOOL DisplayFalse = TRUE;

BOOL ICCUP_DOTA_SUPPORT = FALSE;

BOOL ReplayPauseOnDetect = TRUE;

BOOL PrintDetectedUnitOneTime = TRUE;

BOOL FullEventHookProcess = TRUE;

int DetectQuality = 3;

BOOL ReplayMoreSens = TRUE;

BOOL ExceptionFilterHooked = FALSE;

float DefaultCircleScale = 72.0f;

BOOL DetectLocalPlayer = FALSE;

long long CurGameTime = 0;


// HIGH 20 20 150
// MEDIUM 50 50 200
// LOW 70 70 250
unsigned int FOG_CLICK_UPDATE_TIME = 20;
unsigned int ACTION_UPDATE_TIME = 10;
unsigned int FOG_HISTORY_UPDATE_TIME = 150;

unsigned int FOG_HISTORY_CLEANUP_TIME = 20000;
unsigned int FOG_CLICK_CLEANUP_TIME = 20000;

// LOW 500
// MEDIUM 1000
// HIGHT 1500
unsigned int MAX_FOG_HISTORY_ARRAY = 1000;


// LOW 250
// MEDIUM 500
// HIGHT 1000
unsigned int MAX_CLICK_HISTORY_ARRAY = 500;


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
	long long LatestTime[MAX_PLAYERS];
	BOOL FogState[MAX_PLAYERS][3];
};

std::vector<UnitSelectedStruct> UnitClickList;

std::vector<FogHelper> FogHelperList;

bool IsFoggedInHistory(int playerid, int unitaddr)
{
	if (DetectQuality <= 2)
		return true;

	for (const auto& unit_fog : FogHelperList)
	{
		if (unit_fog.UnitAddr == unitaddr)
		{
			if (DetectQuality == 3)
			{
				return unit_fog.FogState[playerid][0] && unit_fog.FogState[playerid][1];
			}
			else
			{
				return unit_fog.FogState[playerid][0] && unit_fog.FogState[playerid][1] && unit_fog.FogState[playerid][2];
			}
		}
	}
	return false;
}

DWORD MainThread = NULL;

unsigned int GetCurrentLocalTime()
{
	time_t rawtime;
	struct tm timeinfo;

	time(&rawtime);
	localtime_s(&timeinfo, &rawtime);
	return (timeinfo.tm_hour * 3600 + timeinfo.tm_min * 60 + timeinfo.tm_sec) * 1000;
}

static const char* PlayerColorStrings[12] =
{
	"|c00ff0303",
	"|c000042ff",
	"|c001ce6b9",
	"|c00540081",
	"|c00fffc01",
	"|c00ff8000",
	"|c0020c000",
	"|c00e55bb0",
	"|c00959697",
	"|c007ebff1",
	"|c00106246",
	"|c004e2a04"
};

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
	return 16;
}

unsigned char* pGlobalWar3Data;

void* GetGlobalWar3Data()
{
	if (*(int*)(pGlobalWar3Data) > 0)
	{
		return (void*)*(int*)(pGlobalWar3Data);
	}
	return 0;
}

war3::CPlayerWar3* PlayerObject(int playerId) {
	if (playerId >= 0 && playerId < TOTAL_MAX_PLAYERS_ARRAY) {
		war3::CGameWar3* gameObj = (war3::CGameWar3*)(GetGlobalWar3Data());
		if (gameObj)
		{
			return gameObj->players[playerId];
		}
	}
	return NULL;
}

int GetPlayerByNumber(int number)
{
	return (int)PlayerObject(number);
}


int GetLocalPlayerNumber()
{
	if (!IsReplayFound && !DetectLocalPlayer)
		return 16;

	war3::CGameWar3* globalGame = (war3::CGameWar3*)(GetGlobalWar3Data());
	if (globalGame) {
		return (int)globalGame->localPlayerSlot;
	}
	else
		return 16;
}

int GetPlayerColor(int number)
{
	war3::CPlayerWar3* playerdata = PlayerObject(number);
	if (playerdata)
	{
		return playerdata->color;
	}
	else
	{
		return -1;
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

//
//const char* TexturePath = "ReplaceableTextures\\Selection\\SpellAreaOfEffect_basic.blp";
//
//
//struct ImageUpdateStr
//{
//	int unitaddr;
//	long long lastupdate;
//
//}

unsigned char* GameTimeOffset = 0;

long long oldGameTime = 0;

long long realTickCountTime = 0;

long long increaseTimerTicks = 0;

long long GetGameTime()
{
	if (!GameDll)
	{
		return CurTickCount;
	}
	long long tmpGameTime = (long long)*(unsigned int*)GameTimeOffset;

	if (tmpGameTime <= 0 || tmpGameTime == 0xFFFFFFFF)
		return CurTickCount;

	/*if (tmpGameTime == oldGameTime)
	{
		increaseTimerTicks += llabs(CurTickCount - realTickCountTime);
	}
	else
	{
		increaseTimerTicks = 0;
	}

	oldGameTime = tmpGameTime;
	realTickCountTime = CurTickCount;*/
	return tmpGameTime + increaseTimerTicks;
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

bool FileExists(const std::string& fname)
{
	return std::filesystem::exists(fname) && !std::filesystem::is_directory(fname);
}
int SafeItemCount = 1;
int SafeItemArraySize = 1;
int* SafeItemArray = new int[1] {0};
void FillItemCountAndItemArray()
{
	int GlobalClassOffset = *(int*)(pW3XGlobalClass);
	if (GlobalClassOffset > 0)
	{
		int ItemsOffset1 = *(int*)(GlobalClassOffset + 0x3BC) + 0x10;
		if (ItemsOffset1 > 0)
		{
			int* ItemsCount = (int*)(ItemsOffset1 + 0x604);
			if (ItemsCount
				&& *ItemsCount > 0)
			{
				int* Itemarray = (int*)*(int*)(ItemsOffset1 + 0x608);


				if (SafeItemArraySize < *ItemsCount)
				{
					delete[]SafeItemArray;
					SafeItemArraySize = *ItemsCount;
					SafeItemArray = new int[SafeItemArraySize + 1];
				}
				SafeItemCount = *ItemsCount;
				memcpy(SafeItemArray, Itemarray, 4 * SafeItemCount);
				return;
			}
		}
	}

	memset(SafeItemArray, 0, 4 * SafeItemCount);
}

int SafeUnitCount = 1;
int SafeUnitArraySize = 1;
int* SafeUnitArray = new int[1] {0};
void FillUnitCountAndUnitArray()
{
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

				if (SafeUnitArraySize < *UnitsCount)
				{
					delete[]SafeUnitArray;
					SafeUnitArraySize = *UnitsCount;
					SafeUnitArray = new int[SafeUnitArraySize + 1];
				}

				SafeUnitCount = *UnitsCount;
				memcpy(SafeUnitArray, unitarray, 4 * SafeUnitCount);
				return;
			}
		}
	}

	memset(SafeUnitArray, 0, 4 * SafeUnitCount);
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

typedef int(__fastcall* pGetSomeAddr)(UINT a1, UINT a2);
pGetSomeAddr GetSomeAddr;



LPVOID LpConvertHandle(int handle)
{
	if (!*(int*)pGlobalWar3Data)
		return NULL;

	if (!*(int*)(*(int*)pGlobalWar3Data + 0x1c))
		return NULL;

	return handle > 0x100000 ? *(LPVOID*)(*(int*)(*(int*)(*(int*)pGlobalWar3Data + 0x1c) + 0x19c) + handle * 0xc - 0x2fffff * 4) : NULL;
}

int ConvertHandle(int handle)
{
	return (int)LpConvertHandle(handle);
}


BOOL __stdcall IsNotBadUnit(int _unitaddr)
{
	if (_unitaddr > 0)
	{
		bool UnitFoundInArray = false;

		for (int i = 0; i < SafeUnitCount; i++)
		{
			if (SafeUnitArray[i] == _unitaddr)
				UnitFoundInArray = true;
		}

		if (UnitFoundInArray)
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


			return !IsUnitDead(_unitaddr);
		}
	}

	return FALSE;
}

BOOL IsNotBadItem(int itemaddr)
{
	if (itemaddr > 0)
	{
		bool ItemFoundInArray = false;

		for (int i = 0; i < SafeItemCount; i++)
		{
			if (SafeItemArray[i] == itemaddr)
				ItemFoundInArray = true;
		}

		if (ItemFoundInArray)
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
			return hitpoint > 0.0f;
		}
	}

	return FALSE;
}


typedef int(__cdecl* pPlayer)(int number);
pPlayer PlayerReal;


struct PlayerCacheStruct
{
	int whichPlayer;
	long long checktime;
};

PlayerCacheStruct player_cache[TOTAL_MAX_PLAYERS_ARRAY];

int Player(int number)
{
	if (number >= 0 && number < TOTAL_MAX_PLAYERS_ARRAY)
	{
		if (player_cache[number].checktime == 0 || llabs(CurTickCount - player_cache[number].checktime) > 30000)
		{
			player_cache[number].checktime = CurTickCount;
			player_cache[number].whichPlayer = PlayerReal(number);
			return player_cache[number].whichPlayer;
		}

		return player_cache[number].whichPlayer;
	}
	return 0;
}

bool __cdecl IsUnitVisibleToPlayer(unsigned char* unitaddr, unsigned char* player)
{
	int retval = 0;
	if (unitaddr && player && *(int*)unitaddr)
	{
		typedef int(__fastcall* FunctionType)(void*, int, int, int, int);
		FunctionType function = (FunctionType)(*(int*)(*(int*)unitaddr + 0x000000FC));
		retval = function(unitaddr, 0, player[0x30], 0x00, 0x04);
	}
	return retval > 0;
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

	return *(unsigned char**)InGame && **(unsigned char***)InGame == _GameUI;
}


long long ActionTime = 0;

void DisplayText(const std::string& szText, float fDuration)
{
	if (IsReplayFound)
		fDuration = 60.0f;

	unsigned int dwDuration = *((unsigned int*)&fDuration);


	unsigned char* GAME_PrintToScreen = GameDll + 0x2F8E40;

	int ms = (int)(ActionTime % 1000);
	int seconds = (int)(ActionTime / 1000) % 60;
	int minutes = (int)((ActionTime / (1000 * 60)) % 60);
	int hours = (int)((ActionTime / (1000 * 60 * 60)) % 24);

	char timeBuff[64];
	if (DebugModeEnabled)
		sprintf_s(timeBuff, "[%.2d:%.2d:%.2d.%.4d] : ", hours, minutes, seconds, ms);
	else
		sprintf_s(timeBuff, "[%.2d:%.2d:%.2d] : ", hours, minutes, seconds);

	std::string outLineStr = (timeBuff + szText);

	if (LoggingType != -1)
	{
		WatcherLog((outLineStr + "\n").c_str());
	}

	if (!GameDll || !*(unsigned char**)pW3XGlobalClass || LoggingType == 0)
		return;

	// vtable?
	int GlobalClassOffset = *(int*)(pW3XGlobalClass);
	if (GlobalClassOffset > 0)
		GlobalClassOffset = *(int*)(GlobalClassOffset);

	typedef void(__fastcall* GAME_PrintToScreen_t)(int ecx, uint32_t edx, uint32_t arg1, uint32_t arg2, char* outLinePointer, uint32_t dwDuration, uint32_t arg5);
	int ecx = *(int*)pW3XGlobalClass;
	GAME_PrintToScreen_t _GAME_PrintToScreen = (GAME_PrintToScreen_t)GAME_PrintToScreen;
	_GAME_PrintToScreen(ecx, 0x0, 0x0, 0x0, outLineStr.data(), dwDuration, 0xFFFFFFFF);
}

long long latestcheck = 0;

int GetSelectedItem(int slot)
{
	int plr = GetPlayerByNumber(slot);
	if (plr != 0)
	{
		int PlayerData1 = *(int*)(plr + 0x34);
		if (PlayerData1)
		{
			int item1 = *(int*)(PlayerData1 + 0x1A8);
			if (item1 > 0)
			{
				if (IsNotBadItem(item1))
				{
					return item1;
				}
			}
		}
	}
	return 0;
}

int UnitDetectionMethod = 1;

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
			else if (UnitDetectionMethod == 5)
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
			else if (UnitDetectionMethod == 6)
			{
				war3::UnitList* subgroup = *(war3::UnitList**)(PlayerData1 + 0x20C);
				if (subgroup)
				{
					war3::UnitListNode* node = subgroup->firstNode;
					if (node)
					{
						int unitaddr = (int)node->unit;
						if (IsNotBadUnit(unitaddr))
						{
							return unitaddr;
						}
					}
				}
			}
			else
			{
				war3::UnitList* subgroup = *(war3::UnitList**)(PlayerData1 + 0x1C0);
				if (subgroup)
				{
					war3::UnitListNode* node = subgroup->firstNode;
					if (node)
					{
						int unitaddr = (int)node->unit;
						if (IsNotBadUnit(unitaddr))
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
			if (v5 < 0)
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
	if (!module)
		module = (unsigned char*)GetModuleHandle("war3map.override.dll");
	if (!module)
		module = (unsigned char*)GetModuleHandle("war3map.dll.override.dll");

	if (module && ICCUP_DOTA_SUPPORT)
	{
		// Понадобится Cheat Engine
		// Быстрый поиск можно - отключить
		// Поставить "минусы" на все типы памяти
		// Ввести 6F3C83E0 (к примеру, Game.dll+0x3C83E0)
		// Найти в war3map
		// Пройтись до начала массива и указать его адрес далее

		// Список выставлять по возврастанию адреса, что бы избегать крашей

		// Второй вариант
		// Понадобится Cheat Engine с отладчиком VEH 
		// Установить точку останова на любую JASS нативу
		// По возврату ищем откуда взялся адрес для вызова, там ищем начало массива
		// Указать адрес ниже

		// Третий вариант
		// IDA PRO или IDA DEMO 
		// открыть war3map.dll
		// Найти Initialize
		// Там найти вызов реальной функции инициализации
		// В ней будет функция инициализации списка JASS нативок
		// Первый из dword_ адресов нужный нам :)

		// Список Jass функций в Dota 6.83s fukkei
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

		// Список JASS функций в iCCup Dota 372 debug
		if (module && *(int*)(module + 0x102B4D8) == (int)(GameDll + 0x3C81F0))
		{
			WatcherLog("[HOOK JASS NATIVE IN iCCup Dota 390]");
			unsigned char* StartAddr = module + 0x102B4D8;
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

		// Список JASS функций в iCCup Dota 373
		if (module && *(int*)(module + 0x102B4C8) == (int)(GameDll + 0x3C81F0))
		{
			WatcherLog("[HOOK JASS NATIVE IN iCCup Dota 390]");
			unsigned char* StartAddr = module + 0x102B4C8;
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
		// Список JASS функций в iCCup Dota 390
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

		// Список JASS функций в iCCup Dota 396
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

		// Список JASS функций в iCCup Dota 400
		if (module && *(int*)(module + 0x1047C4C) == (int)(GameDll + 0x3C8F00))
		{
			WatcherLog("[HOOK JASS NATIVE IN iCCup Dota 400]");
			unsigned char* StartAddr = module + 0x1047C4C;
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

		// Список JASS функций в iCCup Dota 403
		if (module && *(int*)(module + 0x1047DC8) == (int)(GameDll + 0x3B3E50))
		{
			WatcherLog("[HOOK JASS NATIVE IN iCCup Dota 403]");
			unsigned char* StartAddr = module + 0x1047DC8;
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

int RevertJassNativeHook(int newaddress, int oldaddress)
{
	return CreateJassNativeHook(oldaddress, newaddress);
}

//
//int CreateJassNativeHook(const std::string& name, int newaddress)
//{
//	int FirstAddress = *(int*)pJassEnvAddress;
//	if (FirstAddress > 0)
//	{
//		FirstAddress = *(int*)(FirstAddress + 20);
//		if (FirstAddress > 0)
//		{
//			FirstAddress = *(int*)(FirstAddress + 32);
//			if (FirstAddress > 0)
//			{
//				int NextAddress = FirstAddress;
//				while (TRUE)
//				{
//					if (*(char**)(NextAddress + 8) == name)
//					{
//						int oldaddr = *(int*)(NextAddress + 12);
//						*(int*)(NextAddress + 12) = newaddress;
//						return oldaddr;
//					}
//
//					NextAddress = *(int*)NextAddress;
//
//					if (NextAddress == FirstAddress || NextAddress <= 0)
//						break;
//				}
//			}
//		}
//	}
//	return 0;
//}

typedef int(__cdecl* pGetSpellAbilityId)();
pGetSpellAbilityId GetSpellAbilityId_real;

typedef int(__cdecl* pGetTriggerEventId)();
pGetTriggerEventId GetTriggerEventId_real;

typedef int(__cdecl* pGetTriggerPlayer)();
pGetTriggerPlayer GetTriggerPlayer;

typedef int(__cdecl* pGetPlayerId)(int whichPlayer);
pGetPlayerId GetPlayerId;

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

typedef int(__cdecl* pGetSpellTargetUnit)();
pGetSpellTargetUnit GetSpellTargetUnit;

typedef DWFP(__cdecl* pGetSpellTargetX)();
pGetSpellTargetX GetSpellTargetX;

typedef DWFP(__cdecl* pGetSpellTargetY)();
pGetSpellTargetY GetSpellTargetY;

typedef int(__fastcall* pGetHandleUnitAddress) (int HandleID, int unused);
pGetHandleUnitAddress GetHandleUnitAddress;
pGetHandleUnitAddress GetHandleItemAddress;

typedef BOOL(__cdecl* pIsPlayerEnemy)(int whichPlayer, int otherPlayer);
pIsPlayerEnemy IsPlayerEnemy_real;

typedef int(__cdecl* pGetPlayerController)(int whichPlayer);
pGetPlayerController GetPlayerController_real;

struct CACHE_PLAYER_CONTROLLER_STRUCT
{
	int control;
	long long checktime;
};

CACHE_PLAYER_CONTROLLER_STRUCT player_controller_cache[MAX_PLAYERS];

int GetPlayerControllerById(int player_id)
{
	if (player_id < 0 || player_id >= MAX_PLAYERS)
		return -1;

	if (player_controller_cache[player_id].checktime == 0 || llabs(CurTickCount - player_controller_cache[player_id].checktime) > 30000)
	{
		player_controller_cache[player_id].checktime = CurTickCount;
		int whichPlayer = Player(player_id);
		if (whichPlayer > 0)
		{
			player_controller_cache[player_id].control = GetPlayerController_real(whichPlayer);
			return player_controller_cache[player_id].control;
		}
		player_controller_cache[player_id].control = -1;
	}
	else
	{
		return player_controller_cache[player_id].control;
	}

	return -1;
}

typedef int(__cdecl* pGetPlayerSlotState)(int whichPlayer);
pGetPlayerSlotState GetPlayerSlotState_real;


struct CACHE_PLAYER_STATE_STRUCT
{
	int state;
	long long checktime;
};

CACHE_PLAYER_STATE_STRUCT player_state_cache[MAX_PLAYERS];

int GetPlayerSlotStateById(int player_id)
{
	if (player_id < 0 || player_id >= MAX_PLAYERS)
		return -1;

	if (player_state_cache[player_id].checktime == 0 || llabs(CurTickCount - player_state_cache[player_id].checktime) > 30000)
	{
		player_state_cache[player_id].checktime = CurTickCount;
		int whichPlayer = Player(player_id);
		if (whichPlayer > 0)
		{
			player_state_cache[player_id].state = GetPlayerSlotState_real(whichPlayer);
			return player_state_cache[player_id].state;
		}
		player_state_cache[player_id].state = -1;
	}
	else
	{
		return player_state_cache[player_id].state;
	}

	return -1;
}

//
//BOOL __cdecl IsPlayerEnemy(int hplayer1, int hplayer2)
//{
//	if (hplayer1 != hplayer2)
//	{
//		if (hplayer1 > 0 && hplayer2 > 0)
//		{
//			return IsPlayerEnemy_real(hplayer1, hplayer2) && IsPlayerEnemy_real(hplayer2, hplayer1);
//		}
//	}
//	return FALSE;
//}

struct PLAYER_ENEMY_CACHE
{
	BOOL enemystate[TOTAL_MAX_PLAYERS_ARRAY];
	long long checktime[TOTAL_MAX_PLAYERS_ARRAY];
};

PLAYER_ENEMY_CACHE player_enemy_cache[TOTAL_MAX_PLAYERS_ARRAY];

BOOL __fastcall IsPlayerEnemyById(int id1, int id2)
{
	if (id1 < 0 || id2 < 0 || id1 >= TOTAL_MAX_PLAYERS_ARRAY || id2 >= TOTAL_MAX_PLAYERS_ARRAY)
	{
		return FALSE;
	}

	if (player_enemy_cache[id1].checktime[id2] == 0 ||
		llabs(CurTickCount - player_enemy_cache[id1].checktime[id2] > 30000))
	{
		player_enemy_cache[id1].checktime[id2] = CurTickCount;
		int whichPlayer1 = Player(id1);
		int whichPlayer2 = Player(id2);

		if (whichPlayer1 <= 0 || whichPlayer2 <= 0)
		{
			player_enemy_cache[id1].enemystate[id2] = FALSE;
		}
		else
		{
			BOOL isEnemy = IsPlayerEnemy_real(whichPlayer1, whichPlayer2) && IsPlayerEnemy_real(whichPlayer2, whichPlayer1);
			player_enemy_cache[id1].enemystate[id2] = isEnemy;
		}
	}

	return player_enemy_cache[id1].enemystate[id2];
}

//int GetPlayerTeam(unsigned char* playeraddr)
//{
//	if (!playeraddr)
//		return 0;
//	return *(int*)(playeraddr + 0x278);
//}

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
pGetOrderPointX GetOrderPointX;

typedef DWFP(__cdecl* pGetOrderPointY)();
pGetOrderPointY GetOrderPointY;

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

	if (DetectQuality >= 2 && CheckCenter)
	{
		if (ReplayMoreSens && IsReplayFound)
			return CheckCenter;

		BOOL CheckRight = IsFoggedToPlayerReal(&x2, &y2, player);
		BOOL CheckLeft = IsFoggedToPlayerReal(&x3, &y3, player);
		BOOL CheckTop = IsFoggedToPlayerReal(&x4, &y4, player);
		BOOL CheckBot = IsFoggedToPlayerReal(&x5, &y5, player);

		if (DetectQuality > 3 && CheckRight && CheckLeft && CheckTop && CheckBot)
		{
			x2 = x1 + 64;
			y2 = y1 + 64;

			x3 = x1 - 64;
			y3 = y1 - 64;

			x4 = x1 - 64;
			y4 = y1 + 64;

			x5 = x1 + 64;
			y5 = y1 - 64;

			CheckRight = IsFoggedToPlayerReal(&x2, &y2, player);
			CheckLeft = IsFoggedToPlayerReal(&x3, &y3, player);
			CheckTop = IsFoggedToPlayerReal(&x4, &y4, player);
			CheckBot = IsFoggedToPlayerReal(&x5, &y5, player);

			return CheckRight && CheckLeft && CheckTop && CheckBot;
		}

		return CheckRight && CheckLeft && CheckTop && CheckBot;
	}

	return CheckCenter;
}


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
			//*z = *(float*)(iteminfo + 0x90);
			*z = 0.0f;
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
	FillItemCountAndItemArray();
	for (int i = 0; i < SafeItemCount; i++)
	{
		if (SafeItemArray[i])
		{
			if (IsNotBadItem(SafeItemArray[i]))
			{
				float itemx = 0.0f, itemy = 0.0f, itemz = 0.0f;
				GetItemLocation3D(SafeItemArray[i], &itemx, &itemy, &itemz);
				if (Distance2D(itemx, itemy, x, y) < 14.4f /*GetSafeObjectSelectionCircle(SafeItemArray[i])*/)
					return SafeItemArray[i];
			}
		}
	}
	return 0;
}

BOOL ImpossibleClick = FALSE;

int GetUnitByXY(float x, float y, int playerid, BOOL onlyunits = FALSE)
{
	if (!IsReplayFound && !DetectLocalPlayer && playerid == GetLocalPlayerNumber())
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
				if (selectionCircle > 70.0f)
					selectionCircle = 70.0f;
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
					if (IsValidTowerDetect || IsPlayerEnemyById(playerid, unitowner))
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
	if (onlyunits)
	{
		return 0;
	}

	return GetItemByXY(x, y, playerid);
}

int LastEventID, LastSkillID, LastCasterID;
int LastEventID_2, LastSkillID_2, LastCasterID_2;
long long LastEventTime = 0;

struct PlayerEvent
{
	int EventID;
	int SkillID;
	int OrderID;
	int Caster;
	int SelectedUnits;
	long long Time;
};


std::vector<PlayerEvent> PlayerEventList[MAX_PLAYERS];

//BOOL PlayerMeepoDetect[ 20 ];
void ScanForTechiesBot(int PlayerID, PlayerEvent NewPlayerEvent)
{
	if (TechiesDetonateId)
	{
		PlayerEvent Event1 = PlayerEventList[PlayerID][19];
		PlayerEvent Event2 = PlayerEventList[PlayerID][18];
		PlayerEvent Event3 = PlayerEventList[PlayerID][17];

		/*WatcherLog( "[event1][+%u ms][LogActions] : Event:%i - Skill:%X - Caster:%X - units %i \n", Event1.Time, Event1.EventID, Event1.SkillID, Event1.Caster,Event1.SelectedUnits );
		WatcherLog( "[event2][+%u ms][LogActions] : Event:%i - Skill:%X - Caster:%X - units %i\n", Event2.Time, Event2.EventID, Event2.SkillID, Event2.Caster, Event1.SelectedUnits );
		WatcherLog( "[event3][+%u ms][LogActions] : Event:%i - Skill:%X - Caster:%X - units %i\n", Event3.Time, Event3.EventID, Event3.SkillID, Event3.Caster, Event1.SelectedUnits );
		WatcherLog( "[event4][+%u ms][LogActions] : Event:%i - Skill:%X - Caster:%X - units %i\n", Event4.Time, Event4.EventID, Event4.SkillID, Event4.Caster, Event1.SelectedUnits );
		*/

		if (Event1.OrderID == TechiesDetonateId &&
			Event2.OrderID == TechiesDetonateId &&
			Event3.OrderID == TechiesDetonateId)
		{
			if (Event1.Caster != 0 &&
				Event2.Caster != 0 &&
				Event3.Caster != 0)
			{
				if (Event1.SelectedUnits != Event2.SelectedUnits ||
					Event2.SelectedUnits != Event3.SelectedUnits
					)
				{
					if (Event1.Caster != Event2.Caster &&
						Event1.Caster != Event3.Caster)
					{
						if (
							Event1.Time < 100 &&
							Event2.Time < 100 &&
							Event3.Time < 100)
						{
							sprintf_s(PrintBuffer, 2048, "|c00EF4000[FogCW v18.4]|r: Player %s%s|r use |c00EF1000TechiesBot|r!!\0",
								GetPlayerColorString(PlayerID),
								GetPlayerName(PlayerID, 0));

							ActionTime = CurGameTime;
							DisplayText(PrintBuffer, 14.4f);
							SendPause();
						}
					}
				}
			}
		}
	}
}

int MeepoPoofID = 0x41304E38;
void ScanForMeepoBot(int PlayerID, PlayerEvent NewPlayerEvent)
{
	if (MeepoPoofID != 0)
	{
		PlayerEvent Event1 = PlayerEventList[PlayerID][19];
		PlayerEvent Event2 = PlayerEventList[PlayerID][18];
		PlayerEvent Event3 = PlayerEventList[PlayerID][17];
		PlayerEvent Event4 = PlayerEventList[PlayerID][16];

		/*WatcherLog( "[event1][+%u ms][LogActions] : Event:%i - Skill:%X - Caster:%X - units %i \n", Event1.Time, Event1.EventID, Event1.SkillID, Event1.Caster,Event1.SelectedUnits );
		WatcherLog( "[event2][+%u ms][LogActions] : Event:%i - Skill:%X - Caster:%X - units %i\n", Event2.Time, Event2.EventID, Event2.SkillID, Event2.Caster, Event1.SelectedUnits );
		WatcherLog( "[event3][+%u ms][LogActions] : Event:%i - Skill:%X - Caster:%X - units %i\n", Event3.Time, Event3.EventID, Event3.SkillID, Event3.Caster, Event1.SelectedUnits );
		WatcherLog( "[event4][+%u ms][LogActions] : Event:%i - Skill:%X - Caster:%X - units %i\n", Event4.Time, Event4.EventID, Event4.SkillID, Event4.Caster, Event1.SelectedUnits );
		*/

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
						WatcherLog("[event2][+%u ms][LogActions] : Event:%i - Skill:%X - Caster:%X - units %i\n", Event2.Time, Event2.EventID, Event2.SkillID, Event2.Caster, Event2.SelectedUnits);
						WatcherLog("[event3][+%u ms][LogActions] : Event:%i - Skill:%X - Caster:%X - units %i\n", Event3.Time, Event3.EventID, Event3.SkillID, Event3.Caster, Event3.SelectedUnits);
						WatcherLog("[event4][+%u ms][LogActions] : Event:%i - Skill:%X - Caster:%X - units %i\n", Event4.Time, Event4.EventID, Event4.SkillID, Event4.Caster, Event4.SelectedUnits);
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
							sprintf_s(PrintBuffer, 2048, "|c00EF4000[FogCW v18.4]|r: Player %s%s|r use |c00EF1000MeepoKey|r!!\0",
								GetPlayerColorString(PlayerID),
								GetPlayerName(PlayerID, 0));

							ActionTime = CurGameTime;
							DisplayText(PrintBuffer, 14.4f);
							SendPause();
							//}
						}
					}
				}
			}
		}
	}
}

void AddNewPlayerEvent(int PlayerID, PlayerEvent NewPlayerEvent)
{
	if (DebugLog)
		WatcherLog("[DEBUG][+%u ms][LogActions] : Event:%i - Skill:%X - Caster:%X \n", NewPlayerEvent.Time, NewPlayerEvent.EventID, NewPlayerEvent.SkillID, NewPlayerEvent.Caster);

	if (PlayerEventList[PlayerID].size() < 20)
	{
		for (int i = 0; i < 19; i++)
		{
			PlayerEvent tmpPlayerEvent;
			memset(&tmpPlayerEvent, 0, sizeof(PlayerEvent));
			PlayerEventList[PlayerID].push_back(tmpPlayerEvent);
		}
		PlayerEventList[PlayerID].push_back(NewPlayerEvent);
	}
	else
	{
		PlayerEventList[PlayerID].erase(PlayerEventList[PlayerID].begin());
		PlayerEventList[PlayerID].push_back(NewPlayerEvent);
	}
}

void BotDetector(const int SkillID, const int EventID, const int OrderId, const int CasterID, const char* addinfo = "")
{
	if (abs(GetGameTime() - CurGameTime > 1000))
		return;
	/*WatcherLog("[+%ums][LogActions] : ", (CurGameTime - LastEventTime));
	WatcherLog("Skill:%X - ", SkillID);
	WatcherLog("EventID:%u - ", EventID);
	WatcherLog("OrderId:%X - ", OrderId);
	{
		int CasterAddr = GetHandleUnitAddress(CasterID, 0);
		if (CasterAddr > 0)
		{
			int CasterSlot = GetUnitOwnerSlot(CasterAddr);
			if (CasterSlot < MAX_PLAYERS && CasterSlot >= 0)
			{
				WatcherLog("Selected units:%X - ", GetSelectedUnitCount(CasterSlot, FALSE));
			}
		}
	}
	WatcherLog("CasterID:%X - %s\n", CasterID, addinfo);*/
	// THE BOT DETECTOR!
	if (CasterID > 0 && (EventID == 272 || EventID == 38))
	{
		int CasterAddr = GetHandleUnitAddress(CasterID, 0);
		if (CasterAddr > 0)
		{
			int CasterSlot = GetUnitOwnerSlot(CasterAddr);
			if (CasterSlot < MAX_PLAYERS && CasterSlot >= 0)
			{
				if (CasterSlot >= 0 && CasterSlot < MAX_PLAYERS && (IsReplayFound || GetLocalPlayerNumber() != CasterSlot || DetectLocalPlayer))
				{
					PlayerEvent NewPlayerEvent;
					NewPlayerEvent.Caster = CasterAddr;
					NewPlayerEvent.EventID = EventID;
					NewPlayerEvent.SkillID = SkillID;
					NewPlayerEvent.OrderID = OrderId;
					NewPlayerEvent.Time = llabs(CurGameTime - LastEventTime);
					NewPlayerEvent.SelectedUnits = GetSelectedUnitCount(CasterSlot, FALSE);
					AddNewPlayerEvent(CasterSlot, NewPlayerEvent);
					if (EventID == 272)
					{
						ScanForMeepoBot(CasterSlot, NewPlayerEvent);
					}
					else if (EventID == 38)
					{
						ScanForTechiesBot(CasterSlot, NewPlayerEvent);
					}
					LastEventTime = CurGameTime;
				}
			}
		}
	}
}

struct ProcessNewAction
{
	BOOL IsGetSpellAbilityId;
	int CasterPlayerHandle;
	int CasterUnitHandle;
	int TargetUnitHandle;
	int TargetItemHandle;
	int GetIssuedOrderId;
	int SkillID;
	int EventID;
	float GetSpellOrderTargetX;
	float GetSpellOrderTargetY;
	long long addtime;
};

std::vector<ProcessNewAction> ProcessNewActionList;

void ProcessGetSpellAbilityIdAction(const ProcessNewAction& action)
{
	/*if (action.CasterUnitHandle > 0)
	{
		if (action.TargetUnitHandle > 0)
		{
			int CasterAddr = GetHandleUnitAddress(action.CasterUnitHandle, 0);
			int TargetAddr = GetHandleUnitAddress(action.TargetUnitHandle, 0);
			if (CasterAddr > 0 && IsNotBadUnit(CasterAddr) && TargetAddr > 0 && IsNotBadUnit(TargetAddr))
			{
				int CasterSlot = GetUnitOwnerSlot(CasterAddr);
				int TargetSlot = GetUnitOwnerSlot(TargetAddr);

				if (action.GetIssuedOrderId != 0 && CasterSlot >= 0 && CasterSlot < MAX_PLAYERS && TargetSlot >= 0 && TargetSlot < TOTAL_MAX_PLAYERS_ARRAY &&
					(IsHero(TargetAddr) || (!DetectRightClickOnlyHeroes && !IsUnitTower(TargetAddr)))
					&& (IsReplayFound || CasterSlot != GetLocalPlayerNumber() || DetectLocalPlayer)
					&& TargetSlot != CasterSlot && IsPlayerEnemyById(CasterSlot, TargetSlot))
				{
					if (IsFoggedInHistory(CasterSlot, TargetAddr))
					{
						float unitx = 0.0f, unity = 0.0f, unitz = 0.0f;
						GetUnitLocation3D(TargetAddr, &unitx, &unity, &unitz);
						if (IsFoggedToPlayerMy(&unitx, &unity, Player(CasterSlot)))
						{
							if (MinimapPingFogClick && IsReplayMode)
							{
								unsigned int PlayerColorInt = GetPlayerColorUINT(CasterSlot);
								PingMinimapMy(&unitx, &unity, &pingduration, PlayerColorInt & 0x00FF0000, PlayerColorInt & 0x0000FF00, PlayerColorInt & 0x000000FF, false);
							}

							sprintf_s(PrintBuffer, 2048, "|c00EF4000[FogCW v18.4]|r: Player %s%s|r use ability(%s) in fogged %s%s|r[TARGET]\0",
								GetPlayerColorString(CasterSlot),
								GetPlayerName(CasterSlot, 0),
								ConvertIdToString(action.GetIssuedOrderId).c_str(),
								GetPlayerColorString(TargetSlot),
								GetObjectName(TargetAddr));

							ActionTime = CurGameTime;
							DisplayText(PrintBuffer, 14.4f);
							SendPause();
						}
						else if (!IsUnitVisibleToPlayer((unsigned char*)TargetAddr, (unsigned char*)GetPlayerByNumber(CasterSlot)))
						{
							if (MinimapPingFogClick && IsReplayMode)
							{
								unsigned int PlayerColorInt = GetPlayerColorUINT(CasterSlot);
								PingMinimapMy(&unitx, &unity, &pingduration, PlayerColorInt & 0x00FF0000, PlayerColorInt & 0x0000FF00, PlayerColorInt & 0x000000FF, false);
							}

							sprintf_s(PrintBuffer, 2048, "|c00EF4000[FogCW v18.4]|r: Player %s%s|r use ability(%s) in invisibled %s%s|r[TARGET]\0",
								GetPlayerColorString(CasterSlot),
								GetPlayerName(CasterSlot, 0),
								ConvertIdToString(action.GetIssuedOrderId).c_str(),
								GetPlayerColorString(TargetSlot),
								GetObjectName(TargetAddr));

							ActionTime = CurGameTime;
							DisplayText(PrintBuffer, 14.4f);
							SendPause();
						}
					}
				}
			}
		}
		else if (action.GetIssuedOrderId != 0 && DetectPointClicks && fabs(action.GetSpellOrderTargetX) > 0.01f && fabs(action.GetSpellOrderTargetY) > 0.01f)
		{
			int CasterAddr = GetHandleUnitAddress(action.CasterUnitHandle, 0);

			if (CasterAddr > 0)
			{
				int CasterOwner = GetUnitOwnerSlot(CasterAddr);

				float x = action.GetSpellOrderTargetX;
				float y = action.GetSpellOrderTargetY;
				int TargetAddr = GetUnitByXY(x, y, CasterOwner, TRUE);

				if (TargetAddr > 0 && (IsNotBadUnit(TargetAddr) && (IsHero(TargetAddr) || (!DetectRightClickOnlyHeroes && !IsUnitTower(TargetAddr)))))
				{
					if (IsFoggedInHistory(CasterOwner, TargetAddr))
					{
						int TargetSlot = GetUnitOwnerSlot(TargetAddr);

						float unitx = 0.0f, unity = 0.0f, unitz = 0.0f;
						GetUnitLocation3D(TargetAddr, &unitx, &unity, &unitz);
						if (IsFoggedToPlayerMy(&unitx, &unity, Player(CasterOwner)))
						{
							if (MinimapPingFogClick && IsReplayMode)
							{
								unsigned int PlayerColorInt = GetPlayerColorUINT(CasterOwner);
								PingMinimapMy(&unitx, &unity, &pingduration, PlayerColorInt & 0x00FF0000, PlayerColorInt & 0x0000FF00, PlayerColorInt & 0x000000FF, false);
							}

							sprintf_s(PrintBuffer, 2048, "|c00EF4000[FogCW v18.4]|r: Player %s%s|r use ability(%s) in fogged %s%s|r[POINT]\0",
								GetPlayerColorString(CasterOwner),
								GetPlayerName(CasterOwner, 0),
								ConvertIdToString(action.GetIssuedOrderId).c_str(),
								GetPlayerColorString(TargetSlot),
								GetObjectName(TargetAddr));

							ActionTime = CurGameTime;
							DisplayText(PrintBuffer, 14.4f);
							SendPause();
						}
						else if (!IsUnitVisibleToPlayer((unsigned char*)TargetAddr, (unsigned char*)GetPlayerByNumber(CasterOwner)))
						{
							if (MinimapPingFogClick && IsReplayMode)
							{
								unsigned int PlayerColorInt = GetPlayerColorUINT(CasterOwner);
								PingMinimapMy(&unitx, &unity, &pingduration, PlayerColorInt & 0x00FF0000, PlayerColorInt & 0x0000FF00, PlayerColorInt & 0x000000FF, false);
							}

							sprintf_s(PrintBuffer, 2048, "|c00EF4000[FogCW v18.4]|r: Player %s%s|r use ability(%s) in invisibled %s%s|r[POINT]\0",
								GetPlayerColorString(CasterOwner),
								GetPlayerName(CasterOwner, 0),
								ConvertIdToString(action.GetIssuedOrderId).c_str(),
								GetPlayerColorString(TargetSlot),
								GetObjectName(TargetAddr));

							ActionTime = CurGameTime;
							DisplayText(PrintBuffer, 14.4f);
							SendPause();
						}
					}
				}
			}
		}
	}*/
}

void ProcessGetTriggerEventAction(const ProcessNewAction& action)
{
	if (abs(GetGameTime() - CurGameTime > 1000))
		return;
	if (action.EventID == 77 || action.EventID == 40)
	{
		BOOL IsItem = FALSE;
		int TargetUnitHandle = action.TargetUnitHandle;

		if (TargetUnitHandle <= 0)
		{
			TargetUnitHandle = action.TargetItemHandle;
			if (TargetUnitHandle > 0)
				IsItem = TRUE;
			else
			{
				return;
			}
		}

		if (action.CasterUnitHandle > 0)
		{
			int CasterAddr = GetHandleUnitAddress(action.CasterUnitHandle, 0);
			int TargetAddr = (IsItem ? GetHandleItemAddress(TargetUnitHandle, 0) : GetHandleUnitAddress(TargetUnitHandle, 0));
			if (CasterAddr > 0 && TargetAddr > 0)
			{
				int CasterSlot = GetUnitOwnerSlot(CasterAddr);
				int TargetSlot = (IsItem ? 15 : GetUnitOwnerSlot(TargetAddr));

				if ((CasterSlot >= 0 && CasterSlot < MAX_PLAYERS) && (TargetSlot >= 0 && TargetSlot < TOTAL_MAX_PLAYERS_ARRAY) && Player(CasterSlot) > 0 && (IsItem || Player(TargetSlot) > 0))
				{
					BOOL IsOkay = IsItem ? (IsNotBadItem(TargetAddr) && !(action.GetIssuedOrderId >= 0xD0022 && action.GetIssuedOrderId <= 0xD0028))
						: (IsNotBadUnit(TargetAddr) && (IsHero(TargetAddr) || (!DetectRightClickOnlyHeroes && !IsUnitTower(TargetAddr))));

					if (TargetSlot != CasterSlot && (IsReplayFound || CasterSlot != GetLocalPlayerNumber() || DetectLocalPlayer)
						&& IsOkay && (TargetSlot >= MAX_PLAYERS || IsPlayerEnemyById(CasterSlot, TargetSlot)))
					{
						if (IsItem || IsFoggedInHistory(CasterSlot, TargetAddr))
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
								if (MinimapPingFogClick && IsReplayMode)
								{
									unsigned int PlayerColorInt = GetPlayerColorUINT(CasterSlot);
									PingMinimapMy(&unitx, &unity, &pingduration, PlayerColorInt & 0x00FF0000, PlayerColorInt & 0x0000FF00, PlayerColorInt & 0x000000FF, false);
								}
								sprintf_s(PrintBuffer, 2048, "|c00EF4000[FogCW v18.4]|r: Player %s%s|r use %s in fogged %s %s%s|r[TARGET]\0",
									GetPlayerColorString(CasterSlot),
									GetPlayerName(CasterSlot, 0),
									ConvertIdToString(action.GetIssuedOrderId).c_str(),
									IsItem ? "[item]" : "[unit]",
									IsItem ? "|c004B4B4B" : GetPlayerColorString(TargetSlot),
									GetObjectName(TargetAddr));

								ActionTime = CurGameTime;
								DisplayText(PrintBuffer, 14.4f);
								SendPause();
							}
							else if (!IsItem && !IsUnitVisibleToPlayer((unsigned char*)TargetAddr, (unsigned char*)GetPlayerByNumber(CasterSlot)))
							{
								if (MinimapPingFogClick && IsReplayMode)
								{
									unsigned int PlayerColorInt = GetPlayerColorUINT(CasterSlot);
									PingMinimapMy(&unitx, &unity, &pingduration, PlayerColorInt & 0x00FF0000, PlayerColorInt & 0x0000FF00, PlayerColorInt & 0x000000FF, false);
								}
								sprintf_s(PrintBuffer, 2048, "|c00EF4000[FogCW v18.4]|r: Player %s%s|r use %s in invisibled %s%s|r[TARGET]\0",
									GetPlayerColorString(CasterSlot),
									GetPlayerName(CasterSlot, 0),
									ConvertIdToString(action.GetIssuedOrderId).c_str(),
									GetPlayerColorString(TargetSlot),
									GetObjectName(TargetAddr));

								ActionTime = CurGameTime;
								DisplayText(PrintBuffer, 14.4f);
								SendPause();
							}
							else if (IsItem && (DetectItemDestroyer || DetectOwnItems) && action.GetIssuedOrderId == 0xD000F)
							{
								if ((TargetSlot != CasterSlot && DetectItemDestroyer) || DetectOwnItems)
								{
									if (!IsPlayerEnemyById(CasterSlot, TargetSlot))
									{
										if (MinimapPingFogClick && IsReplayMode)
										{
											unsigned int PlayerColorInt = GetPlayerColorUINT(CasterSlot);
											PingMinimapMy(&unitx, &unity, &pingduration, PlayerColorInt & 0x00FF0000, PlayerColorInt & 0x0000FF00, PlayerColorInt & 0x000000FF, false);
										}
										sprintf_s(PrintBuffer, 2048, "|c00EF4000[FogCW v18.4]|r: Player %s%s|r try to destroy item %s%s|r\0",
											GetPlayerColorString(CasterSlot),
											GetPlayerName(CasterSlot, 0),
											GetPlayerColorString(TargetSlot),
											GetObjectName(TargetAddr));

										ActionTime = CurGameTime;
										DisplayText(PrintBuffer, 14.4f);
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

	if (action.EventID == 76 || action.EventID == 39)
	{
		if (action.CasterUnitHandle > 0)
		{
			int CasterAddr = GetHandleUnitAddress(action.CasterUnitHandle, 0);

			float x = action.GetSpellOrderTargetX;
			float y = action.GetSpellOrderTargetY;
			float unitx = 0.0f, unity = 0.0f, unitz = 0.0f;
			if (CasterAddr > 0 && fabs(x) > 0.01f && fabs(y) > 0.01f)
			{
				int CasterSlot = GetUnitOwnerSlot(CasterAddr);

				if ((CasterSlot >= 0 && CasterSlot < MAX_PLAYERS) && Player(CasterSlot) > 0)
				{
					int TargetAddr = GetUnitByXY(x, y, CasterSlot);
					BOOL DetectionImpossibleClick = FALSE;

					if (TargetAddr > 0 && (IsNotBadUnit(TargetAddr) && (IsHero(TargetAddr) || (DetectImpossibleClicks && IsDetectedTower(TargetAddr)) || (!DetectRightClickOnlyHeroes && !IsUnitTower(TargetAddr)))))
					{
						int TargetSlot = GetUnitOwnerSlot(TargetAddr);
						if ((TargetSlot >= 0 && TargetSlot < TOTAL_MAX_PLAYERS_ARRAY) && Player(TargetSlot) > 0)
						{
							if (IsFoggedInHistory(CasterSlot, TargetAddr))
							{
								GetUnitLocation3D(TargetAddr, &unitx, &unity, &unitz);
								if (DetectImpossibleClicks && IsDetectedTower(TargetAddr))
								{
									DetectionImpossibleClick = TRUE;
								}

								if ((DetectPointClicks || DetectionImpossibleClick) && IsFoggedToPlayerMy(&unitx, &unity, Player(CasterSlot)))
								{
									if (MinimapPingFogClick && IsReplayMode)
									{
										unsigned int PlayerColorInt = GetPlayerColorUINT(CasterSlot);
										PingMinimapMy(&unitx, &unity, &pingduration, PlayerColorInt & 0x00FF0000, PlayerColorInt & 0x0000FF00, PlayerColorInt & 0x000000FF, false);
									}

									if (!DetectImpossibleClicks || (action.GetIssuedOrderId == 0xD0012 || action.GetIssuedOrderId == 0xD0003))
									{
										sprintf_s(PrintBuffer, 2048, "|c00EF4000[FogCW v18.4]|r: Player %s%s|r use %s in fogged [unit] %s%s|r[POINT]\0",
											GetPlayerColorString(CasterSlot),
											GetPlayerName(CasterSlot, 0),
											ImpossibleClick ? "-HACKCLICK-" : ConvertIdToString(action.GetIssuedOrderId).c_str(),
											GetPlayerColorString(TargetSlot),
											GetObjectName(TargetAddr));

										ActionTime = CurGameTime;
										DisplayText(PrintBuffer, 14.4f);
									}
									ImpossibleClick = FALSE;
									SendPause();
								}
								else if (DetectPointClicks && !DetectionImpossibleClick && !IsUnitVisibleToPlayer((unsigned char*)TargetAddr, (unsigned char*)GetPlayerByNumber(CasterSlot)))
								{
									if (MinimapPingFogClick && IsReplayMode)
									{
										unsigned int PlayerColorInt = GetPlayerColorUINT(CasterSlot);
										PingMinimapMy(&unitx, &unity, &pingduration, PlayerColorInt & 0x00FF0000, PlayerColorInt & 0x0000FF00, PlayerColorInt & 0x000000FF, false);
									}

									sprintf_s(PrintBuffer, 2048, "|c00EF4000[FogCW v18.4]|r: Player %s%s|r use %s in invisibled %s%s|r[POINT]\0",
										GetPlayerColorString(CasterSlot),
										GetPlayerName(CasterSlot, 0),
										ConvertIdToString(action.GetIssuedOrderId).c_str(),
										GetPlayerColorString(TargetSlot),
										GetObjectName(TargetAddr));

									ActionTime = CurGameTime;
									DisplayText(PrintBuffer, 14.4f);
									SendPause();
								}
							}
						}
						else if (DetectPointClicks && IsNotBadItem(TargetAddr) && !(action.GetIssuedOrderId >= 0xD0022 && action.GetIssuedOrderId <= 0xD0028))
						{
							float xunitx = 0.0f, xunity = 0.0f, xunitz = 0.0f;
							GetItemLocation3D(TargetAddr, &xunitx, &xunity, &xunitz);
							if (IsFoggedToPlayerMy(&xunitx, &xunity, Player(CasterSlot)))
							{
								if (MinimapPingFogClick && IsReplayMode)
								{
									unsigned int PlayerColorInt = GetPlayerColorUINT(CasterSlot);
									PingMinimapMy(&xunitx, &xunity, &pingduration, PlayerColorInt & 0x00FF0000, PlayerColorInt & 0x0000FF00, PlayerColorInt & 0x000000FF, false);
								}

								sprintf_s(PrintBuffer, 2048, "|c00EF4000[FogCW v18.4]|r: Player %s%s|r use %s in fogged [item] %s%s|r[POINT]\0",
									GetPlayerColorString(CasterSlot),
									GetPlayerName(CasterSlot, 0),
									ConvertIdToString(action.GetIssuedOrderId).c_str(),
									"|c004B4B4B",
									GetObjectName(TargetAddr));

								ActionTime = CurGameTime;
								DisplayText(PrintBuffer, 14.4f);
								SendPause();
							}
						}
					}
				}
			}
			else if (CasterAddr > 0 && (IsReplayFound || GetUnitOwnerSlot(CasterAddr) != GetLocalPlayerNumber() || DetectLocalPlayer) && abs(x) < 0.001f && abs(y) < 0.001f)
			{
				if (action.GetIssuedOrderId == 851971)
				{
					int slott = GetUnitOwnerSlot(CasterAddr);


					sprintf_s(PrintBuffer, 2048, "|c00EF4000[FogCW v18.4]|r: Player %s%s|r activate GuAI Maphack!!\0",
						GetPlayerColorString(slott),
						GetPlayerName(slott, 0));

					ActionTime = CurGameTime;
					DisplayText(PrintBuffer, 14.4f);
					SendPause();
				}
			}
		}

	}
}

void ActionProcessFunction()
{
	std::vector<ProcessNewAction> tmpProcessNewActionList;
	for (auto& CurrentAction : ProcessNewActionList)
	{
		if (CurGameTime - CurrentAction.addtime > 20)
		{
			if (CurrentAction.IsGetSpellAbilityId)
			{
				ProcessGetSpellAbilityIdAction(CurrentAction);
			}
			else
			{
				ProcessGetTriggerEventAction(CurrentAction);
			}
		}
		else
		{
			tmpProcessNewActionList.push_back(CurrentAction);
		}
	}
	ProcessNewActionList = tmpProcessNewActionList;
}

typedef std::chrono::high_resolution_clock Clock;

std::chrono::time_point<std::chrono::high_resolution_clock> GetTriggerEventIdTime = Clock::now();

BOOL GetTriggerEventIdCalled = FALSE;
int __cdecl GetTriggerEventId_hooked()
{
	int TriggerEventId = GetTriggerEventId_real();

	if (!GameStarted || !GameStartedReally || !FogClickEnabled || GetTriggerEventIdCalled)
		return TriggerEventId;
	GetTriggerEventIdCalled = TRUE;

	if (!FullEventHookProcess)
	{
		auto end = Clock::now();
		auto int_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - GetTriggerEventIdTime);
		if (int_ms.count() <= 1)
		{
			GetTriggerEventIdCalled = FALSE;
			return TriggerEventId;
		}
	}

	ProcessNewAction tmpProcessNewAction;
	tmpProcessNewAction.addtime = CurGameTime;
	tmpProcessNewAction.CasterUnitHandle = GetTriggerUnit();

	if (!tmpProcessNewAction.CasterUnitHandle)
	{
		GetTriggerEventIdCalled = FALSE;
		return TriggerEventId;
	}

	int unitaddr = GetHandleUnitAddress(tmpProcessNewAction.CasterUnitHandle, 0);

	if (!unitaddr || !IsNotBadUnit(unitaddr))
	{
		GetTriggerEventIdCalled = FALSE;
		return TriggerEventId;
	}


	bool anothercheck = false;
	int pid = 0;
	int hPlayer = GetTriggerPlayer();
	if (hPlayer <= 0)
	{
		anothercheck = true;
		pid = GetUnitOwnerSlot(unitaddr);

		if (pid < 0 || pid > MAX_PLAYERS)
		{
			GetTriggerEventIdCalled = FALSE;
			return TriggerEventId;
		}

		hPlayer = Player(pid);

		if (hPlayer <= 0)
		{
			GetTriggerEventIdCalled = FALSE;
			return TriggerEventId;
		}
	}

	if (!anothercheck)
	{
		pid = GetPlayerId(hPlayer);

		if (pid < 0 || pid > MAX_PLAYERS)
		{
			pid = GetUnitOwnerSlot(unitaddr);

			if (pid < 0 || pid > MAX_PLAYERS)
			{
				GetTriggerEventIdCalled = FALSE;
				return TriggerEventId;
			}

			hPlayer = Player(pid);

			if (hPlayer <= 0)
			{
				GetTriggerEventIdCalled = FALSE;
				return TriggerEventId;
			}
		}
	}

	if (GetPlayerControllerById(pid) != 0 || GetPlayerSlotStateById(pid) != 1)
	{
		GetTriggerEventIdCalled = FALSE;
		return TriggerEventId;
	}

	if (!FullEventHookProcess)
	{
		GetTriggerEventIdTime = Clock::now();
	}

	tmpProcessNewAction.CasterPlayerHandle = hPlayer;

	if (ProcessNewActionList.size() > 0)
	{
		const auto& action = ProcessNewActionList[ProcessNewActionList.size() - 1];
		if (action.CasterPlayerHandle == hPlayer
			&& action.EventID == TriggerEventId
			&& action.CasterUnitHandle == tmpProcessNewAction.CasterUnitHandle)
		{
			GetTriggerEventIdCalled = FALSE;
			return TriggerEventId;
		}
	}

	tmpProcessNewAction.EventID = TriggerEventId;
	tmpProcessNewAction.GetIssuedOrderId = GetIssuedOrderId_real();
	tmpProcessNewAction.IsGetSpellAbilityId = FALSE;
	tmpProcessNewAction.SkillID = GetSpellAbilityId_real();
	tmpProcessNewAction.TargetItemHandle = GetOrderTargetItem();
	tmpProcessNewAction.TargetUnitHandle = GetOrderTargetUnit();
	tmpProcessNewAction.GetSpellOrderTargetX = GetOrderPointX().fl;
	tmpProcessNewAction.GetSpellOrderTargetY = GetOrderPointY().fl;

	if (ProcessNewActionList.size() < 10000)
		ProcessNewActionList.push_back(tmpProcessNewAction);

	BotDetector(tmpProcessNewAction.SkillID, tmpProcessNewAction.EventID, tmpProcessNewAction.GetIssuedOrderId, tmpProcessNewAction.CasterUnitHandle, "GETTRIGGEREVENT");
	GetTriggerEventIdCalled = FALSE;
	return TriggerEventId;
}

std::chrono::time_point<std::chrono::high_resolution_clock> GetSpellAbilityIdTime = Clock::now();

BOOL GetSpellAbilityIdCalled = FALSE;
int __cdecl GetSpellAbilityId_hooked()
{
	int SpellAbilityId = GetSpellAbilityId_real();

	if (!GameStarted || !GameStartedReally || !FogClickEnabled || GetSpellAbilityIdCalled)
		return SpellAbilityId;
	GetSpellAbilityIdCalled = TRUE;

	if (!FullEventHookProcess)
	{
		auto end = Clock::now();
		auto int_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - GetSpellAbilityIdTime);

		if (int_ms.count() <= 1)
		{
			GetSpellAbilityIdCalled = FALSE;
			return SpellAbilityId;
		}
	}

	ProcessNewAction tmpProcessNewAction;
	tmpProcessNewAction.addtime = CurGameTime;
	tmpProcessNewAction.CasterUnitHandle = GetSpellAbilityUnit_real();

	if (!tmpProcessNewAction.CasterUnitHandle)
	{
		GetSpellAbilityIdCalled = FALSE;
		return SpellAbilityId;
	}

	int unitaddr = GetHandleUnitAddress(tmpProcessNewAction.CasterUnitHandle, 0);

	if (!unitaddr || !IsNotBadUnit(unitaddr))
	{
		GetSpellAbilityIdCalled = FALSE;
		return SpellAbilityId;
	}

	bool anothercheck = false;
	int pid = 0;
	int hPlayer = GetTriggerPlayer();
	if (hPlayer <= 0)
	{
		anothercheck = true;
		pid = GetUnitOwnerSlot(unitaddr);

		if (pid < 0 || pid > MAX_PLAYERS)
		{
			GetSpellAbilityIdCalled = FALSE;
			return SpellAbilityId;
		}

		hPlayer = Player(pid);

		if (hPlayer <= 0)
		{
			GetSpellAbilityIdCalled = FALSE;
			return SpellAbilityId;
		}
	}

	if (!anothercheck)
	{
		pid = GetPlayerId(hPlayer);

		if (pid < 0 || pid > MAX_PLAYERS)
		{
			pid = GetUnitOwnerSlot(unitaddr);

			if (pid < 0 || pid > MAX_PLAYERS)
			{
				GetSpellAbilityIdCalled = FALSE;
				return SpellAbilityId;
			}

			hPlayer = Player(pid);

			if (hPlayer <= 0)
			{
				GetSpellAbilityIdCalled = FALSE;
				return SpellAbilityId;
			}
		}
	}

	if (GetPlayerControllerById(pid) != 0 || GetPlayerSlotStateById(pid) != 1)
	{
		GetSpellAbilityIdCalled = FALSE;
		return SpellAbilityId;
	}

	if (!FullEventHookProcess)
	{
		GetSpellAbilityIdTime = Clock::now();
	}

	tmpProcessNewAction.CasterPlayerHandle = hPlayer;

	if (ProcessNewActionList.size() > 0)
	{
		const auto& action = ProcessNewActionList[ProcessNewActionList.size() - 1];
		if (action.CasterPlayerHandle == hPlayer
			&& action.SkillID == SpellAbilityId
			&& action.CasterUnitHandle == tmpProcessNewAction.CasterUnitHandle)
		{
			GetSpellAbilityIdCalled = FALSE;
			return SpellAbilityId;
		}
	}

	tmpProcessNewAction.EventID = GetTriggerEventId_real();
	tmpProcessNewAction.GetIssuedOrderId = GetIssuedOrderId_real();
	tmpProcessNewAction.IsGetSpellAbilityId = TRUE;
	tmpProcessNewAction.SkillID = SpellAbilityId;
	tmpProcessNewAction.TargetItemHandle = GetOrderTargetItem();
	tmpProcessNewAction.TargetUnitHandle = GetSpellTargetUnit();
	tmpProcessNewAction.GetSpellOrderTargetX = GetSpellTargetX().fl;
	tmpProcessNewAction.GetSpellOrderTargetY = GetSpellTargetY().fl;

	if (ProcessNewActionList.size() < 10000)
		ProcessNewActionList.push_back(tmpProcessNewAction);

	BotDetector(tmpProcessNewAction.SkillID, tmpProcessNewAction.EventID, tmpProcessNewAction.GetIssuedOrderId, tmpProcessNewAction.CasterUnitHandle, "GETSPELLABIL");
	GetSpellAbilityIdCalled = FALSE;
	return SpellAbilityId;
}

BOOL GetIssuedOrderIdCalled = FALSE;
int __cdecl GetIssuedOrderId_hooked()
{
	int issuereal = GetIssuedOrderId_real();
	if (GetIssuedOrderIdCalled)
		return issuereal;
	GetIssuedOrderIdCalled = TRUE;
	if (GameStarted && GameStartedReally && FogClickEnabled)
	{
		GetTriggerEventId_hooked();
	}
	GetIssuedOrderIdCalled = FALSE;
	return issuereal;
}

BOOL GetSpellAbilityUnitCalled = FALSE;
int __cdecl GetSpellAbilityUnit_hooked()
{
	int spellreal = GetSpellAbilityUnit_real();
	if (GetSpellAbilityUnitCalled)
		return spellreal;
	GetSpellAbilityUnitCalled = TRUE;
	if (GameStarted && GameStartedReally && FogClickEnabled)
	{
		if (GetSpellAbilityId_hooked() > 0)
		{
			GetSpellAbilityUnitCalled = FALSE;
			return spellreal;
		}
		GetTriggerEventId_hooked();
	}
	GetSpellAbilityUnitCalled = FALSE;
	return spellreal;
}

BOOL GetAttackerCalled = FALSE;
int __cdecl GetAttacker_hooked()
{
	int attackreal = GetAttacker_real();
	if (GetAttackerCalled)
		return attackreal;
	GetAttackerCalled = TRUE;
	if (GameStarted && GameStartedReally && FogClickEnabled)
	{
		GetTriggerEventId_hooked();
		//GetSpellAbilityId_hooked( );
	}
	GetAttackerCalled = FALSE;
	return attackreal;
}


long long LatestFogCheck = 0;
long long LatestFogCleanup = 0;
long long LatestClickCleanup = 0;

void UpdateFogHelper()
{
	if (llabs(CurGameTime - LatestFogCheck) > FOG_HISTORY_UPDATE_TIME)
	{
		LatestFogCheck = CurGameTime;

		for (int n = 0; n < MAX_PLAYERS; n++)
		{
			if (!IsReplayFound && !DetectLocalPlayer && GetLocalPlayerNumber() == n)
			{
				if (DebugModeEnabled && IsKeyPressed('V'))
				{
					ActionTime = CurGameTime;
					DisplayText("BAD PL 0!" + std::to_string(n), 10.0f);
				}
				continue;
			}

			if (Player(n) <= 0)
			{
				if (DebugModeEnabled && IsKeyPressed('V'))
				{
					ActionTime = CurGameTime;
					DisplayText("BAD PL 1!" + std::to_string(n), 10.0f);
				}
				continue;
			}

			if (GetPlayerControllerById(n) != 0 || GetPlayerSlotStateById(n) != 1)
			{
				/*if (DebugModeEnabled && IsKeyPressed('V'))
				{
					ActionTime = CurGameTime;
					DisplayText("BAD PL 2!" + std::to_string(n), 10.0f);
				}*/
				continue;
			}

			if (SafeUnitCount <= 1)
			{
				if (DebugModeEnabled && IsKeyPressed('V'))
				{
					ActionTime = CurGameTime;
					DisplayText("BAD U 4!" + std::to_string(n), 10.0f);
				}
				continue;
			}

			for (int i = 0; i < SafeUnitCount; i++)
			{
				int CurrentUnit = SafeUnitArray[i];

				if (!IsNotBadUnit(CurrentUnit))
					continue;
				if (!DetectRightClickOnlyHeroes || IsHero(CurrentUnit))
				{
					float unitx, unity, unitz;

					GetUnitLocation3D(CurrentUnit, &unitx, &unity, &unitz);

					BOOL FoundUnit = FALSE;
					for (auto& UnitFogHelper : FogHelperList)
					{
						if (CurrentUnit == UnitFogHelper.UnitAddr)
						{
							FoundUnit = TRUE;
							if (llabs(CurGameTime - UnitFogHelper.LatestTime[n]) > 300)
							{
								UnitFogHelper.LatestTime[n] = CurGameTime;

								if (DebugModeEnabled && IsKeyPressed('V'))
								{
									ActionTime = CurGameTime;
									DisplayText("UPDATE FOG STATE!PL:" + std::to_string(n) + ".UNIT:" + std::to_string(i), 10.0f);
								}

								if (DetectQuality == 3)
								{
									// Если невидимый или в тумане то записать в [1] TRUE, до этого скопировать значение [1] в [0]
									UnitFogHelper.FogState[n][0] = UnitFogHelper.FogState[n][1];
									UnitFogHelper.FogState[n][1] = IsFoggedToPlayerMy(&unitx, &unity, Player(n)) || !IsUnitVisibleToPlayer((unsigned char*)CurrentUnit, (unsigned char*)GetPlayerByNumber(n));
								}
								else
								{
									UnitFogHelper.FogState[n][0] = UnitFogHelper.FogState[n][1];
									UnitFogHelper.FogState[n][1] = UnitFogHelper.FogState[n][2];
									UnitFogHelper.FogState[n][2] = IsFoggedToPlayerMy(&unitx, &unity, Player(n)) || !IsUnitVisibleToPlayer((unsigned char*)CurrentUnit, (unsigned char*)GetPlayerByNumber(n));
								}
							}
						}
					}
					if (!FoundUnit)
					{
						if (DebugModeEnabled && IsKeyPressed('V'))
						{
							ActionTime = CurGameTime;
							DisplayText("NEW FOG STATE! PL:" + std::to_string(n) + ".UNIT:" + std::to_string(i), 10.0f);
						}
						FogHelper tmpghelp = FogHelper();
						// Initial state is visible
						for (int z = 0; z < MAX_PLAYERS; z++)
						{
							tmpghelp.FogState[z][0] = FALSE;
							tmpghelp.FogState[z][1] = FALSE;
							tmpghelp.FogState[z][2] = FALSE;
						}
						tmpghelp.UnitAddr = CurrentUnit;

						for (int z = 0; z < MAX_PLAYERS; z++)
						{
							if (Player(z) <= 0)
							{
								tmpghelp.LatestTime[z] = 0;
								continue;
							}

							if (GetPlayerControllerById(z) != 0 || GetPlayerSlotStateById(z) != 1)
							{
								tmpghelp.LatestTime[z] = 0;
								continue;
							}

							tmpghelp.LatestTime[z] = CurGameTime + (rand() % 150);
						}

						if (FogHelperList.size() >= MAX_FOG_HISTORY_ARRAY)
						{
							FogHelperList.erase(FogHelperList.begin());
						}

						FogHelperList.push_back(tmpghelp);
					}
				}
			}
		}
	}

	// FogState cleanup
	if (llabs(CurGameTime - LatestFogCleanup) > FOG_HISTORY_CLEANUP_TIME)
	{
		LatestFogCleanup = CurGameTime;
		std::vector<FogHelper> newFogHelper;
		for (auto& UnitFogHelper : FogHelperList)
		{
			for (int n = 0; n < MAX_PLAYERS; n++)
			{
				if (UnitFogHelper.LatestTime[n] > 0 && llabs(CurGameTime - UnitFogHelper.LatestTime[n]) < 5000)
				{
					newFogHelper.push_back(UnitFogHelper);
					break;
				}
			}
		}
		FogHelperList = newFogHelper;
	}
}


//FogHelperListTemp.clear( );

int PlayerSelectedItems[MAX_PLAYERS];

// Units initially visibled

// 1 is new visible unit
int LatestVisibledUnits[MAX_PLAYERS];
// 2 is any visible unit
int LatestVisibledUnits2[MAX_PLAYERS];

long long LatestVisibleUnitTime = 0;

int PossibleStrike[MAX_PLAYERS] = { 0 };

int LatestFoggedUnit[MAX_PLAYERS];

void SearchPlayersFogSelect()
{
	FillItemCountAndItemArray();
	FillUnitCountAndUnitArray();

	if (DetectQuality >= 3)
		UpdateFogHelper();

	for (int i = 0; i < MAX_PLAYERS; i++)
	{
		if (!IsReplayFound && !DetectLocalPlayer && GetLocalPlayerNumber() == i)
		{
			if (DebugModeEnabled && IsKeyPressed('C'))
			{
				ActionTime = CurGameTime;
				DisplayText("BAD001!" + std::to_string(i), 10.0f);
			}
			continue;
		}

		int hCurrentPlayer = Player(i);
		if (hCurrentPlayer <= 0)
		{
			if (DebugModeEnabled && IsKeyPressed('C'))
			{
				ActionTime = CurGameTime;
				DisplayText("BAD002!" + std::to_string(i), 10.0f);
			}
			continue;
		}

		if (GetPlayerControllerById(i) != 0 || GetPlayerSlotStateById(i) != 1)
		{
			if (DebugModeEnabled && IsKeyPressed('C'))
			{
				ActionTime = CurGameTime;
				DisplayText("BAD003!" + std::to_string(i), 10.0f);
			}
			continue;
		}

		int selecteditem = GetSelectedItem(i);

		if (selecteditem)
		{
			float itemx, itemy, itemz;

			GetItemLocation3D(selecteditem, &itemx, &itemy, &itemz);

			if (IsFoggedToPlayerMy(&itemx, &itemy, hCurrentPlayer))
			{
				if (PlayerSelectedItems[i] != selecteditem)
				{
					if (MinimapPingFogClick && IsReplayMode)
					{
						unsigned int PlayerColorInt = GetPlayerColorUINT(i);
						PingMinimapMy(&itemx, &itemy, &pingduration, PlayerColorInt & 0x00FF0000, PlayerColorInt & 0x0000FF00, PlayerColorInt & 0x000000FF, false);
					}
					sprintf_s(PrintBuffer, 2048, "|c00EF4000[FogCW v18.4]|r: Player %s%s|r select invisibled [item] |c004B4B4B%s|r [%s]",
						GetPlayerColorString(i),
						GetPlayerName(i, 0),
						GetObjectName(selecteditem),
						"POSSIBLE");

					PlayerSelectedItems[i] = selecteditem;
					DisplayText(PrintBuffer, 14.4f);
					SendPause();
				}
			}
			else
			{
				PlayerSelectedItems[i] = selecteditem;
			}
		}

		int selectedunit = GetSelectedUnit(i);

		if (DebugModeEnabled && IsKeyPressed('X'))
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

					ActionTime = CurGameTime;
					char debug[256];
					sprintf_s(debug, "N:[%s]=%i I=%X PL=%X PD=%X U1:%X U2:%X US:%X [%i=%i] UD:%X SC:%f SD:%X/%X\n", GetPlayerName(i, 0), i, selecteditem, plr, PlayerData1, unit1
						, unit2, selectedunit, unitcount, unitcount2, unitData, scale, somedata, somedata2);
					DisplayText(debug, 20.0f);
				}
			}
		}

		// Если юнит в предудущий раз был видимым - пропустить
		if (selectedunit == LatestVisibledUnits2[i])
		{
			if (DebugModeEnabled && IsKeyPressed('Z'))
			{
				ActionTime = CurGameTime;
				DisplayText("BAD1!" + std::to_string(i), 10.0f);
			}
			continue;
		}

		// Если это последний новый невидимый юнит то пропустить.
		if (selectedunit == LatestVisibledUnits[i])
		{
			if (DebugModeEnabled && IsKeyPressed('Z'))
			{
				ActionTime = CurGameTime;
				DisplayText("BAD2!" + std::to_string(i), 10.0f);
			}
			continue;
		}

		LatestVisibledUnits2[i] = -1;
		if (selectedunit <= 0)
		{
			if (DebugModeEnabled && IsKeyPressed('Z'))
			{
				ActionTime = CurGameTime;
				DisplayText("BAD3!" + std::to_string(i), 10.0f);
			}
			continue;
		}

		if (!IsNotBadUnit(selectedunit))
		{
			if (DebugModeEnabled && IsKeyPressed('Z'))
			{
				ActionTime = CurGameTime;
				DisplayText("BAD4!" + std::to_string(i), 10.0f);
			}
			continue;
		}
		LatestVisibledUnits[i] = -1;

		int OwnedPlayerSlot = GetUnitOwnerSlot(selectedunit);
		if (OwnedPlayerSlot < 0 || OwnedPlayerSlot >= TOTAL_MAX_PLAYERS_ARRAY)
		{
			if (DebugModeEnabled && IsKeyPressed('Z'))
			{
				ActionTime = CurGameTime;
				DisplayText("BAD5!" + std::to_string(i), 10.0f);
			}
			continue;
		}

		for (auto& UnitClick : UnitClickList)
		{
			if (UnitClick.PlayerNum == i)
			{
				if (UnitClick.SelectCount >= 0 && UnitClick.LatestTime != 0 && IsNotBadUnit(UnitClick.UnitAddr))
				{
					if (llabs(CurGameTime - UnitClick.LatestTime) > 200 * DetectQuality)
					{
						BOOL possible = UnitClick.initialVisibled || selectedunit == LatestVisibledUnits[i];

						sprintf_s(PrintBuffer, 2048, "|c00EF4000[FogCW v18.4]|r: Player %s%s|r select invisibled %s%s|r [%s]\0",
							GetPlayerColorString(i),
							GetPlayerName(i, 0),
							GetPlayerColorString(OwnedPlayerSlot),
							GetObjectName(UnitClick.UnitAddr),
							possible ? "FALSE" : "POSSIBLE");

						ActionTime = UnitClick.LatestTime;
						if (possible && !DisplayFalse)
						{
							int seconds = (int)(ActionTime / 1000) % 60;
							int minutes = (int)((ActionTime / (1000 * 60)) % 60);
							int hours = (int)((ActionTime / (1000 * 60 * 60)) % 24);

							if (LoggingType != -1)
							{
								char timeBuff[64];
								sprintf_s(timeBuff, "[%.2d:%.2d:%.2d] : ", hours, minutes, seconds);
								std::string outLineStr = (timeBuff + std::string(PrintBuffer));
								WatcherLog((outLineStr + "\n").c_str());
							}
						}
						else
						{
							DisplayText(PrintBuffer, 14.4f);
							SendPause();
						}

						UnitClick.SelectCount = -1;
						if (LatestVisibledUnits[i] == -1)
							LatestVisibledUnits[i] = UnitClick.UnitAddr;
					}
				}
			}
		}

		int hOwnerPlayer = Player(OwnedPlayerSlot);
		if (hOwnerPlayer <= 0)
		{
			if (DebugModeEnabled && IsKeyPressed('Z'))
			{
				ActionTime = CurGameTime;
				DisplayText("BAD6!" + std::to_string(i), 10.0f);
			}
			continue;
		}

		if (OwnedPlayerSlot != i && (OwnedPlayerSlot >= MAX_PLAYERS || IsPlayerEnemyById(i, OwnedPlayerSlot)))
		{
			if (DebugModeEnabled && IsKeyPressed('Z'))
			{
				ActionTime = CurGameTime;
				DisplayText("ENEMY!" + std::to_string(i), 10.0f);
			}

			float unitx = 0.0f, unity = 0.0f, unitz = 0.0f;
			GetUnitLocation3D(selectedunit, &unitx, &unity, &unitz);


			if (IsFoggedToPlayerMy(&unitx, &unity, hCurrentPlayer) || !IsUnitVisibleToPlayer((unsigned char*)selectedunit, (unsigned char*)GetPlayerByNumber(i)))
			{
				if (DebugModeEnabled && IsKeyPressed('Z'))
				{
					ActionTime = CurGameTime;
					DisplayText("FOG!" + std::to_string(i), 10.0f);
				}
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

				if (!IsFoggedInHistory(i, selectedunit))
				{
					if (DebugModeEnabled && IsKeyPressed('Z'))
					{
						ActionTime = CurGameTime;
						DisplayText("BAD FOG!" + std::to_string(i), 10.0f);
					}
					needcontinue = TRUE;
				}

				if (needcontinue)
				{
					continue;
				}

				if (!found)
				{
					if (DebugModeEnabled && IsKeyPressed('Z'))
					{
						ActionTime = CurGameTime;
						DisplayText("NEW FOG!", 10.0f);
					}
					UnitSelectedStruct tmpus;
					tmpus.PlayerNum = i;
					tmpus.UnitAddr = selectedunit;
					tmpus.LatestTime = CurGameTime;
					tmpus.SelectCount = 0;
					tmpus.initialVisibled = FALSE;

					if (UnitClickList.size() >= MAX_CLICK_HISTORY_ARRAY)
					{
						UnitClickList.erase(UnitClickList.begin());
					}

					UnitClickList.push_back(tmpus);


					tmpunitselected = &UnitClickList[UnitClickList.size() - 1];
				}

				if (tmpunitselected && tmpunitselected->SelectCount >= 0)
				{
					if (tmpunitselected->SelectCount < 2 && (!ReplayMoreSens || !IsReplayFound))
					{
						if (llabs(CurGameTime - tmpunitselected->LatestTime) > 10 * DetectQuality)
						{
							tmpunitselected->SelectCount++;
							tmpunitselected->LatestTime = CurGameTime;
						}
					}
					else
					{
						if ((ReplayMoreSens && IsReplayFound) || llabs(CurGameTime - tmpunitselected->LatestTime) > 50 * DetectQuality)
						{
							BOOL possible = tmpunitselected->initialVisibled || selectedunit == LatestVisibledUnits[i];

							if (!IsFoggedToPlayerMy(&unitx, &unity, hCurrentPlayer))
							{
								if (MinimapPingFogClick && IsReplayMode)
								{
									unsigned int PlayerColorInt = GetPlayerColorUINT(i);
									PingMinimapMy(&unitx, &unity, &pingduration, PlayerColorInt & 0x00FF0000, PlayerColorInt & 0x0000FF00, PlayerColorInt & 0x000000FF, false);
								}

								sprintf_s(PrintBuffer, 2048, "|c00EF4000[FogCW v18.4]|r: Player %s%s|r select invisibled %s%s|r [%s]",
									GetPlayerColorString(i),
									GetPlayerName(i, 0),
									GetPlayerColorString(OwnedPlayerSlot),
									GetObjectName(selectedunit),
									possible ? "FALSE" : "DETECTED");

								if (possible)
								{
									PossibleStrike[i]++;
								}
								else
								{
									if (PrintDetectedUnitOneTime && LatestFoggedUnit[i] == selectedunit)
									{
										possible = true;
									}
									LatestFoggedUnit[i] = selectedunit;
									PossibleStrike[i] = 0;
								}

								if (DebugModeEnabled)
								{
									char detectUnitAndPlayer[64];
									sprintf_s(detectUnitAndPlayer, "%X=%X", GetPlayerByNumber(i), selectedunit);
									DisplayText(detectUnitAndPlayer, 14.4f);
								}
							}
							else
							{
								if (MinimapPingFogClick && IsReplayMode)
								{
									unsigned int PlayerColorInt = GetPlayerColorUINT(i);
									PingMinimapMy(&unitx, &unity, &pingduration, PlayerColorInt & 0x00FF0000, PlayerColorInt & 0x0000FF00, PlayerColorInt & 0x000000FF, false);
								}

								sprintf_s(PrintBuffer, 2048, "|c00EF4000[FogCW v18.4]|r: Player %s%s|r select fogged %s%s|r [%s]\0",
									GetPlayerColorString(i),
									GetPlayerName(i, 0),
									GetPlayerColorString(OwnedPlayerSlot),
									GetObjectName(selectedunit),
									possible ? "FALSE" : "DETECTED");


								if (possible)
								{
									PossibleStrike[i]++;
								}
								else
								{
									if (PrintDetectedUnitOneTime && LatestFoggedUnit[i] == selectedunit)
									{
										possible = true;
									}
									LatestFoggedUnit[i] = selectedunit;
									PossibleStrike[i] = 0;
								}

								if (DebugModeEnabled)
								{
									char detectUnitAndPlayer[64];
									sprintf_s(detectUnitAndPlayer, "%X=%X", GetPlayerByNumber(i), selectedunit);
									DisplayText(detectUnitAndPlayer, 14.4f);
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

									if (LoggingType != -1)
									{
										char timeBuff[64];
										sprintf_s(timeBuff, "[%.2d:%.2d:%.2d] : ", hours, minutes, seconds);

										std::string outLineStr = (timeBuff + std::string(PrintBuffer));
										WatcherLog((outLineStr + "\n").c_str());
									}
								}
								else
								{
									DisplayText(PrintBuffer, 14.4f);
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

					UnitClickList.push_back(tmpus);

					if (UnitClickList.size() >= MAX_CLICK_HISTORY_ARRAY)
					{
						UnitClickList.erase(UnitClickList.begin());
					}

					LatestVisibleUnitTime = CurGameTime;
					LatestVisibledUnits[i] = selectedunit;
				}
			}
		}
		else
		{
			if (DebugModeEnabled && IsKeyPressed('Z'))
			{
				if (i == OwnedPlayerSlot)
				{
					ActionTime = CurGameTime;
					DisplayText("BAD7!" + std::to_string(i), 10.0f);
				}
				else if (!IsPlayerEnemyById(i, OwnedPlayerSlot))
				{
					ActionTime = CurGameTime;
					DisplayText("BAD8!" + std::to_string(i), 10.0f);
				}
				else
				{
					ActionTime = CurGameTime;
					DisplayText("BAD9!" + std::to_string(i), 10.0f);
				}
			}
		}
	}

	// FogState cleanup
	if (llabs(CurGameTime - LatestClickCleanup) > FOG_CLICK_CLEANUP_TIME)
	{
		LatestClickCleanup = CurGameTime;
		std::vector<UnitSelectedStruct> newUnitClickList;
		for (auto& UnitClick : UnitClickList)
		{
			if (llabs(CurGameTime - UnitClick.LatestTime) < 15000)
			{
				newUnitClickList.push_back(UnitClick);
			}
		}
		UnitClickList = newUnitClickList;
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
	fogwatcherconf.WriteBool("FogClickWatcher", "GetAttacker", FALSE);
	fogwatcherconf.WriteBool("FogClickWatcher", "GetSpellAbilityUnit", TRUE);
	fogwatcherconf.WriteInt("FogClickWatcher", "UnitDetectionMethod", 1);
	fogwatcherconf.WriteInt("FogClickWatcher", "MeepoPoofID", 0x41304E38);
	fogwatcherconf.WriteInt("FogClickWatcher", "TechiesDetonateId", 0xD024C);
	fogwatcherconf.WriteBool("FogClickWatcher", "DetectRightClickOnlyHeroes", FALSE);
	fogwatcherconf.WriteBool("FogClickWatcher", "MinimapPingFogClick", FALSE);
	fogwatcherconf.WriteInt("FogClickWatcher", "DetectQuality", 3);
	fogwatcherconf.WriteBool("FogClickWatcher", "ReplayMoreSens", TRUE);
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
	fogwatcherconf.WriteBool("FogClickWatcher", "PrintDetectedUnitOneTime", TRUE);
	fogwatcherconf.WriteBool("FogClickWatcher", "FullEventHookProcess", TRUE);
}


typedef int(__cdecl* pGetEventPlayerChatString)();
pGetEventPlayerChatString GetEventPlayerChatString;

typedef int(__cdecl* pGetEventPlayerChatStringMatched)();
pGetEventPlayerChatStringMatched GetEventPlayerChatStringMatched;

DWORD* GlobalGameTlsIndex;
void* GameTlsDataGet(uint32_t index) {
	DWORD tlsIndex = *GlobalGameTlsIndex;
	void* tlsValue = TlsGetValue(tlsIndex);
	if (tlsValue)
		return aero::offset_element_get<void*>(tlsValue, index * 4);
	else
		return NULL;
}

DWORD GameTlsIndexGet() {
	return *GlobalGameTlsIndex;
}

war3::JassThreadLocal* GetJassThreadLocal() {
	return (war3::JassThreadLocal*)GameTlsDataGet(5);
}

char* ReadJassSID(int JSID)
{
	war3::CGameWar3* gameObj = (war3::CGameWar3*)(GetGlobalWar3Data());
	if (gameObj)
	{
		uint32_t offset = gameObj->jassStringId;
		war3::JassThreadLocal* jtl = GetJassThreadLocal();
		if (!jtl)
			return NULL;
		void* data = jtl->stringArr[offset];
		war3::RCString* str = aero::generic_this_call<war3::RCString*>(GameDll + 0x459640, data, JSID);
		if (str && str->stringRep)
			return str->stringRep->text;
	}
	return NULL;
}


typedef void(WINAPI* PSleep)(unsigned long ms);
typedef int(WINAPI* PFreeLibrary)(HMODULE hModule);
typedef VOID(WINAPI* PExitThread)(unsigned long dwExitCode);
typedef unsigned int (WINAPI* PTHREADPROC)(LPVOID lParam);

typedef struct _DLLUNLOADINFO {
	PFreeLibrary	m_fpFreeLibrary;
	PSleep m_fpSleep;
	PExitThread		m_fpExitThread;
	void* m_hFreeModule;
}DLLUNLOADINFO, * PDLLUNLOADINFO;

unsigned long WINAPI DllUnloadThreadProc(LPVOID lParam)
{
	PDLLUNLOADINFO pDllUnloadInfo = (PDLLUNLOADINFO)lParam;
	(pDllUnloadInfo->m_fpSleep)(100);
	(pDllUnloadInfo->m_fpFreeLibrary)((HMODULE)pDllUnloadInfo->m_hFreeModule);
	pDllUnloadInfo->m_fpExitThread(0);
	return 0;
}

bool DLL_SELF_UNLOADED = false;

void* __stdcall DllSelfUnloading(void* hModule)
{
	PVOID pMemory = NULL;
	pMemory = VirtualAlloc(NULL, 0x1000, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
	if (pMemory != NULL)
	{
		SIZE_T rbytes = 0;
		if (ReadProcessMemory(GetCurrentProcess(), DllUnloadThreadProc, pMemory, 0x500, &rbytes) && rbytes > 0)
		{
			((PDLLUNLOADINFO)(((unsigned char*)pMemory) + 0x500))->m_fpFreeLibrary = FreeLibrary;
			((PDLLUNLOADINFO)(((unsigned char*)pMemory) + 0x500))->m_fpSleep = Sleep;
			((PDLLUNLOADINFO)(((unsigned char*)pMemory) + 0x500))->m_fpExitThread = ExitThread;
			((PDLLUNLOADINFO)(((unsigned char*)pMemory) + 0x500))->m_hFreeModule = hModule;

			DLL_SELF_UNLOADED = true;

			CloseHandle(CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)pMemory,
				(PVOID)(((unsigned char*)pMemory) + 0x500), 0, 0));
		}
	}
	return 0;
}


long long LastCmdGameTime = 0;

void ProcessCmdString(std::string str)
{
	if (str == "-help" || str == "-foghelp")
	{
		DisplayText("|c00FF2000[FogCW v18.4 by UnrealKaraulov]", 14.4f);

		DisplayText("|c00FFFFFF-fogtoggle_______|c00AAFFAAEnable/Disable FogClickWatcher", 14.4f);
		DisplayText("|c00FFFFFF-fogunload______|c00FFAAAAForce unload FogClickWatcher|c00FF0000[!!!CAN CRASH GAME!!!]|r", 14.4f);
		DisplayText("|c00FFFFFF-fogoption_______|c00AAAAFFChange FogClickWatcher settings", 14.4f);
		DisplayText("|c00FFFFFF-fogshowcfg_____|c00AAAAFFShow FogClickWatcher settings", 14.4f);
	}
	else if (str == "-fogtoggle")
	{
		FogClickEnabled = !FogClickEnabled;

		if (FogClickEnabled)
		{
			DisplayText("|c00FF2000[FogCW v18.4 by UnrealKaraulov] : |c0020FF20ENABLED", 14.4f);
		}
		else
		{
			DisplayText("|c00FF2000[FogCW v18.4 by UnrealKaraulov] : |c00FF2020DISABLED", 14.4f);
		}
	}
	else if (str == "-fogunload")
	{
		DisplayText("|c00FF2000[FogCW v18.4 by UnrealKaraulov] : |c00FF2020UNLOADED FROM MEMORY", 14.4f);
		DllSelfUnloading(MainModule);
	}
	else if (str.starts_with("-fogoption"))
	{
		bool showhelp = str == "-fogoption";
		int helpindex = 1;

		if (str.starts_with("-fogoption "))
		{
			str.erase(0, strlen("-fogoption "));
			if (!showhelp)
			{
				showhelp = str.length() == 0;
				if (!showhelp)
				{
					showhelp = str == "1" || str == "2";
					if (str == "2")
					{
						helpindex = 2;
					}
					if (str == "3")
					{
						helpindex = 3;
					}
				}
			}
		}

		if (showhelp)
		{
			DisplayText("|c00FF2000[FogCW v18.4] : |c0020FF20OPTION LIST : |c00AA2000 -fogoption 1/2/3", 14.4f);
			if (helpindex == 1)
			{
				DisplayText("|c00506000[LoggingType]________________: |c0020FF20[ 0 / 1 / -1 ]", 14.4f);
				DisplayText("|c00506000[LocalPlayerEnable]__________: |c0020FF20[ 0 / 1 ]", 14.4f);
				DisplayText("|c00506000[UnitDetectionMethod]________: |c0020FF20[ 1 - 7 ]", 14.4f);
				DisplayText("|c00506000[MeepoPoofID]________________: |c0020FF20[ 0 / POOFID ]", 14.4f);
				DisplayText("|c00506000[DetectRightClickOnlyHeroes]_: |c0020FF20[ 0 / 1 ]", 14.4f);
				DisplayText("|c00506000[DetectQuality]_______________: |c0020FF20[ 1 - 4 ]", 14.4f);
				DisplayText("|c00506000[ReplayMoreSens]_____________: |c0020FF20[ 0 / 1 ]", 14.4f);
				DisplayText("|c00506000[PrintOrderName]_____________: |c0020FF20[ 0 / 1 ]", 14.4f);
				DisplayText("|c00506000[SkipIllusions]________________: |c0020FF20[ 0 / 1 ]", 14.4f);
			}
			else if (helpindex == 2)
			{
				DisplayText("|c00506000[DetectImpossibleClicks]______: |c0020FF20[ 0 / 1 ]", 14.4f);
				DisplayText("|c00506000[DetectItemDestroyer]_________: |c0020FF20[ 0 / 1 ]", 14.4f);
				DisplayText("|c00506000[DetectOwnItems]_____________: |c0020FF20[ 0 / 1 ]", 14.4f);
				DisplayText("|c00506000[DetectPointClicks]___________: |c0020FF20[ 0 / 1 ]", 14.4f);
				DisplayText("|c00506000[TechiesDetonateId]___________: |c0020FF20[ 0 / DETONID ]", 14.4f);
				DisplayText("|c00506000[DebugLog]___________________: |c0020FF20[ 0 / 1 ]", 14.4f);
				DisplayText("|c00506000[Debug]_______________________: |c0020FF20[ 0 / 1 ]", 14.4f);
				DisplayText("|c00506000[PrintDetectedUnitOneTime]___: |c0020FF20[ 0 / 1 ]", 14.4f);
				DisplayText("|c00506000[FullEventHookProcess]_______: |c0020FF20[ 0 / 1 ]", 14.4f);
			}
			else
			{

			}
		}
	}
}

void ProcessJassString(int sid)
{
	if (GetGameTime() != LastCmdGameTime)
	{
		LastCmdGameTime = GetGameTime();
		char* realStr = ReadJassSID(sid);
		if (realStr && realStr[0] != '\0')
			ProcessCmdString(realStr);
	}
}

BOOL GetEventPlayerChatStringCalled = FALSE;
int __cdecl GetEventPlayerChatString_hooked()
{
	int ret_string = GetEventPlayerChatString();
	if (GetEventPlayerChatStringCalled)
		return ret_string;
	GetEventPlayerChatStringCalled = TRUE;
	if (GameStarted && GameStartedReally)
	{
		ProcessJassString(ret_string);
	}
	GetEventPlayerChatStringCalled = FALSE;
	return ret_string;
}

BOOL GetEventPlayerChatStringMatchedCalled = FALSE;
int __cdecl GetEventPlayerChatStringMatched_hooked()
{
	int ret_string = GetEventPlayerChatStringMatched();
	if (GetEventPlayerChatStringMatchedCalled)
		return ret_string;
	GetEventPlayerChatStringMatchedCalled = TRUE;
	if (GameStarted && GameStartedReally)
	{
		ProcessJassString(ret_string);
	}
	GetEventPlayerChatStringMatchedCalled = FALSE;
	return ret_string;
}

void RecoveryFuncBack()
{
	ICCUP_DOTA_SUPPORT = TRUE;
	RevertJassNativeHook((int)ClearTextMessages_real, (int)&ClearTextMessages_hooked);
	RevertJassNativeHook((int)GetTriggerEventId_real, (int)&GetTriggerEventId_hooked);
	RevertJassNativeHook((int)GetSpellAbilityId_real, (int)&GetSpellAbilityId_hooked);
	RevertJassNativeHook((int)GetIssuedOrderId_real, (int)&GetIssuedOrderId_hooked);
	RevertJassNativeHook((int)GetAttacker_real, (int)&GetAttacker_hooked);
	RevertJassNativeHook((int)GetSpellAbilityUnit_real, (int)&GetSpellAbilityUnit_hooked);
	RevertJassNativeHook((int)GetEventPlayerChatString, (int)&GetEventPlayerChatString_hooked);
	RevertJassNativeHook((int)GetEventPlayerChatStringMatched, (int)&GetEventPlayerChatStringMatched_hooked);
}

void LoadFogClickWatcherConfig()
{
	bool newcfg = false;
	if (!FileExists(detectorConfigPath))
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


	ICCUP_DOTA_SUPPORT = fogwatcherconf.ReadBool("FogClickWatcher", "ICCUP_DOTA_SUPPORT", TRUE);

	bool failedhook = false;

	{
		int hookAddr = CreateJassNativeHook((int)ClearTextMessages_real, (int)&ClearTextMessages_hooked);
		WatcherLog("Global:ClearTextMessages hook for Replays...\n");
		if (hookAddr <= 0)
		{
			failedhook = true;
			WatcherLog("Config:ClearTextMessages->%s\n", "HOOK FAILED");
		}
	}
	{
		int hookAddr = CreateJassNativeHook((int)GetEventPlayerChatString, (int)&GetEventPlayerChatString_hooked);
		WatcherLog("Global:GetEventPlayerChatString hook for text commands...\n");
		if (hookAddr <= 0)
		{
			failedhook = true;
			WatcherLog("Config:GetEventPlayerChatString->%s\n", "HOOK FAILED");
		}
		hookAddr = CreateJassNativeHook((int)GetEventPlayerChatStringMatched, (int)&GetEventPlayerChatStringMatched_hooked);
		WatcherLog("Global:GetEventPlayerChatStringMatched hook for text commands...\n");
		if (hookAddr <= 0)
		{
			failedhook = true;
			WatcherLog("Config:GetEventPlayerChatStringMatched->%s\n", "HOOK FAILED");
		}
	}
	if (fogwatcherconf.ReadBool("FogClickWatcher", "GetTriggerEventId", TRUE))
	{
		int hookAddr = CreateJassNativeHook((int)GetTriggerEventId_real, (int)&GetTriggerEventId_hooked);
		WatcherLog("Config:GetTriggerEventId->%s\n", "TRUE");
		if (hookAddr <= 0)
		{
			failedhook = true;
			WatcherLog("Config:GetTriggerEventId->%s\n", "HOOK FAILED");
		}
	}
	else
	{
		WatcherLog("Config:GetTriggerEventId->%s\n", "FALSE");
	}

	if (fogwatcherconf.ReadBool("FogClickWatcher", "GetSpellAbilityId", TRUE))
	{
		int hookAddr = CreateJassNativeHook((int)GetSpellAbilityId_real, (int)&GetSpellAbilityId_hooked);
		WatcherLog("Config:GetSpellAbilityId->%s\n", "TRUE");
		if (hookAddr <= 0)
		{
			failedhook = true;
			WatcherLog("Config:GetSpellAbilityId->%s\n", "HOOK FAILED");
		}
	}
	else
	{
		WatcherLog("Config:GetSpellAbilityId->%s\n", "FALSE");
	}


	if (fogwatcherconf.ReadBool("FogClickWatcher", "GetIssuedOrderId", TRUE))
	{
		int hookAddr = CreateJassNativeHook((int)GetIssuedOrderId_real, (int)&GetIssuedOrderId_hooked);
		WatcherLog("Config:GetIssuedOrderId->%s\n", "TRUE");
		if (hookAddr <= 0)
		{
			failedhook = true;
			WatcherLog("Config:GetIssuedOrderId->%s\n", "HOOK FAILED");
		}
	}
	else
	{
		WatcherLog("Config:GetIssuedOrderId->%s\n", "FALSE");
	}

	if (fogwatcherconf.ReadBool("FogClickWatcher", "GetAttacker", FALSE))
	{
		int hookAddr = CreateJassNativeHook((int)GetAttacker_real, (int)&GetAttacker_hooked);
		WatcherLog("Config:GetAttacker->%s\n", "TRUE");
		if (hookAddr <= 0)
		{
			failedhook = true;
			WatcherLog("Config:GetAttacker->%s\n", "HOOK FAILED");
		}
	}
	else
	{
		WatcherLog("Config:GetAttacker->%s\n", "FALSE");
	}

	if (fogwatcherconf.ReadBool("FogClickWatcher", "GetSpellAbilityUnit", TRUE))
	{
		int hookAddr = CreateJassNativeHook((int)GetSpellAbilityUnit_real, (int)&GetSpellAbilityUnit_hooked);
		WatcherLog("Config:GetSpellAbilityUnit->%s\n", "TRUE");
		if (hookAddr <= 0)
		{
			failedhook = true;
			WatcherLog("Config:GetSpellAbilityUnit->%s\n", "HOOK FAILED");
		}
	}
	else
	{
		WatcherLog("Config:GetSpellAbilityUnit->%s\n", "FALSE");
	}

	if (failedhook && !ICCUP_DOTA_SUPPORT)
	{
		WatcherLog("Possible is need to enable ICCUP_DOTA_SUPPORT option\n");
	}

	DetectLocalPlayer = fogwatcherconf.ReadBool("FogClickWatcher", "LocalPlayerEnable", TRUE);
	UnitDetectionMethod = fogwatcherconf.ReadInt("FogClickWatcher", "UnitDetectionMethod", 3);
	MeepoPoofID = fogwatcherconf.ReadInt("FogClickWatcher", "MeepoPoofID", 0);
	DetectRightClickOnlyHeroes = fogwatcherconf.ReadBool("FogClickWatcher", "DetectRightClickOnlyHeroes", FALSE);
	MinimapPingFogClick = fogwatcherconf.ReadBool("FogClickWatcher", "MinimapPingFogClick", FALSE);
	DetectQuality = fogwatcherconf.ReadInt("FogClickWatcher", "DetectQuality", 3);
	ReplayMoreSens = fogwatcherconf.ReadBool("FogClickWatcher", "ReplayMoreSens", TRUE);
	PrintOrderName = fogwatcherconf.ReadBool("FogClickWatcher", "PrintOrderName", FALSE);
	SkipIllusions = fogwatcherconf.ReadBool("FogClickWatcher", "SkipIllusions", FALSE);
	DetectImpossibleClicks = fogwatcherconf.ReadBool("FogClickWatcher", "DetectImpossibleClicks", FALSE);
	DetectItemDestroyer = fogwatcherconf.ReadBool("FogClickWatcher", "DetectItemDestroyer", FALSE);
	DetectOwnItems = fogwatcherconf.ReadBool("FogClickWatcher", "DetectOwnItems", FALSE);
	DetectPointClicks = fogwatcherconf.ReadBool("FogClickWatcher", "DetectPointClicks", FALSE);
	TechiesDetonateId = fogwatcherconf.ReadInt("FogClickWatcher", "TechiesDetonateId", 0);
	DebugLog = fogwatcherconf.ReadBool("FogClickWatcher", "DebugLog", FALSE);
	DebugModeEnabled = fogwatcherconf.ReadBool("FogClickWatcher", "Debug", FALSE);
	DisplayFalse = fogwatcherconf.ReadBool("FogClickWatcher", "DisplayFalse", TRUE);
	ReplayPauseOnDetect = fogwatcherconf.ReadBool("FogClickWatcher", "ReplayPauseOnDetect", TRUE);
	PrintDetectedUnitOneTime = fogwatcherconf.ReadBool("FogClickWatcher", "PrintDetectedUnitOneTime", TRUE);
	FullEventHookProcess = fogwatcherconf.ReadBool("FogClickWatcher", "FullEventHookProcess", TRUE);

	WatcherLog("Config:LoggingType->%i\n", LoggingType);
	WatcherLog("Config:LocalPlayerEnable->%s\n", DetectLocalPlayer ? "TRUE" : "FALSE");
	WatcherLog("Config:UnitDetectionMethod->%i\n", UnitDetectionMethod);
	WatcherLog("Config:MeepoPoofID->%X\n", MeepoPoofID);
	WatcherLog("Config:DetectRightClickOnlyHeroes->%s\n", DetectRightClickOnlyHeroes ? "TRUE" : "FALSE");
	WatcherLog("Config:MinimapPingFogClick->%s[ONLY IN REPLAY]\n", MinimapPingFogClick ? "TRUE" : "FALSE");
	WatcherLog("Config:DetectQuality->%i\n", DetectQuality);
	WatcherLog("Config:ReplayMoreSens->%s\n", ReplayMoreSens ? "TRUE" : "FALSE");
	WatcherLog("Config:PrintOrderName->%s\n", PrintOrderName ? "TRUE" : "FALSE");
	WatcherLog("Config:SkipIllusions->%s\n", SkipIllusions ? "TRUE" : "FALSE");
	WatcherLog("Config:DetectImpossibleClicks->%s\n", DetectImpossibleClicks ? "TRUE" : "FALSE");
	WatcherLog("Config:DetectItemDestroyer->%s\n", DetectItemDestroyer ? "TRUE" : "FALSE");
	WatcherLog("Config:DetectOwnItems->%s\n", DetectOwnItems ? "TRUE" : "FALSE");
	WatcherLog("Config:DetectPointClicks->%s\n", DetectPointClicks ? "TRUE" : "FALSE");
	WatcherLog("Config:TechiesDetonateId->%i\n", TechiesDetonateId);
	WatcherLog("Config:DebugLog->%s\n", DebugLog ? "TRUE" : "FALSE");
	WatcherLog("Config:Debug->%s\n", DebugModeEnabled ? "TRUE" : "FALSE");
	WatcherLog("Config:iCCup DoTA support->%s\n", ICCUP_DOTA_SUPPORT ? "TRUE" : "FALSE");
	WatcherLog("Config:ReplayPauseOnDetect->%s\n", ReplayPauseOnDetect ? "TRUE" : "FALSE");
	WatcherLog("Config:PrintDetectedUnitOneTime->%s\n", PrintDetectedUnitOneTime ? "TRUE" : "FALSE");
	WatcherLog("Config:FullEventHookProcess->%s\n", FullEventHookProcess ? "TRUE" : "FALSE");
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
	GetSpellTargetX = (pGetSpellTargetX)(GameDll + 0x3C3580);
	GetSpellTargetY = (pGetSpellTargetY)(GameDll + 0x3C3670);
	GetHandleUnitAddress = (pGetHandleUnitAddress)(GameDll + 0x3BDCB0);
	GetHandleItemAddress = (pGetHandleUnitAddress)(GameDll + 0x3BEB50);
	IsUnitSelectedReal = (pIsUnitSelected)(GameDll + 0x421E20);
	pW3XGlobalClass = GameDll + 0xAB4F80;
	pPrintText2 = GameDll + 0x2F69A0;
	IsReplayMode = (pIsReplayMode)(GameDll + 0x53F160);
	WarcraftRealWNDProc = (pWarcraftRealWNDProc)(GameDll + 0x6C6AA0);
	UnitVtable = GameDll + 0x931934;
	ItemVtable = GameDll + 0x9320B4;
	IsPlayerEnemy_real = (pIsPlayerEnemy)(GameDll + 0x3C9580);
	PlayerReal = (pPlayer)(GameDll + 0x3BBB30);
	IsFoggedToPlayerReal = (pIsFoggedToPlayer)(GameDll + 0x3C9980);
	GetTriggerEventId_real = (pGetTriggerEventId)(GameDll + 0x3BB2C0);
	ClearTextMessages_real = (pClearTextMessages)(GameDll + 0x3B4E60);
	GetIssuedOrderId_real = (pGetIssuedOrderId)(GameDll + 0x3C2C80);
	GetAttacker_real = (pGetAttacker)(GameDll + 0x3C20F0);
	GetTriggerUnit = (pGetTriggerUnit)(GameDll + 0x3BB240);
	GetTriggerPlayer = (pGetTriggerPlayer)(GameDll + 0x3BB280);
	GetPlayerId = (pGetPlayerId)(GameDll + 0x3C9640);
	GetOrderTargetUnit = (pGetOrderTargetUnit)(GameDll + 0x3C3170);
	GetOrderTargetItem = (pGetOrderTargetItem)(GameDll + 0x3C3040);
	GetOrderPointX = (pGetOrderPointX)(GameDll + 0x3C2D00);
	GetOrderPointY = (pGetOrderPointY)(GameDll + 0x3C2D50);
	GameTimeOffset = GameDll + 0xAB7E98;
	GetPlayerController_real = (pGetPlayerController)(GameDll + 0x3C12B0);
	GetPlayerSlotState_real = (pGetPlayerSlotState)(GameDll + 0x3C12D0);
	pGlobalWar3Data = 0xAB65F4 + GameDll;
	PingMinimapEx = (pPingMinimapEx)(GameDll + 0x3B8660);
	GetSomeAddr = (pGetSomeAddr)(0x03FA30 + GameDll);
	MapFileName = (const char*)(GameDll + 0xAAE7CE);

	GetEventPlayerChatString = (pGetEventPlayerChatString)(GameDll + 0x3C20B0);
	GetEventPlayerChatStringMatched = (pGetEventPlayerChatStringMatched)(GameDll + 0x3C2080);

	GlobalGameTlsIndex = (DWORD*)(0xAB7BF4 + GameDll);
}

void GameWaiting()
{
	if (IsGame())
	{
		if (!GameStarted)
		{
			GameStartedReally = FALSE;
			GameStarted = TRUE;
			FogClickEnabled = TRUE;

			StartGameTime = CurTickCount;
			UnitClickList.clear();
			FogHelperList.clear();

			memset(PlayerSelectedItems, 0, sizeof(PlayerSelectedItems));
			memset(PlayerEventList, 0, sizeof(PlayerEventList));
			memset(player_cache, 0, sizeof(player_cache));
			memset(player_controller_cache, 0, sizeof(player_controller_cache));
			memset(player_enemy_cache, 0, sizeof(player_enemy_cache));
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
			GameStartedReally = FALSE;
			IsReplayFound = FALSE;
			GameStarted = FALSE;
			StartGameTime = CurTickCount;
			UnitClickList.clear();
			FogHelperList.clear();

			memset(PlayerSelectedItems, 0, sizeof(PlayerSelectedItems));
			memset(PlayerEventList, 0, sizeof(PlayerEventList));
			memset(player_cache, 0, sizeof(player_cache));
			memset(player_controller_cache, 0, sizeof(player_controller_cache));
			memset(player_enemy_cache, 0, sizeof(player_enemy_cache));

			LastEventTime = 0;

			latestcheck = 0;
			ActionTime = 0;
		}
	}
}

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
			WatcherLog("[%s]\n", ".......UnrealFogClickWatcher v18.4 for Warcraft 1.26a by UnrealKaraulov......");
			WatcherLog("Game.dll address is 0x%X.\n", (unsigned int)GameDll);
			WatcherLog("FogClick.dll address is 0x%X\n", (unsigned int)MainModule);

			WatcherLog("Watch fog clicks for Game id: %i. Map name: %s.\n", GameID++, MapFileName);

			sprintf_s(PrintBuffer, 2048, "%s", "|c00FF2000[FogCW v18.4 by UnrealKaraulov][1.26a]: Initialized.|r\0");
			ActionTime = 0;
			DisplayText(PrintBuffer, 14.4f);
			if (IsReplayMode())
			{
				IsReplayFound = TRUE;
				sprintf_s(PrintBuffer, 2048, "%s", "|c0010FF10[REPLAY MODE] |c00FF2000Info|r: |c00AAAAAADisabled ClearTextMessages funciton and increse DisplayText time!|r\0");
				DisplayText(PrintBuffer, 14.4f);
			}

			DefaultCircleScale = GetMiscDataFloat("SelectionCircle", "ScaleFactor", 0);
			if (DefaultCircleScale <= 1.0)
				DefaultCircleScale = 50.0f;
			//WatcherLog("CircleScale : %f.\n", DefaultCircleScale);
		}
	}
	else
	{
		if (GameStartedReally)
		{

		}
		GameStartedReally = FALSE;
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

	if (updated)
	{
		try
		{
			ProcessFogWatcher();
		}
		catch (...)
		{
			if (DebugLog)
				WatcherLog("CRASH: ProcessFogWatcher \n");
		}
		if (GameStarted)
		{
			CurGameTime = GetGameTime();

			if (llabs(CurGameTime - LastClickWatcher) >= FOG_CLICK_UPDATE_TIME && FogClickEnabled)
			{
				LastClickWatcher = CurGameTime;
				try
				{
					SearchPlayersFogSelect();
				}
				catch (...)
				{
					if (DebugLog)
						WatcherLog("CRASH: SearchPlayersFogSelect \n");
				}
			}

			if (llabs(CurGameTime - LastActionProcess) >= ACTION_UPDATE_TIME && GameStartedReally)
			{
				LastActionProcess = CurGameTime;
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
			else if (GetModuleHandleA("Game.dll"))
			{
				RecoveryFuncBack();
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
	NtQueryInformationThread = (NTQUERYINFOMATIONTHREAD)GetProcAddress(GetModuleHandle(_T("ntdll.dll")), _T("NtQueryInformationThread"));
	if (NtQueryInformationThread) {
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