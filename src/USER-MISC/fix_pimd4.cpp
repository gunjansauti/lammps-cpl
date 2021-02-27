/* ----------------------------------------------------------------------
   LAMMPS - Large-scale Atomic/Molecular Massively Parallel Simulator
   http://lammps.sandia.gov, Sandia National Laboratories
   Steve Plimpton, sjplimp@sandia.gov

   Copyright (2003) Sandia Corporation.  Under the terms of Contract
   DE-AC04-94AL85000 with Sandia Corporation, the U.S. Government retains
   certain rights in this software.  This software is distributed under
   the GNU General Public License.

   See the README file in the top-level LAMMPS directory.
------------------------------------------------------------------------- */

/* ----------------------------------------------------------------------
   Package      FixPIMD4
   Purpose      Quantum Path Integral Algorithm for Quantum Chemistry
   Copyright    Voth Group @ University of Chicago
   Authors      Chris Knight & Yuxing Peng (yuxing at uchicago.edu)

   Updated      Oct-01-2011
   Version      1.0
------------------------------------------------------------------------- */

#include "fix_pimd4.h"
#include <mpi.h>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include "universe.h"
#include "comm.h"
#include "neighbor.h"
#include "force.h"
#include "utils.h"
#include "timer.h"
#include "atom.h"
#include "compute.h"
#include "modify.h"
#include "domain.h"
#include "update.h"
#include "math_const.h"
#include "random_mars.h"
#include "memory.h"
#include "error.h"

using namespace LAMMPS_NS;
using namespace FixConst;
using namespace MathConst;

enum{PIMD,NMPIMD,CMD};
enum{baoab};
enum{SVR, PILE_L, PILE_G};
enum{nve, nvt};
enum{MSTI, SCTI};

#define INVOKED_SCALAR 1

/* ---------------------------------------------------------------------- */

FixPIMD4::FixPIMD4(LAMMPS *lmp, int narg, char **arg) : 
  Fix(lmp, narg, arg),
  random(nullptr), c_pe(nullptr)
{
  method       = PIMD;
  integrator   = baoab;
  thermostat   = PILE_L;
  ensemble     = nvt;
  fmass        = 1.0;
  temp         = 298.15;
  baoab_temp   = 298.15;
  sp           = 1.0;
  harmonicflag = 0;
  omega        = 0.0;
  tiflag       = 0;
  timethod     = MSTI;
  lambda       = 0.0;

  for(int i=3; i<narg-1; i+=2)
  {
    if(strcmp(arg[i],"method")==0)
    {
      if(strcmp(arg[i+1],"pimd")==0) method=PIMD;
      else if(strcmp(arg[i+1],"nmpimd")==0) method=NMPIMD;
      else if(strcmp(arg[i+1],"cmd")==0) method=CMD;
      else error->universe_all(FLERR,"Unknown method parameter for fix pimd");
    }

    else if(strcmp(arg[i], "integrator")==0)
    {
      if(strcmp(arg[i+1], "baoab")==0) integrator=baoab;
      else error->universe_all(FLERR, "Unknown integrator parameter for fix pimd. Only baoab integrator is supported!");
    }

    else if(strcmp(arg[i], "ensemble")==0)
    {
      if(strcmp(arg[i+1], "nve")==0) ensemble=nve;
      else if(strcmp(arg[i+1], "nvt")==0) ensemble=nvt;
      else error->universe_all(FLERR, "Unknown ensemble parameter for fix pimd. Only nve and nvt ensembles are supported!");
    }

    else if(strcmp(arg[i],"fmass")==0)
    {
      fmass = atof(arg[i+1]);
      if(fmass<0.0 || fmass>1.0) error->universe_all(FLERR,"Invalid fmass value for fix pimd");
    }

    else if(strcmp(arg[i],"sp")==0)
    {
      sp = atof(arg[i+1]);
      if(fmass<0.0) error->universe_all(FLERR,"Invalid sp value for fix pimd");
    }

    else if(strcmp(arg[i],"temp")==0)
    {
      temp = atof(arg[i+1]);
      if(temp<0.0) error->universe_all(FLERR,"Invalid temp value for fix pimd");
    } 

    else if(strcmp(arg[i], "thermostat")==0)
    {
      if(strcmp(arg[i+1],"PILE_G")==0) 
      {
        thermostat = PILE_G;
        seed = atoi(arg[i+2]);
        i++;
      }
      else if(strcmp(arg[i+1], "SVR")==0)
      {
        thermostat = SVR;
        seed = atoi(arg[i+2]);
        i++;
      }
      else if(strcmp(arg[i+1],"PILE_L")==0) 
      {
        thermostat = PILE_L;
        seed = atoi(arg[i+2]);
        i++;
      }
      else error->universe_all(FLERR,"Unknown thermostat parameter for fix pimd");
    }

    else if(strcmp(arg[i], "tau")==0)
    {
      tau = atof(arg[i+1]);
    }
  
    else if(strcmp(arg[i], "ti")==0)
    {
      tiflag = 1;
      if(strcmp(arg[i+1], "MSTI")==0)  timethod = MSTI;
      else if(strcmp(arg[i+1], "SCTI")==0)  timethod = SCTI;
      else error->universe_all(FLERR, "Unknown method parameter for thermodynamic integration");
      lambda = atof(arg[i+2]);
      i++;
    }
 
    else if(strcmp(arg[i], "model")==0)
    {
      harmonicflag = 1;
      omega = atof(arg[i+1]);
      if(omega<0) error->universe_all(FLERR,"Invalid model frequency value for fix pimd");
    }
    else error->universe_all(arg[i],i+1,"Unknown keyword for fix pimd");
  }

  // initialize Marsaglia RNG with processor-unique seed

  if(integrator==baoab)
  {
    baoab_temp = temp;
    random = new RanMars(lmp, seed + universe->me);
  }
  
  /* Initiation */

  max_nsend = 0;
  tag_send = nullptr;
  buf_send = nullptr;

  max_nlocal = 0;
  buf_recv = nullptr;
  buf_beads = nullptr;

  coords_send = coords_recv = nullptr;
  forces_send = forces_recv = nullptr;
  nsend = nrecv = 0;
  tags_send = nullptr;
  coords = nullptr;
  forces = nullptr;
  size_plan = 0;
  plan_send = plan_recv = nullptr;

  xc = fc = nullptr;
  xf = 0.0;
  t_vir = t_cv = 0.0;
  total_spring_energy = 0.0;
  t_prim = 0.0;

  tote = 0.0;
  totke = 0.0;

  dfdl = 0.0;
  x_scaled = nullptr;

  M_x2xp = M_xp2x = nullptr; // M_f2fp = M_fp2f = nullptr;
  lam = nullptr;
  mode_index = nullptr;

  mass = nullptr;

  array_atom = nullptr;

  gamma = 0.0;
  c1 = 0.0;
  c2 = 0.0;

  restart_peratom = 1;
  peratom_flag    = 1;
  peratom_freq    = 1;

  global_freq = 1;
  thermo_energy = 1;
  vector_flag = 1;
  size_vector = 9;
  extvector   = 1;
  comm_forward = 3;

  atom->add_callback(0); // Call LAMMPS to allocate memory for per-atom array
  atom->add_callback(1); // Call LAMMPS to re-assign restart-data for per-atom array


  // some initilizations

  baoab_ready = false;

  r1 = 0.0;
  r2 = 0.0;

  if(!harmonicflag)
  {
    id_pe = new char[8];
    strcpy(id_pe, "pimd_pe");
    char **newarg = new char*[3];
    newarg[0] = id_pe;
    newarg[1] = (char *) "all";
    newarg[2] = (char *) "pe";
    modify->add_compute(3,newarg);
    delete [] newarg;
  }
  domain->set_global_box();
}

