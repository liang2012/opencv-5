//M*//////////////////////////////////////////////////////////////////////////////////////
//
//  IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING.
//
//  By downloading, copying, installing or using the software you agree to this license.
//  If you do not agree to this license, do not download, install,
//  copy or use the software.
//
//
//                        Intel License Agreement
//                For Open Source Computer Vision Library
//
// Copyright (C) 2000, Intel Corporation, all rights reserved.
// Third party copyrights are property of their respective owners.
//
// Redistribution and use in source and binary forms, with or without modification,
// are permitted provided that the following conditions are met:
//
//   * Redistribution's of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//
//   * Redistribution's in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation
//     and/or other materials provided with the distribution.
//
//   * The name of Intel Corporation may not be used to endorse or promote products
//     derived from this software without specific prior written permission.
//
// This software is provided by the copyright holders and contributors "as is" and
// any express or implied warranties, including, but not limited to, the implied
// warranties of merchantability and fitness for a particular purpose are disclaimed.
// In no event shall the Intel Corporation or contributors be liable for any direct,
// indirect, incidental, special, exemplary, or consequential damages
// (including, but not limited to, procurement of substitute goods or services;
// loss of use, data, or profits; or business interruption) however caused
// and on any theory of liability, whether in contract, strict liability,
// or tort (including negligence or otherwise) arising in any way out of
// the use of this software, even if advised of the possibility of such damage.
//
//M*/

/****************************************************************************************\
*    Very fast SAD-based (Sum-of-Absolute-Diffrences) stereo correspondence algorithm.   *
*    Contributed by Kurt Konolige                                                        *
\****************************************************************************************/

#include "_cv.h"
/*
#undef CV_SSE2
#define CV_SSE2 1
#include "emmintrin.h"
*/

CV_IMPL CvStereoBMState* cvCreateStereoBMState( int /*preset*/, int numberOfDisparities )
{
    CvStereoBMState* state = (CvStereoBMState*)cvAlloc( sizeof(*state) );
    if( !state )
        return 0;

    state->preFilterType = CV_STEREO_BM_XSOBEL; //CV_STEREO_BM_NORMALIZED_RESPONSE;
    state->preFilterSize = 9;
    state->preFilterCap = 31;
    state->SADWindowSize = 15;
    state->minDisparity = 0;
    state->numberOfDisparities = numberOfDisparities > 0 ? numberOfDisparities : 64;
    state->textureThreshold = 10;
    state->uniquenessRatio = 15;
    state->speckleRange = state->speckleWindowSize = 0;
    state->trySmallerWindows = 0;

    state->preFilteredImg0 = state->preFilteredImg1 = state->slidingSumBuf = 0;
    state->dbmin = state->dbmax = state->disp = 0;

    return state;
}


CV_IMPL void cvReleaseStereoBMState( CvStereoBMState** state )
{
    if( !state )
        CV_Error( CV_StsNullPtr, "" );

    if( !*state )
        return;

    cvReleaseMat( &(*state)->preFilteredImg0 );
    cvReleaseMat( &(*state)->preFilteredImg1 );
    cvReleaseMat( &(*state)->slidingSumBuf );
    cvReleaseMat( &(*state)->dbmin );
    cvReleaseMat( &(*state)->dbmax );
    cvReleaseMat( &(*state)->disp );
    cvFree( state );
}

static void icvPrefilterNorm( const CvMat* src, CvMat* dst, int winsize, int ftzero, uchar* buf )
{
    int x, y, wsz2 = winsize/2;
    int* vsum = (int*)cvAlignPtr(buf + (wsz2 + 1)*sizeof(vsum[0]), 32);
    int scale_g = winsize*winsize/8, scale_s = (1024 + scale_g)/(scale_g*2);
    const int OFS = 256*5, TABSZ = OFS*2 + 256;
    uchar tab[TABSZ];
    const uchar* sptr = src->data.ptr;
    int srcstep = src->step;
    CvSize size = cvGetMatSize(src);

    scale_g *= scale_s;

    for( x = 0; x < TABSZ; x++ )
        tab[x] = (uchar)(x - OFS < -ftzero ? 0 : x - OFS > ftzero ? ftzero*2 : x - OFS + ftzero);

    for( x = 0; x < size.width; x++ )
        vsum[x] = (ushort)(sptr[x]*(wsz2 + 2));

    for( y = 1; y < wsz2; y++ )
    {
        for( x = 0; x < size.width; x++ )
            vsum[x] = (ushort)(vsum[x] + sptr[srcstep*y + x]);
    }

    for( y = 0; y < size.height; y++ )
    {
        const uchar* top = sptr + srcstep*MAX(y-wsz2-1,0);
        const uchar* bottom = sptr + srcstep*MIN(y+wsz2,size.height-1);
        const uchar* prev = sptr + srcstep*MAX(y-1,0);
        const uchar* curr = sptr + srcstep*y;
        const uchar* next = sptr + srcstep*MIN(y+1,size.height-1);
        uchar* dptr = dst->data.ptr + dst->step*y;
        x = 0;

        for( ; x < size.width; x++ )
            vsum[x] = (ushort)(vsum[x] + bottom[x] - top[x]);

        for( x = 0; x <= wsz2; x++ )
        {
            vsum[-x-1] = vsum[0];
            vsum[size.width+x] = vsum[size.width-1];
        }

        int sum = vsum[0]*(wsz2 + 1);
        for( x = 1; x <= wsz2; x++ )
            sum += vsum[x];

        int val = ((curr[0]*5 + curr[1] + prev[0] + next[0])*scale_g - sum*scale_s) >> 10;
        dptr[0] = tab[val + OFS];

        for( x = 1; x < size.width-1; x++ )
        {
            sum += vsum[x+wsz2] - vsum[x-wsz2-1];
            val = ((curr[x]*4 + curr[x-1] + curr[x+1] + prev[x] + next[x])*scale_g - sum*scale_s) >> 10;
            dptr[x] = tab[val + OFS];
        }
        
        sum += vsum[x+wsz2] - vsum[x-wsz2-1];
        val = ((curr[x]*5 + curr[x-1] + prev[x] + next[x])*scale_g - sum*scale_s) >> 10;
        dptr[x] = tab[val + OFS];
    }
}


