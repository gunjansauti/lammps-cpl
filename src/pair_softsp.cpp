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

#include "math.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "pair_softsp.h"
#include "atom.h"
#include "comm.h"
#include "force.h"
#include "neigh_list.h"
#include "math_const.h"
#include "memory.h"
#include "error.h"

using namespace LAMMPS_NS;
using namespace MathConst;

/* ---------------------------------------------------------------------- */

Pairsoftsp::Pairsoftsp(LAMMPS *lmp) : Pair(lmp)
{
  writedata = 1;
}

/* ---------------------------------------------------------------------- */

Pairsoftsp::~Pairsoftsp()
{
  if (allocated) {
    memory->destroy(setflag);
    memory->destroy(cutsq);

    memory->destroy(cut);
    memory->destroy(a);
    memory->destroy(rho);
    memory->destroy(c);
    memory->destroy(rhoinv);
    memory->destroy(buck1);
    memory->destroy(buck2);
    memory->destroy(offset);
  }
}

/* ---------------------------------------------------------------------- */

void Pairsoftsp::compute(int eflag, int vflag)
{
  int i,j,ii,jj,inum,jnum,itype,jtype;
  double xtmp,ytmp,ztmp,delx,dely,delz,evdwl,fpair;
  double rsq,r2inv,r6inv,forcebuck,factor_lj;
  double r,rexp;
  int *ilist,*jlist,*numneigh,**firstneigh;

  evdwl = 0.0;
  if (eflag || vflag) ev_setup(eflag,vflag);
  else evflag = vflag_fdotr = 0;

  double **x = atom->x;
  double **f = atom->f;
  int *type = atom->type;
  int nlocal = atom->nlocal;
  double *special_lj = force->special_lj;
  int newton_pair = force->newton_pair;

  double ratio,rn;

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

    for (jj = 0; jj < jnum; jj++) {
      j = jlist[jj];
      factor_lj = special_lj[sbmask(j)];
      j &= NEIGHMASK;

      delx = xtmp - x[j][0];
      dely = ytmp - x[j][1];
      delz = ztmp - x[j][2];
      rsq = delx*delx + dely*dely + delz*delz;
      jtype = type[j];

      if (rsq < cutsq[itype][jtype]) {
        r2inv = 1.0/rsq;
        r6inv = r2inv*r2inv*r2inv;
        r = rhoinv[itype][jtype]/sqrt(rsq);
        //rexp = exp(-r*rhoinv[itype][jtype]);
        //forcebuck = buck1[itype][jtype]*r*rexp - buck2[itype][jtype]*r6inv;
        //fpair = factor_lj*forcebuck*r2inv;

        //ratio=rho[itype][jtype]/r;
        //rn=pow(ratio,c[itype][jtype]);
        //forcebuck=a[itype][jtype]*ratio*c[itype][jtype];

        forcebuck=buck1[itype][jtype]*c[itype][jtype]*pow(r,c[itype][jtype]);
        fpair = factor_lj*forcebuck*r2inv;


        f[i][0] += delx*fpair;
        f[i][1] += dely*fpair;
        f[i][2] += delz*fpair;
        if (newton_pair || j < nlocal) {
          f[j][0] -= delx*fpair;
          f[j][1] -= dely*fpair;
          f[j][2] -= delz*fpair;
        }

        if (eflag) {
//          evdwl = a[itype][jtype]*rexp - c[itype][jtype]*r6inv -
//            offset[itype][jtype];
//          evdwl *= factor_lj;

         //evdwl=a[itype][jtype]*rn-offset[itype][jtype];
          evdwl=buck1[itype][jtype]*pow(r,c[itype][jtype]);
          evdwl *= factor_lj;

        }

        if (evflag) ev_tally(i,j,nlocal,newton_pair,
                             evdwl,0.0,fpair,delx,dely,delz);
      }
    }
  }

  if (vflag_fdotr) virial_fdotr_compute();
}

/* ----------------------------------------------------------------------
   allocate all arrays
------------------------------------------------------------------------- */

void Pairsoftsp::allocate()
{
  allocated = 1;
  int n = atom->ntypes;

  memory->create(setflag,n+1,n+1,"pair:setflag");
  for (int i = 1; i <= n; i++)
    for (int j = i; j <= n; j++)
      setflag[i][j] = 0;

  memory->create(cutsq,n+1,n+1,"pair:cutsq");

  memory->create(cut,n+1,n+1,"pair:cut_lj");
  memory->create(a,n+1,n+1,"pair:a");
  memory->create(rho,n+1,n+1,"pair:rho");
  memory->create(c,n+1,n+1,"pair:c");
  memory->create(rhoinv,n+1,n+1,"pair:rhoinv");
  memory->create(buck1,n+1,n+1,"pair:buck1");
  memory->create(buck2,n+1,n+1,"pair:buck2");
  memory->create(offset,n+1,n+1,"pair:offset");
}

/* ----------------------------------------------------------------------
   global settings
------------------------------------------------------------------------- */

