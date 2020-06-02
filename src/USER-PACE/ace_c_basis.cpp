#include "ace_c_basis.h"

using namespace std;

ACECTildeBasisSet::ACECTildeBasisSet(string filename) {
    load(filename);
}

ACECTildeBasisSet::ACECTildeBasisSet(const ACECTildeBasisSet &other) {
    ACECTildeBasisSet::_copy_scalar_memory(other);
    ACECTildeBasisSet::_copy_dynamic_memory(other);
    pack_flatten_basis();
}


ACECTildeBasisSet &ACECTildeBasisSet::operator=(const ACECTildeBasisSet &other) {
    if (this != &other) {
        _clean();
        _copy_scalar_memory(other);
        _copy_dynamic_memory(other);
        pack_flatten_basis();
    }
    return *this;
}


ACECTildeBasisSet::~ACECTildeBasisSet() {
    ACECTildeBasisSet::_clean();
}

void ACECTildeBasisSet::_clean() {
    // call parent method
    ACEFlattenBasisSet::_clean();
    _clean_contiguous_arrays();
    _clean_basis_arrays();
}

void ACECTildeBasisSet::_copy_scalar_memory(const ACECTildeBasisSet &src) {
    ACEFlattenBasisSet::_copy_scalar_memory(src);
    num_ctilde_max = src.num_ctilde_max;
}

void ACECTildeBasisSet::_copy_dynamic_memory(const ACECTildeBasisSet &src) {//allocate new memory
    ACEFlattenBasisSet::_copy_dynamic_memory(src);

    if (src.basis_rank1 == nullptr)
        throw runtime_error("Could not copy ACECTildeBasisSet::basis_rank1 - array not initialized");
    if (src.basis == nullptr) throw runtime_error("Could not copy ACECTildeBasisSet::basis - array not initialized");


    basis_rank1 = new ACECTildeBasisFunction *[src.nelements];
    basis = new ACECTildeBasisFunction *[src.nelements];

    //copy basis arrays
    for (SPECIES_TYPE mu = 0; mu < src.nelements; ++mu) {
        basis_rank1[mu] = new ACECTildeBasisFunction[src.total_basis_size_rank1[mu]];

        for (size_t i = 0; i < src.total_basis_size_rank1[mu]; i++) {
              basis_rank1[mu][i] = src.basis_rank1[mu][i];
        }


        basis[mu] = new ACECTildeBasisFunction[src.total_basis_size[mu]];
        for (size_t i = 0; i < src.total_basis_size[mu]; i++) {
            basis[mu][i] = src.basis[mu][i];
        }
    }
    //DON"T COPY CONTIGUOUS ARRAY, REBUILD THEM
}

void ACECTildeBasisSet::_clean_contiguous_arrays() {
    ACEFlattenBasisSet::_clean_contiguous_arrays();

    delete[] full_c_tildes_rank1;
    full_c_tildes_rank1 = nullptr;

    delete[] full_c_tildes;
    full_c_tildes = nullptr;
}

