/* -*- c++ -*- ----------------------------------------------------------
   LAMMPS - Large-scale Atomic/Molecular Massively Parallel Simulator
   http://lammps.sandia.gov, Sandia National Laboratories
   Steve Plimpton, sjplimp@sandia.gov

   Copyright (2003) Sandia Corporation.  Under the terms of Contract
   DE-AC04-94AL85000 with Sandia Corporation, the U.S. Government retains
   certain rights in this software.  This software is distributed under
   the GNU General Public License.

   See the README file in the top-level LAMMPS directory.
------------------------------------------------------------------------- */

#ifdef PAIR_CLASS

PairStyle(cac/eam,PairCACEAM)

#else

#ifndef LMP_PAIR_EAM_CAC_H
#define LMP_PAIR_EAM_CAC_H

#include "pair_CAC.h"

namespace LAMMPS_NS {

class PairCACEAM : public PairCAC {
 public:
	 friend class FixSemiGrandCanonicalMC;   // Alex Stukowski option
  // potentials as array data

  int nrho, nr;
  int nfrho, nrhor, nz2r;
  double **frho, **rhor, **z2r;
  int *type2frho, **type2rhor, **type2z2r;

	// potentials in spline form used for force computation

  double dr, rdr, drho, rdrho, rhomax;
  double ***rhor_spline, ***frho_spline, ***z2r_spline;

  PairCACEAM(class LAMMPS *);
  virtual ~PairCACEAM();
  
  virtual void coeff(int, char **);
  virtual void init_style();
  virtual double init_one(int, int);
  
  virtual void *extract(const char *, int &);
  void swap_eam(double *, double **);

  struct Param {
    double epsilon, sigma;
    double littlea, lambda, gamma, costheta;
    double biga, bigb;
    double powerp, powerq;
    double tol;
    double cut, cutsq;
    double sigma_gamma, lambda_epsilon, lambda_epsilon2;
    double c1, c2, c3, c4, c5, c6;
    int ielement, jelement, kelement;
	};

 protected:
  double cutforcesq;
  double **scale;

	// per-atom arrays
  double *rho, *fp;
  double **inner_neighbor_coords;
  double **outer_neighbor_coords;
  int *inner_neighbor_types;
  int *outer_neighbor_types;
  double density;
 
  virtual void allocate();
  virtual void read_file(char *);
  virtual void array2spline();
  virtual void file2array();

  //EAM variables and structures

  // potentials as file data

  int *map;                   // which element each atom type maps to

  struct Funcfl {
	  char *file;
	  int nrho, nr;
	  double drho, dr, cut, mass;
	  double *frho, *rhor, *zr;
  };
  Funcfl *funcfl;
  int nfuncfl;

  struct Setfl {
	  char **elements;
	  int nelements, nrho, nr;
	  double drho, dr, cut;
	  double *mass;
	  double **frho, **rhor, ***z2r;
  };
  Setfl *setfl;

  struct Fs {
    char **elements;
    int nelements, nrho, nr;
    double drho, dr, cut;
    double *mass;
    double **frho, ***rhor, ***z2r;
  };
  Fs *fs;

  void interpolate(int, double, double *, double **);
  void grab(FILE *, int, double *);

  //further CAC functions 
  void force_densities(int, double, double, double, double, double
    &fx, double &fy, double &fz);

};

}

#endif
#endif
