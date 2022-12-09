/* ----------------------------------------------------------------------
   LAMMPS - Large-scale Atomic/Molecular Massively Parallel Simulator
   https://www.lammps.org/, Sandia National Laboratories
   LAMMPS development team: developers@lammps.org

   Copyright (2003) Sandia Corporation.  Under the terms of Contract
   DE-AC04-94AL85000 with Sandia Corporation, the U.S. Government retains
   certain rights in this software.  This software is distributed under
   the GNU General Public License.

   See the README file in the top-level LAMMPS directory.
------------------------------------------------------------------------- */

/* ----------------------------------------------------------------------
    Adapted from pair style table written by: Paul Crozier (SNL)
    This differs by using linear instead of r-squared tabulation internally
    Also, support for bitmap style lookup has been removed.
------------------------------------------------------------------------- */

#include "pair_table_mod.h"

#include "atom.h"
#include "comm.h"
#include "error.h"
#include "force.h"
#include "memory.h"
#include "neigh_list.h"
#include "table_file_reader.h"
#include "tokenizer.h"

#include <cmath>
#include <cstring>

using namespace LAMMPS_NS;

enum { NONE, RLINEAR, RSQ };

#define EPSILONR 1.0e-6

/* ----------------------------------------------------------------------
   spline and splint routines modified from Numerical Recipes
------------------------------------------------------------------------- */

static void spline(double *x, double *y, int n, double yp1, double ypn, double *y2)
{
  int i, k;
  double p, qn, sig, un;
  auto u = new double[n];

  if (yp1 > 0.99e30)
    y2[0] = u[0] = 0.0;
  else {
    y2[0] = -0.5;
    u[0] = (3.0 / (x[1] - x[0])) * ((y[1] - y[0]) / (x[1] - x[0]) - yp1);
  }
  for (i = 1; i < n - 1; i++) {
    sig = (x[i] - x[i - 1]) / (x[i + 1] - x[i - 1]);
    p = sig * y2[i - 1] + 2.0;
    y2[i] = (sig - 1.0) / p;
    u[i] = (y[i + 1] - y[i]) / (x[i + 1] - x[i]) - (y[i] - y[i - 1]) / (x[i] - x[i - 1]);
    u[i] = (6.0 * u[i] / (x[i + 1] - x[i - 1]) - sig * u[i - 1]) / p;
  }
  if (ypn > 0.99e30)
    qn = un = 0.0;
  else {
    qn = 0.5;
    un = (3.0 / (x[n - 1] - x[n - 2])) * (ypn - (y[n - 1] - y[n - 2]) / (x[n - 1] - x[n - 2]));
  }
  y2[n - 1] = (un - qn * u[n - 2]) / (qn * y2[n - 2] + 1.0);
  for (k = n - 2; k >= 0; k--) y2[k] = y2[k] * y2[k + 1] + u[k];

  delete[] u;
}

/* ---------------------------------------------------------------------- */

static double splint(const double *xa, const double *ya, const double *y2a, int n, double x)
{
  int klo = 0;
  int khi = n - 1;
  while (khi - klo > 1) {
    const int k = (khi + klo) >> 1;
    if (xa[k] > x)
      khi = k;
    else
      klo = k;
  }
  const double h = xa[khi] - xa[klo];
  const double a = (xa[khi] - x) / h;
  const double b = (x - xa[klo]) / h;
  const double y = a * ya[klo] + b * ya[khi] +
    ((a * a * a - a) * y2a[klo] + (b * b * b - b) * y2a[khi]) * (h * h) / 6.0;
  return y;
}

/* ---------------------------------------------------------------------- */

PairTableMod::PairTableMod(LAMMPS *lmp) : Pair(lmp)
{
  ntables = 0;
  tables = nullptr;
  unit_convert_flag = utils::get_supported_conversions(utils::ENERGY);
}

/* ---------------------------------------------------------------------- */

PairTableMod::~PairTableMod()
{
  if (copymode) return;

  for (int m = 0; m < ntables; m++) free_table(&tables[m]);
  memory->sfree(tables);

  if (allocated) {
    memory->destroy(setflag);
    memory->destroy(cutsq);
    memory->destroy(tabindex);
  }
}

/* ---------------------------------------------------------------------- */

