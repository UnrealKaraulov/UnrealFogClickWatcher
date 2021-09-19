// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
// Absol (d3scene.ru), Abs0l (d3scene.com) 
// 2016 All Right Reserved :)

#define WIN32_LEAN_AND_MEAN

#include <Windows.h>
#include <string>
#include "verinfo.h"

int GameDll = 0;

int IsInGame = 0;

BOOL GameStarted = FALSE;

int pJassEnvAddress = 0;

int pW3XGlobalClass = 0;

int pPrettyPrint = 0;

char * DefaultString = "DefaultString\0\xa..........................................................................................................";

int GetObjectClassID( int unit_or_item_addr )
{
	return *( int* ) ( unit_or_item_addr + 0x30 );
}

//sub_6F32C880 126a
//sub_6F327020 127a
typedef int( __fastcall * p_GetTypeInfo )( int unit_item_code, int unused );
p_GetTypeInfo GetTypeInfo = NULL;

//sub_6F2F8F90 126a
//sub_6F34F730 127a
typedef char *( __fastcall * p_GetPlayerName )( int a1, int a2 );
p_GetPlayerName GetPlayerName = NULL;

char * ReadJassSID( int JSID )
{
	int Convert = GameDll + 0x459640; int GetCurrentJassEnvironment = GameDll + 0x44B2E0;
	char * cRet = NULL;
	__asm
	{
		push JSID;
		mov ecx, 1;
		call GetCurrentJassEnvironment;
		mov ecx, eax;
		call Convert;
		mov ecx, dword ptr ds : [ eax + 0x08 ];
		mov eax, dword ptr ds : [ ecx + 0x1C ];
		mov cRet, eax;
	}
	return cRet;
}


