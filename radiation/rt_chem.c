#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <gsl/gsl_math.h>

#include "../allvars.h"
#include "../proto.h"


#ifdef RT_CHEM_PHOTOION

/* return photon number density in physical code units */
double rt_return_photon_number_density(int i, int k)
{
    return SphP[i].E_gamma[k] * (SphP[i].Density*All.cf_a3inv/P[i].Mass) / (rt_nu_eff_eV[k]*ELECTRONVOLT_IN_ERGS / All.UnitEnergy_in_cgs * All.HubbleParam);
}


void rt_get_sigma(void)
{
    /* first initialize all bands, so non-ionizing bands don't cause problems */
    int k;
    for(k=0;k<N_RT_FREQ_BINS;k++)
    {
        nu[k] = 0; rt_nu_eff_eV[k] = nu[k]; 
        G_HI[k]=G_HeI[k]=G_HeII[k]=rt_sigma_HI[k]=rt_sigma_HeI[k]=rt_sigma_HeII[k]=precalc_stellar_luminosity_fraction[k]=0;
    }
    double fac = 1.0 / All.UnitLength_in_cm / All.UnitLength_in_cm * All.HubbleParam * All.HubbleParam;
    
#ifndef RT_PHOTOION_MULTIFREQUENCY
    /* just the hydrogen ionization bin */
    rt_sigma_HI[RT_FREQ_BIN_H0] = 6.3e-18 * fac; // cross-section (blackbody-weighted) for photons
    nu[RT_FREQ_BIN_H0] = 13.6; // minimum frequency [in eV] of photons of interest
    rt_nu_eff_eV[RT_FREQ_BIN_H0] = 27.2; // typical blackbody-weighted frequency [in eV] of photons of interest: to convert energies to numbers
    G_HI[RT_FREQ_BIN_H0] = (rt_nu_eff_eV[RT_FREQ_BIN_H0]-13.6)*ELECTRONVOLT_IN_ERGS / All.UnitEnergy_in_cgs * All.HubbleParam; // absorption cross-section weighted photon energy in code units
#else
    
    /* now we use the multi-bin spectral information */
#define N_BINS_FOR_IONIZATION 4
    double nu_vec[N_BINS_FOR_IONIZATION] = {13.6, 24.6, 54.4, 70.0};
    int i_vec[N_BINS_FOR_IONIZATION] = {RT_FREQ_BIN_H0, RT_FREQ_BIN_He0, RT_FREQ_BIN_He1, RT_FREQ_BIN_He2};
    
    int i, j, integral=10000;
    double e, d_nu, e_start, e_end, sum_HI_sigma=0, sum_HI_G=0, hc=C*PLANCK, I_nu, sig, f, fac_two, T_eff, sum_egy_allbands=0;
#if defined(FLAG_NOT_IN_PUBLIC_CODE) || defined(GALSF)
    T_eff = 4.0e4;
#else 
    T_eff = All.star_Teff;
#endif
    fac_two = ELECTRONVOLT_IN_ERGS / All.UnitEnergy_in_cgs * All.HubbleParam;
#ifdef RT_CHEM_PHOTOION_HE
    double sum_HeI_sigma=0, sum_HeII_sigma=0, sum_HeI_G=0, sum_HeII_G=0;
#endif    
    for(k = 0; k < N_BINS_FOR_IONIZATION; k++)
    {
        i = i_vec[k];
        e_start = nu[i] = nu_vec[k];
        if(k==N_BINS_FOR_IONIZATION-1) {e_end = 500.;} else {e_end = nu_vec[k+1];} 
        d_nu = (e_end - e_start) / (float)(integral - 1);
        rt_sigma_HI[i] = G_HI[i] = rt_nu_eff_eV[i] = 0.0;
        sum_HI_sigma = sum_HI_G = 0.0;
#ifdef RT_CHEM_PHOTOION_HE
        rt_sigma_HeI[i] = rt_sigma_HeII[i] = G_HeI[i] = G_HeII[i] = 0.0;
        sum_HeI_sigma = sum_HeII_sigma = sum_HeI_G = sum_HeII_G = 0.0;
#endif
        double n_photon_sum = 0.0, sum_energy = 0.0;
        for(j = 0; j < integral; j++)
        {
            e = e_start + j * d_nu;
            I_nu = 2.0 * pow(e * ELECTRONVOLT_IN_ERGS, 3) / (hc * hc) / (exp(e * ELECTRONVOLT_IN_ERGS / (BOLTZMANN * T_eff)) - 1.0);
            sum_energy += d_nu * I_nu;
            n_photon_sum += d_nu * I_nu / e;
            if(nu[i] >= 13.6)
            {
                f = sqrt((e / 13.6) - 1.0);
                if(j <= 13.6) {sig = 6.3e-18;} else {sig = 6.3e-18 * pow(13.6 / e, 4) * exp(4 - (4 * atan(f) / f)) / (1.0 - exp(-2 * M_PI / f));}
                rt_sigma_HI[i] += d_nu * sig * I_nu / e;
                sum_HI_sigma += d_nu * I_nu / e;
                G_HI[i] += d_nu * sig * (e - 13.6) * I_nu / e;
                sum_HI_G += d_nu * sig * I_nu / e;
            }
#ifdef RT_CHEM_PHOTOION_HE
            if(nu[i] >= 24.6)
            {
                f = sqrt((e / 24.6) - 1.0);
                if(j <= 24.6) {sig = 7.83e-18;} else {sig = 7.83e-18 * pow(24.6 / e, 4) * exp(4 - (4 * atan(f) / f)) / (1.0 - exp(-2 * M_PI / f));}
                rt_sigma_HeI[i] += d_nu * sig * I_nu / e;
                sum_HeI_sigma += d_nu * I_nu / e;
                G_HeI[i] += d_nu * sig * (e - 24.6) * I_nu / e;
                sum_HeI_G += d_nu * sig * I_nu / e;
            }
            if(nu[i] >= 54.4)
            {
                f = sqrt((e / 54.4) - 1.0);
                if(j <= 54.4) {sig = 1.58e-18;} else {sig = 1.58e-18 * pow(54.4 / e, 4) * exp(4 - (4 * atan(f) / f)) / (1.0 - exp(-2 * M_PI / f));}
                rt_sigma_HeII[i] += d_nu * sig * I_nu / e;
                sum_HeII_sigma += d_nu * I_nu / e;
                G_HeII[i] += d_nu * sig * (e - 54.4) * I_nu / e;
                sum_HeII_G += d_nu * sig * I_nu / e;
            }
#endif
        }
        rt_nu_eff_eV[i] = sum_energy / n_photon_sum;
        precalc_stellar_luminosity_fraction[i] = sum_energy;

        if(nu[i] >= 13.6)
        {
            rt_sigma_HI[i] *= fac / sum_HI_sigma;
            G_HI[i] *= fac_two / sum_HI_G;
        }
#ifdef RT_CHEM_PHOTOION_HE
        if(nu[i] >= 24.6)
        {
            rt_sigma_HeI[i] *= fac / sum_HeI_sigma;
            G_HeI[i] *= fac_two / sum_HeI_G;
        }
        if(nu[i] >= 54.4)
        {
            rt_sigma_HeII[i] *= fac / sum_HeII_sigma;
            G_HeII[i] *= fac_two / sum_HeII_G;
        }
#endif
         sum_egy_allbands += sum_energy;
    }

    for(i = 0; i < N_BINS_FOR_IONIZATION; i++){precalc_stellar_luminosity_fraction[i] /= sum_egy_allbands;}
    
    if(ThisTask == 0) {for(i = 0; i < N_RT_FREQ_BINS; i++) {printf("%g %g | %g %g | %g %g\n",rt_sigma_HI[i]/fac, G_HI[i]/fac_two,rt_sigma_HeI[i]/fac, G_HeI[i]/fac_two,rt_sigma_HeII[i]/fac, G_HeII[i]/fac_two);}}
#endif
}




