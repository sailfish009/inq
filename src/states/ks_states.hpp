/* -*- indent-tabs-mode: t -*- */

#ifndef INQ__STATES__KS_STATES
#define INQ__STATES__KS_STATES

/*
 Copyright (C) 2019 Xavier Andrade, Alfredo A. Correa

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

#include <math/complex.hpp>
#include <math/array.hpp>
#include <basis/real_space.hpp>

namespace inq {
namespace states {

class ks_states {

public:

	typedef complex coeff_type;
    
	enum class spin_config {
													UNPOLARIZED,
													POLARIZED,
													NON_COLLINEAR
	};
        
	ks_states(const spin_config spin, const double nelectrons, const int extra_states = 0){

		if(spin == spin_config::NON_COLLINEAR){
			nstates_ = ceil(nelectrons);
		} else {
			nstates_ = ceil(0.5*nelectrons);
		}

		nstates_ += extra_states;
			
		nquantumnumbers_ = 1;
		if(spin == spin_config::POLARIZED) nquantumnumbers_ = 2;

		occs_.reextent({nstates_});

		auto rem_electrons = nelectrons;
		for(int ist = 0; ist < nstates_; ist++){
			occs_[ist] = std::min(2.0, rem_electrons);
			rem_electrons -= occs_[ist];
		}

		total_charge_ = nelectrons;
			
	}

	int num_states() const {
		return nstates_;
	}

	int num_quantum_numbers() const {
		return nquantumnumbers_;
	}

	template <class array_type>
	std::array<long int, 4> cubic_dims(const array_type & basis_dims) const {
		return {basis_dims[0], basis_dims[1], basis_dims[2], nstates_};
	}

	template <class array_type>
	std::array<long int, 2> linear_dims(const array_type & basis_dims) const {
		return {basis_dims[0]*basis_dims[1]*basis_dims[2], nstates_};
	}
    
	template <class output_stream>
	void info(output_stream & out) const {
		out << "KOHN-SHAM STATES:" << std::endl;
		out << "  Number of states = " << num_states() << std::endl;
		out << std::endl;
	}
	
	template<class OStream>
	friend OStream& operator<<(OStream& os, ks_states const& self){
		self.info(os);
		return os;
	}
	
	auto & occupations() const {
		return occs_;
	}

	auto total_charge() const {
		return total_charge_;
	}
		
private:

	double total_charge_;
	int nstates_;
	int nquantumnumbers_;
	math::array<double, 1> occs_;

};

}
}

#ifdef INQ_STATES_KS_STATES_UNIT_TEST
#undef INQ_STATES_KS_STATES_UNIT_TEST

#include <ions/unitcell.hpp>
#include <catch2/catch.hpp>

TEST_CASE("Class states::ks_states", "[ks_states]"){

	using namespace inq;
  using math::vec3d;
  
  double ecut = 30.0;
  double ll = 10.0;
  
  ions::UnitCell cell(vec3d(ll, 0.0, 0.0), vec3d(0.0, ll, 0.0), vec3d(0.0, 0.0, ll));
  basis::real_space pw(cell, input::basis::cutoff_energy(ecut));
  
  SECTION("Spin unpolarized"){
    
    states::ks_states st(states::ks_states::spin_config::UNPOLARIZED, 11.0);
    
    CHECK(st.num_states() == 6);
    CHECK(st.num_quantum_numbers() == 1);
		CHECK(st.cubic_dims(pw.sizes())[0] == pw.sizes()[0]);
		CHECK(st.cubic_dims(pw.sizes())[1] == pw.sizes()[1]);
		CHECK(st.cubic_dims(pw.sizes())[2] == pw.sizes()[2]);
		CHECK(st.cubic_dims(pw.sizes())[3] == st.num_states());

		CHECK(st.occupations()[0] == 2.0);
		CHECK(st.occupations()[1] == 2.0);
		CHECK(st.occupations()[2] == 2.0);
		CHECK(st.occupations()[3] == 2.0);
		CHECK(st.occupations()[4] == 2.0);
		CHECK(st.occupations()[5] == 1.0);
		
  }

  SECTION("Spin polarized"){
    
    states::ks_states st(states::ks_states::spin_config::POLARIZED, 11.0);
    
    CHECK(st.num_states() == 6);
    CHECK(st.num_quantum_numbers() == 2);
    CHECK(st.cubic_dims(pw.sizes())[0] == pw.sizes()[0]);
    CHECK(st.cubic_dims(pw.sizes())[1] == pw.sizes()[1]);
    CHECK(st.cubic_dims(pw.sizes())[2] == pw.sizes()[2]);
    CHECK(st.cubic_dims(pw.sizes())[3] == st.num_states());
  }

  SECTION("Non-collinear spin"){
    
    states::ks_states st(states::ks_states::spin_config::NON_COLLINEAR, 11.0);
    
    CHECK(st.num_states() == 11);
    CHECK(st.num_quantum_numbers() == 1);
    CHECK(st.cubic_dims(pw.sizes())[0] == pw.sizes()[0]);
    CHECK(st.cubic_dims(pw.sizes())[1] == pw.sizes()[1]);
    CHECK(st.cubic_dims(pw.sizes())[2] == pw.sizes()[2]);
    CHECK(st.cubic_dims(pw.sizes())[3] == st.num_states());
  }

  
}

#endif

#endif
