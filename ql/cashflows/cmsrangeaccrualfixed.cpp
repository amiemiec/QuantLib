/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
  Copyright (C) 2024 Sebastian Schlenkrich, Andre Miemiec

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
#include <ql/cashflows/cmscoupon.hpp>
#include <ql/cashflows/cmsrangeaccrualfixed.hpp>
#include <ql/cashflows/conundrumpricer.hpp>
#include <ql/indexes/swapindex.hpp>
#include <ql/math/distributions/normaldistribution.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>
#include <ql/termstructures/volatility/swaption/swaptionvolstructure.hpp>
#include <ql/time/schedule.hpp>
#include <ql/time/daycounters/actual360.hpp>
#include <cmath>
#include <utility>

namespace QuantLib {


    CmsRangeAccrualFixedCoupon::CmsRangeAccrualFixedCoupon(
        // FixedRateCoupon
        const Date& paymentDate,
        Real nominal,
        Real rate,
        const DayCounter& dayCounter,
        const Date& accrualStartDate,
        const Date& accrualEndDate,
        // RA feature
        // calculate observation schedule from coupon
        ext::shared_ptr<SwapIndex> swapIndex,
        Real lowerTrigger,
        Real upperTrigger,
        Natural lockout,
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
      swapIndex_(swapIndex), lowerTrigger_(lowerTrigger), upperTrigger_(upperTrigger), lockout_(lockout), 
      observationsSchedule_(0), pricer_(0), rangeAccrual_(0.0) 
    {
        QL_REQUIRE(swapIndex_, "swapIndex_ required.");
        QL_REQUIRE(lowerTrigger_ < upperTrigger_, "lowerTrigger_ < upperTrigger_ required.");

        Calendar cal = swapIndex->fixingCalendar();

        Date accrualStartDateMod = accrualStartDate;
        Date accrualEndDateMod = cal.advance(accrualEndDate, -lockout_ * Days);

        observationsSchedule_ = ext::make_shared<Schedule>(MakeSchedule()
                                .from(accrualStartDateMod)
                                .to(accrualEndDateMod)
                                .withFrequency(Daily)
                                .withCalendar(cal)
                                .withConvention(Following));

        QL_REQUIRE(observationsSchedule_, "observationsSchedule_ required.");
    }


    void CmsRangeAccrualFixedCoupon::performCalculations() const {
        FixedRateCoupon::performCalculations();
        if (pricer_) {
            pricer_->initialize(*this);
            rangeAccrual_ = pricer_->rangeAccrual();
            additionalResults_ = pricer_->additionalResults();
        } else {
            // calculate fall-back via intrinsic value
            Real inRange = 0.0;
            for (auto d : observationsSchedule()->dates()) {
                auto indexObservation = swapIndex()->fixing(d);
                if (indexObservation >= lowerTrigger() && indexObservation <= upperTrigger())
                    inRange += 1.0;
            }
            rangeAccrual_ = inRange / observationsSchedule()->dates().size();
        }
    }


    Real CmsRangeAccrualFixedCoupon::rangeAccrual() const {
        calculate();  // make sure member is calculated
        return rangeAccrual_;
    }


    Real CmsRangeAccrualFixedCoupon::amount() const {
        calculate();
        return FixedRateCoupon::amount() * rangeAccrual_;
    }


    void CmsRangeAccrualFixedCoupon::accept(AcyclicVisitor& v) {
        auto* v1 = dynamic_cast<Visitor<CmsRangeAccrualFixedCoupon>*>(&v);
        if (v1 != nullptr)
            v1->visit(*this);
        else
            FixedRateCoupon::accept(v);
    }


    void CmsRangeAccrualFixedCoupon::setPricer(const ext::shared_ptr<CmsRangeAccrualFixedCouponPricer>& pricer) 
    {
        if (pricer_ != nullptr)
            unregisterWith(pricer_);
        pricer_ = pricer;
        if (pricer_ != nullptr)
            registerWith(pricer_);
        update();
    }


    CmsRangeAccrualFixedCouponPricer::CmsRangeAccrualFixedCouponPricer(const ext::shared_ptr<CmsCouponPricer> cmsCouponPricer)
    : swaptionVolatility_(cmsCouponPricer->swaptionVolatility()), pricer_(), rangeAccrual_(Null<Real>()) 
    {
        // We allow a CmsCouponPricer here to enable a general interface.
        // However, for our implementation, we require a HaganPricer.
        // Consequently, we need to down-cast the pricer.
        QL_REQUIRE(cmsCouponPricer, "cmsCouponPricer is required.");
        pricer_ = ext::dynamic_pointer_cast<HaganPricer>(cmsCouponPricer);
        QL_REQUIRE(pricer_, "Cannot down-cast cmsCouponPricer to HaganPricer.");
    }


    Real CmsRangeAccrualFixedCouponPricer::ProbFromPutSpread(
        const ext::shared_ptr<SwapIndex>& swapIndex,
        const Date& exerciseDate,
        const Date& paymentDate,
        const Real optionStrike,
        const Real spreadWidth) 
    {
        QL_REQUIRE(pricer_, "pricer_ required.");
        QL_REQUIRE(spreadWidth > 0.0, "spreadWidth > 0.0 required.");
        //AMI:DEBUG: Was wird hier genau berechnet?
        CmsCoupon cmsCoupon(
            paymentDate,
            1.0,               // nominal
            exerciseDate,      // startDate
            exerciseDate + 1,  // endDate
            0,                 // fixingDays
            swapIndex,
            1.0,               // gearing
            0.0,               // spread
            Date(),            // refPeriodStart
            Date(),            // refPeriodEnd
            Actual360()
        );
        cmsCoupon.setPricer(pricer_);
        cmsCoupon.performCalculations();
        Real swapRate = swapIndex->fixing(exerciseDate);
        Real putSprd = 0.0;
        if (optionStrike > swapRate) {  // calculate call spread to improve numerical stability
            Real calPlus = pricer_->capletRate(optionStrike + 0.5 * spreadWidth);
            Real calMins = pricer_->capletRate(optionStrike - 0.5 * spreadWidth);
            putSprd = 1.0 - (calMins - calPlus) / spreadWidth;
        } else {
            Real putPlus = pricer_->floorletRate(optionStrike + 0.5 * spreadWidth);
            Real putMins = pricer_->floorletRate(optionStrike - 0.5 * spreadWidth);
            putSprd = (putPlus - putMins) / spreadWidth;
        }
        return putSprd;
    }


    void CmsRangeAccrualFixedCouponPricer::initialize(const CmsRangeAccrualFixedCoupon& coupon) {
        additionalResults_.clear();
        
        Real minStd = 0.000005;  // 1bp * sqrt(1d)
        Real strikeLow = coupon.lowerTrigger();
        Real strikeUpp = coupon.upperTrigger();
        boost::shared_ptr<SwapIndex> index = coupon.swapIndex();

        CumulativeNormalDistribution Phi;
        
        Real daysInRange = 0.0;
        for (auto d : coupon.observationsSchedule()->dates()) 
        {
            Real indexObservation = index->fixing(d);
            Real standardDevLow = 0.0;
            Real standardDevUpp = 0.0;
            //
            Real probLow = 0.0;
            Real probUpp = 0.0;
            if (d > swaptionVolatility_->referenceDate()) {
                standardDevLow = std::sqrt(std::max(swaptionVolatility_->blackVariance(d,index->tenor(), strikeLow, true), 0.0));
                standardDevUpp = std::sqrt(std::max(swaptionVolatility_->blackVariance(d,index->tenor(), strikeUpp, true), 0.0));
            }
            Real inRangeProbability = 0.0;
            //
            if (standardDevLow < minStd) { // calculate intrinsic value
                probLow = (indexObservation < strikeLow) ? (1.0) : (0.0);
            } else {
                if (pricer_) { // replication
                    //AMI: coupon.date() ?
                    probLow = ProbFromPutSpread(index, d, coupon.date(), strikeLow);
                } else { // fall-back to Bachelier w/o CMS adjustment
                    probLow = Phi((strikeLow - indexObservation) / standardDevLow);
                }
            }
            //
            if (standardDevUpp < minStd) { // calculate intrinsic value
                probUpp = (indexObservation < strikeUpp) ? (1.0) : (0.0);
            } else {
                if (pricer_) { // replication
                    probUpp = ProbFromPutSpread(index, d, coupon.date(), strikeUpp);
                } else { // fall-back to Bachelier w/o CMS adjustment
                    probUpp = Phi((strikeUpp - indexObservation) / standardDevUpp);
                }
            }
            inRangeProbability = probUpp - probLow;
            daysInRange += inRangeProbability;
            
            //debug infos 
            std::ostringstream s;
            s << io::iso_date(d);
            std::string date_s = s.str();

            additionalResults_["indexObservation_" + date_s] = indexObservation;
            additionalResults_["standardDevLow_" + date_s] = standardDevLow;
            additionalResults_["standardDevUpp_" + date_s] = standardDevUpp;
            additionalResults_["inRangeProbability_" + date_s] = inRangeProbability;
        }
        Size observationDays = coupon.observationsSchedule()->dates().size();
        rangeAccrual_ = daysInRange / observationDays;
        additionalResults_["daysInRange"] = daysInRange;
        additionalResults_["observationDays"] = (Real) observationDays;
    }

    Real CmsRangeAccrualFixedCouponPricer::rangeAccrual() const {
        return rangeAccrual_;
    }

}