void PairTableMod::compute(int eflag, int vflag)
{
  ev_init(eflag, vflag);
  if (tabstyle == LOOKUP) {
    if (evflag) {
      if (eflag) {
        if (force->newton_pair) eval<LOOKUP,1,1,1>();
        else eval<LOOKUP,1,1,0>();
      } else {
        if (force->newton_pair) eval<LOOKUP,1,0,1>();
        else eval<LOOKUP,1,0,0>();
      }
    } else {
      if (force->newton_pair) eval<LOOKUP,0,0,1>();
      else eval<LOOKUP,0,0,0>();
    }
  } else if (tabstyle == LINEAR) {
    if (evflag) {
      if (eflag) {
        if (force->newton_pair) eval<LINEAR,1,1,1>();
        else eval<LINEAR,1,1,0>();
      } else {
        if (force->newton_pair) eval<LINEAR,1,0,1>();
        else eval<LINEAR,1,0,0>();
      }
    } else {
      if (force->newton_pair) eval<LINEAR,0,0,1>();
      else eval<LINEAR,0,0,0>();
    }
  } else if (tabstyle == SPLINE) {
    if (evflag) {
      if (eflag) {
        if (force->newton_pair) eval<SPLINE,1,1,1>();
        else eval<SPLINE,1,1,0>();
      } else {
        if (force->newton_pair) eval<SPLINE,1,0,1>();
        else eval<SPLINE,1,0,0>();
      }
    } else {
      if (force->newton_pair) eval<SPLINE,0,0,1>();
      else eval<SPLINE,0,0,0>();
    }
  }
}

/* ---------------------------------------------------------------------- */

template <int TABSTYLE, int EVFLAG, int EFLAG, int NEWTON_PAIR>
void PairTableMod::eval()
{
  int i, j, ii, jj, inum, jnum, itype, jtype, itable;
  double xtmp, ytmp, ztmp, delx, dely, delz, evdwl, fpair;
  double fxtmp, fytmp, fztmp;
  double rsq, factor_lj, fraction, value, a, b;
  int *ilist, *jlist, *numneigh, **firstneigh;
  Table *tb;

  int tlm1 = tablength - 1;

  evdwl = 0.0;

  double **x = atom->x;
  double **f = atom->f;
  int *type = atom->type;
  int nlocal = atom->nlocal;
  double *special_lj = force->special_lj;

  inum = list->inum;
  ilist = list->ilist;
  numneigh = list->numneigh;
  firstneigh = list->firstneigh;

  // loop over neighbors of my atoms

  for (ii = 0; ii < inum; ii++) {
    i = ilist[ii];
    xtmp = x[i][0];
    ytmp = x[i][1];
    ztmp = x[i][2];
    itype = type[i];
    jlist = firstneigh[i];
    jnum = numneigh[i];

    fxtmp = fytmp = fztmp = 0.0;

    for (jj = 0; jj < jnum; jj++) {
      j = jlist[jj];
      factor_lj = special_lj[sbmask(j)];
      j &= NEIGHMASK;

      delx = xtmp - x[j][0];
      dely = ytmp - x[j][1];
      delz = ztmp - x[j][2];
      rsq = delx * delx + dely * dely + delz * delz;
      jtype = type[j];

      if (rsq < cutsq[itype][jtype]) {
        tb = &tables[tabindex[itype][jtype]];
        if (rsq < tb->innersq)
          error->one(FLERR, "Pair distance < table inner cutoff: ijtype {} {} dist {}", itype,
                     jtype, sqrt(rsq));
        double r = sqrt(rsq);
        if (TABSTYLE == LOOKUP) {
          itable = static_cast<int>((r - tb->inner) * tb->invdelta);
          if (itable >= tlm1)
            error->one(FLERR, "Pair distance > table outer cutoff: ijtype {} {} dist {}", itype,
                       jtype, r);
          fpair = factor_lj * tb->f[itable];
        } else if (TABSTYLE == LINEAR) {
          itable = static_cast<int>((r - tb->inner) * tb->invdelta);
          if (itable >= tlm1)
            error->one(FLERR, "Pair distance > table outer cutoff: ijtype {} {} dist {}", itype,
                       jtype, r);
          fraction = (r - tb->r[itable]) * tb->invdelta;
          value = tb->f[itable] + fraction * tb->df[itable];
          fpair = factor_lj * value;
        } else if (TABSTYLE == SPLINE) {
          itable = static_cast<int>((r - tb->inner) * tb->invdelta);
          if (itable >= tlm1)
            error->one(FLERR, "Pair distance > table outer cutoff: ijtype {} {} dist {}", itype,
                       jtype, r);
          b = (r - tb->r[itable]) * tb->invdelta;
          a = 1.0 - b;
          value = a * tb->f[itable] + b * tb->f[itable + 1] +
              ((a * a * a - a) * tb->f2[itable] + (b * b * b - b) * tb->f2[itable + 1]) *
                  tb->deltasq6;
          fpair = factor_lj * value;
        }

        fxtmp += delx * fpair;
        fytmp += dely * fpair;
        fztmp += delz * fpair;
        if (NEWTON_PAIR || j < nlocal) {
          f[j][0] -= delx * fpair;
          f[j][1] -= dely * fpair;
          f[j][2] -= delz * fpair;
        }

        if (EFLAG) {
          if (TABSTYLE == LOOKUP)
            evdwl = tb->e[itable];
          else if (TABSTYLE == LINEAR)
            evdwl = tb->e[itable] + fraction * tb->de[itable];
          else
            evdwl = a * tb->e[itable] + b * tb->e[itable + 1] +
                ((a * a * a - a) * tb->e2[itable] + (b * b * b - b) * tb->e2[itable + 1]) *
                    tb->deltasq6;
          evdwl *= factor_lj;
        }

        if (EVFLAG) ev_tally(i, j, nlocal, NEWTON_PAIR, evdwl, 0.0, fpair, delx, dely, delz);
      }
    }
    
    f[i][0] += fxtmp;
    f[i][1] += fytmp;
    f[i][2] += fztmp;
  }

  if (vflag_fdotr) virial_fdotr_compute();
}

