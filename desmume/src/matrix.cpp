/*
	Copyright (C) 2006-2007 shash
	Copyright (C) 2007-2022 DeSmuME team

	This file is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.

	This file is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with the this software.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>
#include "matrix.h"
#include "MMU.h"

// NDS matrix math functions uses 20.12 fixed-point for calculations. According to
// GEM_TransformVertex(), dot product calculations use accumulation that goes beyond
// 32-bits and then saturates. Therefore, all fixed-point math functions will also
// support that feature here.
//
// But for historical reasons, we can't enable this right away. Therefore, the scalar
// function GEM_TransformVertex() will continue to be used for SetVertex() while these
// fixed-point functions will remain as they are. In order to document the future
// intent of the fixed-point functions while retaining the existing functionality, the
// saturate code will be hidden by this macro.
//
// Testing is highly encouraged! Simply uncomment to try out this feature.
//#define FIXED_POINT_MATH_FUNCTIONS_USE_ACCUMULATOR_SATURATE


// The following floating-point functions exist for historical reasons and are deprecated.
// They should be obsoleted and removed as more of the geometry engine moves to fixed-point.
static FORCEINLINE void __mtx4_copy_mtx4_float(float (&__restrict outMtx)[16], const s32 (&__restrict inMtx)[16])
{
	outMtx[ 0] = (float)inMtx[ 0];
	outMtx[ 1] = (float)inMtx[ 1];
	outMtx[ 2] = (float)inMtx[ 2];
	outMtx[ 3] = (float)inMtx[ 3];
	
	outMtx[ 4] = (float)inMtx[ 4];
	outMtx[ 5] = (float)inMtx[ 5];
	outMtx[ 6] = (float)inMtx[ 6];
	outMtx[ 7] = (float)inMtx[ 7];
	
	outMtx[ 8] = (float)inMtx[ 8];
	outMtx[ 9] = (float)inMtx[ 9];
	outMtx[10] = (float)inMtx[10];
	outMtx[11] = (float)inMtx[11];
	
	outMtx[12] = (float)inMtx[12];
	outMtx[13] = (float)inMtx[13];
	outMtx[14] = (float)inMtx[14];
	outMtx[15] = (float)inMtx[15];
}

static FORCEINLINE void __mtx4_copynormalize_mtx4_float(float (&__restrict outMtx)[16], const s32 (&__restrict inMtx)[16])
{
	outMtx[ 0] = (float)inMtx[ 0] / 4096.0f;
	outMtx[ 1] = (float)inMtx[ 1] / 4096.0f;
	outMtx[ 2] = (float)inMtx[ 2] / 4096.0f;
	outMtx[ 3] = (float)inMtx[ 3] / 4096.0f;
	
	outMtx[ 4] = (float)inMtx[ 4] / 4096.0f;
	outMtx[ 5] = (float)inMtx[ 5] / 4096.0f;
	outMtx[ 6] = (float)inMtx[ 6] / 4096.0f;
	outMtx[ 7] = (float)inMtx[ 7] / 4096.0f;
	
	outMtx[ 8] = (float)inMtx[ 8] / 4096.0f;
	outMtx[ 9] = (float)inMtx[ 9] / 4096.0f;
	outMtx[10] = (float)inMtx[10] / 4096.0f;
	outMtx[11] = (float)inMtx[11] / 4096.0f;
	
	outMtx[12] = (float)inMtx[12] / 4096.0f;
	outMtx[13] = (float)inMtx[13] / 4096.0f;
	outMtx[14] = (float)inMtx[14] / 4096.0f;
	outMtx[15] = (float)inMtx[15] / 4096.0f;
}

static FORCEINLINE float __vec4_dotproduct_vec4_float(const float (&__restrict vecA)[4], const float (&__restrict vecB)[4])
{
	return (vecA[0] * vecB[0]) + (vecA[1] * vecB[1]) + (vecA[2] * vecB[2]) + (vecA[3] * vecB[3]);
}

static FORCEINLINE void __vec4_multiply_mtx4_float(float (&__restrict inoutVec)[4], const s32 (&__restrict inMtx)[16])
{
	const CACHE_ALIGN float v[4] = {inoutVec[0], inoutVec[1], inoutVec[2], inoutVec[3]};
	
	CACHE_ALIGN float m[16];
	__mtx4_copynormalize_mtx4_float(m, inMtx);
	
	inoutVec[0] = (m[0] * v[0]) + (m[4] * v[1]) + (m[ 8] * v[2]) + (m[12] * v[3]);
	inoutVec[1] = (m[1] * v[0]) + (m[5] * v[1]) + (m[ 9] * v[2]) + (m[13] * v[3]);
	inoutVec[2] = (m[2] * v[0]) + (m[6] * v[1]) + (m[10] * v[2]) + (m[14] * v[3]);
	inoutVec[3] = (m[3] * v[0]) + (m[7] * v[1]) + (m[11] * v[2]) + (m[15] * v[3]);
}

static FORCEINLINE void __vec3_multiply_mtx3_float(float (&__restrict inoutVec)[4], const s32 (&__restrict inMtx)[16])
{
	const CACHE_ALIGN float v[4] = {inoutVec[0], inoutVec[1], inoutVec[2], inoutVec[3]};
	
	CACHE_ALIGN float m[16];
	__mtx4_copynormalize_mtx4_float(m, inMtx);
	
	inoutVec[0] = (m[0] * v[0]) + (m[4] * v[1]) + (m[ 8] * v[2]);
	inoutVec[1] = (m[1] * v[0]) + (m[5] * v[1]) + (m[ 9] * v[2]);
	inoutVec[2] = (m[2] * v[0]) + (m[6] * v[1]) + (m[10] * v[2]);
	inoutVec[3] = v[3];
}

static FORCEINLINE void __mtx4_multiply_mtx4_float(float (&__restrict mtxA)[16], const s32 (&__restrict mtxB)[16])
{
	CACHE_ALIGN float a[16];
	CACHE_ALIGN float b[16];
	
	MatrixCopy(a, mtxA);
	
	// Can't call normal MatrixCopy() because the types would cause mtxB to become normalized.
	// So instead, we need to call __mtx4_copy_mtx4_float() directly to copy the unmodified
	// matrix values.
	__mtx4_copy_mtx4_float(b, mtxB);
	
	mtxA[ 0] = (a[ 0] * b[ 0]) + (a[ 4] * b[ 1]) + (a[ 8] * b[ 2]) + (a[12] * b[ 3]);
	mtxA[ 1] = (a[ 1] * b[ 0]) + (a[ 5] * b[ 1]) + (a[ 9] * b[ 2]) + (a[13] * b[ 3]);
	mtxA[ 2] = (a[ 2] * b[ 0]) + (a[ 6] * b[ 1]) + (a[10] * b[ 2]) + (a[14] * b[ 3]);
	mtxA[ 3] = (a[ 3] * b[ 0]) + (a[ 7] * b[ 1]) + (a[11] * b[ 2]) + (a[15] * b[ 3]);
	
	mtxA[ 4] = (a[ 0] * b[ 4]) + (a[ 4] * b[ 5]) + (a[ 8] * b[ 6]) + (a[12] * b[ 7]);
	mtxA[ 5] = (a[ 1] * b[ 4]) + (a[ 5] * b[ 5]) + (a[ 9] * b[ 6]) + (a[13] * b[ 7]);
	mtxA[ 6] = (a[ 2] * b[ 4]) + (a[ 6] * b[ 5]) + (a[10] * b[ 6]) + (a[14] * b[ 7]);
	mtxA[ 7] = (a[ 3] * b[ 4]) + (a[ 7] * b[ 5]) + (a[11] * b[ 6]) + (a[15] * b[ 7]);
	
	mtxA[ 8] = (a[ 0] * b[ 8]) + (a[ 4] * b[ 9]) + (a[ 8] * b[10]) + (a[12] * b[11]);
	mtxA[ 9] = (a[ 1] * b[ 8]) + (a[ 5] * b[ 9]) + (a[ 9] * b[10]) + (a[13] * b[11]);
	mtxA[10] = (a[ 2] * b[ 8]) + (a[ 6] * b[ 9]) + (a[10] * b[10]) + (a[14] * b[11]);
	mtxA[11] = (a[ 3] * b[ 8]) + (a[ 7] * b[ 9]) + (a[11] * b[10]) + (a[15] * b[11]);
	
	mtxA[12] = (a[ 0] * b[12]) + (a[ 4] * b[13]) + (a[ 8] * b[14]) + (a[12] * b[15]);
	mtxA[13] = (a[ 1] * b[12]) + (a[ 5] * b[13]) + (a[ 9] * b[14]) + (a[13] * b[15]);
	mtxA[14] = (a[ 2] * b[12]) + (a[ 6] * b[13]) + (a[10] * b[14]) + (a[14] * b[15]);
	mtxA[15] = (a[ 3] * b[12]) + (a[ 7] * b[13]) + (a[11] * b[14]) + (a[15] * b[15]);
}

static FORCEINLINE void __mtx4_scale_vec3_float(float (&__restrict inoutMtx)[16], const float (&__restrict inVec)[4])
{
	inoutMtx[ 0] *= inVec[0];
	inoutMtx[ 1] *= inVec[0];
	inoutMtx[ 2] *= inVec[0];
	inoutMtx[ 3] *= inVec[0];
	
	inoutMtx[ 4] *= inVec[1];
	inoutMtx[ 5] *= inVec[1];
	inoutMtx[ 6] *= inVec[1];
	inoutMtx[ 7] *= inVec[1];
	
	inoutMtx[ 8] *= inVec[2];
	inoutMtx[ 9] *= inVec[2];
	inoutMtx[10] *= inVec[2];
	inoutMtx[11] *= inVec[2];
}

static FORCEINLINE void __mtx4_translate_vec3_float(float (&__restrict inoutMtx)[16], const float (&__restrict inVec)[4])
{
	inoutMtx[12] = (inoutMtx[0] * inVec[0]) + (inoutMtx[4] * inVec[1]) + (inoutMtx[ 8] * inVec[2]);
	inoutMtx[13] = (inoutMtx[1] * inVec[0]) + (inoutMtx[5] * inVec[1]) + (inoutMtx[ 9] * inVec[2]);
	inoutMtx[14] = (inoutMtx[2] * inVec[0]) + (inoutMtx[6] * inVec[1]) + (inoutMtx[10] * inVec[2]);
	inoutMtx[15] = (inoutMtx[3] * inVec[0]) + (inoutMtx[7] * inVec[1]) + (inoutMtx[11] * inVec[2]);
}

// These SIMD functions may look fancy, but they still operate using floating-point, and therefore
// need to be obsoleted and removed. They exist for historical reasons, one of which is that they
// run on very old CPUs through plain ol' SSE. However, future geometry engine work will only be
// moving towards using the native NDS 20.12 fixed-point math, and so the fixed-point equivalent
// functions shall take precendence over these.

#ifdef ENABLE_SSE

#ifdef ENABLE_SSE2
static FORCEINLINE void __mtx4_copy_mtx4_float_SSE2(float (&__restrict outMtx)[16], const s32 (&__restrict inMtx)[16])
{
	_mm_store_ps( outMtx + 0, _mm_cvtepi32_ps( _mm_load_si128((v128s32 *)inMtx+0) ) );
	_mm_store_ps( outMtx + 4, _mm_cvtepi32_ps( _mm_load_si128((v128s32 *)inMtx+1) ) );
	_mm_store_ps( outMtx + 8, _mm_cvtepi32_ps( _mm_load_si128((v128s32 *)inMtx+2) ) );
	_mm_store_ps( outMtx +12, _mm_cvtepi32_ps( _mm_load_si128((v128s32 *)inMtx+3) ) );
}
#endif // ENABLE_SSE2

static FORCEINLINE void __mtx4_copynormalize_mtx4_float_SSE(float (&__restrict outMtx)[16], const s32 (&__restrict inMtx)[16])
{
#ifdef ENABLE_SSE2
	__m128 row[4] = {
		_mm_cvtepi32_ps( _mm_load_si128((v128s32 *)inMtx+0) ),
		_mm_cvtepi32_ps( _mm_load_si128((v128s32 *)inMtx+1) ),
		_mm_cvtepi32_ps( _mm_load_si128((v128s32 *)inMtx+2) ),
		_mm_cvtepi32_ps( _mm_load_si128((v128s32 *)inMtx+3) )
	};
#else
	__m128 row[4] = {
		_mm_setr_ps((float)inMtx[ 0], (float)inMtx[ 1], (float)inMtx[ 2], (float)inMtx[ 3]),
		_mm_setr_ps((float)inMtx[ 4], (float)inMtx[ 5], (float)inMtx[ 6], (float)inMtx[ 7]),
		_mm_setr_ps((float)inMtx[ 8], (float)inMtx[ 9], (float)inMtx[10], (float)inMtx[11]),
		_mm_setr_ps((float)inMtx[12], (float)inMtx[13], (float)inMtx[14], (float)inMtx[15])
	};
#endif // ENABLE_SSE2
	
	const __m128 normalize = _mm_set1_ps(1.0f/4096.0f);
	
	row[0] = _mm_mul_ps(row[0], normalize);
	_mm_store_ps(outMtx + 0, row[0]);
	row[1] = _mm_mul_ps(row[1], normalize);
	_mm_store_ps(outMtx + 4, row[1]);
	row[2] = _mm_mul_ps(row[2], normalize);
	_mm_store_ps(outMtx + 8, row[2]);
	row[3] = _mm_mul_ps(row[3], normalize);
	_mm_store_ps(outMtx +12, row[3]);
}

#ifdef ENABLE_SSE4_1
static FORCEINLINE float __vec4_dotproduct_vec4_float_SSE4(const float (&__restrict vecA)[4], const float (&__restrict vecB)[4])
{
	const __m128 a = _mm_load_ps(vecA);
	const __m128 b = _mm_load_ps(vecB);
	const __m128 sum = _mm_dp_ps(a, b, 0xF1);
	
	return _mm_cvtss_f32(sum);
}
#endif // ENABLE_SSE4_1

static FORCEINLINE void __vec4_multiply_mtx4_float_SSE(float (&__restrict inoutVec)[4], const s32 (&__restrict inMtx)[16])
{
#ifdef ENABLE_SSE2
	__m128 row[4] = {
		_mm_cvtepi32_ps( _mm_load_si128((v128s32 *)inMtx+0) ),
		_mm_cvtepi32_ps( _mm_load_si128((v128s32 *)inMtx+1) ),
		_mm_cvtepi32_ps( _mm_load_si128((v128s32 *)inMtx+2) ),
		_mm_cvtepi32_ps( _mm_load_si128((v128s32 *)inMtx+3) )
	};
#else
	__m128 row[4] = {
		_mm_setr_ps((float)inMtx[ 0], (float)inMtx[ 1], (float)inMtx[ 2], (float)inMtx[ 3]),
		_mm_setr_ps((float)inMtx[ 4], (float)inMtx[ 5], (float)inMtx[ 6], (float)inMtx[ 7]),
		_mm_setr_ps((float)inMtx[ 8], (float)inMtx[ 9], (float)inMtx[10], (float)inMtx[11]),
		_mm_setr_ps((float)inMtx[12], (float)inMtx[13], (float)inMtx[14], (float)inMtx[15])
	};
#endif // ENABLE_SSE2
	
	const __m128 normalize = _mm_set1_ps(1.0f/4096.0f);
	row[0] = _mm_mul_ps(row[0], normalize);
	row[1] = _mm_mul_ps(row[1], normalize);
	row[2] = _mm_mul_ps(row[2], normalize);
	row[3] = _mm_mul_ps(row[3], normalize);
	
	const __m128 inVec = _mm_load_ps(inoutVec);
	const __m128 v[4] = {
		_mm_shuffle_ps(inVec, inVec, 0x00),
		_mm_shuffle_ps(inVec, inVec, 0x55),
		_mm_shuffle_ps(inVec, inVec, 0xAA),
		_mm_shuffle_ps(inVec, inVec, 0xFF)
	};
	
	__m128 outVec;
	outVec =                     _mm_mul_ps(row[0], v[0]);
	outVec = _mm_add_ps( outVec, _mm_mul_ps(row[1], v[1]) );
	outVec = _mm_add_ps( outVec, _mm_mul_ps(row[2], v[2]) );
	outVec = _mm_add_ps( outVec, _mm_mul_ps(row[3], v[3]) );
	
	_mm_store_ps(inoutVec, outVec);
}

static FORCEINLINE void __vec3_multiply_mtx3_float_SSE(float (&__restrict inoutVec)[4], const s32 (&__restrict inMtx)[16])
{
#ifdef ENABLE_SSE2
	__m128 row[3] = {
		_mm_cvtepi32_ps( _mm_load_si128((v128s32 *)inMtx+0) ),
		_mm_cvtepi32_ps( _mm_load_si128((v128s32 *)inMtx+1) ),
		_mm_cvtepi32_ps( _mm_load_si128((v128s32 *)inMtx+2) )
	};
#else
	__m128 row[3] = {
		_mm_setr_ps((float)inMtx[ 0], (float)inMtx[ 1], (float)inMtx[ 2], (float)inMtx[ 3]),
		_mm_setr_ps((float)inMtx[ 4], (float)inMtx[ 5], (float)inMtx[ 6], (float)inMtx[ 7]),
		_mm_setr_ps((float)inMtx[ 8], (float)inMtx[ 9], (float)inMtx[10], (float)inMtx[11])
	};
#endif // ENABLE_SSE2
	
	const __m128 normalize = _mm_set1_ps(1.0f/4096.0f);
	row[0] = _mm_mul_ps(row[0], normalize);
	row[1] = _mm_mul_ps(row[1], normalize);
	row[2] = _mm_mul_ps(row[2], normalize);
	
	const __m128 inVec = _mm_load_ps(inoutVec);
	const __m128 v[3] = {
		_mm_shuffle_ps(inVec, inVec, 0x00),
		_mm_shuffle_ps(inVec, inVec, 0x55),
		_mm_shuffle_ps(inVec, inVec, 0xAA)
	};
	
	__m128 outVec;
	outVec =                     _mm_mul_ps(row[0], v[0]);
	outVec = _mm_add_ps( outVec, _mm_mul_ps(row[1], v[1]) );
	outVec = _mm_add_ps( outVec, _mm_mul_ps(row[2], v[2]) );
	
	_mm_store_ps(inoutVec, outVec);
}

static FORCEINLINE void __mtx4_multiply_mtx4_float_SSE(float (&__restrict mtxA)[16], const s32 (&__restrict mtxB)[16])
{
#ifdef ENABLE_SSE2
	__m128 rowB[4] = {
		_mm_cvtepi32_ps( _mm_load_si128((v128s32 *)mtxB + 0) ),
		_mm_cvtepi32_ps( _mm_load_si128((v128s32 *)mtxB + 1) ),
		_mm_cvtepi32_ps( _mm_load_si128((v128s32 *)mtxB + 2) ),
		_mm_cvtepi32_ps( _mm_load_si128((v128s32 *)mtxB + 3) )
	};
#else
	__m128 rowB[4] = {
		_mm_setr_ps((float)mtxB[ 0], (float)mtxB[ 1], (float)mtxB[ 2], (float)mtxB[ 3]),
		_mm_setr_ps((float)mtxB[ 4], (float)mtxB[ 5], (float)mtxB[ 6], (float)mtxB[ 7]),
		_mm_setr_ps((float)mtxB[ 8], (float)mtxB[ 9], (float)mtxB[10], (float)mtxB[11]),
		_mm_setr_ps((float)mtxB[12], (float)mtxB[13], (float)mtxB[14], (float)mtxB[15])
	};
#endif // ENABLE_SSE2
	
	const __m128 normalize = _mm_set1_ps(1.0f/4096.0f);
	rowB[0] = _mm_mul_ps(rowB[0], normalize);
	rowB[1] = _mm_mul_ps(rowB[1], normalize);
	rowB[2] = _mm_mul_ps(rowB[2], normalize);
	rowB[3] = _mm_mul_ps(rowB[3], normalize);
	
	const __m128 rowA[4] = {
		_mm_load_ps(mtxA + 0),
		_mm_load_ps(mtxA + 4),
		_mm_load_ps(mtxA + 8),
		_mm_load_ps(mtxA +12)
	};
	
	__m128 vecB[4];
	__m128 outRow;
	
	vecB[0] = _mm_shuffle_ps(rowB[0], rowB[0], 0x00);
	vecB[1] = _mm_shuffle_ps(rowB[0], rowB[0], 0x55);
	vecB[2] = _mm_shuffle_ps(rowB[0], rowB[0], 0xAA);
	vecB[3] = _mm_shuffle_ps(rowB[0], rowB[0], 0xFF);
	outRow =                     _mm_mul_ps(rowA[0], vecB[0]);
	outRow = _mm_add_ps( outRow, _mm_mul_ps(rowA[1], vecB[1]) );
	outRow = _mm_add_ps( outRow, _mm_mul_ps(rowA[2], vecB[2]) );
	outRow = _mm_add_ps( outRow, _mm_mul_ps(rowA[3], vecB[3]) );
	_mm_store_ps(mtxA +  0, outRow);
	
	vecB[0] = _mm_shuffle_ps(rowB[1], rowB[1], 0x00);
	vecB[1] = _mm_shuffle_ps(rowB[1], rowB[1], 0x55);
	vecB[2] = _mm_shuffle_ps(rowB[1], rowB[1], 0xAA);
	vecB[3] = _mm_shuffle_ps(rowB[1], rowB[1], 0xFF);
	outRow =                     _mm_mul_ps(rowA[0], vecB[0]);
	outRow = _mm_add_ps( outRow, _mm_mul_ps(rowA[1], vecB[1]) );
	outRow = _mm_add_ps( outRow, _mm_mul_ps(rowA[2], vecB[2]) );
	outRow = _mm_add_ps( outRow, _mm_mul_ps(rowA[3], vecB[3]) );
	_mm_store_ps(mtxA +  4, outRow);
	
	vecB[0] = _mm_shuffle_ps(rowB[2], rowB[2], 0x00);
	vecB[1] = _mm_shuffle_ps(rowB[2], rowB[2], 0x55);
	vecB[2] = _mm_shuffle_ps(rowB[2], rowB[2], 0xAA);
	vecB[3] = _mm_shuffle_ps(rowB[2], rowB[2], 0xFF);
	outRow =                     _mm_mul_ps(rowA[0], vecB[0]);
	outRow = _mm_add_ps( outRow, _mm_mul_ps(rowA[1], vecB[1]) );
	outRow = _mm_add_ps( outRow, _mm_mul_ps(rowA[2], vecB[2]) );
	outRow = _mm_add_ps( outRow, _mm_mul_ps(rowA[3], vecB[3]) );
	_mm_store_ps(mtxA +  8, outRow);
	
	vecB[0] = _mm_shuffle_ps(rowB[3], rowB[3], 0x00);
	vecB[1] = _mm_shuffle_ps(rowB[3], rowB[3], 0x55);
	vecB[2] = _mm_shuffle_ps(rowB[3], rowB[3], 0xAA);
	vecB[3] = _mm_shuffle_ps(rowB[3], rowB[3], 0xFF);
	outRow =                     _mm_mul_ps(rowA[0], vecB[0]);
	outRow = _mm_add_ps( outRow, _mm_mul_ps(rowA[1], vecB[1]) );
	outRow = _mm_add_ps( outRow, _mm_mul_ps(rowA[2], vecB[2]) );
	outRow = _mm_add_ps( outRow, _mm_mul_ps(rowA[3], vecB[3]) );
	_mm_store_ps(mtxA + 12, outRow);
}

static FORCEINLINE void __mtx4_scale_vec3_float_SSE(float (&__restrict inoutMtx)[16], const float (&__restrict inVec)[4])
{
	const __m128 inVec_m128 = _mm_load_ps(inVec);
	const __m128 v[3] = {
		_mm_shuffle_ps(inVec_m128, inVec_m128, 0x00),
		_mm_shuffle_ps(inVec_m128, inVec_m128, 0x55),
		_mm_shuffle_ps(inVec_m128, inVec_m128, 0xAA)
	};
	
	_mm_store_ps( inoutMtx, _mm_mul_ps( _mm_load_ps(inoutMtx+0), v[0] ) );
	_mm_store_ps( inoutMtx, _mm_mul_ps( _mm_load_ps(inoutMtx+4), v[1] ) );
	_mm_store_ps( inoutMtx, _mm_mul_ps( _mm_load_ps(inoutMtx+8), v[2] ) );
}

static FORCEINLINE void __mtx4_translate_vec3_float_SSE(float (&__restrict inoutMtx)[16], const float (&__restrict inVec)[4])
{
	const __m128 inVec_m128 = _mm_load_ps(inVec);
	const __m128 v[3] = {
		_mm_shuffle_ps(inVec_m128, inVec_m128, 0x00),
		_mm_shuffle_ps(inVec_m128, inVec_m128, 0x55),
		_mm_shuffle_ps(inVec_m128, inVec_m128, 0xAA)
	};
	
	const __m128 row[3] = {
		_mm_load_ps(inoutMtx + 0),
		_mm_load_ps(inoutMtx + 4),
		_mm_load_ps(inoutMtx + 8),
	};
	
	__m128 outVec;
	outVec =                     _mm_mul_ps(row[0], v[0]);
	outVec = _mm_add_ps( outVec, _mm_mul_ps(row[1], v[1]) );
	outVec = _mm_add_ps( outVec, _mm_mul_ps(row[2], v[2]) );
	
	_mm_store_ps(inoutMtx+12, outVec);
}

#endif // ENABLE_SSE

static FORCEINLINE s32 ___s32_saturate_shiftdown_accum64_fixed(s64 inAccum)
{
#ifdef FIXED_POINT_MATH_FUNCTIONS_USE_ACCUMULATOR_SATURATE
	if (inAccum > (s64)0x000007FFFFFFFFFFULL)
	{
		return (s32)0x7FFFFFFFU;
	}
	else if (inAccum < (s64)0xFFFFF80000000000ULL)
	{
		return (s32)0x80000000U;
	}
#endif // FIXED_POINT_MATH_FUNCTIONS_USE_ACCUMULATOR_SATURATE
	
	return sfx32_shiftdown(inAccum);
}

static FORCEINLINE s32 __vec4_dotproduct_vec4_fixed(const s32 (&__restrict vecA)[4], const s32 (&__restrict vecB)[4])
{
	return ___s32_saturate_shiftdown_accum64_fixed( fx32_mul(vecA[0],vecB[0]) + fx32_mul(vecA[1],vecB[1]) + fx32_mul(vecA[2],vecB[2]) + fx32_mul(vecA[3],vecB[3]) );
}

static FORCEINLINE void __vec4_multiply_mtx4_fixed(s32 (&__restrict inoutVec)[4], const s32 (&__restrict inMtx)[16])
{
	const CACHE_ALIGN s32 v[4] = {inoutVec[0], inoutVec[1], inoutVec[2], inoutVec[3]};
	
	inoutVec[0] = ___s32_saturate_shiftdown_accum64_fixed( fx32_mul(inMtx[0],v[0]) + fx32_mul(inMtx[4],v[1]) + fx32_mul(inMtx[ 8],v[2]) + fx32_mul(inMtx[12],v[3]) );
	inoutVec[1] = ___s32_saturate_shiftdown_accum64_fixed( fx32_mul(inMtx[1],v[0]) + fx32_mul(inMtx[5],v[1]) + fx32_mul(inMtx[ 9],v[2]) + fx32_mul(inMtx[13],v[3]) );
	inoutVec[2] = ___s32_saturate_shiftdown_accum64_fixed( fx32_mul(inMtx[2],v[0]) + fx32_mul(inMtx[6],v[1]) + fx32_mul(inMtx[10],v[2]) + fx32_mul(inMtx[14],v[3]) );
	inoutVec[3] = ___s32_saturate_shiftdown_accum64_fixed( fx32_mul(inMtx[3],v[0]) + fx32_mul(inMtx[7],v[1]) + fx32_mul(inMtx[11],v[2]) + fx32_mul(inMtx[15],v[3]) );
}

static FORCEINLINE void __vec3_multiply_mtx3_fixed(s32 (&__restrict inoutVec)[4], const s32 (&__restrict inMtx)[16])
{
	const CACHE_ALIGN s32 v[4] = {inoutVec[0], inoutVec[1], inoutVec[2], inoutVec[3]};
	
	inoutVec[0] = ___s32_saturate_shiftdown_accum64_fixed( fx32_mul(inMtx[0],v[0]) + fx32_mul(inMtx[4],v[1]) + fx32_mul(inMtx[ 8],v[2]) );
	inoutVec[1] = ___s32_saturate_shiftdown_accum64_fixed( fx32_mul(inMtx[1],v[0]) + fx32_mul(inMtx[5],v[1]) + fx32_mul(inMtx[ 9],v[2]) );
	inoutVec[2] = ___s32_saturate_shiftdown_accum64_fixed( fx32_mul(inMtx[2],v[0]) + fx32_mul(inMtx[6],v[1]) + fx32_mul(inMtx[10],v[2]) );
	inoutVec[3] = v[3];
}

static FORCEINLINE void __mtx4_multiply_mtx4_fixed(s32 (&__restrict mtxA)[16], const s32 (&__restrict mtxB)[16])
{
	CACHE_ALIGN s32 a[16];
	MatrixCopy(a, mtxA);
	
	mtxA[ 0] = ___s32_saturate_shiftdown_accum64_fixed( fx32_mul(a[ 0],mtxB[ 0]) + fx32_mul(a[ 4],mtxB[ 1]) + fx32_mul(a[ 8],mtxB[ 2]) + fx32_mul(a[12],mtxB[ 3]) );
	mtxA[ 1] = ___s32_saturate_shiftdown_accum64_fixed( fx32_mul(a[ 1],mtxB[ 0]) + fx32_mul(a[ 5],mtxB[ 1]) + fx32_mul(a[ 9],mtxB[ 2]) + fx32_mul(a[13],mtxB[ 3]) );
	mtxA[ 2] = ___s32_saturate_shiftdown_accum64_fixed( fx32_mul(a[ 2],mtxB[ 0]) + fx32_mul(a[ 6],mtxB[ 1]) + fx32_mul(a[10],mtxB[ 2]) + fx32_mul(a[14],mtxB[ 3]) );
	mtxA[ 3] = ___s32_saturate_shiftdown_accum64_fixed( fx32_mul(a[ 3],mtxB[ 0]) + fx32_mul(a[ 7],mtxB[ 1]) + fx32_mul(a[11],mtxB[ 2]) + fx32_mul(a[15],mtxB[ 3]) );
	
	mtxA[ 4] = ___s32_saturate_shiftdown_accum64_fixed( fx32_mul(a[ 0],mtxB[ 4]) + fx32_mul(a[ 4],mtxB[ 5]) + fx32_mul(a[ 8],mtxB[ 6]) + fx32_mul(a[12],mtxB[ 7]) );
	mtxA[ 5] = ___s32_saturate_shiftdown_accum64_fixed( fx32_mul(a[ 1],mtxB[ 4]) + fx32_mul(a[ 5],mtxB[ 5]) + fx32_mul(a[ 9],mtxB[ 6]) + fx32_mul(a[13],mtxB[ 7]) );
	mtxA[ 6] = ___s32_saturate_shiftdown_accum64_fixed( fx32_mul(a[ 2],mtxB[ 4]) + fx32_mul(a[ 6],mtxB[ 5]) + fx32_mul(a[10],mtxB[ 6]) + fx32_mul(a[14],mtxB[ 7]) );
	mtxA[ 7] = ___s32_saturate_shiftdown_accum64_fixed( fx32_mul(a[ 3],mtxB[ 4]) + fx32_mul(a[ 7],mtxB[ 5]) + fx32_mul(a[11],mtxB[ 6]) + fx32_mul(a[15],mtxB[ 7]) );
	
	mtxA[ 8] = ___s32_saturate_shiftdown_accum64_fixed( fx32_mul(a[ 0],mtxB[ 8]) + fx32_mul(a[ 4],mtxB[ 9]) + fx32_mul(a[ 8],mtxB[10]) + fx32_mul(a[12],mtxB[11]) );
	mtxA[ 9] = ___s32_saturate_shiftdown_accum64_fixed( fx32_mul(a[ 1],mtxB[ 8]) + fx32_mul(a[ 5],mtxB[ 9]) + fx32_mul(a[ 9],mtxB[10]) + fx32_mul(a[13],mtxB[11]) );
	mtxA[10] = ___s32_saturate_shiftdown_accum64_fixed( fx32_mul(a[ 2],mtxB[ 8]) + fx32_mul(a[ 6],mtxB[ 9]) + fx32_mul(a[10],mtxB[10]) + fx32_mul(a[14],mtxB[11]) );
	mtxA[11] = ___s32_saturate_shiftdown_accum64_fixed( fx32_mul(a[ 3],mtxB[ 8]) + fx32_mul(a[ 7],mtxB[ 9]) + fx32_mul(a[11],mtxB[10]) + fx32_mul(a[15],mtxB[11]) );
	
	mtxA[12] = ___s32_saturate_shiftdown_accum64_fixed( fx32_mul(a[ 0],mtxB[12]) + fx32_mul(a[ 4],mtxB[13]) + fx32_mul(a[ 8],mtxB[14]) + fx32_mul(a[12],mtxB[15]) );
	mtxA[13] = ___s32_saturate_shiftdown_accum64_fixed( fx32_mul(a[ 1],mtxB[12]) + fx32_mul(a[ 5],mtxB[13]) + fx32_mul(a[ 9],mtxB[14]) + fx32_mul(a[13],mtxB[15]) );
	mtxA[14] = ___s32_saturate_shiftdown_accum64_fixed( fx32_mul(a[ 2],mtxB[12]) + fx32_mul(a[ 6],mtxB[13]) + fx32_mul(a[10],mtxB[14]) + fx32_mul(a[14],mtxB[15]) );
	mtxA[15] = ___s32_saturate_shiftdown_accum64_fixed( fx32_mul(a[ 3],mtxB[12]) + fx32_mul(a[ 7],mtxB[13]) + fx32_mul(a[11],mtxB[14]) + fx32_mul(a[15],mtxB[15]) );
}

static FORCEINLINE void __mtx4_scale_vec3_fixed(s32 (&__restrict inoutMtx)[16], const s32 (&__restrict inVec)[4])
{
	inoutMtx[ 0] = ___s32_saturate_shiftdown_accum64_fixed( fx32_mul(inoutMtx[ 0], inVec[0]) );
	inoutMtx[ 1] = ___s32_saturate_shiftdown_accum64_fixed( fx32_mul(inoutMtx[ 1], inVec[0]) );
	inoutMtx[ 2] = ___s32_saturate_shiftdown_accum64_fixed( fx32_mul(inoutMtx[ 2], inVec[0]) );
	inoutMtx[ 3] = ___s32_saturate_shiftdown_accum64_fixed( fx32_mul(inoutMtx[ 3], inVec[0]) );
	
	inoutMtx[ 4] = ___s32_saturate_shiftdown_accum64_fixed( fx32_mul(inoutMtx[ 4], inVec[1]) );
	inoutMtx[ 5] = ___s32_saturate_shiftdown_accum64_fixed( fx32_mul(inoutMtx[ 5], inVec[1]) );
	inoutMtx[ 6] = ___s32_saturate_shiftdown_accum64_fixed( fx32_mul(inoutMtx[ 6], inVec[1]) );
	inoutMtx[ 7] = ___s32_saturate_shiftdown_accum64_fixed( fx32_mul(inoutMtx[ 7], inVec[1]) );
	
	inoutMtx[ 8] = ___s32_saturate_shiftdown_accum64_fixed( fx32_mul(inoutMtx[ 8], inVec[2]) );
	inoutMtx[ 9] = ___s32_saturate_shiftdown_accum64_fixed( fx32_mul(inoutMtx[ 9], inVec[2]) );
	inoutMtx[10] = ___s32_saturate_shiftdown_accum64_fixed( fx32_mul(inoutMtx[10], inVec[2]) );
	inoutMtx[11] = ___s32_saturate_shiftdown_accum64_fixed( fx32_mul(inoutMtx[11], inVec[2]) );
}

static FORCEINLINE void __mtx4_translate_vec3_fixed(s32 (&__restrict inoutMtx)[16], const s32 (&__restrict inVec)[4])
{
	inoutMtx[12] = ___s32_saturate_shiftdown_accum64_fixed( fx32_mul(inoutMtx[0], inVec[0]) + fx32_mul(inoutMtx[4], inVec[1]) + fx32_mul(inoutMtx[ 8], inVec[2]) + fx32_shiftup(inoutMtx[12]) );
	inoutMtx[13] = ___s32_saturate_shiftdown_accum64_fixed( fx32_mul(inoutMtx[1], inVec[0]) + fx32_mul(inoutMtx[5], inVec[1]) + fx32_mul(inoutMtx[ 9], inVec[2]) + fx32_shiftup(inoutMtx[13]) );
	inoutMtx[14] = ___s32_saturate_shiftdown_accum64_fixed( fx32_mul(inoutMtx[2], inVec[0]) + fx32_mul(inoutMtx[6], inVec[1]) + fx32_mul(inoutMtx[10], inVec[2]) + fx32_shiftup(inoutMtx[14]) );
	inoutMtx[15] = ___s32_saturate_shiftdown_accum64_fixed( fx32_mul(inoutMtx[3], inVec[0]) + fx32_mul(inoutMtx[7], inVec[1]) + fx32_mul(inoutMtx[11], inVec[2]) + fx32_shiftup(inoutMtx[15]) );
}

#ifdef ENABLE_SSE4_1

static FORCEINLINE void ___s32_saturate_shiftdown_accum64_fixed_SSE4(__m128i &inoutAccum)
{
#ifdef FIXED_POINT_MATH_FUNCTIONS_USE_ACCUMULATOR_SATURATE
	v128u8 outVecMask;
	
#if defined(ENABLE_SSE4_2)
	outVecMask = _mm_cmpgt_epi64( inoutAccum, _mm_set1_epi64x((s64)0x000007FFFFFFFFFFULL) );
	inoutAccum = _mm_blendv_epi8( inoutAccum, _mm_set1_epi64x((s64)0x000007FFFFFFFFFFULL), outVecMask );
	
	outVecMask = _mm_cmpgt_epi64( _mm_set1_epi64x((s64)0xFFFFF80000000000ULL), inoutAccum );
	inoutAccum = _mm_blendv_epi8( inoutAccum, _mm_set1_epi64x((s64)0xFFFFF80000000000ULL), outVecMask );
#else
	const v128u8 outVecSignMask = _mm_cmpeq_epi64( _mm_and_si128(inoutAccum, _mm_set1_epi64x((s64)0x8000000000000000ULL)), _mm_setzero_si128() );
	
	outVecMask = _mm_cmpeq_epi64( _mm_and_si128(inoutAccum, _mm_set1_epi64x((s64)0x7FFFF80000000000ULL)), _mm_setzero_si128() );
	const v128u32 outVecPos = _mm_blendv_epi8( _mm_set1_epi64x((s64)0x000007FFFFFFFFFFULL), inoutAccum, outVecMask );
	
	const v128u32 outVecFlipped = _mm_xor_si128(inoutAccum, _mm_set1_epi8(0xFF));
	outVecMask = _mm_cmpeq_epi64( _mm_and_si128(outVecFlipped, _mm_set1_epi64x((s64)0x7FFFF80000000000ULL)), _mm_setzero_si128() );
	const v128u32 outVecNeg = _mm_blendv_epi8( _mm_set1_epi64x((s64)0xFFFFF80000000000ULL), inoutAccum, outVecMask );
	
	inoutAccum = _mm_blendv_epi8(outVecNeg, outVecPos, outVecSignMask);
#endif // ENABLE_SSE4_2
	
#endif // FIXED_POINT_MATH_FUNCTIONS_USE_ACCUMULATOR_SATURATE
	
	inoutAccum = _mm_srli_epi64(inoutAccum, 12);
	inoutAccum = _mm_shuffle_epi32(inoutAccum, 0xD8);
}

static FORCEINLINE s32 __vec4_dotproduct_vec4_fixed_SSE4(const s32 (&__restrict vecA)[4], const s32 (&__restrict vecB)[4])
{
	// Due to SSE4.1's limitations, this function is actually slower than its scalar counterpart,
	// and so we're just going to use that here. The SSE4.1 code is being included for reference
	// as inspiration for porting to other ISAs that could see more benefit.
	return __vec4_dotproduct_vec4_fixed(vecA, vecB);
	
	/*
	const v128s32 inA = _mm_load_si128((v128s32 *)vecA);
	const v128s32 inB = _mm_load_si128((v128s32 *)vecB);
	
	const v128s32 lo = _mm_mul_epi32( _mm_shuffle_epi32(inA, 0x50), _mm_shuffle_epi32(inB, 0x50) );
	const v128s32 hi = _mm_mul_epi32( _mm_shuffle_epi32(inA, 0xFA), _mm_shuffle_epi32(inB, 0xFA) );
	
	s64 accum[4];
	_mm_store_si128((v128s32 *)&accum[0], lo);
	_mm_store_si128((v128s32 *)&accum[2], hi);
	
	return ___s32_saturate_shiftdown_accum64_fixed( accum[0] + accum[1] + accum[2] + accum[3] );
	*/
}

