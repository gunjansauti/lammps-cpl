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
   Contributing authors:
   Joel Clemmer (SNL)
----------------------------------------------------------------------- */

#include "atom_vec_rheo_thermal.h"

#include "atom.h"

#include <cstring>

using namespace LAMMPS_NS;

/* ---------------------------------------------------------------------- */

AtomVecRHEOThermal::AtomVecRHEOThermal(LAMMPS *lmp) : AtomVec(lmp)
{
  molecular = Atom::ATOMIC;
  mass_type = PER_TYPE;
  forceclearflag = 1;

  atom->status_flag = 1;
  atom->conductivity_flag = 1;
  atom->temperature_flag = 1;
  atom->esph_flag = 1;
  atom->heatflow_flag = 1;
  atom->pressure_flag = 1;
  atom->rho_flag = 1;
  atom->viscosity_flag = 1;

  // strings with peratom variables to include in each AtomVec method
  // strings cannot contain fields in corresponding AtomVec default strings
  // order of fields in a string does not matter
  // except: fields_data_atom & fields_data_vel must match data file

  fields_grow = {"status", "rho", "drho", "temperature", "esph", "heatflow", "conductivity", "pressure", "viscosity"};
  fields_copy = {"status", "rho", "drho", "temperature", "esph", "heatflow", "conductivity", "pressure", "viscosity"};
  fields_comm = {"status", "rho", "esph"};
  fields_comm_vel = {"status", "rho", "esph"};
  fields_reverse = {"drho", "heatflow"};
  fields_border = {"status", "rho", "esph"};
  fields_border_vel = {"status", "rho", "esph"};
  fields_exchange = {"status", "rho", "esph"};
  fields_restart = {"status", "rho", "esph"};
  fields_create = {"status", "rho", "drho", "temperature", "esph", "heatflow", "conductivity", "pressure", "viscosity"};
  fields_data_atom = {"id", "type", "status", "rho", "esph", "x"};
  fields_data_vel = {"id", "v"};

  setup_fields();
}

/* ----------------------------------------------------------------------
   set local copies of all grow ptrs used by this class, except defaults
   needed in replicate when 2 atom classes exist and it calls pack_restart()
------------------------------------------------------------------------- */

void AtomVecRHEOThermal::grow_pointers()
{
  status = atom->status;
  conductivity = atom->conductivity;
  temperature = atom->temperature;
  esph = atom->esph;
  heatflow = atom->heatflow;
  pressure = atom->pressure;
  rho = atom->rho;
  drho = atom->drho;
  viscosity = atom->viscosity;
}

/* ----------------------------------------------------------------------
   clear extra forces starting at atom N
   nbytes = # of bytes to clear for a per-atom vector
------------------------------------------------------------------------- */

void AtomVecRHEOThermal::force_clear(int n, size_t nbytes)
{
  memset(&drho[n], 0, nbytes);
  memset(&heatflow[n], 0, nbytes);
}

/* ----------------------------------------------------------------------
   initialize non-zero atom quantities
------------------------------------------------------------------------- */

void AtomVecRHEOThermal::create_atom_post(int ilocal)
{
  rho[ilocal] = 1.0;
}

/* ----------------------------------------------------------------------
   modify what AtomVec::data_atom() just unpacked
   or initialize other atom quantities
------------------------------------------------------------------------- */

void AtomVecRHEOThermal::data_atom_post(int ilocal)
{
  drho[ilocal] = 0.0;
  heatflow[ilocal] = 0.0;
  temperature[ilocal] = 0.0;
  pressure[ilocal] = 0.0;
  viscosity[ilocal] = 0.0;
  conductivity[ilocal] = 0.0;
}

/* ----------------------------------------------------------------------
   assign an index to named atom property and return index
   return -1 if name is unknown to this atom style
------------------------------------------------------------------------- */

int AtomVecRHEOThermal::property_atom(const std::string &name)
{
  if (name == "status") return 0;
  if (name == "rho") return 1;
  if (name == "drho") return 2;
  if (name == "temperature") return 3;
  if (name == "esph") return 4;
  if (name == "heatflow") return 5;
  if (name == "conductivity") return 6;
  if (name == "pressure") return 7;
  if (name == "viscosity") return 8;
  return -1;
}

/* ----------------------------------------------------------------------
   pack per-atom data into buf for ComputePropertyAtom
   index maps to data specific to this atom style
------------------------------------------------------------------------- */

void AtomVecRHEOThermal::pack_property_atom(int index, double *buf, int nvalues, int groupbit)
{
  int *mask = atom->mask;
  int nlocal = atom->nlocal;
  int n = 0;

  if (index == 0) {
    for (int i = 0; i < nlocal; i++) {
      if (mask[i] & groupbit)
        buf[n] = status[i];
      else
        buf[n] = 0.0;
      n += nvalues;
    }
  } else if (index == 1) {
    for (int i = 0; i < nlocal; i++) {
      if (mask[i] & groupbit)
        buf[n] = rho[i];
      else
        buf[n] = 0.0;
      n += nvalues;
    }
  } else if (index == 2) {
    for (int i = 0; i < nlocal; i++) {
      if (mask[i] & groupbit)
        buf[n] = drho[i];
      else
        buf[n] = 0.0;
      n += nvalues;
    }
  } else if (index == 3) {
    for (int i = 0; i < nlocal; i++) {
      if (mask[i] & groupbit)
        buf[n] = temperature[i];
      else
        buf[n] = 0.0;
      n += nvalues;
    }
  } else if (index == 4) {
    for (int i = 0; i < nlocal; i++) {
      if (mask[i] & groupbit)
        buf[n] = esph[i];
      else
        buf[n] = 0.0;
      n += nvalues;
    }
  } else if (index == 5) {
    for (int i = 0; i < nlocal; i++) {
      if (mask[i] & groupbit)
        buf[n] = heatflow[i];
      else
        buf[n] = 0.0;
      n += nvalues;
    }
  } else if (index == 6) {
    for (int i = 0; i < nlocal; i++) {
      if (mask[i] & groupbit)
        buf[n] = conductivity[i];
      else
        buf[n] = 0.0;
      n += nvalues;
    }
  } else if (index == 7) {
    for (int i = 0; i < nlocal; i++) {
      if (mask[i] & groupbit)
        buf[n] = pressure[i];
      else
        buf[n] = 0.0;
      n += nvalues;
    }
  } else if (index == 8) {
    for (int i = 0; i < nlocal; i++) {
      if (mask[i] & groupbit)
        buf[n] = viscosity[i];
      else
        buf[n] = 0.0;
      n += nvalues;
    }
  }
}
