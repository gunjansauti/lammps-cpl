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
   Contributing authors:
   Joel Clemmer (SNL), Thomas O'Connor (CMU), Eric Palermo (CMU)
----------------------------------------------------------------------- */

#include "fix_rheo_pressure.h"

#include "atom.h"
#include "comm.h"
#include "domain.h"
#include "error.h"
#include "fix_rheo.h"
#include "memory.h"
#include "modify.h"
#include "update.h"

#include <cmath>

using namespace LAMMPS_NS;
using namespace FixConst;
enum {NONE, LINEAR, CUBIC, TAITWATER};

static constexpr double SEVENTH = 1.0 / 7.0;

/* ---------------------------------------------------------------------- */

FixRHEOPressure::FixRHEOPressure(LAMMPS *lmp, int narg, char **arg) :
  Fix(lmp, narg, arg), fix_rheo(nullptr)
{
  if (narg < 4) error->all(FLERR,"Illegal fix command");

  pressure_style = NONE;

  comm_forward = 1;
  nmax_store = 0;

  // Currently can only have one instance of fix rheo/pressure
  if (igroup != 0)
    error->all(FLERR,"fix rheo/pressure command requires group all");

  int ntypes = atom->ntypes;
  int iarg = 3;
  if (strcmp(arg[iarg],"linear") == 0) {
    pressure_style = LINEAR;
  } else if (strcmp(arg[iarg],"taitwater") == 0) {
    pressure_style = TAITWATER;
  } else if (strcmp(arg[iarg],"cubic") == 0) {
    pressure_style = CUBIC;
    if (iarg + 1 >= narg) error->all(FLERR,"Insufficient arguments for pressure option");
    c_cubic = utils::numeric(FLERR,arg[iarg + 1],false,lmp);
  } else {
    error->all(FLERR,"Illegal fix command, {}", arg[iarg]);
  }

  if (pressure_style == NONE)
    error->all(FLERR,"Must specify pressure style for fix/rheo/pressure");
}

/* ---------------------------------------------------------------------- */

FixRHEOPressure::~FixRHEOPressure()
{
  // Remove custom property if it exists
  int tmp1, tmp2, index;
  index = atom->find_custom("rheo_pressure", tmp1, tmp2);
  if (index != -1) atom->remove_custom(index, 1, 0);
}

/* ---------------------------------------------------------------------- */

int FixRHEOPressure::setmask()
{
  int mask = 0;
  mask |= PRE_FORCE;
  return mask;
}

/* ---------------------------------------------------------------------- */

void FixRHEOPressure::init()
{
  auto fixes = modify->get_fix_by_style("^rheo$");
  if (fixes.size() == 0) error->all(FLERR, "Need to define fix rheo to use fix rheo/pressure");
  fix_rheo = dynamic_cast<FixRHEO *>(fixes[0]);

  csq = fix_rheo->csq;
  rho0 = fix_rheo->rho0;
  rho0inv = 1.0 / rho0;

  // Cannot define multiple as pair rheo cannot currently distinguish
  if (modify->get_fix_by_style("rheo/pressure").size() > 1)
    error->all(FLERR,"Can only specify one instance of fix rheo/pressure");
}

/* ---------------------------------------------------------------------- */

void FixRHEOPressure::setup_pre_force(int /*vflag*/)
{
  fix_rheo->pressure_fix_defined = 1;

  // Create pressure array if it doesn't already exist
  // Create a custom atom property so it works with compute property/atom
  // Do not create grow callback as there's no reason to copy/exchange data
  // Manually grow if nmax_store exceeded

  int tmp1, tmp2;
  int index = atom->find_custom("rheo_pressure", tmp1, tmp2);
  if (index == -1) {
    index = atom->add_custom("rheo_pressure", 1, 0);
    nmax_store = atom->nmax;
  }
  pressure = atom->dvector[index];

  pre_force(0);
}

/* ----------------------------------------------------------------------
  Update (and forward) pressure every timestep
------------------------------------------------------------------------- */

void FixRHEOPressure::pre_force(int /*vflag*/)
{
  int i;
  double dr, rr3, rho_ratio;

  int *mask = atom->mask;
  double *rho = atom->rho;

  int nlocal = atom->nlocal;

  if (nmax_store < atom->nmax) {
    memory->grow(pressure, atom->nmax, "atom:rheo_pressure");
    nmax_store = atom->nmax;
  }

  for (i = 0; i < nlocal; i++) {
    if (mask[i] & groupbit) {
      if (pressure_style == LINEAR) {
        pressure[i] = csq * (rho[i] - rho0);
      } else if (pressure_style == CUBIC) {
        dr = rho[i] - rho0;
        pressure[i] = csq * (dr + c_cubic * dr * dr * dr);
      } else if (pressure_style == TAITWATER) {
        rho_ratio = rho[i] / rho0inv;
        rr3 = rho_ratio * rho_ratio * rho_ratio;
        pressure[i] = csq * rho0 * SEVENTH * (rr3 * rr3 * rho_ratio - 1.0);
      }
    }
  }

  if (comm_forward) comm->forward_comm(this);
}

/* ---------------------------------------------------------------------- */

int FixRHEOPressure::pack_forward_comm(int n, int *list, double *buf,
                                        int /*pbc_flag*/, int * /*pbc*/)
{
  int i,j,k,m;
  m = 0;

  for (i = 0; i < n; i++) {
    j = list[i];
    buf[m++] = pressure[j];
  }
  return m;
}

/* ---------------------------------------------------------------------- */

void FixRHEOPressure::unpack_forward_comm(int n, int first, double *buf)
{
  int i, k, m, last;

  m = 0;
  last = first + n;
  for (i = first; i < last; i++) {
    pressure[i] = buf[m++];
  }
}

/* ---------------------------------------------------------------------- */

double FixRHEOPressure::calc_pressure(double rho)
{
  double p, dr, rr3, rho_ratio;

  if (pressure_style == LINEAR) {
    p = csq * (rho - rho0);
  } else if (pressure_style == CUBIC) {
    dr = rho - rho0;
    p = csq * (dr + c_cubic * dr * dr * dr);
  } else if (pressure_style == TAITWATER) {
    rho_ratio = rho / rho0inv;
    rr3 = rho_ratio * rho_ratio * rho_ratio;
    p = csq * rho0 * SEVENTH * (rr3 * rr3 * rho_ratio - 1.0);
  }
  return rho;
}

/* ---------------------------------------------------------------------- */

double FixRHEOPressure::memory_usage()
{
  double bytes = 0.0;
  bytes += (size_t) nmax_store * sizeof(double);
  return bytes;
}