static FORCEINLINE void __vec4_multiply_mtx4_fixed_SSE4(s32 (&__restrict inoutVec)[4], const s32 (&__restrict inMtx)[16])
{
	const v128s32 inVec = _mm_load_si128((v128s32 *)inoutVec);
	
	const v128s32 v[4] = {
		_mm_shuffle_epi32(inVec, 0x00),
		_mm_shuffle_epi32(inVec, 0x55),
		_mm_shuffle_epi32(inVec, 0xAA),
		_mm_shuffle_epi32(inVec, 0xFF)
	};
	
	const v128s32 row[4] = {
		_mm_load_si128((v128s32 *)inMtx + 0),
		_mm_load_si128((v128s32 *)inMtx + 1),
		_mm_load_si128((v128s32 *)inMtx + 2),
		_mm_load_si128((v128s32 *)inMtx + 3)
	};
	
	const v128s32 rowLo[4] = {
		_mm_shuffle_epi32(row[0], 0x50),
		_mm_shuffle_epi32(row[1], 0x50),
		_mm_shuffle_epi32(row[2], 0x50),
		_mm_shuffle_epi32(row[3], 0x50)
	};
	
	const v128s32 rowHi[4] = {
		_mm_shuffle_epi32(row[0], 0xFA),
		_mm_shuffle_epi32(row[1], 0xFA),
		_mm_shuffle_epi32(row[2], 0xFA),
		_mm_shuffle_epi32(row[3], 0xFA)
	};
	
	v128s32 outVecLo =                  _mm_mul_epi32(rowLo[0], v[0]);
	outVecLo = _mm_add_epi64( outVecLo, _mm_mul_epi32(rowLo[1], v[1]) );
	outVecLo = _mm_add_epi64( outVecLo, _mm_mul_epi32(rowLo[2], v[2]) );
	outVecLo = _mm_add_epi64( outVecLo, _mm_mul_epi32(rowLo[3], v[3]) );
	___s32_saturate_shiftdown_accum64_fixed_SSE4(outVecLo);
	
	v128s32 outVecHi =                  _mm_mul_epi32(rowHi[0], v[0]);
	outVecHi = _mm_add_epi64( outVecHi, _mm_mul_epi32(rowHi[1], v[1]) );
	outVecHi = _mm_add_epi64( outVecHi, _mm_mul_epi32(rowHi[2], v[2]) );
	outVecHi = _mm_add_epi64( outVecHi, _mm_mul_epi32(rowHi[3], v[3]) );
	___s32_saturate_shiftdown_accum64_fixed_SSE4(outVecHi);
	
	_mm_store_si128( (v128s32 *)inoutVec, _mm_unpacklo_epi64(outVecLo, outVecHi) );
}

