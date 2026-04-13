// Include this once into your bot DLL to export the required symbol.
// You must also have a `MyBotPlugin kPlayerBotPlugin` global variable.

extern "C"
#ifdef _MSC_VER
__declspec(dllexport)
#else
__attribute__((__visibility__("default")))
#endif
const cvbot::IPlayerBotPlugin& getPlayerBotPlugin()
{
	return kPlayerBotPlugin;
}