/* ----------------------------------------------------------------------
   allocate all arrays
------------------------------------------------------------------------- */

void PairTableMod::allocate()
{
  allocated = 1;
  const int nt = atom->ntypes + 1;

  memory->create(setflag, nt, nt, "pair:setflag");
  memory->create(cutsq, nt, nt, "pair:cutsq");
  memory->create(tabindex, nt, nt, "pair:tabindex");

  memset(&setflag[0][0], 0, sizeof(int) * nt * nt);
  memset(&cutsq[0][0], 0, sizeof(double) * nt * nt);
  memset(&tabindex[0][0], 0, sizeof(int) * nt * nt);
}

/* ----------------------------------------------------------------------
   global settings
------------------------------------------------------------------------- */

void PairTableMod::settings(int narg, char **arg)
{
  if (narg < 2) error->all(FLERR, "Illegal pair_style command");

  // new settings

  if (strcmp(arg[0], "lookup") == 0)
    tabstyle = LOOKUP;
  else if (strcmp(arg[0], "linear") == 0)
    tabstyle = LINEAR;
  else if (strcmp(arg[0], "spline") == 0)
    tabstyle = SPLINE;
  else
    error->all(FLERR, "Unknown table style in pair_style command: {}", arg[0]);

  tablength = utils::inumeric(FLERR, arg[1], false, lmp);
  if (tablength < 2) error->all(FLERR, "Illegal number of pair table entries");

  // optional keywords
  // assert the tabulation is compatible with a specific long-range solver

  int iarg = 2;
  while (iarg < narg) {
    if (strcmp(arg[iarg], "ewald") == 0)
      ewaldflag = 1;
    else if (strcmp(arg[iarg], "pppm") == 0)
      pppmflag = 1;
    else if (strcmp(arg[iarg], "msm") == 0)
      msmflag = 1;
    else if (strcmp(arg[iarg], "dispersion") == 0)
      dispersionflag = 1;
    else if (strcmp(arg[iarg], "tip4p") == 0)
      tip4pflag = 1;
    else
      error->all(FLERR, "Illegal pair_style command");
    iarg++;
  }

  // delete old tables, since cannot just change settings

  for (int m = 0; m < ntables; m++) free_table(&tables[m]);
  memory->sfree(tables);

  if (allocated) {
    memory->destroy(setflag);
    memory->destroy(cutsq);
    memory->destroy(tabindex);
  }
  allocated = 0;

  ntables = 0;
  tables = nullptr;
}

/* ----------------------------------------------------------------------
   set coeffs for one or more type pairs
------------------------------------------------------------------------- */

