/* -*- c++ -*- ----------------------------------------------------------
   LAMMPS - Large-scale Atomic/Molecular Massively Parallel Simulator
   https://www.lammps.org/, Sandia National Laboratories
   Steve Plimpton, sjplimp@sandia.gov

   Copyright (2003) Sandia Corporation.  Under the terms of Contract
   DE-AC04-94AL85000 with Sandia Corporation, the U.S. Government retains
   certain rights in this software.  This software is distributed under
   the GNU General Public License.

   See the README file in the top-level LAMMPS directory.
------------------------------------------------------------------------- */

#include "contact_normal_models.h"
#include "contact.h"
#include "error.h"
#include "math_const.h"

using namespace LAMMPS_NS;
using namespace Contact;
using namespace MathConst;

#define PI27SQ 266.47931882941264802866    // 27*PI**2
#define THREEROOT3 5.19615242270663202362  // 3*sqrt(3)
#define SIXROOT6 14.69693845669906728801   // 6*sqrt(6)
#define INVROOT6 0.40824829046386307274    // 1/sqrt(6)
#define FOURTHIRDS (4.0/3.0)               // 4/3
#define ONETHIRD (1.0/3.0)                 // 1/3
#define THREEQUARTERS 0.75                 // 3/4

/* ----------------------------------------------------------------------
   Default normal model
------------------------------------------------------------------------- */

NormalModel::NormalModel(LAMMPS *lmp) : SubModel(lmp)
{
  material_properties = 0;
}

/* ---------------------------------------------------------------------- */

bool NormalModel::touch()
{
  bool touchflag = (contact->rsq < contact->radsum * contact->radsum);
  return touchflag;
}

/* ---------------------------------------------------------------------- */

double NormalModel::pulloff_distance(double radi, double radj)
{
  //called outside of compute(), do not assume correct geometry defined in contact
  return radi + radj;
}

/* ---------------------------------------------------------------------- */

double NormalModel::calculate_area()
{
  return sqrt(contact->dR);
}

/* ---------------------------------------------------------------------- */

void NormalModel::set_fncrit()
{
  Fncrit = fabs(contact->Fntot);
}

/* ----------------------------------------------------------------------
   No model
------------------------------------------------------------------------- */

NormalNone::NormalNone(LAMMPS *lmp) : NormalModel(lmp) {}

/* ---------------------------------------------------------------------- */

double NormalNone::calculate_forces()
{
  return 0.0;
}

/* ----------------------------------------------------------------------
   Hookean normal force
------------------------------------------------------------------------- */

NormalHooke::NormalHooke(LAMMPS *lmp) : NormalModel(lmp)
{
  num_coeffs = 2;
}

/* ---------------------------------------------------------------------- */

void NormalHooke::coeffs_to_local()
{
  k = coeffs[0];
  damp = coeffs[1];

  if (k < 0.0 || damp < 0.0) error->all(FLERR, "Illegal Hooke normal model");
}

/* ---------------------------------------------------------------------- */

double NormalHooke::calculate_forces()
{
  Fne = knfac * contact->delta;
  return Fne;
}

/* ---------------------------------------------------------------------- */

void NormalHooke::set_knfac()
{
  knfac = k;
}

/* ----------------------------------------------------------------------
   Hertzian normal force
------------------------------------------------------------------------- */

NormalHertz::NormalHertz(LAMMPS *lmp) : NormalModel(lmp)
{
  num_coeffs = 2;
}

/* ---------------------------------------------------------------------- */

void NormalHertz::coeffs_to_local()
{
  k = coeffs[0];
  damp = coeffs[1];

  if (k < 0.0 || damp < 0.0) error->all(FLERR, "Illegal Hertz normal model");
}

/* ---------------------------------------------------------------------- */

double NormalHertz::calculate_forces()
{
  Fne = knfac * contact->delta;
  return Fne;
}

/* ---------------------------------------------------------------------- */

void NormalHertz::set_knfac()
{
  knfac = k * contact->area;
}

