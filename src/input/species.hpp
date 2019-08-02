/* -*- indent-tabs-mode: t; tab-width: 2 -*- */

#ifndef INPUT__SPECIES
#define INPUT__SPECIES

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

#include <pseudo/element.hpp>
#include <vector>
#include <cmath>

namespace input {

	class species : public pseudo::element {

		class options;
		
  public:
		
		species(const pseudo::element & arg_el, const options & arg_options = {}):
			pseudo::element(arg_el),
			opts(arg_options){
		}

		friend species operator|(const species & spec, const options & opts){
			auto rspec = spec;
			rspec.opts = rspec.opts | opts;
			return rspec;
		}

		static auto name(const std::string & arg_name){
			options ropt;
			ropt.name_ = arg_name;
			return ropt;
		}
		
		static auto pseudo(const std::string & pseudo_file){
			options ropt;
			ropt.pseudo_file_ = pseudo_file;
			return ropt;
		}

		auto has_file() const {
			return opts.pseudo_file_.has_value();
		}

		auto const & file_path() const {
			return opts.pseudo_file_.value();
		}

		auto symbol() const {
			return opts.name_.value_or(pseudo::element::symbol());
		}
		
	private:

		struct options {
		private:
			
			std::optional<std::string> name_;
			std::optional<std::string> pseudo_file_;		
			std::optional<double> mass_;
			
			options(){
			}
			
			template <class opt_type>
			static opt_type merge_option(const opt_type & option1, const opt_type & option2){
				if(option2) return option2;
				if(option1) return option1;
				return opt_type{};
			}
			
		public:

			friend options operator|(const options & opt1, const options & opt2){
				options ropt;
				
				ropt.name_ = merge_option(opt1.name_, opt2.name_);
				ropt.pseudo_file_ = merge_option(opt1.pseudo_file_, opt2.pseudo_file_);
				ropt.mass_ = merge_option(opt1.mass_, opt2.mass_);
				
				return ropt;
			}


			friend class species;
			
		};

		options opts;
		
  };
  
}


#ifdef UNIT_TEST
#include <catch2/catch.hpp>

TEST_CASE("class ions::species", "[species]") {
  
  using namespace Catch::literals;

	input::species s(pseudo::element("Xe"));

	REQUIRE(s.atomic_number() == 54);
	
}


#endif


#endif

// Local Variables:
// eval:(setq indent-tabs-mode: t tab-width: 2)
// mode: c++
// coding: utf-8
// End:
