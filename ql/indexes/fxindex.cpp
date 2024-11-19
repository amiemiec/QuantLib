/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2024 Sebastian Schlenkrich

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

#include <ql/indexes/fxindex.hpp>
#include <ql/settings.hpp>
#include <utility>

namespace QuantLib {

    FxIndex::FxIndex(
	    std::string name,
        Calendar fixingCalendar,
        Handle<YieldTermStructure> domInterest,
        Handle<YieldTermStructure> forInterest,
        Handle<Quote> spot
	) : EquityIndex(std::move(name), std::move(fixingCalendar), std::move(domInterest), std::move(forInterest), std::move(spot)) {} 


     Real FxIndex::forecastFixing(const Date& fixingDate) const {
        QL_REQUIRE(!domesticInterestRateCurve().empty(),
                   "null interest rate term structure set to this instance of " << name());

        Date today = Settings::instance().evaluationDate();
        Date lastFixingDate = fixingCalendar().adjust(today, BusinessDayConvention::Preceding);
        Date spotDate = fixingCalendar().advance(today, 2 * Days);

        Real fxspot = (*this).spot()->value();

        Real forward = fxspot;
        forward *= foreignInterestRateCurve()->discount(fixingDate) /
                       foreignInterestRateCurve()->discount(spotDate);
        forward /= domesticInterestRateCurve()->discount(fixingDate) /
                   domesticInterestRateCurve()->discount(spotDate);

        return forward;
    }


}