/* ---------------------------------------------------------------------- */
/*
FixPIMD4::~FixPIMD4()
{
  if(thermostat==baoab)
  {
    delete random;
  }
}
*/
/* ---------------------------------------------------------------------- */

int FixPIMD4::setmask()
{
  int mask = 0;
  //mask |= PRE_EXCHANGE;
  mask |= PRE_FORCE;
  mask |= POST_FORCE;
  mask |= INITIAL_INTEGRATE;
  mask |= POST_INTEGRATE;
  mask |= FINAL_INTEGRATE;
  mask |= END_OF_STEP;
  return mask;
}

/* ---------------------------------------------------------------------- */

void FixPIMD4::end_of_step()
{
  compute_totke();
  compute_tote();
  if(update->ntimestep % 10000 == 0)
  {
  if(universe->me==0) printf("This is the end of step %ld.\n", update->ntimestep);
  }
  //if(universe->iworld==0) printf("This is the end of step %ld.\n\n\n", update->ntimestep);
}

/* ---------------------------------------------------------------------- */

void FixPIMD4::init()
{
  if (atom->map_style == 0)
    error->all(FLERR,"Fix pimd requires an atom map, see atom_modify");

  if(universe->me==0 && screen) fprintf(screen,"Fix pimd initializing Path-Integral ...\n");
  // fprintf(stdout, "Fix pimd initilizing Path-Integral ...\n");

  // prepare the constants

  np = universe->nworlds;
  inverse_np = 1.0 / np;

  /* The first solution for the force constant, using SI units

  const double Boltzmann = 1.3806488E-23;    // SI unit: J/K
  const double Plank     = 6.6260755E-34;    // SI unit: m^2 kg / s

  double hbar = Plank / ( 2.0 * MY_PI ) * sp;
  double beta = 1.0 / ( Boltzmann * input.nh_temp);

  // - P / ( beta^2 * hbar^2)   SI unit: s^-2
  double _fbond = -1.0 / (beta*beta*hbar*hbar) * input.nbeads;

  // convert the units: s^-2 -> (kcal/mol) / (g/mol) / (A^2)
  fbond = _fbond * 4.184E+26;

  */

  /* The current solution, using LAMMPS internal real units */

  const double Boltzmann = force->boltz;
  const double Plank     = force->hplanck;

  double hbar   = Plank / ( 2.0 * MY_PI );
  double beta   = 1.0 / (Boltzmann * temp);
  double _fbond = 1.0 * np*np / (beta*beta*hbar*hbar) ;

  omega_np = np / (hbar * beta) * sqrt(force->mvv2e);
  fbond = - _fbond * force->mvv2e;

  if(universe->me==0)
    printf("Fix pimd -P/(beta^2 * hbar^2) = %20.7lE (kcal/mol/A^2)\n\n", fbond);

  if(integrator==baoab)   
  {
    dtf = 0.5 * update->dt * force->ftm2v;
    dtv = 0.5 * update->dt;
  }
  else
  {
    error->universe_all(FLERR,"Unknown integrator parameter for fix pimd");
  }

  comm_init();

  mass = new double [atom->ntypes+1];

  if(method==CMD || method==NMPIMD) nmpimd_init();
  else for(int i=1; i<=atom->ntypes; i++) mass[i] = atom->mass[i] / np * fmass;

  if(integrator==baoab)
  {
    if(!baoab_ready)
    {
      baoab_init();
    }
    // fprintf(stdout, "baoab thermostat initialized!\n");
  }
  else error->universe_all(FLERR,"Unknown integrator parameter for fix pimd");


  // initialize compute pe 
  int ipe = modify->find_compute(id_pe);
  c_pe = modify->compute[ipe];

  t_prim = t_vir = t_cv = p_prim = p_cv = 0.0;

  if(universe->me==0) fprintf(screen, "Fix pimd successfully initialized!\n");
}

void FixPIMD4::setup_pre_force(int vflag)
//void FixPIMD4::setup_pre_exchange()
{
  //atom->x[0][0] = 0.0;
  //atom->x[0][1] = 0.0;
  //atom->x[0][2] = 0.0;
  double *boxlo = domain->boxlo;
  double *boxhi = domain->boxhi;
  //fprintf(stdout, "%.8e, %.8e, %.8e, %.8e, %.8e, %.8e\n", boxlo[0], boxlo[1], boxlo[2], boxhi[0], boxhi[1], boxhi[2]);
  char xdim[8], ydim[8], zdim[8];
  double xdimd, ydimd, zdimd;
  sprintf(xdim, "%.3f", domain->xprd);
  sprintf(ydim, "%.3f", domain->yprd);
  sprintf(zdim, "%.3f", domain->zprd);
  xdimd = (double)(atof(xdim));
  ydimd = (double)(atof(ydim));
  zdimd = (double)(atof(zdim));
  // fprintf(stdout, "%.8e, %.8e, %.8e, %.8e, %.8e, %.8e\n", boxlo[0], boxlo[1], boxlo[2], boxhi[0], boxhi[1], boxhi[2]);
  // fprintf(stdout, "%.8e, %.8e, %.8e.\n", xdimd, ydimd, zdimd);
  boxlo[0] = -0.5 * xdimd;
  boxlo[1] = -0.5 * ydimd;
  boxlo[2] = -0.5 * zdimd;
  boxhi[0] = -boxlo[0];
  boxhi[1] = -boxlo[1];
  boxhi[2] = -boxlo[2];
  domain->xy = domain->yz = domain->xz = 0.0;
  //fprintf(stdout, "%.8e, %.8e, %.8e, %.8e, %.8e, %.8e\n", boxlo[0], boxlo[1], boxlo[2], boxhi[0], boxhi[1], boxhi[2]);
  //fprintf(stdout, "%.3f, %.3f, %.3f, %.3f, %.3f, %.3f, %.3f, %.3f, %.3f\n", boxlo[0], boxlo[1], boxlo[2], boxhi[0], boxhi[1], boxhi[2], domain->xy, domain->xz, domain->yz);

  domain->set_initial_box();
  domain->reset_box();
  domain->box_change=1;


  char x_tmp[20];
  int nlocal = atom->nlocal;
  for(int i=0; i<nlocal; i++)
  {
    for(int j=0; j<3; j++)
    {
      sprintf(x_tmp, "%.4f", atom->x[i][j]);
      atom->x[i][j] = (double)(atof(x_tmp));
      //fprintf(stdout, "%s %.8e\n", x_tmp, atom->x[i][j]);
      //x_tmp = "\0";
    }
  }  
  int triclinic = domain->triclinic;
  if (triclinic) domain->x2lamda(atom->nlocal);
  domain->pbc();
  domain->reset_box();
  comm->setup();
  if (neighbor->style) neighbor->setup_bins();
  comm->exchange();
  comm->borders();
  if (triclinic) domain->lamda2x(atom->nlocal+atom->nghost);
  neighbor->build(1);
  // if(universe->me==0)  printf("Fix pimd successfully initialized!\n");
}