//re-pack the constituent dynamic arrays of all basis functions in contiguous arrays
void ACECTildeBasisSet::pack_flatten_basis() {
    compute_array_sizes(basis_rank1, basis);

    //1. clean contiguous arrays
    _clean_contiguous_arrays();
    //2. allocate contiguous arrays
    delete[] full_ns_rank1;
    full_ns_rank1 = new NS_TYPE[rank_array_total_size_rank1];
    delete[] full_ls_rank1;
    full_ls_rank1 = new NS_TYPE[rank_array_total_size_rank1];
    delete[] full_mus_rank1;
    full_mus_rank1 = new SPECIES_TYPE[rank_array_total_size_rank1];
    delete[] full_ms_rank1;
    full_ms_rank1 = new MS_TYPE[rank_array_total_size_rank1];

    delete[] full_c_tildes_rank1;
    full_c_tildes_rank1 = new DOUBLE_TYPE[coeff_array_total_size_rank1];


    delete[] full_ns;
    full_ns = new NS_TYPE[rank_array_total_size];
    delete[] full_ls;
    full_ls = new LS_TYPE[rank_array_total_size];
    delete[] full_mus;
    full_mus = new SPECIES_TYPE[rank_array_total_size];
    delete[] full_ms;
    full_ms = new MS_TYPE[ms_array_total_size];

    delete[] full_c_tildes;
    full_c_tildes = new DOUBLE_TYPE[coeff_array_total_size];


    //3. copy the values from private C_tilde_B_basis_function arrays to new contigous space
    //4. clean private memory
    //5. reassign private array pointers

    //r = 0, rank = 1
    size_t rank_array_ind_rank1 = 0;
    size_t coeff_array_ind_rank1 = 0;
    size_t ms_array_ind_rank1 = 0;

    for (SPECIES_TYPE mu = 0; mu < nelements; ++mu) {
        for (int func_ind_r1 = 0; func_ind_r1 < total_basis_size_rank1[mu]; ++func_ind_r1) {
            ACECTildeBasisFunction &func = basis_rank1[mu][func_ind_r1];

            //copy values ns from c_tilde_basis_function private memory to contigous memory part
            full_ns_rank1[rank_array_ind_rank1] = func.ns[0];

            //copy values ls from c_tilde_basis_function private memory to contigous memory part
            full_ls_rank1[rank_array_ind_rank1] = func.ls[0];

            //copy values mus from c_tilde_basis_function private memory to contigous memory part
            full_mus_rank1[rank_array_ind_rank1] = func.mus[0];

            //copy values ctildes from c_tilde_basis_function private memory to contigous memory part
            memcpy(&full_c_tildes_rank1[coeff_array_ind_rank1], func.ctildes,
                   func.ndensity * sizeof(DOUBLE_TYPE));


            //copy values mus from c_tilde_basis_function private memory to contigous memory part
            memcpy(&full_ms_rank1[ms_array_ind_rank1], func.ms_combs,
                   func.num_ms_combs *
                   func.rank * sizeof(MS_TYPE));

            //release memory of each ACECTildeBasisFunction if it is not proxy
            func._clean();

            func.mus = &full_mus_rank1[rank_array_ind_rank1];
            func.ns = &full_ns_rank1[rank_array_ind_rank1];
            func.ls = &full_ls_rank1[rank_array_ind_rank1];
            func.ms_combs = &full_ms_rank1[ms_array_ind_rank1];
            func.ctildes = &full_c_tildes_rank1[coeff_array_ind_rank1];
            func.is_proxy = true;

            rank_array_ind_rank1 += func.rank;
            ms_array_ind_rank1 += func.rank *
                                  func.num_ms_combs;
            coeff_array_ind_rank1 += func.num_ms_combs * func.ndensity;

        }
    }


    //rank>1, r>0
    size_t rank_array_ind = 0;
    size_t coeff_array_ind = 0;
    size_t ms_array_ind = 0;

    for (SPECIES_TYPE mu = 0; mu < nelements; ++mu) {
        for (int func_ind = 0; func_ind < total_basis_size[mu]; ++func_ind) {
            ACECTildeBasisFunction &func = basis[mu][func_ind];

            //copy values mus from c_tilde_basis_function private memory to contigous memory part
            memcpy(&full_mus[rank_array_ind], func.mus,
                   func.rank * sizeof(SPECIES_TYPE));

            //copy values ns from c_tilde_basis_function private memory to contigous memory part
            memcpy(&full_ns[rank_array_ind], func.ns,
                   func.rank * sizeof(NS_TYPE));
            //copy values ls from c_tilde_basis_function private memory to contigous memory part
            memcpy(&full_ls[rank_array_ind], func.ls,
                   func.rank * sizeof(LS_TYPE));
            //copy values mus from c_tilde_basis_function private memory to contigous memory part
            memcpy(&full_ms[ms_array_ind], func.ms_combs,
                   func.num_ms_combs *
                   func.rank * sizeof(MS_TYPE));

            //copy values ctildes from c_tilde_basis_function private memory to contigous memory part
            memcpy(&full_c_tildes[coeff_array_ind], func.ctildes,
                   func.num_ms_combs * func.ndensity * sizeof(DOUBLE_TYPE));


            //release memory of each ACECTildeBasisFunction if it is not proxy
            func._clean();

            func.ns = &full_ns[rank_array_ind];
            func.ls = &full_ls[rank_array_ind];
            func.mus = &full_mus[rank_array_ind];
            func.ctildes = &full_c_tildes[coeff_array_ind];
            func.ms_combs = &full_ms[ms_array_ind];
            func.is_proxy = true;

            rank_array_ind += func.rank;
            ms_array_ind += func.rank *
                            func.num_ms_combs;
            coeff_array_ind += func.num_ms_combs * func.ndensity;
        }
    }
}