#ifndef RT_PHOTOION_MULTIFREQUENCY
void rt_update_chemistry(void)
{
    int i;
    double nH, temp, molecular_weight, rho, nHII, dtime, c_light, A, B, CC, n_photons_vol, alpha_HII, gamma_HI, fac;
#ifdef RT_CHEM_PHOTOION_HE
    double alpha_HeII, alpha_HeIII, gamma_HeI, gamma_HeII, nHeII, nHeIII, D, E, F, G, J, L, y_fac;
#endif
    
    fac = All.UnitTime_in_s / pow(All.UnitLength_in_cm, 3) * All.HubbleParam * All.HubbleParam;
    c_light = C / All.UnitVelocity_in_cm_per_s;
    
    for(i = FirstActiveParticle; i >= 0; i = NextActiveParticle[i])
        if(P[i].Type == 0)
        {
            dtime = (P[i].TimeBin ? (((integertime) 1) << P[i].TimeBin) : 0) * All.Timebase_interval / All.cf_hubble_a;
            rho = SphP[i].Density * All.cf_a3inv;
            nH = HYDROGEN_MASSFRAC * rho / PROTONMASS * All.UnitMass_in_g / All.HubbleParam;
            molecular_weight = 4 / (1 + 3 * HYDROGEN_MASSFRAC + 4 * HYDROGEN_MASSFRAC * SphP[i].Ne);
#ifdef RT_ILIEV_TEST1
            temp = 1.0e4;
#else
            temp = GAMMA_MINUS1 * SphP[i].InternalEnergyPred * molecular_weight * PROTONMASS / All.UnitMass_in_g * All.HubbleParam / BOLTZMANN * All.UnitEnergy_in_cgs / All.HubbleParam;
#endif
            /* collisional ionization rate */
            gamma_HI = 5.85e-11 * sqrt(temp) * exp(-157809.1 / temp) / (1.0 + sqrt(temp / 1e5)) * fac;
            /* alpha_B recombination coefficient */
            alpha_HII = 2.59e-13 * pow(temp / 1e4, -0.7) * fac;
            n_photons_vol = rt_return_photon_number_density(i,RT_FREQ_BIN_H0);
            /* number of photons should be positive */
            if(n_photons_vol < 0 || isnan(n_photons_vol))
            {
                printf("NEGATIVE n_photons_per_volume: %g %d %d \n", n_photons_vol, i, ThisTask);
                printf("E_gamma %g mass %g All.cf_a3inv %g \n", SphP[i].E_gamma[0], P[i].Mass, All.cf_a3inv);
                endrun(111);
            }
            
            A = dtime * gamma_HI * nH * SphP[i].Ne;
            B = dtime * c_light * n_photons_vol * rt_sigma_HI[RT_FREQ_BIN_H0];
            CC = dtime * alpha_HII * nH * SphP[i].Ne;
            
            /* semi-implicit scheme for ionization */
            nHII = SphP[i].HII + B + A;
            nHII /= 1.0 + B + CC + A;
            if(nHII < 0 || nHII > 1 || isnan(nHII))
            {
                printf("ERROR nHII %g \n", nHII);
                endrun(333);
            }
            SphP[i].Ne = nHII;
            SphP[i].HII = nHII;
            SphP[i].HI = 1.0 - nHII;
            
#ifdef RT_CHEM_PHOTOION_HE
            /* collisional ionization rate */
            gamma_HeI = 2.38e-11 * sqrt(temp) * exp(-285335.4 / temp) / (1.0 + sqrt(temp / 1e5)) * fac;
            gamma_HeII = 5.68e-12 * sqrt(temp) * exp(-631515 / temp) / (1.0 + sqrt(temp / 1e5)) * fac;
            /* alpha_B recombination coefficient */
            alpha_HeII = 1.5e-10 * pow(temp, -0.6353) * fac;
            alpha_HeIII = 3.36e-10 / sqrt(temp) * pow(temp / 1e3, -0.2) / (1.0 + pow(temp / 1e6, 0.7)) * fac;
            SphP[i].Ne += SphP[i].HeII + 2.0 * SphP[i].HeIII;
            D = dtime * gamma_HeII * nH * SphP[i].Ne;
            E = dtime * alpha_HeIII * nH * SphP[i].Ne;
            F = dtime * gamma_HeI * nH * SphP[i].Ne;
            J = dtime * alpha_HeII * nH * SphP[i].Ne;
            G = 0.0;
            L = 0.0;
            y_fac = (1.0-HYDROGEN_MASSFRAC)/4.0/HYDROGEN_MASSFRAC;
            
            nHeII = SphP[i].HeII / y_fac;
            nHeIII = SphP[i].HeIII / y_fac;
            nHeII = nHeII + G + F - ((G + F - E) / (1.0 + E)) * nHeIII;
            nHeII /= 1.0 + G + F + D + J + ((G + F - E) / (1.0 + E)) * (D + L);
            if(nHeII < 0 || nHeII > 1 || isnan(nHeII))
            {
                printf("ERROR nHeII %g \n", nHeII);
                endrun(333);
            }
            nHeIII = nHeIII + (D + L) * nHeII;
            nHeIII /= 1.0 + E;
            if(nHeIII < 0 || nHeIII > 1 || isnan(nHeIII))
            {
                printf("ERROR nHeIII %g \n", nHeIII);
                endrun(333);
            }
            SphP[i].Ne = SphP[i].HII + nHeII + 2.0 * nHeIII;
            nHeII *= y_fac;
            nHeIII *= y_fac;
            SphP[i].Ne = SphP[i].HII + nHeII + 2.0 * nHeIII;
            SphP[i].HeII = nHeII;
            SphP[i].HeIII = nHeIII;
            SphP[i].HeI = y_fac - SphP[i].HeII - SphP[i].HeIII;
            if(SphP[i].HeI < 0) {SphP[i].HeI = 0.0;}
            if(SphP[i].HeI > y_fac) {SphP[i].HeI = y_fac;}
#endif
        }
}

