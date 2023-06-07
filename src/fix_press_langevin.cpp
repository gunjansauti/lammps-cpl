// clang-format off
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
   Contributing author: Germain Clavier (TUe)
------------------------------------------------------------------------- */

#include "fix_press_langevin.h"

#include "atom.h"
#include "comm.h"
#include "compute.h"
#include "domain.h"
#include "error.h"
#include "fix_deform.h"
#include "force.h"
#include "group.h"
#include "irregular.h"
#include "kspace.h"
#include "modify.h"
#include "random_mars.h"
#include "update.h"

#include <cmath>
#include <cstring>

using namespace LAMMPS_NS;
using namespace FixConst;

#define DELTAFLIP 0.1
#define TILTMAX 1.5

enum{NONE,XYZ,XY,YZ,XZ};
enum{ISO,ANISO,TRICLINIC};

/* ---------------------------------------------------------------------- */

FixPressLangevin::FixPressLangevin(LAMMPS *lmp, int narg, char **arg) :
  Fix(lmp, narg, arg),
  id_press(nullptr), pflag(0), random(nullptr), irregular(nullptr)
{
  if (narg < 5) error->all(FLERR,"Illegal fix press/langevin command");

  // Langevin barostat applied every step
  // For details on the equations of motion see:
  // Grønbech-Jensen & Farago J. Chem. Phys. 141 194108 (2014)

  nevery = 1;

  // default values

  pcouple = NONE;
  allremap = 1;
  pre_exchange_flag = 0;
  flipflag = 1;

  // Piston mass
  p_mass = 1e-3;

  // Target temperature
  t_start = t_stop = t_target = 0.0;

  for (int i = 0; i < 6; i++) {
    // Pressure and pistons period tau_p
    p_start[i] = p_stop[i] = p_period[i] = 0.0;
    p_flag[i] = 0;

    p_fric[i] = 0.;

    // Pistons coordinates derivative V
    p_deriv[i] = 0.0;

    // a and b values for each piston
    gjfa[i] = 0.0;
    gjfb[i] = 0.0;

    // Random value for each piston
    fran[i] = 0.0;
    f_piston[i] = 0.0;
  }

  // process keywords

  dimension = domain->dimension;

  int iarg = 3;

  while (iarg < narg) {
    if (strcmp(arg[iarg],"iso") == 0) {
      if (iarg+4 > narg)
        error->all(FLERR,"Illegal fix press/langevin command");
      pcouple = XYZ;
      p_start[0] = p_start[1] = p_start[2] = utils::numeric(FLERR,arg[iarg+1],false,lmp);
      p_stop[0] = p_stop[1] = p_stop[2] = utils::numeric(FLERR,arg[iarg+2],false,lmp);
      p_period[0] = p_period[1] = p_period[2] = utils::numeric(FLERR,arg[iarg+3],false,lmp);
      p_flag[0] = p_flag[1] = p_flag[2] = 1;
      if (dimension == 2) {
        p_start[2] = p_stop[2] = p_period[2] = 0.0;
        p_flag[2] = 0;
      }
      iarg += 4;
    } else if (strcmp(arg[iarg],"aniso") == 0) {
      if (iarg+4 > narg)
        error->all(FLERR,"Illegal fix press/langevin command");
      pcouple = NONE;
      p_start[0] = p_start[1] = p_start[2] = utils::numeric(FLERR,arg[iarg+1],false,lmp);
      p_stop[0] = p_stop[1] = p_stop[2] = utils::numeric(FLERR,arg[iarg+2],false,lmp);
      p_period[0] = p_period[1] = p_period[2] = utils::numeric(FLERR,arg[iarg+3],false,lmp);
      p_flag[0] = p_flag[1] = p_flag[2] = 1;
      if (dimension == 2) {
        p_start[2] = p_stop[2] = p_period[2] = 0.0;
        p_flag[2] = 0;
      }
      iarg += 4;
    } else if (strcmp(arg[iarg],"tri") == 0) {
      if (iarg+4 > narg)
        error->all(FLERR,"Illegal fix press/langevin command");
      pcouple = NONE;
      p_start[0] = p_start[1] = p_start[2] = utils::numeric(FLERR,arg[iarg+1],false,lmp);
      p_stop[0] = p_stop[1] = p_stop[2] = utils::numeric(FLERR,arg[iarg+2],false,lmp);
      p_period[0] = p_period[1] = p_period[2] = utils::numeric(FLERR,arg[iarg+3],false,lmp);
      p_flag[0] = p_flag[1] = p_flag[2] = 1;
      p_start[3] = p_start[4] = p_start[5] = 0.0;
      p_stop[3] = p_stop[4] = p_stop[5] = 0.0;
      p_period[3] = p_period[4] = p_period[5] =
        utils::numeric(FLERR,arg[iarg+3],false,lmp);
      p_flag[3] = p_flag[4] = p_flag[5] = 1;
      if (dimension == 2) {
        p_start[2] = p_stop[2] = p_period[2] = 0.0;
        p_flag[2] = 0;
        p_start[3] = p_stop[3] = p_period[3] = 0.0;
        p_flag[3] = 0;
        p_start[4] = p_stop[4] = p_period[4] = 0.0;
        p_flag[4] = 0;
      }
      iarg += 4;
    } else if (strcmp(arg[iarg],"x") == 0) {
      if (iarg+4 > narg)
        error->all(FLERR,"Illegal fix press/langevin command");
      p_start[0] = utils::numeric(FLERR,arg[iarg+1],false,lmp);
      p_stop[0] = utils::numeric(FLERR,arg[iarg+2],false,lmp);
      p_period[0] = utils::numeric(FLERR,arg[iarg+3],false,lmp);
      p_flag[0] = 1;
      iarg += 4;
    } else if (strcmp(arg[iarg],"y") == 0) {
      if (iarg+4 > narg)
        error->all(FLERR,"Illegal fix press/langevin command");
      p_start[1] = utils::numeric(FLERR,arg[iarg+1],false,lmp);
      p_stop[1] = utils::numeric(FLERR,arg[iarg+2],false,lmp);
      p_period[1] = utils::numeric(FLERR,arg[iarg+3],false,lmp);
      p_flag[1] = 1;
      iarg += 4;
    } else if (strcmp(arg[iarg],"z") == 0) {
      if (iarg+4 > narg)
        error->all(FLERR,"Illegal fix press/langevin command");
      p_start[2] = utils::numeric(FLERR,arg[iarg+1],false,lmp);
      p_stop[2] = utils::numeric(FLERR,arg[iarg+2],false,lmp);
      p_period[2] = utils::numeric(FLERR,arg[iarg+3],false,lmp);
      p_flag[2] = 1;
      iarg += 4;
      if (dimension == 2)
        error->all(FLERR,"Invalid fix press/langevin for a 2d simulation");
    } else if (strcmp(arg[iarg],"xy") == 0) {
      if (iarg+4 > narg) utils::missing_cmd_args(FLERR, fmt::format("fix {} yz", style), error);
      p_start[3] = utils::numeric(FLERR,arg[iarg+1],false,lmp);
      p_stop[3] = utils::numeric(FLERR,arg[iarg+2],false,lmp);
      p_period[3] = utils::numeric(FLERR,arg[iarg+3],false,lmp);
      p_flag[3] = 1;
      iarg += 4;
      if (dimension == 2) error->all(FLERR,"Invalid fix {} command for a 2d simulation", style);

    } else if (strcmp(arg[iarg],"xz") == 0) {
      if (iarg+4 > narg) utils::missing_cmd_args(FLERR, fmt::format("fix {} xz", style), error);
      p_start[4] = utils::numeric(FLERR,arg[iarg+1],false,lmp);
      p_stop[4] = utils::numeric(FLERR,arg[iarg+2],false,lmp);
      p_period[4] = utils::numeric(FLERR,arg[iarg+3],false,lmp);
      p_flag[4] = 1;
      iarg += 4;
      if (dimension == 2) error->all(FLERR,"Invalid fix {} command for a 2d simulation", style);

    } else if (strcmp(arg[iarg],"yz") == 0) {
      if (iarg+4 > narg) utils::missing_cmd_args(FLERR, fmt::format("fix {} xy", style), error);
      p_start[5] = utils::numeric(FLERR,arg[iarg+1],false,lmp);
      p_stop[5] = utils::numeric(FLERR,arg[iarg+2],false,lmp);
      p_period[5] = utils::numeric(FLERR,arg[iarg+3],false,lmp);
      p_flag[5] = 1;
      iarg += 4;
      if (dimension == 2) error->all(FLERR,"Invalid fix {} command for a 2d simulation", style);

    } else if (strcmp(arg[iarg],"flip") == 0) {
      if (iarg+2 > narg) utils::missing_cmd_args(FLERR, fmt::format("fix {} flip", style), error);
      flipflag = utils::logical(FLERR,arg[iarg+1],false,lmp);
      iarg += 2;

    } else if (strcmp(arg[iarg],"couple") == 0) {
      if (iarg+2 > narg)
        error->all(FLERR,"Illegal fix press/langevin command");
      if (strcmp(arg[iarg+1],"xyz") == 0) pcouple = XYZ;
      else if (strcmp(arg[iarg+1],"xy") == 0) pcouple = XY;
      else if (strcmp(arg[iarg+1],"yz") == 0) pcouple = YZ;
      else if (strcmp(arg[iarg+1],"xz") == 0) pcouple = XZ;
      else if (strcmp(arg[iarg+1],"none") == 0) pcouple = NONE;
      else error->all(FLERR,"Illegal fix press/langevin command");
      iarg += 2;

    } else if (strcmp(arg[iarg],"mass") == 0) {
      if (iarg+2 > narg)
        error->all(FLERR,"Illegal fix press/langevin command");
      p_mass = utils::numeric(FLERR,arg[iarg+1],false,lmp);
      if (p_mass <= 0.0)
        error->all(FLERR,"Illegal fix press/langevin command");
      iarg += 2;
    } else if (strcmp(arg[iarg],"dilate") == 0) {
      if (iarg+2 > narg)
        error->all(FLERR,"Illegal fix press/langevin command");
      if (strcmp(arg[iarg+1],"all") == 0) allremap = 1;
      else if (strcmp(arg[iarg+1],"partial") == 0) allremap = 0;
      else error->all(FLERR,"Illegal fix press/langevin command");
      iarg += 2;
    } else if (strcmp(arg[iarg], "temp") == 0) {
      if (iarg+4 > narg)
        error->all(FLERR,"Illegal fix press/langevin command");
      t_start = utils::numeric(FLERR,arg[iarg+1],false,lmp);
      t_stop = utils::numeric(FLERR,arg[iarg+2],false,lmp);
      seed = utils::numeric(FLERR,arg[iarg+3],false,lmp);
      if (seed <= 0.0)
        error->all(FLERR,"Illegal fix press/langevin command");
      iarg += 4;
    }

    else error->all(FLERR,"Illegal fix press/langevin command");
  }

  if (allremap == 0) restart_pbc = 1;

  random = new RanMars(lmp, seed);

  // error checks

  if (dimension == 2 && p_flag[2])
    error->all(FLERR,"Invalid fix press/langevin for a 2d simulation");
  if (dimension == 2 && (pcouple == YZ || pcouple == XZ))
    error->all(FLERR,"Invalid fix press/langevin for a 2d simulation");

  if (pcouple == XYZ && (p_flag[0] == 0 || p_flag[1] == 0))
    error->all(FLERR,"Invalid fix press/langevin pressure settings");
  if (pcouple == XYZ && dimension == 3 && p_flag[2] == 0)
    error->all(FLERR,"Invalid fix press/langevin pressure settings");
  if (pcouple == XY && (p_flag[0] == 0 || p_flag[1] == 0))
    error->all(FLERR,"Invalid fix press/langevin pressure settings");
  if (pcouple == YZ && (p_flag[1] == 0 || p_flag[2] == 0))
    error->all(FLERR,"Invalid fix press/langevin pressure settings");
  if (pcouple == XZ && (p_flag[0] == 0 || p_flag[2] == 0))
    error->all(FLERR,"Invalid fix press/langevin pressure settings");

  if (p_flag[0] && domain->xperiodic == 0)
    error->all(FLERR,
               "Cannot use fix press/langevin on a non-periodic dimension");
  if (p_flag[1] && domain->yperiodic == 0)
    error->all(FLERR,
               "Cannot use fix press/langevin on a non-periodic dimension");
  if (p_flag[2] && domain->zperiodic == 0)
    error->all(FLERR,
               "Cannot use fix press/langevin on a non-periodic dimension");

  // require periodicity in 2nd dim of off-diagonal tilt component

  if (p_flag[3] && domain->zperiodic == 0)
    error->all(FLERR, "Cannot use fix {} on a 2nd non-periodic dimension", style);
  if (p_flag[4] && domain->zperiodic == 0)
    error->all(FLERR, "Cannot use fix {} on a 2nd non-periodic dimension", style);
  if (p_flag[5] && domain->yperiodic == 0)
    error->all(FLERR, "Cannot use fix {} on a 2nd non-periodic dimension", style);
  if (!domain->triclinic && (p_flag[3] || p_flag[4] || p_flag[5]))
    error->all(FLERR,"Can not specify Pxy/Pxz/Pyz in fix {} with non-triclinic box", style);

  if (pcouple == XYZ && dimension == 3 &&
      (p_start[0] != p_start[1] || p_start[0] != p_start[2] ||
       p_stop[0] != p_stop[1] || p_stop[0] != p_stop[2] ||
       p_period[0] != p_period[1] || p_period[0] != p_period[2]))
    error->all(FLERR,"Invalid fix press/langevin pressure settings");
  if (pcouple == XYZ && dimension == 2 &&
      (p_start[0] != p_start[1] || p_stop[0] != p_stop[1] ||
       p_period[0] != p_period[1]))
    error->all(FLERR,"Invalid fix press/langevin pressure settings");
  if (pcouple == XY &&
      (p_start[0] != p_start[1] || p_stop[0] != p_stop[1] ||
       p_period[0] != p_period[1]))
    error->all(FLERR,"Invalid fix press/langevin pressure settings");
  if (pcouple == YZ &&
      (p_start[1] != p_start[2] || p_stop[1] != p_stop[2] ||
       p_period[1] != p_period[2]))
    error->all(FLERR,"Invalid fix press/langevin pressure settings");
  if (pcouple == XZ &&
      (p_start[0] != p_start[2] || p_stop[0] != p_stop[2] ||
       p_period[0] != p_period[2]))
    error->all(FLERR,"Invalid fix press/langevin pressure settings");

  if (t_start < 0.0 || t_stop < 0.0)
    error->all(FLERR,"Fix press/langevin temperature parameters must be >= 0.0");

  if ((p_flag[0] && p_period[0] <= 0.0) ||
      (p_flag[1] && p_period[1] <= 0.0) ||
      (p_flag[2] && p_period[2] <= 0.0) ||
      (p_flag[3] && p_period[3] <= 0.0) ||
      (p_flag[4] && p_period[4] <= 0.0) ||
      (p_flag[5] && p_period[5] <= 0.0))
    error->all(FLERR,"Fix press/langevin damping parameters must be > 0.0");

  // pstyle = ISO if XYZ coupling or XY coupling in 2d -> 1 dof
  // else pstyle = ANISO -> 3 dof

  if (p_flag[3] || p_flag[4] || p_flag[5]) pstyle = TRICLINIC;
  else if (pcouple == XYZ || (dimension == 2 && pcouple == XY)) pstyle = ISO;
  else pstyle = ANISO;

    // pre_exchange only required if flips can occur due to shape changes

  if (flipflag && (p_flag[3] || p_flag[4] || p_flag[5]))
    pre_exchange_flag = pre_exchange_migrate = 1;
  if (flipflag && (domain->yz != 0.0 || domain->xz != 0.0 ||
                   domain->xy != 0.0))
    pre_exchange_flag = pre_exchange_migrate = 1;

  if (pre_exchange_flag) irregular = new Irregular(lmp);
  else irregular = nullptr;

  // C1
  // Langevin GJF dynamics does NOT need a temperature compute
  // This is stated explicitely in their paper.
  // The temperature used for the pressure is NkT/V on purpose.

  // id = fix-ID + temp
  // compute group = all since pressure is always global (group all)
  //   and thus its KE/temperature contribution should use group all

  // id_temp = utils::strdup(std::string(id) + "_temp");
  // modify->add_compute(fmt::format("{} all temp",id_temp));
  // tflag = 1;

  // C2
  // Following C1, the compute must use the virial pressure
  // Kinetic contribution will be added by the fix style
  // create a new compute pressure style
  // id = fix-ID + press, compute group = all
  // pass id_temp as 4th arg to pressure constructor

  id_press = utils::strdup(std::string(id) + "_press");
  modify->add_compute(fmt::format("{} all pressure NULL virial", id_press));
  pflag = 1;

  // p_fric is alpha coeff from GJF
  // with alpha = Q/p_period
  // similar to fix_langevin formalism
  for (int i = 0; i < 6; i++) {
    p_fric[i] = p_mass/p_period[i];
    gjfa[i] = (1.0 - p_fric[i] * update->dt / 2.0 / p_mass) / (1.0 + p_fric[i] * update->dt / 2.0 / p_mass);
    gjfb[i] = 1./(1.0 + p_fric[i] * update->dt / 2.0 / p_mass);
  }

  nrigid = 0;
  rfix = nullptr;
}

