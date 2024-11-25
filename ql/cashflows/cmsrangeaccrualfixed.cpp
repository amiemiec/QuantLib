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
        ext::shared_ptr<SwapIndex> index,
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
      index_(index), lowerTrigger_(lowerTrigger), upperTrigger_(upperTrigger), lockout_(lockout),
      crystallizedAt_(lockout),
      observationSchedule_(0),observationDates_(0), pricer_(0), rangeAccrual_(0.0), accrualStartDateIncl_(true),
      accrualEndDateExcl_(true) 
    {
        QL_REQUIRE(index_, "swapIndex_ required.");
        QL_REQUIRE(lowerTrigger_ < upperTrigger_, "lowerTrigger_ < upperTrigger_ required.");

        Calendar cal = index->fixingCalendar();

        Date accrualStartDateMod = accrualStartDate;
        Date accrualEndDateMod = cal.advance(accrualEndDate,-(signed)(lockout_)*Days);

        observationSchedule_ = ext::make_shared<Schedule>(MakeSchedule()
                                .from(accrualStartDateMod)
                                .to(accrualEndDateMod)
                                .withFrequency(Daily)
                                .withCalendar(cal)
                                .withConvention(Following));

        observationDates_ = observationSchedule_->dates();

        Date lastNonCrystallizedDate = observationDates_.back();
  
        if (crystallizedAt_ > 0) {
            for (Size i = 0; i < crystallizedAt_; i++) {
                observationDates_.push_back(lastNonCrystallizedDate);
            }
        }
        

        QL_REQUIRE(observationSchedule_, "observationsSchedule_ required.");
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
            for (auto d : observationDates_) {
                auto indexObservation = index()->fixing(d);
                if (indexObservation >= lowerTrigger() && indexObservation <= upperTrigger())
                    inRange += 1.0;
            }
            rangeAccrual_ = inRange / observationDates_.size();
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


    Real CmsRangeAccrualFixedCoupon::accruedAmount(const Date& d) const {
        calculate();
        Date accruedAmountSettlementDate = index_->fixingCalendar().advance(d, 1 * Days);
        return FixedRateCoupon::accruedAmount(accruedAmountSettlementDate) *
               deterministicRangeAccrual(d);
    }


    Real CmsRangeAccrualFixedCoupon::deterministicRangeAccrual(const Date& d) const {
        Date refDate = Settings::instance().evaluationDate();

        Natural accrualDays =
            index_->fixingCalendar().businessDaysBetween(accrualStartDate_, refDate);
        accrualDays += 1; // refDate included

        Date accrualDate = index_->fixingCalendar().advance(refDate, 1 * Days);
        Date lastRelevantObsDate = accrualDate;

        Natural inRange = 0;

        for each (Date dt in observationDates()) {
            if (dt < lastRelevantObsDate) {
                Real observation = index_->fixing(dt);
                if (!((observation < lowerTrigger_) || (observation > upperTrigger_))) {
                    inRange += 1;
                }
            }
        }
        
        return (Real)(inRange) / (Real)(accrualDays);
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


    CmsRangeAccrualFixedCouponPricer::CmsRangeAccrualFixedCouponPricer(const ext::shared_ptr<CmsCouponPricer> pricer)
    : volatility_(pricer->swaptionVolatility()), pricer_(0), rangeAccrual_(0.0) 
    {
        // We allow a CmsCouponPricer here to enable a general interface.
        // However, for our implementation, we require a HaganPricer.
        // Consequently, we need to down-cast the pricer.
        QL_REQUIRE(pricer, "cmsCouponPricer is required.");
        pricer_ = ext::dynamic_pointer_cast<HaganPricer>(pricer);
        QL_REQUIRE(pricer_, "Cannot down-cast cmsCouponPricer to HaganPricer.");
    }




    void CmsRangeAccrualFixedCouponPricer::initialize(const CmsRangeAccrualFixedCoupon& coupon) {
        additionalResults_.clear();
        
        Real minStd = 0.000005;  // 1bp * sqrt(1d)
        Real strikeLow = coupon.lowerTrigger();
        Real strikeUpp = coupon.upperTrigger();
        boost::shared_ptr<SwapIndex> index = coupon.index();

        CumulativeNormalDistribution Phi;
        
        Real daysInRange = 0.0;
        Size observationDays = coupon.observationDates().size();

        for (auto d : coupon.observationDates()) 
        {
            Real indexObservation = index->fixing(d);
            Real standardDevLow = 0.0;
            Real standardDevUpp = 0.0;
            //
            Real probLow = 0.0;
            Real probUpp = 0.0;

            if (d > volatility_->referenceDate()) {
                standardDevLow = std::sqrt(std::max(volatility_->blackVariance(d,index->tenor(), strikeLow, true), 0.0));
                standardDevUpp = std::sqrt(std::max(volatility_->blackVariance(d,index->tenor(), strikeUpp, true), 0.0));
            }
           
            Real inRangeProbability = 0.0;
            
            if (standardDevLow < minStd) { // calculate intrinsic value
                probLow = (indexObservation < strikeLow) ? (1.0) : (0.0);
            } else {
                if (pricer_) { // replication
                    //AMI: coupon.date() ?
                    probLow = ProbFromDigital(index, d, coupon.date(), strikeLow);
                } else { // fall-back to Bachelier w/o CMS adjustment
                    probLow = Phi((strikeLow - indexObservation) / standardDevLow);
                }
            }
            
            if (standardDevUpp < minStd) { // calculate intrinsic value
                probUpp = (indexObservation < strikeUpp) ? (1.0) : (0.0);
            } else {
                if (pricer_) { // replication
                    probUpp = ProbFromDigital(index, d, coupon.date(), strikeUpp);
                } else { // fall-back to Bachelier w/o CMS adjustment
                    probUpp = Phi((strikeUpp - indexObservation) / standardDevUpp);
                }
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
        additionalResults_["observationDays"] = (Real) observationDays;
    }

    Real CmsRangeAccrualFixedCouponPricer::rangeAccrual() const {
        return rangeAccrual_;
    }

    Real CmsRangeAccrualFixedCouponPricer::ProbFromDigital(const ext::shared_ptr<SwapIndex>& index,
                                                             const Date& exerciseDate,
                                                             const Date& paymentDate,
                                                             const Real optionStrike,
                                                             const Real spreadWidth) 
    {
        QL_REQUIRE(pricer_, "pricer_ required.");
        QL_REQUIRE(spreadWidth > 0.0, "spreadWidth > 0.0 required.");
        // AMI:DEBUG: Was wird hier genau berechnet?
        CmsCoupon cmsCoupon(paymentDate,
                            1.0,              // nominal
                            exerciseDate,     // startDate
                            exerciseDate + 1, // endDate
                            0,                // fixingDays
                            index,
                            1.0,    // gearing
                            0.0,    // spread
                            Date(), // refPeriodStart
                            Date(), // refPeriodEnd
                            Actual360());
        cmsCoupon.setPricer(pricer_);
        cmsCoupon.performCalculations();
        Real swapRate = index->fixing(exerciseDate);
        Real putSprd = 0.0;
        if (optionStrike > swapRate) { // calculate call spread to improve numerical stability
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




    CmsRangeAccrualLeg::CmsRangeAccrualLeg(Schedule schedule, ext::shared_ptr<SwapIndex> index, ext::shared_ptr<CmsRangeAccrualFixedCouponPricer> pricer)
    : schedule_(std::move(schedule)), index_(std::move(index)), pricer_(std::move(pricer)) {}

    CmsRangeAccrualLeg& CmsRangeAccrualLeg::withNotionals(Real notional) {
        notionals_ = std::vector<Real>(1, notional);
        return *this;
    }

    CmsRangeAccrualLeg& CmsRangeAccrualLeg::withNotionals(const std::vector<Real>& notionals) {
        notionals_ = notionals;
        return *this;
    }

    CmsRangeAccrualLeg& CmsRangeAccrualLeg::withPaymentDayCounter(const DayCounter& dayCounter) {
        paymentDayCounter_ = dayCounter;
        return *this;
    }

    CmsRangeAccrualLeg& CmsRangeAccrualLeg::withPaymentAdjustment(BusinessDayConvention convention) {
        paymentAdjustment_ = convention;
        return *this;
    }

    CmsRangeAccrualLeg& CmsRangeAccrualLeg::withFixingDays(Natural fixingDays) {
        fixingDays_ = std::vector<Natural>(1, fixingDays);
        return *this;
    }

    CmsRangeAccrualLeg& CmsRangeAccrualLeg::withFixingDays(const std::vector<Natural>& fixingDays) {
        fixingDays_ = fixingDays;
        return *this;
    }

    CmsRangeAccrualLeg& CmsRangeAccrualLeg::withFixedRates(Rate fixedRate) {
        fixedRates_ = std::vector<Rate>(1, fixedRate);
        return *this;
    }

    CmsRangeAccrualLeg& CmsRangeAccrualLeg::withFixedRates(const std::vector<Rate>& fixedRates) {
        fixedRates_ = fixedRates;
        return *this;
    }

    CmsRangeAccrualLeg& CmsRangeAccrualLeg::withLowerTriggers(Rate trigger) {
        lowerTriggers_ = std::vector<Rate>(1, trigger);
        return *this;
    }

    CmsRangeAccrualLeg& CmsRangeAccrualLeg::withLowerTriggers(const std::vector<Rate>& triggers) {
        lowerTriggers_ = triggers;
        return *this;
    }

    CmsRangeAccrualLeg& CmsRangeAccrualLeg::withUpperTriggers(Rate trigger) {
        upperTriggers_ = std::vector<Rate>(1, trigger);
        return *this;
    }

    CmsRangeAccrualLeg& CmsRangeAccrualLeg::withUpperTriggers(const std::vector<Rate>& triggers) {
        upperTriggers_ = triggers;
        return *this;
    }


    CmsRangeAccrualLeg& CmsRangeAccrualLeg::withObservationLockouts(Natural lockout) {
        lockouts_ = std::vector<Natural>(1, lockout);
        return *this;
    }


    CmsRangeAccrualLeg& CmsRangeAccrualLeg::withObservationLockouts(const std::vector<Natural>& lockouts) {
        lockouts_ = lockouts;
        return *this;
    }



    CmsRangeAccrualLeg::operator Leg() const {

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
            ext::shared_ptr<CmsRangeAccrualFixedCoupon> cpn = 
                ext::shared_ptr<CmsRangeAccrualFixedCoupon>(
                    new CmsRangeAccrualFixedCoupon(
                    paymentDate, detail::get(notionals_, i, Null<Real>()),
                    detail::get(fixedRates_, i, 0.0), paymentDayCounter_, start, end, index_,
                    detail::get(lowerTriggers_, i, 0.0), detail::get(upperTriggers_, i, 0.0),
                    detail::get(lockouts_, i, 0), refStart, refEnd));
            cpn->setPricer(pricer_);
            leg.push_back(ext::dynamic_pointer_cast<CashFlow>(cpn));
        }
        return leg;
    }



}