#else

/*---------------------------------------------------------------------*/
/* if the multi-frequency scheme is used*/
/*---------------------------------------------------------------------*/
void rt_update_chemistry(void)
{
    int i, j;
    double nH, temp, molecular_weight, rho, nHII, c_light, n_photons_vol, dtime, A, B, CC, alpha_HII, gamma_HI, fac, k_HI;
#ifdef RT_CHEM_PHOTOION_HE
    double alpha_HeII, alpha_HeIII, gamma_HeI, gamma_HeII, nHeII, nHeIII, D, E, F, G, J, L, k_HeI, k_HeII, y_fac;
#endif
    
    fac = All.UnitTime_in_s / pow(All.UnitLength_in_cm, 3) * All.HubbleParam * All.HubbleParam;
    c_light = C / All.UnitVelocity_in_cm_per_s;
    
    for(i = FirstActiveParticle; i >= 0; i = NextActiveParticle[i])
        if(P[i].Type == 0)
        {
            /* get the photo-ionization rates*/
            k_HI = 0.0;
#ifdef RT_CHEM_PHOTOION_HE
            k_HeI = k_HeII = 0.0;
#endif
            for(j = 0; j < N_RT_FREQ_BINS; j++)
            {
                n_photons_vol = rt_return_photon_number_density(i,j);
                if(nu[j] >= 13.6) {k_HI += c_light * rt_sigma_HI[j] * n_photons_vol;}
#ifdef RT_CHEM_PHOTOION_HE
                if(nu[j] >= 24.6) {k_HeI += c_light * rt_sigma_HeI[j] * n_photons_vol;}
                if(nu[j] >= 54.4) {k_HeII += c_light * rt_sigma_HeII[j] * n_photons_vol;}
#endif
            }
            
            dtime = (P[i].TimeBin ? (((integertime) 1) << P[i].TimeBin) : 0) * All.Timebase_interval / All.cf_hubble_a;
            rho = SphP[i].Density * All.cf_a3inv;
            nH = HYDROGEN_MASSFRAC * rho / PROTONMASS * All.UnitMass_in_g / All.HubbleParam;
            molecular_weight = 4 / (1 + 3 * HYDROGEN_MASSFRAC + 4 * HYDROGEN_MASSFRAC * SphP[i].Ne);
            temp = GAMMA_MINUS1 * SphP[i].InternalEnergyPred * molecular_weight * PROTONMASS /
            All.UnitMass_in_g * All.HubbleParam / BOLTZMANN * All.UnitEnergy_in_cgs / All.HubbleParam;
            /* collisional ionization rate */
            gamma_HI = 5.85e-11 * sqrt(temp) * exp(-157809.1 / temp) / (1.0 + sqrt(temp / 1e5)) * fac;
            /* alpha_B recombination coefficient */
            alpha_HII = 2.59e-13 * pow(temp / 1e4, -0.7) * fac;
            
            A = dtime * gamma_HI * nH * SphP[i].Ne;
            B = dtime * k_HI;
            CC = dtime * alpha_HII * nH * SphP[i].Ne;
            
            /* semi-implicit scheme for ionization */
            nHII = SphP[i].HII + B + A;
            nHII /= 1.0 + B + CC + A;
            if(nHII < 0 || nHII > 1 || isnan(nHII))
            {
                printf("ERROR nHII %g \n", nHII);
                printf("HII %g \n", SphP[i].HII);
                printf("B %g CC %g A %g \n",B,CC,A);
                printf("alpha HII %g \n",alpha_HII);
                printf("nH %g \n",nH);
                printf("fac %g \n",fac);
                printf("temp %g \n",temp);
                printf("pressure %g \n",SphP[i].Pressure);
                printf("kHI %g \n",k_HI);
                printf("E_gamma %g \n",SphP[i].E_gamma[0]);
                endrun(333);
            }
            SphP[i].Ne = nHII;
            SphP[i].HII = nHII;
            SphP[i].HI = 1.0 - nHII;
            
#ifdef RT_CHEM_PHOTOION_HE
            /* collisional ionization rate */
            gamma_HeI = 2.38e-11 * sqrt(temp) * exp(-285335.4 / temp) / (1.0 + sqrt(temp / 1e5)) * fac;
            gamma_HeII = 5.68e-12 * sqrt(temp) * exp(-631515 / temp) / (1.0 + sqrt(temp / 1e5)) * fac;
            /* alpha_B recombination coefficient */
            alpha_HeII = 1.5e-10 * pow(temp, -0.6353) * fac;
            alpha_HeIII = 3.36e-10 / sqrt(temp) * pow(temp / 1e3, -0.2) / (1.0 + pow(temp / 1e6, 0.7)) * fac;
            SphP[i].Ne += SphP[i].HeII +  2.0 * SphP[i].HeIII;
            
            D = dtime * gamma_HeII * nH * SphP[i].Ne;
            E = dtime * alpha_HeIII * nH * SphP[i].Ne;
            F = dtime * gamma_HeI * nH * SphP[i].Ne;
            J = dtime * alpha_HeII * nH * SphP[i].Ne;
            G = dtime * k_HeI;
            L = dtime * k_HeII;
            
            y_fac = (1.0-HYDROGEN_MASSFRAC)/4.0/HYDROGEN_MASSFRAC;
            nHeII = SphP[i].HeII / y_fac;
            nHeIII = SphP[i].HeIII / y_fac;
            nHeII = nHeII + G + F - ((G + F - E) / (1.0 + E)) * nHeIII;
            nHeII /= 1.0 + G + F + D + J + ((G + F - E) / (1.0 + E)) * (D + L);
            if(nHeII < 0 || nHeII > 1 || isnan(nHeII))
            {
                printf("ERROR nHeII %g %g %g\n", nHeII, temp, k_HeI);
                endrun(333);
            }
            nHeIII = nHeIII + (D + L) * nHeII;
            nHeIII /= 1.0 + E;
            if(nHeIII < 0 || nHeIII > 1 || isnan(nHeIII))
            {
                printf("ERROR nHeIII %g %g %g\n", nHeIII, temp, k_HeII);
                endrun(333);
            }
            nHeII *= y_fac;
            nHeIII *= y_fac;
            SphP[i].Ne = SphP[i].HII + nHeII + 2.0 * nHeIII;
            SphP[i].HeII = nHeII;
            SphP[i].HeIII = nHeIII;
            SphP[i].HeI = y_fac - SphP[i].HeII - SphP[i].HeIII;
            if(SphP[i].HeI < 0) {SphP[i].HeI = 0.0;}
            if(SphP[i].HeI > y_fac) {SphP[i].HeI = y_fac;}
#endif
        }
}
#endif