/* ---------------------------------------------------------------------- */

FixPressLangevin::~FixPressLangevin()
{
  delete random;
  delete[] rfix;
  delete irregular;


  // delete temperature and pressure if fix created them

  if (pflag) modify->delete_compute(id_press);
  delete[] id_press;
}

/* ---------------------------------------------------------------------- */

int FixPressLangevin::setmask()
{
  int mask = 0;
  mask |= INITIAL_INTEGRATE;
  mask |= POST_FORCE;
  mask |= END_OF_STEP;
  if (pre_exchange_flag) mask |= PRE_EXCHANGE;
  return mask;
}

/* ---------------------------------------------------------------------- */

void FixPressLangevin::init()
{
  // ensure no conflict with fix deform

  for (const auto &ifix : modify->get_fix_list())
    if (strcmp(ifix->style, "^deform") == 0) {
      int *dimflag = static_cast<FixDeform *>(ifix)->dimflag;
      if ((p_flag[0] && dimflag[0]) || (p_flag[1] && dimflag[1]) ||
          (p_flag[2] && dimflag[2]) || (p_flag[3] && dimflag[3]) ||
          (p_flag[4] && dimflag[4]) || (p_flag[5] && dimflag[5]))
        error->all(FLERR,"Cannot use fix press/langevin and "
                   "fix deform on same component of stress tensor");
    }

  // set pressure ptr

  pressure = modify->get_compute_by_id(id_press);
  if (!pressure)
    error->all(FLERR, "Pressure compute ID {} for fix press/langevin does not exist", id_press);

  // Kspace setting

  if (force->kspace) kspace_flag = 1;
  else kspace_flag = 0;

  // detect if any rigid fixes exist so rigid bodies move when box is remapped
  // rfix[] = indices to each fix rigid

  delete[] rfix;
  nrigid = 0;
  rfix = nullptr;

  for (int i = 0; i < modify->nfix; i++)
    if (modify->fix[i]->rigid_flag) nrigid++;
  if (nrigid > 0) {
    rfix = new int[nrigid];
    nrigid = 0;
    for (int i = 0; i < modify->nfix; i++)
      if (modify->fix[i]->rigid_flag) rfix[nrigid++] = i;
  }
}

