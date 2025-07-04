/*
 Copyright (C) 2012 - 2021 by the authors of the ASPECT code.

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
 along with ASPECT; see the file LICENSE.  If not see
 <http://www.gnu.org/licenses/>.
 */

#ifndef _aspect_particle_manager_h
#define _aspect_particle_manager_h

#include <aspect/global.h>

#include <deal.II/particles/particle.h>
#include <deal.II/particles/particle_accessor.h>
#include <deal.II/particles/particle_iterator.h>
#include <deal.II/particles/particle_handler.h>
#include <deal.II/particles/property_pool.h>

#include <deal.II/matrix_free/fe_point_evaluation.h>

#include <aspect/particle/generator/interface.h>
#include <aspect/particle/integrator/interface.h>
#include <aspect/particle/interpolator/interface.h>
#include <aspect/particle/property/interface.h>

#include <aspect/simulator_access.h>
#include <aspect/simulator_signals.h>
#include <aspect/solution_evaluator.h>

#include <deal.II/base/timer.h>
#include <deal.II/base/array_view.h>

#include <boost/serialization/unique_ptr.hpp>

#include <random>


namespace aspect
{
  template <int dim>
  struct SimulatorSignals;

  namespace Particle
  {
    using namespace dealii::Particles;

    namespace Generator
    {
      template <int dim>
      class Interface;
    }


    namespace Property
    {
      template <int dim>
      class Manager;
    }

    /**
     * This class manages the storage and handling of particles. It provides
     * interfaces to generate and store particles, functions to initialize,
     * update and advect them, and ways to retrieve information about the
     * particles. The implementation of most of these methods is outsourced
     * to different plugin systems, this class is mostly concerned with
     * managing the interactions of the different systems with the code
     * outside the particle manager.
     *
     * @ingroup Particle
     */
    template <int dim>
    class Manager : public SimulatorAccess<dim>
    {
      public:
        /**
         * Default constructor.
         */
        Manager();

        /**
         * Default destructor.
         */
        ~Manager() override;

        /**
         * Move constructor. This is required to be able to put instances
         * of this class into a std::vector.
         */
        Manager(Manager &&) noexcept;

        /**
         * Initialize the particle manager.
         */
        void initialize();

        /**
         * Update the particle manager.
        */
        void update();

        /**
         * Get the particle property manager for this particle manager.
         *
         * @return The property manager for this particle manager.
         */
        const Property::Manager<dim> &
        get_property_manager() const;

        /**
         * Get the const particle handler for this particle manager.
         *
         * @return The particle handler for this particle manager.
         */
        const Particles::ParticleHandler<dim> &
        get_particle_handler() const;

        /**
         * Get the particle handler for this particle manager.
         * There is no get_particles() function in the deal.II
         * ParticleHandler, so we get and set the positions
         * of the particles. These getter/setter functions are
         * not const, and neither are the calling functions,
         * but the existing get_particle_handler is.
         * Therefore this non-const function is added.
         *
         * @return The particle handler for this particle manager.
         */
        Particles::ParticleHandler<dim> &
        get_particle_handler();

        /**
         * Copy the state of particle handler @p from_particle_handler into the
         * particle handler @p to_particle_handler. This will copy
         * all particles and properties and leave @p to_particle_handler
         * as an identical copy of @p from_particle_handler, assuming it
         * was set up by this particle manager class. This means we assume
         * @p from_particle_handler uses the same triangulation and
         * particle properties as are used in this model. Existing
         * particles in @p to_particle_handler are deleted.
         *
         * This function is expensive as it has to duplicate all data
         * in @p from_particle_handler, and insert it into @p to_particle_handler,
         * which may be a significant amount of data. However, it can
         * be useful to save the state of a particle
         * collection at a certain point in time and reset this
         * state later under certain conditions, for example if
         * a timestep has to be undone and repeated.
         */
        void copy_particle_handler (const Particles::ParticleHandler<dim> &from_particle_handler,
                                    Particles::ParticleHandler<dim> &to_particle_handler) const;

        /**
         * @brief Stores a copy of the particle handler in particle_handler_backup. This copy can be
         * used to restore the position and properties of the particles for example after advection
         * solver iterations of the iterated advection scheme or when a timestep has to be repeated.
         */
        void backup_particles ();

        /**
         * @brief Restores the particle handler particle_handler based on the copy
         * in particle_handler_backup. This restores the position and properties of the particles
         * to those in the copy and can be used for example after advection solver iterations
         * of the iterated advection scheme or when a timestep has to be repeated.
         */
        void restore_particles ();


