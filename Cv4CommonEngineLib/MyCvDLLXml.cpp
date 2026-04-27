#include "MyCvDLLXml.h"
#include "inc/Cv4CommonEngineLib/CvVFS.h"
#include "inc/Cv4CommonEngineLib/CivIni.h"

#include "CommonEngineGlobal.h"

#include <CommonStuff/StringConversion.h>

//#include <tinyxml2.h>
// Pugixml, to handle file encodings better.
#include <pugixml.hpp>

#include <filesystem>
#include <ranges>
#include <unordered_map>
#include <map>
#include <iostream>
#include <cerrno>
#include <string_view>

using cvengine::gVFS;

class FXml
{
public:
	pugi::xml_document doc;
	pugi::xml_node current{};
	std::map<std::string_view, pugi::xml_node> childrenMap;

	bool trySetCurrent(pugi::xml_node element) noexcept
	{
		if (element)
		{
			current = element;
			return true;
		}
		else
			return false;
	}
};

FXml* MyCvDLLXml::CreateFXml(FXmlSchemaCache*)
{
	return new FXml();
}

void MyCvDLLXml::DestroyFXml(FXml*& xml)
{
	delete std::exchange(xml, nullptr);
}

void MyCvDLLXml::DestroyFXmlSchemaCache(FXmlSchemaCache*&)
{
}

FXmlSchemaCache* MyCvDLLXml::CreateFXmlSchemaCache()
{
	return nullptr;
}

/*
#include <fstream>
#include <mutex>

static void checkFileHasUtf8ExtensionBytes(const std::filesystem::path& path)
{
	std::ifstream file(path);
	std::ostringstream buffer;
	buffer << file.rdbuf();
	const size_t numUtf8ExtBytes = std::ranges::count_if(buffer.view(), [](unsigned char c) { return 0x80 <= c && c < 0xA0; });
	if (numUtf8ExtBytes)
		std::wclog << L"WARNING: " << path << " contains characters that are Windows-1252 but not ISO-8859-1 (UTF-8 extension bytes)." << std::endl;
}*/

bool MyCvDLLXml::LoadXml(FXml* xml, const TCHAR* pszXmlFile)
{
	//return xml->doc.LoadFile(gVFS->resolve(pszXmlFile).string().c_str()) == tinyxml2::XML_SUCCESS;

	//static bool kCheckAllXML = [] {
	//	for (const std::filesystem::directory_entry& dirEntry : std::filesystem::recursive_directory_iterator(
	//		gCivilizationIVIni.get(kCivilizationIVIniSection_CV4ENGINE, kCivilizationIVIniProp_VanillaCiv4RootDir, L""),
	//		std::filesystem::directory_options::follow_directory_symlink
	//	))
	//	{
	//		if (heck::ci_wstring_view(dirEntry.path().extension().wstring()) == L".xml")
	//			checkFileHasUtf8ExtensionBytes(dirEntry.path());
	//	}
	//	return true;
	//	}();

	const std::filesystem::path& physPath = gVFS->resolve(pszXmlFile);
	const std::wstring wpath = physPath.wstring();

	std::wclog << L"Loading " << wpath << L"..." << std::endl;

	// TODO: What's the encoding of the given path? Really, the caller's file enumeration will need to return wstring/fs::path paths.
	const pugi::xml_parse_result result = xml->doc.load_file(wpath.c_str());
	std::wclog << L"Status = " << std::to_underlying(result.status) << L", encoding = " << std::to_underlying(result.encoding) << std::endl;

	// If you want to check whether any XML files contain potential Windows-1252.
	//checkFileHasUtf8ExtensionBytes(physPath);

	return result.status == pugi::status_ok;
}

bool MyCvDLLXml::Validate(FXml*, [[maybe_unused]] TCHAR* pszError)
{
	std::abort();
}

bool MyCvDLLXml::LocateNode(FXml* xml, const TCHAR* pszXmlNode)
{
	pugi::xml_node node = xml->doc.root();
	for (const auto name : std::views::split(std::string(pszXmlNode), '/'))
		node = node.child(std::string_view(name));
	if (!node)
		return false;
	xml->current = node;
	return true;
}

