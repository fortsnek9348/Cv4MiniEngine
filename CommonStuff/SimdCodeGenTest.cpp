// This is just a temporary file with assembler output enabled, like compiler explorer.

#include "inc/CommonStuff/SimdConversion.h"
#include "inc/CommonStuff/SimdConversionsAvx.h"
#include "inc/CommonStuff/SimdRegOpsAvx128.h"
#include "inc/CommonStuff/Simd.h"

using namespace heck::simd;

Vector<int32_t, 32, Avx256IntegerReg> HECK_VECTORCALL foo(Vector<int16_t, 32, Avx256IntegerReg> v)
{
	//genericConvertRegArray<Avx256IntegerReg, int32_t, 2, int32_t, Avx512IntegerReg, 1>({});

	//RegOps<Avx128IntegerReg, int32_t>::vtest({}, {});

	return Vector<int32_t, 32, Avx256IntegerReg>{ v };
}
