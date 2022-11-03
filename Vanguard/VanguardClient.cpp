// A basic test implementation of Netcore for IPC in Dolphin

#pragma warning(disable : 4564)


#include <string>

#include "VanguardClient.h"
#include "VanguardClientInitializer.h"
#include "Helpers.hpp"

#include <msclr/marshal_cppstd.h>

#include "UnmanagedWrapper.h"

//#include "core/core.h"
#using < system.dll>
#using < system.windows.forms.dll>
#using < system.collections.dll>

//If we provide just the dll name and then compile with /AI it works, but intellisense doesn't pick up on it, so we use a full relative path
#using <../../../../RTCV/Build/NetCore.dll>
#using <../../../../RTCV/Build/Vanguard.dll>
#using <../../../../RTCV/Build/CorruptCore.dll>
#using <../../../../RTCV/Build/RTCV.Common.dll>


using namespace cli;
using namespace System;
using namespace Text;
using namespace RTCV;
using namespace RTCV::CorruptCore::Extensions;
using namespace NetCore;
using namespace CorruptCore;
using namespace Vanguard;
using namespace Runtime::InteropServices;
using namespace Threading;
using namespace Collections::Generic;
using namespace Reflection;
using namespace Diagnostics;

#define SRAM_SIZE 25165824
#define ARAM_SIZE 16777216
#define EXRAM_SIZE 67108864

static void EmuThreadExecute(Action^ callback);
static void EmuThreadExecute(IntPtr ptr);

// Define this in here as it's managed and weird stuff happens if it's in a header
public
ref class VanguardClient {
public:
    static NetCoreReceiver^ receiver;
    static VanguardConnector^ connector;

    static void OnMessageReceived(Object^ sender, NetCoreEventArgs^ e);
    static void SpecUpdated(Object^ sender, SpecUpdateEventArgs^ e);
    static void RegisterVanguardSpec();

    static void StartClient();
    static void RestartClient();
    static void StopClient();

    static void LoadRom(String^ filename);
    static bool LoadState(std::string filename);
    static bool SaveState(String^ filename, bool wait);

    //static String^ GetConfigAsJson(VanguardSettingsWrapper^ settings);
    //static VanguardSettingsWrapper^ GetConfigFromJson(String^ json);

    static void LoadWindowPosition();
    static void SaveWindowPosition();
    static String^ GetSyncSettings();
    static void SetSyncSettings(String^ ss);

    static int GetWordSizeByCore();
    static bool GetEndianByCore();

    static String^ emuDir = IO::Path::GetDirectoryName(Assembly::GetExecutingAssembly()->Location);
    static String^ logPath = IO::Path::Combine(emuDir, "EMU_LOG.txt");


    static array<String^>^ configPaths;

    static String^ CoreToLoad;

    static volatile bool loading = false;
    static volatile bool stateLoading = false;
    static bool attached = false;
    static Object^ GenericLockObject = gcnew Object();
    static bool enableRTC = true;
    static System::String^ lastStateName = "";
    static System::String^ fileToCopy = "";
    //static Core::TimingEventType* event;
};

static void EmuThreadExecute(Action^ callback) {
    EmuThreadExecute(Marshal::GetFunctionPointerForDelegate(callback));
}

static void EmuThreadExecute(IntPtr callbackPtr) {
    //  main_window.SetEmuThread(false);
    static_cast<void(__stdcall*)(void)>(callbackPtr.ToPointer())();
    // main_window.SetEmuThread(true);
}

static PartialSpec^
getDefaultPartial() {
    PartialSpec^ partial = gcnew PartialSpec("VanguardSpec");
    partial->Set(VSPEC::NAME,"RetroArch");
    partial->Set(VSPEC::SYSTEM,"RetroArch");
    partial->Set(VSPEC::GAMENAME,"IGNORE");
    partial->Set(VSPEC::SYSTEMPREFIX,"RA");
    partial->Set(VSPEC::OPENROMFILENAME,"IGNORE");
    partial->Set(VSPEC::SYNCSETTINGS,String::Empty);
    partial->Set(VSPEC::OVERRIDE_DEFAULTMAXINTENSITY,1000000);
    partial->Set(VSPEC::MEMORYDOMAINS_BLACKLISTEDDOMAINS, gcnew array<String^>{ });
    partial->Set(VSPEC::CORE_LASTLOADERROM,-1);
    partial->Set(VSPEC::SUPPORTS_RENDERING,false);
    partial->Set(VSPEC::SUPPORTS_CONFIG_MANAGEMENT,true);
    partial->Set(VSPEC::SUPPORTS_CONFIG_HANDOFF,true);
    partial->Set(VSPEC::SUPPORTS_KILLSWITCH,true);
    partial->Set(VSPEC::SUPPORTS_REALTIME,true);
    partial->Set(VSPEC::SUPPORTS_SAVESTATES,true);
    partial->Set(VSPEC::SUPPORTS_REFERENCES,true);
    partial->Set(VSPEC::CORE_DISKBASED,true);
    partial->Set(VSPEC::SUPPORTS_MIXED_STOCKPILE,true);
    partial->Set(VSPEC::CONFIG_PATHS, gcnew array<String^> { });
    partial->Set(VSPEC::EMUDIR, VanguardClient::emuDir);
    return partial;
}

