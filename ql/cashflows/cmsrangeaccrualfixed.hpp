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

/*! \file cmsrangeaccrualfixed.hpp
    \brief cms range-accrual coupon
*/

#ifndef quantlib_cms_range_accrual_fixed_h
#define quantlib_cms_range_accrual_fixed_h

#include <ql/time/schedule.hpp>
#include <ql/cashflows/fixedratecoupon.hpp>

namespace QuantLib {

    class SwapIndex;
    class SwaptionVolatilityStructure;
    class HaganPricer;
    class CmsCouponPricer;
    class CmsRangeAccrualFixedCouponPricer;

    class CmsRangeAccrualFixedCoupon: public FixedRateCoupon {

      public:

        CmsRangeAccrualFixedCoupon(
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
        ext::shared_ptr<SwapIndex> index() const { return index_; }
        Real lowerTrigger() const { return lowerTrigger_; }
        Real upperTrigger() const { return upperTrigger_; }
        Natural lockout() const { return lockout_; }
        Real rangeAccrual() const;
        Real deterministicRangeAccrual(const Date& d) const;


        //! \name Visitability
        //@{
        void accept(AcyclicVisitor&) override;
        //@}

        void setPricer(const ext::shared_ptr<CmsRangeAccrualFixedCouponPricer>&);

        std::map<std::string, Real>& additionalResults() const { return additionalResults_; }

      private:

        ext::shared_ptr<Schedule> observationSchedule_;  //to be removed
        mutable std::vector<Date> observationDates_;
        ext::shared_ptr<SwapIndex> index_;
        Real lowerTrigger_;
        Real upperTrigger_;
        Natural lockout_;
        Natural crystallizedAt_;
        bool accrualStartDateIncl_;
        bool accrualEndDateExcl_; 

        ext::shared_ptr<CmsRangeAccrualFixedCouponPricer> pricer_;
        mutable Real rangeAccrual_;
        mutable std::map<std::string, Real> additionalResults_;
     };


    class CmsRangeAccrualFixedCouponPricer: public virtual Observer, public virtual Observable {
      public:

        CmsRangeAccrualFixedCouponPricer(const ext::shared_ptr<CmsCouponPricer> pricer);

        void initialize(const CmsRangeAccrualFixedCoupon& coupon);

        Real rangeAccrual() const;

        std::map<std::string, Real>& additionalResults() const { return additionalResults_; }

      //! \name Observer interface
      //@{
      void update() override { notifyObservers(); }
      //@}

      protected:
        ext::shared_ptr<HaganPricer> pricer_;
        Handle<SwaptionVolatilityStructure> volatility_;
        Real rangeAccrual_;
        mutable std::map<std::string, Real> additionalResults_;

      private:
        Real ProbFromDigital(const ext::shared_ptr<SwapIndex>& index,
                             const Date& exerciseDate,
                             const Date& paymentDate,
                             const Real optionStrike,
                             const Real spreadWidth = 1.0e-4  // 1bp, be carefull with numerical instabilities
        );

    };




    //! helper class building a sequence of range-accrual coupons
    class CmsRangeAccrualLeg {
      public:
        CmsRangeAccrualLeg(Schedule schedule, ext::shared_ptr<SwapIndex> index,ext::shared_ptr<CmsRangeAccrualFixedCouponPricer> pricer);
        CmsRangeAccrualLeg& withNotionals(Real notional);
        CmsRangeAccrualLeg& withNotionals(const std::vector<Real>& notionals);
        CmsRangeAccrualLeg& withPaymentDayCounter(const DayCounter&);
        CmsRangeAccrualLeg& withPaymentAdjustment(BusinessDayConvention);
        CmsRangeAccrualLeg& withFixingDays(Natural fixingDays);
        CmsRangeAccrualLeg& withFixingDays(const std::vector<Natural>& fixingDays);
        CmsRangeAccrualLeg& withFixedRates(Rate fixedRate);
        CmsRangeAccrualLeg& withFixedRates(const std::vector<Rate>& fixedRates);
        CmsRangeAccrualLeg& withLowerTriggers(Rate trigger);
        CmsRangeAccrualLeg& withLowerTriggers(const std::vector<Rate>& triggers);
        CmsRangeAccrualLeg& withUpperTriggers(Rate trigger);
        CmsRangeAccrualLeg& withUpperTriggers(const std::vector<Rate>& triggers);
        CmsRangeAccrualLeg& withObservationLockouts(Natural lockout);
        CmsRangeAccrualLeg& withObservationLockouts(const std::vector<Natural>& lockouts);
        operator Leg() const;

      private:
        Schedule schedule_;
        ext::shared_ptr<SwapIndex> index_;
        std::vector<Real> notionals_;
        DayCounter paymentDayCounter_;
        BusinessDayConvention paymentAdjustment_;
        std::vector<Natural> fixingDays_;
        std::vector<Rate> fixedRates_;
        std::vector<Rate> lowerTriggers_, upperTriggers_;
        std::vector<Natural> lockouts_;
        ext::shared_ptr<CmsRangeAccrualFixedCouponPricer> pricer_;
    };




}


#endif