void Pairsoftsp::settings(int narg, char **arg)
{
  if (narg != 1) error->all(FLERR,"Illegal pair_style command");

  cut_global = force->numeric(FLERR,arg[0]);

  // reset cutoffs that have been explicitly set

  if (allocated) {
    int i,j;
    for (i = 1; i <= atom->ntypes; i++)
      for (j = i+1; j <= atom->ntypes; j++)
        if (setflag[i][j]) cut[i][j] = cut_global;
  }
}

/* ----------------------------------------------------------------------
   set coeffs for one or more type pairs
------------------------------------------------------------------------- */

void Pairsoftsp::coeff(int narg, char **arg)
{
  if (narg < 5 || narg > 6) error->all(FLERR,"Incorrect args for pair coefficients");
  if (!allocated) allocate();

  int ilo,ihi,jlo,jhi;
  force->bounds(FLERR,arg[0],atom->ntypes,ilo,ihi);
  force->bounds(FLERR,arg[1],atom->ntypes,jlo,jhi);

  double a_one = force->numeric(FLERR,arg[2]);
  double rho_one = force->numeric(FLERR,arg[3]);
  if (rho_one <= 0) error->all(FLERR,"Incorrect args for pair coefficients");
  double c_one = force->numeric(FLERR,arg[4]);

  double cut_one = cut_global;
  if (narg == 6) cut_one = force->numeric(FLERR,arg[5]);

  int count = 0;
  for (int i = ilo; i <= ihi; i++) {
    for (int j = MAX(jlo,i); j <= jhi; j++) {
      a[i][j] = a_one;
      rho[i][j] = rho_one;
      c[i][j] = c_one;
      cut[i][j] = cut_one;
      setflag[i][j] = 1;
      count++;
    }
  }

  if (count == 0) error->all(FLERR,"Incorrect args for pair coefficients");
}

/* ----------------------------------------------------------------------
   init for one type pair i,j and corresponding j,i
------------------------------------------------------------------------- */

double Pairsoftsp::init_one(int i, int j)
{
  if (setflag[i][j] == 0) error->all(FLERR,"All pair coeffs are not set");

//  rhoinv[i][j] = 1.0/rho[i][j];
//  buck1[i][j] = a[i][j]/rho[i][j];
//  buck2[i][j] = 6.0*c[i][j];

    //rhoinv[i][j] = pow(rho[i][j],c[i][j]);
    rhoinv[i][j]=rho[i][j];
    buck1[i][j] = a[i][j];
    buck2[i][j] = c[i][j];

  if (offset_flag) {
//    double rexp = exp(-cut[i][j]/rho[i][j]);
//    offset[i][j] = a[i][j]*rexp - c[i][j]/pow(cut[i][j],6.0);
      double rexp=rho[i][j]/cut[i][j];    
      offset[i][j]=a[i][j]*pow(rexp,c[i][j]);
  } else offset[i][j] = 0.0;

  a[j][i] = a[i][j];
  c[j][i] = c[i][j];
  rhoinv[j][i] = rhoinv[i][j];
  buck1[j][i] = buck1[i][j];
  buck2[j][i] = buck2[i][j];
  offset[j][i] = offset[i][j];

  // compute I,J contribution to long-range tail correction
  // count total # of atoms of type I and J via Allreduce

  if (tail_flag) {
    int *type = atom->type;
    int nlocal = atom->nlocal;

    double count[2],all[2];
    count[0] = count[1] = 0.0;
    for (int k = 0; k < nlocal; k++) {
      if (type[k] == i) count[0] += 1.0;
      if (type[k] == j) count[1] += 1.0;
    }
    MPI_Allreduce(count,all,2,MPI_DOUBLE,MPI_SUM,world);

    double rho1 = rho[i][j];
    double rho2 = rho1*rho1;
    double rho3 = rho2*rho1;
    double rc = cut[i][j];
    double rc2 = rc*rc;
    double rc3 = rc2*rc;
    double cutn;
//    etail_ij = 2.0*MY_PI*all[0]*all[1]*
//      (a[i][j]*exp(-rc/rho1)*rho1*(rc2 + 2.0*rho1*rc + 2.0*rho2) -
//       c[i][j]/(3.0*rc3));
//    ptail_ij = (-1/3.0)*2.0*MY_PI*all[0]*all[1]*
//      (-a[i][j]*exp(-rc/rho1)*
//       (rc3 + 3.0*rho1*rc2 + 6.0*rho2*rc + 6.0*rho3) + 2.0*c[i][j]/rc3);
     cutn=rho[i][j]/rc;
     etail_ij=2.0*MY_PI*all[0]*all[1]*a[i][j]*pow(cutn,c[i][j])*rc*rc*rc*a[i][j]/(3.0-c[i][j]);
     ptail_ij=etail_ij*c[i][j];

  }

  return cut[i][j];
}

/* ----------------------------------------------------------------------
  proc 0 writes to restart file
------------------------------------------------------------------------- */