void fwrite_c_tilde_b_basis_func(FILE *fptr, ACECTildeBasisFunction &func) {
    RANK_TYPE r;
    fprintf(fptr, "ctilde_basis_func: ");
    fprintf(fptr, "rank=%d ndens=%d mu0=%d ", func.rank, func.ndensity, func.mu0);

    fprintf(fptr, "mu=(");
    for (r = 0; r < func.rank; ++r)
        fprintf(fptr, " %d ", func.mus[r]);
    fprintf(fptr, ")\n");

    fprintf(fptr, "n=(");
    for (r = 0; r < func.rank; ++r)
        fprintf(fptr, " %d ", func.ns[r]);
    fprintf(fptr, ")\n");

    fprintf(fptr, "l=(");
    for (r = 0; r < func.rank; ++r)
        fprintf(fptr, " %d ", func.ls[r]);
    fprintf(fptr, ")\n");

    fprintf(fptr, "num_ms=%d\n", func.num_ms_combs);

    for (int m_ind = 0; m_ind < func.num_ms_combs; m_ind++) {
        fprintf(fptr, "<");
        for (r = 0; r < func.rank; ++r)
            fprintf(fptr, " %d ", func.ms_combs[m_ind * func.rank + r]);
        fprintf(fptr, ">: ");
        for (DENSITY_TYPE p = 0; p < func.ndensity; p++)
            fprintf(fptr, " %.18f ", func.ctildes[m_ind * func.ndensity + p]);
        fprintf(fptr, "\n");
    }

}