void VanguardClient::SpecUpdated(Object^ sender, SpecUpdateEventArgs^ e) {
    PartialSpec^ partial = e->partialSpec;

    LocalNetCoreRouter::Route(Endpoints::CorruptCore,
        Commands::Remote::PushVanguardSpecUpdate, partial, true);
    LocalNetCoreRouter::Route(Endpoints::UI, Commands::Remote::PushVanguardSpecUpdate,
        partial, true);
}

void VanguardClient::RegisterVanguardSpec() {
    PartialSpec^ emuSpecTemplate = gcnew PartialSpec("VanguardSpec");

    emuSpecTemplate->Insert(getDefaultPartial());

    AllSpec::VanguardSpec = gcnew FullSpec(emuSpecTemplate, true);
    // You have to feed a partial spec as a template

    if (VanguardClient::attached)
        RTCV::Vanguard::VanguardConnector::PushVanguardSpecRef(AllSpec::VanguardSpec);

    LocalNetCoreRouter::Route(Endpoints::CorruptCore,
        Commands::Remote::PushVanguardSpec, emuSpecTemplate, true);
    LocalNetCoreRouter::Route(Endpoints::UI, Commands::Remote::PushVanguardSpec,
        emuSpecTemplate, true);
    AllSpec::VanguardSpec->SpecUpdated += gcnew EventHandler<SpecUpdateEventArgs^>(
        &VanguardClient::SpecUpdated);
}

// Lifted from Bizhawk
static Assembly^ CurrentDomain_AssemblyResolve(Object^ sender, ResolveEventArgs^ args) {
    try {
        Trace::WriteLine("Entering AssemblyResolve\n" + args->Name + "\n" +
            args->RequestingAssembly);
        String^ requested = args->Name;
        Monitor::Enter(AppDomain::CurrentDomain);
        {
            array<Assembly^>^ asms = AppDomain::CurrentDomain->GetAssemblies();
            for (int i = 0; i < asms->Length; i++) {
                Assembly^ a = asms[i];
                if (a->FullName == requested) {
                    return a;
                }
            }

            AssemblyName^ n = gcnew AssemblyName(requested);
            // load missing assemblies by trying to find them in the dll directory
            String^ dllname = n->Name + ".dll";
            String^ directory = IO::Path::Combine(
                IO::Path::GetDirectoryName(Assembly::GetExecutingAssembly()->Location), "..",
                "RTCV");
            String^ fname = IO::Path::Combine(directory, dllname);
            if (!IO::File::Exists(fname)) {
                Trace::WriteLine(fname + " doesn't exist");
                return nullptr;
            }

            // it is important that we use LoadFile here and not load from a byte array; otherwise
            // mixed (managed/unamanged) assemblies can't load
            Trace::WriteLine("Loading " + fname);
            return Assembly::UnsafeLoadFrom(fname);
        }
    }
    catch (Exception^ e) {
        Trace::WriteLine("Something went really wrong in AssemblyResolve. Send this to the devs\n" +
            e);
        return nullptr;
    }
    finally {
        Monitor::Exit(AppDomain::CurrentDomain);
    }
}

// Create our VanguardClient
void VanguardClientInitializer::Initialize() {
    // This has to be in its own method where no other dlls are used so the JIT can compile it
    AppDomain::CurrentDomain->AssemblyResolve +=
        gcnew ResolveEventHandler(CurrentDomain_AssemblyResolve);

    ConfigureVisualStyles();
    StartVanguardClient();
}

//This ensures things render as we want them.
//There are no issues running this within QT/WXWidgets applications
//This HAS to be its own method for the JIT. If it's merged with StartVanguardClient(), fun exceptions occur
void VanguardClientInitializer::ConfigureVisualStyles()
{
    // this needs to be done before the warnings/errors show up
    System::Windows::Forms::Application::EnableVisualStyles();
    System::Windows::Forms::Application::SetCompatibleTextRenderingDefault(false);
}

