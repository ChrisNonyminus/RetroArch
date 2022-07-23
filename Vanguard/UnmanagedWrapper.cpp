#include "UnmanagedWrapper.h"
#include "runloop.h"
#include "libretro.h"
#include "core.h"
#include "content.h"
#include "tasks/task_content.h"
#include <paths.h>
#include "frontend/frontend.h"
#include <menu/menu_driver.h>

uint8_t UnmanagedWrapper::PEEK_BYTE(unsigned int id, long long addr)
{
	rarch_system_info_t* system = &runloop_state_get_ptr()->system;
	if (id > system->mmaps.num_descriptors)
		return 0;
	unsigned char* ptr = (unsigned char*)system->mmaps.descriptors[id].core.ptr;
	if (ptr) {
		return ptr[addr];
	}
	return 0;
}

void UnmanagedWrapper::POKE_BYTE(unsigned int id, long long addr, uint8_t val)
{
	rarch_system_info_t* system = &runloop_state_get_ptr()->system;
	if (id > system->mmaps.num_descriptors)
		return;
	unsigned char* ptr = (unsigned char*)system->mmaps.descriptors[id].core.ptr;
	if (ptr) {
		ptr[addr] = val;
	}
}

uint8_t UnmanagedWrapper::PEEK_BYTE_ALT(unsigned int type, long long addr)
{
	runloop_state_t* runloop_st = runloop_state_get_ptr();
	auto ptr = (unsigned char*)runloop_st->current_core.retro_get_memory_data(type);
	if (ptr) {
		return ptr[addr];
	}
	return 0;
}

void UnmanagedWrapper::POKE_BYTE_ALT(unsigned int type, long long addr, uint8_t val)
{
	runloop_state_t* runloop_st = runloop_state_get_ptr();
	auto ptr = (unsigned char*)runloop_st->current_core.retro_get_memory_data(type);
	if (ptr) {
		ptr[addr] = val;
	}
}

size_t UnmanagedWrapper::GET_MEM_SIZE(unsigned int id)
{
	rarch_system_info_t* system = &runloop_state_get_ptr()->system;
	if (id > system->mmaps.num_descriptors)
		return 0;
	return system->mmaps.descriptors[id].core.len;
}

std::string UnmanagedWrapper::GET_MEM_NAME(unsigned int id)
{
	rarch_system_info_t* system = &runloop_state_get_ptr()->system;
	if (id > system->mmaps.num_descriptors)
		return "";
	const char* addrspacename = system->mmaps.descriptors[id].core.addrspace;
	if (addrspacename != nullptr) {
		return std::string(addrspacename);
	}
	char name[1024];
	const char* ramtype = system->mmaps.descriptors[id].core.flags & RETRO_MEMDESC_SYSTEM_RAM ? "SYSTEMRAM" : (system->mmaps.descriptors[id].core.flags & RETRO_MEMDESC_VIDEO_RAM ? "VIDEORAM" : (system->mmaps.descriptors[id].core.flags & RETRO_MEMDESC_SAVE_RAM ? "SAVERAM" : ""));
	sprintf(name, "%s_%zX", ramtype, system->mmaps.descriptors[id].core.start);
	return std::string(name);
}

size_t UnmanagedWrapper::GET_MEM_SIZE_ALT(unsigned int type)
{
	runloop_state_t* runloop_st = runloop_state_get_ptr();
	return runloop_st->current_core.retro_get_memory_size(type);
}

std::string UnmanagedWrapper::GET_MEM_NAME_ALT(unsigned int type)
{
	switch (type) {
	case RETRO_MEMORY_SAVE_RAM:
		return "SAVERAM";
	case RETRO_MEMORY_SYSTEM_RAM:
		return "SYSTEMRAM";
	case RETRO_MEMORY_VIDEO_RAM:
		return "VIDEORAM";
	}
	return "UNKNOWN";
}