void FixPIMD4::setup(int vflag)
{
  if(universe->me==0 && screen) fprintf(screen,"Setting up Path-Integral ...\n");
  if(universe->me==0) printf("Setting up Path-Integral ...\n");
  post_force(vflag);
  end_of_step();
  c_pe->addstep(update->ntimestep+1); 
  double *boxlo = domain->boxlo;
  double *boxhi = domain->boxhi;
  //fprintf(stdout, "%.8e, %.8e, %.8e, %.8e, %.8e, %.8e\n", boxlo[0], boxlo[1], boxlo[2], boxhi[0], boxhi[1], boxhi[2]);

  //fprintf(stdout, "x=%.4e.\n", atom->x[0][0]);  
}

/* ---------------------------------------------------------------------- */

void FixPIMD4::initial_integrate(int /*vflag*/)
{
  char x_tmp[8];
  int nlocal = atom->nlocal;
  for(int i=0; i<nlocal; i++)
  {
    for(int j=0; j<3; j++)
    {
      //sprintf(x_tmp, "%.4f", atom->x[i][j]);
      //atom->x[i][j] = (double)(atof(x_tmp));
      //fprintf(stdout, "%s %.8e\n", x_tmp, atom->x[i][j]);
      //x_tmp = "\0";
    }
  }  

  double *boxlo = domain->boxlo;
  double *boxhi = domain->boxhi;
  char xdim[8], ydim[8], zdim[8];
  double xdimd, ydimd, zdimd;
  sprintf(xdim, "%.3f", domain->xprd);
  sprintf(ydim, "%.3f", domain->yprd);
  sprintf(zdim, "%.3f", domain->zprd);
  xdimd = (double)(atof(xdim));
  ydimd = (double)(atof(ydim));
  zdimd = (double)(atof(zdim));
  // fprintf(stdout, "%.8e, %.8e, %.8e, %.8e, %.8e, %.8e\n", boxlo[0], boxlo[1], boxlo[2], boxhi[0], boxhi[1], boxhi[2]);
  // fprintf(stdout, "%.8e, %.8e, %.8e.\n", xdimd, ydimd, zdimd);
  boxlo[0] = -0.5 * xdimd;
  boxlo[1] = -0.5 * ydimd;
  boxlo[2] = -0.5 * zdimd;
  boxhi[0] = -boxlo[0];
  boxhi[1] = -boxlo[1];
  boxhi[2] = -boxlo[2];
  domain->xy = domain->yz = domain->xz = 0.0;
  // fprintf(stdout, "%.8e, %.8e, %.8e, %.8e, %.8e, %.8e\n", boxlo[0], boxlo[1], boxlo[2], boxhi[0], boxhi[1], boxhi[2]);
  // fprintf(stdout, "%.3f, %.3f, %.3f, %.3f, %.3f, %.3f, %.3f, %.3f, %.3f\n", boxlo[0], boxlo[1], boxlo[2], boxhi[0], boxhi[1], boxhi[2], domain->xy, domain->xz, domain->yz);

  domain->set_initial_box();
  domain->reset_box();
  domain->box_change=1;
  // printf("begin to initialize integrate\n");
  if(integrator==baoab)
  {
    // printf("trying to perform b_step\n");
    b_step();
    // printf("b_step succeeded\n");
    
    if(method==NMPIMD)
    {
      nmpimd_fill(atom->x);
      // printf("filled\n");
      comm_exec(atom->x);
      // printf("executed\n");
      nmpimd_transform(buf_beads, atom->x, M_x2xp[universe->iworld]);
      // printf("transformed\n");
    }
    // printf("nm trans finished\n");
  // if(universe->iworld==0) printf("after x2xp, %.6e, %.6e, %.6e, %.6e, %.6e, %.6e, %.6e, %.6e, %.6e, %.6e.\n", atom->x[0][0], atom->v[0][0], atom->f[0][0], mass[atom->type[0]], dtf, dtv, dtf, _omega_np, baoab_c, baoab_s);

   a_step();
  }

  else
  {
    error->universe_all(FLERR,"Unknown integrator parameter for fix pimd");
  }
  // printf("initial_integrate finished!\n");
}

/* ---------------------------------------------------------------------- */

void FixPIMD4::post_integrate()
{
  if(integrator==baoab)
  {
    if(ensemble==nvt)
    {
      o_step();
      a_step();
    }
    else if(ensemble==nve)
    {
      a_step();
    }
    else
    {
      error->universe_all(FLERR, "Unknown ensemble parameter for fix pimd. Only nve and nvt are supported!\n");
    }

    if(method==NMPIMD)
    {
      nmpimd_fill(atom->x);
      comm_exec(atom->x);
      nmpimd_transform(buf_beads, atom->x, M_xp2x[universe->iworld]);
    }
  //printf("after xp2x, x=%.6e, v=%.6e, f=%.6e, dtf=%.6e, dtv=%.6e.\n", atom->x[0][0], atom->v[0][0], atom->f[0][0], dtf, dtv);

  char x_tmp[8];
  int nlocal = atom->nlocal;
  for(int i=0; i<nlocal; i++)
  {
    for(int j=0; j<3; j++)
    {
      //sprintf(x_tmp, "%.4f", atom->x[i][j]);
      //atom->x[i][j] = (double)(atof(x_tmp));
      //fprintf(stdout, "%s %.8e\n", x_tmp, atom->x[i][j]);
      //x_tmp = "\0";
    }
  }  
  }

  else
  {
    error->universe_all(FLERR, "Unknown integrator parameter for fix pimd");
  }
}

/* ---------------------------------------------------------------------- */

void FixPIMD4::final_integrate()
{
  if(integrator==baoab)
  {
    b_step();
  }
  else
  {
    error->universe_all(FLERR,"Unknown integrator parameter for fix pimd");
  }
}

/* ---------------------------------------------------------------------- */