void PairTableMod::coeff(int narg, char **arg)
{
  if (narg != 4 && narg != 5) error->all(FLERR, "Illegal pair_coeff command");
  if (!allocated) allocate();

  int ilo, ihi, jlo, jhi;
  utils::bounds(FLERR, arg[0], 1, atom->ntypes, ilo, ihi, error);
  utils::bounds(FLERR, arg[1], 1, atom->ntypes, jlo, jhi, error);

  int me;
  MPI_Comm_rank(world, &me);
  tables = (Table *) memory->srealloc(tables, (ntables + 1) * sizeof(Table), "pair:tables");
  Table *tb = &tables[ntables];
  null_table(tb);
  if (me == 0) read_table(tb, arg[2], arg[3]);
  bcast_table(tb);

  // set table cutoff

  if (narg == 5)
    tb->cut = utils::numeric(FLERR, arg[4], false, lmp);
  else if (tb->rflag)
    tb->cut = tb->rhi;
  else
    tb->cut = tb->rfile[tb->ninput - 1];

  // error check on table parameters
  // insure cutoff is within table

  if (tb->ninput <= 1) error->one(FLERR, "Invalid pair table length");
  double rlo, rhi;
  if (tb->rflag == 0) {
    rlo = tb->rfile[0];
    rhi = tb->rfile[tb->ninput - 1];
  } else {
    rlo = tb->rlo;
    rhi = tb->rhi;
  }
  if (tb->cut <= rlo || tb->cut > rhi) error->all(FLERR, "Pair table cutoff outside of table");
  if (rlo <= 0.0) error->all(FLERR, "Invalid pair table lower boundary");

  // match = 1 if don't need to spline read-in tables
  // this is only the case if r values needed by final tables
  //   exactly match r values read from file
  // for tabstyle SPLINE, always need to build spline tables

  tb->match = 0;
  if (tabstyle == LINEAR && tb->ninput == tablength && tb->rflag == RLINEAR && tb->rhi == tb->cut)
    tb->match = 1;

  // spline read-in values and compute r,e,f vectors within table

  if (tb->match == 0) spline_table(tb);
  compute_table(tb);

  // store ptr to table in tabindex

  int count = 0;
  for (int i = ilo; i <= ihi; i++) {
    for (int j = MAX(jlo, i); j <= jhi; j++) {
      tabindex[i][j] = ntables;
      setflag[i][j] = 1;
      count++;
    }
  }

  if (count == 0) error->all(FLERR, "Illegal pair_coeff command");
  ntables++;
}

/* ----------------------------------------------------------------------
   init for one type pair i,j and corresponding j,i
------------------------------------------------------------------------- */

double PairTableMod::init_one(int i, int j)
{
  if (setflag[i][j] == 0) error->all(FLERR, "All pair coeffs are not set");

  tabindex[j][i] = tabindex[i][j];

  return tables[tabindex[i][j]].cut;
}

/* ----------------------------------------------------------------------
   read a table section from a tabulated potential file
   only called by proc 0
   this function sets these values in Table:
     ninput,rfile,efile,ffile,rflag,rlo,rhi,fpflag,fplo,fphi,ntablebits
------------------------------------------------------------------------- */

void PairTableMod::read_table(Table *tb, char *file, char *keyword)
{
  TableFileReader reader(lmp, file, "pair", unit_convert_flag);

  // transparently convert units for supported conversions

  int unit_convert = reader.get_unit_convert();
  double conversion_factor = utils::get_conversion_factor(utils::ENERGY, unit_convert);
  char *line = reader.find_section_start(keyword);

  if (!line) { error->one(FLERR, "Did not find keyword in table file"); }

  // read args on 2nd line of section
  // allocate table arrays for file values

  line = reader.next_line();
  param_extract(tb, line);
  memory->create(tb->rfile, tb->ninput, "pair:rfile");
  memory->create(tb->efile, tb->ninput, "pair:efile");
  memory->create(tb->ffile, tb->ninput, "pair:ffile");

  // read r,e,f table values from file
  // if rflag set, compute r
  // if rflag not set, use r from file

  double rfile, rnew;

  int rerror = 0;
  reader.skip_line();
  for (int i = 0; i < tb->ninput; i++) {
    line = reader.next_line();
    if (!line)
      error->one(FLERR, "Data missing when parsing pair table '{}' line {} of {}.", keyword, i + 1,
                 tb->ninput);
    try {
      ValueTokenizer values(line);
      values.next_int();
      rfile = values.next_double();
      tb->efile[i] = conversion_factor * values.next_double();
      tb->ffile[i] = conversion_factor * values.next_double();
    } catch (TokenizerException &e) {
      error->one(FLERR, "Error parsing pair table '{}' line {} of {}. {}\nLine was: {}", keyword,
                 i + 1, tb->ninput, e.what(), line);
    }

    rnew = rfile;
    if (tb->rflag == RLINEAR)
      rnew = tb->rlo + (tb->rhi - tb->rlo) * i / (tb->ninput - 1);
    else if (tb->rflag == RSQ) {
      rnew = tb->rlo * tb->rlo + (tb->rhi * tb->rhi - tb->rlo * tb->rlo) * i / (tb->ninput - 1);
      rnew = sqrt(rnew);
    }

    if (tb->rflag && fabs(rnew - rfile) / rfile > EPSILONR) rerror++;

    tb->rfile[i] = rnew;
  }

  // warn if force != dE/dr at any point that is not an inflection point
  // check via secant approximation to dE/dr
  // skip two end points since do not have surrounding secants
  // inflection point is where curvature changes sign

  double r, e, f, rprev, rnext, eprev, enext, fleft, fright;

  int ferror = 0;

  for (int i = 1; i < tb->ninput - 1; i++) {
    r = tb->rfile[i];
    rprev = tb->rfile[i - 1];
    rnext = tb->rfile[i + 1];
    e = tb->efile[i];
    eprev = tb->efile[i - 1];
    enext = tb->efile[i + 1];
    f = tb->ffile[i];
    fleft = -(e - eprev) / (r - rprev);
    fright = -(enext - e) / (rnext - r);
    if (f < fleft && f < fright) ferror++;
    if (f > fleft && f > fright) ferror++;
    //printf("Values %d: %g %g %g\n",i,r,e,f);
    //printf("  secant %d %d %g: %g %g %g\n",i,ferror,r,fleft,fright,f);
  }

  if (ferror)
    error->warning(FLERR,
                   "{} of {} force values in table {} are inconsistent with -dE/dr.\n"
                   "WARNING:  Should only be flagged at inflection points",
                   ferror, tb->ninput, keyword);

  // warn if re-computed distance values differ from file values

  if (rerror)
    error->warning(FLERR,
                   "{} of {} distance values in table {} with relative error\n"
                   "WARNING:  over {} to re-computed values",
                   rerror, tb->ninput, EPSILONR, keyword);
}