static FORCEINLINE void __vec3_multiply_mtx3_fixed_SSE4(s32 (&__restrict inoutVec)[4], const s32 (&__restrict inMtx)[16])
{
	const v128s32 inVec = _mm_load_si128((v128s32 *)inoutVec);
	
	const v128s32 v[3] = {
		_mm_shuffle_epi32(inVec, 0x00),
		_mm_shuffle_epi32(inVec, 0x55),
		_mm_shuffle_epi32(inVec, 0xAA)
	};
	
	const v128s32 row[3] = {
		_mm_load_si128((v128s32 *)inMtx + 0),
		_mm_load_si128((v128s32 *)inMtx + 1),
		_mm_load_si128((v128s32 *)inMtx + 2)
	};
	
	const v128s32 rowLo[4] = {
		_mm_shuffle_epi32(row[0], 0x50),
		_mm_shuffle_epi32(row[1], 0x50),
		_mm_shuffle_epi32(row[2], 0x50)
	};
	
	const v128s32 rowHi[4] = {
		_mm_shuffle_epi32(row[0], 0xFA),
		_mm_shuffle_epi32(row[1], 0xFA),
		_mm_shuffle_epi32(row[2], 0xFA)
	};
	
	v128s32 outVecLo =                  _mm_mul_epi32(rowLo[0], v[0]);
	outVecLo = _mm_add_epi64( outVecLo, _mm_mul_epi32(rowLo[1], v[1]) );
	outVecLo = _mm_add_epi64( outVecLo, _mm_mul_epi32(rowLo[2], v[2]) );
	___s32_saturate_shiftdown_accum64_fixed_SSE4(outVecLo);
	
	v128s32 outVecHi =                  _mm_mul_epi32(rowHi[0], v[0]);
	outVecHi = _mm_add_epi64( outVecHi, _mm_mul_epi32(rowHi[1], v[1]) );
	outVecHi = _mm_add_epi64( outVecHi, _mm_mul_epi32(rowHi[2], v[2]) );
	___s32_saturate_shiftdown_accum64_fixed_SSE4(outVecHi);
	
	v128s32 outVec = _mm_unpacklo_epi64(outVecLo, outVecHi);
	outVec = _mm_blend_epi16(outVec, inVec, 0xC0);
	
	_mm_store_si128((v128s32 *)inoutVec, outVec);
}