void FixPIMD4::post_force(int /*flag*/)
{
  //char x_tmp[8];
  //int nlocal = atom->nlocal;
  //for(int i=0; i<nlocal; i++)
  //{
  //  for(int j=0; j<3; j++)
  //  {
  //    sprintf(x_tmp, "%.4f", atom->f[i][j]);
  //    atom->f[i][j] = (double)(atof(x_tmp));
  //    // fprintf(stdout, "%s %.8e\n", x_tmp, atom->f[i][j]);
  //    //x_tmp = "\0";
  //  }
  //}  
  inv_volume = 1.0 / (domain->xprd * domain->yprd * domain->zprd);
  comm_exec(atom->x);
  compute_spring_energy();
  comm_coords();
  comm_forces();
  compute_xc();
  compute_fc();
  compute_vir();
  compute_t_prim();
  compute_t_vir();
  compute_p_prim();
  compute_p_cv();
  compute_pote();
  // fprintf(stdout, "me = %d, x = %.6e, f = %.6e.\n", universe->me, atom->x[0][0], atom->f[0][0]);
  // fprintf(stdout, "me = %d, vir = %.6e, pote = %.6e.\n", universe->me, vir, pote);
  // fprintf(stdout, "Coming into post_force!\n");
  
  // divide the force by np 
  // for(int i=0; i<atom->nlocal; i++) 
  // {
  //   for(int j=0; j<3; j++) 
  //   {
  //     atom->f[i][j] = atom->f[i][j] * 1.0 / np;
  //   }
  // }
  // fprintf(stdout, "Successfully computed t_prim!\n");

  // transform the force into normal mode representation
  if(method==NMPIMD)
  {
    nmpimd_fill(atom->f);
    comm_exec(atom->f);
    nmpimd_transform(buf_beads, atom->f, M_x2xp[universe->iworld]);
  }
  // if(universe->iworld==0) printf("after f2fp, %.6e, %.6e, %.6e, %.6e, %.6e, %.6e, %.6e, %.6e, %.6e, %.6e.\n", atom->x[0][0], atom->v[0][0], atom->f[0][0], mass[atom->type[0]], dtf, dtv, dtf, _omega_np, baoab_c, baoab_s);
  c_pe->addstep(update->ntimestep+1); 
}

/* ----------------------------------------------------------------------
   Langevin thermostat, BAOAB integrator
------------------------------------------------------------------------- */

void FixPIMD4::baoab_init()
{
  //fprintf(stdout, "baoab_temp=%.2f.\n", baoab_temp);
  double KT = force->boltz * baoab_temp;
  double beta = 1.0 / KT;
  double hbar = force->hplanck / (2.0 * MY_PI);
  _omega_np = np / beta / hbar;
  double _omega_np_dt_half = _omega_np * update->dt * 0.5;

  _omega_k = new double[np];
  baoab_c = new double[np];
  baoab_s = new double[np];
  for (int i=0; i<np; i++)
  {
    _omega_k[i] = _omega_np * sqrt(lam[i]); 
    baoab_c[i] = cos(sqrt(lam[i])*_omega_np_dt_half);
    baoab_s[i] = sin(sqrt(lam[i])*_omega_np_dt_half);
  }
  // baoab_c = cos(_omega_np_dt_half);
  // baoab_s = sin(_omega_np_dt_half);
  // printf("initializing baoab, c = %.6f, s = %.6f.\n", baoab_c, baoab_s);
  if(tau > 0) gamma = 0.5 / tau;
  else gamma = sqrt(np) / beta / hbar;
  c1 = exp(-gamma * update->dt);
  c2 = sqrt(1.0 - c1 * c1);

  if(thermostat == PILE_G)
  {
    int natoms = atom->natoms;
    eta = new double* [np];
    for(int i=0; i<np; i++) eta[i] = new double [3*natoms];
  }

  baoab_ready = true;
}

/* ---------------------------------------------------------------------- */

void FixPIMD4::b_step()
{
  // if(universe->iworld==0) printf("start of b_step, %.6e.\n", atom->x[0][0]);
  //fprintf(stdout, "step=%ld, starting b_step, x=%.8e, v=%.8e, f=%.8e, dtf=%.6e.\n", update->ntimestep, atom->x[0][0], atom->v[0][0], atom->f[0][0],dtf);
  int n = atom->nlocal;
  int *type = atom->type;
  double **v = atom->v;
  double **f = atom->f;

  for(int i=0; i<n; i++)
  {
    double dtfm = dtf / mass[type[i]];
    v[i][0] += dtfm * f[i][0];
    v[i][1] += dtfm * f[i][1];
    v[i][2] += dtfm * f[i][2];
  }

  double dtfm = dtf / mass[type[0]];
  //printf("end of b_step, x=%.8e, v=%.6e, f=%.6e, dtf=%.6e, mass=%.6e, dtfm=%.6e, o_np=%.6e, c=%.6e, s=%.6e.\n", atom->x[0][0], atom->v[0][0], atom->f[0][0], dtf, mass[type[0]], dtfm, _omega_np, baoab_c[1], baoab_s[1]);
}

/* ---------------------------------------------------------------------- */

void FixPIMD4::a_step(){
  int n = atom->nlocal;
  double **x = atom->x;
  double **v = atom->v;
  double x0, x1, x2, v0, v1, v2; // three components of x[i] and v[i]

  //fprintf(stdout, "step=%ld, starting a_step, x=%.8e, v=%.8e, dtv=%.6e.\n", update->ntimestep, x[0][0], v[0][0], dtv);
  if(universe->iworld == 0)
  {

    for(int i=0; i<n; i++)
    {
      x[i][0] += dtv * v[i][0];
      x[i][1] += dtv * v[i][1];
      x[i][2] += dtv * v[i][2];
    }
  }
  else if(universe->iworld != 0)
  {
    for(int i=0; i<n; i++)
    {
      x0 = x[i][0]; x1 = x[i][1]; x2 = x[i][2];
      v0 = v[i][0]; v1 = v[i][1]; v2 = v[i][2];
      x[i][0] = baoab_c[universe->iworld] * x0 + 1./_omega_k[universe->iworld] * baoab_s[universe->iworld] * v0;
      x[i][1] = baoab_c[universe->iworld] * x1 + 1./_omega_k[universe->iworld] * baoab_s[universe->iworld] * v1;
      x[i][2] = baoab_c[universe->iworld] * x2 + 1./_omega_k[universe->iworld] * baoab_s[universe->iworld] * v2;
      v[i][0] = -1.*_omega_k[universe->iworld] * baoab_s[universe->iworld] * x0 + baoab_c[universe->iworld] * v0;
      v[i][1] = -1.*_omega_k[universe->iworld] * baoab_s[universe->iworld] * x1 + baoab_c[universe->iworld] * v1;
      v[i][2] = -1.*_omega_k[universe->iworld] * baoab_s[universe->iworld] * x2 + baoab_c[universe->iworld] * v2;
    }
  }

  //fprintf(stdout, "step=%ld, end_a_step, x=%.8e, v=%.8e, dtv=%.6e.\n", update->ntimestep, x[0][0], v[0][0], dtv);
  // if(universe->iworld==0) printf("end of a_step, %.6e, %.6e, %.6e, %.6e, %.6e, %.6e, %.6e, %.6e, %.6e, %.6e.\n", atom->x[0][0], atom->v[0][0], atom->f[0][0], mass[atom->type[0]], dtf, dtv, dtf, _omega_np, baoab_c, baoab_s);
}

