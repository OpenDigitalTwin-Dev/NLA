/*****************************************************************************
  Copyright (c) 2014, Intel Corp.
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice,
      this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of Intel Corporation nor the names of its contributors
      may be used to endorse or promote products derived from this software
      without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
  THE POSSIBILITY OF SUCH DAMAGE.
*****************************************************************************
* Contents: Native high-level C interface to LAPACK function strsna
* Author: Intel Corporation
*****************************************************************************/

#include "lapacke_utils.h"

lapack_int API_SUFFIX(LAPACKE_strsna)( int matrix_layout, char job, char howmny,
                           const lapack_logical* select, lapack_int n,
                           const float* t, lapack_int ldt, const float* vl,
                           lapack_int ldvl, const float* vr, lapack_int ldvr,
                           float* s, float* sep, lapack_int mm, lapack_int* m )
{
    lapack_int info = 0;
    lapack_int ldwork =  API_SUFFIX(LAPACKE_lsame)( job, 'e' ) ? 1 : MAX(1,n) ;
    lapack_int* iwork = NULL;
    float* work = NULL;
    if( matrix_layout != LAPACK_COL_MAJOR && matrix_layout != LAPACK_ROW_MAJOR ) {
        API_SUFFIX(LAPACKE_xerbla)( "LAPACKE_strsna", -1 );
        return -1;
    }
#ifndef LAPACK_DISABLE_NAN_CHECK
    if( LAPACKE_get_nancheck() ) {
        /* Optionally check input matrices for NaNs */
        if( API_SUFFIX(LAPACKE_sge_nancheck)( matrix_layout, n, n, t, ldt ) ) {
            return -6;
        }
        if( API_SUFFIX(LAPACKE_lsame)( job, 'b' ) || API_SUFFIX(LAPACKE_lsame)( job, 'e' ) ) {
            if( API_SUFFIX(LAPACKE_sge_nancheck)( matrix_layout, n, mm, vl, ldvl ) ) {
                return -8;
            }
        }
        if( API_SUFFIX(LAPACKE_lsame)( job, 'b' ) || API_SUFFIX(LAPACKE_lsame)( job, 'e' ) ) {
            if( API_SUFFIX(LAPACKE_sge_nancheck)( matrix_layout, n, mm, vr, ldvr ) ) {
                return -10;
            }
        }
    }
#endif
    /* Allocate memory for working array(s) */
    if( API_SUFFIX(LAPACKE_lsame)( job, 'b' ) || API_SUFFIX(LAPACKE_lsame)( job, 'v' ) ) {
        iwork = (lapack_int*)
            LAPACKE_malloc( sizeof(lapack_int) * MAX(1,2*(n-1)) );
        if( iwork == NULL ) {
            info = LAPACK_WORK_MEMORY_ERROR;
            goto exit_level_0;
        }
    }
    if( API_SUFFIX(LAPACKE_lsame)( job, 'b' ) || API_SUFFIX(LAPACKE_lsame)( job, 'v' ) ) {
        work = (float*)LAPACKE_malloc( sizeof(float) * ldwork * MAX(1,n+6) );
        if( work == NULL ) {
            info = LAPACK_WORK_MEMORY_ERROR;
            goto exit_level_1;
        }
    }
    /* Call middle-level interface */
    info = API_SUFFIX(LAPACKE_strsna_work)( matrix_layout, job, howmny, select, n, t, ldt,
                                vl, ldvl, vr, ldvr, s, sep, mm, m, work, ldwork,
                                iwork );
    /* Release memory and exit */
    if( API_SUFFIX(LAPACKE_lsame)( job, 'b' ) || API_SUFFIX(LAPACKE_lsame)( job, 'v' ) ) {
        LAPACKE_free( work );
    }
exit_level_1:
    if( API_SUFFIX(LAPACKE_lsame)( job, 'b' ) || API_SUFFIX(LAPACKE_lsame)( job, 'v' ) ) {
        LAPACKE_free( iwork );
    }
exit_level_0:
    if( info == LAPACK_WORK_MEMORY_ERROR ) {
        API_SUFFIX(LAPACKE_xerbla)( "LAPACKE_strsna", info );
    }
    return info;
}