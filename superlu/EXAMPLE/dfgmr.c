/* ITSOL COPYRIGHT

Copyright (C) 2006, the University of Minnesota 

ITSOL is free software; you can redistribute it and/or modify it under
the terms of  the GNU General Public License as  published by the Free
Software Foundation [version 2 of the License, or any later version]
For details, see 

http://www.gnu.org/licenses/gpl-2.0.txt

A copy of the GNU licencing agreement is attached to the ITSOL package
in the file GNU.  For additional information contact the Free Software
Foundation, 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA. 

DISCLAIMER
----------

This program  is distributed in the  hope that it will  be useful, but
WITHOUT   ANY  WARRANTY;   without  even   the  implied   warranty  of
MERCHANTABILITY  or FITNESS  FOR A  PARTICULAR PURPOSE.   See  the GNU
General Public License for more details. 

For information on ITSOL contact saad@cs.umn.edu
*/

/*! \file
 * \brief flexible GMRES from ITSOL developed by Yousef Saad.
 *
 * \ingroup Example
 */

#include "slu_ddefs.h"

#define  epsmac  1.0e-16

extern double ddot_(int *, double [], int *, double [], int *);
extern double dnrm2_(int *, double [], int *);

/*!
 * \brief Simple version of the ARMS preconditioned FGMRES algorithm.
 *
 *  Y. S. Dec. 2000. -- Apr. 2008
 *
 *  internal work arrays:
 *  vv      = work array of length [im+1][n] (used to store the Arnoldi
 *            basis)
 *  hh      = work array of length [im][im+1] (Householder matrix)
 *  z       = work array of length [im][n] to store preconditioned vectors
 *
 * \param [in] n         Dimension of vectors and matrices.
 * \param [in] dmatvec   Operation for matrix-vector multiplication.
 * \param [in] dpsolve   (right) preconditioning operation. Can be a NULL pointer (GMRES without preconditioner)
 * \param [in] rhs       Real vector of length n containing the right hand side.
 * \param [in,out] sol   In: Real vector of length n containing an initial guess to the solution on input.
 *                       Out: Contains an approximate solution (upon successful return).
 * \param [in] tol       Tolerance for stopping iteration
 * \param [in] im        Krylov subspace dimension
 * \param [in,out] itmax In: max number of iterations allowed.
 *                       Out: number of steps required to converge.
 * \param [in] fits      If NULL, no output. If not NULL, file handle to output "resid vs time and its".
 * \return Whether the algorithm finished successfully.
 */