/* ---------------------------------------------------------------------- */
void FixPIMD4::svr_step()
{
  int nlocal = atom->nlocal;
  int *type = atom->type;
  double beta_np = 1.0 / force->boltz / baoab_temp / np * force->mvv2e;

  // compute centroid kinetic energy
  double ke_0 = 0.0;
  for(int i=0; i<nlocal; i++) for(int j=0; j<3; j++) ke_0 += 0.5 * mass[type[i]] * atom->v[i][j] * atom->v[i][j];
  MPI_Allreduce(&ke_0, &ke_centroid, 1, MPI_DOUBLE, MPI_SUM, world);

  // compute alpha
  double noise_ = 0.0, ksi0_ = 0.0, ksi_ = 0.0;
  for(int i=0; i<atom->natoms; i++) 
  {
    for(int j=0; j<3; j++) 
    {
      ksi_ = random->gaussian();
      if(i==0 && j==0) ksi0_ = ksi_;
      noise_ += ksi_ * ksi_;
    }
  }
  
  alpha2 = c1 + (1.0 - c1) * (noise_) / 2 / beta_np / ke_centroid + 2 * ksi0_ * sqrt(c1 * (1.0 - c1) / 2 / beta_np / ke_centroid);
  sgn_ = ksi0_ + sqrt(2 * beta_np * ke_centroid * c1 / (1.0 - c1));
  // sgn = sgn_ / abs(sgn_);
  if(sgn_<0) sgn = -1.0;
  else sgn = 1.0;
  alpha = sgn * sqrt(alpha2);

  // broadcast alpha to the other processes in world 0
  MPI_Bcast(&alpha, 1, MPI_DOUBLE, 0, world);

  // fprintf(stdout, "me = %d, alpha = %.6e.\n", universe->me, alpha);

  // scale the velocities
  for(int i=0; i<nlocal; i++)
  {
    for(int j=0; j<3; j++)
    {
      atom->v[i][j] *= alpha;
    }
  }

}

void FixPIMD4::o_step()
{
  int nlocal = atom->nlocal;
  int *type = atom->type;
  double beta_np = 1.0 / force->boltz / baoab_temp / np * force->mvv2e;
  if(thermostat == PILE_L)
  {
    for(int i=0; i<nlocal; i++)
    {
      r1 = random->gaussian();
      r2 = random->gaussian();
      r3 = random->gaussian();
      atom->v[i][0] = c1 * atom->v[i][0] + c2 * sqrt(1.0 / mass[type[i]] / beta_np) * r1; 
      atom->v[i][1] = c1 * atom->v[i][1] + c2 * sqrt(1.0 / mass[type[i]] / beta_np) * r2;
      atom->v[i][2] = c1 * atom->v[i][2] + c2 * sqrt(1.0 / mass[type[i]] / beta_np) * r3;
    }
  }
  else if(thermostat == SVR)
  {
    svr_step();
  }
  else if(thermostat == PILE_G)
  {
    if(universe->iworld == 0)
    {
      svr_step();
/*
      // compute centroid kinetic energy
      double ke_0 = 0.0;
      for(int i=0; i<nlocal; i++) for(int j=0; j<3; j++) ke_0 += 0.5 * mass[type[i]] * atom->v[i][j] * atom->v[i][j];
      MPI_Allreduce(&ke_0, &ke_centroid, 1, MPI_DOUBLE, MPI_SUM, world);

      // compute alpha
      double noise_ = 0.0, ksi0_ = 0.0, ksi_ = 0.0;
      for(int i=0; i<atom->natoms; i++) 
      {
        for(int j=0; j<3; j++) 
        {
          ksi_ = random->gaussian();
          if(i==0 && j==0) ksi0_ = ksi_;
          noise_ += ksi_ * ksi_;
        }
      }
      
      alpha2 = c1 + (1.0 - c1) * (noise_) / 2 / beta / ke_centroid + 2 * ksi0_ * sqrt(c1 * (1.0 - c1) / 2 / beta / ke_centroid);
      sgn_ = ksi0_ + sqrt(2 * beta * ke_centroid * c1 / (1.0 - c1));
      // sgn = sgn_ / abs(sgn_);
      if(sgn_<0) sgn = -1.0;
      else sgn = 1.0;
      alpha = sgn * sqrt(alpha2);

      // broadcast alpha to the other processes in world 0
      MPI_Bcast(&alpha, 1, MPI_DOUBLE, 0, world);

      // fprintf(stdout, "me = %d, alpha = %.6e.\n", universe->me, alpha);

      for(int i=0; i<nlocal; i++)
      {
        for(int j=0; j<3; j++)
        {
          atom->v[i][j] *= alpha;
        }
      }
*/
    }
    else
    {
      for(int i=0; i<nlocal; i++)
      {
        r1 = random->gaussian();
        r2 = random->gaussian();
        r3 = random->gaussian();
        atom->v[i][0] = c1 * atom->v[i][0] + c2 * sqrt(1.0 / mass[type[i]] / beta_np) * r1; 
        atom->v[i][1] = c1 * atom->v[i][1] + c2 * sqrt(1.0 / mass[type[i]] / beta_np) * r2;
        atom->v[i][2] = c1 * atom->v[i][2] + c2 * sqrt(1.0 / mass[type[i]] / beta_np) * r3;
      }
    }
  }
}

/* ----------------------------------------------------------------------
   Normal Mode PIMD
------------------------------------------------------------------------- */

void FixPIMD4::nmpimd_init()
{
  memory->create(M_x2xp, np, np, "fix_feynman:M_x2xp");
  memory->create(M_xp2x, np, np, "fix_feynman:M_xp2x");

  lam = (double*) memory->smalloc(sizeof(double)*np, "FixPIMD4::lam");

  // Set up  eigenvalues

  lam[0] = 0.0;
  if(np%2==0) lam[np-1] = 4.0;

  for(int i=2; i<=np/2; i++)
  {
    lam[2*i-3] = lam[2*i-2] = 2.0 * (1.0 - 1.0 *cos(2.0*MY_PI*(i-1)/np));
  }

  // Set up eigenvectors for non-degenerated modes

  for(int i=0; i<np; i++)
  {
    M_x2xp[0][i] = 1.0 / sqrt(np);
    if(np%2==0) M_x2xp[np-1][i] = 1.0 / sqrt(np) * pow(-1.0, i);
  }

  // Set up eigenvectors for degenerated modes

  for(int i=0; i<(np-1)/2; i++) for(int j=0; j<np; j++)
  {
    M_x2xp[2*i+1][j] =   sqrt(2.0) * cos ( 2.0 * MY_PI * (i+1) * j / np) / sqrt(np);
    M_x2xp[2*i+2][j] = - sqrt(2.0) * sin ( 2.0 * MY_PI * (i+1) * j / np) / sqrt(np);
  }

  // Set up Ut

  for(int i=0; i<np; i++)
    for(int j=0; j<np; j++)
    {
      M_xp2x[i][j] = M_x2xp[j][i];
    }

  // Set up masses

  int iworld = universe->iworld;

  for(int i=1; i<=atom->ntypes; i++)
  {
    mass[i] = atom->mass[i];

    if(iworld)
    {
//      mass[i] *= lam[iworld];
      mass[i] *= fmass;
    }
  }
}