bool MyCvDLLXml::LocateFirstSiblingNodeByTagName(FXml*, [[maybe_unused]] const TCHAR* pszTagName)
{
	std::abort();
}

bool MyCvDLLXml::LocateNextSiblingNodeByTagName(FXml* xml, const TCHAR* pszTagName)
{
	return xml->trySetCurrent(xml->current.next_sibling(pszTagName));
}

static pugi::xml_node skipToElement(pugi::xml_node node)
{
	while (node && node.type() != pugi::node_element)
		node = node.next_sibling();
	return node;
}

bool MyCvDLLXml::NextSibling(FXml* xml)
{
	if (const pugi::xml_node next = skipToElement(xml->current.next_sibling()))
	{
		xml->current = next;
		return true;
	}
	else
		return false;
}

bool MyCvDLLXml::PrevSibling(FXml*)
{
	std::abort();
}

bool MyCvDLLXml::SetToChild(FXml* xml)
{
	return xml->trySetCurrent(skipToElement(xml->current.first_child()));
}

bool MyCvDLLXml::SetToChildByTagName(FXml* xml, const TCHAR* szTagName)
{
	return xml->trySetCurrent(xml->current.child(szTagName));
}

bool MyCvDLLXml::SetToParent(FXml* xml)
{
	if (const pugi::xml_node parent = xml->current.parent())
	{
		xml->current = parent;
		return true;
	}
	else
		return false;
}

bool MyCvDLLXml::AddChildNode(FXml*, [[maybe_unused]] TCHAR* pszNewNode)
{
	std::abort();
}

bool MyCvDLLXml::AddSiblingNodeBefore(FXml*, [[maybe_unused]] TCHAR* pszNewNode)
{
	std::abort();
}

bool MyCvDLLXml::AddSiblingNodeAfter(FXml*, [[maybe_unused]] TCHAR* pszNewNode)
{
	std::abort();
}

bool MyCvDLLXml::WriteXml(FXml*, [[maybe_unused]] TCHAR* pszXmlFile)
{
	std::abort();
}

bool MyCvDLLXml::SetInsertedNodeAttribute(FXml*, [[maybe_unused]] TCHAR* pszAttributeName, [[maybe_unused]] TCHAR* pszAttributeValue)
{
	std::abort();
}

int MyCvDLLXml::GetLastNodeTextSize(FXml*)
{
	std::abort();
}

bool MyCvDLLXml::GetLastNodeText(FXml* xml, TCHAR* pszText)
{
	//abort();

	// TODO: Is this correct?
	return GetLastNodeValue(xml, pszText);
}

bool MyCvDLLXml::GetLastNodeValue(FXml* xml, std::string& pszText)
{
	// TODO: Is this right? CvRouteInfo might need this.
	//       msxml3 has some odd behaviour, maybe. See nodeTypedValue.
	pugi::xml_node element = xml->current;
	if (const pugi::xml_node childElement = skipToElement(element.first_child()))
		element = childElement;

	// This should definitely be UTF-8!
	pszText = element.text().get();
	return true;
}

bool MyCvDLLXml::GetLastNodeValue(FXml* xml, std::wstring& pszText)
{
	std::string utf8Str;
	if (GetLastNodeValue(xml, utf8Str))
	{
		pszText = heck::convertUtf8ToWide(utf8Str);
		return true;
	}
	else
		return false;
}

bool MyCvDLLXml::GetLastNodeValue(FXml* xml, char* pszText)
{
	std::string text;
	if (GetLastNodeValue(xml, text))
	{
		std::copy_n(text.c_str(), text.size() + 1, pszText); // copy nul terminator too
		return true;
	}
	else
		return false;
}

bool MyCvDLLXml::GetLastNodeValue(FXml*, [[maybe_unused]] wchar_t* pszText)
{
	std::abort();
}

bool MyCvDLLXml::GetLastNodeValue(FXml* xml, bool* pbVal)
{
	int value = 0;
	if (GetLastNodeValue(xml, &value) && (value == 0 || value == 1))
	{
		*pbVal = bool(value);
		return true;
	}
	else
		return false;
}

