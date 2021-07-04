// clang-format off
/* ----------------------------------------------------------------------
   LAMMPS - Large-scale Atomic/Molecular Massively Parallel Simulator
   https://www.lammps.org/, Sandia National Laboratories
   Steve Plimpton, sjplimp@sandia.gov

   Copyright (2003) Sandia Corporation.  Under the terms of Contract
   DE-AC04-94AL85000 with Sandia Corporation, the U.S. Government retains
   certain rights in this software.  This software is distributed under
   the GNU General Public License.

   See the README file in the top-level LAMMPS directory.
------------------------------------------------------------------------- */

/* ----------------------------------------------------------------------
   Contributing author: Tim Bernhard (ETHZ)
------------------------------------------------------------------------- */

#include "angle_fourier_simple_approx.h"
#include <cmath>
#include "atom.h"
#include "comm.h"
#include "force.h"
#include "neighbor.h"


#include "suffix.h"
using namespace LAMMPS_NS;

#define SMALL 0.001
#define TWO_PI 2.*M_PI

/* ---------------------------------------------------------------------- */

AngleFourierSimpleApprox::AngleFourierSimpleApprox(class LAMMPS *lmp)
  : AngleFourierSimple(lmp)
{
  suffix_flag |= Suffix::OMP;
}

/* ---------------------------------------------------------------------- */

/**
 * Approximation of cos
 * maximum error is about 0.00109 for the range -pi to pi
 * Source: https://stackoverflow.com/a/28050328/3909202
 */
double AngleFourierSimpleApprox::fastCos(double x) {
  constexpr double tp = 1./(TWO_PI);
  // TODO: check if range map is necessary
  double x_wrapped = x - TWO_PI * floor(x * tp);
    x_wrapped *= tp;
    x_wrapped -= .25 + std::floor(x_wrapped + .25);
    x_wrapped *= 16. * (std::abs(x_wrapped) - .5);
    x_wrapped += .225 * x_wrapped * (std::abs(x_wrapped) - 1.);
    return x_wrapped;
}

/* ---------------------------------------------------------------------- */

/**
 * Approximation of acos 
 * Returns the arccosine of x in the range [0,pi], expecting x to be in the range [-1,+1]. 
 * Absolute error <= 6.7e-5
 * Range mapping not necessary as C++ standard library would throw domain error
 * Source: https://developer.download.nvidia.com/cg/acos.html
 */
double AngleFourierSimpleApprox::fastAcos(double x) {
  double negate = double(x < 0);
  x = abs(x);
  double ret = -0.0187293;
  ret = ret * x;
  // TODO: verify that fpas are used here
  ret = ret + 0.0742610;
  ret = ret * x;
  ret = ret - 0.2121144;
  ret = ret * x;
  ret = ret + 1.5707288;
  ret = ret * sqrt(1.0-x); 
  ret = ret - 2 * negate * ret;
  return negate * 3.14159265358979 + ret;
}

/* ---------------------------------------------------------------------- */

void AngleFourierSimpleApprox::compute(int eflag, int vflag)
{
  ev_init(eflag,vflag);

  const int nall = atom->nlocal + atom->nghost;
  const int inum = neighbor->nanglelist;

      if (evflag) {
        if (eflag) {
          if (force->newton_bond) eval<1,1,1>();
          else eval<1,1,0>();
        } else {
          if (force->newton_bond) eval<1,0,1>();
          else eval<1,0,0>();
        }
      } else {
        if (force->newton_bond) eval<0,0,1>();
        else eval<0,0,0>();
      }
}

template <int EVFLAG, int EFLAG, int NEWTON_BOND>
void AngleFourierSimpleApprox::eval()
{
  int i1,i2,i3,n,type;
  double delx1,dely1,delz1,delx2,dely2,delz2;
  double eangle,f1[3],f3[3];
  double term,sgn;
  double rsq1,rsq2,r1,r2,c,cn,th,nth,a,a11,a12,a22;
  int nanglelist = neighbor->nanglelist;

  const dbl3_t * _noalias const x = (dbl3_t *) atom->x[0];
  dbl3_t * _noalias const f = (dbl3_t *) thr->get_f()[0];
  const int4_t * _noalias const anglelist = (int4_t *) neighbor->anglelist[0];
  const int nlocal = atom->nlocal;
  eangle = 0.0;

  for (n = 0; n < nanglelist; n++) {
    i1 = anglelist[n].a;
    i2 = anglelist[n].b;
    i3 = anglelist[n].c;
    type = anglelist[n].t;

    // 1st bond

    delx1 = x[i1].x - x[i2].x;
    dely1 = x[i1].y - x[i2].y;
    delz1 = x[i1].z - x[i2].z;

    rsq1 = delx1*delx1 + dely1*dely1 + delz1*delz1;
    r1 = sqrt(rsq1);

    // 2nd bond

    delx2 = x[i3].x - x[i2].x;
    dely2 = x[i3].y - x[i2].y;
    delz2 = x[i3].z - x[i2].z;

    rsq2 = delx2*delx2 + dely2*dely2 + delz2*delz2;
    r2 = sqrt(rsq2);

    // angle (cos and sin)

    c = delx1*delx2 + dely1*dely2 + delz1*delz2;
    c /= r1*r2;

    if (c > 1.0) c = 1.0;
    if (c < -1.0) c = -1.0;

    // force & energy

    th = fastAcos(c);
    nth = N[type]*fastAcos(c);
    cn = cos(nth);
    term = k[type]*(1.0+C[type]*cn);

    if (EFLAG) eangle = term;

    // handle sin(n th)/sin(th) singulatiries

    if (fabs(c)-1.0 > 0.0001) {
      a = k[type]*C[type]*N[type]*sin(nth)/sin(th);
    } else {
      if (c >= 0.0) {
        term = 1.0 - c;
        sgn = 1.0;
      } else {
        term = 1.0 + c;
        sgn = ( fmodf((float)(N[type]),2.0) == 0.0f )?-1.0:1.0;
      }
      a = N[type]+N[type]*(1.0-N[type]*N[type])*term/3.0;
      a = k[type]*C[type]*N[type]*(sgn)*a;
    }

    a11 = a*c / rsq1;
    a12 = -a / (r1*r2);
    a22 = a*c / rsq2;

    f1[0] = a11*delx1 + a12*delx2;
    f1[1] = a11*dely1 + a12*dely2;
    f1[2] = a11*delz1 + a12*delz2;
    f3[0] = a22*delx2 + a12*delx1;
    f3[1] = a22*dely2 + a12*dely1;
    f3[2] = a22*delz2 + a12*delz1;

    // apply force to each of 3 atoms

    if (NEWTON_BOND || i1 < nlocal) {
      f[i1].x += f1[0];
      f[i1].y += f1[1];
      f[i1].z += f1[2];
    }

    if (NEWTON_BOND || i2 < nlocal) {
      f[i2].x -= f1[0] + f3[0];
      f[i2].y -= f1[1] + f3[1];
      f[i2].z -= f1[2] + f3[2];
    }

    if (NEWTON_BOND || i3 < nlocal) {
      f[i3].x += f3[0];
      f[i3].y += f3[1];
      f[i3].z += f3[2];
    }

    if (EVFLAG) ev_tally(this,i1,i2,i3,nlocal,NEWTON_BOND,eangle,f1,f3,
                             delx1,dely1,delz1,delx2,dely2,delz2);
  }
}
