#include <petsc/private/viewerimpl.h>
#include <mat.h>

typedef struct {
  MATFile      *ep;
  PetscMPIInt   rank;
  PetscFileMode btype;
} PetscViewer_Matlab;

/*@C
  PetscViewerMatlabPutArray - Puts an array into the `PETSCVIEWERMATLAB` viewer.

  Not Collective, only processor zero saves `array`

  Input Parameters:
+ mfile - the viewer
. m     - the first dimensions of `array`
. n     - the second dimensions of `array`
. array - the array (represented in one dimension)
- name  - the MATLAB name of `array`

  Level: advanced

  Note:
  Only writes `array` values on processor 0.

.seealso: `PETSCVIEWERMATLAB`, `PetscViewerMatlabGetArray()`
@*/
PetscErrorCode PetscViewerMatlabPutArray(PetscViewer mfile, int m, int n, const PetscScalar *array, const char *name)
{
  PetscViewer_Matlab *ml;
  mxArray            *mat;

  PetscFunctionBegin;
  PetscCheck(mfile, PETSC_COMM_SELF, PETSC_ERR_ARG_NULL, "Null argument: probably PETSC_VIEWER_MATLAB_() failed");
  ml = (PetscViewer_Matlab *)mfile->data;
  if (!ml->rank) {
    PetscCall(PetscInfo(mfile, "Putting MATLAB array %s\n", name));
#if !defined(PETSC_USE_COMPLEX)
    mat = mxCreateDoubleMatrix(m, n, mxREAL);
#else
    mat = mxCreateDoubleMatrix(m, n, mxCOMPLEX);
#endif
    PetscCall(PetscArraycpy(mxGetPr(mat), array, m * n));
    matPutVariable(ml->ep, name, mat);

    PetscCall(PetscInfo(mfile, "Put MATLAB array %s\n", name));
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode PetscViewerMatlabPutVariable(PetscViewer viewer, const char *name, void *mat)
{
  PetscViewer_Matlab *ml = (PetscViewer_Matlab *)viewer->data;

  PetscFunctionBegin;
  matPutVariable(ml->ep, name, (mxArray *)mat);
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
  PetscViewerMatlabGetArray - Gets a variable from a `PETSCVIEWERMATLAB` viewer into an array

  Not Collective; only processor zero reads in the array

  Input Parameters:
+ mfile - the MATLAB file viewer
. m     - the first dimensions of `array`
. n     - the second dimensions of `array`
. array - the array (represented in one dimension), must of be length `m` * `n`
- name  - the MATLAB name of `array`

  Level: advanced

  Note:
  Only reads in `array` values on processor 0.

.seealso: `PETSCVIEWERMATLAB`, `PetscViewerMatlabPutArray()`
@*/
PetscErrorCode PetscViewerMatlabGetArray(PetscViewer mfile, int m, int n, PetscScalar *array, const char *name)
{
  PetscViewer_Matlab *ml;
  mxArray            *mat;

  PetscFunctionBegin;
  PetscCheck(mfile, PETSC_COMM_SELF, PETSC_ERR_ARG_NULL, "Null argument: probably PETSC_VIEWER_MATLAB_() failed");
  ml = (PetscViewer_Matlab *)mfile->data;
  if (!ml->rank) {
    PetscCall(PetscInfo(mfile, "Getting MATLAB array %s\n", name));
    mat = matGetVariable(ml->ep, name);
    PetscCheck(mat, PETSC_COMM_SELF, PETSC_ERR_LIB, "Unable to get array %s from matlab", name);
    PetscCall(PetscArraycpy(array, mxGetPr(mat), m * n));
    PetscCall(PetscInfo(mfile, "Got MATLAB array %s\n", name));
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode PetscViewerFileSetMode_Matlab(PetscViewer viewer, PetscFileMode type)
{
  PetscViewer_Matlab *vmatlab = (PetscViewer_Matlab *)viewer->data;

  PetscFunctionBegin;
  vmatlab->btype = type;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*
        Actually opens the file
*/
static PetscErrorCode PetscViewerFileSetName_Matlab(PetscViewer viewer, const char name[])
{
  PetscViewer_Matlab *vmatlab = (PetscViewer_Matlab *)viewer->data;
  PetscFileMode       type    = vmatlab->btype;

  PetscFunctionBegin;
  PetscCheck(type != (PetscFileMode)-1, PETSC_COMM_SELF, PETSC_ERR_ORDER, "Must call PetscViewerFileSetMode() before PetscViewerFileSetName()");
  if (vmatlab->ep) matClose(vmatlab->ep);

  /* only first processor opens file */
  if (!vmatlab->rank) {
    if (type == FILE_MODE_READ) vmatlab->ep = matOpen(name, "r");
    else if (type == FILE_MODE_WRITE) vmatlab->ep = matOpen(name, "w");
    else {
      PetscCheck(type != FILE_MODE_UNDEFINED, PetscObjectComm((PetscObject)viewer), PETSC_ERR_ORDER, "Must call PetscViewerFileSetMode() before PetscViewerFileSetName()");
      SETERRQ(PetscObjectComm((PetscObject)viewer), PETSC_ERR_SUP, "Unsupported file mode %s", PetscFileModes[type]);
    }
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode PetscViewerDestroy_Matlab(PetscViewer v)
{
  PetscViewer_Matlab *vf = (PetscViewer_Matlab *)v->data;

  PetscFunctionBegin;
  if (vf->ep) matClose(vf->ep);
  PetscCall(PetscFree(vf));
  PetscCall(PetscObjectComposeFunction((PetscObject)v, "PetscViewerFileSetName_C", NULL));
  PetscCall(PetscObjectComposeFunction((PetscObject)v, "PetscViewerFileSetMode_C", NULL));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*MC
   PETSCVIEWERMATLAB - A viewer that saves the variables into a MATLAB .mat file that may be read into MATLAB
       with load('filename').

   Level: intermediate

       Notes:
             Currently can only save PETSc vectors to .mat files, not matrices (use the `PETSCVIEWERBINARY` and
             ${PETSC_DIR}/share/petsc/matlab/PetscBinaryRead.m to read matrices into MATLAB).

             For parallel vectors obtained with `DMCreateGlobalVector()` or `DMGetGlobalVector()` the vectors are saved to
             the .mat file in natural ordering. You can use DMView() to save the `DMDA` information to the .mat file
             the fields in the MATLAB loaded da variable give the array dimensions so you can reshape the MATLAB
             vector to the same multidimensional shape as it had in PETSc for plotting etc. For example,

             In your PETSc C/C++ code (assuming a two dimensional `DMDA` with one degree of freedom per node)
.vb
                PetscObjectSetName((PetscObject)x,"x");
                VecView(x,PETSC_VIEWER_MATLAB_WORLD);
                PetscObjectSetName((PetscObject)da,"da");
                DMView(x,PETSC_VIEWER_MATLAB_WORLD);
.ve
             Then from MATLAB
.vb
                load('matlaboutput.mat')   % matlaboutput.mat is the default filename
                xnew = zeros(da.n,da.m);
                xnew(:) = x;    % reshape one dimensional vector back to two dimensions
.ve

              If you wish to put the same variable into the .mat file several times you need to give it a new
              name before each call to view.

              Use `PetscViewerMatlabPutArray()` to just put an array of doubles into the .mat file

.seealso: `PETSC_VIEWER_MATLAB_()`, `PETSC_VIEWER_MATLAB_SELF`, `PETSC_VIEWER_MATLAB_WORLD`, `PetscViewerCreate()`,
          `PetscViewerMatlabOpen()`, `VecView()`, `DMView()`, `PetscViewerMatlabPutArray()`, `PETSCVIEWERBINARY`, `PETSCVIEWERASCII`, `PETSCVIEWERDRAW`,
          `PETSC_VIEWER_STDOUT_()`, `PetscViewerFileSetName()`, `PetscViewerFileSetMode()`, `PetscViewerFormat`, `PetscMatlabEngine`
M*/
PETSC_EXTERN PetscErrorCode PetscViewerCreate_Matlab(PetscViewer viewer)
{
  PetscViewer_Matlab *e;

  PetscFunctionBegin;
  PetscCall(PetscNew(&e));
  PetscCallMPI(MPI_Comm_rank(PetscObjectComm((PetscObject)viewer), &e->rank));
  e->btype     = FILE_MODE_UNDEFINED;
  viewer->data = (void *)e;

  PetscCall(PetscObjectComposeFunction((PetscObject)viewer, "PetscViewerFileSetName_C", PetscViewerFileSetName_Matlab));
  PetscCall(PetscObjectComposeFunction((PetscObject)viewer, "PetscViewerFileSetMode_C", PetscViewerFileSetMode_Matlab));

  viewer->ops->destroy = PetscViewerDestroy_Matlab;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
  PetscViewerMatlabOpen - Opens a MATLAB .mat file for output

  Collective

  Input Parameters:
+ comm - MPI communicator
. name - name of file
- type - type of file
.vb
    FILE_MODE_WRITE - create new file for MATLAB output
    FILE_MODE_READ - open existing file for MATLAB input
    FILE_MODE_WRITE - open existing file for MATLAB output
.ve

  Output Parameter:
. binv - PetscViewer for MATLAB output to use with the specified file

  Level: beginner

  Notes:
  This `PetscViewer` should be destroyed with `PetscViewerDestroy()`.

  For writing files it only opens the file on processor 0 in the communicator.

  This only saves `Vec`s it cannot be used to save `Mat`s. We recommend using the `PETSCVIEWERBINARY` to save objects to be loaded into MATLAB
  instead of this routine.

  PETSc must be configured with the option `--with-matlab` for this functionality

.seealso: `PETSCVIEWERMATLAB`, `PetscViewerASCIIOpen()`, `PetscViewerPushFormat()`, `PetscViewerDestroy()`, `PETSCVIEWERBINARY`, `PetscViewerBinaryOpen()`
          `VecView()`, `MatView()`, `VecLoad()`, `MatLoad()`
@*/
PetscErrorCode PetscViewerMatlabOpen(MPI_Comm comm, const char name[], PetscFileMode type, PetscViewer *binv)
{
  PetscFunctionBegin;
  PetscCall(PetscViewerCreate(comm, binv));
  PetscCall(PetscViewerSetType(*binv, PETSCVIEWERMATLAB));
  PetscCall(PetscViewerFileSetMode(*binv, type));
  PetscCall(PetscViewerFileSetName(*binv, name));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscMPIInt Petsc_Viewer_Matlab_keyval = MPI_KEYVAL_INVALID;

/*@C
   PETSC_VIEWER_MATLAB_ - Creates a `PETSCVIEWERMATLAB` `PetscViewer` shared by all processors
   in a communicator.

   Collective

   Input Parameter:
.  comm - the MPI communicator to share the MATLAB `PetscViewer`

   Options Database Key:
.    -viewer_matlab_filename <name> - name of the MATLAB file

   Environmental variable:
.  `PETSC_VIEWER_MATLAB_FILENAME` - name of the MATLAB file

   Level: intermediate

   Notes:
   This object is destroyed in `PetscFinalize()`, `PetscViewerDestroy()` should never be called on it

   Unlike almost all other PETSc routines, `PETSC_VIEWER_MATLAB_()` does not return
   an error code.  The MATLAB `PetscViewer` is usually used in the form `XXXView(XXX object, PETSC_VIEWER_MATLAB_(comm))`

   Use `PETSC_VIEWER_SOCKET_()` or `PetscViewerSocketOpen()` to communicator with an interactive MATLAB session.

.seealso: `PETSC_VIEWER_MATLAB_WORLD`, `PETSC_VIEWER_MATLAB_SELF`, `PetscViewerMatlabOpen()`, `PetscViewerCreate()`,
          `PetscViewerDestroy()`
@*/
PetscViewer PETSC_VIEWER_MATLAB_(MPI_Comm comm)
{
  PetscErrorCode ierr;
  PetscBool      flg;
  PetscViewer    viewer;
  char           fname[PETSC_MAX_PATH_LEN];
  MPI_Comm       ncomm;

  PetscFunctionBegin;
  ierr = PetscCommDuplicate(comm, &ncomm, NULL);
  if (ierr) {
    PetscError(PETSC_COMM_SELF, __LINE__, "PETSC_VIEWER_MATLAB_", __FILE__, PETSC_ERR_PLIB, PETSC_ERROR_INITIAL, " ");
    PetscFunctionReturn(NULL);
  }
  if (Petsc_Viewer_Matlab_keyval == MPI_KEYVAL_INVALID) {
    ierr = MPI_Comm_create_keyval(MPI_COMM_NULL_COPY_FN, MPI_COMM_NULL_DELETE_FN, &Petsc_Viewer_Matlab_keyval, 0);
    if (ierr) {
      PetscError(PETSC_COMM_SELF, __LINE__, "PETSC_VIEWER_MATLAB_", __FILE__, PETSC_ERR_PLIB, PETSC_ERROR_INITIAL, " ");
      PetscFunctionReturn(NULL);
    }
  }
  ierr = MPI_Comm_get_attr(ncomm, Petsc_Viewer_Matlab_keyval, (void **)&viewer, (int *)&flg);
  if (ierr) {
    PetscError(PETSC_COMM_SELF, __LINE__, "PETSC_VIEWER_MATLAB_", __FILE__, PETSC_ERR_PLIB, PETSC_ERROR_INITIAL, " ");
    PetscFunctionReturn(NULL);
  }
  if (!flg) { /* PetscViewer not yet created */
    ierr = PetscOptionsGetenv(ncomm, "PETSC_VIEWER_MATLAB_FILENAME", fname, PETSC_MAX_PATH_LEN, &flg);
    if (ierr) {
      PetscError(PETSC_COMM_SELF, __LINE__, "PETSC_VIEWER_MATLAB_", __FILE__, PETSC_ERR_PLIB, PETSC_ERROR_REPEAT, " ");
      PetscFunctionReturn(NULL);
    }
    if (!flg) {
      ierr = PetscStrncpy(fname, "matlaboutput.mat", sizeof(fname));
      if (ierr) {
        PetscError(PETSC_COMM_SELF, __LINE__, "PETSC_VIEWER_MATLAB_", __FILE__, PETSC_ERR_PLIB, PETSC_ERROR_REPEAT, " ");
        PetscFunctionReturn(NULL);
      }
    }
    ierr                              = PetscViewerMatlabOpen(ncomm, fname, FILE_MODE_WRITE, &viewer);
    ((PetscObject)viewer)->persistent = PETSC_TRUE;
    if (ierr) {
      PetscError(PETSC_COMM_SELF, __LINE__, "PETSC_VIEWER_MATLAB_", __FILE__, PETSC_ERR_PLIB, PETSC_ERROR_REPEAT, " ");
      PetscFunctionReturn(NULL);
    }
    ierr = PetscObjectRegisterDestroy((PetscObject)viewer);
    if (ierr) {
      PetscError(PETSC_COMM_SELF, __LINE__, "PETSC_VIEWER_MATLAB_", __FILE__, PETSC_ERR_PLIB, PETSC_ERROR_REPEAT, " ");
      PetscFunctionReturn(NULL);
    }
    ierr = MPI_Comm_set_attr(ncomm, Petsc_Viewer_Matlab_keyval, (void *)viewer);
    if (ierr) {
      PetscError(PETSC_COMM_SELF, __LINE__, "PETSC_VIEWER_MATLAB_", __FILE__, PETSC_ERR_PLIB, PETSC_ERROR_INITIAL, " ");
      PetscFunctionReturn(NULL);
    }
  }
  ierr = PetscCommDestroy(&ncomm);
  if (ierr) {
    PetscError(PETSC_COMM_SELF, __LINE__, "PETSC_VIEWER_MATLAB_", __FILE__, PETSC_ERR_PLIB, PETSC_ERROR_REPEAT, " ");
    PetscFunctionReturn(NULL);
  }
  PetscFunctionReturn(viewer);
}