void ACECTildeBasisSet::save(const string &filename) {
    // TODO: save radbasename to file
    FILE *fptr;
    fptr = fopen(filename.c_str(), "w");
    fprintf(fptr, "lmax=%d\n", lmax);
    fprintf(fptr, "nradbase=%d\n", nradbase);
    fprintf(fptr, "nradmax=%d\n", nradmax);
    fprintf(fptr, "nelements=%d\n", nelements);
    fprintf(fptr, "rankmax=%d\n", rankmax);
    fprintf(fptr, "ndensitymax=%d\n", ndensitymax);
    fprintf(fptr, "cutoffmax=%f\n", cutoffmax);

    fprintf(fptr, "ntot=%d\n", ntot);

    fprintf(fptr, "%ld parameters: ", FS_parameters.size());
    for (int i = 0; i < FS_parameters.size(); ++i) {
        fprintf(fptr, " %f", FS_parameters.at(i));
    }
    fprintf(fptr, "\n");

    //hard-core repulsion
    fprintf(fptr, "core repulsion parameters: ");
    for (SPECIES_TYPE mu_i = 0; mu_i < nelements; ++mu_i)
        for (SPECIES_TYPE mu_j = 0; mu_j < nelements; ++mu_j)
            fprintf(fptr, "%.18f %.18f\n", radial_functions.prehc(mu_i, mu_j), radial_functions.lambdahc(mu_j, mu_j));

    //hard-core energy cutoff repulsion
    fprintf(fptr, "core energy-cutoff parameters: ");
    for (SPECIES_TYPE mu_i = 0; mu_i < nelements; ++mu_i)
        fprintf(fptr, "%.18f %.18f\n", rho_core_cutoffs(mu_i), drho_core_cutoffs(mu_i));

    //elements mapping
    fprintf(fptr, "elements:");
    for (SPECIES_TYPE mu = 0; mu < nelements; ++mu)
        fprintf(fptr, " %s", elements_name[mu].c_str());
    fprintf(fptr, "\n");

    //TODO: radial functions
    //radparameter
    fprintf(fptr, "radparameter=");
    for (SPECIES_TYPE mu_i = 0; mu_i < nelements; ++mu_i)
        for (SPECIES_TYPE mu_j = 0; mu_j < nelements; ++mu_j)
            fprintf(fptr, " %.18f", radial_functions.lambda(mu_i, mu_j));
    fprintf(fptr, "\n");

    fprintf(fptr, "cutoff=");
    for (SPECIES_TYPE mu_i = 0; mu_i < nelements; ++mu_i)
        for (SPECIES_TYPE mu_j = 0; mu_j < nelements; ++mu_j)
            fprintf(fptr, " %.18f", radial_functions.cut(mu_i, mu_j));
    fprintf(fptr, "\n");

    fprintf(fptr, "dcut=");
    for (SPECIES_TYPE mu_i = 0; mu_i < nelements; ++mu_i)
        for (SPECIES_TYPE mu_j = 0; mu_j < nelements; ++mu_j)
            fprintf(fptr, " %.18f", radial_functions.dcut(mu_i, mu_j));
    fprintf(fptr, "\n");

    fprintf(fptr, "crad=");
    for (SPECIES_TYPE mu_i = 0; mu_i < nelements; ++mu_i)
        for (SPECIES_TYPE mu_j = 0; mu_j < nelements; ++mu_j) {
            for (NS_TYPE idx = 1; idx <= nradbase; idx++) {
                for (NS_TYPE nr = 1; nr <= nradmax; nr++) {
                    for (LS_TYPE l = 0; l <= lmax; l++) {
                        fprintf(fptr, " %.18f", radial_functions.crad(mu_i, mu_j, l, nr - 1, idx - 1));
                    }
                    fprintf(fptr, "\n");
                }
            }
        }

    fprintf(fptr, "\n");

    //num_c_tilde_max
    fprintf(fptr, "num_c_tilde_max=%d\n", num_ctilde_max);
    fprintf(fptr, "num_ms_combinations_max=%d\n", num_ms_combinations_max);


    //write total_basis_size and total_basis_size_rank1
    fprintf(fptr, "total_basis_size_rank1: ");
    for (SPECIES_TYPE mu = 0; mu < nelements; ++mu) {
        fprintf(fptr, "%d ", total_basis_size_rank1[mu]);
    }
    fprintf(fptr, "\n");

    for (SPECIES_TYPE mu = 0; mu < nelements; mu++)
        for (SHORT_INT_TYPE func_ind = 0; func_ind < total_basis_size_rank1[mu]; ++func_ind)
            fwrite_c_tilde_b_basis_func(fptr, basis_rank1[mu][func_ind]);

    fprintf(fptr, "total_basis_size: ");
    for (SPECIES_TYPE mu = 0; mu < nelements; ++mu) {
        fprintf(fptr, "%d ", total_basis_size[mu]);
    }
    fprintf(fptr, "\n");

    for (SPECIES_TYPE mu = 0; mu < nelements; mu++)
        for (SHORT_INT_TYPE func_ind = 0; func_ind < total_basis_size[mu]; ++func_ind)
            fwrite_c_tilde_b_basis_func(fptr, basis[mu][func_ind]);


    fclose(fptr);
}

