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

#include "npair_multi.h"
#include "atom.h"
#include "atom_vec.h"
#include "domain.h"
#include "error.h"
#include "molecule.h"
#include "my_page.h"
#include "neighbor.h"
#include "neigh_list.h"

using namespace LAMMPS_NS;
using namespace NeighConst;

/* ---------------------------------------------------------------------- */

template<int HALF, int NEWTON, int TRI, int SIZE>
NPairMulti<HALF, NEWTON, TRI, SIZE>::NPairMulti(LAMMPS *lmp) : NPair(lmp) {}

/* ----------------------------------------------------------------------
   multi stencil is icollection-jcollection dependent
   Full:
     binned neighbor list construction for all neighbors
     every neighbor pair appears in list of both atoms i and j
   Half + Newtoff:
     binned neighbor list construction with partial Newton's 3rd law
     each owned atom i checks own bin and other bins in stencil
     pair stored once if i,j are both owned and i < j
     pair stored by me if j is ghost (also stored by proc owning j)
   Half + Newton:
     binned neighbor list construction with full Newton's 3rd law
     each owned atom i checks its own bin and other bins in Newton stencil
     every pair stored exactly once by some processor
------------------------------------------------------------------------- */

template<int HALF, int NEWTON, int TRI, int SIZE>
void NPairMulti<HALF, NEWTON, TRI, SIZE>::build(NeighList *list)
{
  int i,j,jh,js,k,n,itype,jtype,icollection,jcollection,ibin,jbin,which,ns,imol,iatom,moltemplate;
  tagint tagprev;
  double xtmp,ytmp,ztmp,delx,dely,delz,rsq;
  double radsum,cut,cutsq;
  int *neighptr,*s;

  int *collection = neighbor->collection;
  double **x = atom->x;
  double *radius = atom->radius;
  int *type = atom->type;
  int *mask = atom->mask;
  tagint *tag = atom->tag;
  tagint *molecule = atom->molecule;
  tagint **special = atom->special;
  int **nspecial = atom->nspecial;
  int nlocal = atom->nlocal;
  if (includegroup) nlocal = atom->nfirst;

  int *molindex = atom->molindex;
  int *molatom = atom->molatom;
  Molecule **onemols = atom->avec->onemols;
  if (molecular == Atom::TEMPLATE) moltemplate = 1;
  else moltemplate = 0;

  int history = list->history;
  int mask_history = 1 << HISTBITS;

  int *ilist = list->ilist;
  int *numneigh = list->numneigh;
  int **firstneigh = list->firstneigh;
  MyPage<int> *ipage = list->ipage;

  int inum = 0;
  ipage->reset();

  for (i = 0; i < nlocal; i++) {
    n = 0;
    neighptr = ipage->vget();
    itype = type[i];
    icollection = collection[i];
    xtmp = x[i][0];
    ytmp = x[i][1];
    ztmp = x[i][2];
    if (moltemplate) {
      imol = molindex[i];
      iatom = molatom[i];
      tagprev = tag[i] - iatom - 1;
    }

    ibin = atom2bin[i];

    // loop through stencils for all collections
    for (jcollection = 0; jcollection < ncollections; jcollection++) {

      // if same collection use own bin
      if (icollection == jcollection) jbin = ibin;
      else jbin = coord2bin(x[i], jcollection);

      s = stencil_multi[icollection][jcollection];
      ns = nstencil_multi[icollection][jcollection];

      for (k = 0; k < ns; k++) {
        js = binhead_multi[jcollection][jbin + s[k]];
        for (j = js; j >= 0; j = bins[j]) {
          if (!HALF) {
            // Full neighbor list
            // only skip i = j
            if (i == j) continue;
          } else if (!NEWTON) {
            // Half neighbor list, newton off
            // only store pair if i < j
            // stores own/own pairs only once
            // stores own/ghost pairs on both procs
            if (j <= i) continue;
          } else if (TRI) {
            // Half neighbor list, newton on, triclinic
            // pairs for atoms j "below" i are excluded
            // below = lower z or (equal z and lower y) or (equal zy and lower x)
            //         (equal zyx and j <= i)
            // latter excludes self-self interaction but allows superposed atoms
            if (x[j][2] < ztmp) continue;
            if (x[j][2] == ztmp) {
              if (x[j][1] < ytmp) continue;
              if (x[j][1] == ytmp) {
                if (x[j][0] < xtmp) continue;
                if (x[j][0] == xtmp && j <= i) continue;
              }
            }
          } else {
            // Half neighbor list, newton on, orthonormal
            // store every pair for every bin in stencil,except for i's bin

            if (stencil[k] == 0) {
              // if j is owned atom, store it, since j is beyond i in linked list
              // if j is ghost, only store if j coords are "above and to the "right" of i
              if (j >= nlocal) {
                if (x[j][2] < ztmp) continue;
                if (x[j][2] == ztmp) {
                  if (x[j][1] < ytmp) continue;
                  if (x[j][1] == ytmp && x[j][0] < xtmp) continue;
                }
              }
            }
          }

          jtype = type[j];
          if (exclude && exclusion(i,j,itype,jtype,mask,molecule)) continue;

          delx = xtmp - x[j][0];
          dely = ytmp - x[j][1];
          delz = ztmp - x[j][2];
          rsq = delx * delx + dely * dely + delz * delz;

          if (SIZE) {
            radsum = radius[i] + radius[j];
            cut = radsum + skin;
            cutsq = cut * cut;

            if (rsq <= cutsq) {
              jh = j;
              if (history && rsq < radsum * radsum)
                jh = jh ^ mask_history;

              if (molecular != Atom::ATOMIC) {
                if (!moltemplate)
                  which = find_special(special[i],nspecial[i],tag[j]);
                else if (imol >= 0)
                  which = find_special(onemols[imol]->special[iatom],
                                       onemols[imol]->nspecial[iatom],
                                       tag[j]-tagprev);
                else which = 0;
                if (which == 0) neighptr[n++] = jh;
                else if (domain->minimum_image_check(delx,dely,delz))
                  neighptr[n++] = jh;
                else if (which > 0) neighptr[n++] = jh ^ (which << SBBITS);
              } else neighptr[n++] = jh;
            }
          } else {
            if (rsq <= cutneighsq[itype][jtype]) {
              if (molecular != Atom::ATOMIC) {
                if (!moltemplate)
                  which = find_special(special[i],nspecial[i],tag[j]);
                else if (imol >= 0)
                  which = find_special(onemols[imol]->special[iatom],
                                       onemols[imol]->nspecial[iatom],
                                       tag[j]-tagprev);
                else which = 0;
                if (which == 0) neighptr[n++] = j;
                else if (domain->minimum_image_check(delx,dely,delz))
                  neighptr[n++] = j;
                else if (which > 0) neighptr[n++] = j ^ (which << SBBITS);
              } else neighptr[n++] = j;
            }
          }
        }
      }
    }

    ilist[inum++] = i;
    firstneigh[i] = neighptr;
    numneigh[i] = n;
    ipage->vgot(n);
    if (ipage->status())
      error->one(FLERR,"Neighbor list overflow, boost neigh_modify one");
  }

  list->inum = inum;
  list->gnum = 0;
}

namespace LAMMPS_NS {
template class NPairMulti<0,1,0,0>;
template class NPairMulti<1,0,0,0>;
template class NPairMulti<1,1,0,0>;
template class NPairMulti<1,1,1,0>;
template class NPairMulti<0,1,0,1>;
template class NPairMulti<1,0,0,1>;
template class NPairMulti<1,1,0,1>;
template class NPairMulti<1,1,1,1>;
}