/* ---------------------------------------------------------------------- */

void FixPIMD4::nmpimd_fill(double **ptr)
{
  comm_ptr = ptr;
  comm->forward_comm_fix(this);
}

/* ---------------------------------------------------------------------- */

void FixPIMD4::nmpimd_transform(double** src, double** des, double *vector)
{
  int n = atom->nlocal;
  int m = 0;

  for(int i=0; i<n; i++) for(int d=0; d<3; d++)
  {
    des[i][d] = 0.0;
    for(int j=0; j<np; j++) { des[i][d] += (src[j][m] * vector[j]); }
    m++;
  }
}

/* ----------------------------------------------------------------------
   Comm operations
------------------------------------------------------------------------- */

void FixPIMD4::comm_init()
{
  if(size_plan)
  {
    delete [] plan_send;
    delete [] plan_recv;
  }

  if(method == PIMD)
  {
    size_plan = 2;
    plan_send = new int [2];
    plan_recv = new int [2];
    mode_index = new int [2];

    int rank_last = universe->me - comm->nprocs;
    int rank_next = universe->me + comm->nprocs;
    if(rank_last<0) rank_last += universe->nprocs;
    if(rank_next>=universe->nprocs) rank_next -= universe->nprocs;

    plan_send[0] = rank_next; plan_send[1] = rank_last;
    plan_recv[0] = rank_last; plan_recv[1] = rank_next;

    mode_index[0] = 0; mode_index[1] = 1;
    x_last = 1; x_next = 0;
  }
  else
  {
    size_plan = np - 1;
    plan_send = new int [size_plan];
    plan_recv = new int [size_plan];
    mode_index = new int [size_plan];

    for(int i=0; i<size_plan; i++)
    {
      plan_send[i] = universe->me + comm->nprocs * (i+1);
      if(plan_send[i]>=universe->nprocs) plan_send[i] -= universe->nprocs;

      plan_recv[i] = universe->me - comm->nprocs * (i+1);
      if(plan_recv[i]<0) plan_recv[i] += universe->nprocs;

      mode_index[i]=(universe->iworld+i+1)%(universe->nworlds);
    }

    x_next = (universe->iworld+1+universe->nworlds)%(universe->nworlds);
    x_last = (universe->iworld-1+universe->nworlds)%(universe->nworlds);
  }

  if(buf_beads)
  {
    for(int i=0; i<np; i++) if(buf_beads[i]) delete [] buf_beads[i];
    delete [] buf_beads;
  }

  buf_beads = new double* [np];
  for(int i=0; i<np; i++) buf_beads[i] = nullptr;
  
  if(coords)
  {
    for(int i=0; i<np; i++) if(coords[i]) delete [] coords[i];
    delete [] coords;
  }

  if(forces)
  {
    for(int i=0; i<np; i++) if(forces[i]) delete [] forces[i];
    delete [] forces;
  }
  
  coords = new double* [np];
  for(int i=0; i<np; i++) coords[i] = nullptr;

  forces = new double* [np];
  for(int i=0; i<np; i++) forces[i] = nullptr;
  
  if(x_scaled)
  {
    for(int i=0; i<np; i++) if(x_scaled[i]) delete [] x_scaled[i];
    delete [] x_scaled;
  }

  x_scaled = new double* [np];
  for(int i=0; i<np; i++) x_scaled[i] = nullptr;
}

/* ---------------------------------------------------------------------- */

void FixPIMD4::comm_exec(double **ptr)
{
  int nlocal = atom->nlocal;

  if(nlocal > max_nlocal)
  {
    max_nlocal = nlocal+200;
    int size = sizeof(double) * max_nlocal * 3;
    buf_recv = (double*) memory->srealloc(buf_recv, size, "FixPIMD4:x_recv");

    for(int i=0; i<np; i++)
      buf_beads[i] = (double*) memory->srealloc(buf_beads[i], size, "FixPIMD4:x_beads[i]");
  }

  // copy local positions

  memcpy(buf_beads[universe->iworld], &(ptr[0][0]), sizeof(double)*nlocal*3);

  // go over comm plans

  for(int iplan = 0; iplan<size_plan; iplan++)
  {
    // sendrecv nlocal

    int nsend;

    MPI_Sendrecv( &(nlocal), 1, MPI_INT, plan_send[iplan], 0,
                  &(nsend),  1, MPI_INT, plan_recv[iplan], 0, universe->uworld, MPI_STATUS_IGNORE);

    // allocate arrays

    if(nsend > max_nsend)
    {
      max_nsend = nsend+200;
      tag_send = (tagint*) memory->srealloc(tag_send, sizeof(tagint)*max_nsend, "FixPIMD4:tag_send");
      buf_send = (double*) memory->srealloc(buf_send, sizeof(double)*max_nsend*3, "FixPIMD4:x_send");
    }

    // send tags

    MPI_Sendrecv( atom->tag, nlocal, MPI_LMP_TAGINT, plan_send[iplan], 0,
                  tag_send,  nsend,  MPI_LMP_TAGINT, plan_recv[iplan], 0, universe->uworld, MPI_STATUS_IGNORE);

    // wrap positions

    double *wrap_ptr = buf_send;
    int ncpy = sizeof(double)*3;

    for(int i=0; i<nsend; i++)
    {
      int index = atom->map(tag_send[i]);

      if(index<0)
      {
        char error_line[256];

        sprintf(error_line, "Atom " TAGINT_FORMAT " is missing at world [%d] "
                "rank [%d] required by  rank [%d] (" TAGINT_FORMAT ", "
                TAGINT_FORMAT ", " TAGINT_FORMAT ").\n", tag_send[i],
                universe->iworld, comm->me, plan_recv[iplan],
                atom->tag[0], atom->tag[1], atom->tag[2]);

        error->universe_one(FLERR,error_line);
      }

      memcpy(wrap_ptr, ptr[index], ncpy);
      wrap_ptr += 3;
    }

    // sendrecv x

    MPI_Sendrecv( buf_send, nsend*3,  MPI_DOUBLE, plan_recv[iplan], 0,
                  buf_recv, nlocal*3, MPI_DOUBLE, plan_send[iplan], 0, universe->uworld, MPI_STATUS_IGNORE);

    // copy x

    memcpy(buf_beads[mode_index[iplan]], buf_recv, sizeof(double)*nlocal*3);
  }
}

/* ---------------------------------------------------------------------- */

