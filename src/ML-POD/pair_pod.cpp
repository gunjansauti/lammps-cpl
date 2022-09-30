
#include "pair_pod.h"

#include "atom.h"
#include "comm.h"
#include "error.h"
#include "force.h"
#include "memory.h"
#include "neigh_list.h"
#include "neighbor.h"
#include "tokenizer.h"

#include <glob.h>

#include <cmath>
#include <cstring>
#include <sys/time.h>

using namespace LAMMPS_NS;

/* ---------------------------------------------------------------------- */

CPairPOD::CPairPOD(LAMMPS *lmp) : Pair(lmp)
{
  single_enable = 0;
  restartinfo = 0;
  one_coeff = 1;
  manybody_flag = 1;
  centroidstressflag = CENTROID_NOTAVAIL;
}

/* ---------------------------------------------------------------------- */

CPairPOD::~CPairPOD()
{
  this->free_memory();
  TemplateFree(gd, backend);
  TemplateFree(energycoeff, backend);
  TemplateFree(forcecoeff, backend);
  TemplateFree(podcoeff, backend);
  TemplateFree(newpodcoeff, backend);

  delete podptr;

  if (allocated) {
    memory->destroy(setflag);
    memory->destroy(cutsq);
    memory->destroy(scale);
  }
}

void CPairPOD::compute(int eflag, int vflag)
{
  ev_init(eflag,vflag);
  vflag_fdotr = 1;

  double **x = atom->x;
  double **f = atom->f;
  int **firstneigh = list->firstneigh;
  int *numneigh = list->numneigh;
  int *type = atom->type;
  int *ilist = list->ilist;
  int nlocal = atom->nlocal;
  int inum = list->inum;
  int nall = inum + atom->nghost;
  
  // initialize global descriptors to zero
  
  int nd1234 = podptr->pod.nd1234;
  podptr->podArraySetValue(gd, 0.0, nd1234);

  for (int ii = 0; ii < inum; ii++) {
    int i = ilist[ii];
    int jnum = numneigh[i];

    // allocate temporary memory
    
    if (nijmax < jnum) {
      nijmax = PODMAX(nijmax, jnum);
      nablockmax = 1;
      this->free_tempmemory();
      this->estimate_tempmemory();
      this->allocate_tempmemory();
    }

    // get neighbor pairs for atom i
    
    this->lammpsNeighPairs(x, firstneigh, type, numneigh, i);

    // compute global POD descriptors for atom i
    
    podptr->linear_descriptors_ij(gd, tmpmem, rij, &tmpmem[nd1234], numneighsum,
      typeai, idxi, ti, tj, 1, nij);
  }

  int nd1 = podptr->pod.nd1;
  int nd2 = podptr->pod.nd2;
  int nd3 = podptr->pod.nd3;
  int nd4 = podptr->pod.nd4;
  int nd22 = podptr->pod.nd22;
  int nd23 = podptr->pod.nd23;
  int nd24 = podptr->pod.nd24;
  int nd33 = podptr->pod.nd33;
  int nd34 = podptr->pod.nd34;
  int nd44 = podptr->pod.nd44;    
  int nd = podptr->pod.nd;  
  bigint natom = atom->natoms;
  
  for (int j=nd1234; j<(nd1234+nd22+nd23+nd24+nd33+nd34+nd44); j++)
    newpodcoeff[j] = podcoeff[j]/(natom);

  for (int j=(nd1234+nd22+nd23+nd24+nd33+nd34+nd44); j<nd; j++)
    newpodcoeff[j] = podcoeff[j]/(natom*natom);
  
  // compute energy and effective coefficients
  
  eng_vdwl = podptr->calculate_energy(energycoeff, forcecoeff, gd, newpodcoeff);

  for (int ii = 0; ii < inum; ii++) {
    int i = ilist[ii];

    // get neighbor pairs for atom i
    
    this->lammpsNeighPairs(x, firstneigh, type, numneigh, i);

    // compute atomic force for atom i
    
    podptr->calculate_force(f, forcecoeff, rij, tmpmem, numneighsum,
      typeai, idxi, ai, aj, ti, tj, 1, nij);
  }

  if (vflag_fdotr) virial_fdotr_compute();


//  printf("%d %d %d %d\n", eflag, vflag, vflag_fdotr, eflag_atom);
//
//   if (eflag_atom) {
//     eng_vdwl = this->lammpseatom(eatom, atom->x, list->firstneigh, atom->type, list->numneigh,
//             list->ilist, list->inum, list->inum + atom->nghost);
//
//     this->lammpsforce(atom->f, atom->x, list->firstneigh, atom->type, list->numneigh,
//             list->ilist, list->inum, list->inum + atom->nghost);
//   }
//   else {
//     eng_vdwl = this->lammpsenergyforce(atom->f, atom->x, list->firstneigh, atom->type, list->numneigh,
//           list->ilist, list->inum, list->inum + atom->nghost);
//   }
//
// //   podptr->print_matrix("atom->f", 3, list->inum + atom->nghost, atom->f, 3);
// //   podptr->print_matrix("atom->x", 3, list->inum + atom->nghost, atom->x, 3);
// //   cout<<"Energy: "<< eng_vdwl<<endl;
// //   podptr->print_matrix("virial", 1, 6, virial, 1);
//
//   if (vflag_fdotr) virial_fdotr_compute();
}