static FORCEINLINE void __mtx4_multiply_mtx4_fixed_SSE4(s32 (&__restrict mtxA)[16], const s32 (&__restrict mtxB)[16])
{
	const v128s32 rowA[4] = {
		_mm_load_si128((v128s32 *)(mtxA + 0)),
		_mm_load_si128((v128s32 *)(mtxA + 4)),
		_mm_load_si128((v128s32 *)(mtxA + 8)),
		_mm_load_si128((v128s32 *)(mtxA +12))
	};
	
	const v128s32 rowB[4] = {
		_mm_load_si128((v128s32 *)(mtxB + 0)),
		_mm_load_si128((v128s32 *)(mtxB + 4)),
		_mm_load_si128((v128s32 *)(mtxB + 8)),
		_mm_load_si128((v128s32 *)(mtxB +12))
	};
	
	const v128s32 rowLo[4] = {
		_mm_shuffle_epi32(rowA[0], 0x50),
		_mm_shuffle_epi32(rowA[1], 0x50),
		_mm_shuffle_epi32(rowA[2], 0x50),
		_mm_shuffle_epi32(rowA[3], 0x50)
	};
	
	const v128s32 rowHi[4] = {
		_mm_shuffle_epi32(rowA[0], 0xFA),
		_mm_shuffle_epi32(rowA[1], 0xFA),
		_mm_shuffle_epi32(rowA[2], 0xFA),
		_mm_shuffle_epi32(rowA[3], 0xFA)
	};
	
	v128s32 outVecLo;
	v128s32 outVecHi;
	v128s32 v[4];
	
	v[0] = _mm_shuffle_epi32(rowB[0], 0x00);
	v[1] = _mm_shuffle_epi32(rowB[0], 0x55);
	v[2] = _mm_shuffle_epi32(rowB[0], 0xAA);
	v[3] = _mm_shuffle_epi32(rowB[0], 0xFF);
	outVecLo =                          _mm_mul_epi32(rowLo[0], v[0]);
	outVecLo = _mm_add_epi64( outVecLo, _mm_mul_epi32(rowLo[1], v[1]) );
	outVecLo = _mm_add_epi64( outVecLo, _mm_mul_epi32(rowLo[2], v[2]) );
	outVecLo = _mm_add_epi64( outVecLo, _mm_mul_epi32(rowLo[3], v[3]) );
	___s32_saturate_shiftdown_accum64_fixed_SSE4(outVecLo);
	outVecHi =                          _mm_mul_epi32(rowHi[0], v[0]);
	outVecHi = _mm_add_epi64( outVecHi, _mm_mul_epi32(rowHi[1], v[1]) );
	outVecHi = _mm_add_epi64( outVecHi, _mm_mul_epi32(rowHi[2], v[2]) );
	outVecHi = _mm_add_epi64( outVecHi, _mm_mul_epi32(rowHi[3], v[3]) );
	___s32_saturate_shiftdown_accum64_fixed_SSE4(outVecHi);
	_mm_store_si128( (v128s32 *)(mtxA + 0), _mm_unpacklo_epi64(outVecLo, outVecHi) );
	
	v[0] = _mm_shuffle_epi32(rowB[1], 0x00);
	v[1] = _mm_shuffle_epi32(rowB[1], 0x55);
	v[2] = _mm_shuffle_epi32(rowB[1], 0xAA);
	v[3] = _mm_shuffle_epi32(rowB[1], 0xFF);
	outVecLo =                          _mm_mul_epi32(rowLo[0], v[0]);
	outVecLo = _mm_add_epi64( outVecLo, _mm_mul_epi32(rowLo[1], v[1]) );
	outVecLo = _mm_add_epi64( outVecLo, _mm_mul_epi32(rowLo[2], v[2]) );
	outVecLo = _mm_add_epi64( outVecLo, _mm_mul_epi32(rowLo[3], v[3]) );
	___s32_saturate_shiftdown_accum64_fixed_SSE4(outVecLo);
	outVecHi =                          _mm_mul_epi32(rowHi[0], v[0]);
	outVecHi = _mm_add_epi64( outVecHi, _mm_mul_epi32(rowHi[1], v[1]) );
	outVecHi = _mm_add_epi64( outVecHi, _mm_mul_epi32(rowHi[2], v[2]) );
	outVecHi = _mm_add_epi64( outVecHi, _mm_mul_epi32(rowHi[3], v[3]) );
	___s32_saturate_shiftdown_accum64_fixed_SSE4(outVecHi);
	_mm_store_si128( (v128s32 *)(mtxA + 4), _mm_unpacklo_epi64(outVecLo, outVecHi) );
	
	v[0] = _mm_shuffle_epi32(rowB[2], 0x00);
	v[1] = _mm_shuffle_epi32(rowB[2], 0x55);
	v[2] = _mm_shuffle_epi32(rowB[2], 0xAA);
	v[3] = _mm_shuffle_epi32(rowB[2], 0xFF);
	outVecLo =                          _mm_mul_epi32(rowLo[0], v[0]);
	outVecLo = _mm_add_epi64( outVecLo, _mm_mul_epi32(rowLo[1], v[1]) );
	outVecLo = _mm_add_epi64( outVecLo, _mm_mul_epi32(rowLo[2], v[2]) );
	outVecLo = _mm_add_epi64( outVecLo, _mm_mul_epi32(rowLo[3], v[3]) );
	___s32_saturate_shiftdown_accum64_fixed_SSE4(outVecLo);
	outVecHi =                          _mm_mul_epi32(rowHi[0], v[0]);
	outVecHi = _mm_add_epi64( outVecHi, _mm_mul_epi32(rowHi[1], v[1]) );
	outVecHi = _mm_add_epi64( outVecHi, _mm_mul_epi32(rowHi[2], v[2]) );
	outVecHi = _mm_add_epi64( outVecHi, _mm_mul_epi32(rowHi[3], v[3]) );
	___s32_saturate_shiftdown_accum64_fixed_SSE4(outVecHi);
	_mm_store_si128( (v128s32 *)(mtxA + 8), _mm_unpacklo_epi64(outVecLo, outVecHi) );
	
	v[0] = _mm_shuffle_epi32(rowB[3], 0x00);
	v[1] = _mm_shuffle_epi32(rowB[3], 0x55);
	v[2] = _mm_shuffle_epi32(rowB[3], 0xAA);
	v[3] = _mm_shuffle_epi32(rowB[3], 0xFF);
	outVecLo =                          _mm_mul_epi32(rowLo[0], v[0]);
	outVecLo = _mm_add_epi64( outVecLo, _mm_mul_epi32(rowLo[1], v[1]) );
	outVecLo = _mm_add_epi64( outVecLo, _mm_mul_epi32(rowLo[2], v[2]) );
	outVecLo = _mm_add_epi64( outVecLo, _mm_mul_epi32(rowLo[3], v[3]) );
	___s32_saturate_shiftdown_accum64_fixed_SSE4(outVecLo);
	outVecHi =                          _mm_mul_epi32(rowHi[0], v[0]);
	outVecHi = _mm_add_epi64( outVecHi, _mm_mul_epi32(rowHi[1], v[1]) );
	outVecHi = _mm_add_epi64( outVecHi, _mm_mul_epi32(rowHi[2], v[2]) );
	outVecHi = _mm_add_epi64( outVecHi, _mm_mul_epi32(rowHi[3], v[3]) );
	___s32_saturate_shiftdown_accum64_fixed_SSE4(outVecHi);
	_mm_store_si128( (v128s32 *)(mtxA +12), _mm_unpacklo_epi64(outVecLo, outVecHi) );
}

