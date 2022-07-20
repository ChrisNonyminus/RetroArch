#include "retroarch_types.h"
#include "runloop.h"
#include "libretro.h"
#include "core.h"
#include "content.h"
#include "tasks/task_content.h"
#include <paths.h>
#include <menu/menu_driver.h>



extern "C" __declspec(dllexport) unsigned char VanguardWrapper_peekbyte(unsigned int id, long long addr) {
	rarch_system_info_t* system = &runloop_state_get_ptr()->system;
	if (id > system->mmaps.num_descriptors)
		return 0;
	unsigned char* ptr = (unsigned char*)system->mmaps.descriptors[id].core.ptr;
	return ptr[addr];
}

extern "C" __declspec(dllexport) void VanguardWrapper_pokebyte(unsigned int id, long long addr, unsigned char val) {
	rarch_system_info_t* system = &runloop_state_get_ptr()->system;
	if (id > system->mmaps.num_descriptors)
		return;
	unsigned char* ptr = (unsigned char*)system->mmaps.descriptors[id].core.ptr;
	ptr[addr] = val;
}

extern "C" __declspec(dllexport) unsigned char VanguardWrapper_peekbyte_alt(unsigned int type, long long addr) {
	runloop_state_t* runloop_st = runloop_state_get_ptr();
	auto ptr = (unsigned char*)runloop_st->current_core.retro_get_memory_data(type);
	if (ptr) {
		return ptr[addr];
	}
}

extern "C" __declspec(dllexport) void VanguardWrapper_pokebyte_alt(unsigned int type, long long addr, unsigned char val) {
	runloop_state_t* runloop_st = runloop_state_get_ptr();
	auto ptr = (unsigned char*)runloop_st->current_core.retro_get_memory_data(type);
	if (ptr) {
		ptr[addr] = val;
	}
}

extern "C" __declspec(dllexport) size_t VanguardWrapper_getmemsize(unsigned int id) {
	rarch_system_info_t* system = &runloop_state_get_ptr()->system;
	if (id > system->mmaps.num_descriptors)
		return 0;
	return system->mmaps.descriptors[id].core.len;
}

extern "C" __declspec(dllexport) const char* VanguardWrapper_getmemname(unsigned int id) {
	rarch_system_info_t* system = &runloop_state_get_ptr()->system;
	if (id > system->mmaps.num_descriptors)
		return "";
	const char* addrspacename = system->mmaps.descriptors[id].core.addrspace;
	if (addrspacename != nullptr) {
		return addrspacename;
	}
	char name[1024];
	const char* ramtype = system->mmaps.descriptors[id].core.flags & RETRO_MEMDESC_SYSTEM_RAM ? "SYSTEMRAM" : (system->mmaps.descriptors[id].core.flags & RETRO_MEMDESC_VIDEO_RAM ? "VIDEORAM" : (system->mmaps.descriptors[id].core.flags & RETRO_MEMDESC_SAVE_RAM ? "SAVERAM" : ""));
	sprintf(name, "%s_%zX", ramtype, system->mmaps.descriptors[id].core.start);
	return name;
}

extern "C" __declspec(dllexport) size_t VanguardWrapper_getmemsize_alt(unsigned int type) {
	runloop_state_t* runloop_st = runloop_state_get_ptr();
	return runloop_st->current_core.retro_get_memory_size(type);
}

extern "C" __declspec(dllexport) const char* VanguardWrapper_getmemname_alt(unsigned int type) {
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

extern "C" __declspec(dllexport) unsigned int VanguardWrapper_getmemdesccount() {
	rarch_system_info_t* system = &runloop_state_get_ptr()->system;
	return system->mmaps.num_descriptors;
}

extern "C" __declspec(dllexport) void VanguardWrapper_savestate(const char* path) {
	content_save_state(path, true, true);
}

extern "C" __declspec(dllexport) void VanguardWrapper_loadstate(const char* path) {
	content_load_state(path, false, true);
}

extern "C" __declspec(dllexport) const char* VanguardWrapper_getrompath() {
	return path_get(RARCH_PATH_CONTENT);
}

extern "C" __declspec(dllexport) const char* VanguardWrapper_getcorepath() {
	return path_get(RARCH_PATH_CORE);
}

extern "C" __declspec(dllexport) const char* VanguardWrapper_getcorename() {
	rarch_system_info_t* system = &runloop_state_get_ptr()->system;
	if (system->subsystem.data)
		return system->subsystem.data->desc;
	
	// TODO: the above code doesn't seem to work
	return "Unknown";
}

extern "C" __declspec(dllexport) void VanguardWrapper_loadcontent(const char* core, const char* rompath) {

	content_ctx_info_t content_info;

	content_info.argc = 0;
	content_info.argv = NULL;
	content_info.args = NULL;
	content_info.environ_get = NULL;

	task_push_load_content_with_new_core_from_menu(core, rompath, &content_info, CORE_TYPE_PLAIN, NULL, NULL);
}

extern "C" __declspec(dllexport) const char* VanguardWrapper_getcontentname() {

	rarch_system_info_t* system = &runloop_state_get_ptr()->system;
	const struct retro_subsystem_info* subsystem = NULL;
	runloop_state_t* runloop_st = runloop_state_get_ptr();

	/* Core fully loaded, use the subsystem data */
	if (system->subsystem.data) {
		return subsystem->roms[content_get_subsystem_rom_id()].desc;
	}
	/* Core not loaded completely, use the data we peeked on load core */
	else
		return "Unknown";
	
	// TODO: the above code doesn't seem to work
	return "Unknown";
}

extern "C" __declspec(dllexport) bool VanguardWrapper_ismemregionbigendian(unsigned int id) {
	rarch_system_info_t* system = &runloop_state_get_ptr()->system;
	if (id > system->mmaps.num_descriptors)
		return "";
	return system->mmaps.descriptors[id].core.flags & RETRO_MEMDESC_BIGENDIAN;
}
