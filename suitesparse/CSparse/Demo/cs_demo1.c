// CSparse/Demo/cs_demo1: CSparse demo program
// CSparse, Copyright (c) 2006-2022, Timothy A. Davis. All Rights Reserved.
// SPDX-License-Identifier: LGPL-2.1+
#include "cs.h"
int main (void)
{
    cs *T, *A, *Eye, *AT, *C, *D ;
    csi i, m ;
    int version [3] ;
    csparse_version (version) ;
    printf ("CSparse v%d.%d.%d\n", version [0], version [1], version [2]) ;

    #ifndef TEST_COVERAGE
    if ((version [0] != CS_VER) || (version [1] != CS_SUBVER) ||
        (version [2] != CS_SUBSUB))
    {
        fprintf (stderr, "version in header does not match library\n") ;
        abort ( ) ;
    }
    #endif

    T = cs_load (stdin) ;               /* load triplet matrix T from stdin */
    printf ("T:\n") ; cs_print (T, 0) ; /* print T */
    A = cs_compress (T) ;               /* A = compressed-column form of T */
    printf ("A:\n") ; cs_print (A, 0) ; /* print A */
    cs_spfree (T) ;                     /* clear T */
    AT = cs_transpose (A, 1) ;          /* AT = A' */
    printf ("AT:\n") ; cs_print (AT, 0) ; /* print AT */
    m = A ? A->m : 0 ;                  /* m = # of rows of A */
    T = cs_spalloc (m, m, m, 1, 1) ;    /* create triplet identity matrix */
    for (i = 0 ; i < m ; i++) cs_entry (T, i, i, 1) ;
    Eye = cs_compress (T) ;             /* Eye = speye (m) */
    cs_spfree (T) ;
    C = cs_multiply (A, AT) ;           /* C = A*A' */
    D = cs_add (C, Eye, 1, cs_norm (C)) ;   /* D = C + Eye*norm (C,1) */
    printf ("D:\n") ; cs_print (D, 0) ; /* print D */
    cs_spfree (A) ;                     /* clear A AT C D Eye */
    cs_spfree (AT) ;
    cs_spfree (C) ;
    cs_spfree (D) ;
    cs_spfree (Eye) ;
    return (0) ;
}