static void
icvPrefilterXSobel( const CvMat* src, CvMat* dst, int ftzero )
{
    int x, y;
    const int OFS = 256*4, TABSZ = OFS*2 + 256;
    uchar tab[TABSZ];
    CvSize size = cvGetMatSize(src);
    
    for( x = 0; x < TABSZ; x++ )
        tab[x] = (uchar)(x - OFS < -ftzero ? 0 : x - OFS > ftzero ? ftzero*2 : x - OFS + ftzero);
    
    for( y = 0; y < size.height; y++ )
    {
        const uchar* srow1 = src->data.ptr + y*src->step;
        const uchar* srow0 = y > 0 ? srow1 - src->step : size.height > 1 ? srow1 + src->step : srow1;
        const uchar* srow2 = y < size.height-1 ? srow1 + src->step : size.height > 1 ? srow1 - src->step : srow1;
        uchar* dptr = dst->data.ptr + dst->step*y;
        
        dptr[0] = dptr[size.width-1] = tab[0 + OFS];
        
        for( x = 1; x < size.width-1; x++ )
            dptr[x] = tab[(srow1[x+1] - srow1[x-1])*2 + srow2[x+1] - srow2[x-1] + srow0[x+1] - srow0[x-1] + OFS];
    }
}


static const int DISPARITY_SHIFT = 4;