// Create our VanguardClient
void VanguardClientInitializer::StartVanguardClient()
{
    System::Windows::Forms::Form^ dummy = gcnew System::Windows::Forms::Form();
    IntPtr Handle = dummy->Handle;
    SyncObjectSingleton::SyncObject = dummy;
    //SyncObjectSingleton::EmuInvokeDelegate = gcnew SyncObjectSingleton::ActionDelegate(&EmuThreadExecute);
    SyncObjectSingleton::UseQueue = true;

    // Start everything
    VanguardClient::configPaths = gcnew array<String^>{""
    };

    VanguardClient::StartClient();
    VanguardClient::RegisterVanguardSpec();
    RtcCore::StartEmuSide();

    // Lie if we're in attached
    if (VanguardClient::attached)
        VanguardConnector::ImplyClientConnected();

    //VanguardClient::LoadWindowPosition();
}


void VanguardClient::StartClient() {
    RTCV::Common::Logging::StartLogging(logPath);
    // Can't use contains
    auto args = Environment::GetCommandLineArgs();
    for (int i = 0; i < args->Length; i++) {
        if (args[i] == "-ATTACHED") {
            attached = true;
        }
        if (args[i] == "-DISABLERTC") {
            enableRTC = false;
        }
    }

    receiver = gcnew NetCoreReceiver();
    receiver->Attached = attached;
    receiver->MessageReceived += gcnew EventHandler<NetCoreEventArgs^>(
        &VanguardClient::OnMessageReceived);
    connector = gcnew VanguardConnector(receiver);
}


void VanguardClient::RestartClient() {
    VanguardClient::StopClient();
    VanguardClient::StartClient();
}

void VanguardClient::StopClient() {
    connector->Kill();
    connector = nullptr;
}

void VanguardClient::LoadWindowPosition() {
    if (connector == nullptr)
        return;
}

void VanguardClient::SaveWindowPosition() {
}

String^ VanguardClient::GetSyncSettings() {
    /*auto wrapper = VanguardSettingsWrapper::GetVanguardSettingsFromCitra();
    auto ws = GetConfigAsJson(wrapper);
    return ws;*/
    return "";
}

void VanguardClient::SetSyncSettings(String^ ss) {
    //VanguardSettingsWrapper^ wrapper = nullptr;
    ////Hack for now to maintain compatibility.
    //if (ss == "N3DS") {
    //    wrapper = gcnew VanguardSettingsWrapper();
    //    wrapper->is_new_3ds = true;
    //}
    //else {
    //    wrapper = GetConfigFromJson(ss);
    //}
    //VanguardSettingsWrapper::SetSettingsFromWrapper(wrapper);
    CoreToLoad = ss;
}

int VanguardClient::GetWordSizeByCore()
{
    auto name = AllSpec::VanguardSpec->Get<String^>(VSPEC::SYSTEMCORE);
    return 4; // TODO
}

bool VanguardClient::GetEndianByCore()
{
    auto name = AllSpec::VanguardSpec->Get<String^>(VSPEC::SYSTEMCORE);
    return false; // TODO
}

#pragma region MemoryDomains

//For some reason if we do these in another class, melon won't build
public
ref class LibretroMdescMemoryDomain : RTCV::CorruptCore::IMemoryDomain {
public:
    LibretroMdescMemoryDomain(unsigned int id);
    property System::String^ Name { virtual System::String^ get(); }
    property long long Size { virtual long long get(); }
    property int WordSize { virtual int get(); }
    property bool BigEndian { virtual bool get(); }
    unsigned int Id;
    virtual unsigned char PeekByte(long long addr);
    virtual array<unsigned char>^ PeekBytes(long long address, int length);
    virtual void PokeByte(long long addr, unsigned char val);
    virtual String^ ToString() override
    {
        return Name;
    }
};

// the fallback retro_get_memory_data returned with SYSTEM_RAM; used if core has no mdescs
public
ref class LibretroFallbackMemoryDomain : RTCV::CorruptCore::IMemoryDomain {
public:
    property System::String^ Name { virtual System::String^ get(); }
    property long long Size { virtual long long get(); }
    property int WordSize { virtual int get(); }
    property bool BigEndian { virtual bool get(); }
    virtual unsigned char PeekByte(long long addr);
    virtual array<unsigned char>^ PeekBytes(long long address, int length);
    virtual void PokeByte(long long addr, unsigned char val);
    virtual String^ ToString() override
    {
        return Name;
    }
};