void fread_c_tilde_b_basis_func(FILE *fptr, ACECTildeBasisFunction &func) {
    RANK_TYPE r;
    int res;
    char buf[3][128];

    res = fscanf(fptr, " ctilde_basis_func: ");

    res = fscanf(fptr, "rank=%s ndens=%s mu0=%s ", buf[0], buf[1], buf[2]);
    if (res != 3)
        throw invalid_argument("Could not read C-tilde basis function");

    func.rank = (RANK_TYPE) stol(buf[0]);
    func.ndensity = (DENSITY_TYPE) stol(buf[1]);
    func.mu0 = (SPECIES_TYPE) stol(buf[2]);

    func.mus = new SPECIES_TYPE[func.rank];
    func.ns = new NS_TYPE[func.rank];
    func.ls = new LS_TYPE[func.rank];

    res = fscanf(fptr, " mu=(");
    for (r = 0; r < func.rank; ++r) {
        res = fscanf(fptr, "%s", buf[0]);
        if (res != 1)
            throw invalid_argument("Could not read C-tilde basis function");
        func.mus[r] = (SPECIES_TYPE) stol(buf[0]);
    }
    res = fscanf(fptr, " )"); // ")"

    res = fscanf(fptr, " n=("); // "n="
    for (r = 0; r < func.rank; ++r) {
        res = fscanf(fptr, "%s", buf[0]);
        if (res != 1)
            throw invalid_argument("Could not read C-tilde basis function");

        func.ns[r] = (NS_TYPE) stol(buf[0]);
    }
    res = fscanf(fptr, " )");

    res = fscanf(fptr, " l=(");
    for (r = 0; r < func.rank; ++r) {
        res = fscanf(fptr, "%s", buf[0]);
        if (res != 1)
            throw invalid_argument("Could not read C-tilde basis function");
        func.ls[r] = (NS_TYPE) stol(buf[0]);
    }
    res = fscanf(fptr, " )");

    res = fscanf(fptr, " num_ms=%s\n", buf[0]);
    if (res != 1)
        throw invalid_argument("Could not read C-tilde basis function");

    func.num_ms_combs = (SHORT_INT_TYPE) stoi(buf[0]);

    func.ms_combs = new MS_TYPE[func.rank * func.num_ms_combs];
    func.ctildes = new DOUBLE_TYPE[func.ndensity * func.num_ms_combs];

    for (int m_ind = 0; m_ind < func.num_ms_combs; m_ind++) {
        res = fscanf(fptr, " <");
        for (r = 0; r < func.rank; ++r) {
            res = fscanf(fptr, "%s", buf[0]);
            if (res != 1)
                throw invalid_argument("Could not read C-tilde basis function");
            func.ms_combs[m_ind * func.rank + r] = stoi(buf[0]);
        }
        res = fscanf(fptr, " >:");
        for (DENSITY_TYPE p = 0; p < func.ndensity; p++) {
            res = fscanf(fptr, "%s", buf[0]);
            if (res != 1)
                throw invalid_argument("Could not read C-tilde basis function");
            func.ctildes[m_ind * func.ndensity + p] = stod(buf[0]);
        }
    }
}