#if CV_SSE2
static void icvFindStereoCorrespondenceBM_SSE2( const CvMat* left, const CvMat* right,
                                    CvMat* disp, const CvMat* dbmin,
                                    const CvMat* dbmax, CvStereoBMState* state,
                                    int realSADWin, uchar* buf, int _dy0, int _dy1 )
{
    int x, y, d;
    int wsz = realSADWin, wsz2 = wsz/2;
    int dy0 = MIN(_dy0, wsz2+1), dy1 = MIN(_dy1, wsz2+1);
    int ndisp = state->numberOfDisparities;
    int mindisp = state->minDisparity;
    int lofs = MAX(ndisp - 1 + mindisp, 0);
    int rofs = -MIN(ndisp - 1 + mindisp, 0);
    int width = left->cols, height = left->rows;
    int width1 = width - rofs - ndisp + 1;
    int ftzero = state->preFilterCap;
    int textureThreshold = state->textureThreshold;
    int uniquenessRatio = state->uniquenessRatio;
    short FILTERED = (short)((mindisp - 1) << DISPARITY_SHIFT);
    int DELTA = (state->numberOfDisparities - 1 - state->minDisparity) << DISPARITY_SHIFT;

    ushort *sad, *hsad0, *hsad, *hsad_sub;
    int *htext;
    uchar *cbuf0, *cbuf;
    const uchar* lptr0 = left->data.ptr + lofs;
    const uchar* rptr0 = right->data.ptr + rofs;
    const uchar *lptr, *lptr_sub, *rptr;
    short* dptr = disp->data.s;
    int sstep = left->step;
    int dstep = disp->step/sizeof(dptr[0]);
    int cstep = (height + dy0 + dy1)*ndisp;
    const int TABSZ = 256;
    uchar tab[TABSZ];
    const __m128i d0_8 = _mm_setr_epi16(0,1,2,3,4,5,6,7), dd_8 = _mm_set1_epi16(8);

    sad = (ushort*)cvAlignPtr(buf + sizeof(sad[0]));
    hsad0 = (ushort*)cvAlignPtr(sad + ndisp + 1 + dy0*ndisp);
    htext = (int*)cvAlignPtr((int*)(hsad0 + (height+dy1)*ndisp) + wsz2 + 2);
    cbuf0 = (uchar*)cvAlignPtr(htext + height + wsz2 + 2 + dy0*ndisp);

    for( x = 0; x < TABSZ; x++ )
        tab[x] = (uchar)abs(x - ftzero);

    // initialize buffers
    memset( hsad0 - dy0*ndisp, 0, (height + dy0 + dy1)*ndisp*sizeof(hsad0[0]) );
    memset( htext - wsz2 - 1, 0, (height + wsz + 1)*sizeof(htext[0]) );

    for( x = -wsz2-1; x < wsz2; x++ )
    {
        hsad = hsad0 - dy0*ndisp; cbuf = cbuf0 + (x + wsz2 + 1)*cstep - dy0*ndisp;
        lptr = lptr0 + MIN(MAX(x, -lofs), width-lofs-1) - dy0*sstep;
        rptr = rptr0 + MIN(MAX(x, -rofs), width-rofs-1) - dy0*sstep;

        for( y = -dy0; y < height + dy1; y++, hsad += ndisp, cbuf += ndisp, lptr += sstep, rptr += sstep )
        {
            int lval = lptr[0];
            for( d = 0; d < ndisp; d++ )
            {
                int diff = abs(lval - rptr[d]);
                cbuf[d] = (uchar)diff;
                hsad[d] = (ushort)(hsad[d] + diff);
            }
            htext[y] += tab[lval];
        }
    }

    // initialize the left and right borders of the disparity map
    for( y = 0; y < height; y++ )
    {
        for( x = 0; x < lofs; x++ )
            dptr[y*dstep + x] = FILTERED;
        for( x = lofs + width1; x < width; x++ )
            dptr[y*dstep + x] = FILTERED;
    }
    dptr += lofs;

    for( x = 0; x < width1; x++, dptr++ )
    {
        int x0 = x - wsz2 - 1, x1 = x + wsz2;
        const uchar* cbuf_sub = cbuf0 + ((x0 + wsz2 + 1) % (wsz + 1))*cstep - dy0*ndisp;
        uchar* cbuf = cbuf0 + ((x1 + wsz2 + 1) % (wsz + 1))*cstep - dy0*ndisp;
        hsad = hsad0 - dy0*ndisp;
        lptr_sub = lptr0 + MIN(MAX(x0, -lofs), width-1-lofs) - dy0*sstep;
        lptr = lptr0 + MIN(MAX(x1, -lofs), width-1-lofs) - dy0*sstep;
        rptr = rptr0 + MIN(MAX(x1, -rofs), width-1-rofs) - dy0*sstep;

        for( y = -dy0; y < height + dy1; y++, cbuf += ndisp, cbuf_sub += ndisp,
             hsad += ndisp, lptr += sstep, lptr_sub += sstep, rptr += sstep )
        {
            int lval = lptr[0];
            __m128i lv = _mm_set1_epi8((char)lval), z = _mm_setzero_si128();
            for( d = 0; d < ndisp; d += 16 )
            {
                __m128i rv = _mm_loadu_si128((const __m128i*)(rptr + d));
                __m128i hsad_l = _mm_load_si128((__m128i*)(hsad + d));
                __m128i hsad_h = _mm_load_si128((__m128i*)(hsad + d + 8));
                __m128i cbs = _mm_load_si128((const __m128i*)(cbuf_sub + d));
                __m128i diff = _mm_adds_epu8(_mm_subs_epu8(lv, rv), _mm_subs_epu8(rv, lv));
                __m128i diff_h = _mm_sub_epi16(_mm_unpackhi_epi8(diff, z), _mm_unpackhi_epi8(cbs, z));
                _mm_store_si128((__m128i*)(cbuf + d), diff);
                diff = _mm_sub_epi16(_mm_unpacklo_epi8(diff, z), _mm_unpacklo_epi8(cbs, z));
                hsad_h = _mm_add_epi16(hsad_h, diff_h);
                hsad_l = _mm_add_epi16(hsad_l, diff);
                _mm_store_si128((__m128i*)(hsad + d), hsad_l);
                _mm_store_si128((__m128i*)(hsad + d + 8), hsad_h);
            }
            htext[y] += tab[lval] - tab[lptr_sub[0]];
        }

        // fill borders
        for( y = dy1; y <= wsz2; y++ )
            htext[height+y] = htext[height+dy1-1];
        for( y = -wsz2-1; y < -dy0; y++ )
            htext[y] = htext[-dy0];

        // initialize sums
        for( d = 0; d < ndisp; d++ )
            sad[d] = (ushort)(hsad0[d-ndisp*dy0]*(wsz2 + 2 - dy0));
        
        hsad = hsad0 + (1 - dy0)*ndisp;
        for( y = 1 - dy0; y < wsz2; y++, hsad += ndisp )
            for( d = 0; d < ndisp; d++ )
                sad[d] = (ushort)(sad[d] + hsad[d]);
        int tsum = 0;
        for( y = -wsz2-1; y < wsz2; y++ )
            tsum += htext[y];

        // finally, start the real processing
        for( y = 0; y < height; y++ )
        {
            int minsad = INT_MAX, mind = -1;
            hsad = hsad0 + MIN(y + wsz2, height+dy1-1)*ndisp;
            hsad_sub = hsad0 + MAX(y - wsz2 - 1, -dy0)*ndisp;
            __m128i minsad8 = _mm_set1_epi16(SHRT_MAX);
            __m128i mind8 = _mm_set1_epi16(-1), d8 = d0_8, mask;

            for( d = 0; d < ndisp; d += 8 )
            {
                __m128i v0 = _mm_load_si128((__m128i*)(hsad_sub + d));
                __m128i v1 = _mm_load_si128((__m128i*)(hsad + d));
                __m128i sad8 = _mm_load_si128((__m128i*)(sad + d));
                sad8 = _mm_sub_epi16(sad8, v0);
                sad8 = _mm_add_epi16(sad8, v1);

                mask = _mm_cmpgt_epi16(minsad8, sad8);
                _mm_store_si128((__m128i*)(sad + d), sad8);
                minsad8 = _mm_min_epi16(minsad8, sad8);
                mind8 = _mm_xor_si128(mind8,_mm_and_si128(_mm_xor_si128(d8,mind8),mask));
                d8 = _mm_add_epi16(d8, dd_8);
            }

            __m128i minsad82 = _mm_unpackhi_epi64(minsad8, minsad8);
            __m128i mind82 = _mm_unpackhi_epi64(mind8, mind8);
            mask = _mm_cmpgt_epi16(minsad8, minsad82);
            mind8 = _mm_xor_si128(mind8,_mm_and_si128(_mm_xor_si128(mind82,mind8),mask));
            minsad8 = _mm_min_epi16(minsad8, minsad82);

            minsad82 = _mm_shufflelo_epi16(minsad8, _MM_SHUFFLE(3,2,3,2));
            mind82 = _mm_shufflelo_epi16(mind8, _MM_SHUFFLE(3,2,3,2));
            mask = _mm_cmpgt_epi16(minsad8, minsad82);
            mind8 = _mm_xor_si128(mind8,_mm_and_si128(_mm_xor_si128(mind82,mind8),mask));
            minsad8 = _mm_min_epi16(minsad8, minsad82);

            minsad82 = _mm_shufflelo_epi16(minsad8, 1);
            mind82 = _mm_shufflelo_epi16(mind8, 1);
            mask = _mm_cmpgt_epi16(minsad8, minsad82);
            mind8 = _mm_xor_si128(mind8,_mm_and_si128(_mm_xor_si128(mind82,mind8),mask));
            mind = (short)_mm_cvtsi128_si32(mind8);
            minsad = sad[mind];
            tsum += htext[y + wsz2] - htext[y - wsz2 - 1];
            if( tsum < textureThreshold )
            {
                if( !dbmin )
                    dptr[y*dstep] = FILTERED;
                continue;
            }

            if( uniquenessRatio > 0 )
            {
                int thresh = minsad + (minsad * uniquenessRatio/100);
                __m128i thresh8 = _mm_set1_epi16((short)(thresh + 1));
                __m128i d1 = _mm_set1_epi16((short)(mind-1)), d2 = _mm_set1_epi16((short)(mind+1));
                __m128i d8 = d0_8;

                for( d = 0; d < ndisp; d += 8 )
                {
                    __m128i sad8 = _mm_load_si128((__m128i*)(sad + d));
                    __m128i mask = _mm_cmpgt_epi16( thresh8, sad8 );
                    mask = _mm_and_si128(mask, _mm_or_si128(_mm_cmpgt_epi16(d1,d8), _mm_cmpgt_epi16(d8,d2)));
                    if( _mm_movemask_epi8(mask) )
                        break;
                    d8 = _mm_add_epi16(d8, dd_8);
                }
                if( d < ndisp )
                {
                    if( !dbmin )
                        dptr[y*dstep] = FILTERED;
                    continue;
                }
            }
            
            if( dbmin )
            {
                int maxval = ((const short*)(dbmin->data.ptr + dbmin->step*y))[x];
                int minval = ((const short*)(dbmax->data.ptr + dbmax->step*y))[x];
                minval = (DELTA - minval) >> DISPARITY_SHIFT;
                maxval = (DELTA - maxval + (1<<DISPARITY_SHIFT)-1) >> DISPARITY_SHIFT;
                if( d < minval || d > maxval )
                    continue;
            }

            {
            sad[-1] = sad[1];
            sad[ndisp] = sad[ndisp-2];
            int p = sad[mind+1], n = sad[mind-1], d = p + n - 2*sad[mind];
            dptr[y*dstep] = (short)(((ndisp - mind - 1 + mindisp)*256 + (d != 0 ? (p-n)*128/d : 0) + 15) >> 4);
            }
        }
    }
}
#endif

