/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2006, 2007 Giorgio Facchinetti
 Copyright (C) 2006, 2007 Mario Pucci

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

#include <ql/patterns/visitor.hpp>
#include <ql/patterns/lazyobject.hpp>

#include <ql/cashflows/cashflowvectors.hpp>
#include <ql/cashflows/fxrangeaccrualfixed.hpp>
#include <ql/indexes/fxindex.hpp>
#include <ql/math/distributions/normaldistribution.hpp>
#include <ql/pricingengines/blackformula.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>
#include <ql/termstructures/volatility/equityfx/blackvoltermstructure.hpp>
#include <ql/time/schedule.hpp>
#include <cmath>
#include <utility>

namespace QuantLib {


    FxRangeAccrualFixedCoupon::FxRangeAccrualFixedCoupon(
        // FixedRateCoupon
        const Date& paymentDate,
        Real nominal,
        Real rate,
        const DayCounter& dayCounter,
        const Date& accrualStartDate,
        const Date& accrualEndDate,
        // RA feature
        // calculate observation schedule from coupon
        ext::shared_ptr<FxIndex> index,
        Real lowerTrigger,
        Real upperTrigger,
        // optional FixedRateCoupon
        const Date& refPeriodStart,
        const Date& refPeriodEnd,
        const Date& exCouponDate)
    : FixedRateCoupon(paymentDate,
                      nominal,
                      rate,
                      dayCounter,
                      accrualStartDate,
                      accrualEndDate,
                      refPeriodStart,
                      refPeriodEnd,
                      exCouponDate),
      index_(index), lowerTrigger_(lowerTrigger), upperTrigger_(upperTrigger),
      observationsSchedule_(0), pricer_(0), rangeAccrual_(0.0) 
    {
        QL_REQUIRE(index_, "fxIndex_ required.");
        QL_REQUIRE(lowerTrigger_ > 0.0, "lowerTrigger_ > 0.0 required.");
        QL_REQUIRE(lowerTrigger_ < upperTrigger_, "lowerTrigger_ < upperTrigger_ required.");

        Calendar cal = index_->fixingCalendar();

        Date accrualStartDateMod = accrualStartDate;
        Date accrualEndDateMod =   accrualEndDate;

        observationsSchedule_ = ext::make_shared<Schedule>(MakeSchedule()
                                                               .from(accrualStartDateMod)
                                                               .to(accrualEndDateMod)
                                                               .withFrequency(Daily)
                                                               .withCalendar(cal)
                                                               .withConvention(Following));

        QL_REQUIRE(observationsSchedule_, "observationsSchedule_ required.");
    }


    void FxRangeAccrualFixedCoupon::performCalculations() const {
        FixedRateCoupon::performCalculations();
        if (pricer_) {
            pricer_->initialize(*this);
            rangeAccrual_ = pricer_->rangeAccrual();
            additionalResults_ = pricer_->additionalResults();
        } else {
            // calculate fall-back via intrinsic value
            Real inRange = 0.0;
            for (auto d : observationsSchedule()->dates()) {
                auto indexObservation = index()->fixing(d);
                if (indexObservation >= lowerTrigger() && indexObservation <= upperTrigger())
                    inRange += 1.0;
            }
            rangeAccrual_ = inRange / observationsSchedule()->dates().size();
        }
    }

    Real FxRangeAccrualFixedCoupon::rangeAccrual() const {
        calculate();  // make sure member is calculated
        return rangeAccrual_;
    }


    Real FxRangeAccrualFixedCoupon::amount() const {
        calculate();
        return FixedRateCoupon::amount() * rangeAccrual_;
    }


    void FxRangeAccrualFixedCoupon::accept(AcyclicVisitor& v) {
        auto* v1 = dynamic_cast<Visitor<FxRangeAccrualFixedCoupon>*>(&v);
        if (v1 != nullptr)
            v1->visit(*this);
        else
            FixedRateCoupon::accept(v);
    }

    void FxRangeAccrualFixedCoupon::setPricer(const ext::shared_ptr<FxRangeAccrualFixedCouponPricer>& pricer){
        if (pricer_ != nullptr)
            unregisterWith(pricer_);
        pricer_ = pricer;
        if (pricer_ != nullptr)
            registerWith(pricer_);
        update();
    }

    FxRangeAccrualFixedCouponPricer::FxRangeAccrualFixedCouponPricer(
            Handle<BlackVolTermStructure> volatility
    ) : volatility_(volatility), rangeAccrual_(0.0) {}

    void FxRangeAccrualFixedCouponPricer::initialize(const FxRangeAccrualFixedCoupon& coupon) {
        additionalResults_.clear();
        // implement rangeAccrual_ calculation...
        Real minStd = 0.0005;  // 1% * sqrt(1d)
        Real relSkewShift = 0.0001;
        Real strikeLow = coupon.lowerTrigger();
        Real strikeUpp = coupon.upperTrigger();
        
        boost::shared_ptr<FxIndex> index = coupon.index();

        CumulativeNormalDistribution Phi;

        Real daysInRange = 0.0;
        Size observationDays = coupon.observationsSchedule()->dates().size();
        
        for (auto d : coupon.observationsSchedule()->dates()) {
            // we declare the valiables here to have them available for additional results later
            Real indexObservation = index->fixing(d);
     
            Real standardDevLow = 0.0;
            Real standardDevUpp = 0.0;

            Real probLow = 0.0;
            Real probUpp = 0.0;
            

            if (d > volatility_->referenceDate()) {
                standardDevLow = std::sqrt(std::max(volatility_->blackVariance(d, strikeLow, true), 0.0));
                standardDevUpp = std::sqrt(std::max(volatility_->blackVariance(d, strikeUpp, true), 0.0));
            }
            Real inRangeProbability = 0.0;
            
            if (standardDevLow < minStd) { // calculate intrinsic value
                probLow = (indexObservation < strikeLow) ? (1.0) : (0.0);
            } else { // digital option 

                probLow = ProbFromDigital(index, d, coupon.date(), strikeLow);

            }
            
            if (standardDevUpp < minStd) { // calculate intrinsic value
                probUpp = (indexObservation < strikeUpp) ? (1.0) : (0.0);
            } else { // digital option

                probUpp = ProbFromDigital(index, d, coupon.date(), strikeUpp);

            }
            inRangeProbability = probUpp - probLow;
            daysInRange += inRangeProbability;
            
            //additional results
            std::ostringstream s;
            s << io::iso_date(d);
            std::string date_s = s.str();

            additionalResults_["indexObservation_" + date_s] = indexObservation;
            additionalResults_["standardDevLow_" + date_s] = standardDevLow;
            additionalResults_["standardDevUpp_" + date_s] = standardDevUpp;
            additionalResults_["inRangeProbability_" + date_s] = inRangeProbability;
        }

        rangeAccrual_ = daysInRange / observationDays;
        additionalResults_["daysInRange"] = daysInRange;
        additionalResults_["observationDays"] = (double) observationDays;
    }

    Real FxRangeAccrualFixedCouponPricer::rangeAccrual() const {
        return rangeAccrual_;
    }

    Real FxRangeAccrualFixedCouponPricer::ProbFromDigital(const ext::shared_ptr<FxIndex>& fxIndex,
                                                          const Date& exerciseDate,
                                                          const Date& paymentDate,
                                                          const Real optionStrike)
    {
        return 0.0;
    }



}