void ACECTildeBasisSet::load(const string filename) {
    int res;
    FILE *fptr;
    char buffer[1024], buffer2[1024];
    string radbasename = "ChebExpCos";

    fptr = fopen(filename.c_str(), "r");
    if (fptr == NULL)
        throw invalid_argument("Could not open file %s."+filename);
    res = fscanf(fptr, "lmax=%s", buffer);
    if (res != 1)
        throw invalid_argument(("File '" +filename +"': couldn't read lmax").c_str());
    lmax = stoi(buffer);

    res = fscanf(fptr, " nradbase=");
    res = fscanf(fptr, "%s", buffer);
    if (res != 1)
        throw invalid_argument(("File '" +filename +"': couldn't read nradbase").c_str());
    nradbase = stoi(buffer);

    res = fscanf(fptr, " nradmax=");
    res = fscanf(fptr, "%s", buffer);
    if (res != 1)
        throw invalid_argument(("File '" +filename +"': couldn't read nradmax").c_str());
    nradmax = stoi(buffer);

    res = fscanf(fptr, " nelements=");
    res = fscanf(fptr, "%s", buffer);
    if (res != 1)
        throw invalid_argument(("File '" +filename +"': couldn't read nelements").c_str());
    nelements = stoi(buffer);

    res = fscanf(fptr, " rankmax=");
    res = fscanf(fptr, "%s", buffer);
    if (res != 1)
        throw invalid_argument(("File '" +filename +"': couldn't read rankmax").c_str());
    rankmax = stoi(buffer);

    res = fscanf(fptr, " ndensitymax=");
    res = fscanf(fptr, "%s", buffer);
    if (res != 1)
        throw invalid_argument(("File '" +filename +"': couldn't read ndensitymax").c_str());
    ndensitymax = stoi(buffer);


    res = fscanf(fptr, " cutoffmax=");
    res = fscanf(fptr, "%s", buffer);
    if (res != 1)
        throw invalid_argument(("File '" +filename +"': couldn't read cutoffmax").c_str());
    cutoffmax = stod(buffer);


    res = fscanf(fptr, " ntot=");
    res = fscanf(fptr, "%s", buffer);
    if (res != 1)
        throw invalid_argument(("File '" +filename +"': couldn't read ntot").c_str());
    ntot = stoi(buffer);


    int parameters_size;
    res = fscanf(fptr, "%s parameters:", buffer);
    if (res != 1)
        throw invalid_argument(("File '" +filename +"': couldn't read number of FS_parameters").c_str());
    parameters_size = stoi(buffer);
    FS_parameters.resize(parameters_size);

    spherical_harmonics.init(lmax);
    //TODO: read "radbasename" argument from file
    radial_functions.init(nradbase, lmax, nradmax,
                          ntot,
                          nelements,
                          cutoffmax, radbasename);
    rho_core_cutoffs.init(nelements, "rho_core_cutoffs");
    drho_core_cutoffs.init(nelements, "drho_core_cutoffs");

    for(int i = 0; i < FS_parameters.size(); ++i) {
        res = fscanf(fptr, "%s", buffer);
        if (res != 1)
            throw invalid_argument(("File '" +filename +"': couldn't read  FS_parameters").c_str());
        FS_parameters[i] = stof(buffer);
    }

    //hard-core repulsion
    res = fscanf(fptr, " core repulsion parameters:");
    if (res != 0)
        throw invalid_argument(("File '" +filename +"': couldn't read core repulsion parameters").c_str());
    for (SPECIES_TYPE mu_i = 0; mu_i < nelements; ++mu_i)
        for (SPECIES_TYPE mu_j = 0; mu_j < nelements; ++mu_j) {
            res = fscanf(fptr, "%s %s", buffer, buffer2);
            if (res != 2)
                throw invalid_argument(("File '" +filename +"': couldn't read core repulsion parameters (values)").c_str());
            radial_functions.prehc(mu_i, mu_j) = stod(buffer);
            radial_functions.lambdahc(mu_i, mu_j) = stod(buffer2);
        }

    //hard-core energy cutoff repulsion
    res = fscanf(fptr, " core energy-cutoff parameters:");
    if (res != 0)
        throw invalid_argument(("File '" +filename +"': couldn't read core energy-cutoff parameters").c_str());
    for (SPECIES_TYPE mu_i = 0; mu_i < nelements; ++mu_i) {
        res = fscanf(fptr, "%s %s", buffer, buffer2);
        if (res != 2)
            throw invalid_argument(("File '" +filename +"': couldn't read core energy-cutoff parameters (values)").c_str());
        rho_core_cutoffs(mu_i) = stod(buffer);
        drho_core_cutoffs(mu_i) = stod(buffer2);
    }


    //elements mapping
    elements_name = new string[nelements];
    res = fscanf(fptr, " elements:");
    for (SPECIES_TYPE mu = 0; mu < nelements; ++mu) {
        res = fscanf(fptr, "%s", buffer);
        if (res != 1)
            throw invalid_argument(("File '" +filename +"': couldn't read elements name").c_str());
        elements_name[mu] = buffer;
    }

    //read radial functions parameter

    res = fscanf(fptr, " radparameter=");
    for (SPECIES_TYPE mu_i = 0; mu_i < nelements; ++mu_i)
        for (SPECIES_TYPE mu_j = 0; mu_j < nelements; ++mu_j) {
            res = fscanf(fptr, "%s", buffer);
            if (res != 1)
                throw invalid_argument(("File '" +filename +"': couldn't read radparameter").c_str());
            radial_functions.lambda(mu_i, mu_j) = stod(buffer);
        }


    res = fscanf(fptr, " cutoff=");
    for (SPECIES_TYPE mu_i = 0; mu_i < nelements; ++mu_i)
        for (SPECIES_TYPE mu_j = 0; mu_j < nelements; ++mu_j) {
            res = fscanf(fptr, "%s", buffer);
            if (res != 1)
                throw invalid_argument(("File '" +filename +"': couldn't read cutoff").c_str());
            radial_functions.cut(mu_i, mu_j) = stod(buffer);
        }


    res = fscanf(fptr, " dcut=");
    for (SPECIES_TYPE mu_i = 0; mu_i < nelements; ++mu_i)
        for (SPECIES_TYPE mu_j = 0; mu_j < nelements; ++mu_j) {
            res = fscanf(fptr, " %s", buffer);
            if (res != 1)
                throw invalid_argument(("File '" +filename +"': couldn't read dcut").c_str());
            radial_functions.dcut(mu_i, mu_j) = stod(buffer);
        }


    res = fscanf(fptr, " crad=");
    for (SPECIES_TYPE mu_i = 0; mu_i < nelements; ++mu_i)
        for (SPECIES_TYPE mu_j = 0; mu_j < nelements; ++mu_j)
            for (NS_TYPE idx = 1; idx <= nradbase; idx++)
                for (NS_TYPE nr = 1; nr <= nradmax; nr++)
                    for (LS_TYPE l = 0; l <= lmax; l++) {
                        res = fscanf(fptr, "%s", buffer);
                        if (res != 1)
                            throw invalid_argument(("File '" +filename +"': couldn't read crad").c_str());
                        radial_functions.crad(mu_i, mu_j, l, nr - 1, idx - 1) = stod(buffer);
                    }

    radial_functions.setuplookupRadspline();

    //num_c_tilde_max
    res = fscanf(fptr, " num_c_tilde_max=");
    res = fscanf(fptr, "%s\n", buffer);
    if (res != 1)
        throw invalid_argument(("File '" +filename +"': couldn't read num_c_tilde_max").c_str());
    num_ctilde_max = stol(buffer);

    res = fscanf(fptr, " num_ms_combinations_max=");
    res = fscanf(fptr, "%s", buffer);
    if (res != 1)
        throw invalid_argument(("File '" +filename +"': couldn't read num_ms_combinations_max").c_str());
    num_ms_combinations_max = stol(buffer);

    //read total_basis_size_rank1
    total_basis_size_rank1 = new SHORT_INT_TYPE[nelements];
    basis_rank1 = new ACECTildeBasisFunction *[nelements];
    res = fscanf(fptr, " total_basis_size_rank1: ");


    for (SPECIES_TYPE mu = 0; mu < nelements; ++mu) {
        res = fscanf(fptr, "%s", buffer);
        if (res != 1)
            throw invalid_argument(("File '" +filename +"': couldn't read total_basis_size_rank1").c_str());
        total_basis_size_rank1[mu] = stoi(buffer);
        basis_rank1[mu] = new ACECTildeBasisFunction[total_basis_size_rank1[mu]];
    }
    for (SPECIES_TYPE mu = 0; mu < nelements; mu++)
        for (SHORT_INT_TYPE func_ind = 0; func_ind < total_basis_size_rank1[mu]; ++func_ind) {
            fread_c_tilde_b_basis_func(fptr, basis_rank1[mu][func_ind]);
        }

    //read total_basis_size
    res = fscanf(fptr, " total_basis_size: ");
    total_basis_size = new SHORT_INT_TYPE[nelements];
    basis = new ACECTildeBasisFunction *[nelements];

    for (SPECIES_TYPE mu = 0; mu < nelements; ++mu) {
        res = fscanf(fptr, "%s", buffer);
        if (res != 1)
            throw invalid_argument(("File '" +filename +"': couldn't read total_basis_size").c_str());
        total_basis_size[mu] = stoi(buffer);
        basis[mu] = new ACECTildeBasisFunction[total_basis_size[mu]];
    }
    for (SPECIES_TYPE mu = 0; mu < nelements; mu++)
        for (SHORT_INT_TYPE func_ind = 0; func_ind < total_basis_size[mu]; ++func_ind) {
            fread_c_tilde_b_basis_func(fptr, basis[mu][func_ind]);
        }

    fclose(fptr);
//    spherical_harmonics.init(lmax);
//    //TODO: pass "radbasename" argument
//    radial_functions.init(nradbase, lmax, nradmax,
//                          ntot,
//                          nelements,
//                          cutoffmax, radbasename);
//    rho_core_cutoffs.init(nelements, "rho_core_cutoffs");
//    drho_core_cutoffs.init(nelements, "drho_core_cutoffs");
//    radial_functions.setuplookupRadspline();
    pack_flatten_basis();
}