void rt_write_chemistry_stats(void)
{
#ifndef IO_REDUCED_MODE
    int i;
    double rho, total_nHI, total_V, total_nHI_all, total_V_all;
    total_nHI = 0.0; total_V = 0.0;
#ifndef RT_PHOTOION_MULTIFREQUENCY
    double total_ng, total_ng_all, n_photons_vol;
    total_ng = 0.0;
#endif
#ifdef RT_CHEM_PHOTOION_HE
    double total_nHeI, total_nHeI_all;
    double total_nHeII, total_nHeII_all;
    total_nHeI = total_nHeII = 0.0;
#endif
    
    for(i = 0; i < N_gas; i++)
        if(P[i].Type == 0)
        {
            rho = SphP[i].Density * All.cf_a3inv;
#ifndef RT_PHOTOION_MULTIFREQUENCY
            n_photons_vol = rt_return_photon_number_density(i,RT_FREQ_BIN_H0);
            total_ng += n_photons_vol / 1e53 * P[i].Mass / rho;
#endif
#ifdef RT_CHEM_PHOTOION_HE
            total_nHeI += SphP[i].HeI * P[i].Mass / rho;
            total_nHeII += SphP[i].HeII * P[i].Mass / rho;
#endif
            total_nHI += SphP[i].HI * P[i].Mass / rho;
            total_V += P[i].Mass / rho;
        }
    MPI_Allreduce(&total_nHI, &total_nHI_all, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
    MPI_Allreduce(&total_V, &total_V_all, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
#ifndef RT_PHOTOION_MULTIFREQUENCY
    MPI_Allreduce(&total_ng, &total_ng_all, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
#endif
#ifdef RT_CHEM_PHOTOION_HE
    MPI_Allreduce(&total_nHeI, &total_nHeI_all, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
    MPI_Allreduce(&total_nHeII, &total_nHeII_all, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
#endif
    
    if(ThisTask == 0)
    {
        if(All.Time == All.TimeBegin)
        {
            fprintf(FdRad, "time, nHI");
#ifndef RT_PHOTOION_MULTIFREQUENCY
            fprintf(FdRad, ", N_photons");
#endif
#ifdef RT_CHEM_PHOTOION_HE
            fprintf(FdRad, ", nHeI, nHeII \n");
#else
            fprintf(FdRad, "\n");
#endif
        }
        fprintf(FdRad, "%g %g ", All.Time, total_nHI_all / total_V_all);
#ifndef RT_PHOTOION_MULTIFREQUENCY
        fprintf(FdRad, "%g ", total_ng_all / total_V_all);
#endif
#ifdef RT_CHEM_PHOTOION_HE
        fprintf(FdRad, "%g %g\n", total_nHeI_all / total_V_all, total_nHeII_all / total_V_all);
#else
        fprintf(FdRad, "\n");
#endif
        fflush(FdRad);
    }
#endif
}

#endif