/* ----------------------------------------------------------------------
   global settings
------------------------------------------------------------------------- */

void CPairPOD::settings(int narg, char ** /* arg */)
{
  if (narg > 0)
  error->all(FLERR,"Illegal pair_style command");
}

/* ----------------------------------------------------------------------
   set coeffs for one or more type pairs
------------------------------------------------------------------------- */

void CPairPOD::coeff(int narg, char **arg)
{
  // set default scaling  
  int n = atom->ntypes;
  memory->create(setflag,n+1,n+1,"pair:setflag");
  memory->create(cutsq,n+1,n+1,"pair:cutsq");
  memory->create(scale,n+1,n+1,"pair:scale");
  map = new int[n+1];
  for (int ii = 0; ii < n+1; ii++)
    for (int jj = 0; jj < n+1; jj++)
      scale[ii][jj] = 1.0;
  allocated = 1;

  if (narg < 4) error->all(FLERR,"Incorrect args for pair coefficients");
//  if (narg != 5 + atom->ntypes) error->all(FLERR,"Incorrect args for pair coefficients");

  map_element2type(narg-4,&arg[4]);
//   cout<<map[0]<<endl;
//   cout<<map[1]<<endl;

  std::string pod_file = std::string(arg[2]);  // pod input file
  std::string coeff_file = std::string(arg[3]); // coefficient input file

  this->InitPairPOD(pod_file, coeff_file);

  for (int ii = 0; ii < n+1; ii++)
    for (int jj = 0; jj < n+1; jj++)
      cutsq[ii][jj] = podptr->pod.rcut*podptr->pod.rcut;
}

/* ----------------------------------------------------------------------
   init specific to this pair style
------------------------------------------------------------------------- */

void CPairPOD::init_style()
{
  if (force->newton_pair == 0)
  error->all(FLERR,"Pair style POD requires newton pair on");

  // need a full neighbor list

  neighbor->add_request(this, NeighConst::REQ_FULL);
}

/* ----------------------------------------------------------------------
   init for one type pair i,j and corresponding j,i
------------------------------------------------------------------------- */

double CPairPOD::init_one(int i, int j)
{
  if (setflag[i][j] == 0) error->all(FLERR,"All pair coeffs are not set");
    scale[j][i] = scale[i][j];
  return podptr->pod.rcut;
}

/* ----------------------------------------------------------------------
   memory usage
------------------------------------------------------------------------- */

double CPairPOD::memory_usage()
{
  double bytes = Pair::memory_usage();
  return bytes;
}

void *CPairPOD::extract(const char *str, int &dim)
{
  dim = 2;
  if (strcmp(str,"scale") == 0) return (void *) scale;
  return nullptr;
}

void CPairPOD::InitPairPOD(std::string pod_file, std::string coeff_file)
{
  podptr = new CPOD(lmp, pod_file, coeff_file);

  podpairlist = 0;
  lammpspairlist = 1;

  if (coeff_file != "") {
    TemplateMalloc(&podcoeff, podptr->pod.nd, backend);
    TemplateMalloc(&newpodcoeff, podptr->pod.nd, backend);
    TemplateMalloc(&energycoeff, podptr->pod.nd1234, backend);
    TemplateMalloc(&forcecoeff, podptr->pod.nd1234, backend);
    TemplateMalloc(&gd, podptr->pod.nd1234, backend);
    podptr->podArrayCopy(podcoeff, podptr->pod.coeff, podptr->pod.nd);
    podptr->podArrayCopy(newpodcoeff, podptr->pod.coeff, podptr->pod.nd);
  }
}