/* ----------------------------------------------------------------------
   compute T,P before integrator starts
------------------------------------------------------------------------- */

void FixPressLangevin::setup(int /*vflag*/)
{
  // trigger virial computation on next timestep

  pressure->addstep(update->ntimestep+1);
}

/* ---------------------------------------------------------------------- */

void FixPressLangevin::initial_integrate(int /* vflag */)
{
  // compute new V

  double dt;
  double dl;
  double displacement;
  double delta = update->ntimestep - update->beginstep;

  // Compute new random term on pistons dynamics
  if (delta != 0.0) delta /= update->endstep - update->beginstep;
  t_target = t_start + delta * (t_stop-t_start);
  couple_beta(t_target);

  dt = update->dt;

  for (int i = 0; i < 6; i++) {
    if (p_flag[i]) {
      // See equation 13
      displacement = dt*p_deriv[i]*gjfb[i];
      displacement += 0.5*dt*dt*f_piston[i]*gjfb[i]/p_mass;
      displacement += 0.5*dt*fran[i]*gjfb[i]/p_mass;
      dl = domain->boxhi[i] - domain->boxlo[i];
      if (i < 3) dilation[i] = (dl + displacement)/dl;
      else dilation[i] = displacement;
    }
  }

  // remap simulation box and atoms
  // redo KSpace coeffs since volume has changed

  remap();
  if (kspace_flag) force->kspace->setup();

}