static FORCEINLINE void __mtx4_scale_vec3_fixed_SSE4(s32 (&__restrict inoutMtx)[16], const s32 (&__restrict inVec)[4])
{
	const v128s32 inVec_v128 = _mm_load_si128((v128s32 *)inVec);
	const v128s32 v[3] = {
		_mm_shuffle_epi32(inVec_v128, 0x00),
		_mm_shuffle_epi32(inVec_v128, 0x55),
		_mm_shuffle_epi32(inVec_v128, 0xAA)
	};
	
	v128s32 row[3] = {
		_mm_load_si128((v128s32 *)inoutMtx + 0),
		_mm_load_si128((v128s32 *)inoutMtx + 1),
		_mm_load_si128((v128s32 *)inoutMtx + 2)
	};
	
	v128s32 rowLo;
	v128s32 rowHi;
	
	rowLo = _mm_shuffle_epi32(row[0], 0x50);
	rowLo = _mm_mul_epi32(rowLo, v[0]);
	___s32_saturate_shiftdown_accum64_fixed_SSE4(rowLo);
	
	rowHi = _mm_shuffle_epi32(row[0], 0xFA);
	rowHi = _mm_mul_epi32(rowHi, v[0]);
	___s32_saturate_shiftdown_accum64_fixed_SSE4(rowHi);
	_mm_store_si128( (v128s32 *)inoutMtx + 0, _mm_unpacklo_epi64(rowLo, rowHi) );
	
	rowLo = _mm_shuffle_epi32(row[1], 0x50);
	rowLo = _mm_mul_epi32(rowLo, v[1]);
	___s32_saturate_shiftdown_accum64_fixed_SSE4(rowLo);
	
	rowHi = _mm_shuffle_epi32(row[1], 0xFA);
	rowHi = _mm_mul_epi32(rowHi, v[1]);
	___s32_saturate_shiftdown_accum64_fixed_SSE4(rowHi);
	_mm_store_si128( (v128s32 *)inoutMtx + 1, _mm_unpacklo_epi64(rowLo, rowHi) );
	
	rowLo = _mm_shuffle_epi32(row[2], 0x50);
	rowLo = _mm_mul_epi32(rowLo, v[2]);
	___s32_saturate_shiftdown_accum64_fixed_SSE4(rowLo);
	
	rowHi = _mm_shuffle_epi32(row[2], 0xFA);
	rowHi = _mm_mul_epi32(rowHi, v[2]);
	___s32_saturate_shiftdown_accum64_fixed_SSE4(rowHi);
	_mm_store_si128( (v128s32 *)inoutMtx + 2, _mm_unpacklo_epi64(rowLo, rowHi) );
}

