/* -*- indent-tabs-mode: t; tab-width: 2 -*- */

#ifndef HAMILTONIAN_KS_HAMILTONIAN
#define HAMILTONIAN_KS_HAMILTONIAN

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

#include <basis/coefficients.hpp>
#include <states/ks_states.hpp>
#include <multi/adaptors/fftw.hpp>
#include <hamiltonian/projector.hpp>
#include <operations/space.hpp>

namespace hamiltonian {
  template <class basis_type>
  class ks_hamiltonian {
		
  public:

    ks_hamiltonian(const basis_type & basis, const ions::UnitCell & cell, const atomic_potential & pot, const ions::geometry & geo):
			scalar_potential(basis){

			pot.local_potential(basis, cell, geo, scalar_potential);

			for(int iatom = 0; iatom < geo.num_atoms(); iatom++){
				proj_.push_back(projector(basis, cell, pot.pseudo_for_element(geo.atoms()[iatom]), geo.coordinates()[iatom]));
			}

    }

		basis::coefficients<basis::real_space, double> scalar_potential;

    auto apply(const states::ks_states & st, const basis_type & basis, const states::coefficients & phi) const{
      
			namespace multi = boost::multi;
			namespace fftw = boost::multi::fftw;

			
			multi::array<complex, 3> fftgrid(basis.rsize());

			auto hphi = operations::space::to_fourier(st, basis, phi);

			for(int ix = 0; ix < basis.gsize()[0]; ix++){
				for(int iy = 0; iy < basis.gsize()[1]; iy++){
					for(int iz = 0; iz < basis.gsize()[2]; iz++){
						double lapl = -0.5*(-basis.g2(ix, iy, iz));
						for(int ist = 0; ist < st.num_states(); ist++) hphi.cubic[ix][iy][iz][ist] *= lapl;
					}
				}
			}

			operations::space::to_real_inplace(st, basis, hphi);

			//the non local potential in real space
			for(unsigned iproj = 0; iproj < proj_.size(); iproj++) proj_[iproj].apply(st, phi.cubic, hphi.cubic);

			//the scalar local potential in real space
			for(int ix = 0; ix < basis.rsize()[0]; ix++){
				for(int iy = 0; iy < basis.rsize()[1]; iy++){
					for(int iz = 0; iz < basis.rsize()[2]; iz++){

						double vv  = scalar_potential.cubic[ix][iy][iz];
						
						for(int ist = 0; ist < st.num_states(); ist++){
							hphi.cubic[ix][iy][iz][ist] += vv*phi.cubic[ix][iy][iz][ist];
						}
						
					}
				}
			}

			return hphi;
			
		}

		int num_projectors() const {
			int nn = 0;
			for(unsigned iproj = 0; iproj < proj_.size(); iproj++){
				nn += proj_[iproj].num_projectors();
			}
			return nn;			
		}

    template <class output_stream>
    void info(output_stream & out) const {
      out << "HAMILTONIAN:" << std::endl;
      out << "  Total number of projectors = " << num_projectors() << std::endl;
      out << std::endl;
    }	
		
  private:

		std::vector<projector> proj_;

  };

}

#ifdef UNIT_TEST

#include <ions/unitcell.hpp>
#include <catch2/catch.hpp>
#include <basis/real_space.hpp>

