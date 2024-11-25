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

/*! \file fxrangeaccrualfixed.hpp
    \brief fx range-accrual coupon
*/

#ifndef quantlib_fx_range_accrual_fixed_h
#define quantlib_fx_range_accrual_fixed_h

#include <ql/time/schedule.hpp>
#include <ql/cashflows/fixedratecoupon.hpp>

namespace QuantLib {

    class FxIndex;
    class BlackVolTermStructure;
    class FxRangeAccrualFixedCouponPricer;

    class FxRangeAccrualFixedCoupon: public FixedRateCoupon {

      public:

        FxRangeAccrualFixedCoupon(
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
            const Date& refPeriodStart = Date(),
            const Date& refPeriodEnd = Date(),
            const Date& exCouponDate = Date());

        //@}
        //! \name LazyObject interface
        //@{
        void performCalculations() const override;
        //@}
        //! \name Coupon interface
        //@{
        Real amount() const override;
        Real accruedAmount(const Date&) const override;
        //@}

        std::vector<Date> observationDates() const { return observationDates_; }
        ext::shared_ptr<FxIndex> index() const { return index_; }
        Real lowerTrigger() const { return lowerTrigger_; }
        Real upperTrigger() const { return upperTrigger_; }
        Real rangeAccrual() const;
        Real deterministicRangeAccrual(const Date& d) const;

        //! \name Visitability
        //@{
        void accept(AcyclicVisitor&) override;
        //@}

        void setPricer(const ext::shared_ptr<FxRangeAccrualFixedCouponPricer>&);

        std::map<std::string, Real>& additionalResults() const { return additionalResults_; }

      private:

        ext::shared_ptr<Schedule> observationSchedule_; //to be removed
        std::vector<Date> observationDates_;
        ext::shared_ptr<FxIndex> index_;
        Real lowerTrigger_;
        Real upperTrigger_;
        Natural shifter_;
        bool accrualStartDateIncl_;
        bool accrualEndDateExcl_;

        ext::shared_ptr<FxRangeAccrualFixedCouponPricer> pricer_;
        mutable Real rangeAccrual_;
        mutable std::map<std::string, Real> additionalResults_;
     };


    class FxRangeAccrualFixedCouponPricer
    : public virtual Observer,
      public virtual Observable {
      public:

        FxRangeAccrualFixedCouponPricer(
            Handle<BlackVolTermStructure> volatility
        );

        void initialize(const FxRangeAccrualFixedCoupon& coupon);

        Real rangeAccrual() const;

        std::map<std::string, Real>& additionalResults() const { return additionalResults_; }

      //! \name Observer interface
      //@{
      void update() override { notifyObservers(); }
      //@}

      protected:
        Handle<BlackVolTermStructure> volatility_;
        Real rangeAccrual_;
        mutable std::map<std::string, Real> additionalResults_;

      private:
        Real ProbFromDigital(
            const ext::shared_ptr<FxIndex>& index,
            const Date& exerciseDate,
            const Date& paymentDate,
            const Real optionStrike,
            const Real spreadWidth = 1.0e-3 
        );

    };

        //! helper class building a sequence of range-accrual coupons
    class FxRangeAccrualLeg {
      public:
        FxRangeAccrualLeg(Schedule schedule,
                           ext::shared_ptr<FxIndex> index,
                           ext::shared_ptr<FxRangeAccrualFixedCouponPricer> pricer);
        FxRangeAccrualLeg& withNotionals(Real notional);
        FxRangeAccrualLeg& withNotionals(const std::vector<Real>& notionals);
        FxRangeAccrualLeg& withPaymentDayCounter(const DayCounter&);
        FxRangeAccrualLeg& withPaymentAdjustment(BusinessDayConvention);
        FxRangeAccrualLeg& withFixingDays(Natural fixingDays);
        FxRangeAccrualLeg& withFixingDays(const std::vector<Natural>& fixingDays);
        FxRangeAccrualLeg& withFixedRates(Rate fixedRate);
        FxRangeAccrualLeg& withFixedRates(const std::vector<Rate>& fixedRates);
        FxRangeAccrualLeg& withLowerTriggers(Rate trigger);
        FxRangeAccrualLeg& withLowerTriggers(const std::vector<Rate>& triggers);
        FxRangeAccrualLeg& withUpperTriggers(Rate trigger);
        FxRangeAccrualLeg& withUpperTriggers(const std::vector<Rate>& triggers);
        FxRangeAccrualLeg& withObservationShifters(Natural shifter);
        FxRangeAccrualLeg& withObservationShifters(const std::vector<Natural>& shifters);
        operator Leg() const;

      private:
        Schedule schedule_;
        ext::shared_ptr<FxIndex> index_;
        std::vector<Real> notionals_;
        DayCounter paymentDayCounter_;
        BusinessDayConvention paymentAdjustment_;
        std::vector<Natural> fixingDays_;
        std::vector<Rate> fixedRates_;
        std::vector<Rate> lowerTriggers_, upperTriggers_;
        std::vector<Natural> shifters_;
        ext::shared_ptr<FxRangeAccrualFixedCouponPricer> pricer_;
    };



}


#endif
