#pragma once
#include <string>
#include <mutex>
#include "retroarch_types.h"

struct VanguardSettingsUnmanaged {

   void GetSettingsFromRetroarch() {}
};
typedef void (*FnPtr_VanguardMethod)();
class UnmanagedWrapper
{
public:

   static uint8_t PEEK_BYTE(unsigned int id, long long addr);
   static void POKE_BYTE(unsigned int id, long long addr, uint8_t val);
   static uint8_t PEEK_BYTE_ALT(unsigned int type, long long addr);
   static void POKE_BYTE_ALT(unsigned int type, long long addr, uint8_t val);
   static size_t GET_MEM_SIZE(unsigned int id);
   static std::string GET_MEM_NAME(unsigned int id);
   static size_t GET_MEM_SIZE_ALT(unsigned int type);
   static std::string GET_MEM_NAME_ALT(unsigned int type);
   static unsigned int GET_NUM_MEMDESCS();
   static void VANGUARD_SAVESTATE(const std::string& file);
   static void VANGUARD_LOADSTATE(const std::string& file);
   static std::string GET_ROM_PATH();
   static std::string GET_CORE_PATH();
   static std::string GET_CORE_NAME();
   static void LOAD_CONTENT(const std::string& corepath, const std::string& rompath);
   static std::string GET_CONTENT_NAME();
   static bool IS_MEMDESC_BIGENDIAN(unsigned int id);

   static void VANGUARD_SAVESTATE_DONE();
   static void VANGUARD_LOADSTATE_DONE();
   static void VANGUARD_STOPGAME();
   static std::string VANGUARD_SAVECONFIG();
   static void VANGUARD_LOADCONFIG(std::string cfg);
   static void VANGUARD_CORESTEP();
   static void LOAD_GAME_DONE();
   static void LOAD_STATE_DONE();
   static void VANGUARD_EXIT();
   static void VANGUARD_PAUSEEMULATION();
   static void VANGUARD_RESUMEEMULATION();


   static VanguardSettingsUnmanaged nSettings;
};