TEST_CASE("Class hamiltonian::ks_hamiltonian", "[ks_hamiltonian]"){

  using namespace Catch::literals;
  using math::d3vector;
  
  double ecut = 20.0;
  double ll = 10.0;

	ions::geometry geo;
  ions::UnitCell cell(d3vector(ll, 0.0, 0.0), d3vector(0.0, ll, 0.0), d3vector(0.0, 0.0, ll));
  basis::real_space pw(cell, ecut);

	REQUIRE(pw.size() == 8000);
	REQUIRE(pw.rspacing()[0] == 0.5_a);
	REQUIRE(pw.rspacing()[1] == 0.5_a);	
	REQUIRE(pw.rspacing()[2] == 0.5_a);
	REQUIRE(pw.volume_element() == 0.125_a);
	
	hamiltonian::atomic_potential pot(geo.num_atoms(), geo.atoms());
	
	states::ks_states st(states::ks_states::spin_config::UNPOLARIZED, 11.0);

  states::coefficients phi(st, pw);
	states::coefficients hphi(st, pw);
	
	hamiltonian::ks_hamiltonian<basis::real_space> ham(pw, cell, pot, geo);

	SECTION("Constant function"){
		
		
		for(int ix = 0; ix < pw.rsize()[0]; ix++){
			for(int iy = 0; iy < pw.rsize()[1]; iy++){
				for(int iz = 0; iz < pw.rsize()[2]; iz++){
					for(int ist = 0; ist < st.num_states(); ist++){
						phi.cubic[ix][iy][iz][ist] = 1.0;
					}
				}
			}
		}
		
		hphi = ham.apply(st, pw, phi);
		
		double diff = 0.0;
		for(int ix = 0; ix < pw.rsize()[0]; ix++){
			for(int iy = 0; iy < pw.rsize()[1]; iy++){
				for(int iz = 0; iz < pw.rsize()[2]; iz++){
					for(int ist = 0; ist < st.num_states(); ist++){
						diff += fabs(hphi.cubic[ix][iy][iz][ist] - 0.0);
					}
				}
			}
		}

		diff /= hphi.cubic.num_elements();
		
		REQUIRE(diff == 0.0_a);
		
	}
	
	SECTION("Plane wave"){
		
		double kk = 2.0*M_PI/pw.rlength()[0];
		
		for(int ix = 0; ix < pw.rsize()[0]; ix++){
			for(int iy = 0; iy < pw.rsize()[1]; iy++){
				for(int iz = 0; iz < pw.rsize()[2]; iz++){
					for(int ist = 0; ist < st.num_states(); ist++){
						double xx = pw.rvector(ix, iy, iz)[0];
						phi.cubic[ix][iy][iz][ist] = complex(cos(ist*kk*xx), sin(ist*kk*xx));
					}
				}
			}
		}

		hphi = ham.apply(st, pw, phi);
		
		double diff = 0.0;
		for(int ix = 0; ix < pw.rsize()[0]; ix++){
			for(int iy = 0; iy < pw.rsize()[1]; iy++){
				for(int iz = 0; iz < pw.rsize()[2]; iz++){
					for(int ist = 0; ist < st.num_states(); ist++){
							diff += fabs(hphi.cubic[ix][iy][iz][ist] - 0.5*ist*kk*ist*kk*phi.cubic[ix][iy][iz][ist]);
					}
				}
			}
		}

		diff /= hphi.cubic.num_elements();

		REQUIRE(diff < 1e-15);
		
	}


	SECTION("Harmonic oscillator"){

		double ww = 2.0;

		for(int ix = 0; ix < pw.rsize()[0]; ix++){
			for(int iy = 0; iy < pw.rsize()[1]; iy++){
				for(int iz = 0; iz < pw.rsize()[2]; iz++){
					double r2 = pw.r2(ix, iy, iz);
					ham.scalar_potential.cubic[ix][iy][iz] = ww*r2;

					for(int ist = 0; ist < st.num_states(); ist++){
						phi.cubic[ix][iy][iz][ist] = exp(-0.5*ww*r2);
					}
					
				}
			}
		}

		hphi = ham.apply(st, pw, phi);
		
		double diff = 0.0;
		for(int ix = 0; ix < pw.rsize()[0]; ix++){
			for(int iy = 0; iy < pw.rsize()[1]; iy++){
				for(int iz = 0; iz < pw.rsize()[2]; iz++){
					for(int ist = 0; ist < st.num_states(); ist++){
						diff += fabs(hphi.cubic[ix][iy][iz][ist] - ww*phi.cubic[ix][iy][iz][ist]);
					}
				}
			}
		}

		diff /= hphi.cubic.num_elements();

		REQUIRE(diff == 0.0055687279_a);
		
	}

	
	
}

#endif

#endif