void ACECTildeBasisSet::compute_array_sizes(ACECTildeBasisFunction **basis_rank1, ACECTildeBasisFunction **basis) {
    //compute arrays sizes
    rank_array_total_size_rank1 = 0;
    //ms_array_total_size_rank1 = rank_array_total_size_rank1;
    coeff_array_total_size_rank1 = 0;

    for (SPECIES_TYPE mu = 0; mu < nelements; ++mu) {
        if (total_basis_size_rank1[mu] > 0) {
            rank_array_total_size_rank1 += total_basis_size_rank1[mu];

            ACEAbstractBasisFunction &func = basis_rank1[mu][0];//TODO: get total density instead of density from first function
            coeff_array_total_size_rank1 += total_basis_size_rank1[mu] * func.ndensity;
        }
    }

    rank_array_total_size = 0;
    coeff_array_total_size = 0;

    ms_array_total_size = 0;
    max_dB_array_size = 0;


    max_B_array_size = 0;

    size_t cur_ms_size = 0;
    size_t cur_ms_rank_size = 0;

    for (SPECIES_TYPE mu = 0; mu < nelements; ++mu) {

        cur_ms_size = 0;
        cur_ms_rank_size = 0;
        for (int func_ind = 0; func_ind < total_basis_size[mu]; ++func_ind) {
            auto &func = basis[mu][func_ind];
            rank_array_total_size += func.rank;
            ms_array_total_size += func.rank * func.num_ms_combs;
            coeff_array_total_size += func.ndensity * func.num_ms_combs;

            cur_ms_size += func.num_ms_combs;
            cur_ms_rank_size += func.rank * func.num_ms_combs;
        }

        if (cur_ms_size > max_B_array_size)
            max_B_array_size = cur_ms_size;

        if (cur_ms_rank_size > max_dB_array_size)
            max_dB_array_size = cur_ms_rank_size;
    }
}