static void
icvFindStereoCorrespondenceBM( const CvMat* left, const CvMat* right,
                               CvMat* disp, const CvMat* dbmin,
                               const CvMat* dbmax, CvStereoBMState* state,
                               int realSADWin, uchar* buf, int _dy0, int _dy1 )
{
    int x, y, d;
    int wsz = realSADWin, wsz2 = wsz/2;
    int dy0 = MIN(_dy0, wsz2+1), dy1 = MIN(_dy1, wsz2+1);
    int ndisp = state->numberOfDisparities;
    int mindisp = state->minDisparity;
    int lofs = MAX(ndisp - 1 + mindisp, 0);
    int rofs = -MIN(ndisp - 1 + mindisp, 0);
    int width = left->cols, height = left->rows;
    int width1 = width - rofs - ndisp + 1;
    int ftzero = state->preFilterCap;
    int textureThreshold = state->textureThreshold;
    int uniquenessRatio = state->uniquenessRatio;
    short FILTERED = (short)((mindisp - 1) << DISPARITY_SHIFT);
    int DELTA = (state->numberOfDisparities - 1 - state->minDisparity) << DISPARITY_SHIFT;

    int *sad, *hsad0, *hsad, *hsad_sub, *htext;
    uchar *cbuf0, *cbuf;
    const uchar* lptr0 = left->data.ptr + lofs;
    const uchar* rptr0 = right->data.ptr + rofs;
    const uchar *lptr, *lptr_sub, *rptr;
    short* dptr = disp->data.s;
    int sstep = left->step;
    int dstep = disp->step/sizeof(dptr[0]);
    int cstep = (height+dy0+dy1)*ndisp;
    const int TABSZ = 256;
    uchar tab[TABSZ];

    sad = (int*)cvAlignPtr(buf + sizeof(sad[0]));
    hsad0 = (int*)cvAlignPtr(sad + ndisp + 1 + dy0*ndisp);
    htext = (int*)cvAlignPtr((int*)(hsad0 + (height+dy1)*ndisp) + wsz2 + 2);
    cbuf0 = (uchar*)cvAlignPtr(htext + height + wsz2 + 2 + dy0*ndisp);

    for( x = 0; x < TABSZ; x++ )
        tab[x] = (uchar)abs(x - ftzero);

    // initialize buffers
    memset( hsad0 - dy0*ndisp, 0, (height + dy0 + dy1)*ndisp*sizeof(hsad0[0]) );
    memset( htext - wsz2 - 1, 0, (height + wsz + 1)*sizeof(htext[0]) );

    for( x = -wsz2-1; x < wsz2; x++ )
    {
        hsad = hsad0 - dy0*ndisp; cbuf = cbuf0 + (x + wsz2 + 1)*cstep - dy0*ndisp;
        lptr = lptr0 + MIN(MAX(x, -lofs), width-lofs-1) - dy0*sstep;
        rptr = rptr0 + MIN(MAX(x, -rofs), width-rofs-1) - dy0*sstep;

        for( y = -dy0; y < height + dy1; y++, hsad += ndisp, cbuf += ndisp, lptr += sstep, rptr += sstep )
        {
            int lval = lptr[0];
            for( d = 0; d < ndisp; d++ )
            {
                int diff = abs(lval - rptr[d]);
                cbuf[d] = (uchar)diff;
                hsad[d] = (int)(hsad[d] + diff);
            }
            htext[y] += tab[lval];
        }
    }

    // initialize the left and right borders of the disparity map
    for( y = 0; y < height; y++ )
    {
        for( x = 0; x < lofs; x++ )
            dptr[y*dstep + x] = FILTERED;
        for( x = lofs + width1; x < width; x++ )
            dptr[y*dstep + x] = FILTERED;
    }
    dptr += lofs;

    for( x = 0; x < width1; x++, dptr++ )
    {
        int x0 = x - wsz2 - 1, x1 = x + wsz2;
        const uchar* cbuf_sub = cbuf0 + ((x0 + wsz2 + 1) % (wsz + 1))*cstep - dy0*ndisp;
        uchar* cbuf = cbuf0 + ((x1 + wsz2 + 1) % (wsz + 1))*cstep - dy0*ndisp;
        hsad = hsad0 - dy0*ndisp;
        lptr_sub = lptr0 + MIN(MAX(x0, -lofs), width-1-lofs) - dy0*sstep;
        lptr = lptr0 + MIN(MAX(x1, -lofs), width-1-lofs) - dy0*sstep;
        rptr = rptr0 + MIN(MAX(x1, -rofs), width-1-rofs) - dy0*sstep;

        for( y = -dy0; y < height + dy1; y++, cbuf += ndisp, cbuf_sub += ndisp,
             hsad += ndisp, lptr += sstep, lptr_sub += sstep, rptr += sstep )
        {
            int lval = lptr[0];
            for( d = 0; d < ndisp; d++ )
            {
                int diff = abs(lval - rptr[d]);
                cbuf[d] = (uchar)diff;
                hsad[d] = hsad[d] + diff - cbuf_sub[d];
            }
            htext[y] += tab[lval] - tab[lptr_sub[0]];
        }

        // fill borders
        for( y = dy1; y <= wsz2; y++ )
            htext[height+y] = htext[height+dy1-1];
        for( y = -wsz2-1; y < -dy0; y++ )
            htext[y] = htext[-dy0];

        // initialize sums
        for( d = 0; d < ndisp; d++ )
            sad[d] = (int)(hsad0[d-ndisp*dy0]*(wsz2 + 2 - dy0));
        
        hsad = hsad0 + (1 - dy0)*ndisp;
        for( y = 1 - dy0; y < wsz2; y++, hsad += ndisp )
            for( d = 0; d < ndisp; d++ )
                sad[d] = (int)(sad[d] + hsad[d]);
        int tsum = 0;
        for( y = -wsz2-1; y < wsz2; y++ )
            tsum += htext[y];

        // finally, start the real processing
        for( y = 0; y < height; y++ )
        {
            int minsad = INT_MAX, mind = -1;
            hsad = hsad0 + MIN(y + wsz2, height+dy1-1)*ndisp;
            hsad_sub = hsad0 + MAX(y - wsz2 - 1, -dy0)*ndisp;

            for( d = 0; d < ndisp; d++ )
            {
                int currsad = sad[d] + hsad[d] - hsad_sub[d];
                sad[d] = currsad;
                if( currsad < minsad )
                {
                    minsad = currsad;
                    mind = d;
                }
            }
            tsum += htext[y + wsz2] - htext[y - wsz2 - 1];
            if( tsum < textureThreshold )
            {
                if( !dbmin )
                    dptr[y*dstep] = FILTERED;
                continue;
            }

            if( uniquenessRatio > 0 )
            {
                int thresh = minsad + (minsad * uniquenessRatio/100);
                for( d = 0; d < ndisp; d++ )
                {
                    if( sad[d] <= thresh && (d < mind-1 || d > mind+1))
                        break;
                }
                if( d < ndisp )
                {
                    if( !dbmin )
                        dptr[y*dstep] = FILTERED;
                    continue;
                }
            }

            if( dbmin )
            {
                int maxval = ((const short*)(dbmin->data.ptr + dbmin->step*y))[x];
                int minval = ((const short*)(dbmax->data.ptr + dbmax->step*y))[x];
                minval = (DELTA - minval) >> DISPARITY_SHIFT;
                maxval = (DELTA - maxval + (1<<DISPARITY_SHIFT)-1) >> DISPARITY_SHIFT;
                if( d < minval || d > maxval )
                    continue;
            }
            
            {
            sad[-1] = sad[1];
            sad[ndisp] = sad[ndisp-2];
            int p = sad[mind+1], n = sad[mind-1], d = p + n - 2*sad[mind];
            dptr[y*dstep] = (short)(((ndisp - mind - 1 + mindisp)*256 + (d != 0 ? (p-n)*128/d : 0) + 15) >> 4);
            }
        }
    }
}