/* ---------------------------------------------------------------------- */
void FixPressLangevin::post_force(int /*vflag*/)
{
  // compute new forces on pistons after internal virial computation

  double delta = update->ntimestep - update->beginstep;
  if (delta != 0.0) delta /= update->endstep - update->beginstep;

  // Compute current pressure tensor and add kinetic term
  if (pstyle == ISO) {
    pressure->compute_scalar();
  } else {
    pressure->compute_vector();
  }

  couple_pressure();
  couple_kinetic(t_target);

  for (int i = 0; i < 6; i++) {
    if (p_flag[i]) {
      f_old_piston[i] = f_piston[i];
      p_target[i] = p_start[i] + delta * (p_stop[i]-p_start[i]);
      f_piston[i] = p_current[i] - p_target[i];
    }
  }

  // trigger virial computation on next timestep

  pressure->addstep(update->ntimestep+1);
}

/* ---------------------------------------------------------------------- */

void FixPressLangevin::end_of_step()
{
  // compute pistons velocity

  double dt;
  dt = update->dt;

  for (int i = 0; i < 6; i++) {
    if (p_flag[i]) {
      p_deriv[i] *= gjfa[i];
      p_deriv[i] += 0.5*dt*(gjfa[i]*f_old_piston[i]+f_piston[i])/p_mass;
      p_deriv[i] += fran[i]*gjfb[i]/p_mass;
    }
  }

}