/* ----------------------------------------------------------------------
   Hertzian normal force with material properties
------------------------------------------------------------------------- */

NormalHertzMaterial::NormalHertzMaterial(LAMMPS *lmp) : NormalHertz(lmp)
{
  material_properties = 1;
  num_coeffs = 3;
}

/* ---------------------------------------------------------------------- */

void NormalHertzMaterial::coeffs_to_local()
{
  Emod = coeffs[0];
  damp = coeffs[1];
  poiss = coeffs[2];
  k = FOURTHIRDS * mix_stiffnessE(Emod, Emod, poiss, poiss);

  if (Emod < 0.0 || damp < 0.0) error->all(FLERR, "Illegal Hertz material normal model");
}

/* ---------------------------------------------------------------------- */

void NormalHertzMaterial::mix_coeffs(double* icoeffs, double* jcoeffs)
{
  coeffs[0] = mix_stiffnessE(icoeffs[0], jcoeffs[0],icoeffs[2], jcoeffs[2]);
  coeffs[1] = mix_geom(icoeffs[1], jcoeffs[1]);
  coeffs[2] = mix_geom(icoeffs[2], jcoeffs[2]);
  coeffs_to_local();
}

/* ----------------------------------------------------------------------
   DMT normal force
------------------------------------------------------------------------- */

NormalDMT::NormalDMT(LAMMPS *lmp) : NormalModel(lmp)
{
  allow_limit_damping = 0;
  material_properties = 1;
  num_coeffs = 4;
}

/* ---------------------------------------------------------------------- */

void NormalDMT::coeffs_to_local()
{
  Emod = coeffs[0];
  damp = coeffs[1];
  poiss = coeffs[2];
  cohesion = coeffs[3];
  k = FOURTHIRDS * mix_stiffnessE(Emod, Emod, poiss, poiss);

  if (Emod < 0.0 || damp < 0.0) error->all(FLERR, "Illegal DMT normal model");
}

/* ---------------------------------------------------------------------- */

void NormalDMT::mix_coeffs(double* icoeffs, double* jcoeffs)
{
  coeffs[0] = mix_stiffnessE(icoeffs[0], jcoeffs[0],icoeffs[2], jcoeffs[2]);
  coeffs[1] = mix_geom(icoeffs[1], jcoeffs[1]);
  coeffs[2] = mix_geom(icoeffs[2], jcoeffs[2]);
  coeffs[3] = mix_geom(icoeffs[3], jcoeffs[3]);
  coeffs_to_local();
}

/* ---------------------------------------------------------------------- */

double NormalDMT::calculate_forces()
{
  Fne = knfac * contact->delta;
  F_pulloff = 4.0 * MathConst::MY_PI * cohesion * contact->Reff;
  Fne -= F_pulloff;
  return Fne;
}

/* ---------------------------------------------------------------------- */

void NormalDMT::set_knfac()
{
  knfac = k * contact->area;
}

/* ---------------------------------------------------------------------- */

void NormalDMT::set_fncrit()
{
  Fncrit = fabs(Fne + 2.0 * F_pulloff);
}

/* ----------------------------------------------------------------------
   JKR normal force
------------------------------------------------------------------------- */

NormalJKR::NormalJKR(LAMMPS *lmp) : NormalModel(lmp)
{
  allow_limit_damping = 0;
  material_properties = 1;
  beyond_contact = 1;
  num_coeffs = 4;
}

/* ---------------------------------------------------------------------- */

void NormalJKR::coeffs_to_local()
{
  Emod = coeffs[0];
  damp = coeffs[1];
  poiss = coeffs[2];
  cohesion = coeffs[3];
  k = FOURTHIRDS * Emod;
  Escaled = mix_stiffnessE(Emod, Emod, poiss, poiss);

  if (Emod < 0.0 || damp < 0.0) error->all(FLERR, "Illegal JKR normal model");
}

/* ---------------------------------------------------------------------- */

