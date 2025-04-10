/*
  Copyright (C) 2011 - 2024 by the authors of the ASPECT code.

  This file is part of ASPECT.

  ASPECT is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2, or (at your option)
  any later version.

  ASPECT is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with ASPECT; see the file doc/COPYING.  If not see
  <http://www.gnu.org/licenses/>.
*/



#ifndef _aspect_boundary_temperature_dynamic_core_h
#define _aspect_boundary_temperature_dynamic_core_h

#include <aspect/boundary_temperature/interface.h>
#include <aspect/simulator_access.h>

namespace aspect
{
  namespace BoundaryTemperature
  {

    /**
     * Data structure for core energy balance calculation
     */
    namespace internal
    {
      struct CoreData
      {
        /**
         * Energy for specific heat, radioactive heating, gravitational contribution,
         * adiabatic contribution, and latent heat. These variables are updated each time step.
         */
        double Qs,Qr,Qg,Qk,Ql;

        /**
         * Entropy for specific heat, radioactive heating, gravitational contribution,
         * adiabatic contribution, latent heat, and heat solution. These entropy terms are not
         * used for solving the core evolution. However, the total excess entropy
         * (dE = Es*dT/dt+Er+El*dR/dt+Eg*dR/dt+Eh*dR/dt-Ek) is useful to determine the core is active or not.
         * For dE>0, the core is likely to be active and generating magnetic field. These variables are updated each time step as well.
         */
        double Es,Er,Eg,Ek,El,Eh;

        /**
         * Parameters for core evolution
         * Ri     inner core radius
         * Ti     core-mantle boundary (CMB) temperature
         * Xi     light component concentration in liquid core
         */
        double Ri,Ti,Xi;

        /**
         * Core-mantle boundary heat flux (Q) and core radioactive heating rate (H)
         */
        double Q,H;

        /**
         * Time step for core energy balance solver
         */
        double dt;

        /**
         * The changing rate of inner core radius, CMB temperature, and light component
         * concentration.
         */
        double dR_dt,dT_dt,dX_dt;

        /**
         * Other energy source into the core
         */
        double Q_OES;

        bool is_initialized;
      };
    }


    /**
     * A class that implements a temperature boundary condition for a spherical
     * shell geometry in which the temperature at the outer surfaces are constant
     * and the core-mantle boundaries (CMB) temperature is calculated by core energy balance.
     * The formulation of the core energy balance is from \cite NPB+04 .
     * @ingroup BoundaryTemperatures
     */
    template <int dim>
    class DynamicCore : public Interface<dim>, public aspect::SimulatorAccess<dim>
    {
      public:
        /**
         * Constructor
         */
        DynamicCore();

        /**
         * This function update the core-mantle boundary (CMB) temperature by
         * the core energy balance solver using the core-mantle boundary heat flux.
         */
        void
        update() override;

        /**
         * Pass core data to other modules
         */
        const internal::CoreData &
        get_core_data() const;

        /**
         * Check if other energy source in the core is in use. The 'other energy source' is used for external core energy source.
         * For example if someone want to test the early lunar core powered by precession
         * (Dwyer, C. A., et al. (2011). "A long-lived lunar dynamo driven by continuous mechanical stirring." Nature 479(7372): 212-214.)
         */
        bool
        is_OES_used() const;

        /**
         * Return the temperature that is to hold at a particular location on the
         * boundary of the domain. This function returns the temperatures
         * at the inner and outer boundaries.
         *
         * @param boundary_indicator The boundary indicator of the part of the boundary
         *   of the domain on which the point is located at which we are requesting the
         *   temperature.
         * @param location The location of the point at which we ask for the temperature.
         */
        double
        boundary_temperature (const types::boundary_id boundary_indicator,
                              const Point<dim> &location) const override;

        /**
         * Return the minimal temperature on that part of the boundary
         * on which Dirichlet conditions are posed.
         *
         * This value is used in computing dimensionless numbers such as the
         * Nusselt number indicating heat flux.
         */
        double
        minimal_temperature (const std::set<types::boundary_id> &fixed_boundary_ids) const override;

        /**
         * Return the maximal temperature on that part of the boundary
         * on which Dirichlet conditions are posed.
         *
         * This value is used in computing dimensionless numbers such as the
         * Nusselt number indicating heat flux.
         */
        double
        maximal_temperature (const std::set<types::boundary_id> &fixed_boundary_ids) const override;

        /**
         * Declare the parameters this class takes through input files.
         * This class declares the inner and outer boundary temperatures.
         */
        static
        void
        declare_parameters (ParameterHandler &prm);

