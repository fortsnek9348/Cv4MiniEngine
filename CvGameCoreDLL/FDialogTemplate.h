#pragma once

#ifndef CDIALOGTEMPLATE_H
#define CDIALOGTEMPLATE_H

// fortsnek: _WIN32 instead of WIN32.
#if defined(_WIN32) // only makes sense on win32

#include <windows.h>	// This is needed just for the const char*, DWORD, and WORD typedefs
						// As a rule, I hate including windows.h in headers


// This class is a convenient wrapper around DLGTEMPLATE.  This can be used to
// algorithmically create dialog boxes as opposed to creating them in a resource file.
// The resource file method should be preferred, but that isn't possible for static libs.
// Example uses for this--custom asserts, custom exception handlers, d3d device selection.
// Basically dialog boxes you'd like to put into static libs and use in many projects
class CDialogTemplate
{
public:
	CDialogTemplate(const char* caption, DWORD style, int x, int y, int w, int h,
		const char* font = nullptr, WORD fontSize = 8);

	~CDialogTemplate();

	void AddComponent(const char* type, const char* caption, DWORD style, DWORD exStyle,
		int x, int y, int w, int h, WORD id);

	void AddButton(const char* caption, DWORD style, DWORD exStyle, int x, int y,
		int w, int h, WORD id);

	void AddEditBox(const char* caption, DWORD style, DWORD exStyle, int x, int y,
		int w, int h, WORD id);

	void AddStatic(const char* caption, DWORD style, DWORD exStyle, int x, int y,
		int w, int h, WORD id);

	void AddListBox(const char* caption, DWORD style, DWORD exStyle, int x, int y,
		int w, int h, WORD id);

	void AddScrollBar(const char* caption, DWORD style, DWORD exStyle, int x, int y,
		int w, int h, WORD id);

	void AddComboBox(const char* caption, DWORD style, DWORD exStyle, int x, int y,
		int w, int h, WORD id);

	DLGTEMPLATE* GetDialogTemplate() const;

private:
	// disable copy and assignment
	CDialogTemplate( const CDialogTemplate& );
	CDialogTemplate& operator=( const CDialogTemplate& );

	void AddStandardComponent(WORD type, const char* caption, DWORD style,
		DWORD exStyle, int x, int y, int w, int h, WORD id);

	void AlignData(int size);
	void AppendString(const char* string);
	void AppendData(const void* data, int dataLength);
	void EnsureSpace(int length);

	DLGTEMPLATE* dialogTemplate;

	int totalBufferLength;
	int usedBufferLength;
};

#endif // WIN32

#endif // CDIALOGTEMPLATE_H