char * GetObjectNameByID( int clid )
{
	if ( clid > 0 )
	{
		int v3 = GetTypeInfo( clid, 0 );
		int v4, v5;
		if ( v3 && ( v4 = *( int * ) ( v3 + 40 ) ) != 0 )
		{
			v5 = v4 - 1;
			if ( v5 >= ( unsigned int ) 0 )
				v5 = 0;
			return ( char * ) *( int * ) ( *( int * ) ( v3 + 44 ) + 4 * v5 );
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
	int FirstAddress = *( int * ) pJassEnvAddress;
	if ( FirstAddress )
	{
		FirstAddress = *( int * ) ( FirstAddress + 20 );
		if ( FirstAddress )
		{

			FirstAddress = *( int * ) ( FirstAddress + 32 );
			if ( FirstAddress )
			{

				int NextAddress = FirstAddress;

				while ( true )
				{
					if ( *( int * ) ( NextAddress + 12 ) == oldaddress )
					{
						*( int * ) ( NextAddress + 12 ) = newaddress;

						return NextAddress + 12;
					}

					NextAddress = *( int* ) NextAddress;

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

typedef int( __cdecl * pGetSpellAbilityUnit )( );
pGetSpellAbilityUnit GetSpellAbilityUnit;

typedef int( __cdecl * pGetSpellTargetUnit )( );
pGetSpellTargetUnit GetSpellTargetUnit;

typedef int( __fastcall * pGetHandleAddress ) ( int HandleID, int unused );
pGetHandleAddress GetHandleAddress;


DWORD LatestAbilSpell = GetTickCount( );

char PrintBuffer[ 1024 ];


void DisplayText( char* text, float StayUpTime )
{
	__asm
	{
		PUSH 0xFFFED312;
		PUSH StayUpTime;
		PUSH text;
		MOV ECX, DWORD PTR DS : [ pW3XGlobalClass ];
		MOV ECX, DWORD PTR DS : [ ECX ];
		MOV EAX, pPrettyPrint;
		CALL EAX;
	}
}




int GetSpellAbilityId_hooked( )
{
	int SpellAbilCode = GetSpellAbilityId_real( );


	if ( GetTickCount( ) - LatestAbilSpell > 1000 )
	{
	
		LatestAbilSpell = GetTickCount( );

		int CasterUnitHandle = GetSpellAbilityUnit( );
		int TargetUnitHandle = GetSpellTargetUnit( );

		if ( CasterUnitHandle > 0 )
		{

			if ( CasterUnitHandle > 0 && TargetUnitHandle <= 0 )
			{
				int unit1 = GetHandleAddress( CasterUnitHandle, 0 );
				if ( unit1 )
				{
					sprintf_s( PrintBuffer, 1024, "[%s] uses [%s]", GetObjectName( unit1 ), GetObjectNameByID( SpellAbilCode ) );
					DisplayText( PrintBuffer, 2.2f );
				}
			}
			else if ( CasterUnitHandle > 0 )
			{
				int unit1 = GetHandleAddress( CasterUnitHandle, 0 );
				int unit2 = GetHandleAddress( TargetUnitHandle, 0 );
				if ( unit1 && unit2 )
				{
					sprintf_s( PrintBuffer, 1024, "[%s] uses [%s]\nTarget:[%s]", GetObjectName( unit1 ), GetObjectNameByID( SpellAbilCode ), GetObjectName( unit2 ) );
					DisplayText( PrintBuffer, 2.2f );
				}
				else if ( unit1 )
				{
					sprintf_s( PrintBuffer, 1024, "[%s] uses [%s]", GetObjectName( unit1 ), GetObjectNameByID( SpellAbilCode ) );
					DisplayText( PrintBuffer, 2.2f );
				}
			}
		}
	}
	return SpellAbilCode;
}

//GetSpellAbilityId
void InitOtherFunctions( )
{
	int pOldRecoveryJassNative1 = pRecoveryJassNative1;
	pRecoveryJassNative1 = CreateJassNativeHook( ( int ) GetSpellAbilityId_real, ( int ) &GetSpellAbilityId_hooked );
	if ( pRecoveryJassNative1 == 0 )
		pRecoveryJassNative1 = pOldRecoveryJassNative1;
}

void * GameWaitThreadID = 0;
DWORD __stdcall GameWaiting( void * )
{


	while ( true )
	{
		if ( *( BOOL* ) IsInGame )
		{
			if ( !GameStarted )
			{
				GameStarted = TRUE;
				InitOtherFunctions( );
			}

		}
		else
		{
			GameStarted = FALSE;
		}

		Sleep( 1000 );
	}

	return 0;
}



void Init126aVer( )
{
	IsInGame = GameDll + 0xAB62A4;
	pJassEnvAddress = GameDll + 0xADA848;
	GetTypeInfo = ( p_GetTypeInfo ) ( GameDll + 0x32C880 );
	GetPlayerName = ( p_GetPlayerName ) ( GameDll + 0x2F8F90 );
	GetSpellAbilityId_real = ( pGetSpellAbilityId ) ( GameDll + 0x3C32A0 );
	GetSpellAbilityUnit = ( pGetSpellAbilityUnit ) ( GameDll + 0x3C3470 );
	GetSpellTargetUnit = ( pGetSpellTargetUnit ) ( GameDll + 0x3C3A80 );
	GetHandleAddress = ( pGetHandleAddress ) ( GameDll + 0x3BDCB0 );
	pW3XGlobalClass = GameDll + 0xAB4F80;
	pPrettyPrint = GameDll + 0x2F3CF0;


	GameStarted = TRUE;
	InitOtherFunctions( );
	GameWaitThreadID = CreateThread( 0, 0, GameWaiting, 0, 0, 0 );
	
}


void Init127aVer( )
{
	IsInGame = GameDll + 0xBE6530;
	pJassEnvAddress = GameDll + 0xBE3740;
	GetTypeInfo = ( p_GetTypeInfo ) ( GameDll + 0x327020 );
	GetPlayerName = ( p_GetPlayerName ) ( GameDll + 0x34F730 );
	GetSpellAbilityId_real = ( pGetSpellAbilityId ) ( GameDll + 0x1E4DD0 );
	GetSpellAbilityUnit = ( pGetSpellAbilityUnit ) ( GameDll + 0x1E4E40 );
	GetSpellTargetUnit = ( pGetSpellTargetUnit ) ( GameDll + 0x1E52B0 );
	GetHandleAddress = ( pGetHandleAddress ) ( GameDll + 0x1D1550 );
	pW3XGlobalClass = GameDll + 0xBE6350;
	pPrettyPrint = GameDll + 0x3574B0;

	GameStarted = TRUE;
	InitOtherFunctions( );
	GameWaitThreadID = CreateThread( 0, 0, GameWaiting, 0, 0, 0 );
	
}


void InitializeSkillInformer( )
{
	HMODULE hGameDll = GetModuleHandle( L"Game.dll" );
	if ( !hGameDll )
	{
		MessageBox( 0, L"UnrealSkillInformer problem!\nNo game.dll found.", L"Game.dll not found", 0 );
		return;
	}

	GameDll = ( int ) hGameDll;

	CFileVersionInfo gdllver;
	gdllver.Open( hGameDll );
	// Game.dll version (1.XX)
	int GameDllVer = gdllver.GetFileVersionQFE( );

	if ( GameDllVer == 6401 )
	{
		Init126aVer( );
	}
	else if ( GameDllVer == 52240 )
	{
		Init127aVer( );
	}
	else
	{
		MessageBox( 0, L"UnrealSkillInformer problem!\nGame version not supported.", L"\nGame version not supported", 0 );
	}

	// 6401 - 126
	// 52240 - 127

	gdllver.Close( );
}


BOOL __stdcall DllMain( HINSTANCE, DWORD r, LPVOID )
{
	if ( r == DLL_PROCESS_ATTACH )
	{
		InitializeSkillInformer( );
	}
	else if ( r == DLL_PROCESS_DETACH )
	{
		if ( GameWaitThreadID )
			TerminateThread( GameWaitThreadID, 0 );
		if ( pRecoveryJassNative1 > 0 )
		{
			if ( !IsBadReadPtr( ( void* ) pRecoveryJassNative1, 4 ) )
			{
				if ( !IsBadWritePtr( ( void* ) pRecoveryJassNative1, 4 ) )
				{
					*( int* ) pRecoveryJassNative1 = ( int ) GetSpellAbilityId_real;
				}
			}
		}
	}

	return TRUE;
}