static void
icvFilterSpeckles(CvMat* disp, short badval,
                  int dispDiff, int maxSpeckleSize,
                  CvMat* _buf)
{
    typedef cv::Point_<short> Point2s;
    int width = disp->cols, height = disp->rows;
    CV_Assert(CV_IS_MAT_CONT(_buf->type) && _buf->data.ptr &&
              _buf->cols*_buf->rows*CV_ELEM_SIZE(_buf->type) >=
              width*height*(int)(sizeof(Point2s) + sizeof(int) + sizeof(uchar)));
    uchar* buf = _buf->data.ptr;
    int dstep = disp->step/sizeof(short);
    int npixels = width*height;
    int i, j;
    int* labels = (int*)buf;
    buf += npixels*sizeof(labels[0]);
    Point2s* wbuf = (Point2s*)buf;
    buf += npixels*sizeof(wbuf[0]);
    uchar* rtype = (uchar*)buf;
    int curlabel = 0;
    
    // clear out label assignments
    memset(labels, 0, npixels*sizeof(labels[0]));
    
    for( i = 1; i < height-1; i++ )
    {
        short* ds = (short*)(disp->data.ptr + disp->step*i);
        int* ls = labels + width*i;
        
        for( j = 1; j < width-1; j++ )
        {
            if( ds[j] != badval )	// not a bad disparity
            {
                if( ls[j] )		// has a label, check for bad label
                {  
                    if( rtype[ls[j]] ) // small region, zero out disparity
                        ds[j] = badval;
                }
                // no label, assign and propagate
                else
                {
                    Point2s* ws = wbuf;	// initialize wavefront
                    Point2s p(j, i);	// current pixel
                    curlabel++;	// next label
                    int count = 0;	// current region size
                    ls[j] = curlabel;
                    
                    // wavefront propagation
                    while( ws >= wbuf ) // wavefront not empty
                    {
                        count++;
                        // put neighbors onto wavefront
                        short* dpp = (short*)(disp->data.ptr + disp->step*p.y) + p.x;
                        short dp = *dpp;
                        int* lpp = labels + width*p.y + p.x;
                        
                        if( p.x < width-1 && !lpp[+1] && dpp[+1] != badval && abs(dp - dpp[+1]) < dispDiff )
                        {
                            lpp[+1] = curlabel;
                            *ws++ = Point2s(p.x+1, p.y);
                        }
                        
                        if( p.x > 0 && !lpp[-1] && dpp[-1] != badval && abs(dp - dpp[-1]) < dispDiff )
                        {
                            lpp[-1] = curlabel;
                            *ws++ = Point2s(p.x-1, p.y);
                        }
                        
                        if( p.y < height-1 && !lpp[+width] && dpp[+dstep] != badval && abs(dp - dpp[+dstep]) < dispDiff )
                        {
                            lpp[+width] = curlabel;
                            *ws++ = Point2s(p.x, p.y+1);
                        }
                        
                        if( p.y > 0 && !lpp[-width] && dpp[-dstep] != badval && abs(dp - dpp[-dstep]) < dispDiff )
                        {
                            lpp[-width] = curlabel;
                            *ws++ = Point2s(p.x, p.y-1);
                        }
                        
                        // pop most recent and propagate
                        // NB: could try least recent, maybe better convergence
                        p = *--ws;
                    }
                    
                    // assign label type
                    if( count < maxSpeckleSize )	// speckle region
                    {
                        rtype[ls[j]] = 1;	// small region label
                        ds[j] = badval;
                    }
                    else
                        rtype[ls[j]] = 0;	// large region label
                }
            }
        }
    }
}