/* ---------------------------------------------------------------------- */

void FixPressLangevin::couple_pressure()
{
  double *tensor = pressure->vector;

  if (pstyle == ISO)
    p_current[0] = p_current[1] = p_current[2] = pressure->scalar;
  else if (pcouple == XYZ) {
    double ave = 1.0/3.0 * (tensor[0] + tensor[1] + tensor[2]);
    p_current[0] = p_current[1] = p_current[2] = ave;
  } else if (pcouple == XY) {
    double ave = 0.5 * (tensor[0] + tensor[1]);
    p_current[0] = p_current[1] = ave;
    p_current[2] = tensor[2];
  } else if (pcouple == YZ) {
    double ave = 0.5 * (tensor[1] + tensor[2]);
    p_current[1] = p_current[2] = ave;
    p_current[0] = tensor[0];
  } else if (pcouple == XZ) {
    double ave = 0.5 * (tensor[0] + tensor[2]);
    p_current[0] = p_current[2] = ave;
    p_current[1] = tensor[1];
  } else {
    p_current[0] = tensor[0];
    p_current[1] = tensor[1];
    p_current[2] = tensor[2];
  }
    p_current[3] = tensor[3];
    p_current[4] = tensor[4];
    p_current[5] = tensor[5];
}
/* ---------------------------------------------------------------------- */

