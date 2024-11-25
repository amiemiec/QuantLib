/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2024 Andre Miemiec, Sebastian Schlenkrich

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
#include <ql/termstructures/volatility/fx/blackvolsurfacedelta.hpp>
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
        Natural shifter,
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
      observationSchedule_(0), observationDates_(0), pricer_(0), rangeAccrual_(0.0), shifter_(shifter),
      accrualStartDateIncl_(true), accrualEndDateExcl_(true) 
    {
        QL_REQUIRE(index_, "fxIndex_ required.");
        QL_REQUIRE(lowerTrigger_ > 0.0, "lowerTrigger_ > 0.0 required.");
        QL_REQUIRE(lowerTrigger_ < upperTrigger_, "lowerTrigger_ < upperTrigger_ required.");

        Calendar cal = index_->fixingCalendar();

        Date accrualStartDateMod = cal.advance(accrualStartDate,-(signed)(shifter) * Days);
        Date accrualEndDateMod = cal.advance(accrualEndDate, - (signed)(shifter_) * Days);

        observationSchedule_ = ext::make_shared<Schedule>(MakeSchedule()
                                                               .from(accrualStartDateMod)
                                                               .to(accrualEndDateMod)
                                                               .withFrequency(Daily)
                                                               .withCalendar(cal)
                                                               .withConvention(Following));

        QL_REQUIRE(observationSchedule_, "observationsSchedule_ required.");
        observationDates_ = observationSchedule_->dates();
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
            for (auto d : observationDates()) {
                auto indexObservation = index()->fixing(d);
                if (indexObservation >= lowerTrigger() && indexObservation <= upperTrigger())
                    inRange += 1.0;
            }
            rangeAccrual_ = inRange / observationDates().size();
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

     Real FxRangeAccrualFixedCoupon::accruedAmount(const Date& d) const {
        calculate();
        Date accruedAmountSettlementDate = index_->fixingCalendar().advance(d, 1 * Days);
        return FixedRateCoupon::accruedAmount(accruedAmountSettlementDate) *
               deterministicRangeAccrual(d);
     }


    Real FxRangeAccrualFixedCoupon::deterministicRangeAccrual(const Date& d) const {
        Date refDate = Settings::instance().evaluationDate();

        Natural accrualDays = index_->fixingCalendar().businessDaysBetween(accrualStartDate_, refDate);
        accrualDays += 1; //refDate included

        Date accrualDate = index_->fixingCalendar().advance(refDate, 1 * Days);
        Date lastRelevantObsDate =
            index_->fixingCalendar().advance(accrualDate, -(signed)(shifter_) * Days);
        
        Natural inRange = 0;

        for each (Date dt in observationSchedule_->dates()) {
            if (dt < lastRelevantObsDate) {
                Real observation = index_->fixing(dt);
                if (!((observation < lowerTrigger_) || (observation > upperTrigger_))) {
                    inRange += 1;
                }
            }
        }
        return (Real)(inRange)/(Real)(accrualDays);
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
        Size observationDays = coupon.observationDates().size();
        
        for (auto d : coupon.observationDates()) {
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
                                                          const Real optionStrike,
                                                          const Real spreadWidth)
    {
        Date referenceDate = Settings::instance().evaluationDate();
        Date spotDate = fxIndex->fixingCalendar().advance(referenceDate, 2 * Days);
        Date settlementDate = fxIndex->fixingCalendar().advance(exerciseDate, 2 * Days);

        Real spot = fxIndex->fixing(referenceDate);
        Real forward  = fxIndex->fixing(settlementDate);

        //Replication of Murex-Approach
        
        Real put_spread = 0.0;
        Real cll_spread = 0.0;

        Real variance   = volatility_->blackVariance(exerciseDate, optionStrike + spreadWidth);
        put_spread += blackFormula(Option::Put,  optionStrike + spreadWidth, forward, std::sqrt(variance), 1.0)/(optionStrike + spreadWidth);
        cll_spread -= blackFormula(Option::Call, optionStrike + spreadWidth, forward, std::sqrt(variance), 1.0)/(optionStrike + spreadWidth);
        
        variance = volatility_->blackVariance(exerciseDate, optionStrike);
        put_spread -= blackFormula(Option::Put,  optionStrike, forward, std::sqrt(variance), 1.0)/optionStrike;
        cll_spread += blackFormula(Option::Call, optionStrike, forward, std::sqrt(variance), 1.0)/optionStrike;

        //++AMI: numerically redundant
        put_spread /= spreadWidth;
        put_spread /= spot;

        cll_spread /= spreadWidth;
        cll_spread /= spot;
        //AMI++

        Real prob = put_spread / (put_spread + cll_spread);

        return prob;
    }


    FxRangeAccrualLeg::FxRangeAccrualLeg(Schedule schedule,
                                           ext::shared_ptr<FxIndex> index,
                                           ext::shared_ptr<FxRangeAccrualFixedCouponPricer> pricer)
    : schedule_(std::move(schedule)), index_(std::move(index)), pricer_(std::move(pricer)) {}

    FxRangeAccrualLeg& FxRangeAccrualLeg::withNotionals(Real notional) {
        notionals_ = std::vector<Real>(1, notional);
        return *this;
    }

    FxRangeAccrualLeg& FxRangeAccrualLeg::withNotionals(const std::vector<Real>& notionals) {
        notionals_ = notionals;
        return *this;
    }

    FxRangeAccrualLeg& FxRangeAccrualLeg::withPaymentDayCounter(const DayCounter& dayCounter) {
        paymentDayCounter_ = dayCounter;
        return *this;
    }

    FxRangeAccrualLeg&
    FxRangeAccrualLeg::withPaymentAdjustment(BusinessDayConvention convention) {
        paymentAdjustment_ = convention;
        return *this;
    }

    FxRangeAccrualLeg& FxRangeAccrualLeg::withFixingDays(Natural fixingDays) {
        fixingDays_ = std::vector<Natural>(1, fixingDays);
        return *this;
    }

    FxRangeAccrualLeg& FxRangeAccrualLeg::withFixingDays(const std::vector<Natural>& fixingDays) {
        fixingDays_ = fixingDays;
        return *this;
    }

    FxRangeAccrualLeg& FxRangeAccrualLeg::withFixedRates(Rate fixedRate) {
        fixedRates_ = std::vector<Rate>(1, fixedRate);
        return *this;
    }

    FxRangeAccrualLeg& FxRangeAccrualLeg::withFixedRates(const std::vector<Rate>& fixedRates) {
        fixedRates_ = fixedRates;
        return *this;
    }

    FxRangeAccrualLeg& FxRangeAccrualLeg::withLowerTriggers(Rate trigger) {
        lowerTriggers_ = std::vector<Rate>(1, trigger);
        return *this;
    }

    FxRangeAccrualLeg& FxRangeAccrualLeg::withLowerTriggers(const std::vector<Rate>& triggers) {
        lowerTriggers_ = triggers;
        return *this;
    }

    FxRangeAccrualLeg& FxRangeAccrualLeg::withUpperTriggers(Rate trigger) {
        upperTriggers_ = std::vector<Rate>(1, trigger);
        return *this;
    }

    FxRangeAccrualLeg& FxRangeAccrualLeg::withUpperTriggers(const std::vector<Rate>& triggers) {
        upperTriggers_ = triggers;
        return *this;
    }


    FxRangeAccrualLeg& FxRangeAccrualLeg::withObservationShifters(Natural shifter) {
        shifters_ = std::vector<Natural>(1, shifter);
        return *this;
    }


    FxRangeAccrualLeg&
    FxRangeAccrualLeg::withObservationShifters(const std::vector<Natural>& shifters) {
        shifters_ = shifters;
        return *this;
    }


    FxRangeAccrualLeg::operator Leg() const {

        QL_REQUIRE(!notionals_.empty(), "no notional given");

        Size n = schedule_.size() - 1;
        QL_REQUIRE(notionals_.size() <= n,
                   "too many nominals (" << notionals_.size() << "), only " << n << " required");
        QL_REQUIRE(fixingDays_.size() <= n,
                   "too many fixingDays (" << fixingDays_.size() << "), only " << n << " required");
        QL_REQUIRE(lowerTriggers_.size() <= n, "too many lowerTriggers (" << lowerTriggers_.size()
                                                                          << "), only " << n
                                                                          << " required");
        QL_REQUIRE(upperTriggers_.size() <= n, "too many upperTriggers (" << upperTriggers_.size()
                                                                          << "), only " << n
                                                                          << " required");

        Leg leg;
        leg.clear();

        // the following is not always correct
        Calendar calendar = schedule_.calendar();

        Date refStart, start, refEnd, end;
        Date paymentDate;

        for (Size i = 0; i < n; ++i) {
            refStart = start = schedule_.date(i);
            refEnd = end = schedule_.date(i + 1);
            paymentDate = calendar.adjust(end, paymentAdjustment_);
            if (i == 0 && schedule_.hasIsRegular() && !schedule_.isRegular(i + 1)) {
                BusinessDayConvention bdc = schedule_.businessDayConvention();
                refStart = calendar.adjust(end - schedule_.tenor(), bdc);
            }
            if (i == n - 1 && schedule_.hasIsRegular() && !schedule_.isRegular(i + 1)) {
                BusinessDayConvention bdc = schedule_.businessDayConvention();
                refEnd = calendar.adjust(start + schedule_.tenor(), bdc);
            }
            ext::shared_ptr<FxRangeAccrualFixedCoupon> cpn =
                ext::shared_ptr<FxRangeAccrualFixedCoupon>(new FxRangeAccrualFixedCoupon(
                    paymentDate, detail::get(notionals_, i, Null<Real>()),
                    detail::get(fixedRates_, i, 0.0), paymentDayCounter_, start, end, index_,
                    detail::get(lowerTriggers_, i, 0.0), detail::get(upperTriggers_, i, 0.0),
                    detail::get(shifters_, i, 0), refStart, refEnd));
            cpn->setPricer(pricer_);
            leg.push_back(ext::dynamic_pointer_cast<CashFlow>(cpn));
        }
        return leg;
    }


}