/* ----------------------------------------------------------------------
   broadcast read-in table info from proc 0 to other procs
   this function communicates these values in Table:
     ninput,rfile,efile,ffile,rflag,rlo,rhi,fpflag,fplo,fphi
------------------------------------------------------------------------- */

void PairTableMod::bcast_table(Table *tb)
{
  MPI_Bcast(&tb->ninput, 1, MPI_INT, 0, world);

  int me;
  MPI_Comm_rank(world, &me);
  if (me > 0) {
    memory->create(tb->rfile, tb->ninput, "pair:rfile");
    memory->create(tb->efile, tb->ninput, "pair:efile");
    memory->create(tb->ffile, tb->ninput, "pair:ffile");
  }

  MPI_Bcast(tb->rfile, tb->ninput, MPI_DOUBLE, 0, world);
  MPI_Bcast(tb->efile, tb->ninput, MPI_DOUBLE, 0, world);
  MPI_Bcast(tb->ffile, tb->ninput, MPI_DOUBLE, 0, world);

  MPI_Bcast(&tb->rflag, 1, MPI_INT, 0, world);
  if (tb->rflag) {
    MPI_Bcast(&tb->rlo, 1, MPI_DOUBLE, 0, world);
    MPI_Bcast(&tb->rhi, 1, MPI_DOUBLE, 0, world);
  }
  MPI_Bcast(&tb->fpflag, 1, MPI_INT, 0, world);
  if (tb->fpflag) {
    MPI_Bcast(&tb->fplo, 1, MPI_DOUBLE, 0, world);
    MPI_Bcast(&tb->fphi, 1, MPI_DOUBLE, 0, world);
  }
}

/* ----------------------------------------------------------------------
   build spline representation of e,f over entire range of read-in table
   this function sets these values in Table: e2file,f2file
------------------------------------------------------------------------- */

void PairTableMod::spline_table(Table *tb)
{
  memory->create(tb->e2file, tb->ninput, "pair:e2file");
  memory->create(tb->f2file, tb->ninput, "pair:f2file");

  double ep0 = -tb->ffile[0];
  double epn = -tb->ffile[tb->ninput - 1];
  spline(tb->rfile, tb->efile, tb->ninput, ep0, epn, tb->e2file);

  if (tb->fpflag == 0) {
    tb->fplo = (tb->ffile[1] - tb->ffile[0]) / (tb->rfile[1] - tb->rfile[0]);
    tb->fphi = (tb->ffile[tb->ninput - 1] - tb->ffile[tb->ninput - 2]) /
        (tb->rfile[tb->ninput - 1] - tb->rfile[tb->ninput - 2]);
  }

  double fp0 = tb->fplo;
  double fpn = tb->fphi;
  spline(tb->rfile, tb->ffile, tb->ninput, fp0, fpn, tb->f2file);
}

/* ----------------------------------------------------------------------
   extract attributes from parameter line in table section
   format of line: N value R/RSQ lo hi FPRIME fplo fphi
   N is required, other params are optional
------------------------------------------------------------------------- */