void Pairsoftsp::write_restart(FILE *fp)
{
  write_restart_settings(fp);

  int i,j;
  for (i = 1; i <= atom->ntypes; i++)
    for (j = i; j <= atom->ntypes; j++) {
      fwrite(&setflag[i][j],sizeof(int),1,fp);
      if (setflag[i][j]) {
        fwrite(&a[i][j],sizeof(double),1,fp);
        fwrite(&rho[i][j],sizeof(double),1,fp);
        fwrite(&c[i][j],sizeof(double),1,fp);
        fwrite(&cut[i][j],sizeof(double),1,fp);
      }
    }
}

/* ----------------------------------------------------------------------
  proc 0 reads from restart file, bcasts
------------------------------------------------------------------------- */

void Pairsoftsp::read_restart(FILE *fp)
{
  read_restart_settings(fp);

  allocate();

  int i,j;
  int me = comm->me;
  for (i = 1; i <= atom->ntypes; i++)
    for (j = i; j <= atom->ntypes; j++) {
      if (me == 0) fread(&setflag[i][j],sizeof(int),1,fp);
      MPI_Bcast(&setflag[i][j],1,MPI_INT,0,world);
      if (setflag[i][j]) {
        if (me == 0) {
          fread(&a[i][j],sizeof(double),1,fp);
          fread(&rho[i][j],sizeof(double),1,fp);
          fread(&c[i][j],sizeof(double),1,fp);
          fread(&cut[i][j],sizeof(double),1,fp);
        }
        MPI_Bcast(&a[i][j],1,MPI_DOUBLE,0,world);
        MPI_Bcast(&rho[i][j],1,MPI_DOUBLE,0,world);
        MPI_Bcast(&c[i][j],1,MPI_DOUBLE,0,world);
        MPI_Bcast(&cut[i][j],1,MPI_DOUBLE,0,world);
      }
    }
}

/* ----------------------------------------------------------------------
  proc 0 writes to restart file
------------------------------------------------------------------------- */

void Pairsoftsp::write_restart_settings(FILE *fp)
{
  fwrite(&cut_global,sizeof(double),1,fp);
  fwrite(&offset_flag,sizeof(int),1,fp);
  fwrite(&mix_flag,sizeof(int),1,fp);
  fwrite(&tail_flag,sizeof(int),1,fp);
}

/* ----------------------------------------------------------------------
  proc 0 reads from restart file, bcasts
------------------------------------------------------------------------- */

void Pairsoftsp::read_restart_settings(FILE *fp)
{
  if (comm->me == 0) {
    fread(&cut_global,sizeof(double),1,fp);
    fread(&offset_flag,sizeof(int),1,fp);
    fread(&mix_flag,sizeof(int),1,fp);
    fread(&tail_flag,sizeof(int),1,fp);
  }
  MPI_Bcast(&cut_global,1,MPI_DOUBLE,0,world);
  MPI_Bcast(&offset_flag,1,MPI_INT,0,world);
  MPI_Bcast(&mix_flag,1,MPI_INT,0,world);
  MPI_Bcast(&tail_flag,1,MPI_INT,0,world);
}

/* ----------------------------------------------------------------------
   proc 0 writes to data file
------------------------------------------------------------------------- */

void Pairsoftsp::write_data(FILE *fp)
{
  for (int i = 1; i <= atom->ntypes; i++)
    fprintf(fp,"%d %g %g %g\n",i,a[i][i],rho[i][i],c[i][i]);
}

/* ----------------------------------------------------------------------
   proc 0 writes all pairs to data file
------------------------------------------------------------------------- */

void Pairsoftsp::write_data_all(FILE *fp)
{
  for (int i = 1; i <= atom->ntypes; i++)
    for (int j = i; j <= atom->ntypes; j++)
      fprintf(fp,"%d %d %g %g %g %g\n",i,j,
              a[i][j],rho[i][j],c[i][j],cut[i][j]);
}

/* ---------------------------------------------------------------------- */

double Pairsoftsp::single(int i, int j, int itype, int jtype,
                        double rsq, double factor_coul, double factor_lj,
                        double &fforce)
{
  double r2inv,r6inv,r,rexp,forcebuck,phibuck;
  double ratio,rn;

//  r2inv = 1.0/rsq;
//  r6inv = r2inv*r2inv*r2inv;
//  r = sqrt(rsq);
//  rexp = exp(-r*rhoinv[itype][jtype]);
//  forcebuck = buck1[itype][jtype]*r*rexp - buck2[itype][jtype]*r6inv;

    r=sqrt(rsq);
    ratio=rho[itype][jtype]/r;
    rn=pow(ratio,c[itype][jtype]);
    forcebuck=a[itype][jtype]*c[itype][jtype]*rn/r;
    fforce = factor_lj*forcebuck*r2inv;

//  phibuck = a[itype][jtype]*rexp - c[itype][jtype]*r6inv - offset[itype][jtype];

    phibuck=a[itype][jtype]*rn - offset[itype][jtype];

  return factor_lj*phibuck;
}

/* ---------------------------------------------------------------------- */

void *Pairsoftsp::extract(const char *str, int &dim)
{
  dim = 2;
  if (strcmp(str,"a") == 0) return (void *) a;
  if (strcmp(str,"c") == 0) return (void *) c;
  return NULL;
}