bool CPairPOD::is_a_number(std::string line)
{
  return isdigit(line.at(0));
}

void CPairPOD::get_atomblocks(int natom)
{
  if (blocksize >= natom) {
    numblocks = 1;
    atomblocks[0] = 0;
    atomblocks[1] = natom;
  }
  else {
  numblocks = (int) ceil( ((double) natom)/((double) blocksize) );

  double delta = ((double) natom) / ((double) numblocks);
  for(int i=0; i < numblocks; ++i)
    atomblocks[i]= (int) delta * i;
  atomblocks[numblocks] = natom;
  }
  if (numblocks > 1000) error->all(FLERR,"number of computation blocks can not be more than 1000. This error can be fixed by increasing the number of atoms per computation block.");
}

int CPairPOD::latticecoords(double *y, int *alist, double *x, double *a1, double *a2, double *a3, double rcut, int *pbc, int nx)
{
  int m=0, n=0, p=0;
  if (pbc[0] == 1) m = (int) ceil(rcut/a1[0]);
  if (pbc[1] == 1) n = (int) ceil(rcut/a2[1]);
  if (pbc[2] == 1) p = (int) ceil(rcut/a3[2]);

  // index for the center lattice

  int ind = m + (2*m+1)*(n) + (2*m+1)*(2*n+1)*(p);

  // number of lattices

  int nl = (2*m+1)*(2*n+1)*(2*p+1);

  //y = zeros(3, nx*nl)

  for (int j=0; j<3*nx; j++)
    y[j] = x[j];
  int q = nx;

  for (int i = 0; i < (2*p+1); i++)
    for (int j = 0; j < (2*n+1); j++)
      for (int k = 0; k < (2*m+1); k++) {
        int ii = k + (2*m+1)*j + (2*m+1)*(2*n+1)*i;
        if (ii != ind) {
          double x0 = a1[0]*(k - m) + a2[0]*(j - n) + a3[0]*(i - p);
          double x1 = a1[1]*(k - m) + a2[1]*(j - n) + a3[1]*(i - p);
          double x2 = a1[2]*(k - m) + a2[2]*(j - n) + a3[2]*(i - p);
          for (int jj=0; jj<nx; jj++) {
          y[0+3*q] = x0 + x[0+3*jj];
          y[1+3*q] = x1 + x[1+3*jj];
          y[2+3*q] = x2 + x[2+3*jj];
          q = q + 1;
          }
        }
    }

  //alist = zeros(Int32,nx*nl)

  for (int i=0; i <nl; i++)
    for (int j=0; j<nx; j++)
      alist[j + nx*i] = j;

  return nl;
}

int CPairPOD::podneighborcount(double *r, double rcutsq, int nx, int N, int dim)
{
  int k = 0;
  for (int i = 0; i<nx; i++) {
    double *ri = &r[i*dim];
    for (int j=0; j<N; j++) {
      double *rj = &r[dim*j];
      double rijsq = (ri[0]-rj[0])*(ri[0]-rj[0]) + (ri[1]-rj[1])*(ri[1]-rj[1]) + (ri[2]-rj[2])*((ri[2]-rj[2]));
      if  ((rijsq > 1e-12) && (rijsq <= rcutsq)) k += 1;
    }
  }
  return k;
}

int CPairPOD::podneighborlist(int *neighlist, int *numneigh, double *r, double rcutsq, int nx, int N, int dim)
{
  int k = 0;
  for (int i = 0; i<nx; i++) {
  double *ri = &r[i*dim];
  int inc = 0;
  for (int j=0; j<N; j++) {
    double *rj = &r[dim*j];
    double rijsq = (ri[0]-rj[0])*(ri[0]-rj[0]) + (ri[1]-rj[1])*(ri[1]-rj[1]) + (ri[2]-rj[2])*((ri[2]-rj[2]));
    if  ((rijsq > 1e-12) && (rijsq <= rcutsq))  {
      inc += 1;
      neighlist[k] = j;
      k += 1;
    }
  }
  numneigh[i] = inc;
  }
  return k;
}