void PairTableMod::param_extract(Table *tb, char *line)
{
  tb->ninput = 0;
  tb->rflag = NONE;
  tb->fpflag = 0;

  try {
    ValueTokenizer values(line);

    while (values.has_next()) {
      std::string word = values.next_string();
      if (word == "N") {
        tb->ninput = values.next_int();
      } else if ((word == "R") || (word == "RSQ")) {
        if (word == "R")
          tb->rflag = RLINEAR;
        else if (word == "RSQ")
          tb->rflag = RSQ;
        tb->rlo = values.next_double();
        tb->rhi = values.next_double();
      } else if (word == "FPRIME") {
        tb->fpflag = 1;
        tb->fplo = values.next_double();
        tb->fphi = values.next_double();
      } else {
        error->one(FLERR, "Invalid keyword {} in pair table parameters", word);
      }
    }
  } catch (TokenizerException &e) {
    error->one(FLERR, e.what());
  }

  if (tb->ninput == 0) error->one(FLERR, "Pair table parameters did not set N");
}

/* ----------------------------------------------------------------------
   compute r,e,f vectors from splined values
------------------------------------------------------------------------- */

void PairTableMod::compute_table(Table *tb)
{
  int tlm1 = tablength - 1;

  // inner = inner table bound
  // cut = outer table bound
  // delta = table spacing in r for N-1 bins

  if (tb->rflag)
    tb->inner = tb->rlo;
  else
    tb->inner = tb->rfile[0];
  tb->innersq = tb->inner * tb->inner;
  tb->delta = (tb->cut - tb->inner) / tlm1;
  tb->invdelta = 1.0 / tb->delta;

  // direct lookup tables
  // N-1 evenly spaced bins in r from inner to cut
  // e,f = value at midpt of bin
  // e,f are N-1 in length since store 1 value at bin midpt
  // f is converted to f/r when stored in f[i]
  // e,f are never a match to read-in values, always computed via spline interp

  if (tabstyle == LOOKUP) {
    memory->create(tb->e, tlm1, "pair:e");
    memory->create(tb->f, tlm1, "pair:f");

    double r;
    for (int i = 0; i < tlm1; i++) {
      r = tb->inner + (i + 0.5) * tb->delta;
      tb->e[i] = splint(tb->rfile, tb->efile, tb->e2file, tb->ninput, r);
      tb->f[i] = splint(tb->rfile, tb->ffile, tb->f2file, tb->ninput, r) / r;
    }
  }

  // linear tables
  // N-1 evenly spaced bins in r from inner to cut
  // r,e,f = value at lower edge of bin
  // de,df values = delta from lower edge to upper edge of bin
  // r,e,f are N in length so de,df arrays can compute difference
  // f is converted to f/r when stored in f[i]
  // e,f can match read-in values, else compute via spline interp

  if (tabstyle == LINEAR) {
    memory->create(tb->r, tablength, "pair:r");
    memory->create(tb->e, tablength, "pair:e");
    memory->create(tb->f, tablength, "pair:f");
    memory->create(tb->de, tlm1, "pair:de");
    memory->create(tb->df, tlm1, "pair:df");

    double r;
    for (int i = 0; i < tablength; i++) {
      r = tb->inner + i * tb->delta;
      tb->r[i] = r;
      if (tb->match) {
        tb->e[i] = tb->efile[i];
        tb->f[i] = tb->ffile[i] / r;
      } else {
        tb->e[i] = splint(tb->rfile, tb->efile, tb->e2file, tb->ninput, r);
        tb->f[i] = splint(tb->rfile, tb->ffile, tb->f2file, tb->ninput, r) / r;
      }
    }

    for (int i = 0; i < tlm1; i++) {
      tb->de[i] = tb->e[i + 1] - tb->e[i];
      tb->df[i] = tb->f[i + 1] - tb->f[i];
    }
  }

  // cubic spline tables
  // N-1 evenly spaced bins in r from inner to cut
  // r,e,f = value at lower edge of bin
  // e2,f2 = spline coefficient for each bin
  // r,e,f,e2,f2 are N in length so have N-1 spline bins
  // f is converted to f/r after e is splined
  // e,f can match read-in values, else compute via spline interp

  if (tabstyle == SPLINE) {
    memory->create(tb->r, tablength, "pair:r");
    memory->create(tb->e, tablength, "pair:e");
    memory->create(tb->f, tablength, "pair:f");
    memory->create(tb->e2, tablength, "pair:e2");
    memory->create(tb->f2, tablength, "pair:f2");

    tb->deltasq6 = tb->delta * tb->delta / 6.0;

    double r;
    for (int i = 0; i < tablength; i++) {
      r = tb->inner + i * tb->delta;
      tb->r[i] = r;
      if (tb->match) {
        tb->e[i] = tb->efile[i];
        tb->f[i] = tb->ffile[i] / r;
      } else {
        tb->e[i] = splint(tb->rfile, tb->efile, tb->e2file, tb->ninput, r);
        tb->f[i] = splint(tb->rfile, tb->ffile, tb->f2file, tb->ninput, r);
      }
    }

    // ep0,epn = dh/dg at inner and at cut
    // h(r) = e(r) and g(r) = r^2
    // dh/dg = (de/dr) / 2r = -f/2r

    double ep0 = -tb->f[0] / (2.0 * tb->inner);
    double epn = -tb->f[tlm1] / (2.0 * tb->cut);
    spline(tb->r, tb->e, tablength, ep0, epn, tb->e2);

    // fp0,fpn = dh/dg at inner and at cut
    // h(r) = f(r)/r and g(r) = r^2
    // dh/dg = (1/r df/dr - f/r^2) / 2r
    // dh/dg in secant approx = (f(r2)/r2 - f(r1)/r1) / (g(r2) - g(r1))

    double fp0, fpn;
    double secant_factor = 0.1;
    if (tb->fpflag)
      fp0 = (tb->fplo / tb->inner - tb->f[0] / tb->inner) / (2.0 * tb->inner);
    else {
      double r1 = tb->inner;
      double r2 = r1 + secant_factor * tb->delta;
      fp0 = (splint(tb->rfile, tb->ffile, tb->f2file, tb->ninput, r2) / r2 -
             tb->f[0] / r1) / (secant_factor * tb->delta);
    }

    if (tb->fpflag && tb->cut == tb->rfile[tb->ninput - 1])
      fpn = (tb->fphi / tb->cut - tb->f[tlm1] / (tb->cut * tb->cut)) / (2.0 * tb->cut);
    else {
      double r2 = tb->cut;
      double r1 = r2 - secant_factor * tb->delta;
      fpn = (tb->f[tlm1] / r2) -
             splint(tb->rfile, tb->ffile, tb->f2file, tb->ninput, r1) / r1 /
             (secant_factor * tb->delta);
    }

    for (int i = 0; i < tablength; i++) tb->f[i] /= tb->r[i];
    spline(tb->r, tb->f, tablength, fp0, fpn, tb->f2);
  }
}