int dfgmr(int n,
          void (*dmatvec) (double, double[], double, double[]),
          void (*dpsolve) (int, double[], double[]),
          double *rhs, double *sol, double tol, int im, int *itmax, FILE * fits)
{
    int maxits = *itmax;
    int its, i_1 = 1;
    double eps1 = 0.0;
    double **hh, *c, *s, *rs;
    double **vv, **z;
    double zero = 0.0;
    double one = 1.0;

    /* prototypes */
    extern int dcopy_(int *, double *, int *, double *, int *);
    extern int daxpy_(int *, double *, double [], int *, double [], int *);

    its = 0;
    vv = (double **)SUPERLU_MALLOC((im + 1) * sizeof(double *));
    for (int i = 0; i <= im; i++)
        vv[i] = doubleMalloc(n);
    z = (double **)SUPERLU_MALLOC(im * sizeof(double *));
    hh = (double **)SUPERLU_MALLOC(im * sizeof(double *));
    for (int i = 0; i < im; i++)
    {
	hh[i] = doubleMalloc(i + 2);
	z[i] = doubleMalloc(n);
    }
    c = doubleMalloc(im);
    s = doubleMalloc(im);
    rs = doubleMalloc(im + 1);

    /*---- outer loop starts here ----*/
    do
    {
	/*---- compute initial residual vector ----*/
	dmatvec(one, sol, zero, vv[0]);
        for (int j = 0; j < n; j++)
            vv[0][j] = rhs[j] - vv[0][j];	/* vv[0]= initial residual */
        double beta = dnrm2_(&n, vv[0], &i_1);

	/*---- print info if fits != null ----*/
	if (fits != NULL && its == 0)
	    fprintf(fits, "%8d   %10.2e\n", its, beta);
	/*if ( beta <= tol * dnrm2_(&n, rhs, &i_1) )*/
	if ( !(beta > tol * dnrm2_(&n, rhs, &i_1)) )
	    break;
        double t = 1.0 / beta;

	/*---- normalize: vv[0] = vv[0] / beta ----*/
        for (int j = 0; j < n; j++)
	    vv[0][j] = vv[0][j] * t;
	if (its == 0)
	    eps1 = tol * beta;

	/*---- initialize 1-st term of rhs of hessenberg system ----*/
	rs[0] = beta;
        int i = 0;
        for (i = 0; i < im; i++)
        {
            its++;
            int i1 = i + 1;

	    /*------------------------------------------------------------
	    |  (Right) Preconditioning Operation   z_{j} = M^{-1} v_{j}
	    +-----------------------------------------------------------*/
	    if (dpsolve)
		dpsolve(n, z[i], vv[i]);
	    else
		dcopy_(&n, vv[i], &i_1, z[i], &i_1);

	    /*---- matvec operation w = A z_{j} = A M^{-1} v_{j} ----*/
	    dmatvec(one, z[i], zero, vv[i1]);

	    /*------------------------------------------------------------
	    |     modified gram - schmidt...
	    |     h_{i,j} = (w,v_{i})
	    |     w  = w - h_{i,j} v_{i}
	    +------------------------------------------------------------*/
            double t0 = dnrm2_(&n, vv[i1], &i_1);
            for (int j = 0; j <= i; j++)
            {
		double negt;
                double tt = ddot_(&n, vv[j], &i_1, vv[i1], &i_1);
		hh[i][j] = tt;
		negt = -tt;
		daxpy_(&n, &negt, vv[j], &i_1, vv[i1], &i_1);
	    }

	    /*---- h_{j+1,j} = ||w||_{2} ----*/
	    t = dnrm2_(&n, vv[i1], &i_1);
	    while (t < 0.5 * t0)
	    {
		t0 = t;
                for (int j = 0; j <= i; j++)
                {
		    double negt;
                    double tt = ddot_(&n, vv[j], &i_1, vv[i1], &i_1);
		    hh[i][j] += tt;
		    negt = -tt;
		    daxpy_(&n, &negt, vv[j], &i_1, vv[i1], &i_1);
		}
		t = dnrm2_(&n, vv[i1], &i_1);
	    }

	    hh[i][i1] = t;

	    if (t != 0.0)
	    {
		/*---- v_{j+1} = w / h_{j+1,j} ----*/
		t = 1.0 / t;
                for (int k = 0; k < n; k++)
		    vv[i1][k] = vv[i1][k] * t;
	    }
	    /*---------------------------------------------------
	    |     done with modified gram schimdt and arnoldi step
	    |     now  update factorization of hh
	    +--------------------------------------------------*/

	    /*--------------------------------------------------------
	    |   perform previous transformations  on i-th column of h
	    +-------------------------------------------------------*/
            for (int k = 1; k <= i; k++)
            {
                int k1 = k - 1;
                double tt = hh[i][k1];
		hh[i][k1] = c[k1] * tt + s[k1] * hh[i][k];
		hh[i][k] = -s[k1] * tt + c[k1] * hh[i][k];
	    }

            double gam = sqrt(pow(hh[i][i], 2) + pow(hh[i][i1], 2));

	    /*---------------------------------------------------
	    |     if gamma is zero then any small value will do
	    |     affect only residual estimate
	    +--------------------------------------------------*/
	    /* if (gam == 0.0) gam = epsmac; */

	    /*---- get next plane rotation ---*/
	    if (gam == 0.0)
	    {
		c[i] = one;
		s[i] = zero;
	    }
            else
	    {
		c[i] = hh[i][i] / gam;
		s[i] = hh[i][i1] / gam;
	    }

	    rs[i1] = -s[i] * rs[i];
	    rs[i] = c[i] * rs[i];

	    /*----------------------------------------------------
	    |   determine residual norm and test for convergence
	    +---------------------------------------------------*/
	    hh[i][i] = c[i] * hh[i][i] + s[i] * hh[i][i1];
	    beta = fabs(rs[i1]);
	    if (fits != NULL)
		fprintf(fits, "%8d   %10.2e\n", its, beta);
	    if (beta <= eps1 || its >= maxits)
		break;
	}

	if (i == im) i--;

	/*---- now compute solution. 1st, solve upper triangular system ----*/
	rs[i] = rs[i] / hh[i][i];

        for (int ii = 1; ii <= i; ii++)
        {
            int k = i - ii;
            double tt = rs[k];
            for (int j = k + 1; j <= i; j++)
                tt = tt - hh[j][k] * rs[j];
	    rs[k] = tt / hh[k][k];
	}

	/*---- linear combination of v[i]'s to get sol. ----*/
        for (int j = 0; j <= i; j++)
        {
            double tt = rs[j];
            for (int k = 0; k < n; k++)
		sol[k] += tt * z[j][k];
        }

	/* calculate the residual and output */
	dmatvec(one, sol, zero, vv[0]);
        for (int j = 0; j < n; j++)
            vv[0][j] = rhs[j] - vv[0][j]; /* vv[0]= initial residual */

	/*---- print info if fits != null ----*/
	beta = dnrm2_(&n, vv[0], &i_1);

	/*---- restart outer loop if needed ----*/
	/*if (beta >= eps1 / tol)*/
	if ( !(beta < eps1 / tol) )
	{
	    its = maxits + 10;
	    break;
	}
	if (beta <= eps1)
	    break;
    } while(its < maxits);

    int retval = (its >= maxits);
    for (int i = 0; i <= im; i++)
        SUPERLU_FREE(vv[i]);
    SUPERLU_FREE(vv);
    for (int i = 0; i < im; i++)
    {
        SUPERLU_FREE(hh[i]);
        SUPERLU_FREE(z[i]);
    }
    SUPERLU_FREE(hh);
    SUPERLU_FREE(z);
    SUPERLU_FREE(c);
    SUPERLU_FREE(s);
    SUPERLU_FREE(rs);

    *itmax = its;

    return retval;
} /*----end of fgmr ----*/