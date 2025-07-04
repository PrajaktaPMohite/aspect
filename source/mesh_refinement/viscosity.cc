/*
  Copyright (C) 2011 - 2022 by the authors of the ASPECT code.

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


#include <aspect/mesh_refinement/viscosity.h>

#include <deal.II/base/quadrature_lib.h>
#include <deal.II/dofs/dof_tools.h>
#include <deal.II/fe/fe_values.h>
#include <deal.II/numerics/derivative_approximation.h>

namespace aspect
{
  namespace MeshRefinement
  {
    template <int dim>
    void
    Viscosity<dim>::execute(Vector<float> &indicators) const
    {
      indicators = 0;

      //TODO: if the viscosity doesn't actually depend on the solution
      // then we can get away with simply interpolating it spatially


      // create a vector in which we set the temperature block to
      // be a finite element interpolation of the viscosity.
      // we do so by setting up a quadrature formula with the
      // temperature unit support points, then looping over these
      // points, compute the output quantity at them, and writing
      // the result into the output vector in the same order
      // (because quadrature points and temperature dofs are,
      // by design of the quadrature formula, numbered in the
      // same way)
      LinearAlgebra::BlockVector vec_distributed (this->introspection().index_sets.system_partitioning,
                                                  this->get_mpi_communicator());

      const Quadrature<dim> quadrature(this->get_fe().base_element(this->introspection().base_elements.temperature).get_unit_support_points());
      std::vector<types::global_dof_index> local_dof_indices (this->get_fe().dofs_per_cell);
      FEValues<dim> fe_values (this->get_mapping(),
                               this->get_fe(),
                               quadrature,
                               update_quadrature_points | update_values | update_gradients);

      // the values of the compositional fields are stored as block vectors for each field
      // we have to extract them in this structure
      std::vector<std::vector<double>> prelim_composition_values (this->n_compositional_fields(),
                                                                   std::vector<double> (quadrature.size()));

      MaterialModel::MaterialModelInputs<dim> in(quadrature.size(),
                                                 this->n_compositional_fields());
      MaterialModel::MaterialModelOutputs<dim> out(quadrature.size(),
                                                   this->n_compositional_fields());
      in.requested_properties = MaterialModel::MaterialProperties::viscosity;

      for (const auto &cell : this->get_dof_handler().active_cell_iterators())
        if (cell->is_locally_owned())
          {
            fe_values.reinit(cell);
            in.reinit(fe_values, cell, this->introspection(), this->get_solution());
            this->get_material_model().evaluate(in, out);

            cell->get_dof_indices (local_dof_indices);

            // for each temperature dof, write into the output
            // vector the viscosity. note that quadrature points and
            // dofs are enumerated in the same order
            for (unsigned int i=0; i<this->get_fe().base_element(this->introspection().base_elements.temperature).dofs_per_cell; ++i)
              {
                const unsigned int system_local_dof
                  = this->get_fe().component_to_system_index(this->introspection().component_indices.temperature,
                                                             /*dof index within component=*/i);

                vec_distributed(local_dof_indices[system_local_dof])
                  = std::log(out.viscosities[i]);
              }
          }

      vec_distributed.compress(VectorOperation::insert);

      // now create a vector with the requisite ghost elements
      // and use it for estimating the gradients
      LinearAlgebra::BlockVector vec (this->introspection().index_sets.system_partitioning,
                                      this->introspection().index_sets.system_relevant_partitioning,
                                      this->get_mpi_communicator());
      vec = vec_distributed;

      DerivativeApproximation::approximate_gradient  (this->get_mapping(),
                                                      this->get_dof_handler(),
                                                      vec,
                                                      indicators,
                                                      this->introspection().component_indices.temperature);

      // Scale gradient in each cell with the correct power of h. Otherwise,
      // error indicators do not reduce when refined if there is a viscosity
      // jump. We need at least order 1 for the error not to grow when
      // refining, so anything >1 should work. (note that the gradient
      // itself scales like 1/h, so multiplying it with any factor h^s, s>1
      // will yield convergence of the error indicators to zero as h->0)
      const double power = 1.0 + dim/2.0;
      for (const auto &cell : this->get_dof_handler().active_cell_iterators())
        if (cell->is_locally_owned())
          indicators(cell->active_cell_index()) *= std::pow(cell->diameter(), power);
    }
  }
}

// explicit instantiations
namespace aspect
{
  namespace MeshRefinement
  {
    ASPECT_REGISTER_MESH_REFINEMENT_CRITERION(Viscosity,
                                              "viscosity",
                                              "A mesh refinement criterion that computes "
                                              "refinement indicators from a field that describes "
                                              "the spatial variability of the logarithm of the viscosity, $\\log\\eta$. "
                                              "(We choose the logarithm of the viscosity because it can vary by "
                                              "orders of magnitude.)"
                                              "Because this quantity may not be a continuous function ($\\eta$ "
                                              "may be a discontinuous function along discontinuities in the "
                                              "medium, for example due to phase changes), we approximate the "
                                              "gradient of this quantity to refine the mesh. The error indicator "
                                              "defined here takes the magnitude of the approximate gradient "
                                              "and scales it by $h_K^{1+d/2}$ where $h_K$ is the diameter of each cell "
                                              "and $d$ is the dimension. "
                                              "This scaling ensures that the error indicators converge to zero as "
                                              "$h_K\\rightarrow 0$ even if the viscosity is discontinuous, since "
                                              "the gradient of a discontinuous function grows like $1/h_K$.")
  }
}