        /**
         * Do initial logic for handling pre-refinement steps
         */
        void setup_initial_state ();

        /**
         * Get the particle interpolator for this particle manager.
         *
         * @return The interpolator for this particle manager.
         */
        const Interpolator::Interface<dim> &
        get_interpolator() const;

        /**
         * Initialize the particle properties.
         */
        void generate_particles();
        /**
         * Initialize the particle properties.
         */
        void initialize_particles();

        /**
         * Advance particles by the old timestep using the current
         * integration scheme. This accounts for the fact that the particles
         * are actually still at their old positions and the current timestep
         * length is already updated for the next step at the time this
         * function is called.
         */
        void advance_timestep();

        /**
         * Return the total number of particles in the simulation. This
         * function is useful for monitoring how many particles have been
         * lost by falling out of the domain. Note that this function does
         * not compute the number of particles, because that is an expensive
         * global MPI operation. Instead it returns the number, which is
         * updated internally every time it might change by a call to
         * update_n_global_particles().
         *
         * @return Total number of particles in simulation.
         */
        types::particle_index n_global_particles() const;

        /**
         * This callback function is registered within Simulator by the
         * constructor of this class and will be
         * called from Simulator during construction. It allows to attach slot
         * functions to the provided SimulatorSignals. This particle manager will register
         * the register_store_callback_function() and
         * register_load_callback_function() to the
         * pre_refinement_store_user_data signal and the
         * post_refinement_load_user_data signal respectively.
         */
        void
        connect_to_signals(aspect::SimulatorSignals<dim> &signals);

        /**
         * Called by listener functions from Triangulation for every cell
         * before a refinement step. A weight is attached to every cell
         * depending on the number of contained particles.
         */
#if DEAL_II_VERSION_GTE(9,6,0)
        unsigned int
        cell_weight(const typename parallel::distributed::Triangulation<dim>::cell_iterator &cell,
                    const CellStatus status);
#else
        unsigned int
        cell_weight(const typename parallel::distributed::Triangulation<dim>::cell_iterator &cell,
                    const typename parallel::distributed::Triangulation<dim>::CellStatus status);
#endif

        /**
         * Update the particle properties if necessary.
         */
        void update_particles();

        /**
         * Serialize the contents of this class.
         */
        template <class Archive>
        void serialize (Archive &ar, const unsigned int version);

        /**
         * Save the state of the object.
         */
        virtual
        void
        save (std::ostringstream &os) const;

        /**
         * Restore the state of the object.
         */
        virtual
        void
        load (std::istringstream &is);

        /**
         * Declare the parameters this class takes through input files.
         */
        static
        void
        declare_parameters (ParameterHandler &prm);

        /**
         * Read the parameters this class declares from the parameter file.
         *
         * @param prm The ParameterHandler.
         * @param particle_manager Parse the parameters for the Particle manager with this index.
         */
        virtual
        void
        parse_parameters (ParameterHandler &prm, const unsigned int particle_manager);

      private:
        struct ParticleLoadBalancing
        {
          enum Kind
          {
            no_balancing = 0x0,
            remove_particles = 0x1,
            add_particles = 0x2,
            repartition = 0x4,
            remove_and_add_particles = remove_particles | add_particles
          };
        };

        /**
         * Generation scheme for creating particles in this manager
         */
        std::unique_ptr<Generator::Interface<dim>> generator;

        /**
         * Integration scheme for moving particles in this manager
         */
        std::unique_ptr<Integrator::Interface<dim>> integrator;

        /**
         * Random number generator used for creating and deleting particles
         */
        std::mt19937 random_number_generator;

        /**
         * Interpolation scheme for moving particles in this manager
         */
        std::unique_ptr<Interpolator::Interface<dim>> interpolator;

        /**
         * Particle handler object that is responsible for storing and
         * managing the internal particle structures.
         */
        std::unique_ptr<Particles::ParticleHandler<dim>> particle_handler;

        /**
         * Particle handler object that is responsible for storing and
         * managing the internal particle structures before starting nonlinear iterations
         * such that the particle position and properties can be restored after
         * each outer advection iteration.
         */
        Particles::ParticleHandler<dim> particle_handler_backup;

        /**
         * The property manager stores information about the additional
         * particle properties and handles the initialization and update of
         * these properties.
         */
        std::unique_ptr<Property::Manager<dim>> property_manager;