        /**
         * Read the parameters this class declares from the parameter
         * file.
         */
        void
        parse_parameters (ParameterHandler &prm) override;

      private:

        /**
         * Data for core energy balance
         * it get updated each time step.
         */
        internal::CoreData core_data;

        /**
         * Temperature at the inner boundary.
         */
        double inner_temperature;

        /**
         * Temperatures at the outer boundaries.
         */
        double outer_temperature;

        /**
         * Initial CMB temperature changing rate
         */
        double init_dT_dt;

        /**
         * Initial inner core radius changing rate
         */
        double init_dR_dt;

        /**
         * Initial light composition changing rate
         */
        double init_dX_dt;

        /**
         * Flag for determining the initial call for update().
         */
        bool is_first_call;

        /**
         * Core radius
         */
        double Rc;

        /**
         * (Heat capacity) * density
         */
        double CpRho;

        /**
         * Initial light composition concentration
         */
        double X_init;

        /**
         * Partition coefficient of the light element
         */
        double Delta;

        /**
         * Gravitational acceleration
         */
        double g;

        /**
         * Pressure at the core mantle boundary
         */
        double P_CMB;

        /**
         * Pressure at the center of the core
         */
        double P_Core;

        /**
         * Parameters for core solidus following:
         * if not dependent on composition
         *   Tm(p)= Tm0*(1-Theta)*(1+Tm1*p+Tm2*p^2)
         * if depend on composition X
         *   Tm(p)= Tm0*(1-Theta*X)*(1+Tm1*p+Tm2*p^2)
         */
        double Tm0;
        double Tm1;
        double Tm2;
        double Theta;
        bool composition_dependency;

        /**
         * If using the Fe-FeS system solidus from Buono & Walker (2011) instead.
         */
        bool use_bw11;

        //Variables for formulation of \cite NPB+04
        /**
         * Compressibility at zero pressure
         */
        double K0;

        /**
         * Thermal expansivity
         */
        double Alpha;

        /**
         * Density at zero pressure
         */
        double Rho_0;

        /**
         * Density at the center of the planet
         */
        double Rho_cen;

        /**
         * Latent heat of fusion
         */
        double Lh;

        /**
         * Heat of reaction
         */
        double Rh;

        /**
         * Compositional expansion coefficient
         */
        double Beta_c;

        /**
         * Heat conductivity of the core
         */
        double k_c;

        /**
         * Heat capacity
         */
        double Cp;

        /**
         * Number of radioheating element in core
         */
        unsigned int n_radioheating_elements;

        /**
         * Heating rates of different elements
         */
        std::vector<double> heating_rate;

        /**
         * Half life of different elements
         */
        std::vector<double> half_life;

        /**
         * Initial concentration of different elements
         */
        std::vector<double> initial_concentration;

        /**
         * Two length scales in \cite NPB+04 .
         */
        double L;
        double D;

        /**
         * Mass of the core
         */
        double Mc;

        /**
         * Max iterations for the core energy balance solver.
         */
        unsigned int max_steps;

        /**
         * Temperature correction value for adiabatic
         */
        double dTa;

        /**
         * Other energy source into the core, e.g. the mechanical stirring of the moon.
         */
        std::string name_OES;
        struct str_data_OES
        {
          double t;
          double w;
        };
        std::vector<struct str_data_OES> data_OES;
        void read_data_OES();
        double get_OES(double t) const;

        /**
         * Solve core energy balance for each time step.
         * When solving the change in core-mantle boundary temperature @p T, inner core radius @p R, and
         *    light element (e.g. S, O, Si) composition @p X, the following relations have to be respected:
         * 1. At the inner core boundary the adiabatic temperature should be equal to solidus temperature
         * 2. The following energy production rates should be balanced in the core:
         *    Heat flux at core-mantle boundary         Q
         *    Specific heat                             Qs*dT/dt
         *    Radioactive heating                       Qr
         *    Gravitational contribution                Qg*dR/dt
         *    Latent heat                               Ql*dR/dt
         *    So that         Q+Qs*dT/dt+Qr+Qg*dR/dt*Ql*dR/dt=0
         * 3. The light component composition X depends on inner core radius (See function get_X() ),
         *    and core solidus may dependent on X as well.
         *    This becomes a small nonlinear problem. Directly iterate through the above three equations doesn't
         *    converge well. Alternatively we solve the inner core radius using the bisection method.
         *
         *    At the conditions of the Earth's core, an inner core is forming at the center of the Earth and surrounded by a liquid outer core.
         *    However, the core solidus is influenced by light components (e.g. S) and its slope is very close to an adiabat. So there is an alternative
         *    scenario that the crystallization happens first at the core mantle boundary instead of at the center, which is called a 'snowing core'
         *    (Stewart, A. J., et al. (2007). "Mars: a new core-crystallization regime." Science 316(5829): 1323-1325.). This also
         *    provides a valid solution for the solver. The return value of the function is true for a 'normal core', and false for 'snowing core'.
         *    TODO: The current code is only able to treat the normal core scenario, treating 'snowing core' scenario may be possible and could be added.
         */
        bool solve_time_step(double &X, double &T, double &R) const;