int CPairPOD::podfullneighborlist(double *xy, int *alist, int *neighlist, int *numneigh, int *numneighsum,
  double *x, double *a1, double *a2, double *a3, double rcut, int *pbc, int nx)
{
  double rcutsq = rcut*rcut;
  int dim = 3, nl = 0, nn = 0;

  // number of lattices

  nl = this->latticecoords(xy, alist, x, a1, a2, a3, rcut, pbc, nx);
  int N = nx*nl;

  // total number of neighbors

   nn = this->podneighborlist(neighlist, numneigh, xy, rcutsq, nx, N, dim);

   podptr->podCumsum(numneighsum, numneigh, nx+1);

   return nn;
}

void CPairPOD::free_tempmemory()
{
  TemplateFree(rij, backend);
  TemplateFree(idxi, backend);
  TemplateFree(ai, backend);
  TemplateFree(aj, backend);
  TemplateFree(ti, backend);
  TemplateFree(tj, backend);
  TemplateFree(numneighsum, backend);
  TemplateFree(typeai, backend);
  TemplateFree(tmpmem, backend);
}

void CPairPOD::free_atommemory()
{
  TemplateFree(forces, backend);
  TemplateFree(stress, backend);
  if (atommemory) {
  TemplateFree(atomtype, backend);
  TemplateFree(pos, backend);
  TemplateFree(vel, backend);
  }
}

void CPairPOD::free_pairmemory()
{
  if (podpairlist) {
  TemplateFree(y, backend);
  TemplateFree(pairlist, backend);
  TemplateFree(pairnum, backend);
  TemplateFree(pairnumsum, backend);
  TemplateFree(atomID, backend);
  }
}

void CPairPOD::free_memory()
{
  this->free_tempmemory();
  this->free_atommemory();
  this->free_pairmemory();
}

void CPairPOD::allocate_tempmemory()
{
  TemplateMalloc(&rij, dim*nijmax, backend);
  TemplateMalloc(&idxi, nijmax, backend);
  TemplateMalloc(&ai, nijmax, backend);
  TemplateMalloc(&aj, nijmax, backend);
  TemplateMalloc(&ti, nijmax, backend);
  TemplateMalloc(&tj, nijmax, backend);
  TemplateMalloc(&numneighsum, nablockmax+1, backend);
  TemplateMalloc(&typeai, nablockmax, backend);
  TemplateMalloc(&tmpmem, szd, backend);
}

void CPairPOD::allocate_atommemory()
{
  TemplateMalloc(&forces, dim*nmaxatom, backend);
  TemplateMalloc(&stress, 9, backend);
  if (atommemory) {
  TemplateMalloc(&atomtype, nmaxatom, backend);
  TemplateMalloc(&pos, dim*nmaxatom, backend);
  TemplateMalloc(&vel, dim*nmaxatom, backend);
  }
}

void CPairPOD::allocate_pairmemory()
{
  if (podpairlist) {
  TemplateMalloc(&y, dim*nmaxatom, backend);
  TemplateMalloc(&atomID, nmaxatom, backend);
  TemplateMalloc(&pairnum, nlocalmax, backend);
  TemplateMalloc(&pairnumsum, nlocalmax+1, backend);
  TemplateMalloc(&pairlist, nmaxpairs, backend);
  }
}

void CPairPOD::allocate_memory()
{
  this->allocate_tempmemory();
  this->allocate_atommemory();
  this->allocate_pairmemory();
}

void CPairPOD::check_atommemory(int inum, int nall)
{
  if (nmaxatom < nall) {
  nmaxatom = nall;
  this->free_atommemory();
  this->allocate_atommemory();
  }
  nlocalatom = inum;
  nghostatom = nall - inum;
  ntotalatom = nall;
  nlocalmax = PODMAX(nlocalmax, nlocalatom);
}