        /**
         * Strategy for particle load balancing.
         */
        typename ParticleLoadBalancing::Kind particle_load_balancing;

        /**
         * Lower limit for particle number per cell. This limit is
         * useful for adaptive meshes to prevent fine cells from being empty
         * of particles. It will be checked and enforced after mesh
         * refinement and after particle movement. If there are
         * n_number_of_particles < min_particles_per_cell
         * particles in one cell then
         * min_particles_per_cell - n_number_of_particles particles are
         * generated and randomly placed in this cell. If the particles carry
         * properties the individual property plugins control how the
         * properties of the new particles are initialized.
         */
        unsigned int min_particles_per_cell;

        /**
         * Upper limit for particle number per cell. This limit is
         * useful for adaptive meshes to prevent coarse cells from slowing down
         * the whole model. It will be checked and enforced after mesh
         * refinement, after MPI transfer of particles and after particle
         * movement. If there are
         * n_number_of_particles > max_particles_per_cell
         * particles in one cell then
         * n_number_of_particles - max_particles_per_cell
         * particles in this cell are randomly chosen and destroyed.
         */
        unsigned int max_particles_per_cell;

        /**
         * The computational cost of a single particle. This is an input
         * parameter that is set during initialization and is only used if the
         * particle load balancing strategy 'repartition' is used. This value
         * determines how costly the computation of a single particle is compared
         * to the computation of a whole cell, which is arbitrarily defined
         * to represent a cost of 1000.
         */
        unsigned int particle_weight;

        /**
         * Get a map between subdomain id and the neighbor index. In other words
         * the returned map answers the question: Given a subdomain id, which
         * neighbor of the current processor's domain (in terms of a contiguous
         * number from 0 to n_neighbors) owns this subdomain?
         */
        std::map<types::subdomain_id, unsigned int>
        get_subdomain_id_to_neighbor_map() const;

        /**
         * Apply the bounds for the maximum and minimum number of particles
         * per cell, if the appropriate @p particle_load_balancing strategy
         * has been selected.
         */
        void
        apply_particle_per_cell_bounds();

        /**
         * Advect the particle positions by one integration step. Needs to be
         * called until integrator->continue() returns false.
         */
        void advect_particles();

        /**
         * Initialize the particle properties of one cell.
         */
        void
        local_initialize_particles(const typename ParticleHandler<dim>::particle_iterator &begin_particle,
                                   const typename ParticleHandler<dim>::particle_iterator &end_particle);

        /**
         * Update the particle properties of one cell.
         *
         * @param inputs The input data required for the particle update. This
         * function will fill this structure with the necessary data.
         * @param positions The reference positions of the particles in the cell.
         * This function will update these positions for the current cell.
         * @param evaluation_flags The required evaluation flags for each component.
         * @param evaluator The solution evaluator that is used to update the particles.
         */
        void
        local_update_particles(Property::ParticleUpdateInputs<dim> &inputs,
                               small_vector<Point<dim>> &positions,
                               const std::vector<EvaluationFlags::EvaluationFlags> &evaluation_flags,
                               SolutionEvaluator<dim> &evaluator);

        /**
         * Advect the particles of one cell. Performs only one step for
         * multi-step integrators. Needs to be called until integrator->continue()
         * evaluates to false. Particles that moved out of their old cell
         * during this advection step are removed from the local multimap and
         * stored in @p particles_out_of_cell for further treatment (sorting
         * them into the new cell).
         */
        void
        local_advect_particles(const typename DoFHandler<dim>::active_cell_iterator &cell,
                               const typename ParticleHandler<dim>::particle_iterator &begin_particle,
                               const typename ParticleHandler<dim>::particle_iterator &end_particle,
                               SolutionEvaluator<dim> &evaluators);

        /**
         * This function registers the necessary functions to the
         * @p signals that the @p particle_handler needs to know about.
         */
        void
        connect_particle_handler_signals(aspect::SimulatorSignals<dim> &signals,
                                         ParticleHandler<dim> &particle_handler,
                                         const bool connect_to_checkpoint_signals = true) const;
    };

    /* -------------------------- inline and template functions ---------------------- */

    template <int dim>
    template <class Archive>
    void Manager<dim>::serialize (Archive &ar, const unsigned int)
    {
      // Note that although Boost claims to handle serialization of pointers
      // correctly, at least for the case of unique_ptr it seems to not work.
      // It works correctly when archiving the content of the pointer instead.
      ar
      &(*particle_handler)
      ;
    }
  }
}

#endif