void NormalJKR::mix_coeffs(double* icoeffs, double* jcoeffs)
{
  coeffs[0] = mix_stiffnessE(icoeffs[0], jcoeffs[0],icoeffs[2], jcoeffs[2]);
  coeffs[1] = mix_geom(icoeffs[1], jcoeffs[1]);
  coeffs[2] = mix_geom(icoeffs[2], jcoeffs[2]);
  coeffs[3] = mix_geom(icoeffs[3], jcoeffs[3]);
  coeffs_to_local();
}

/* ---------------------------------------------------------------------- */

bool NormalJKR::touch()
{
  double area_at_pulloff, R2, delta_pulloff, dist_pulloff;
  bool touchflag;

  if (contact->touch) {
    R2 = contact->Reff * contact->Reff;
    area_at_pulloff = cbrt(9.0 * MY_PI * cohesion * R2 / (4.0 * Escaled));
    delta_pulloff = area_at_pulloff * area_at_pulloff / contact->Reff - 2.0 * sqrt(MY_PI * cohesion * area_at_pulloff /Escaled);
    dist_pulloff = contact->radsum - delta_pulloff;
    touchflag = contact->rsq < (dist_pulloff * dist_pulloff);
  } else {
    touchflag = contact->rsq < (contact->radsum * contact->radsum);
  }

  return touchflag;
}

/* ----------------------------------------------------------------------
  called outside of compute(), do not assume geometry defined in contact
------------------------------------------------------------------------- */

double NormalJKR::pulloff_distance(double radi, double radj)
{
  double area_at_pulloff, Reff_tmp;

  Reff_tmp = radi * radj / (radi + radj); // May not be defined
  if (Reff_tmp <= 0) return 0;

  area_at_pulloff = cbrt(9.0 * MY_PI * cohesion * Reff_tmp * Reff_tmp / (4.0 * Escaled));
  return area_at_pulloff * area_at_pulloff / Reff_tmp - 2.0 * sqrt(MY_PI * cohesion * area_at_pulloff / Escaled);
}

/* ---------------------------------------------------------------------- */

double NormalJKR::calculate_area()
{
  double R2, dR2, t0, t1, t2, t3, t4, t5, t6;
  double sqrt1, sqrt2, sqrt3;

  R2 = contact->Reff * contact->Reff;
  dR2 = contact->dR * contact->dR;
  t0 = cohesion * cohesion * R2 * R2 * Escaled;
  t1 = PI27SQ * t0;
  t2 = 8.0 * contact->dR * dR2 * Escaled * Escaled * Escaled;
  t3 = 4.0 * dR2 * Escaled;

  // in case sqrt(0) < 0 due to precision issues
  sqrt1 = MAX(0, t0 * (t1 + 2.0 * t2));
  t4 = cbrt(t1 + t2 + THREEROOT3 * MY_PI * sqrt(sqrt1));
  t5 = t3 / t4 + t4 / Escaled;
  sqrt2 = MAX(0, 2.0 * contact->dR + t5);
  t6 = sqrt(sqrt2);
  sqrt3 = MAX(0, 4.0 * contact->dR - t5 + SIXROOT6 * cohesion * MY_PI * R2 / (Escaled * t6));

  return INVROOT6 * (t6 + sqrt(sqrt3));
}

/* ---------------------------------------------------------------------- */

double NormalJKR::calculate_forces()
{
  double a2;

  a2 = contact->area * contact->area;
  Fne = Escaled * contact->area * a2 / contact->Reff - MY_2PI * a2 * sqrt(4.0 * cohesion * Escaled / (MY_PI * contact->area));
  F_pulloff = 3.0 * MY_PI * cohesion * contact->Reff;

  return Fne;
}

/* ---------------------------------------------------------------------- */

void NormalJKR::set_knfac()
{
  knfac = Escaled * contact->area;
}

/* ---------------------------------------------------------------------- */

void NormalJKR::set_fncrit()
{
  Fncrit = fabs(Fne + 2.0 * F_pulloff);
}
