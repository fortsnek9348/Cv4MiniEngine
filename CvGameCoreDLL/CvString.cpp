#include "CvString.h"

#include <CommonStuff/StringConversion.h>

#include <stdexcept>

CvString convertToAscii(const CvWString& s)
{
	return heck::toAsciiString(s);
}