delegate void MessageDelegate(Object^);
#pragma region LibretroMdesc
LibretroMdescMemoryDomain::LibretroMdescMemoryDomain(unsigned int id) {
    Id = id;
}
String^ LibretroMdescMemoryDomain::Name::get() {
    return Helpers::utf8StringToSystemString(UnmanagedWrapper::GET_MEM_NAME(Id));
}

long long LibretroMdescMemoryDomain::Size::get() {
    return UnmanagedWrapper::GET_MEM_SIZE(Id);
}

int LibretroMdescMemoryDomain::WordSize::get() {
    return VanguardClient::GetWordSizeByCore();
}

bool LibretroMdescMemoryDomain::BigEndian::get() {
    return UnmanagedWrapper::IS_MEMDESC_BIGENDIAN(Id);
}

unsigned char LibretroMdescMemoryDomain::PeekByte(long long addr) {
    return UnmanagedWrapper::PEEK_BYTE(Id, addr);
}

void LibretroMdescMemoryDomain::PokeByte(long long addr, unsigned char val) {
    UnmanagedWrapper::POKE_BYTE(Id, addr, val);
}

array<unsigned char>^ LibretroMdescMemoryDomain::PeekBytes(long long address, int length) {
    array<unsigned char>^ bytes = gcnew array<unsigned char>(length);
    for (int i = 0; i < length; i++) {
        bytes[i] = PeekByte(address + i);
    }
    return bytes;
}
#pragma endregion
#pragma region LibretroFallback
String^ LibretroFallbackMemoryDomain::Name::get() {
    return Helpers::utf8StringToSystemString(UnmanagedWrapper::GET_MEM_NAME_ALT(2)); // SYSTEM_RAM
}

long long LibretroFallbackMemoryDomain::Size::get() {
    return UnmanagedWrapper::GET_MEM_SIZE_ALT(2);
}

int LibretroFallbackMemoryDomain::WordSize::get() {
    return VanguardClient::GetWordSizeByCore();
}

bool LibretroFallbackMemoryDomain::BigEndian::get() {
    return VanguardClient::GetEndianByCore();
}

unsigned char LibretroFallbackMemoryDomain::PeekByte(long long addr) {
    return UnmanagedWrapper::PEEK_BYTE_ALT(2, addr);
}

void LibretroFallbackMemoryDomain::PokeByte(long long addr, unsigned char val) {
    UnmanagedWrapper::POKE_BYTE_ALT(2, addr, val);
}

array<unsigned char>^ LibretroFallbackMemoryDomain::PeekBytes(long long address, int length) {
    array<unsigned char>^ bytes = gcnew array<unsigned char>(length);
    for (int i = 0; i < length; i++) {
        bytes[i] = PeekByte(address + i);
    }
    return bytes;
}
#pragma endregion


static array<MemoryDomainProxy^>^ GetInterfaces() {

    if (String::IsNullOrWhiteSpace(AllSpec::VanguardSpec->Get<String^>(VSPEC::OPENROMFILENAME)))
        return gcnew array<MemoryDomainProxy^>(0);

    List<MemoryDomainProxy^>^ interfaces = gcnew List<MemoryDomainProxy^>();
    for (unsigned int i = 0; i < UnmanagedWrapper::GET_NUM_MEMDESCS(); i++)
        interfaces->Add(gcnew MemoryDomainProxy(gcnew LibretroMdescMemoryDomain(i)));
    if (UnmanagedWrapper::GET_NUM_MEMDESCS() == 0)
        interfaces->Add(gcnew MemoryDomainProxy(gcnew LibretroFallbackMemoryDomain));
    return interfaces->Count > 0 ? interfaces->ToArray() : gcnew array<MemoryDomainProxy^>(0);
}

static bool RefreshDomains(bool updateSpecs = true) {
    array<MemoryDomainProxy^>^ oldInterfaces =
        AllSpec::VanguardSpec->Get<array<MemoryDomainProxy^>^>(VSPEC::MEMORYDOMAINS_INTERFACES);
    array<MemoryDomainProxy^>^ newInterfaces = GetInterfaces();

    // Bruteforce it since domains can c`   hange inconsistently in some configs and we keep code
    // consistent between implementations
    bool domainsChanged = false;
    if (oldInterfaces == nullptr)
        domainsChanged = true;
    else {
        domainsChanged = oldInterfaces->Length != newInterfaces->Length;
        for (int i = 0; i < oldInterfaces->Length; i++) {
            if (domainsChanged)
                break;
            if (oldInterfaces[i]->Name != newInterfaces[i]->Name)
                domainsChanged = true;
            if (oldInterfaces[i]->Size != newInterfaces[i]->Size)
                domainsChanged = true;
        }
    }

    if (updateSpecs) {
        AllSpec::VanguardSpec->Update(VSPEC::MEMORYDOMAINS_INTERFACES, newInterfaces, true, true);
        LocalNetCoreRouter::Route(Endpoints::CorruptCore,
            Commands::Remote::EventDomainsUpdated, domainsChanged,
            true);
    }

    return domainsChanged;
}

