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

#include <ql/experimental/shortrate/generalizedornsteinuhlenbeckprocess.hpp>

namespace QuantLib {

	GeneralizedOrnsteinUhlenbeckProcess::GeneralizedOrnsteinUhlenbeckProcess(
        const Handle<YieldTermStructure>&  speedTS,
		const Handle<YieldTermStructure>&  volTS,
		Real x0,
		Real level)
    : x0_(x0), level_(level), speed_(speedTS), volatility_(volTS) {

        QL_REQUIRE(x0 >= 0.0, "negative initial data given");
        QL_REQUIRE(level >= 0.0, "negative level given");
	}

	Real GeneralizedOrnsteinUhlenbeckProcess::x0() const {
		return x0_;
	}

	Real GeneralizedOrnsteinUhlenbeckProcess::drift(Time t, Real x) const {

		Real speed = 0.0;

		if (t > 0.0001) {
			//speed = speed_->zeroRate(t,Continuous,NoFrequency,true);
			speed = - std::log(speed_->discount(t,true))/t;
		} else {
			//speed = speed_->zeroRate(0.0001,Continuous,NoFrequency,true);
			speed = - std::log(speed_->discount(0.0001,true))/(0.0001);
		}

		return speed * (level_ - x);;
    }

	Real GeneralizedOrnsteinUhlenbeckProcess::diffusion(Time t, Real) const {

		Real volatility = 0.0;

		if (t > 0.0001) {
			//volatility = volatility_->zeroRate(t,Continuous,NoFrequency,true);
			volatility = - std::log(volatility_->discount(t,true))/t;
		} else {
			//volatility = volatility_->zeroRate(0.0001,Continuous,NoFrequency,true);
			volatility = - std::log(volatility_->discount(0.0001,true))/(0.0001);
		}

		return volatility;
    }

	Real GeneralizedOrnsteinUhlenbeckProcess::expectation(
                                             Time t, Real x0, Time dt) const {

		Real speed = 0.0;

		if (t > 0.0001) {
			//speed = speed_->zeroRate(t,Continuous,NoFrequency,true);
			speed = - std::log(speed_->discount(t,true))/t;
		} else {
			//speed = speed_->zeroRate(0.0001,Continuous,NoFrequency,true);
			speed = - std::log(speed_->discount(0.0001,true))/(0.0001);
		}

	    return level_ + (x0 - level_) * std::exp(-speed*dt);
	}

	Real GeneralizedOrnsteinUhlenbeckProcess::stdDeviation(
                                             Time t, Real x0, Time dt) const {
        return std::sqrt(variance(t,x0,dt));
	}

	Real GeneralizedOrnsteinUhlenbeckProcess::variance(
                                              Time t, Real x, Time dt) const {

        Real speed = 0.0;

		if (t > 0.0001) {
			//speed = speed_->zeroRate(t,Continuous,NoFrequency,true);
			speed = - std::log(speed_->discount(t,true))/t;
		} else {
			//speed = speed_->zeroRate(0.0001,Continuous,NoFrequency,true);
			speed = - std::log(speed_->discount(0.0001,true))/(0.0001);
		}

		Real volatility = 0.0;

		if (t > 0.0001) {
			//volatility = volatility_->zeroRate(t,Continuous,NoFrequency,true);
			volatility = - std::log(volatility_->discount(t,true))/t;
		} else {
			//volatility = volatility_->zeroRate(0.0001,Continuous,NoFrequency,true);
			volatility = - std::log(volatility_->discount(0.0001,true))/(0.0001);
		}


		if (speed < std::sqrt(QL_EPSILON)) {
			// algebraic limit for small speed
			return volatility*volatility*dt;
		} else {
			return 0.5*volatility*volatility/speed*(1.0 - std::exp(-2.0*speed*dt));
		}
	}


	Handle<YieldTermStructure> GeneralizedOrnsteinUhlenbeckProcess::speed() const {
		return speed_;
	}

	Handle<YieldTermStructure>  GeneralizedOrnsteinUhlenbeckProcess::volatility() const {
		return volatility_;
	}

	Real GeneralizedOrnsteinUhlenbeckProcess::level() const {
		return level_;
	}

}