void FixPIMD4::comm_coords()
{
  int nlocal = atom->nlocal;

  // assign memory for arrays
  int size_coords = sizeof(double) * nlocal * 3;
  int size_tags;// = sizeof(tagint) * nlocal;
  coords_recv = (double*) memory->srealloc(coords_recv, size_coords, "FixPIMD4:coords_recv");
  for(int i=0; i<np; i++)
  {
    coords[i] = (double*) memory->srealloc(coords[i], size_coords, "FixPIMD4:coords[i]");
  }
  
  // copy local positions and tags
  memcpy(coords[universe->iworld], &(atom->x[0][0]), size_coords);

  // traversing over all the other worlds
  for(int dworld=1; dworld<=np-1; dworld++)
  {
      // send the tags and coords to the process proc_send
      // receive the tags and coords from the process proc_recv
    int proc_send = (universe->me + dworld * comm->nprocs) % universe->nprocs; 
    int proc_recv = (universe->me - dworld * comm->nprocs + universe->nprocs) % universe->nprocs;
    int world_recv = (int)(proc_recv / comm->nprocs);
    
    // determine the number of atoms to be sent to and received from the other worlds
    MPI_Sendrecv(&(nlocal), 1, MPI_INT, proc_send, 0, 
                 &(nsend), 1, MPI_INT, proc_recv, 0, 
                 universe->uworld, MPI_STATUS_IGNORE);
    nrecv = nlocal;

    size_coords = sizeof(double) * nsend * 3;
    size_tags = sizeof(tagint) * nsend;

    coords_send = (double*) memory->srealloc(coords_send, size_coords, "FixPIMD4:coords_send");
    tags_send = (tagint*) memory->srealloc(tags_send, size_tags, "FixPIMD4:tags_send");

    MPI_Sendrecv(atom->tag, nlocal, MPI_LMP_TAGINT, proc_send, 0,
                 tags_send, nsend, MPI_LMP_TAGINT, proc_recv, 0,
                 universe->uworld, MPI_STATUS_IGNORE);

    // wrap positions
    double *wrap_ptr = coords_send;
    int ncpy = sizeof(double)*3;

    for(int i=0; i<nsend; i++)
    {
      int index = atom->map(tags_send[i]);
      if(index < 0)
      {
        char error_line[256];

        sprintf(error_line, "Atom " TAGINT_FORMAT " is missing at world [%d] "
                "rank [%d] required by  rank [%d] (" TAGINT_FORMAT ", "
                TAGINT_FORMAT ", " TAGINT_FORMAT ").\n", tags_send[i],
                universe->iworld, comm->me, proc_recv,
                atom->tag[0], atom->tag[1], atom->tag[2]);

        error->universe_one(FLERR,error_line);
      }    
      memcpy(wrap_ptr, atom->x[index], ncpy);
      wrap_ptr += 3;
    }
    MPI_Sendrecv(coords_send, nsend*3, MPI_DOUBLE, proc_recv, 0,
                coords_recv, nrecv*3, MPI_DOUBLE, proc_send, 0,
                universe->uworld, MPI_STATUS_IGNORE);

    memcpy(coords[world_recv], coords_recv, sizeof(double)*nlocal*3);          
  }
}

void FixPIMD4::comm_forces()
{
  int nlocal = atom->nlocal;

  // assign memory for arrays
  int size_forces = sizeof(double) * nlocal * 3;
  int size_tags;// = sizeof(tagint) * nlocal;
  forces_recv = (double*) memory->srealloc(forces_recv, size_forces, "FixPIMD4:forces_recv");
  for(int i=0; i<np; i++)
  {
    forces[i] = (double*) memory->srealloc(forces[i], size_forces, "FixPIMD4:forces[i]");
  }
  
  // copy local positions and tags
  memcpy(forces[universe->iworld], &(atom->f[0][0]), size_forces);

  // traversing over all the other worlds
  for(int dworld=1; dworld<=np-1; dworld++)
  {
      // send the tags and forces to the process proc_send
      // receive the tags and forces from the process proc_recv
    int proc_send = (universe->me + dworld * comm->nprocs) % universe->nprocs; 
    int proc_recv = (universe->me - dworld * comm->nprocs + universe->nprocs) % universe->nprocs;
    int world_recv = (int)(proc_recv / comm->nprocs);
    
    // determine the number of atoms to be sent to and received from the other worlds
    MPI_Sendrecv(&(nlocal), 1, MPI_INT, proc_send, 0, 
                 &(nsend), 1, MPI_INT, proc_recv, 0, 
                 universe->uworld, MPI_STATUS_IGNORE);
    nrecv = nlocal;

    size_forces = sizeof(double) * nsend * 3;
    size_tags = sizeof(tagint) * nsend;

    forces_send = (double*) memory->srealloc(forces_send, size_forces, "FixPIMD4:forces_send");
    tags_send = (tagint*) memory->srealloc(tags_send, size_tags, "FixPIMD4:tags_send");

    MPI_Sendrecv(atom->tag, nlocal, MPI_LMP_TAGINT, proc_send, 0,
                 tags_send, nsend, MPI_LMP_TAGINT, proc_recv, 0,
                 universe->uworld, MPI_STATUS_IGNORE);

    // wrap positions
    double *wrap_ptr = forces_send;
    int ncpy = sizeof(double)*3;

    for(int i=0; i<nsend; i++)
    {
      int index = atom->map(tags_send[i]);
      if(index < 0)
      {
        char error_line[256];

        sprintf(error_line, "Atom " TAGINT_FORMAT " is missing at world [%d] "
                "rank [%d] required by  rank [%d] (" TAGINT_FORMAT ", "
                TAGINT_FORMAT ", " TAGINT_FORMAT ").\n", tags_send[i],
                universe->iworld, comm->me, proc_recv,
                atom->tag[0], atom->tag[1], atom->tag[2]);

        error->universe_one(FLERR,error_line);
      }    
      memcpy(wrap_ptr, atom->f[index], ncpy);
      wrap_ptr += 3;
    }
    MPI_Sendrecv(forces_send, nsend*3, MPI_DOUBLE, proc_recv, 0,
                forces_recv, nrecv*3, MPI_DOUBLE, proc_send, 0,
                universe->uworld, MPI_STATUS_IGNORE);

    memcpy(forces[world_recv], forces_recv, sizeof(double)*nlocal*3);          
  }
}

/* ---------------------------------------------------------------------- */

void FixPIMD4::compute_xc()
{
  int nlocal = atom->nlocal;
  xc = (double*) memory->srealloc(xc, sizeof(double) * nlocal * 3, "FixPIMD4:xc");
  for(int i=0; i<nlocal; i++)
  {
    xc[3*i] = xc[3*i+1] = xc[3*i+2] = 0.0;
    for(int j=0; j<np; j++)
    {
      xc[3*i] += coords[j][3*i];
      xc[3*i+1] += coords[j][3*i+1];
      xc[3*i+2] += coords[j][3*i+2];
    }
    xc[3*i] /= np;
    xc[3*i+1] /= np;
    xc[3*i+2] /= np;
  } 
}

void FixPIMD4::compute_fc()
{
  int nlocal = atom->nlocal;
  fc = (double*) memory->srealloc(fc, sizeof(double) * nlocal * 3, "FixPIMD4:fc");
  for(int i=0; i<nlocal; i++)
  {
    fc[3*i] = fc[3*i+1] = fc[3*i+2] = 0.0;
    for(int j=0; j<np; j++)
    {
      fc[3*i] += forces[j][3*i];
      fc[3*i+1] += forces[j][3*i+1];
      fc[3*i+2] += forces[j][3*i+2];
    }
    // fc[3*i] /= np;
    // fc[3*i+1] /= np;
    // fc[3*i+2] /= np;
  } 
}