static FORCEINLINE void __mtx4_translate_vec3_fixed_SSE4(s32 (&__restrict inoutMtx)[16], const s32 (&__restrict inVec)[4])
{
	const v128s32 tempVec = _mm_load_si128((v128s32 *)inVec);
	
	const v128s32 v[3] = {
		_mm_shuffle_epi32(tempVec, 0x00),
		_mm_shuffle_epi32(tempVec, 0x55),
		_mm_shuffle_epi32(tempVec, 0xAA)
	};
	
	const v128s32 row[4] = {
		_mm_load_si128((v128s32 *)(inoutMtx + 0)),
		_mm_load_si128((v128s32 *)(inoutMtx + 4)),
		_mm_load_si128((v128s32 *)(inoutMtx + 8)),
		_mm_load_si128((v128s32 *)(inoutMtx +12))
	};
	
	// Notice how we use pmovsxdq for the 4th row instead of pshufd. This is
	// because the dot product calculation for the 4th row involves adding a
	// 12-bit shift up (psllq) instead of adding a pmuldq. When using SSE
	// vectors as 64x2, pmuldq ignores the high 32 bits, while psllq needs
	// those high bits in case of a negative number. pmovsxdq does preserve
	// the sign bits, while pshufd does not.
	
	const v128s32 rowLo[4] = {
		_mm_shuffle_epi32(row[0], 0x50),
		_mm_shuffle_epi32(row[1], 0x50),
		_mm_shuffle_epi32(row[2], 0x50),
		_mm_cvtepi32_epi64(row[3])
	};
	
	const v128s32 rowHi[4] = {
		_mm_shuffle_epi32(row[0], 0xFA),
		_mm_shuffle_epi32(row[1], 0xFA),
		_mm_shuffle_epi32(row[2], 0xFA),
		_mm_cvtepi32_epi64( _mm_srli_si128(row[3],8) )
	};
	
	v128s32 outVecLo;
	v128s32 outVecHi;
	
	outVecLo =                          _mm_mul_epi32(rowLo[0], v[0]);
	outVecLo = _mm_add_epi64( outVecLo, _mm_mul_epi32(rowLo[1], v[1]) );
	outVecLo = _mm_add_epi64( outVecLo, _mm_mul_epi32(rowLo[2], v[2]) );
	outVecLo = _mm_add_epi64( outVecLo, _mm_slli_epi64(rowLo[3], 12) );
	___s32_saturate_shiftdown_accum64_fixed_SSE4(outVecLo);
	
	outVecHi =                          _mm_mul_epi32(rowHi[0], v[0]);
	outVecHi = _mm_add_epi64( outVecHi, _mm_mul_epi32(rowHi[1], v[1]) );
	outVecHi = _mm_add_epi64( outVecHi, _mm_mul_epi32(rowHi[2], v[2]) );
	outVecHi = _mm_add_epi64( outVecHi, _mm_slli_epi64(rowHi[3], 12) );
	___s32_saturate_shiftdown_accum64_fixed_SSE4(outVecHi);
	
	_mm_store_si128( (v128s32 *)(inoutMtx + 12), _mm_unpacklo_epi64(outVecLo, outVecHi) );
}