void FixPressLangevin::couple_kinetic(double t_target)
{
  double Pk, volume;

  // Kinetic part
  if (dimension == 3) volume = domain->xprd * domain->yprd * domain->zprd;
  else volume = domain->xprd * domain->yprd;

  Pk = atom->natoms*force->boltz*t_target/volume;

  p_current[0] += Pk;
  p_current[1] += Pk;
  if (dimension == 3) p_current[2] += Pk;
}

/* ---------------------------------------------------------------------- */

void FixPressLangevin::couple_beta(double t_target)
{
  double gamma[6];
  int me = comm->me;

  for (int i=0; i<6; i++)
    gamma[i] = sqrt(2.0*force->boltz*update->dt*p_fric[i]*t_target);

  fran[0] = fran[1] = fran[2] = 0.0;
  fran[3] = fran[4] = fran[5] = 0.0;
  if (me == 0) {
    if (pstyle == ISO)
      fran[0] = fran[1] = fran[2] = gamma[0]*random->gaussian();
    else if (pcouple == XYZ) {
      fran[0] = fran[1] = fran[2] = gamma[0]*random->gaussian();
    } else if (pcouple == XY) {
      fran[0] = fran[1] = gamma[0]*random->gaussian();
      fran[2] = gamma[2]*random->gaussian();
    } else if (pcouple == YZ) {
      fran[1] = fran[2] = gamma[1]*random->gaussian();
      fran[0] = gamma[0]*random->gaussian();
    } else if (pcouple == XZ) {
      fran[0] = fran[2] = gamma[0]*random->gaussian();
      fran[1] = gamma[1]*random->gaussian();
    } else {
      fran[0] = gamma[0]*random->gaussian();
      fran[1] = gamma[1]*random->gaussian();
      fran[2] = gamma[2]*random->gaussian();
    }
    fran[3] = gamma[3]*random->gaussian();
    fran[4] = gamma[4]*random->gaussian();
    fran[5] = gamma[5]*random->gaussian();
  }
  MPI_Bcast(&fran, 6, MPI_DOUBLE, 0, world);
}

