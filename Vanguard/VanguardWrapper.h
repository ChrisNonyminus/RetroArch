#ifndef H_VANGUARDWRAPPER
#define H_VANGUARDWRAPPER
// Wrapper from C++ vanguard functions to C
#ifdef __cplusplus
extern "C" {
#endif

void Vanguard_InitializeVanguard();
void Vanguard_CoreStep();
void Vanguard_LoadGameDone();
void Vanguard_GameClosed();
void Vanguard_LoadStateDone();
#ifdef __cplusplus
}
#endif
#endif
