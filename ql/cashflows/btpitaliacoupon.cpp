/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2025 André Miemiec
 
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


#include <ql/cashflows/cashflowvectors.hpp>
#include <ql/cashflows/btpitaliacoupon.hpp>
#include <ql/cashflows/btpitaliacouponpricer.hpp>
#include <ql/cashflows/inflationcoupon.hpp>
#include <ql/cashflows/simplecashflow.hpp>
#include <ql/time/daycounters/thirty360.hpp>
#include <ql/time/daycounters/actualactual.hpp>
#include <utility>


namespace QuantLib {

    QL_DEPRECATED_DISABLE_WARNING


    BTPItaliaCoupon::BTPItaliaCoupon(
                         const Date& paymentDate,
                         Real nominal,
                         const Date& startDate,
                         const Date& endDate,
                         const ext::shared_ptr<ZeroInflationIndex>& index,
                         const Period& observationLag,
                         CPI::InterpolationType observationInterpolation,
                         std::vector<Date> observationDates,
                         const DayCounter& dayCounter,
                         Real fixedRate,
                         const Date& refPeriodStart,
                         const Date& refPeriodEnd,
                         const Date& exCouponDate)
    : InflationCoupon(paymentDate, nominal, startDate, endDate, 0,
                      index, observationLag, dayCounter,
                      refPeriodStart, refPeriodEnd, exCouponDate), 
                      observationInterpolation_(observationInterpolation),
                      observationDates_(0),
                      fixedRate_(InterestRate(fixedRate, dayCounter, Simple, NoFrequency))
    {
        QL_REQUIRE(index_, "no inflation index provided");

        //extract past and current period start dates
        for (int i = 0; i < observationDates.size();i++) {
            if (refPeriodStart > observationDates[i])
                observationDates_.push_back(observationDates[i]);
        }
        observationDates_.push_back(refPeriodStart);

    }


    void BTPItaliaCoupon::accept(AcyclicVisitor& v) {
        auto* v1 = dynamic_cast<Visitor<BTPItaliaCoupon>*>(&v);
        if (v1 != nullptr)
            v1->visit(*this);
        else
            InflationCoupon::accept(v);
    }


    Real BTPItaliaCoupon::amount() const  {

        //tbd: path dependency of the option, that depends on the past index fixings according to the list of dates in observationDates_

        Real adjustmtAmt = nominal() * std::max(indexRatio(referencePeriodEnd()) - 1.0, 0.0);

        Real interestAmt = fixedRate_.rate() * accrualPeriod() * (nominal() + adjustmtAmt);
                               
        return interestAmt + adjustmtAmt;
    }

    // is the accrued interest as for a standard bond, i.e. 
    // it does not take the indexation coefficient into account
    Real BTPItaliaCoupon::accruedAmount(const Date& d) const {
        if (d <= accrualStartDate_ || d > paymentDate_) {
            return 0.0;
        } else {
            return fixedRate_.rate() * accruedPeriod(d) * nominal();   
        }
    }

    Real BTPItaliaCoupon::indexRatio(Date d) const {

        Real I0 = CPI::laggedFixing(CPIIndex(), referencePeriodStart(),
                                    observationLag(),
                                    observationInterpolation()); 


        Real I1 = CPI::laggedFixing(CPIIndex(),
                                    d,
                                    observationLag(),
                                    observationInterpolation());

        return I1 / I0;
    }


    bool BTPItaliaCoupon::checkPricerImpl(const ext::shared_ptr<InflationCouponPricer>& pricer) const {
        return static_cast<bool>(ext::dynamic_pointer_cast<BTPItaliaCouponPricer>(pricer));
    }


    BTPItaliaLeg::BTPItaliaLeg(Schedule schedule,
                   ext::shared_ptr<ZeroInflationIndex> index,
                   const Period& observationLag)
    : schedule_(std::move(schedule)), index_(std::move(index)), 
      observationLag_(observationLag) {}


    BTPItaliaLeg& BTPItaliaLeg::withObservationInterpolation(CPI::InterpolationType interp) {
        observationInterpolation_ = interp;
        return *this;
    }


