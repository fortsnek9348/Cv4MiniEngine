#include "inc/CommonStuff/SimdVector.h"
#include "inc/CommonStuff/SimdOpsAvx512Int32.h"

using namespace heck;
using namespace heck::simd;

static void transpose_AVX512_16x16(std::span<uint32_t, 16 * 16> matT, std::span<const uint32_t, 16 * 16> mat)
{
	// https://stackoverflow.com/questions/29519222/how-to-transpose-a-16x16-matrix-using-simd-instructions#

	alignas(__m512i) static constexpr int64_t idx1[8] = { 2, 3, 0, 1, 6, 7, 4, 5 };
	alignas(__m512i) static constexpr int64_t idx2[8] = { 1, 0, 3, 2, 5, 4, 7, 6 };
	alignas(__m512i) static constexpr int32_t idx3[16] = { 1, 0, 3, 2, 5 ,4 ,7 ,6 ,9 ,8 , 11, 10, 13, 12 ,15, 14 };
	__m512i vidx1 = _mm512_load_epi64(idx1);
	__m512i vidx2 = _mm512_load_epi64(idx2);
	__m512i vidx3 = _mm512_load_epi32(idx3);

	__m512i t0 = _mm512_inserti64x4(_mm512_castsi256_si512(_mm256_load_si256((__m256i*)&mat[0*16+0])), _mm256_load_si256((__m256i*)&mat[8*16+0]), 1);
	__m512i t1 = _mm512_inserti64x4(_mm512_castsi256_si512(_mm256_load_si256((__m256i*)&mat[1*16+0])), _mm256_load_si256((__m256i*)&mat[9*16+0]), 1);
	__m512i t2 = _mm512_inserti64x4(_mm512_castsi256_si512(_mm256_load_si256((__m256i*)&mat[2*16+0])), _mm256_load_si256((__m256i*)&mat[10*16+0]), 1);
	__m512i t3 = _mm512_inserti64x4(_mm512_castsi256_si512(_mm256_load_si256((__m256i*)&mat[3*16+0])), _mm256_load_si256((__m256i*)&mat[11*16+0]), 1);
	__m512i t4 = _mm512_inserti64x4(_mm512_castsi256_si512(_mm256_load_si256((__m256i*)&mat[4*16+0])), _mm256_load_si256((__m256i*)&mat[12*16+0]), 1);
	__m512i t5 = _mm512_inserti64x4(_mm512_castsi256_si512(_mm256_load_si256((__m256i*)&mat[5*16+0])), _mm256_load_si256((__m256i*)&mat[13*16+0]), 1);
	__m512i t6 = _mm512_inserti64x4(_mm512_castsi256_si512(_mm256_load_si256((__m256i*)&mat[6*16+0])), _mm256_load_si256((__m256i*)&mat[14*16+0]), 1);
	__m512i t7 = _mm512_inserti64x4(_mm512_castsi256_si512(_mm256_load_si256((__m256i*)&mat[7*16+0])), _mm256_load_si256((__m256i*)&mat[15*16+0]), 1);

	__m512i t8 = _mm512_inserti64x4(_mm512_castsi256_si512(_mm256_load_si256((__m256i*)&mat[0*16+8])), _mm256_load_si256((__m256i*)&mat[8*16+8]), 1);
	__m512i t9 = _mm512_inserti64x4(_mm512_castsi256_si512(_mm256_load_si256((__m256i*)&mat[1*16+8])), _mm256_load_si256((__m256i*)&mat[9*16+8]), 1);
	__m512i ta = _mm512_inserti64x4(_mm512_castsi256_si512(_mm256_load_si256((__m256i*)&mat[2*16+8])), _mm256_load_si256((__m256i*)&mat[10*16+8]), 1);
	__m512i tb = _mm512_inserti64x4(_mm512_castsi256_si512(_mm256_load_si256((__m256i*)&mat[3*16+8])), _mm256_load_si256((__m256i*)&mat[11*16+8]), 1);
	__m512i tc = _mm512_inserti64x4(_mm512_castsi256_si512(_mm256_load_si256((__m256i*)&mat[4*16+8])), _mm256_load_si256((__m256i*)&mat[12*16+8]), 1);
	__m512i td = _mm512_inserti64x4(_mm512_castsi256_si512(_mm256_load_si256((__m256i*)&mat[5*16+8])), _mm256_load_si256((__m256i*)&mat[13*16+8]), 1);
	__m512i te = _mm512_inserti64x4(_mm512_castsi256_si512(_mm256_load_si256((__m256i*)&mat[6*16+8])), _mm256_load_si256((__m256i*)&mat[14*16+8]), 1);
	__m512i tf = _mm512_inserti64x4(_mm512_castsi256_si512(_mm256_load_si256((__m256i*)&mat[7*16+8])), _mm256_load_si256((__m256i*)&mat[15*16+8]), 1);

	__m512i r0 = _mm512_mask_permutexvar_epi64(t0, 0xcc, vidx1, t4);
	__m512i r1 = _mm512_mask_permutexvar_epi64(t1, 0xcc, vidx1, t5);
	__m512i r2 = _mm512_mask_permutexvar_epi64(t2, 0xcc, vidx1, t6);
	__m512i r3 = _mm512_mask_permutexvar_epi64(t3, 0xcc, vidx1, t7);
	__m512i r8 = _mm512_mask_permutexvar_epi64(t8, 0xcc, vidx1, tc);
	__m512i r9 = _mm512_mask_permutexvar_epi64(t9, 0xcc, vidx1, td);
	__m512i ra = _mm512_mask_permutexvar_epi64(ta, 0xcc, vidx1, te);
	__m512i rb = _mm512_mask_permutexvar_epi64(tb, 0xcc, vidx1, tf);

	__m512i r4 = _mm512_mask_permutexvar_epi64(t4, 0x33, vidx1, t0);
	__m512i r5 = _mm512_mask_permutexvar_epi64(t5, 0x33, vidx1, t1);
	__m512i r6 = _mm512_mask_permutexvar_epi64(t6, 0x33, vidx1, t2);
	__m512i r7 = _mm512_mask_permutexvar_epi64(t7, 0x33, vidx1, t3);
	__m512i rc = _mm512_mask_permutexvar_epi64(tc, 0x33, vidx1, t8);
	__m512i rd = _mm512_mask_permutexvar_epi64(td, 0x33, vidx1, t9);
	__m512i re = _mm512_mask_permutexvar_epi64(te, 0x33, vidx1, ta);
	__m512i rf = _mm512_mask_permutexvar_epi64(tf, 0x33, vidx1, tb);

	t0 = _mm512_mask_permutexvar_epi64(r0, 0xaa, vidx2, r2);
	t1 = _mm512_mask_permutexvar_epi64(r1, 0xaa, vidx2, r3);
	t4 = _mm512_mask_permutexvar_epi64(r4, 0xaa, vidx2, r6);
	t5 = _mm512_mask_permutexvar_epi64(r5, 0xaa, vidx2, r7);
	t8 = _mm512_mask_permutexvar_epi64(r8, 0xaa, vidx2, ra);
	t9 = _mm512_mask_permutexvar_epi64(r9, 0xaa, vidx2, rb);
	tc = _mm512_mask_permutexvar_epi64(rc, 0xaa, vidx2, re);
	td = _mm512_mask_permutexvar_epi64(rd, 0xaa, vidx2, rf);

	t2 = _mm512_mask_permutexvar_epi64(r2, 0x55, vidx2, r0);
	t3 = _mm512_mask_permutexvar_epi64(r3, 0x55, vidx2, r1);
	t6 = _mm512_mask_permutexvar_epi64(r6, 0x55, vidx2, r4);
	t7 = _mm512_mask_permutexvar_epi64(r7, 0x55, vidx2, r5);
	ta = _mm512_mask_permutexvar_epi64(ra, 0x55, vidx2, r8);
	tb = _mm512_mask_permutexvar_epi64(rb, 0x55, vidx2, r9);
	te = _mm512_mask_permutexvar_epi64(re, 0x55, vidx2, rc);
	tf = _mm512_mask_permutexvar_epi64(rf, 0x55, vidx2, rd);

	r0 = _mm512_mask_permutexvar_epi32(t0, 0xaaaa, vidx3, t1);
	r2 = _mm512_mask_permutexvar_epi32(t2, 0xaaaa, vidx3, t3);
	r4 = _mm512_mask_permutexvar_epi32(t4, 0xaaaa, vidx3, t5);
	r6 = _mm512_mask_permutexvar_epi32(t6, 0xaaaa, vidx3, t7);
	r8 = _mm512_mask_permutexvar_epi32(t8, 0xaaaa, vidx3, t9);
	ra = _mm512_mask_permutexvar_epi32(ta, 0xaaaa, vidx3, tb);
	rc = _mm512_mask_permutexvar_epi32(tc, 0xaaaa, vidx3, td);
	re = _mm512_mask_permutexvar_epi32(te, 0xaaaa, vidx3, tf);

	r1 = _mm512_mask_permutexvar_epi32(t1, 0x5555, vidx3, t0);
	r3 = _mm512_mask_permutexvar_epi32(t3, 0x5555, vidx3, t2);
	r5 = _mm512_mask_permutexvar_epi32(t5, 0x5555, vidx3, t4);
	r7 = _mm512_mask_permutexvar_epi32(t7, 0x5555, vidx3, t6);
	r9 = _mm512_mask_permutexvar_epi32(t9, 0x5555, vidx3, t8);
	rb = _mm512_mask_permutexvar_epi32(tb, 0x5555, vidx3, ta);
	rd = _mm512_mask_permutexvar_epi32(td, 0x5555, vidx3, tc);
	rf = _mm512_mask_permutexvar_epi32(tf, 0x5555, vidx3, te);

	_mm512_store_epi32(&matT[0*16], r0);
	_mm512_store_epi32(&matT[1*16], r1);
	_mm512_store_epi32(&matT[2*16], r2);
	_mm512_store_epi32(&matT[3*16], r3);
	_mm512_store_epi32(&matT[4*16], r4);
	_mm512_store_epi32(&matT[5*16], r5);
	_mm512_store_epi32(&matT[6*16], r6);
	_mm512_store_epi32(&matT[7*16], r7);
	_mm512_store_epi32(&matT[8*16], r8);
	_mm512_store_epi32(&matT[9*16], r9);
	_mm512_store_epi32(&matT[10*16], ra);
	_mm512_store_epi32(&matT[11*16], rb);
	_mm512_store_epi32(&matT[12*16], rc);
	_mm512_store_epi32(&matT[13*16], rd);
	_mm512_store_epi32(&matT[14*16], re);
	_mm512_store_epi32(&matT[15*16], rf);
}

