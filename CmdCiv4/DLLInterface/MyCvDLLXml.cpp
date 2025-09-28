#include "MyCvDLLXml.h"
#include "../CvVFS.h"
#include "../Common.h"

#include <CommonStuff/StringConversion.h>

#include <tinyxml2.h>

#include <filesystem>
#include <ranges>
#include <unordered_map>
#include <map>

class FXml
{
public:
	tinyxml2::XMLDocument doc;
	const tinyxml2::XMLElement* current = nullptr;
	std::map<std::string_view, const tinyxml2::XMLElement*> childrenMap;

	bool trySetCurrent(const tinyxml2::XMLElement* element) noexcept
	{
		return element ? current = element, true : false;
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

bool MyCvDLLXml::LoadXml(FXml* xml, const TCHAR* pszXmlFile)
{
	return xml->doc.LoadFile(gVFS->resolve(pszXmlFile).string().c_str()) == tinyxml2::XML_SUCCESS;
}

bool MyCvDLLXml::Validate(FXml*, [[maybe_unused]] TCHAR* pszError)
{
	std::abort();
}

bool MyCvDLLXml::LocateNode(FXml* xml, const TCHAR* pszXmlNode)
{
	const tinyxml2::XMLElement* const root = xml->doc.RootElement();
	const tinyxml2::XMLElement* node = nullptr;
	for (const auto name : std::views::split(std::string(pszXmlNode), '/'))
	{
		if (!node)
		{
			
			if (std::string_view(name) == root->Name())
				node = root;
			else
				return false;
		}
		else
		{
			node = node->FirstChildElement(std::string(std::string_view(name)).c_str());
			if (!node)
				return false;
		}
	}
	if (!node)
		std::abort();
	xml->current = node;
	return true;
}

bool MyCvDLLXml::LocateFirstSiblingNodeByTagName(FXml*, [[maybe_unused]] const TCHAR* pszTagName)
{
	std::abort();
}

bool MyCvDLLXml::LocateNextSiblingNodeByTagName(FXml* xml, const TCHAR* pszTagName)
{
	return xml->trySetCurrent(xml->current->NextSiblingElement(pszTagName));
}

bool MyCvDLLXml::NextSibling(FXml* xml)
{
	return xml->trySetCurrent(xml->current->NextSiblingElement());
}

bool MyCvDLLXml::PrevSibling(FXml*)
{
	std::abort();
}

bool MyCvDLLXml::SetToChild(FXml* xml)
{
	return xml->trySetCurrent(xml->current->FirstChildElement());
}

bool MyCvDLLXml::SetToChildByTagName(FXml* xml, const TCHAR* szTagName)
{
	return xml->trySetCurrent(xml->current->FirstChildElement(szTagName));
}

bool MyCvDLLXml::SetToParent(FXml* xml)
{
	const tinyxml2::XMLNode* const parent = xml->current->Parent();
	if (parent && parent->ToElement())
	{
		xml->current = parent->ToElement();
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
	const tinyxml2::XMLElement* element = xml->current;
	if (element->FirstChildElement())
		element = element->FirstChildElement();
	if (element->GetText())
		pszText = element->GetText();
	else
		pszText.clear();
	return true;
}

bool MyCvDLLXml::GetLastNodeValue(FXml* xml, std::wstring& pszText)
{
	std::string codepageStr;
	if (GetLastNodeValue(xml, codepageStr))
	{
		// ISO-8859-1 == Unicode
		pszText.resize_and_overwrite(codepageStr.size(), [&codepageStr](wchar_t* out, size_t n) {
			for (size_t i = 0; i < n; ++i)
				out[i] = static_cast<std::uint8_t>(codepageStr[i]);
			return n;
			});
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
	const char* const text = xml->current->GetText();
	char* endPtr = nullptr;
	errno = 0;
	*piVal = std::strtol(text, &endPtr, 10);
	return errno == 0 && *endPtr == '\0';
}

bool MyCvDLLXml::GetLastNodeValue(FXml* xml, float* pfVal)
{
	const char* const text = xml->current->GetText();
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
	const std::string_view name = xml->current->Name();
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
	const tinyxml2::XMLElement* const root = xml->doc.RootElement();
	const tinyxml2::XMLElement* node = nullptr;
	std::string lastSegment;
	for (const auto name : std::views::split(std::string(pszTagName), '/'))
	{
		lastSegment = std::string_view(name);
		if (!node)
		{

			if (lastSegment == root->Name())
				node = root;
			else
				return false;
		}
		else
		{
			node = node->FirstChildElement(lastSegment.c_str());
			if (!node)
				return false;
		}
	}

	if (!node)
		std::abort();
	
	int n = 0;
	for (; node; node = node->NextSiblingElement(lastSegment.c_str()))
		++n;
	return n;
}

int MyCvDLLXml::NumOfChildrenByTagName(FXml* xml, const TCHAR* pszTagName)
{
	int n = 0;
	for (const tinyxml2::XMLElement* node = xml->current->FirstChildElement(pszTagName); node; node = node->NextSiblingElement(pszTagName))
		++n;
	return n;
}

int MyCvDLLXml::GetNumSiblings(FXml* xml)
{
	int n = 0;
	// TODO: Include previous?
	//for (const tinyxml2::XMLElement* node = xml->current->PreviousSiblingElement(); node; node = node->PreviousSiblingElement())
	//	++n;
	for (const tinyxml2::XMLElement* node = xml->current->NextSiblingElement(); node; node = node->NextSiblingElement())
		++n;
	return n;
}

int MyCvDLLXml::GetNumChildren(FXml* xml)
{
	int n = 0;
	for (const tinyxml2::XMLElement* node = xml->current->FirstChildElement(); node; node = node->NextSiblingElement())
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
	for (const tinyxml2::XMLElement* child = xml->current->FirstChildElement(); child; child = child->NextSiblingElement())
		xml->childrenMap.emplace(child->Name(), child);
}