#pragma endregion

static void STEP_CORRUPT() // errors trapped by CPU_STEP
{
    if (!VanguardClient::enableRTC)
        return;
    RtcClock::StepCorrupt(true, true);
}


#pragma region Hooks
void VanguardClientUnmanaged::CORE_STEP() {
    if (!VanguardClient::enableRTC || (UnmanagedWrapper::GET_NUM_MEMDESCS() == 0 && UnmanagedWrapper::GET_MEM_SIZE_ALT(2) == 0))
        return;
    // Any step hook for corruption
    ActionDistributor::Execute("ACTION");
    STEP_CORRUPT();
}

// This is on the main thread not the emu thread
void VanguardClientUnmanaged::LOAD_GAME_START(std::string romPath) {
    if (!VanguardClient::enableRTC)
        return;
    StepActions::ClearStepBlastUnits();
    RtcClock::ResetCount();

    String^ gameName = Helpers::utf8StringToSystemString(romPath);
    AllSpec::VanguardSpec->Update(VSPEC::OPENROMFILENAME, gameName, true, true);
}


void VanguardClientUnmanaged::LOAD_GAME_DONE() {
    if (!VanguardClient::enableRTC)
        return;
    PartialSpec^ gameDone = gcnew PartialSpec("VanguardSpec");

    StepActions::ClearStepBlastUnits();
    RtcClock::ResetCount();

    if (UnmanagedWrapper::GET_NUM_MEMDESCS() == 0 && UnmanagedWrapper::GET_MEM_SIZE_ALT(2) == 0)
        return;


    String^ gameName = Helpers::utf8StringToSystemString(UnmanagedWrapper::GET_ROM_PATH());
    AllSpec::VanguardSpec->Update(VSPEC::OPENROMFILENAME, gameName, true, true);
    try {
        gameDone->Set(VSPEC::SYSTEM, Helpers::utf8StringToSystemString(UnmanagedWrapper::GET_CORE_NAME()));
        gameDone->Set(VSPEC::SYSTEMPREFIX, Helpers::utf8StringToSystemString(UnmanagedWrapper::GET_CORE_NAME()));
        gameDone->Set(VSPEC::SYSTEMCORE, Helpers::utf8StringToSystemString(UnmanagedWrapper::GET_CORE_PATH())->Substring(Helpers::utf8StringToSystemString(UnmanagedWrapper::GET_CORE_PATH())->LastIndexOf("\\") + 1));
        gameDone->Set(VSPEC::CORE_DISKBASED, true);

        String^ oldGame = AllSpec::VanguardSpec->Get<String^>(VSPEC::GAMENAME);

        String^ gameName = Helpers::utf8StringToSystemString(UnmanagedWrapper::GET_CONTENT_NAME());

        char replaceChar = L'-';
        gameDone->Set(VSPEC::GAMENAME,
            StringExtensions::MakeSafeFilename(gameName, replaceChar));

        //String^ syncsettings = VanguardClient::GetConfigAsJson(VanguardSettingsWrapper::GetVanguardSettingsFromCitra());
        gameDone->Set(VSPEC::SYNCSETTINGS, Helpers::utf8StringToSystemString(UnmanagedWrapper::GET_CORE_PATH()));

        AllSpec::VanguardSpec->Update(gameDone, true, false);

        bool domainsChanged = RefreshDomains(true);

        if (oldGame != gameName) {
            LocalNetCoreRouter::Route(Endpoints::UI,
                Commands::Basic::ResetGameProtectionIfRunning, true);
        }
    }
    catch (Exception^ e) {
        Trace::WriteLine(e->ToString());
    }

    /*
    VanguardClient::event = Core::System::GetInstance().CoreTiming().RegisterEvent(
        "RTCV::run_event",
        [](u64 thread_id,
               s64 cycle_late)
        {
            RunCallback(
                thread_id, cycle_late);
        });

     Core::System::GetInstance().CoreTiming().ScheduleEvent(run_interval_ticks, VanguardClient::event);
     */
    VanguardClient::loading = false;

}


void VanguardClientUnmanaged::LOAD_STATE_DONE() {
    if (!VanguardClient::enableRTC)
        return;
    VanguardClient::stateLoading = false;
}