//static void transpose_AVX2_8x8(std::span<uint32_t, 8 * 8> matT, std::span<const uint32_t, 8 * 8> mat)
//{
//	// https://stackoverflow.com/questions/25622745/transpose-an-8x8-float-using-avx-avx2
//
//	__m256  r0, r1, r2, r3, r4, r5, r6, r7;
//	__m256  t0, t1, t2, t3, t4, t5, t6, t7;
//
//	r0 = _mm256_insertf128_ps(_mm256_castps128_ps256(_mm_load_ps(reinterpret_cast<const float*>(&mat[0*8+0]))), _mm_load_ps(reinterpret_cast<const float*>(&mat[4*8+0])), 1);
//	r1 = _mm256_insertf128_ps(_mm256_castps128_ps256(_mm_load_ps(reinterpret_cast<const float*>(&mat[1*8+0]))), _mm_load_ps(reinterpret_cast<const float*>(&mat[5*8+0])), 1);
//	r2 = _mm256_insertf128_ps(_mm256_castps128_ps256(_mm_load_ps(reinterpret_cast<const float*>(&mat[2*8+0]))), _mm_load_ps(reinterpret_cast<const float*>(&mat[6*8+0])), 1);
//	r3 = _mm256_insertf128_ps(_mm256_castps128_ps256(_mm_load_ps(reinterpret_cast<const float*>(&mat[3*8+0]))), _mm_load_ps(reinterpret_cast<const float*>(&mat[7*8+0])), 1);
//	r4 = _mm256_insertf128_ps(_mm256_castps128_ps256(_mm_load_ps(reinterpret_cast<const float*>(&mat[0*8+4]))), _mm_load_ps(reinterpret_cast<const float*>(&mat[4*8+4])), 1);
//	r5 = _mm256_insertf128_ps(_mm256_castps128_ps256(_mm_load_ps(reinterpret_cast<const float*>(&mat[1*8+4]))), _mm_load_ps(reinterpret_cast<const float*>(&mat[5*8+4])), 1);
//	r6 = _mm256_insertf128_ps(_mm256_castps128_ps256(_mm_load_ps(reinterpret_cast<const float*>(&mat[2*8+4]))), _mm_load_ps(reinterpret_cast<const float*>(&mat[6*8+4])), 1);
//	r7 = _mm256_insertf128_ps(_mm256_castps128_ps256(_mm_load_ps(reinterpret_cast<const float*>(&mat[3*8+4]))), _mm_load_ps(reinterpret_cast<const float*>(&mat[7*8+4])), 1);
//
//	t0 = _mm256_unpacklo_ps(r0, r1);
//	t1 = _mm256_unpackhi_ps(r0, r1);
//	t2 = _mm256_unpacklo_ps(r2, r3);
//	t3 = _mm256_unpackhi_ps(r2, r3);
//	t4 = _mm256_unpacklo_ps(r4, r5);
//	t5 = _mm256_unpackhi_ps(r4, r5);
//	t6 = _mm256_unpacklo_ps(r6, r7);
//	t7 = _mm256_unpackhi_ps(r6, r7);
//
//	__m256 v;
//
//	//r0 = _mm256_shuffle_ps(t0,t2, 0x44);
//	//r1 = _mm256_shuffle_ps(t0,t2, 0xEE);  
//	v = _mm256_shuffle_ps(t0, t2, 0x4E);
//	r0 = _mm256_blend_ps(t0, v, 0xCC);
//	r1 = _mm256_blend_ps(t2, v, 0x33);
//
//	//r2 = _mm256_shuffle_ps(t1,t3, 0x44);
//	//r3 = _mm256_shuffle_ps(t1,t3, 0xEE);
//	v = _mm256_shuffle_ps(t1, t3, 0x4E);
//	r2 = _mm256_blend_ps(t1, v, 0xCC);
//	r3 = _mm256_blend_ps(t3, v, 0x33);
//
//	//r4 = _mm256_shuffle_ps(t4,t6, 0x44);
//	//r5 = _mm256_shuffle_ps(t4,t6, 0xEE);
//	v = _mm256_shuffle_ps(t4, t6, 0x4E);
//	r4 = _mm256_blend_ps(t4, v, 0xCC);
//	r5 = _mm256_blend_ps(t6, v, 0x33);
//
//	//r6 = _mm256_shuffle_ps(t5,t7, 0x44);
//	//r7 = _mm256_shuffle_ps(t5,t7, 0xEE);
//	v = _mm256_shuffle_ps(t5, t7, 0x4E);
//	r6 = _mm256_blend_ps(t5, v, 0xCC);
//	r7 = _mm256_blend_ps(t7, v, 0x33);
//
//	_mm256_store_ps(reinterpret_cast<float*>(&matT[0*8]), r0);
//	_mm256_store_ps(reinterpret_cast<float*>(&matT[1*8]), r1);
//	_mm256_store_ps(reinterpret_cast<float*>(&matT[2*8]), r2);
//	_mm256_store_ps(reinterpret_cast<float*>(&matT[3*8]), r3);
//	_mm256_store_ps(reinterpret_cast<float*>(&matT[4*8]), r4);
//	_mm256_store_ps(reinterpret_cast<float*>(&matT[5*8]), r5);
//	_mm256_store_ps(reinterpret_cast<float*>(&matT[6*8]), r6);
//	_mm256_store_ps(reinterpret_cast<float*>(&matT[7*8]), r7);
//}

std::array<Vector<int32_t, 16>, 16> simd::transposed(std::span<const Vector<int32_t, 16>, 16> matrix)
{
	std::array<Vector<int32_t, 16>, 16> a;
	transpose_AVX512_16x16(std::span(reinterpret_cast<uint32_t*>(&a), 16*16).subspan<0, 16*16>(), 
		std::span(reinterpret_cast<const uint32_t*>(matrix.data()), 16*16).subspan<0, 16*16>());
	return a;
}
std::array<Vector<uint32_t, 16>, 16> simd::transposed(std::span<const Vector<uint32_t, 16>, 16> matrix)
{
	std::array<Vector<uint32_t, 16>, 16> a;
	transpose_AVX512_16x16(std::span(reinterpret_cast<uint32_t*>(&a), 16*16).subspan<0, 16*16>(),
		std::span(reinterpret_cast<const uint32_t*>(matrix.data()), 16*16).subspan<0, 16*16>());
	return a;
}