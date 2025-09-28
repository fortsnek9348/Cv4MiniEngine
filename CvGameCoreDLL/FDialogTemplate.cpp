#include "CvGameCoreDLL.h"
#include "FDialogTemplate.h"
#include "CvGameCoreDLLUndefNew.h"

#if defined(WIN32)

#include <tchar.h>
#include "CvGameCoreDLLUndefNew.h"

CDialogTemplate::CDialogTemplate(const char* caption, DWORD style, int x, int y, int w, int h,
								 const char* font, WORD fontSize)
{
	usedBufferLength = sizeof(DLGTEMPLATE);
	totalBufferLength = usedBufferLength;

	dialogTemplate = (DLGTEMPLATE*)malloc(totalBufferLength);

	dialogTemplate->style = style;

	if (font != nullptr)
	{
		dialogTemplate->style |= DS_SETFONT;
	}

	dialogTemplate->x     = static_cast<short>(x);
	dialogTemplate->y     = static_cast<short>(y);
	dialogTemplate->cx    = static_cast<short>(w);
	dialogTemplate->cy    = static_cast<short>(h);
	dialogTemplate->cdit  = 0;

	dialogTemplate->dwExtendedStyle = 0;

	// Assume no menu or special class
	AppendData(_T("\0"), 2);
	AppendData(_T("\0"), 2);

	// Add the dialog's caption to the template
	AppendString(caption);

	if (font != nullptr)
	{
		AppendData(&fontSize, sizeof(WORD));
		AppendString(font);
	}
}

CDialogTemplate::~CDialogTemplate()
{
	free(dialogTemplate);
}

void CDialogTemplate::AddComponent(const char* type, const char* caption, DWORD style, DWORD exStyle,
								   int x, int y, int w, int h, WORD id)
{
	DLGITEMTEMPLATE item;

	item.style = style;
	item.x     = static_cast<short>(x);
	item.y     = static_cast<short>(y);
	item.cx    = static_cast<short>(w);
	item.cy    = static_cast<short>(h);
	item.id    = id;

	item.dwExtendedStyle = exStyle;

	AppendData(&item, sizeof(DLGITEMTEMPLATE));

	AppendString(type);
	AppendString(caption);

	WORD creationDataLength = 0;
	AppendData(&creationDataLength, sizeof(WORD));

	// Increment the component count
	dialogTemplate->cdit++;
}

void CDialogTemplate::AddButton(const char* caption, DWORD style, DWORD exStyle, int x, int y,
								int w, int h, WORD id)
{
	AddStandardComponent(0x0080, caption, style, exStyle, x, y, w, h, id);

	WORD creationDataLength = 0;
	AppendData(&creationDataLength, sizeof(WORD));
}

void CDialogTemplate::AddEditBox(const char* caption, DWORD style, DWORD exStyle, int x, int y,
								 int w, int h, WORD id)
{
	AddStandardComponent(0x0081, caption, style, exStyle, x, y, w, h, id);

	WORD creationDataLength = 0;
	AppendData(&creationDataLength, sizeof(WORD));
}

void CDialogTemplate::AddStatic(const char* caption, DWORD style, DWORD exStyle, int x, int y,
								int w, int h, WORD id)
{
	AddStandardComponent(0x0082, caption, style, exStyle, x, y, w, h, id);

	WORD creationDataLength = 0;
	AppendData(&creationDataLength, sizeof(WORD));
}

void CDialogTemplate::AddListBox(const char* caption, DWORD style, DWORD exStyle, int x, int y,
								 int w, int h, WORD id)
{
	AddStandardComponent(0x0083, caption, style, exStyle, x, y, w, h, id);

	WORD creationDataLength = 0;
	AppendData(&creationDataLength, sizeof(WORD));
}

void CDialogTemplate::AddScrollBar(const char* caption, DWORD style, DWORD exStyle, int x, int y,
								   int w, int h, WORD id)
{
	AddStandardComponent(0x0084, caption, style, exStyle, x, y, w, h, id);

	WORD creationDataLength = 0;
	AppendData(&creationDataLength, sizeof(WORD));
}

void CDialogTemplate::AddComboBox(const char* caption, DWORD style, DWORD exStyle, int x, int y,
								  int w, int h, WORD id)
{
	AddStandardComponent(0x0085, caption, style, exStyle, x, y, w, h, id);

	WORD creationDataLength = 0;
	AppendData(&creationDataLength, sizeof(WORD));
}

DLGTEMPLATE* CDialogTemplate::GetDialogTemplate() const
{
	return dialogTemplate;
}

void CDialogTemplate::AddStandardComponent(WORD type, const char* caption, DWORD style,
										   DWORD exStyle, int x, int y, int w, int h, WORD id)
{
	DLGITEMTEMPLATE item;

	// DWORD algin the beginning of the component data
	AlignData(sizeof(DWORD));

	item.style = style;
	item.x     = static_cast<short>(x);
	item.y     = static_cast<short>(y);
	item.cx    = static_cast<short>(w);
	item.cy    = static_cast<short>(h);
	item.id    = id;
	item.dwExtendedStyle = exStyle;

	AppendData(&item, sizeof(DLGITEMTEMPLATE));

	WORD preType = 0xFFFF;

	AppendData(&preType, sizeof(WORD));
	AppendData(&type, sizeof(WORD));

	AppendString(caption);

	// Increment the component count
	dialogTemplate->cdit++;
}

void CDialogTemplate::AlignData(int size)
{
	int paddingSize = usedBufferLength % size;

	if (paddingSize != 0)
	{
		EnsureSpace(paddingSize);
		usedBufferLength += paddingSize;
	}
}

void CDialogTemplate::AppendString(const char* string)
{
	int length = MultiByteToWideChar(CP_ACP, 0, string, -1, nullptr, 0);

	WCHAR* wideString = (WCHAR*)malloc(sizeof(WCHAR) * length);
	MultiByteToWideChar(CP_ACP, 0, string, -1, wideString, length);

	AppendData(wideString, length * sizeof(WCHAR));
	free(wideString);
}

void CDialogTemplate::AppendData(const void* data, int dataLength)
{
	EnsureSpace(dataLength);

	memcpy((char*)dialogTemplate + usedBufferLength, data, dataLength);
	usedBufferLength += dataLength;
}

void CDialogTemplate::EnsureSpace(int length)
{
	if (length + usedBufferLength > totalBufferLength)
	{
		totalBufferLength += length * 2;

		void* newBuffer = malloc(totalBufferLength);
		memcpy(newBuffer, dialogTemplate, usedBufferLength);

		free(dialogTemplate);
		dialogTemplate = (DLGTEMPLATE*)newBuffer;
	}
}

#endif // WIN32