/* ----------------------------------------------------------------------
   set all ptrs in a table to a null pointer, so can be freed safely
------------------------------------------------------------------------- */

void PairTableMod::null_table(Table *tb)
{
  tb->rfile = tb->efile = tb->ffile = nullptr;
  tb->e2file = tb->f2file = nullptr;
  tb->r = tb->e = tb->de = nullptr;
  tb->f = tb->df = tb->e2 = tb->f2 = nullptr;
}

/* ----------------------------------------------------------------------
   free all arrays in a table
------------------------------------------------------------------------- */

void PairTableMod::free_table(Table *tb)
{
  memory->destroy(tb->rfile);
  memory->destroy(tb->efile);
  memory->destroy(tb->ffile);
  memory->destroy(tb->e2file);
  memory->destroy(tb->f2file);

  memory->destroy(tb->r);
  memory->destroy(tb->e);
  memory->destroy(tb->de);
  memory->destroy(tb->f);
  memory->destroy(tb->df);
  memory->destroy(tb->e2);
  memory->destroy(tb->f2);
}

/* ----------------------------------------------------------------------
   proc 0 writes to restart file
------------------------------------------------------------------------- */

void PairTableMod::write_restart(FILE *fp)
{
  write_restart_settings(fp);
}

/* ----------------------------------------------------------------------
   proc 0 reads from restart file, bcasts
------------------------------------------------------------------------- */

void PairTableMod::read_restart(FILE *fp)
{
  read_restart_settings(fp);
  allocate();
}

/* ----------------------------------------------------------------------
   proc 0 writes to restart file
------------------------------------------------------------------------- */

void PairTableMod::write_restart_settings(FILE *fp)
{
  fwrite(&tabstyle, sizeof(int), 1, fp);
  fwrite(&tablength, sizeof(int), 1, fp);
  fwrite(&ewaldflag, sizeof(int), 1, fp);
  fwrite(&pppmflag, sizeof(int), 1, fp);
  fwrite(&msmflag, sizeof(int), 1, fp);
  fwrite(&dispersionflag, sizeof(int), 1, fp);
  fwrite(&tip4pflag, sizeof(int), 1, fp);
}

/* ----------------------------------------------------------------------
   proc 0 reads from restart file, bcasts
------------------------------------------------------------------------- */