#endif // ENABLE_SSE4_1

void MatrixInit(s32 (&mtx)[16])
{
	MatrixIdentity(mtx);
}

void MatrixInit(float (&mtx)[16])
{
	MatrixIdentity(mtx);
}

void MatrixIdentity(s32 (&mtx)[16])
{
	static const CACHE_ALIGN s32 mtxIdentity[16] = {
		(1 << 12), 0, 0, 0,
		0, (1 << 12), 0, 0,
		0, 0, (1 << 12), 0,
		0, 0, 0, (1 << 12)
	};
	
	MatrixCopy(mtx, mtxIdentity);
}

void MatrixIdentity(float (&mtx)[16])
{
	static const CACHE_ALIGN float mtxIdentity[16] = {
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
	};
	
	MatrixCopy(mtx, mtxIdentity);
}

void MatrixSet(s32 (&mtx)[16], const size_t x, const size_t y, const s32 value)
{
	mtx[x+(y<<2)] = value;
}

void MatrixSet(float (&mtx)[16], const size_t x, const size_t y, const float value)
{
	mtx[x+(y<<2)] = value;
}

void MatrixSet(float (&mtx)[16], const size_t x, const size_t y, const s32 value)
{
	mtx[x+(y<<2)] = (float)value / 4096.0f;
}

void MatrixCopy(s32 (&__restrict mtxDst)[16], const s32 (&__restrict mtxSrc)[16])
{
	buffer_copy_fast<sizeof(s32)*16>((s32 *)mtxDst, (s32 *)mtxSrc);
}

void MatrixCopy(float (&__restrict mtxDst)[16], const float (&__restrict mtxSrc)[16])
{
	// Can't use buffer_copy_fast() here because it assumes the copying of integers,
	// so just use regular memcpy() for copying the floats.
	memcpy(mtxDst, mtxSrc, sizeof(float)*16);
}