void VanguardClientUnmanaged::GAME_CLOSED() {
    if (!VanguardClient::enableRTC)
        return;
    AllSpec::VanguardSpec->Update(VSPEC::OPENROMFILENAME, "", true, true);
    RefreshDomains();
    RtcCore::InvokeGameClosed(true);
}


bool VanguardClientUnmanaged::RTC_OSD_ENABLED() {
    if (!VanguardClient::enableRTC)
        return true;
    if (RTCV::NetCore::Params::IsParamSet(RTCSPEC::CORE_EMULATOROSDDISABLED))
        return false;
    return true;
}

#pragma endregion

/*ENUMS FOR THE SWITCH STATEMENT*/
enum COMMANDS {
    SAVESAVESTATE,
    LOADSAVESTATE,
    REMOTE_LOADROM,
    REMOTE_CLOSEGAME,
    REMOTE_DOMAIN_GETDOMAINS,
    REMOTE_KEY_SETSYNCSETTINGS,
    REMOTE_KEY_SETSYSTEMCORE,
    REMOTE_EVENT_EMU_MAINFORM_CLOSE,
    REMOTE_EVENT_EMUSTARTED,
    REMOTE_ISNORMALADVANCE,
    REMOTE_EVENT_CLOSEEMULATOR,
    REMOTE_ALLSPECSSENT,
    REMOTE_POSTCORRUPTACTION,
    REMOTE_RESUMEEMULATION,
    UNKNOWN
};

inline COMMANDS CheckCommand(String^ inString) {
    if (String::Compare(inString, NetCore::Commands::Basic::LoadSavestate) == 0)
        return LOADSAVESTATE;
    if (String::Compare(inString, NetCore::Commands::Basic::SaveSavestate) == 0)
        return SAVESAVESTATE;
    if (String::Compare(inString, NetCore::Commands::Remote::LoadROM) == 0)
        return REMOTE_LOADROM;
    if (String::Compare(inString, NetCore::Commands::Remote::CloseGame) == 0)
        return REMOTE_CLOSEGAME;
    if (String::Compare(inString, NetCore::Commands::Remote::AllSpecSent) == 0)
        return REMOTE_ALLSPECSSENT;
    if (String::Compare(inString, NetCore::Commands::Remote::DomainGetDomains) == 0)
        return REMOTE_DOMAIN_GETDOMAINS;
    if (String::Compare(inString, NetCore::Commands::Remote::KeySetSystemCore) == 0)
        return REMOTE_KEY_SETSYSTEMCORE;
    if (String::Compare(inString, NetCore::Commands::Remote::KeySetSyncSettings) == 0)
        return REMOTE_KEY_SETSYNCSETTINGS;
    if (String::Compare(inString, NetCore::Commands::Remote::EventEmuMainFormClose) == 0)
        return REMOTE_EVENT_EMU_MAINFORM_CLOSE;
    if (String::Compare(inString, NetCore::Commands::Remote::EventEmuStarted) == 0)
        return REMOTE_EVENT_EMUSTARTED;
    if (String::Compare(inString, NetCore::Commands::Remote::IsNormalAdvance) == 0)
        return REMOTE_ISNORMALADVANCE;
    if (String::Compare(inString, NetCore::Commands::Remote::EventCloseEmulator) == 0)
        return REMOTE_EVENT_CLOSEEMULATOR;
    if (String::Compare(inString, NetCore::Commands::Remote::PostCorruptAction) == 0)
        return REMOTE_POSTCORRUPTACTION;
    if (String::Compare(inString, NetCore::Commands::Remote::ResumeEmulation) == 0)
        return REMOTE_RESUMEEMULATION;
    return UNKNOWN;
}

/* IMPLEMENT YOUR COMMANDS HERE */
void VanguardClient::LoadRom(String^ filename) {
    String^ currentOpenRom = "";
    if (AllSpec::VanguardSpec->Get<String^>(VSPEC::OPENROMFILENAME) != "")
        currentOpenRom = AllSpec::VanguardSpec->Get<String^>(VSPEC::OPENROMFILENAME);

    // Game is not running
    if (currentOpenRom != filename || CoreToLoad != Helpers::utf8StringToSystemString(UnmanagedWrapper::GET_CORE_PATH())) {
        std::string path = Helpers::systemStringToUtf8String(filename);
        loading = true;
        UnmanagedWrapper::LOAD_CONTENT(Helpers::systemStringToUtf8String(CoreToLoad), path);
        // We have to do it this way to prevent deadlock due to synced calls. It sucks but it's
        // required at the moment
        /*while (loading) {
            Thread::Sleep(20);
            System::Windows::Forms::Application::DoEvents();
        }*/

        Thread::Sleep(10); // Give the emu thread a chance to recover
    }
}