void CPairPOD::check_pairmemory(double *x, double *a1, double *a2, double *a3, int natom)
{
  double rcut = podptr->pod.rcut;
  int m=0, n=0, p=0;
  if (podptr->pod.pbc[0] == 1) m = (int) ceil(rcut/a1[0]);
  if (podptr->pod.pbc[1] == 1) n = (int) ceil(rcut/a2[1]);
  if (podptr->pod.pbc[2] == 1) p = (int) ceil(rcut/a3[2]);

  // number of lattices

  int nl = (2*m+1)*(2*n+1)*(2*p+1);
  int nall = natom*nl;

  nlocalatom = natom;
  nghostatom = nall - natom;
  ntotalatom = nall;

  if (nlocalmax < natom) {
  nlocalmax = natom;
  printf("reallocate memory for pairnum and pairnumsum arrays\n");
  TemplateFree(pairnum, backend);
  TemplateFree(pairnumsum, backend);
  TemplateMalloc(&pairnum, nlocalmax, backend);
  TemplateMalloc(&pairnumsum, nlocalmax+1, backend);
  }

  if (nmaxatom < nall) {
  nmaxatom = nall;
  printf("reallocate memory for y and atomID arrays\n");
  TemplateFree(y, backend);
  TemplateFree(atomID, backend);
  TemplateMalloc(&y, dim*nmaxatom, backend);
  TemplateMalloc(&atomID, nmaxatom, backend);

  // allocate memory for atom arrays

  this->free_atommemory();
  this->allocate_atommemory();
  }

  double rcutsq = rcut*rcut;
  this->latticecoords(y, atomID, x, a1, a2, a3, rcut, podptr->pod.pbc, natom);

  natompairs = this->podneighborcount(y, rcutsq, natom, nall, dim);
  if (nmaxpairs < natompairs) {
  nmaxpairs = natompairs;
  printf("reallocate memory for pairlist arrays\n");
  TemplateFree(pairlist, backend);
  TemplateMalloc(&pairlist, nmaxpairs, backend);
  }

  // total number of neighbors

   natompairs = this->podneighborlist(pairlist, pairnum, y, rcutsq, natom, nall, dim);
   podptr->podCumsum(pairnumsum, pairnum, natom+1);
}

void CPairPOD::estimate_tempmemory()
{
  int nrbf2 = podptr->pod.nbf2;
  int nabf3 = podptr->pod.nabf3;
  int nrbf3 = podptr->pod.nrbf3;
  int ns2 = podptr->pod.ns2;
  int ns3 = podptr->pod.ns3;

  szd = dim*nijmax+ (1+dim)*nijmax*PODMAX(nrbf2+ns2,nrbf3+ns3) + (nabf3+1)*7;
  int szsnap = 0;
  if (podptr->sna.twojmax>0) {
  szsnap += nijmax*dim;
  szsnap += PODMAX(2*podptr->sna.idxu_max*nijmax, 2*podptr->sna.idxz_max*podptr->sna.ndoubles*nablockmax); // (Ur, Ui) and (Zr, Zi)
  szsnap += 2*podptr->sna.idxu_max*dim*nijmax; // dUr, dUi
  szsnap += PODMAX(podptr->sna.idxb_max*podptr->sna.ntriples*dim*nijmax, 2*podptr->sna.idxu_max*podptr->sna.nelements*nablockmax); // dblist and (Utotr, Utoti)
  }

  szd = PODMAX(szsnap, szd);
  szd = nablockmax*(podptr->pod.nd1234) + szd;
}

void CPairPOD::check_tempmemory(int start, int end)
{
  nablock = end - start;
  nij = 0;
  for (int ii=0; ii<nablock; ii++) {
  int gi = start + ii;
  nij += pairnumsum[gi+1] - pairnumsum[gi];
  }

  if ( (nij > nijmax) || (nablock > nablockmax) ) {
  nijmax = PODMAX(nijmax, nij);
  nablockmax = PODMAX(nablockmax, nablock);
  this->estimate_tempmemory();
  this->free_tempmemory();
  this->allocate_tempmemory();
  }
}

void CPairPOD::podNeighPairs(int *atomtypes, int start, int end)
{
  this->check_tempmemory(start, end);

  nablock = end - start;
  int k = 0;

  // loop over atoms ini a simulation block, used for GPU

  for (int ii=0; ii<nablock; ii++) {  
    int gi = start + ii; // atom i
    int itype = atomtypes[gi];
    int s = pairnumsum[gi];
    int m = pairnumsum[gi+1] - s;
    typeai[ii] = itype;
    numneighsum[ii+1] = m;
    for (int l=0; l<m ; l++) {
      int gj = pairlist[s + l]; // atom j
      idxi[k]  = ii;
      ai[k]  = atomID[gi];
      aj[k]  = atomID[gj];
      ti[k]  = itype;
      tj[k]  = atomtypes[aj[k]];
      rij[k*3+0]   = y[gj*3+0] -  y[gi*3+0];  // xj - xi
      rij[k*3+1]   = y[gj*3+1] -  y[gi*3+1];  // xj - xi
      rij[k*3+2]   = y[gj*3+2] -  y[gi*3+2];  // xj - xi
      k += 1;
    }
  }

  numneighsum[0] = 0;
  for (int ii=0; ii<nablock; ii++)
    numneighsum[ii+1] = numneighsum[ii+1] + numneighsum[ii];
}

