/*
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
   SLEPc - Scalable Library for Eigenvalue Problem Computations
   Copyright (c) 2002-, Universitat Politecnica de Valencia, Spain

   This file is part of SLEPc.
   SLEPc is distributed under a 2-clause BSD license (see LICENSE).
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
*/

static char help[] = "Partial hyperbolic singular value decomposition (HSVD) of matrix generated by plane rotations.\n"
  "The command line options are:\n"
  "  -p <p>, number of rows in upper part of matrix A.\n"
  "  -q <q>, number of rows in lower part of matrix A.\n"
  "  -n <n>, number of columns of matrix A.\n"
  "  -k <k>, condition number.\n"
  "  -d <d>, density.\n"
  "  -prog, show progress of matrix generation\n";

#include <slepcsvd.h>
#include <slepcblaslapack.h>

/* The problem matrix is the (p+q)-by-n matrix

     A =  |     Q1 * D1 * U |
          | 1/2*Q2 * D2 * U |

 with p>=n, where Q1 (p-by-p), Q2 (q-by-q) and U (n-by-n) are orthonormal
 matrices, D1 is a diagonal p-by-n matrix with elements
     D1(i,i) = sqrt(5/4)*d(i), if i<=q
             = d(i),           if i>q
 D2 is a diagonal q-by-n matrix with elements D2(i,i)= d(i),
 and d is an n-element vector with elements distributed exponentially
 from 1/k to 1.

 The signature matrix is Omega = diag(I_p,-I_q)

 The hyperbolic singular values of A are equal to the elements of vector d.

 A is generated by random plane rotations applied to the diagonal matrices
 D1 and D2

 This test case is based on the paper:

 [1] A. Bojanczyk, N.J. Higham, and H. Patel, "Solving the Indefinite Least
     Squares Problem by Hyperbolic QR Factorization", SIAM J. Matrix Anal.
     Appl. 24(4):914-931 2003
*/

/* Timing */
#ifdef TIMING
  static PetscReal tt_rotr,tt_rotc,tt_assmr=0.0,tt_assmc=0.0,tt_getr=0.0,tt_getc=0.0,tt_setvalr=0.0,tt_setvalc=0.0;
  #define time_now(t) t=MPI_Wtime();
  #define time_diff(tacum,t0,t,t1) {t=MPI_Wtime(); tacum += t-t0; t1=t;}
#else
  #define time_diff(tacum,t0,t,t1)
  #define time_now(t)
#endif

struct _p_XMat {
  MPI_Comm    comm;
  PetscInt    M,N;         /* global number of rows and columns */
  PetscInt    Istart,Iend; /* Index of 1st row and last local rows */
  PetscInt    *nzr;        /* number of non-zeros in each row */
  PetscInt    **rcol;      /* row columns */
  PetscInt    *ranges;     /* proc row ranges */
  PetscScalar **rval;      /* row values */
};

typedef struct _p_XMat *XMat;

/* Iterator over non-zero rows of 2 columns */
struct _p_ColsNzIter {
  XMat      A;
  PetscInt  j1,j2;    /* Columns considered */
  PetscInt  iloc;     /* current local row */
};

typedef struct _p_ColsNzIter *ColsNzIter;

