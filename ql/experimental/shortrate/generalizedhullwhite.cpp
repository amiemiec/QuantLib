/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2010 SunTrust Bank
 Copyright (C) 2010 Cavit Hafizoglu
 Copyright (C) 2010 Andre Miemiec

 This file is part of QuantLib, a free-software/open-source library
 for financial quantitative analysts and developers - http://quantlib.org/

 QuantLib is free software: you can redistribute it and/or modify it
 under the terms of the QuantLib license.  You should have received a
 copy of the license along with this program; if not, please email
 <quantlib-dev@lists.sf.net>. The license is also available online at
 <http://quantlib.org/license.shtml>.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE.  See the license for more details.
*/

#include <ql/experimental/shortrate/generalizedhullwhite.hpp>
#include <ql/termstructures/interpolatedcurve.hpp>
#include <ql/termstructures/yield/zerocurve.hpp>
#include <ql/math/interpolations/backwardflatinterpolation.hpp>
#include <ql/methods/lattices/trinomialtree.hpp>
#include <ql/math/solvers1d/brent.hpp>

namespace QuantLib {

	/* Private function used by solver to determine time-dependent parameter
       df(r) = [theta(t) - a(t) f(r)]dt + sigma(t) dz
       dg = [theta(t) - a(t) g(t)] dt
       dx = -a(t) x dt + sigma(t) dz
       x = f(r) - g(t)
	*/
	// Change the overloaded operator to change the model by changing
	// the function below

	Real fInverse_(Real x) {
		//Hull-White Modell
		return x;
	}



	GeneralizedHullWhite::GeneralizedHullWhite(
        const Handle<YieldTermStructure>& yieldtermStructure,
		const std::vector<Date>& speedstructure,
		const std::vector<Date>& volstructure,
		const std::vector<Real>& speed,
		const std::vector<Real>& vol,
		const InterestRate& oas)
  : OneFactorModel(2), TermStructureConsistentModel(yieldtermStructure),
    speedstructure_(speedstructure),
    volstructure_(volstructure),
	oas_(oas),
	dc_(yieldtermStructure->dayCounter()),
    a_(arguments_[0]), sigma_(arguments_[1]) {

        DayCounter dc = yieldtermStructure->dayCounter();
        Date today    = yieldtermStructure->referenceDate();

        QL_REQUIRE(speedstructure_[0]==today,"first date must be today");
        QL_REQUIRE(volstructure_[0]==today,"first date must be today");

        // set up speed structure		

        for (Size i=0;i<speedstructure_.size();i++)
            speedperiods_.push_back(dc.yearFraction(today,
                                                    speedstructure_[i]));

        a_ = PiecewiseConstantParameter2(speedperiods_, PositiveConstraint());

		for (Size i=0; i< a_.size();i++) {
            a_.setParam(i,speed[i]);
        }

		
        // set up vol structure		
		
		for (Size i=0;i<volstructure_.size();i++)
            volperiods_.push_back(dc.yearFraction(today,
                                                  volstructure_[i]));

        sigma_ = PiecewiseConstantParameter2(volperiods_, PositiveConstraint());

        for (Size i=0; i< sigma_.size();i++) {
            sigma_.setParam(i,vol[i]);
        }
   
        registerWith(yieldtermStructure);
	}

	boost::shared_ptr<Lattice> GeneralizedHullWhite::tree(
                                                  const TimeGrid& grid) const
	{

		TermStructureFittingParameter phi(termStructure());
		boost::shared_ptr<ShortRateDynamics> numericDynamics(
			new Dynamics(phi, speed(), vol()));
		boost::shared_ptr<TrinomialTree> trinomial(
			new TrinomialTree(numericDynamics->process(), grid));
		boost::shared_ptr<ShortRateTree> numericTree(
			new ShortRateTree(trinomial, numericDynamics, grid));
		typedef TermStructureFittingParameter::NumericalImpl NumericalImpl;
		boost::shared_ptr<NumericalImpl> impl =
			boost::dynamic_pointer_cast<NumericalImpl>(phi.implementation());

		impl->reset();

        for (Size i=0; i<(grid.size() - 1); i++) {
            Real discountBond = termStructure()->discount(grid[i+1]);
            const Array& statePrices = numericTree->statePrices(i);
            Size size = numericTree->size(i);
            Time dt = numericTree->timeGrid().dt(i);
            Real dx = trinomial->dx(i);
            Real x = trinomial->underlying(i,0);
            Real value = 0.0;
            for (Size j=0; j<size; j++) {
                value += statePrices[j]*std::exp(-x*dt);
                x += dx;
            }
            value = std::log(value/discountBond)/dt;
            impl->set(grid[i], value);
        }

        //apply oas 
		boost::shared_ptr<Dynamics> dyn = boost::dynamic_pointer_cast<Dynamics>(numericDynamics);
        dyn->setOAS(oas_);


		return numericTree;
	}


    Handle<YieldTermStructure> GeneralizedHullWhite::speed() const {

		std::vector<Real> speedvals;

		for (Size i=0;i<a_.size();i++)
		{
			speedvals.push_back( a_(speedperiods_[i]) );
		}

		Handle<YieldTermStructure> speed_(boost::shared_ptr<YieldTermStructure>(
			new InterpolatedZeroCurve<BackwardFlat>(speedstructure_,speedvals,dc_)));

		return speed_;
	}

    Handle<YieldTermStructure> GeneralizedHullWhite::vol() const {

		std::vector<Real> volvals;

		for (Size i=0;i<sigma_.size();i++)
      	{
			volvals.push_back( sigma_(volperiods_[i]) );
		}

		Handle<YieldTermStructure> volatlity_(boost::shared_ptr<YieldTermStructure>(
			new InterpolatedZeroCurve<BackwardFlat>(volstructure_,volvals,dc_)));

		return volatlity_;	
	}

}
