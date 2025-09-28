#pragma once

#include <CvDLLXMLIFaceBase.h>

class MyCvDLLXml : public CvDLLXmlIFaceBase
{
public:

	// Inherited via CvDLLXmlIFaceBase
	virtual FXml* CreateFXml(FXmlSchemaCache* pSchemaCache) override;
	virtual void DestroyFXml(FXml*& xml) override;
	virtual void DestroyFXmlSchemaCache(FXmlSchemaCache*&) override;
	virtual FXmlSchemaCache* CreateFXmlSchemaCache() override;
	virtual bool LoadXml(FXml* xml, const TCHAR* pszXmlFile) override;
	virtual bool Validate(FXml* xml, TCHAR* pszError) override;
	virtual bool LocateNode(FXml* xml, const TCHAR* pszXmlNode) override;
	virtual bool LocateFirstSiblingNodeByTagName(FXml* xml, const TCHAR* pszTagName) override;
	virtual bool LocateNextSiblingNodeByTagName(FXml* xml, const TCHAR* pszTagName) override;
	virtual bool NextSibling(FXml* xml) override;
	virtual bool PrevSibling(FXml* xml) override;
	virtual bool SetToChild(FXml* xml) override;
	virtual bool SetToChildByTagName(FXml* xml, const TCHAR* szTagName) override;
	virtual bool SetToParent(FXml* xml) override;
	virtual bool AddChildNode(FXml* xml, TCHAR* pszNewNode) override;
	virtual bool AddSiblingNodeBefore(FXml* xml, TCHAR* pszNewNode) override;
	virtual bool AddSiblingNodeAfter(FXml* xml, TCHAR* pszNewNode) override;
	virtual bool WriteXml(FXml* xml, TCHAR* pszXmlFile) override;
	virtual bool SetInsertedNodeAttribute(FXml* xml, TCHAR* pszAttributeName, TCHAR* pszAttributeValue) override;
	virtual int GetLastNodeTextSize(FXml* xml) override;
	virtual bool GetLastNodeText(FXml* xml, TCHAR* pszText) override;
	virtual bool GetLastNodeValue(FXml* xml, std::string& pszText) override;
	virtual bool GetLastNodeValue(FXml* xml, std::wstring& pszText) override;
	virtual bool GetLastNodeValue(FXml* xml, char* pszText) override;
	virtual bool GetLastNodeValue(FXml* xml, wchar_t* pszText) override;
	virtual bool GetLastNodeValue(FXml* xml, bool* pbVal) override;
	virtual bool GetLastNodeValue(FXml* xml, int* piVal) override;
	virtual bool GetLastNodeValue(FXml* xml, float* pfVal) override;
	virtual bool GetLastNodeValue(FXml* xml, unsigned int* puiVal) override;
	virtual int GetInsertedNodeTextSize(FXml* xml) override;
	virtual bool GetInsertedNodeText(FXml* xml, TCHAR* pszText) override;
	virtual bool SetInsertedNodeText(FXml* xml, TCHAR* pszText) override;
	virtual bool GetLastLocatedNodeType(FXml* xml, TCHAR* pszType) override;
	virtual bool GetLastInsertedNodeType(FXml* xml, TCHAR* pszType) override;
	virtual bool IsLastLocatedNodeCommentNode(FXml* xml) override;
	virtual int NumOfElementsByTagName(FXml* xml, const TCHAR* pszTagName) override;
	virtual int NumOfChildrenByTagName(FXml* xml, const TCHAR* pszTagName) override;
	virtual int GetNumSiblings(FXml* xml) override;
	virtual int GetNumChildren(FXml* xml) override;
	virtual bool GetLastLocatedNodeTagName(FXml* xml, TCHAR* pszTagName) override;
	virtual bool IsAllowXMLCaching() override;
	virtual void MapChildren(FXml*) override;
};