        /**
         * Compute the difference between solidus and adiabatic temperature at inner
         * core boundary for a given inner core radius @p r.
         */
        double get_dT(const double r) const;

        /**
         * Use energy balance to calculate core mantle boundary temperature
         * with a given inner core radius @p r.
         */
        double get_Tc(const double r) const;

        /**
         * Get the solidus temperature at inner core boundary
         * with a given inner core radius @p r.
         */
        double get_Ts(const double r) const;

        /**
         * Compute the core solidus at a given light element concentration @p X (in wt.%)
         * and pressure @p pressure.
         */
        double get_solidus(const double X, const double pressure) const;

        /**
         * Get initial inner core radius with given initial core mantle temperature
         * @p T.
         */
        double get_initial_Ri(const double T) const;

        /**
         * Get the light element concentration (in wt.%) in the outer core from given
         * inner core radius @p r.
         */
        double get_X(const double r) const;

        /**
         * Compute the core mass inside a certain radius @p r.
         */
        double get_mass(const double r) const;

        /**
         * Calculate Sn(B,R), referring to \cite NPB+04 .
         */
        double fun_Sn(const double B, const double R, const unsigned int n) const;

        /**
         * Calculate density at given radius @p r.
         */
        double get_rho(const double r) const;

        /**
         * Calculate gravitational acceleration at given radius @p r.
         */
        double get_g(const double r) const;

        /**
         * Calculate the core temperature at given radius @p r and
         * temperature at CMB @p Tc.
         */
        double get_T(const double Tc, const double r) const;

        /**
         * Calculate pressure at given radius @p r
         */
        double get_pressure(const double r) const;

        /**
         * Calculate the gravitational potential at given radius @p r
         */
        double get_gravity_potential(const double r) const;

        /**
         * Calculate energy (@p Qs) and entropy (@p Es) change rate factor
         * (regarding the core cooling rated Tc/dt) for a given core-mantle boundary (CMB)
         * temperature @p Tc
         */
        void get_specific_heating(const double Tc, double &Qs, double &Es) const;

        /**
         * Calculate energy (@p Qr) and entropy (@p Er) change rate factor (regarding the
         * radioactive heating rate H) for a given CMB temperature @p Tc
         */
        void get_radio_heating(const double Tc, double &Qr, double &Er) const;

        /**
         * Calculate energy (@p Qg) and entropy (@p Eg) change rate factor
         * (regarding the inner core growth rate dR/dt) for a given
         * @p Tc (CMB temperature), @p r (inner core radius), and @p X
         * (light element concentration)
         */
        void get_gravity_heating(const double Tc, const double r, const double X, double &Qg, double &Eg) const;

        /**
         * Calculate energy (@p Qk) and entropy (@p Ek) change rate factor
         * (regarding the core cooling rate Tc/dt) for a given @p Tc (CMB temperature)
         */
        void get_adiabatic_heating(const double Tc, double &Ek, double &Qk) const;

        /**
         * Calculate energy (@p Ql) and entropy (@p El) change rate factor
         * (regarding the inner core growth rate dR/dt) for a given @p Tc (CMB temperature)
         * and @p r (inner core radius)
         */
        void get_latent_heating(const double Tc, const double r, double &El, double &Ql) const;

        /**
         * Calculate entropy of heat of solution @p Eh for a given @p Tc (CMB temperature),
         * @p r (inner core radius), and @p X (light element concentration)
         */
        void get_heat_solution(const double Tc, const double r, const double X, double &Eh) const;

        /**
         * return radiogenic heating rate at the current time
         */
        double get_radioheating_rate() const;

        /**
         * Update the data of the core dynamic simulation, the data will be used
         * in the next timestep and for postprocessing.
         */
        void update_core_data();

    };
  }
}


#endif