CV_IMPL void cvFindStereoCorrespondenceBM( const CvArr* leftarr, const CvArr* rightarr,
                                           CvArr* disparr, CvStereoBMState* state )
{
    CvMat lstub, *left0 = cvGetMat( leftarr, &lstub );
    CvMat rstub, *right0 = cvGetMat( rightarr, &rstub );
    CvMat left, right;
    CvMat dstub, *disp0 = cvGetMat( disparr, &dstub ), *disp = disp0;
    int bufSize0, bufSize1, bufSize2 = 0, bufSize, width, width1, height;
    int wsz, ndisp, mindisp, lofs, rofs, disptype;
    int i, n = cvGetNumThreads();
    int FILTERED, SMALL_WIN_SIZE;

    if( !CV_ARE_SIZES_EQ(left0, right0) ||
        !CV_ARE_SIZES_EQ(disp, left0) )
        CV_Error( CV_StsUnmatchedSizes, "All the images must have the same size" );

    if( CV_MAT_TYPE(left0->type) != CV_8UC1 ||
        !CV_ARE_TYPES_EQ(left0, right0) ||
        (CV_MAT_TYPE(disp->type) != CV_16SC1 &&
         CV_MAT_TYPE(disp->type) != CV_32FC1) )
        CV_Error( CV_StsUnsupportedFormat,
        "Both input images must have 8uC1 format and the disparity image must have 16sC1 format" );

    if( !state )
        CV_Error( CV_StsNullPtr, "Stereo BM state is NULL." );

    if( state->preFilterType != CV_STEREO_BM_NORMALIZED_RESPONSE &&
       state->preFilterType != CV_STEREO_BM_XSOBEL )
        CV_Error( CV_StsOutOfRange, "preFilterType must be =CV_STEREO_BM_NORMALIZED_RESPONSE" );

    if( state->preFilterSize < 5 || state->preFilterSize > 255 || state->preFilterSize % 2 == 0 )
        CV_Error( CV_StsOutOfRange, "preFilterSize must be odd and be within 5..255" );

    if( state->preFilterCap < 1 || state->preFilterCap > 63 )
        CV_Error( CV_StsOutOfRange, "preFilterCap must be within 1..63" );

    if( state->SADWindowSize < 5 || state->SADWindowSize > 255 || state->SADWindowSize % 2 == 0 ||
        state->SADWindowSize >= MIN(left0->cols, left0->rows) )
        CV_Error( CV_StsOutOfRange, "SADWindowSize must be odd, be within 5..255 and "
                                    "be not larger than image width or height" );

    if( state->numberOfDisparities <= 0 || state->numberOfDisparities % 16 != 0 )
        CV_Error( CV_StsOutOfRange, "numberOfDisparities must be positive and divisble by 16" );
    if( state->textureThreshold < 0 )
        CV_Error( CV_StsOutOfRange, "texture threshold must be non-negative" );
    if( state->uniquenessRatio < 0 )
        CV_Error( CV_StsOutOfRange, "uniqueness ratio must be non-negative" );

    if( !state->preFilteredImg0 ||
        state->preFilteredImg0->cols*state->preFilteredImg0->rows < left0->cols*left0->rows )
    {
        cvReleaseMat( &state->preFilteredImg0 );
        cvReleaseMat( &state->preFilteredImg1 );

        state->preFilteredImg0 = cvCreateMat( left0->rows, left0->cols, CV_8U );
        state->preFilteredImg1 = cvCreateMat( left0->rows, left0->cols, CV_8U );
    }
    left = cvMat(left0->rows, left0->cols, CV_8U, state->preFilteredImg0->data.ptr);
    right = cvMat(right0->rows, right0->cols, CV_8U, state->preFilteredImg1->data.ptr);
    
    mindisp = state->minDisparity;
    ndisp = state->numberOfDisparities;

    width = left0->cols;
    height = left0->rows;
    lofs = MAX(ndisp - 1 + mindisp, 0);
    rofs = -MIN(ndisp - 1 + mindisp, 0);
    width1 = width - rofs - ndisp + 1;
    FILTERED = (short)((state->minDisparity - 1) << DISPARITY_SHIFT);
    disptype = CV_MAT_TYPE(disp->type);
    
    if( lofs >= width || rofs >= width || width1 < 1 )
    {
        cvSet( disp, cvScalarAll(FILTERED*(disptype == CV_32S ? 1 : 1./(1 << DISPARITY_SHIFT))) );
        return;
    }
              
    if( disptype == CV_32F )
    {
        if( !state->disp || !CV_ARE_SIZES_EQ(state->disp, disp) )
        {
            cvReleaseMat( &state->disp );
            state->disp = cvCreateMat(disp->rows, disp->cols, CV_16S);
        }
        disp = state->disp;
    }
    
    wsz = state->SADWindowSize;
    bufSize0 = (ndisp + 2)*sizeof(int) + (height+wsz+2)*ndisp*sizeof(int) +
        (height + wsz + 2)*sizeof(int) + (height+wsz+2)*ndisp*(wsz+1)*sizeof(uchar) + 256;
    bufSize1 = (width + state->preFilterSize + 2)*sizeof(int) + 256;
    bufSize = MAX(bufSize0, bufSize1);
    
    n = MAX(MIN(height/wsz, n), 1);

    if( state->speckleRange >= 0 && state->speckleWindowSize > 0 )
        bufSize2 = width*height*(sizeof(cv::Point_<short>) + sizeof(int) + sizeof(uchar));
    
    if( !state->slidingSumBuf || state->slidingSumBuf->cols < MAX(bufSize*n, bufSize2) )
    {
        cvReleaseMat( &state->slidingSumBuf );
        state->slidingSumBuf = cvCreateMat( 1, MAX(bufSize*n, bufSize2), CV_8U );
    }

#ifdef _OPENMP
#pragma omp parallel sections num_threads(n)
#endif
    {
    #ifdef _OPENMP
    #pragma omp section
    #endif
        if( state->preFilterType == CV_STEREO_BM_NORMALIZED_RESPONSE )
            icvPrefilterNorm( left0, &left, state->preFilterSize,
                              state->preFilterCap, state->slidingSumBuf->data.ptr );
        else
            icvPrefilterXSobel( left0, &left, state->preFilterCap );
    #ifdef _OPENMP
    #pragma omp section
    #endif
        if( state->preFilterType == CV_STEREO_BM_NORMALIZED_RESPONSE )
            icvPrefilterNorm( right0, &right, state->preFilterSize,
                              state->preFilterCap,
                              state->slidingSumBuf->data.ptr + bufSize1*(n>1) );
        else
            icvPrefilterXSobel( right0, &right, state->preFilterCap );
    }

#ifdef _OPENMP
    #pragma omp parallel for num_threads(n) schedule(static)
#endif
    for( i = 0; i < n; i++ )
    {
        int thread_id = cvGetThreadNum();
        CvMat left_i, right_i, disp_i;
        int row0 = i*left.rows/n, row1 = (i+1)*left.rows/n;
        cvGetRows( &left, &left_i, row0, row1 );
        cvGetRows( &right, &right_i, row0, row1 );
        cvGetRows( disp, &disp_i, row0, row1 );
    #if CV_SSE2
        if( state->preFilterCap <= 31 && state->SADWindowSize <= 21 )
        {
            icvFindStereoCorrespondenceBM_SSE2( &left_i, &right_i, &disp_i, 0, 0, state,
                state->SADWindowSize, state->slidingSumBuf->data.ptr + thread_id*bufSize0,
                row0, left.rows-row1 );
        }
        else
    #endif
        {
            icvFindStereoCorrespondenceBM( &left_i, &right_i, &disp_i, 0, 0, state,
                state->SADWindowSize, state->slidingSumBuf->data.ptr + thread_id*bufSize0,
                row0, left.rows-row1 );
        }
    }

    SMALL_WIN_SIZE = MAX((state->SADWindowSize/2)|1, 9);

    if( state->trySmallerWindows && SMALL_WIN_SIZE < state->SADWindowSize )
    {
        if( !state->dbmin || !CV_ARE_SIZES_EQ(state->dbmin, disp) )
        {
            cvReleaseMat( &state->dbmin );
            cvReleaseMat( &state->dbmax );
            state->dbmin = cvCreateMat( disp->rows, disp->cols, CV_16SC1 );
            state->dbmax = cvCreateMat( disp->rows, disp->cols, CV_16SC1 );
        }
        
        for( i = 0; i < disp->rows; i++ )
        {
            int j;
            short* minptr = (short*)(state->dbmin->data.ptr + state->dbmin->step*i);
            short* maxptr = (short*)(state->dbmax->data.ptr + state->dbmax->step*i);

            for( j = 0; j < disp->cols; j++ )
            {
                short dval = ((const short*)(disp->data.ptr + disp->step*i))[j];
                if( dval < (state->minDisparity << DISPARITY_SHIFT) )
                    minptr[j] = maxptr[j] = dval;
                else
                {
                    minptr[j] = SHRT_MAX;
                    maxptr[j] = SHRT_MIN;
                }
            }
        }

        cvErode(state->dbmin, state->dbmin, 0, SMALL_WIN_SIZE );
        cvDilate(state->dbmax, state->dbmax, 0, SMALL_WIN_SIZE );

    #ifdef _OPENMP
        #pragma omp parallel for num_threads(n) schedule(static)
    #endif
        for( i = 0; i < n; i++ )
        {
            int thread_id = cvGetThreadNum();
            CvMat left_i, right_i, disp_i, dbmin_i, dbmax_i;
            int row0 = i*left.rows/n, row1 = (i+1)*left.rows/n;
            cvGetRows( &left, &left_i, row0, row1 );
            cvGetRows( &right, &right_i, row0, row1 );
            cvGetRows( disp, &disp_i, row0, row1 );
            cvGetRows( state->dbmin, &dbmin_i, row0, row1 );
            cvGetRows( state->dbmax, &dbmax_i, row0, row1 );
        #if CV_SSE2
            if( state->preFilterCap <= 31 && SMALL_WIN_SIZE <= 21 )
            {
                icvFindStereoCorrespondenceBM_SSE2( &left_i, &right_i,
                    &disp_i, &dbmin_i, &dbmax_i, state, SMALL_WIN_SIZE,
                    state->slidingSumBuf->data.ptr + thread_id*bufSize0,
                    row0, left.rows-row1 );
            }
            else
        #endif
            {
                icvFindStereoCorrespondenceBM( &left_i, &right_i,
                    &disp_i, &dbmin_i, &dbmax_i, state, SMALL_WIN_SIZE,
                    state->slidingSumBuf->data.ptr + thread_id*bufSize0,
                    row0, left.rows-row1 );
            }
        }
    }
    
    if( state->speckleRange >= 0 && state->speckleWindowSize > 0 )
        icvFilterSpeckles(disp, FILTERED, state->speckleRange,
                          state->speckleWindowSize, state->slidingSumBuf);
    
    if( disp0 != disp )
        cvConvertScale( disp, disp0, 1./(1 << DISPARITY_SHIFT), 0 );
}

namespace cv
{

StereoBM::StereoBM()
{ state = cvCreateStereoBMState(); }

StereoBM::StereoBM(int _preset, int _ndisparities, int _SADWindowSize)
{ init(_preset, _ndisparities, _SADWindowSize); }

void StereoBM::init(int _preset, int _ndisparities, int _SADWindowSize)
{
    state = cvCreateStereoBMState(_preset, _ndisparities);
    state->SADWindowSize = _SADWindowSize;
}

void StereoBM::operator()( const Mat& left, const Mat& right, Mat& disparity, int disptype )
{
    CV_Assert( disptype == CV_16S || disptype == CV_32F );
    disparity.create(left.size(), disptype);
    CvMat _left = left, _right = right, _disparity = disparity;
    cvFindStereoCorrespondenceBM(&_left, &_right, &_disparity, state);
}

}

/* End of file. */