double CPairPOD::podenergy(double *x, double *a1, double *a2, double *a3, int *atomtypes, int inum)
{
  // determine computation blocks

  this->get_atomblocks(inum);

  // check and allocate memory for atom/pair arrays, and create full neighbor list

  this->check_pairmemory(x, a1, a2, a3, inum);

  // initialize global descriptors to zero

  int nd1234 = podptr->pod.nd1234;
  podptr->podArraySetValue(gd, 0.0, nd1234);

  for (int i = 0; i< numblocks; i++) {

    // number of atoms in this computation block

    int nat = atomblocks[i+1] - atomblocks[i];

    // get POD neighbor pairs for this computation block

    podNeighPairs(atomtypes, atomblocks[i], atomblocks[i+1]);

    // compute global POD descriptors for this computation block

    podptr->linear_descriptors_ij(gd, tmpmem, rij, &tmpmem[nat*nd1234], numneighsum,
      typeai, idxi, ti, tj, nat, nij);

  }

  // compute energy and effective coefficients

  energy = podptr->calculate_energy(energycoeff, forcecoeff, gd, podcoeff);

  return energy;
}

double CPairPOD::podeatom(double *eatom, double *x, double *a1, double *a2, double *a3, int *atomtypes, int inum)
{
  int nd1234 = podptr->pod.nd1234;

  // compute energy and effective coefficients

  energy = this->podenergy(x, a1, a2, a3, atomtypes, inum);

  // initialize force to zero

  podptr->podArraySetValue(eatom, 0.0, inum);

  for (int i = 0; i< numblocks; i++) { // loop over each computation block

    // # of atoms in this computation block

    int nat = atomblocks[i+1] - atomblocks[i];

    // get POD neighbor pairs for this computation block

    podNeighPairs(atomtypes, atomblocks[i], atomblocks[i+1]);

    // compute global POD descriptors for this computation block

    podptr->linear_descriptors_ij(gd, tmpmem, rij, &tmpmem[nat*nd1234], numneighsum,
      typeai, idxi, ti, tj, nat, nij);

    // calculate eatom = ld * energycoeff

    char chn = 'N';
    double one = 1.0, zero = 0.0;
    int inc1 = 1;
    DGEMV(&chn, &nat, &nd1234, &one, tmpmem, &nat, energycoeff, &inc1, &zero, &eatom[atomblocks[i]], &inc1);
  }

  return energy;
}

void CPairPOD::podforce(double *f, double *x, double *a1, double *a2, double *a3, int *atomtypes, int inum)
{
  // initialize force to zero

  podptr->podArraySetValue(f, 0.0, dim*inum);

  for (int i = 0; i< numblocks; i++) { // loop over each computation block

    // # of atoms in this computation block

    int nat = atomblocks[i+1] - atomblocks[i];

    // get POD neighbor pairs for this computation block

    podNeighPairs(atomtypes, atomblocks[i], atomblocks[i+1]);

    // compute atomic force for this computation block

    podptr->calculate_force(f, forcecoeff, rij, tmpmem, numneighsum,
      typeai, idxi, ai, aj, ti, tj, nat, nij);
    }
}

double CPairPOD::podenergyforce(double *f, double *x, double *a1, double *a2, double *a3, int *atomtypes, int inum)
{
  // compute energy and effective coefficients

  energy = this->podenergy(x, a1, a2, a3, atomtypes, inum);

  // initialize force to zero

  podptr->podArraySetValue(f, 0.0, dim*inum);

  for (int i = 0; i< numblocks; i++) { // loop over each computation block

    // # of atoms in this computation block

    int nat = atomblocks[i+1] - atomblocks[i];

    // get POD neighbor pairs for this computation block

    podNeighPairs(atomtypes, atomblocks[i], atomblocks[i+1]);

    // compute atomic force for this computation block

    podptr->calculate_force(f, forcecoeff, rij, tmpmem, numneighsum,
      typeai, idxi, ai, aj, ti, tj, nat, nij);
    }

  return energy;
}