    BTPItaliaLeg& BTPItaliaLeg::withFixedRates(Real fixedRate) {
        fixedRates_ = std::vector<Real>(1,fixedRate);
        return *this;
    }

    BTPItaliaLeg& BTPItaliaLeg::withFixedRates(const std::vector<Real>& fixedRates) {
        fixedRates_ =   fixedRates;
        return *this;
    }

    BTPItaliaLeg& BTPItaliaLeg::withNotionals(Real notional) {
        notionals_ = std::vector<Real>(1,notional);
        return *this;
    }

    BTPItaliaLeg& BTPItaliaLeg::withNotionals(const std::vector<Real>& notionals) {
        notionals_ = notionals;
        return *this;
    }


    BTPItaliaLeg& BTPItaliaLeg::withPaymentDayCounter(const DayCounter& dayCounter) {
        paymentDayCounter_ = dayCounter;
        return *this;
    }

    BTPItaliaLeg& BTPItaliaLeg::withPaymentAdjustment(BusinessDayConvention convention) {
        paymentAdjustment_ = convention;
        return *this;
    }

    BTPItaliaLeg& BTPItaliaLeg::withPaymentCalendar(const Calendar& cal) {
        paymentCalendar_ = cal;
        return *this;
    }


    BTPItaliaLeg& BTPItaliaLeg::withExCouponPeriod(
                        const Period& period,
                        const Calendar& cal,
                        BusinessDayConvention convention,
                        bool endOfMonth) {
        exCouponPeriod_ = period;
        exCouponCalendar_ = cal;
        exCouponAdjustment_ = convention;
        exCouponEndOfMonth_ = endOfMonth;
        return *this;
    }



    BTPItaliaLeg::operator Leg() const {

        QL_REQUIRE(!notionals_.empty(), "no notional given");
        Size n = schedule_.size()-1;
        Leg leg;
        leg.reserve(n+1);   // +1 for notional, we always have some sort ...


        if (n>0) {
            QL_REQUIRE(!fixedRates_.empty(), "no fixedRates given");

            Date refStart, start, refEnd, end;

            for (Size i=0; i<n; ++i) {
                refStart = start = schedule_.date(i);
                refEnd   =   end = schedule_.date(i+1);
                Date paymentDate = paymentCalendar_.adjust(end, paymentAdjustment_);

                Date exCouponDate;
                if (exCouponPeriod_ != Period())
                {
                    exCouponDate = exCouponCalendar_.advance(paymentDate,
                                                                -exCouponPeriod_,
                                                                exCouponAdjustment_,
                                                                exCouponEndOfMonth_);
                }

                if (i==0   && schedule_.hasIsRegular() && !schedule_.isRegular(i+1)) {
                    BusinessDayConvention bdc = schedule_.businessDayConvention();
                    refStart = schedule_.calendar().adjust(end - schedule_.tenor(), bdc);
                }
                if (i==n-1 && schedule_.hasIsRegular() && !schedule_.isRegular(i+1)) {
                    BusinessDayConvention bdc = schedule_.businessDayConvention();
                    refEnd = schedule_.calendar().adjust(start + schedule_.tenor(), bdc);
                }
                
                leg.push_back(ext::make_shared<BTPItaliaCoupon>(paymentDate,
                                                                detail::get(notionals_, i, 0.0),
                                                                start, end,
                                                                index_, observationLag_,
                                                                observationInterpolation_, 
                                                                schedule_.dates(),
                                                                paymentDayCounter_,
                                                                detail::get(fixedRates_, i, 0.0),
                                                                refStart, refEnd, exCouponDate));
                
            }
        }

        // in BTP Italia legs you always have a notional flow of some sort
        
        Date paymentDate = paymentCalendar_.adjust(schedule_.date(n), paymentAdjustment_);
        leg.push_back(ext::make_shared<SimpleCashFlow>
                          (detail::get(notionals_, n, 0.0), paymentDate));
        
        // in BTP Italia a bonus payment might occur

        // tbd: add a fixed cpn here in dependence of a switch w/o bonus
        
        setCouponPricer(leg, ext::make_shared<BTPItaliaCouponPricer>());

        return leg;
    }

}