bool VanguardClient::LoadState(std::string filename) {
    StepActions::ClearStepBlastUnits();
    RtcClock::ResetCount();
    stateLoading = true;
    UnmanagedWrapper::VANGUARD_LOADSTATE(filename);
    // We have to do it this way to prevent deadlock due to synced calls. It sucks but it's required
    // at the moment
    UnmanagedWrapper::PERFORM_TASKS();
    int i = 0;
    do {
        Thread::Sleep(20);
        System::Windows::Forms::Application::DoEvents();

        // We wait for 20 ms every time. If loading a game takes longer than 10 seconds, break out.
        if (++i > 500) {
            stateLoading = false;
            return false;
        }
    } while (stateLoading);
    //RefreshDomains();
    return true;
}

bool VanguardClient::SaveState(String^ filename, bool wait) {
    std::string s = Helpers::systemStringToUtf8String(filename);
    const char* converted_filename = s.c_str();
    VanguardClient::lastStateName = filename;
    UnmanagedWrapper::VANGUARD_SAVESTATE(s);
    VanguardClientUnmanaged::SAVE_STATE_DONE();
    return true;
}

void VanguardClientUnmanaged::SAVE_STATE_DONE() {
    Thread::Sleep(2000);
}

// No fun anonymous classes with closure here
#pragma region Delegates
void StopGame() {
    UnmanagedWrapper::VANGUARD_STOPGAME();
}

void Quit() {
    System::Environment::Exit(0);
}

void AllSpecsSent() {
    VanguardClient::LoadWindowPosition();
}
#pragma endregion

/* THIS IS WHERE YOU HANDLE ANY RECEIVED MESSAGES */
void VanguardClient::OnMessageReceived(Object^ sender, NetCoreEventArgs^ e) {
    NetCoreMessage^ message = e->message;
    NetCoreAdvancedMessage^ advancedMessage;

    if (Helpers::is<NetCoreAdvancedMessage^>(message))
        advancedMessage = static_cast<NetCoreAdvancedMessage^>(message);

    switch (CheckCommand(message->Type)) {
    case REMOTE_ALLSPECSSENT: {
        auto g = gcnew SyncObjectSingleton::GenericDelegate(&AllSpecsSent);
        SyncObjectSingleton::FormExecute(g);
    }
                            break;

    case LOADSAVESTATE: {
        array<Object^>^ cmd = static_cast<array<Object^>^>(advancedMessage->objectValue);
        String^ path = static_cast<String^>(cmd[0]);
        std::string converted_path = Helpers::systemStringToUtf8String(path);

        // Load up the sync settings
        String^ settingStr = AllSpec::VanguardSpec->Get<String^>(VSPEC::SYNCSETTINGS);
        if (!String::IsNullOrEmpty(settingStr)) {
            VanguardClient::SetSyncSettings(settingStr);
        }
        bool success = LoadState(converted_path);
        // bool success = true;
        e->setReturnValue(success);
    }
                      break;

    case SAVESAVESTATE: {
        String^ Key = (String^)(advancedMessage->objectValue);

        ////Save the syncsettings
        //AllSpec::VanguardSpec->Set(VSPEC::SYNCSETTINGS, VanguardClient::GetSyncSettings());

        // Build the shortname
        String^ quickSlotName = Key + ".timejump";
        // Get the prefix for the state

        String^ gameName = Helpers::utf8StringToSystemString(UnmanagedWrapper::GET_CONTENT_NAME());

        char replaceChar = L'-';
        String^ prefix = StringExtensions::MakeSafeFilename(gameName, replaceChar);
        prefix = prefix->Substring(prefix->LastIndexOf('\\') + 1);

        String^ path = nullptr;
        // Build up our path
        path = RtcCore::workingDir + IO::Path::DirectorySeparatorChar + "SESSION" + IO::Path::
            DirectorySeparatorChar + prefix + "." + quickSlotName + ".State";

        // If the path doesn't exist, make it
        IO::FileInfo^ file = gcnew IO::FileInfo(path);
        if (file->Directory != nullptr && file->Directory->Exists == false)
            file->Directory->Create();
        VanguardClient::SaveState(path, true);
        e->setReturnValue(path);
    }
                      break;

    case REMOTE_LOADROM: {
        // Load up the sync settings
        String^ settingStr = AllSpec::VanguardSpec->Get<String^>(VSPEC::SYNCSETTINGS);
        if (!String::IsNullOrEmpty(settingStr)) {
            VanguardClient::SetSyncSettings(settingStr);
        }
        String^ filename = (String^)advancedMessage->objectValue;
        //Citra DEMANDS the rom is loaded from the main thread
        System::Action<String^>^ a = gcnew Action<String^>(&LoadRom);
        SyncObjectSingleton::FormExecute<String^>(a, filename);
    }
                       break;

    case REMOTE_CLOSEGAME: {
        SyncObjectSingleton::GenericDelegate^ g = gcnew SyncObjectSingleton::GenericDelegate(&StopGame);
        SyncObjectSingleton::FormExecute(g);
    }
                         break;

    case REMOTE_DOMAIN_GETDOMAINS: {
        RefreshDomains();
    }
                                 break;

    case REMOTE_KEY_SETSYNCSETTINGS: {
        String^ settings = (String^)(advancedMessage->objectValue);
        AllSpec::VanguardSpec->Set(VSPEC::SYNCSETTINGS, settings);
        CoreToLoad = settings;
    }
                                   break;

    case REMOTE_KEY_SETSYSTEMCORE: {
        // Do nothing
    }
                                 break;

    case REMOTE_EVENT_EMUSTARTED: {
        // Do nothing
    }
                                break;

    case REMOTE_ISNORMALADVANCE: {
        // Todo - Dig out fast forward?
        e->setReturnValue(true);
    }
                               break;
    case REMOTE_POSTCORRUPTACTION: {
    }
                                 break;

    case REMOTE_RESUMEEMULATION: {
        UnmanagedWrapper::VANGUARD_RESUMEEMULATION();
    }
                               break;

    case REMOTE_EVENT_EMU_MAINFORM_CLOSE:
    case REMOTE_EVENT_CLOSEEMULATOR: {
        //Don't allow re-entry on this
        Monitor::Enter(VanguardClient::GenericLockObject);
        {
            VanguardClient::SaveWindowPosition();
            Quit();
        }
        Monitor::Exit(VanguardClient::GenericLockObject);
    }
                                   break;

    default:
        break;
    }
}