void CPairPOD::lammpsNeighPairs(double **x, int **firstneigh, int *atomtypes, int *numneigh, int gi)
{
  double rcutsq = podptr->pod.rcut*podptr->pod.rcut;

  nij = 0;
  int itype = atomtypes[gi];
  int m = numneigh[gi];
  typeai[0] = itype;
  for (int l=0; l<m ; l++) {   // loop over each atom around atom i
    int gj = firstneigh[gi][l];  // atom j
    double delx   = x[gj][0] -  x[gi][0];  // xj - xi
    double dely   = x[gj][1] -  x[gi][1];  // xj - xi
    double delz   = x[gj][2] -  x[gi][2];  // xj - xi
    double rsq = delx*delx + dely*dely + delz*delz;
    if (rsq < rcutsq && rsq > 1e-20) {
      rij[nij*3 + 0] = delx;
      rij[nij*3 + 1] = dely;
      rij[nij*3 + 2] = delz;
      idxi[nij]    = 0;
      ai[nij]    = gi;
      aj[nij]    = gj;
      ti[nij]    = itype;
      tj[nij]    = atomtypes[gj];
      nij++;
    }
  }

  numneighsum[0] = 0;
  numneighsum[1] = nij;
}

void CPairPOD::check_tempmemory(double **x, int **firstneigh, int *numneigh, int *ilist, int start, int end)
{
  double rcutsq = podptr->pod.rcut*podptr->pod.rcut;
  nablock = end - start;
  nij = 0;
  for (int ii=0; ii<nablock; ii++) {  // for each atom i in the simulation box
    int gi = ilist[start+ii];   // atom i
    int m = numneigh[gi];
    for (int l=0; l<m ; l++) {   // loop over each atom around atom i
      int gj = firstneigh[gi][l];  // atom j
      double delx   = x[gj][0] -  x[gi][0];  // xj - xi
      double dely   = x[gj][1] -  x[gi][1];  // xj - xi
      double delz   = x[gj][2] -  x[gi][2];  // xj - xi
      double rsq = delx*delx + dely*dely + delz*delz;
      if (rsq < rcutsq && rsq > 1e-20) nij++;
    }
  }

  if ( (nij > nijmax) || (nablock > nablockmax) ) {
    nijmax = PODMAX(nijmax, nij);
    nablockmax = PODMAX(nablockmax, nablock);
    this->estimate_tempmemory();
    this->free_tempmemory();
    this->allocate_tempmemory();
  }
}

void CPairPOD::lammpsNeighPairs(double **x, int **firstneigh, int *atomtypes, int *numneigh,
  int *ilist, int start, int end)
{
  this->check_tempmemory(x, firstneigh, numneigh, ilist, start, end);

  nablock = end - start;
  double rcutsq = podptr->pod.rcut*podptr->pod.rcut;

  nij = 0;
  for (int ii=0; ii<nablock; ii++) {  // for each atom i in the simulation box
    int gi = ilist[start+ii];   // atom i
    int itype = atomtypes[gi];
    int m = numneigh[gi];
    numneighsum[ii+1] = 0;
    typeai[ii] = itype;
    for (int l=0; l<m ; l++) {   // loop over each atom around atom i
      int gj = firstneigh[gi][l];  // atom j
      double delx   = x[gj][0] -  x[gi][0];  // xj - xi
      double dely   = x[gj][1] -  x[gi][1];  // xj - xi
      double delz   = x[gj][2] -  x[gi][2];  // xj - xi
      double rsq = delx*delx + dely*dely + delz*delz;
      if (rsq < rcutsq && rsq > 1e-20) {
        rij[nij*3 + 0] = delx;
        rij[nij*3 + 1] = dely;
        rij[nij*3 + 2] = delz;
        idxi[nij]  = ii;
        ai[nij]  = gi;
        aj[nij]  = gj;
        ti[nij]  = itype;
        tj[nij]  = atomtypes[gj];
        nij++;
        numneighsum[ii+1] += 1;
      }
    }
  }

  numneighsum[0] = 0;
  for (int ii=0; ii<nablock; ii++)
    numneighsum[ii+1] = numneighsum[ii+1] + numneighsum[ii];
}