void FixPIMD4::compute_vir()
{
  int nlocal = atom->nlocal;  
  xf = vir = xcfc = centroid_vir = 0.0;
  for(int i=0; i<nlocal; i++)
  {
    for(int j=0; j<3; j++)
    {
      xf += atom->x[i][j] * atom->f[i][j];
      xcfc += xc[3*i+j] * fc[3*i+j];
    }
  }

  MPI_Allreduce(&xf,&vir,1,MPI_DOUBLE,MPI_SUM,universe->uworld);
  MPI_Allreduce(&xcfc, &centroid_vir, 1, MPI_DOUBLE, MPI_SUM, world);
}
/* ---------------------------------------------------------------------- */

void FixPIMD4::compute_xscaled()
{
  int nlocal = atom->nlocal;
  for(int i=0; i<np; i++)
  {
    x_scaled[i] = (double*) memory->srealloc(x_scaled[i], sizeof(double) * nlocal * 3, "FixPIMD4:x_scaled[i]");
  }
  for(int i=0; i<np; i++)
  {
    for(int j=0; j<nlocal; j++)
    {
    x_scaled[i][3*j] = lambda * coords[i][3*j] + (1.0 - lambda) * xc[3*j];
    x_scaled[i][3*j+1] = lambda * coords[i][3*j+1] + (1.0 - lambda) * xc[3*j+1];
    x_scaled[i][3*j+2] = lambda * coords[i][3*j+2] + (1.0 - lambda) * xc[3*j+2];
    }
  }
}

/* ---------------------------------------------------------------------- */
/* ----------------------------------------------------------------------
   Compute centroid-virial kinetic energy estimator
------------------------------------------------------------------------- */

void FixPIMD4::compute_t_vir()
{
  t_vir = -0.5 / np * vir;
  t_cv = 1.5 * atom->natoms * force->boltz * temp - 0.5 / np * (vir - centroid_vir);
}

/* ----------------------------------------------------------------------
   Compute primitive kinetic energy estimator
------------------------------------------------------------------------- */

void FixPIMD4::compute_t_prim()
{
  // fprintf(stdout, "in compute_t_prim, me = %d, N = %d, np = %d, force->boltz = %2.8f, temp = %2.8f, total_spring_energy = %2.8e.\n", universe->me, atom->natoms, np, force->boltz, temp, total_spring_energy);
  t_prim = 1.5 * atom->natoms * np * force->boltz * temp - total_spring_energy;
}

void FixPIMD4::compute_p_prim()
{
  p_prim = atom->natoms * np * force->boltz * temp * inv_volume - 1.0 / 1.5 * inv_volume * total_spring_energy + 1.0 / 3 / np * inv_volume * vir;
}

void FixPIMD4::compute_p_cv()
{
  p_cv = 3 * atom->natoms * force->boltz * temp * inv_volume + 1.0 / np * inv_volume * centroid_vir; 
}

/* ---------------------------------------------------------------------- */

void FixPIMD4::compute_totke()
{
  double kine = 0.0;
  totke = 0.0;
  int nlocal = atom->nlocal;
  int *type = atom->type;
  double *_mass = atom->mass;
  for(int i=0; i<nlocal; i++)
  {
    for(int j=0; j<3; j++)
    {
      kine += 0.5 * _mass[type[i]] * atom->v[i][j] * atom->v[i][j];
    }
  }
  // if(universe->iworld==0) printf("kine = %.6e.\n", kine);
  //printf("iworld = %d, m = %.6e, _m = %.6e, kine = %.6e.\n", universe->iworld, mass[type[0]], atom->mass[type[0]], kine*force->mvv2e);
  MPI_Allreduce(&kine, &totke, 1, MPI_DOUBLE, MPI_SUM, universe->uworld);
  // printf("iworld = %d, totke = %.6e.\n", universe->iworld, totke);
  // if(universe->iworld==0) printf("mvv2e = %.6e.\n", force->mvv2e);
  totke *= force->mvv2e / np;
}

/* ---------------------------------------------------------------------- */

void FixPIMD4::compute_pote()
{
  double pot_energy_partition = 0.0;
  pote = 0.0;
  // printf("ntimestep = %d.\n", update->ntimestep); 
  // printf("update->eflag_global = %d, c_pe->invoked_scalar = %d.\n", update->eflag_global, c_pe->invoked_scalar);
  // printf("update->eflag_global != invoked_scalar = %d.\n", update->eflag_global != c_pe->invoked_scalar);
  c_pe->compute_scalar();
  pot_energy_partition = c_pe->scalar;
  pot_energy_partition /= np;
  MPI_Allreduce(&pot_energy_partition, &pote, 1, MPI_DOUBLE, MPI_SUM, universe->uworld);
}

/* ---------------------------------------------------------------------- */

void FixPIMD4::compute_spring_energy()
{
  spring_energy = 0.0;

  double **x = atom->x;
  double* _mass = atom->mass;
  int* type = atom->type;
  int nlocal = atom->nlocal;

  double* xlast = buf_beads[x_last];
  double* xnext = buf_beads[x_next];

  for(int i=0; i<nlocal; i++)
  {
    double delx1 = xlast[0] - x[i][0];
    double dely1 = xlast[1] - x[i][1];
    double delz1 = xlast[2] - x[i][2];
    xlast += 3;
    domain->minimum_image(delx1, dely1, delz1);

    double delx2 = xnext[0] - x[i][0];
    double dely2 = xnext[1] - x[i][1];
    double delz2 = xnext[2] - x[i][2];
    xnext += 3;
    domain->minimum_image(delx2, dely2, delz2);

    double ff = fbond * _mass[type[i]];

    double dx = delx1+delx2;
    double dy = dely1+dely2;
    double dz = delz1+delz2;

    spring_energy += -ff * (delx1*delx1+dely1*dely1+delz1*delz1+delx2*delx2+dely2*dely2+delz2*delz2);
  }
  MPI_Allreduce(&spring_energy, &total_spring_energy, 1, MPI_DOUBLE, MPI_SUM, universe->uworld);
  total_spring_energy *= 0.25;
  total_spring_energy /= np;
}

/* ---------------------------------------------------------------------- */

void FixPIMD4::compute_tote()
{
  // tote = totke + hope;
  tote = totke + pote + total_spring_energy;
}

/* ---------------------------------------------------------------------- */

double FixPIMD4::compute_vector(int n)
{
  if(n==0) { return totke; }
  if(n==1) { return total_spring_energy; }
  if(n==2) { return pote; }
  if(n==3) { return tote; }
  if(n==4) { return t_prim; }
  if(n==5) { return t_vir; }
  if(n==6) { return t_cv; }
  if(n==7) { return p_prim; }
  if(n==8) { return p_cv; }
  return 0.0;
}