//VanguardSettingsWrapper^ VanguardSettingsWrapper::GetVanguardSettingsFromCitra() {
//    VanguardSettingsWrapper^ vSettings = gcnew VanguardSettingsWrapper();
//
//    UnmanagedWrapper::GetSettingsFromCitra();
//    vSettings->is_new_3ds = UnmanagedWrapper::nSettings.is_new_3ds;
//    vSettings->region_value = UnmanagedWrapper::nSettings.region_value;
//    vSettings->init_clock = UnmanagedWrapper::nSettings.init_clock;
//    vSettings->init_time = UnmanagedWrapper::nSettings.init_time;
//    vSettings->shaders_accurate_mul = UnmanagedWrapper::nSettings.shaders_accurate_mul;
//    vSettings->upright_screen = UnmanagedWrapper::nSettings.upright_screen;
//    vSettings->enable_dsp_lle = UnmanagedWrapper::nSettings.enable_dsp_lle;
//    vSettings->enable_dsp_lle_multithread = UnmanagedWrapper::nSettings.enable_dsp_lle_multithread;
//
//    // settings->birthmonth = Service::PTM::Module::GetPlayCoins();
//    // settings->birthday = Service::PTM::Module::GetPlayCoins();
//    // settings->language_index = GetSystemLanguage()
//    // settings->country = Service::PTM::Module::GetPlayCoins();
//    // settings->play_coin = Service::PTM::Module::GetPlayCoins();
//
//    return vSettings;
//}
//
//void VanguardSettingsWrapper::SetSettingsFromWrapper(VanguardSettingsWrapper^ vSettings) {
//    UnmanagedWrapper::nSettings.is_new_3ds = vSettings->is_new_3ds;
//    UnmanagedWrapper::nSettings.region_value = vSettings->region_value;
//    UnmanagedWrapper::nSettings.init_clock = 1;
//    UnmanagedWrapper::nSettings.init_time = vSettings->init_time;
//    UnmanagedWrapper::nSettings.shaders_accurate_mul = vSettings->shaders_accurate_mul;
//    UnmanagedWrapper::nSettings.upright_screen = vSettings->upright_screen;
//    UnmanagedWrapper::nSettings.enable_dsp_lle = vSettings->enable_dsp_lle;
//    UnmanagedWrapper::nSettings.enable_dsp_lle_multithread = vSettings->enable_dsp_lle_multithread;
//    UnmanagedWrapper::SetSettingsFromUnmanagedWrapper();
//}