double CPairPOD::lammpsenergy(double **x, int **firstneigh, int *atomtypes, int *numneigh,
  int *ilist, int inum, int nall)
{
  // determine computation blocks

  this->get_atomblocks(inum);

  // check atom memory

  this->check_atommemory(inum, nall);

  int nd1234 = podptr->pod.nd1234;
  podptr->podArraySetValue(gd, 0.0, nd1234);

  // loop over computation blocks

  for (int i = 0; i< numblocks; i++) {

    // # of atoms in this computation block

    int nat = atomblocks[i+1] - atomblocks[i];

    // get LAMMPS neighbor pairs for this computation block

    lammpsNeighPairs(x, firstneigh, atomtypes, numneigh, ilist, atomblocks[i], atomblocks[i+1]);

    // compute global POD descriptors for this computation block

    podptr->linear_descriptors_ij(gd, tmpmem, rij, &tmpmem[nat*nd1234], numneighsum,
      typeai, idxi, ti, tj, nat, nij);
  }

  // compute energy and effective coefficients

  energy = podptr->calculate_energy(energycoeff, forcecoeff, gd, podcoeff);

  return energy;
}

double CPairPOD::lammpseatom(double *eatom, double **x, int **firstneigh, int *atomtypes, int *numneigh,
  int *ilist, int inum, int nall)
{
  // compute energy and effective coefficients

  energy = this->lammpsenergy(x, firstneigh, atomtypes, numneigh, ilist, inum, nall);

  int nd1234 = podptr->pod.nd1234;
  podptr->podArraySetValue(gd, 0.0, nd1234);

  for (int i = 0; i< numblocks; i++) { // loop over each computation block

    // # of atoms in this computation block

    int nat = atomblocks[i+1] - atomblocks[i];

    double *localdesc = tmpmem;
    double *ea = &tmpmem[nat*nd1234];

    // get LAMMPS neighbor pairs for this computation block

    lammpsNeighPairs(x, firstneigh, atomtypes, numneigh, ilist, atomblocks[i], atomblocks[i+1]);

    // compute global POD descriptors for this computation block

    podptr->linear_descriptors_ij(gd, localdesc, rij, ea, numneighsum,
      typeai, idxi, ti, tj, nat, nij);

    // calculate eatom = localdesc * energycoeff

    char chn = 'N';
    double one = 1.0, zero = 0.0;
    int inc1 = 1;
    DGEMV(&chn, &nat, &nd1234, &one, localdesc, &nat, energycoeff, &inc1, &zero, ea, &inc1);

    for (int j = 0; j<nat; j++)
      eatom[ilist[atomblocks[i] + j]] = ea[j];

  }

  return energy;
}

void CPairPOD::lammpsforce(double **f, double **x, int **firstneigh, int *atomtypes,
  int *numneigh, int *ilist, int inum, int nall)
{
  podptr->podArraySetValue(forces, 0.0, dim*nall);

  // loop over computation blocks

  for (int i = 0; i< numblocks; i++) {

    // # of atoms in this computation block

    int nat = atomblocks[i+1] - atomblocks[i];

    // get LAMMPS neighbor pairs for this computation block

    lammpsNeighPairs(x, firstneigh, atomtypes, numneigh, ilist, atomblocks[i], atomblocks[i+1]);

    // compute atomic force for this computation block

    podptr->calculate_force(forces, forcecoeff, rij, tmpmem, numneighsum,
      typeai, idxi, ai, aj, ti, tj, nat, nij);
  }

  // copy force to lammps force array

  for (int i = 0; i<nall; i++) {
    f[i][0] = forces[0+3*i];
    f[i][1] = forces[1+3*i];
    f[i][2] = forces[2+3*i];
  }
}

double CPairPOD::lammpsenergyforce(double **f, double **x, int **firstneigh, int *atomtypes,
  int *numneigh, int *ilist, int inum, int nall)
{

  // compute energy and effective coefficients

  energy = this->lammpsenergy(x, firstneigh, atomtypes, numneigh, ilist, inum, nall);

  podptr->podArraySetValue(forces, 0.0, dim*nall);

  // loop over computation blocks

  for (int i = 0; i< numblocks; i++) {

    // # of atoms in this computation block

    int nat = atomblocks[i+1] - atomblocks[i];

    // get LAMMPS neighbor pairs for this computation block

    lammpsNeighPairs(x, firstneigh, atomtypes, numneigh, ilist, atomblocks[i], atomblocks[i+1]);

    // compute atomic force for this computation block

    podptr->calculate_force(forces, forcecoeff, rij, tmpmem, numneighsum,
      typeai, idxi, ai, aj, ti, tj, nat, nij);
  }

  // copy force to lammps force array

  for (int i = 0; i<nall; i++) {
    f[i][0] = forces[0+3*i];
    f[i][1] = forces[1+3*i];
    f[i][2] = forces[2+3*i];
  }

  return energy;
}

