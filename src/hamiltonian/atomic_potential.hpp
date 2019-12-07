/* -*- indent-tabs-mode: t -*- */

#ifndef ATOMIC_POTENTIAL_HPP
#define ATOMIC_POTENTIAL_HPP

/*
 Copyright (C) 2019 Xavier Andrade

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU Lesser General Public License as published by
 the Free Software Foundation; either version 3 of the License, or
 (at your option) any later version.
  
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU Lesser General Public License for more details.
  
 You should have received a copy of the GNU Lesser General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include <pseudopod/set.hpp>
#include <pseudopod/pseudopotential.hpp>
#include <basis/spherical_grid.hpp>
#include <math/array.hpp>
#include <solvers/poisson.hpp>
#include <utils/distribution.hpp>

#include <unordered_map>

#include <mpi3/environment.hpp>

namespace hamiltonian {

  class atomic_potential {

  public:

    enum class error {
      PSEUDOPOTENTIAL_NOT_FOUND
    };

    template <class atom_array>
    atomic_potential(const int natoms, const atom_array & atom_list, boost::mpi3::communicator & comm = boost::mpi3::environment::get_self_instance()):
			sep_(0.5), //this is the default from qball, but it can be optimized for the grid. Check AtomsSet.cc:1102
      pseudo_set_("pseudopotentials/pseudo-dojo.org/nc-sr-04_pbe_standard/"),
			dist_(natoms, comm)
		{

      nelectrons_ = 0.0;
      for(int iatom = 0; iatom < natoms; iatom++){
				if(!pseudo_set_.has(atom_list[iatom])) throw error::PSEUDOPOTENTIAL_NOT_FOUND; 

				auto file_path = pseudo_set_.file_path(atom_list[iatom]);
				if(atom_list[iatom].has_file()) file_path = atom_list[iatom].file_path();

				auto insert = pseudopotential_list_.emplace(atom_list[iatom].symbol(), pseudo::pseudopotential(file_path, sep_));
				
				auto & pseudo = insert.first->second;
				
				nelectrons_ += pseudo.valence_charge();
				
      }
      
    }
		
    int num_species() const {
      return pseudopotential_list_.size();
    }

    const double & num_electrons() const {
      return nelectrons_;
    }
		
		template <class element_type>
    const pseudo::pseudopotential & pseudo_for_element(const element_type & el) const {
      return pseudopotential_list_.at(el.symbol());
    }

    template <class basis_type, class cell_type, class geo_type>
    auto local_potential(const basis_type & basis, const cell_type & cell, const geo_type & geo) const {

      basis::field<basis_type, double> potential(basis);

			for(long ii = 0; ii < potential.basis().size(); ii++) potential.linear()[ii] = 0.0;
			
      for(auto iatom = dist_.start(); iatom < dist_.end(); iatom++){
				
				auto atom_position = geo.coordinates()[iatom];
				
				auto & ps = pseudo_for_element(geo.atoms()[iatom]);
				basis::spherical_grid sphere(basis, cell, atom_position, ps.short_range_potential_radius());
				
				//DATAOPERATIONS LOOP + GPU::RUN 1D (random access output)
				for(int ipoint = 0; ipoint < sphere.size(); ipoint++){
					auto rr = length(basis.rvector(sphere.points()[ipoint]) - atom_position);
					auto sr_potential = ps.short_range_potential().value(rr);
					potential.cubic()[sphere.points()[ipoint][0]][sphere.points()[ipoint][1]][sphere.points()[ipoint][2]] += sr_potential;
				}
				
      }

			if(dist_.parallel()){
				dist_.comm().all_reduce_in_place_n(static_cast<double *>(potential.linear().data()), potential.linear().size(), std::plus<>{});
			}

			return potential;			
    }
		
    template <class basis_type, class cell_type, class geo_type>
    basis::field<basis_type, double> ionic_density(const basis_type & basis, const cell_type & cell, const geo_type & geo) const {

      basis::field<basis_type, double> density(basis);
			
			density = 0.0;

			for(auto iatom = dist_.start(); iatom < dist_.end(); iatom++){
				
				auto atom_position = geo.coordinates()[iatom];
				
				auto & ps = pseudo_for_element(geo.atoms()[iatom]);
				basis::spherical_grid sphere(basis, cell, atom_position, sep_.long_range_density_radius());

				//DATAOPERATIONS LOOP + GPU::RUN 1D (random access output)
#ifdef HAVE_CUDA
				//OPTIMIZATION: this should be done in parallel for atoms too
				gpu::run(sphere.size(),
								 [dns = begin(density.cubic()), pts = begin(sphere.points()),
									atpos = atom_position, chrg = ps.valence_charge(),
									sp = sep_, bas = basis] __device__
								 (auto ipoint){
									 double rr = length(bas.rvector(pts[ipoint]) - atpos);
									 dns[pts[ipoint][0]][pts[ipoint][1]][pts[ipoint][2]] += chrg*sp.long_range_density(rr);
								 });
#else
				for(int ipoint = 0; ipoint < sphere.size(); ipoint++){
					double rr = length(basis.rvector(sphere.points()[ipoint]) - atom_position);
					density.cubic()[sphere.points()[ipoint][0]][sphere.points()[ipoint][1]][sphere.points()[ipoint][2]]
						+= ps.valence_charge()*sep_.long_range_density(rr);
				}
#endif
      }

			if(dist_.parallel()){
				dist_.comm().all_reduce_in_place_n(static_cast<double *>(density.linear().data()), density.linear().size(), std::plus<>{});
			}
			
			return density;			
    }
    
    template <class output_stream>
    void info(output_stream & out) const {
      out << "ATOMIC POTENTIAL:" << std::endl;
      out << "  Number of species   = " << num_species() << std::endl;
      out << "  Number of electrons = " << num_electrons() << std::endl;
      out << std::endl;
    }

		auto & range_separation() const {
			return sep_;
		}
    
  private:

		const math::erf_range_separation sep_;
    double nelectrons_;
    pseudo::set pseudo_set_;
    std::unordered_map<std::string, pseudo::pseudopotential> pseudopotential_list_;
		utils::distribution<boost::mpi3::communicator> dist_;
        
  };

}

#ifdef UNIT_TEST
#include <catch2/catch.hpp>
#include <ions/geometry.hpp>
#include <basis/real_space.hpp>

TEST_CASE("Class hamiltonian::atomic_potential", "[hamiltonian::atomic_potential]") {

  using namespace Catch::literals;
	using pseudo::element;
  using input::species;

	auto comm = boost::mpi3::environment::get_world_instance();
	
	SECTION("Non-existing element"){
    std::vector<species> el_list({element("P"), element("X")});

    REQUIRE_THROWS(hamiltonian::atomic_potential(el_list.size(), el_list));
  }
  
  SECTION("Duplicated element"){
    std::vector<species> el_list({element("N"), element("N")});

    hamiltonian::atomic_potential pot(el_list.size(), el_list.begin());

    REQUIRE(pot.num_species() == 1);
    REQUIRE(pot.num_electrons() == 10.0_a);
    
  }

  SECTION("Empty list"){
    std::vector<species> el_list;
    
    hamiltonian::atomic_potential pot(el_list.size(), el_list);

    REQUIRE(pot.num_species() == 0);
    REQUIRE(pot.num_electrons() == 0.0_a);
  }

  SECTION("CNOH"){
    species el_list[] = {element("C"), element("N"), element("O"), element("H")};

    hamiltonian::atomic_potential pot(4, el_list);

    REQUIRE(pot.num_species() == 4);
    REQUIRE(pot.num_electrons() == 16.0_a);
  }

  SECTION("Construct from a geometry"){

    ions::geometry geo(config::path::unit_tests_data() + "benzene.xyz");

    hamiltonian::atomic_potential pot(geo.num_atoms(), geo.atoms(), comm);

    REQUIRE(pot.num_species() == 2);
    REQUIRE(pot.num_electrons() == 30.0_a);

		double ll = 20.0;
		auto cell = input::cell::cubic(ll, ll, ll);
		basis::real_space rs(cell, input::basis::cutoff_energy(20.0));

		rs.info(std::cout);
		
		auto vv = pot.local_potential(rs, cell, geo);

		REQUIRE(operations::integral(vv) == -19.0458972795_a);

		REQUIRE(vv.cubic()[5][3][0] == -0.4493576913_a);
		REQUIRE(vv.cubic()[3][1][0] == -0.0921256967_a);
							 
		auto nn = pot.ionic_density(rs, cell, geo);

		REQUIRE(operations::integral(nn) == -30.0000000746_a);
		REQUIRE(nn.cubic()[5][3][0] == -1.7354140489_a);
		REQUIRE(nn.cubic()[3][1][0] == -0.155824769_a);
  }
  
}

#endif
  
#endif
