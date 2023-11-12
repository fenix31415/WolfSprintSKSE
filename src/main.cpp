extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Query(const SKSE::QueryInterface* a_skse, SKSE::PluginInfo* a_info)
{
#ifndef DEBUG
	auto sink = std::make_shared<spdlog::sinks::msvc_sink_mt>();
#else
	auto path = logger::log_directory();
	if (!path) {
		return false;
	}

	*path /= Version::PROJECT;
	*path += ".log"sv;
	auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);
#endif

	auto log = std::make_shared<spdlog::logger>("global log"s, std::move(sink));

#ifndef DEBUG
	log->set_level(spdlog::level::trace);
#else
	log->set_level(spdlog::level::info);
	log->flush_on(spdlog::level::info);
#endif

	spdlog::set_default_logger(std::move(log));
	spdlog::set_pattern("%g(%#): [%^%l%$] %v"s);

	logger::info(FMT_STRING("{} v{}"), Version::PROJECT, Version::NAME);

	a_info->infoVersion = SKSE::PluginInfo::kVersion;
	a_info->name = Version::PROJECT.data();
	a_info->version = Version::MAJOR;

	if (a_skse->IsEditor()) {
		logger::critical("Loaded in editor, marking as incompatible"sv);
		return false;
	}

	const auto ver = a_skse->RuntimeVersion();
	if (ver < SKSE::RUNTIME_1_5_39) {
		logger::critical(FMT_STRING("Unsupported runtime version {}"), ver.string());
		return false;
	}

	return true;
}

class DataHandler
{
public:
	static inline RE::SpellItem* intro;
	static inline RE::SpellItem* Pre_Holder;
	static inline RE::SpellItem* model;

	static void init()
	{
		auto handler = RE::TESDataHandler::GetSingleton();
		intro = handler->LookupForm<RE::SpellItem>(0xD73, "Woof_Woof_Auf.esl"sv);
		Pre_Holder = handler->LookupForm<RE::SpellItem>(0xD70, "Woof_Woof_Auf.esl"sv);
		model = handler->LookupForm<RE::SpellItem>(0xD68, "Woof_Woof_Auf.esl"sv);
	}
};

class SprintHandlerHook
{
	static RE::BSEventNotifyControl ProcessAnimEvent(RE::BSTEventSink<RE::BSAnimationGraphEvent>* _this,
		const RE::BSAnimationGraphEvent* a_event, RE::BSTEventSource<RE::BSAnimationGraphEvent>* a_eventSource)
	{
		if (a_event && a_event->holder) {
			if (auto a = const_cast<RE::Actor*>(a_event->holder->As<RE::Actor>())) {
				if (a_event->tag == "StartAnimatedCameraDelta" && a->HasSpell(DataHandler::Pre_Holder)) {
					FenixUtils::cast_spell(a, a, DataHandler::intro);
					//FenixUtils::cast_spell(a, a, DataHandler::model);
				}

				if (a_event->tag == "EndAnimatedCameraDelta") {
					a->RemoveSpell(DataHandler::model);
				}
			}
		}

		return _ProcessAnimEvent(_this, a_event, a_eventSource);
	}
	
	static inline REL::Relocation<decltype(ProcessAnimEvent)> _ProcessAnimEvent;

public:
	static void Hook()
	{
		_ProcessAnimEvent = REL::Relocation<uintptr_t>(RE::VTABLE_PlayerCharacter[2]).write_vfunc(0x1, ProcessAnimEvent);
	}
};

static void SKSEMessageHandler(SKSE::MessagingInterface::Message* message)
{
	switch (message->type) {
	case SKSE::MessagingInterface::kDataLoaded:
		DataHandler::init();
		SprintHandlerHook::Hook();

		break;
	}
}

extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Load(const SKSE::LoadInterface* a_skse)
{
	auto g_messaging = reinterpret_cast<SKSE::MessagingInterface*>(a_skse->QueryInterface(SKSE::LoadInterface::kMessaging));
	if (!g_messaging) {
		logger::critical("Failed to load messaging interface! This error is fatal, plugin will not load.");
		return false;
	}

	logger::info("loaded");

	SKSE::Init(a_skse);
	SKSE::AllocTrampoline(1 << 10);

	g_messaging->RegisterListener("SKSE", SKSEMessageHandler);

	return true;
}