void MatrixCopy(float (&__restrict mtxDst)[16], const s32 (&__restrict mtxSrc)[16])
{
#if defined(ENABLE_SSE)
	__mtx4_copynormalize_mtx4_float_SSE(mtxDst, mtxSrc);
#else
	__mtx4_copynormalize_mtx4_float(mtxDst, mtxSrc);
#endif
}

int MatrixCompare(const s32 (&__restrict mtxDst)[16], const s32 (&__restrict mtxSrc)[16])
{
	return memcmp(mtxDst, mtxSrc, sizeof(s32)*16);
}

int MatrixCompare(const float (&__restrict mtxDst)[16], const float (&__restrict mtxSrc)[16])
{
	return memcmp(mtxDst, mtxSrc, sizeof(float)*16);
}

s32 MatrixGetMultipliedIndex(const u32 index, const s32 (&__restrict mtxA)[16], const s32 (&__restrict mtxB)[16])
{
	assert(index < 16);
	
	const u32 col = index & 0x00000003;
	const u32 row = index & 0x0000000C;
	
	const s32 vecA[4] = { mtxA[col+0], mtxA[col+4], mtxA[col+8], mtxA[col+12] };
	const s32 vecB[4] = { mtxB[row+0], mtxB[row+1], mtxB[row+2], mtxB[row+3] };
	
#if defined(ENABLE_SSE4_1)
	return __vec4_dotproduct_vec4_fixed_SSE4(vecA, vecB);
#else
	return __vec4_dotproduct_vec4_fixed(vecA, vecB);
#endif
}

float MatrixGetMultipliedIndex(const u32 index, const float (&__restrict mtxA)[16], const float (&__restrict mtxB)[16])
{
	assert(index < 16);
	
	const u32 col = index & 0x00000003;
	const u32 row = index & 0x0000000C;
	
	const float vecA[4] = { mtxA[col+0], mtxA[col+4], mtxA[col+8], mtxA[col+12] };
	const float vecB[4] = { mtxB[row+0], mtxB[row+1], mtxB[row+2], mtxB[row+3] };
	
#if defined(ENABLE_SSE4_1)
	return __vec4_dotproduct_vec4_float_SSE4(vecA, vecB);
#else
	return __vec4_dotproduct_vec4_float(vecA, vecB);
#endif
}

template <MatrixMode MODE>
void MatrixStackInit(MatrixStack<MODE> *stack)
{
	for (size_t i = 0; i < MatrixStack<MODE>::size; i++)
	{
		MatrixInit(stack->matrix[i]);
	}
	
	stack->position = 0;
}

template <MatrixMode MODE>
s32* MatrixStackGet(MatrixStack<MODE> *stack)
{
	return stack->matrix[stack->position];
}

template void MatrixStackInit(MatrixStack<MATRIXMODE_PROJECTION> *stack);
template void MatrixStackInit(MatrixStack<MATRIXMODE_POSITION> *stack);
template void MatrixStackInit(MatrixStack<MATRIXMODE_POSITION_VECTOR> *stack);
template void MatrixStackInit(MatrixStack<MATRIXMODE_TEXTURE> *stack);

template s32* MatrixStackGet(MatrixStack<MATRIXMODE_PROJECTION> *stack);
template s32* MatrixStackGet(MatrixStack<MATRIXMODE_POSITION> *stack);
template s32* MatrixStackGet(MatrixStack<MATRIXMODE_POSITION_VECTOR> *stack);
template s32* MatrixStackGet(MatrixStack<MATRIXMODE_TEXTURE> *stack);

// TODO: All of these float-based vector functions are obsolete and should be deleted.
void Vector2Copy(float *dst, const float *src)
{
	dst[0] = src[0];
	dst[1] = src[1];
}

void Vector2Add(float *dst, const float *src)
{
	dst[0] += src[0];
	dst[1] += src[1];
}

void Vector2Subtract(float *dst, const float *src)
{
	dst[0] -= src[0];
	dst[1] -= src[1];
}

float Vector2Dot(const float *a, const float *b)
{
	return (a[0]*b[0]) + (a[1]*b[1]);
}

/* http://www.gamedev.net/community/forums/topic.asp?topic_id=289972 */
float Vector2Cross(const float *a, const float *b)
{
	return (a[0]*b[1]) - (a[1]*b[0]);
}

float Vector3Dot(const float *a, const float *b) 
{
	return a[0]*b[0] + a[1]*b[1] + a[2]*b[2];
}

void Vector3Cross(float* dst, const float *a, const float *b) 
{
	dst[0] = a[1]*b[2] - a[2]*b[1];
	dst[1] = a[2]*b[0] - a[0]*b[2];
	dst[2] = a[0]*b[1] - a[1]*b[0];
}

float Vector3Length(const float *a)
{
	float lengthSquared = Vector3Dot(a,a);
	float length = sqrt(lengthSquared);
	return length;
}

void Vector3Add(float *dst, const float *src)
{
	dst[0] += src[0];
	dst[1] += src[1];
	dst[2] += src[2];
}

void Vector3Subtract(float *dst, const float *src)
{
	dst[0] -= src[0];
	dst[1] -= src[1];
	dst[2] -= src[2];
}

void Vector3Scale(float *dst, const float scale)
{
	dst[0] *= scale;
	dst[1] *= scale;
	dst[2] *= scale;
}

void Vector3Copy(float *dst, const float *src)
{
	dst[0] = src[0];
	dst[1] = src[1];
	dst[2] = src[2];
}

void Vector3Normalize(float *dst)
{
	float length = Vector3Length(dst);
	Vector3Scale(dst,1.0f/length);
}

void Vector4Copy(float *dst, const float *src)
{
	dst[0] = src[0];
	dst[1] = src[1];
	dst[2] = src[2];
	dst[3] = src[3];
}

void MatrixMultVec4x4(const s32 (&__restrict mtx)[16], s32 (&__restrict vec)[4])
{
#if defined(ENABLE_SSE4_1)
	__vec4_multiply_mtx4_fixed_SSE4(vec, mtx);
#else
	__vec4_multiply_mtx4_fixed(vec, mtx);
#endif
}

void MatrixMultVec4x4(const s32 (&__restrict mtx)[16], float (&__restrict vec)[4])
{
#if defined(ENABLE_SSE)
	__vec4_multiply_mtx4_float_SSE(vec, mtx);
#else
	__vec4_multiply_mtx4_float(vec, mtx);
#endif
}

void MatrixMultVec3x3(const s32 (&__restrict mtx)[16], s32 (&__restrict vec)[4])
{
#if defined(ENABLE_SSE4_1)
	__vec3_multiply_mtx3_fixed_SSE4(vec, mtx);
#else
	__vec3_multiply_mtx3_fixed(vec, mtx);
#endif
}

void MatrixMultVec3x3(const s32 (&__restrict mtx)[16], float (&__restrict vec)[4])
{
#if defined(ENABLE_SSE)
	__vec3_multiply_mtx3_float_SSE(vec, mtx);
#else
	__vec3_multiply_mtx3_float(vec, mtx);
#endif
}

void MatrixTranslate(s32 (&__restrict mtx)[16], const s32 (&__restrict vec)[4])
{
#if defined(ENABLE_SSE4_1)
	__mtx4_translate_vec3_fixed_SSE4(mtx, vec);
#else
	__mtx4_translate_vec3_fixed(mtx, vec);
#endif
}

void MatrixTranslate(float (&__restrict mtx)[16], const float (&__restrict vec)[4])
{
#if defined(ENABLE_SSE)
	__mtx4_translate_vec3_float_SSE(mtx, vec);
#else
	__mtx4_translate_vec3_float(mtx, vec);
#endif
}

void MatrixScale(s32 (&__restrict mtx)[16], const s32 (&__restrict vec)[4])
{
#if defined(ENABLE_SSE4_1)
	__mtx4_scale_vec3_fixed_SSE4(mtx, vec);
#else
	__mtx4_scale_vec3_fixed(mtx, vec);
#endif
}

void MatrixScale(float (&__restrict mtx)[16], const float (&__restrict vec)[4])
{
#if defined(ENABLE_SSE)
	__mtx4_scale_vec3_float_SSE(mtx, vec);
#else
	__mtx4_scale_vec3_float(mtx, vec);
#endif
}

void MatrixMultiply(s32 (&__restrict mtxA)[16], const s32 (&__restrict mtxB)[16])
{
#if defined(ENABLE_SSE4_1)
	__mtx4_multiply_mtx4_fixed_SSE4(mtxA, mtxB);
#else
	__mtx4_multiply_mtx4_fixed(mtxA, mtxB);
#endif
}

void MatrixMultiply(float (&__restrict mtxA)[16], const s32 (&__restrict mtxB)[16])
{
#if defined(ENABLE_SSE)
	__mtx4_multiply_mtx4_float_SSE(mtxA, mtxB);
#else
	__mtx4_multiply_mtx4_float(mtxA, mtxB);
#endif
}