unsigned int UnmanagedWrapper::GET_NUM_MEMDESCS()
{
	rarch_system_info_t* system = &runloop_state_get_ptr()->system;
	return system->mmaps.num_descriptors;
}

void UnmanagedWrapper::VANGUARD_SAVESTATE(const std::string& file)
{
	content_save_state(file.c_str(), true, true);
}

void UnmanagedWrapper::VANGUARD_LOADSTATE(const std::string& file)
{
	content_load_state(file.c_str(), false, true);
}

std::string UnmanagedWrapper::GET_ROM_PATH()
{
	return std::string(path_get(RARCH_PATH_CONTENT));
}

std::string UnmanagedWrapper::GET_CORE_PATH()
{
	return std::string(path_get(RARCH_PATH_CORE));
}

std::string UnmanagedWrapper::GET_CORE_NAME()
{
	rarch_system_info_t* system = &runloop_state_get_ptr()->system;
	if (system->subsystem.data)
		return std::string(system->subsystem.data->desc);

	// TODO: the above code doesn't seem to work
	return "Unknown";
}

void UnmanagedWrapper::LOAD_CONTENT(const std::string& corepath, const std::string& rompath)
{

	content_ctx_info_t content_info;

	content_info.argc = 0;
	content_info.argv = NULL;
	content_info.args = NULL;
	content_info.environ_get = NULL;

	task_push_load_content_with_new_core_from_menu(corepath.c_str(), rompath.c_str(), &content_info, CORE_TYPE_PLAIN, NULL, NULL);
}

std::string UnmanagedWrapper::GET_CONTENT_NAME()
{

	//rarch_system_info_t* system = &runloop_state_get_ptr()->system;
	//const struct retro_subsystem_info* subsystem = NULL;
	//runloop_state_t* runloop_st = runloop_state_get_ptr();

	///* Core fully loaded, use the subsystem data */
	//if (system->subsystem.data) {
	//	return std::string(subsystem->roms[content_get_subsystem_rom_id()].desc);
	//}
	///* Core not loaded completely, use the data we peeked on load core */
	//else
		return "Unknown";

	// TODO: the above code doesn't seem to work
}

bool UnmanagedWrapper::IS_MEMDESC_BIGENDIAN(unsigned int id)
{
	rarch_system_info_t* system = &runloop_state_get_ptr()->system;
	if (id > system->mmaps.num_descriptors)
		return false;
	return system->mmaps.descriptors[id].core.flags & RETRO_MEMDESC_BIGENDIAN;
}

#include "VanguardClient.h"

void UnmanagedWrapper::VANGUARD_SAVESTATE_DONE()
{
	VanguardClientUnmanaged::SAVE_STATE_DONE();
}

void UnmanagedWrapper::VANGUARD_LOADSTATE_DONE()
{
	VanguardClientUnmanaged::LOAD_STATE_DONE();
}

void UnmanagedWrapper::VANGUARD_STOPGAME()
{
	runloop_event_deinit_core();
}

std::string UnmanagedWrapper::VANGUARD_SAVECONFIG()
{
	return "";
}

void UnmanagedWrapper::VANGUARD_LOADCONFIG(std::string cfg)
{
}

void UnmanagedWrapper::VANGUARD_CORESTEP()
{
	VanguardClientUnmanaged::CORE_STEP();
}

void UnmanagedWrapper::LOAD_GAME_DONE()
{
	VanguardClientUnmanaged::LOAD_GAME_DONE();
}

void UnmanagedWrapper::LOAD_STATE_DONE()
{
	VanguardClientUnmanaged::LOAD_STATE_DONE();
}

void UnmanagedWrapper::VANGUARD_EXIT()
{
}

void UnmanagedWrapper::VANGUARD_PAUSEEMULATION()
{
	command_event(CMD_EVENT_PAUSE, NULL);
}

void UnmanagedWrapper::VANGUARD_RESUMEEMULATION()
{
	command_event(CMD_EVENT_UNPAUSE, NULL);
}