/* ----------------------------------------------------------------------
   change box size
   remap all atoms or fix group atoms depending on allremap flag
   if rigid bodies exist, scale rigid body centers-of-mass
------------------------------------------------------------------------- */

void FixPressLangevin::remap()
{
  int i;
  double oldlo,oldhi,ctr;

  double **x = atom->x;
  int *mask = atom->mask;
  int nlocal = atom->nlocal;

  // convert pertinent atoms and rigid bodies to lamda coords

  if (allremap) domain->x2lamda(nlocal);
  else {
    for (i = 0; i < nlocal; i++)
      if (mask[i] & groupbit)
        domain->x2lamda(x[i],x[i]);
  }

  if (nrigid)
    for (i = 0; i < nrigid; i++)
      modify->fix[rfix[i]]->deform(0);

  // reset global and local box to new size/shape

  for (i = 0; i < 3; i++) {
    if (p_flag[i]) {
      oldlo = domain->boxlo[i];
      oldhi = domain->boxhi[i];
      ctr = 0.5 * (oldlo + oldhi);
      domain->boxlo[i] = (oldlo-ctr)*dilation[i] + ctr;
      domain->boxhi[i] = (oldhi-ctr)*dilation[i] + ctr;
    }
  }

  if (p_flag[3]) domain->xy += dilation[3];
  if (p_flag[4]) domain->xz += dilation[4];
  if (p_flag[5]) domain->yz += dilation[5];

  if (domain->yz < -TILTMAX*domain->yprd ||
      domain->yz > TILTMAX*domain->yprd ||
      domain->xz < -TILTMAX*domain->xprd ||
      domain->xz > TILTMAX*domain->xprd ||
      domain->xy < -TILTMAX*domain->xprd ||
      domain->xy > TILTMAX*domain->xprd)
    error->all(FLERR,"Fix {} has tilted box too far in one step - "
               "periodic cell is too far from equilibrium state", style);

  domain->set_global_box();
  domain->set_local_box();

  // convert pertinent atoms and rigid bodies back to box coords

  if (allremap) domain->lamda2x(nlocal);
  else {
    for (i = 0; i < nlocal; i++)
      if (mask[i] & groupbit)
        domain->lamda2x(x[i],x[i]);
  }

  if (nrigid)
    for (i = 0; i < nrigid; i++)
      modify->fix[rfix[i]]->deform(1);
}