void PairTableMod::read_restart_settings(FILE *fp)
{
  if (comm->me == 0) {
    utils::sfread(FLERR, &tabstyle, sizeof(int), 1, fp, nullptr, error);
    utils::sfread(FLERR, &tablength, sizeof(int), 1, fp, nullptr, error);
    utils::sfread(FLERR, &ewaldflag, sizeof(int), 1, fp, nullptr, error);
    utils::sfread(FLERR, &pppmflag, sizeof(int), 1, fp, nullptr, error);
    utils::sfread(FLERR, &msmflag, sizeof(int), 1, fp, nullptr, error);
    utils::sfread(FLERR, &dispersionflag, sizeof(int), 1, fp, nullptr, error);
    utils::sfread(FLERR, &tip4pflag, sizeof(int), 1, fp, nullptr, error);
  }
  MPI_Bcast(&tabstyle, 1, MPI_INT, 0, world);
  MPI_Bcast(&tablength, 1, MPI_INT, 0, world);
  MPI_Bcast(&ewaldflag, 1, MPI_INT, 0, world);
  MPI_Bcast(&pppmflag, 1, MPI_INT, 0, world);
  MPI_Bcast(&msmflag, 1, MPI_INT, 0, world);
  MPI_Bcast(&dispersionflag, 1, MPI_INT, 0, world);
  MPI_Bcast(&tip4pflag, 1, MPI_INT, 0, world);
}

/* ---------------------------------------------------------------------- */

double PairTableMod::single(int /*i*/, int /*j*/, int itype, int jtype, double rsq,
                         double /*factor_coul*/, double factor_lj, double &fforce)
{
  int itable;
  double fraction, value, a, b, phi;
  int tlm1 = tablength - 1;

  Table *tb = &tables[tabindex[itype][jtype]];
  if (rsq < tb->innersq) error->one(FLERR, "Pair distance < table inner cutoff");

  double r = sqrt(rsq);
  if (tabstyle == LOOKUP) {
    itable = static_cast<int>((r - tb->inner) * tb->invdelta);
    if (itable >= tlm1) error->one(FLERR, "Pair distance > table outer cutoff");
    fforce = factor_lj * tb->f[itable];
  } else if (tabstyle == LINEAR) {
    itable = static_cast<int>((r - tb->inner) * tb->invdelta);
    if (itable >= tlm1) error->one(FLERR, "Pair distance > table outer cutoff");
    fraction = (r - tb->r[itable]) * tb->invdelta;
    value = tb->f[itable] + fraction * tb->df[itable];
    fforce = factor_lj * value;
  } else if (tabstyle == SPLINE) {
    itable = static_cast<int>((r - tb->inner) * tb->invdelta);
    if (itable >= tlm1) error->one(FLERR, "Pair distance > table outer cutoff");
    b = (r - tb->r[itable]) * tb->invdelta;
    a = 1.0 - b;
    value = a * tb->f[itable] + b * tb->f[itable + 1] +
        ((a * a * a - a) * tb->f2[itable] + (b * b * b - b) * tb->f2[itable + 1]) * tb->deltasq6;
    fforce = factor_lj * value;
  }

  if (tabstyle == LOOKUP)
    phi = tb->e[itable];
  else if (tabstyle == LINEAR)
    phi = tb->e[itable] + fraction * tb->de[itable];
  else
    phi = a * tb->e[itable] + b * tb->e[itable + 1] +
        ((a * a * a - a) * tb->e2[itable] + (b * b * b - b) * tb->e2[itable + 1]) * tb->deltasq6;
  return factor_lj * phi;
}

/* ----------------------------------------------------------------------
   return the Coulomb cutoff for tabled potentials
   called by KSpace solvers which require that all pairwise cutoffs be the same
   loop over all tables not just those indexed by tabindex[i][j] since
     no way to know which tables are active since pair::init() not yet called
------------------------------------------------------------------------- */

void *PairTableMod::extract(const char *str, int &dim)
{
  if (strcmp(str, "cut_coul") != 0) return nullptr;
  if (ntables == 0) error->all(FLERR, "All pair coeffs are not set");

  // only check for cutoff consistency if claiming to be KSpace compatible

  if (ewaldflag || pppmflag || msmflag || dispersionflag || tip4pflag) {
    double cut_coul = tables[0].cut;
    for (int m = 1; m < ntables; m++)
      if (tables[m].cut != cut_coul)
        error->all(FLERR, "Pair table cutoffs must all be equal to use with KSpace");
    dim = 0;
    return &tables[0].cut;
  } else
    return nullptr;
}