static PetscErrorCode XMatCreate(MPI_Comm comm,XMat *A,PetscInt Istart,PetscInt Iend,PetscInt M,PetscInt N)
{
  PetscInt    i;
  PetscMPIInt np;

  PetscFunctionBeginUser;
  PetscCall(PetscNew(A));
  (*A)->M = M;
  (*A)->N = N;
  (*A)->Istart = Istart;
  (*A)->Iend = Iend;
  (*A)->comm = comm;
  PetscCallMPI(MPI_Comm_size(comm,&np));
  PetscCall(PetscMalloc4(Iend-Istart,&(*A)->nzr,Iend-Istart,&(*A)->rcol,Iend-Istart,&(*A)->rval,np+1,&(*A)->ranges));
  for (i=0; i<Iend-Istart; i++) {
    (*A)->nzr[i] = 0;
    (*A)->rcol[i] = NULL;
    (*A)->rval[i] = NULL;
  }
  PetscCallMPI(MPI_Allgather(&Istart,1,MPIU_INT,(*A)->ranges,1,MPIU_INT,comm));
  (*A)->ranges[np]=M;
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode XMatDestroy(XMat *A)
{
  PetscInt m,i;

  PetscFunctionBeginUser;
  m = (*A)->Iend-(*A)->Istart;
  for (i=0; i<m; i++) {
    if ((*A)->nzr[i]>0) {
      PetscCall(PetscFree2((*A)->rcol[i],(*A)->rval[i]));
    }
  }
  PetscCall(PetscFree4((*A)->nzr,(*A)->rcol,(*A)->rval,(*A)->ranges));
  PetscCall(PetscFree(*A));
  *A = NULL;
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode XMatGetSize(XMat A,PetscInt *M,PetscInt *N)
{
  PetscFunctionBeginUser;
  *M = A->M;
  *N = A->N;
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode XMatGetOwnershipRange(XMat A,PetscInt *Istart,PetscInt *Iend)
{
  PetscFunctionBeginUser;
  *Istart = A->Istart;
  *Iend = A->Iend;
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode XMatGetOwnershipRanges(XMat A,PetscInt **ranges)
{
  PetscFunctionBeginUser;
  *ranges = A->ranges;
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode XMatGetRow(XMat A,PetscInt i,PetscInt *nc,PetscInt **vj,PetscScalar **va)
{
  PetscInt iloc=i-A->Istart;

  PetscFunctionBeginUser;
  *nc = A->nzr[iloc];
  *vj = A->rcol[iloc];
  *va = A->rval[iloc];
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode XMatRestoreRow(XMat A,PetscInt i,PetscInt *nc,PetscInt **vj,PetscScalar **va)
{
  PetscFunctionBeginUser;
  *nc = 0;
  *vj = NULL;
  *va = NULL;
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode XMatChangeRow(XMat A,PetscInt i,PetscInt nc,PetscInt vj[],PetscScalar va[])
{
  PetscInt iloc = i-A->Istart;

  PetscFunctionBeginUser;
  if (A->nzr[iloc]>0) {
    PetscCall(PetscFree2(A->rcol[iloc],A->rval[iloc]));
  }
  A->nzr[iloc] = nc;
  A->rcol[iloc] = vj;
  A->rval[iloc] = va;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/* Given a row with non-zero elements in columns indicated by rcol, get the
   position where the non-zero element of column j is, or where it should be inserted.
   Returns true if there is a non-zero element for the column, false otherwise */
static PetscBool getcolpos(const PetscInt rcol[],PetscInt nz,PetscInt j,PetscInt *k)
{
  PetscInt ka,kb;
  if (nz==0) {
    *k = 0;
    return PETSC_FALSE;
  }
  if (j<=rcol[0]) {
    *k = 0;
    return (rcol[0]==j)? PETSC_TRUE: PETSC_FALSE;
  }
  if (rcol[nz-1]<=j) {
    *k = nz-(rcol[nz-1]==j);
    return (rcol[nz-1]==j)? PETSC_TRUE: PETSC_FALSE;
  }
  /* Bisection: rcol[ka] < j < rcol[kb] */
  ka = 0; kb = nz-1;
  while (ka+1<kb) {
    *k = (ka+kb)/2;
    if (rcol[*k]<=j) {
      if (rcol[*k]==j) return PETSC_TRUE;
      else ka = *k;
    } else kb = *k;
  }
  *k = kb;
  return PETSC_FALSE;
}

/* Only local elements can be set */
static PetscErrorCode XMatSetValue(XMat A,PetscInt i,PetscInt j,PetscScalar x,InsertMode dummy)
{
  PetscInt    nz,iloc,k,kx,*col1,*col2;
  PetscScalar *val1,*val2;

  PetscFunctionBeginUser;
  PetscCheck(i>=A->Istart && i<A->Iend && j>=0 && j<A->N,A->comm,PETSC_ERR_ARG_OUTOFRANGE,"Incorrect index of row/column");
  iloc = i-A->Istart;
  nz = A->nzr[iloc];
  col1 = A->rcol[iloc];
  val1 = A->rval[iloc];
  if (!getcolpos(col1,nz,j,&k)) {
    A->nzr[iloc]++;
    /* Replace row */
    PetscCall(PetscMalloc2(nz+1,&col2,nz+1,&val2));
    A->rcol[iloc] = col2;
    A->rval[iloc] = val2;
    for (kx=0; kx<k; kx++) {
      col2[kx] = col1[kx];
      val2[kx] = val1[kx];
    }
    for (; kx<nz; kx++) {
      col2[kx+1] = col1[kx];
      val2[kx+1] = val1[kx];
    }
    if (nz>0) {
      PetscCall(PetscFree2(col1,val1));
    }
    col1 = col2;
    val1 = val2;
  }
  col1[k] = j;
  val1[k] = x;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/* Convert to PETSc Mat */
static PetscErrorCode XMatConvert(XMat XA,Mat A)
{
  PetscInt    i,iloc;

  PetscFunctionBeginUser;
  for (i=XA->Istart; i<XA->Iend; i++) {
    iloc = i-XA->Istart;
    PetscCall(MatSetValues(A,1,&i,XA->nzr[iloc],XA->rcol[iloc],XA->rval[iloc],INSERT_VALUES));
  }
  PetscCall(MatAssemblyBegin(A,MAT_FINAL_ASSEMBLY));
  PetscCall(MatAssemblyEnd(A,MAT_FINAL_ASSEMBLY));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/* gets the number of nonzeros in the matrix on this MPI rank */
static PetscErrorCode XMatGetNumberNonzeros(XMat A,PetscInt *nz)
{
  PetscInt i;

  PetscFunctionBeginUser;
  *nz = 0;
  for (i=0; i<A->Iend-A->Istart; i++) *nz += A->nzr[i];
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode XMatCreateColsNzIter(XMat A,PetscInt j1,PetscInt j2,ColsNzIter *iter)
{
  PetscFunctionBeginUser;
  PetscCall(PetscNew(iter));
  (*iter)->A = A;
  (*iter)->j1 = j1;
  (*iter)->j2 = j2;
  (*iter)->iloc = 0;
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode ColitDestroy(ColsNzIter *iter)
{
  PetscFunctionBeginUser;
  PetscCall(PetscFree(*iter));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode ColitNextRow(ColsNzIter iter,PetscInt *i,PetscScalar **paij1,PetscScalar **paij2)
{
  PetscInt    iloc = iter->iloc,*rcol,nz,k,k1,k2,kx,jx,*rcol2;
  PetscScalar *rval,*rval2;
  PetscBool   found1, found2;
  XMat        A=iter->A;

  PetscFunctionBeginUser;
  *i = -1;
  *paij1 = *paij2 = NULL;
  if (iloc>=0) {
    rcol =  A->rcol[iloc];
    nz = A->nzr[iloc];
    while (PETSC_TRUE) {
      found1 = getcolpos(rcol,nz,iter->j1,&k1);
      found2 = getcolpos(rcol+k1+found1,nz-k1-found1,iter->j2,&k2);
      if (found1 || found2) break;
      iloc++;
      if (iloc>=A->Iend-A->Istart) {
        iloc = -1;
        break;
      }
      rcol = A->rcol[iloc];
      nz = A->nzr[iloc];
    }
    if (iloc>=0) {
      k2 += k1 + 1;
      rval = A->rval[iloc];
      *i = A->Istart+iloc;
      if (!found1 || !found2) {
        /* Copy row to a new one */
        rcol2 = rcol;
        rval2 = rval;
        PetscCall(PetscMalloc2(nz+1,&rcol,nz+1,&rval));
        if (found1) {
          kx = k2;
          jx = iter->j2;
        } else {
          kx = k1;
          jx = iter->j1;
        }
        for (k=0; k<kx; k++) {
          rcol[k] = rcol2[k];
          rval[k] = rval2[k];
        }
        rcol[kx] = jx;
        rval[kx] = 0.0;
        for (k=kx+1; k<nz+1; k++) {
          rcol[k] = rcol2[k-1];
          rval[k] = rval2[k-1];
        }
        PetscCall(XMatChangeRow(A,*i,nz+1,rcol,rval));
      }
      *paij1 = &rval[k1];
      *paij2 = &rval[k2];
      iloc++;
      if (iloc>=A->Iend-A->Istart) iloc = -1;
    }
    iter->iloc = iloc;
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode SendrecvRow(XMat A,PetscInt nc1,const PetscInt vj1[],const PetscScalar va1[],PetscInt i2,PetscInt *nc2,PetscInt vj2[],PetscScalar va2[])
{
  PetscInt    rank,*ranges,N=A->N;
  PetscMPIInt naux;
  MPI_Status  st;

  PetscFunctionBeginUser;
  PetscCall(XMatGetOwnershipRanges(A,&ranges));
  PetscAssertAbort(i2>=0 && i2<A->M,A->comm,PETSC_ERR_ARG_OUTOFRANGE,"Incorrect index of row");
  /* Find owner of row i2 */
  rank=0;
  while (ranges[rank+1]<=i2) rank++;
  /* Send row i1, receive row i2 */
  PetscCallMPI(MPI_Sendrecv(vj1,nc1,MPIU_INT,rank,0,vj2,N,MPIU_INT,rank,0,A->comm,&st));
  PetscCallMPI(MPI_Sendrecv(va1,nc1,MPIU_SCALAR,rank,0,va2,N,MPIU_SCALAR,rank,0,A->comm,MPI_STATUS_IGNORE));
  PetscCallMPI(MPI_Get_count(&st,MPIU_INT,&naux));
  *nc2 = (PetscInt)naux;
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode PadRows(PetscInt nc1,const PetscInt vj1[],const PetscScalar va1[],PetscInt nc2,const PetscInt vj2[],const PetscScalar va2[],PetscInt *nc,PetscInt **vjj1,PetscScalar **vaa1,PetscInt **vjj2,PetscScalar **vaa2,PetscInt *iwork,PetscScalar *swork)
{
  PetscInt    k1,k2,k,n1=nc1+nc2,*vjjaux=iwork;
  PetscScalar *vaa1aux=swork,*vaa2aux=swork+n1;

  PetscFunctionBeginUser;
  *nc=0;
  for (k1=k2=k=0; k1<nc1 && k2<nc2; ) {
    if (vj1[k1]<vj2[k2]) {
      /* Take next element from first row */
      vaa1aux[k] = va1[k1];
      vaa2aux[k] = 0.0;
      vjjaux[k++] = vj1[k1++];
    } else if (vj1[k1]>vj2[k2]) {
      /* Take next element from second row */
      vaa1aux[k] = 0.0;
      vaa2aux[k] = va2[k2];
      vjjaux[k++] = vj2[k2++];
    } else {
      /* Take next element from both rows */
      vaa1aux[k] = va1[k1];
      vaa2aux[k] = va2[k2++];
      vjjaux[k++] = vj1[k1++];
    }
  }

  /* We reached the end of one of the rows. Continue with the other one */
  while (k1<nc1) {
      /* Take next element from first row */
      vaa1aux[k] = va1[k1];
      vaa2aux[k] = 0.0;
      vjjaux[k++] = vj1[k1++];
  }
  while (k2<nc2) {
      /* Take next element from second row */
      vaa1aux[k] = 0.0;
      vaa2aux[k] = va2[k2];
      vjjaux[k++] = vj2[k2++];
  }
  *nc=k;

  /* Copy rows (each row must be allocated separately)*/
  PetscCall(PetscMalloc2(k,vjj1,k,vaa1));
  PetscCall(PetscMalloc2(k,vjj2,k,vaa2));
  PetscCall(PetscArraycpy(*vjj1,vjjaux,k));
  PetscCall(PetscArraycpy(*vjj2,vjjaux,k));
  PetscCall(PetscArraycpy(*vaa1,vaa1aux,k));
  PetscCall(PetscArraycpy(*vaa2,vaa2aux,k));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static inline void MyAxpby(PetscInt n,PetscScalar a,PetscScalar x[],PetscScalar b,const PetscScalar y[])
{
  PetscInt i;

  for (i=0; i<n; i++) x[i] = a*x[i]+b*y[i];
}

/* Apply plane rotation on rows i1, i2 of A */
static PetscErrorCode RotateRows(XMat A,PetscInt i1,PetscInt i2,PetscReal c,PetscReal s,PetscInt *nzloc,PetscInt *iwork,PetscScalar *swork)
{
  PetscInt     M,N,Istart,Iend,nc,nc1,nc2,*vj1,*vj2,*vjj1,*vjj2,*iworkx=iwork;
  PetscBLASInt nc_,one=1;
  PetscScalar  *va1,*va2,*vaa1,*vaa2,*sworkx=swork;
  PetscBool    i1mine, i2mine;
#ifdef TIMING
  PetscReal    t,t0;
#endif

  PetscFunctionBeginUser;
  PetscCall(XMatGetOwnershipRange(A,&Istart,&Iend));
  i1mine = (Istart<=i1 && i1<Iend)? PETSC_TRUE: PETSC_FALSE;
  i2mine = (Istart<=i2 && i2<Iend)? PETSC_TRUE: PETSC_FALSE;

  if (i1mine || i2mine) {
    PetscCall(XMatGetSize(A,&M,&N));

    /* Get local row(s) (at least 1, possibly 2) */
    time_now(t0);
    if (i1mine) PetscCall(XMatGetRow(A,i1,&nc1,&vj1,&va1));
    if (i2mine) PetscCall(XMatGetRow(A,i2,&nc2,&vj2,&va2));
    time_diff(tt_getr,t0,t,t0);
    /* Get remote row (at most 1, possibly none) */
    if (!i2mine) {
      vj2 = iworkx;
      iworkx += N;
      va2 = sworkx;
      sworkx += N;
      PetscCall(SendrecvRow(A,nc1,vj1,va1,i2,&nc2,vj2,va2));
    } else if (!i1mine) {
      vj1 = iworkx;
      iworkx += N;
      va1 = sworkx;
      sworkx += N;
      PetscCall(SendrecvRow(A,nc2,vj2,va2,i1,&nc1,vj1,va1));
    }

    PetscCall(PadRows(nc1,vj1,va1,nc2,vj2,va2,&nc,&vjj1,&vaa1,&vjj2,&vaa2,iworkx,sworkx));
    if (i1mine) {
      *nzloc += nc-nc1;
      PetscCall(XMatRestoreRow(A,i1,&nc1,&vj1,&va1));
    }
    if (i2mine) {
      *nzloc += nc-nc2;
      PetscCall(XMatRestoreRow(A,i2,&nc2,&vj2,&va2));
    }

    /* Compute rotation and update matrix */
    if (nc) {
      PetscCall(PetscBLASIntCast(nc,&nc_));
      if (i1mine && i2mine) PetscCallBLAS("BLASrot",BLASMIXEDrot_(&nc_,vaa1,&one,vaa2,&one,&c,&s));
      else if (i1mine) {
        MyAxpby(nc,c,vaa1,s,vaa2);
        PetscCall(PetscFree2(vjj2,vaa2));
      } else {
        MyAxpby(nc,c,vaa2,-s,vaa1);
        PetscCall(PetscFree2(vjj1,vaa1));
      }
      time_now(t0);
      if (i1mine) PetscCall(XMatChangeRow(A,i1,nc,vjj1,vaa1));
      if (i2mine) PetscCall(XMatChangeRow(A,i2,nc,vjj2,vaa2));
      time_diff(tt_setvalr,t0,t,t0);
    }
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/* Apply plane rotation on columns j1, j2 of A */
static PetscErrorCode RotateCols(XMat A,PetscInt j1,PetscInt j2,PetscReal c,PetscReal s,PetscInt *nzloc)
{
  PetscInt    i;
  PetscScalar aux1,aux2,*paij1,*paij2;
  ColsNzIter  iter;

  PetscFunctionBeginUser;
  PetscCall(XMatCreateColsNzIter(A,j1,j2,&iter));
  while (PETSC_TRUE) {
    PetscCall(ColitNextRow(iter,&i,&paij1,&paij2));
    if (i<0) break;
    if (*paij1**paij2==0.0) (*nzloc)++;
    aux1 = *paij1*c+*paij2*s;
    aux2 = -*paij1*s+*paij2*c;
    *paij1 = aux1;
    *paij2 = aux2;
  }
  PetscCall(ColitDestroy(&iter));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/* Generate a pair of indices i1, i2, where 0 <= i1 < i2 < n */
static PetscErrorCode GetIndexPair(PetscRandom rand,PetscInt n,PetscInt *i1,PetscInt *i2)
{
  PetscInt  aux;
  PetscReal x;

  PetscFunctionBeginUser;
  PetscCall(PetscRandomGetValueReal(rand,&x));
  *i1 = (PetscInt)(x*n);
  PetscCall(PetscRandomGetValueReal(rand,&x));
  *i2 = (PetscInt)(x*(n-1));
  if (*i2>=*i1) (*i2)++;
  if (*i1>*i2) {
    aux = *i1;
    *i1 = *i2;
    *i2 = aux;
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

int main(int argc,char **argv)
{
  Mat         A,Omega;         /* operator matrix, signature matrix */
  XMat        XA;
  SVD         svd;             /* singular value problem solver context */
  Vec         u,v,vomega,*U,*V;
  MatType     Atype;
  PetscReal   tol,lev1=0.0,lev2=0.0,k=1e3,dens=0.1,log0,loginc,sq,x,angle,c,s;
  PetscScalar *swork;
  PetscInt    P=110,Q=80,N=100,i,j,Istart,Iend,nconv,nsv,i1,i2,nroth,nrotv,*iwork,nnzwanted,nz,nzloc;
  PetscBool   flg,flgp,flgq,flgn,terse,skiporth=PETSC_FALSE,progress=PETSC_FALSE;
  PetscRandom rand;
  MatInfo     Ainfo;
#ifdef TIMING
  PetscReal   t,t0;
#endif

  PetscFunctionBeginUser;
  PetscCall(SlepcInitialize(&argc,&argv,(char*)0,help));

  PetscCall(PetscOptionsGetInt(NULL,NULL,"-p",&P,&flgp));
  PetscCall(PetscOptionsGetInt(NULL,NULL,"-q",&Q,&flgq));
  PetscCall(PetscOptionsGetInt(NULL,NULL,"-n",&N,&flgn));
  PetscCall(PetscOptionsGetReal(NULL,NULL,"-k",&k,&flg));
  PetscCall(PetscOptionsGetReal(NULL,NULL,"-d",&dens,&flg));
  PetscCall(PetscOptionsGetBool(NULL,NULL,"-prog",&progress,&flg));
  if (!flgp && flgn) P=N;
  if (!flgq && flgn) Q=N;
  PetscCheck(P>=N,PETSC_COMM_WORLD,PETSC_ERR_USER_INPUT,"Parameter p must be >= n");
  PetscCheck(k>=1.0,PETSC_COMM_WORLD,PETSC_ERR_USER_INPUT,"Parameter k must be >= 1.0");
  PetscCheck(dens<=1.0,PETSC_COMM_WORLD,PETSC_ERR_USER_INPUT,"Parameter d must be <= 1.0");

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
        Build matrix that defines the hyperbolic singular value problem
     - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  PetscCall(PetscPrintf(PETSC_COMM_WORLD,"\nHyperbolic singular value problem.\n\n"));
  PetscCall(PetscPrintf(PETSC_COMM_WORLD,"\n Matrix dimensions: (%" PetscInt_FMT "+%" PetscInt_FMT ")x%" PetscInt_FMT,P,Q,N));
  PetscCall(PetscPrintf(PETSC_COMM_WORLD,", using signature Omega=blkdiag(I_%" PetscInt_FMT ",-I_%" PetscInt_FMT ")\n\n",P,Q));

  PetscCall(MatCreate(PETSC_COMM_WORLD,&A));
  PetscCall(MatSetSizes(A,PETSC_DECIDE,PETSC_DECIDE,P+Q,N));
  PetscCall(MatSetFromOptions(A));
  PetscCall(MatSetOption(A,MAT_IGNORE_OFF_PROC_ENTRIES,PETSC_TRUE));

  /* Set diagonals */
  PetscCall(MatGetOwnershipRange(A,&Istart,&Iend));

  PetscCall(XMatCreate(MPI_COMM_WORLD,&XA,Istart,Iend,P+Q,N));
  log0 = PetscLogReal(1/k);
  loginc = -log0/(N-1);
  sq = PetscSqrtReal(5.0/4.0);
  for (i=Istart;i<Iend;i++) {
    if (i<P && i<N) {
      if (i<Q) x = sq*PetscExpReal(log0+i*loginc);
      else     x = PetscExpReal(log0+i*loginc);
      PetscCall(XMatSetValue(XA,i,i,x,INSERT_VALUES));
    } else if (i>=P && i-P<N) {
      j = i-P;
      PetscCall(XMatSetValue(XA,i,j,PetscExpReal(log0+j*loginc),INSERT_VALUES));
    }
  }

  /* Apply plane rotations */
  nnzwanted = dens*(P+Q)*N;
  PetscCall(XMatGetNumberNonzeros(XA,&nzloc));
  PetscCallMPI(MPI_Allreduce(&nzloc,&nz,1,MPIU_INT,MPI_SUM,MPI_COMM_WORLD));
  PetscCall(PetscRandomCreate(PETSC_COMM_SELF,&rand));
  PetscCall(PetscRandomSetFromOptions(rand));
  nroth = nrotv = 0;
  PetscCall(PetscMalloc2(2*N,&iwork,3*N,&swork));
  time_now(t0);
  while (nz<0.95*nnzwanted) {
    /* Plane rotation on 2 of the top P rows*/
    PetscCall(PetscRandomGetValueReal(rand,&angle));
    c=PetscCosReal(angle);
    s=PetscSinReal(angle);
    PetscCall(GetIndexPair(rand,P,&i1,&i2));
    PetscCall(RotateRows(XA,i1,i2,c,s,&nzloc,iwork,swork));
    time_diff(tt_rotr,t0,t,t0);
    nroth++;
    /* Plane rotation on 2 of the bottom Q rows*/
    if (Q>1) {
      PetscCall(PetscRandomGetValueReal(rand,&angle));
      c=PetscCosReal(angle);
      s=PetscSinReal(angle);
      PetscCall(GetIndexPair(rand,Q,&i1,&i2));
      PetscCall(RotateRows(XA,P+i1,P+i2,c,s,&nzloc,iwork,swork));
      time_diff(tt_rotr,t0,t,t0);
      nroth++;
    }
    /* Plane rotation on 2 columns */
    PetscCall(PetscRandomGetValueReal(rand,&angle));
    c=PetscCosReal(angle);
    s=PetscSinReal(angle);
    PetscCall(GetIndexPair(rand,N,&i1,&i2));
    PetscCall(RotateCols(XA,i1,i2,c,s,&nzloc));
    time_diff(tt_rotc,t0,t,t0);
    nrotv++;
    /* Update total number of non-zeros */
    PetscCallMPI(MPI_Allreduce(&nzloc,&nz,1,MPIU_INT,MPI_SUM,MPI_COMM_WORLD));
    if (progress) PetscCall(PetscFPrintf(MPI_COMM_WORLD,stderr,"\rProgress: %.2f%%",(double)nz/nnzwanted*100));
  }
  if (progress) PetscCall(PetscFPrintf(MPI_COMM_WORLD,stderr,"\r"));

  PetscCall(PetscFree2(iwork,swork));
  PetscCall(PetscRandomDestroy(&rand));

  PetscCall(XMatConvert(XA,A));
  PetscCall(XMatDestroy(&XA));
  PetscCall(MatGetInfo(A,MAT_GLOBAL_SUM,&Ainfo));
  PetscCall(PetscPrintf(MPI_COMM_WORLD," Matrix created. %" PetscInt_FMT " rotations applied. nnz=%.0f. Density: %.4g\n\n",nroth+nrotv,Ainfo.nz_used,(double)Ainfo.nz_used/(1.0*(P+Q)*N)));

#ifdef TIMING
  PetscCall(PetscPrintf(MPI_COMM_WORLD,"#T rot-rows: %6.3f  get-rows: %6.3f  setval-rows: %6.3f  assm-rows: %.3f\n",tt_rotr,tt_getr,tt_setvalr,tt_assmr));
  PetscCall(PetscPrintf(MPI_COMM_WORLD,"#T rot cols: %6.3f  get-cols: %6.3f  setval-cols: %6.3f  assm-cols: %.3f\n",tt_rotc,tt_getc,tt_setvalc,tt_assmc));
#endif

  /* scale by 0.5 the lower Q rows of A */
  PetscCall(MatCreateVecs(A,NULL,&vomega));
  PetscCall(VecSet(vomega,1.0));
  for (i=PetscMax(P,Istart);i<Iend;i++) {
    PetscCall(VecSetValue(vomega,i,0.5,INSERT_VALUES));
  }
  PetscCall(VecAssemblyBegin(vomega));
  PetscCall(VecAssemblyEnd(vomega));
  PetscCall(MatDiagonalScale(A,vomega,NULL));

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
                          Create the signature
     - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  PetscCall(VecSet(vomega,1.0));
  PetscCall(VecGetOwnershipRange(vomega,&Istart,&Iend));
  for (i=Istart;i<Iend;i++) {
    if (i>=P) PetscCall(VecSetValue(vomega,i,-1.0,INSERT_VALUES));
  }
  PetscCall(VecAssemblyBegin(vomega));
  PetscCall(VecAssemblyEnd(vomega));

  PetscCall(MatGetType(A,&Atype));
  PetscCall(MatCreate(PETSC_COMM_WORLD,&Omega));
  PetscCall(MatSetSizes(Omega,PETSC_DECIDE,PETSC_DECIDE,P+Q,P+Q));
  PetscCall(MatSetType(Omega,Atype));
  PetscCall(MatDiagonalSet(Omega,vomega,INSERT_VALUES));

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
          Create the singular value solver and set various options
     - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  PetscCall(SVDCreate(PETSC_COMM_WORLD,&svd));

  PetscCall(SVDSetOperators(svd,A,NULL));
  PetscCall(SVDSetSignature(svd,vomega));
  PetscCall(SVDSetProblemType(svd,SVD_HYPERBOLIC));

  PetscCall(SVDSetFromOptions(svd));

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
                 Solve the problem, display solution
     - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  PetscCall(MatCreateVecs(A,&v,&u));
  PetscCall(SVDSolve(svd));

  /* show detailed info unless -terse option is given by user */
  PetscCall(PetscOptionsHasName(NULL,NULL,"-terse",&terse));
  if (terse) PetscCall(SVDErrorView(svd,SVD_ERROR_NORM,NULL));
  else {
    PetscCall(PetscViewerPushFormat(PETSC_VIEWER_STDOUT_WORLD,PETSC_VIEWER_ASCII_INFO_DETAIL));
    PetscCall(SVDConvergedReasonView(svd,PETSC_VIEWER_STDOUT_WORLD));
    PetscCall(SVDErrorView(svd,SVD_ERROR_NORM,PETSC_VIEWER_STDOUT_WORLD));
    PetscCall(PetscViewerPopFormat(PETSC_VIEWER_STDOUT_WORLD));
  }

  /* check orthogonality */
  PetscCall(PetscOptionsGetBool(NULL,NULL,"-skiporth",&skiporth,NULL));
  PetscCall(SVDGetConverged(svd,&nconv));
  PetscCall(SVDGetDimensions(svd,&nsv,NULL,NULL));
  nconv = PetscMin(nconv,nsv);
  if (nconv>0 && !skiporth) {
    PetscCall(SVDGetTolerances(svd,&tol,NULL));
    PetscCall(VecDuplicateVecs(u,nconv,&U));
    PetscCall(VecDuplicateVecs(v,nconv,&V));
    for (i=0;i<nconv;i++) PetscCall(SVDGetSingularTriplet(svd,i,NULL,U[i],V[i]));
    PetscCall(VecCheckOrthonormality(U,nconv,NULL,nconv,Omega,NULL,&lev1));
    PetscCall(VecCheckOrthonormality(V,nconv,NULL,nconv,NULL,NULL,&lev2));
    if (lev1+lev2<20*tol) PetscCall(PetscPrintf(PETSC_COMM_WORLD,"Level of orthogonality below the tolerance\n"));
    else PetscCall(PetscPrintf(PETSC_COMM_WORLD,"Level of orthogonality: %g (U) %g (V)\n",(double)lev1,(double)lev2));
    PetscCall(VecDestroyVecs(nconv,&U));
    PetscCall(VecDestroyVecs(nconv,&V));
  }
  PetscCall(VecDestroy(&u));
  PetscCall(VecDestroy(&v));

  /* free work space */
  PetscCall(SVDDestroy(&svd));
  PetscCall(MatDestroy(&Omega));
  PetscCall(MatDestroy(&A));
  PetscCall(VecDestroy(&vomega));
  PetscCall(SlepcFinalize());
  return 0;
}

/*TEST

   testset:
      args: -svd_nsv 5 -d .15 -terse
      output_file: output/ex53_1.out
      nsize: {{1 2}}
      test:
         args: -svd_type trlanczos -ds_parallel {{redundant synchronized}}
         suffix: 1_trlanczos
      test:
         args: -svd_type cross
         suffix: 1_cross
      test:
         args: -svd_type cyclic
         requires: !single
         suffix: 1_cyclic

TEST*/