/* ----------------------------------------------------------------------
  if any tilt ratios exceed limits, set flip = 1 and compute new tilt values
  do not flip in x or y if non-periodic (can tilt but not flip)
    this is b/c the box length would be changed (dramatically) by flip
  if yz tilt exceeded, adjust C vector by one B vector
  if xz tilt exceeded, adjust C vector by one A vector
  if xy tilt exceeded, adjust B vector by one A vector
  check yz first since it may change xz, then xz check comes after
  if any flip occurs, create new box in domain
  image_flip() adjusts image flags due to box shape change induced by flip
  remap() puts atoms outside the new box back into the new box
  perform irregular on atoms in lamda coords to migrate atoms to new procs
  important that image_flip comes before remap, since remap may change
    image flags to new values, making eqs in doc of Domain:image_flip incorrect
------------------------------------------------------------------------- */

void FixPressLangevin::pre_exchange()
{
  double xprd = domain->xprd;
  double yprd = domain->yprd;

  // flip is only triggered when tilt exceeds 0.5 by DELTAFLIP
  // this avoids immediate re-flipping due to tilt oscillations

  double xtiltmax = (0.5+DELTAFLIP)*xprd;
  double ytiltmax = (0.5+DELTAFLIP)*yprd;

  int flipxy,flipxz,flipyz;
  flipxy = flipxz = flipyz = 0;

  if (domain->yperiodic) {
    if (domain->yz < -ytiltmax) {
      domain->yz += yprd;
      domain->xz += domain->xy;
      flipyz = 1;
    } else if (domain->yz >= ytiltmax) {
      domain->yz -= yprd;
      domain->xz -= domain->xy;
      flipyz = -1;
    }
  }

  if (domain->xperiodic) {
    if (domain->xz < -xtiltmax) {
      domain->xz += xprd;
      flipxz = 1;
    } else if (domain->xz >= xtiltmax) {
      domain->xz -= xprd;
      flipxz = -1;
    }
    if (domain->xy < -xtiltmax) {
      domain->xy += xprd;
      flipxy = 1;
    } else if (domain->xy >= xtiltmax) {
      domain->xy -= xprd;
      flipxy = -1;
    }
  }

  int flip = 0;
  if (flipxy || flipxz || flipyz) flip = 1;

  if (flip) {
    domain->set_global_box();
    domain->set_local_box();

    domain->image_flip(flipxy,flipxz,flipyz);

    double **x = atom->x;
    imageint *image = atom->image;
    int nlocal = atom->nlocal;
    for (int i = 0; i < nlocal; i++) domain->remap(x[i],image[i]);

    domain->x2lamda(atom->nlocal);
    irregular->migrate_atoms();
    domain->lamda2x(atom->nlocal);
  }
}

/* ---------------------------------------------------------------------- */

int FixPressLangevin::modify_param(int narg, char **arg)
{
  if (strcmp(arg[0],"press") == 0) {
    if (narg < 2) error->all(FLERR,"Illegal fix_modify command");
    if (pflag) {
      modify->delete_compute(id_press);
      pflag = 0;
    }
    delete[] id_press;
    id_press = utils::strdup(arg[1]);

    pressure = modify->get_compute_by_id(arg[1]);
    if (pressure) error->all(FLERR,"Could not find fix_modify pressure compute ID: {}", arg[1]);
    if (pressure->pressflag == 0)
      error->all(FLERR,"Fix_modify pressure compute {} does not compute pressure", arg[1]);
    return 2;
  }
  return 0;
}

/* ---------------------------------------------------------------------- */

void FixPressLangevin::reset_dt()
{
  for (int i=0; i<6; i++) {
    gjfa[i] = (1.0 - p_fric[i] * update->dt / 2.0 / p_mass) / (1.0 + p_fric[i] * update->dt / 2.0 / p_mass);
    gjfb[i] = sqrt(1.0 + p_fric[i] * update->dt / 2.0 / p_mass);
    }
}