void ACECTildeBasisSet::_clean_basis_arrays() {
    if (basis_rank1 != nullptr)
        for (SPECIES_TYPE mu = 0; mu < nelements; ++mu) {
            delete[] basis_rank1[mu];
            basis_rank1[mu] = nullptr;
        }

    if (basis != nullptr)
        for (SPECIES_TYPE mu = 0; mu < nelements; ++mu) {
            delete[] basis[mu];
            basis[mu] = nullptr;
        }
    delete[] basis;
    basis = nullptr;

    delete[] basis_rank1;
    basis_rank1 = nullptr;
}

//pack into 1D array with all basis functions
void ACECTildeBasisSet::flatten_basis(C_tilde_full_basis_vector2d &mu0_ctilde_basis_vector) {

    _clean_basis_arrays();
    basis_rank1 = new ACECTildeBasisFunction *[nelements];
    basis = new ACECTildeBasisFunction *[nelements];

    delete[] total_basis_size_rank1;
    delete[] total_basis_size;
    total_basis_size_rank1 = new SHORT_INT_TYPE[nelements];
    total_basis_size = new SHORT_INT_TYPE[nelements];


    size_t tot_size_rank1 = 0;
    size_t tot_size = 0;

    for (SPECIES_TYPE mu = 0; mu < this->nelements; ++mu) {
        tot_size = 0;
        tot_size_rank1 = 0;

        for (auto &func: mu0_ctilde_basis_vector[mu]) {
            if (func.rank == 1) tot_size_rank1 += 1;
            else tot_size += 1;
        }

        total_basis_size_rank1[mu] = tot_size_rank1;
        basis_rank1[mu] = new ACECTildeBasisFunction[tot_size_rank1];

        total_basis_size[mu] = tot_size;
        basis[mu] = new ACECTildeBasisFunction[tot_size];
    }


    for (SPECIES_TYPE mu = 0; mu < this->nelements; ++mu) {
        size_t ind_rank1 = 0;
        size_t ind = 0;

        for (auto &func: mu0_ctilde_basis_vector[mu]) {
            if (func.rank == 1) { //r=0, rank=1
                basis_rank1[mu][ind_rank1] = func;
                ind_rank1 += 1;
            } else {  //r>0, rank>1
                basis[mu][ind] = func;
                ind += 1;
            }
        }

    }
}