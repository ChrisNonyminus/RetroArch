#include "VanguardClientInitializer.h"
#include "VanguardClient.h"
#include "UnmanagedWrapper.h"
#include "VanguardWrapper.h"

void Vanguard_InitializeVanguard()
{
	VanguardClientInitializer::Initialize();
}

void Vanguard_CoreStep()
{
	UnmanagedWrapper::VANGUARD_CORESTEP();
}

void Vanguard_LoadGameDone()
{
	VanguardClientUnmanaged::LOAD_GAME_DONE();
}

void Vanguard_GameClosed()
{
	VanguardClientUnmanaged::GAME_CLOSED();
}

void Vanguard_LoadStateDone()
{
	VanguardClientUnmanaged::LOAD_STATE_DONE();
}