bool MyCvDLLXml::GetLastNodeValue(FXml* xml, int* piVal)
{
	const char* const text = xml->current.text().get();
	char* endPtr = nullptr;
	errno = 0;
	*piVal = std::strtol(text, &endPtr, 10);
	return errno == 0 && *endPtr == '\0';
}

bool MyCvDLLXml::GetLastNodeValue(FXml* xml, float* pfVal)
{
	const char* const text = xml->current.text().get();
	char* endPtr = nullptr;
	errno = 0;
	*pfVal = std::strtof(text, &endPtr);
	return errno == 0 && *endPtr == '\0';
}

bool MyCvDLLXml::GetLastNodeValue(FXml*, [[maybe_unused]] unsigned int* puiVal)
{
	std::abort();
}

int MyCvDLLXml::GetInsertedNodeTextSize(FXml*)
{
	std::abort();
}

bool MyCvDLLXml::GetInsertedNodeText(FXml*, [[maybe_unused]] TCHAR* pszText)
{
	std::abort();
}

bool MyCvDLLXml::SetInsertedNodeText(FXml*, [[maybe_unused]] TCHAR* pszText)
{
	std::abort();
}

bool MyCvDLLXml::GetLastLocatedNodeType(FXml* xml, TCHAR* pszType)
{
	// TODO: How the heck should this be implemented.
	const std::string_view name = xml->current.name();
	if (name == "iDefineIntVal")
		std::ranges::copy("int", pszType);
	else if (name == "fDefineFloatVal")
		std::ranges::copy("float", pszType);
	else if (name == "DefineTextVal")
		std::ranges::copy("string", pszType);
	else
		std::abort();
	return true;
}

bool MyCvDLLXml::GetLastInsertedNodeType(FXml*, [[maybe_unused]] TCHAR* pszType)
{
	std::abort();
}

bool MyCvDLLXml::IsLastLocatedNodeCommentNode(FXml*)
{
	return false;
}

int MyCvDLLXml::NumOfElementsByTagName(FXml* xml, const TCHAR* pszTagName)
{
	pugi::xml_node node = xml->doc.root();
	std::string lastSegment;
	for (const auto name : std::views::split(std::string(pszTagName), '/'))
		node = node.child(lastSegment = std::string_view(name));

	if (!node)
		return false;
	
	int n = 0;
	for (; node; node = node.next_sibling(lastSegment))
		if (node.type() == pugi::node_element)
			++n;
	return n;
}

int MyCvDLLXml::NumOfChildrenByTagName(FXml* xml, const TCHAR* pszTagName)
{
	int n = 0;
	for (pugi::xml_node node = xml->current.child(pszTagName); node; node = node.next_sibling(pszTagName))
		if (node.type() == pugi::node_element)
			++n;
	return n;
}

int MyCvDLLXml::GetNumSiblings(FXml* xml)
{
	int n = 0;
	// TODO: Include previous?
	//for (const tinyxml2::XMLElement* node = xml->current->PreviousSiblingElement(); node; node = node->PreviousSiblingElement())
	//	++n;
	for (pugi::xml_node node = xml->current.next_sibling(); node; node = node.next_sibling())
		if (node.type() == pugi::node_element)
			++n;
	return n;
}

int MyCvDLLXml::GetNumChildren(FXml* xml)
{
	int n = 0;
	for (pugi::xml_node node = xml->current.first_child(); node; node = node.next_sibling())
		if (node.type() == pugi::node_element)
			++n;
	return n;
}

bool MyCvDLLXml::GetLastLocatedNodeTagName(FXml*, [[maybe_unused]] TCHAR* pszTagName)
{
	std::abort();
}

bool MyCvDLLXml::IsAllowXMLCaching()
{
	return false;
}

void MyCvDLLXml::MapChildren(FXml* xml)
{
	xml->childrenMap.clear();
	for (pugi::xml_node child = xml->current.first_child(); child; child = child.next_sibling())
		if (const std::string_view name = child.name(); !name.empty())
			xml->childrenMap.emplace(name, child);
}
