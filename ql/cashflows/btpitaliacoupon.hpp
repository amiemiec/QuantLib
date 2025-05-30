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

/*! \file BTPItaliacoupon.hpp
    \brief Coupon paying a zero-inflation index
*/

#ifndef quantlib_btpitaliacoupon_hpp
#define quantlib_btpitaliacoupon_hpp

#include <ql/cashflows/inflationcoupon.hpp>
#include <ql/cashflows/indexedcashflow.hpp>
#include <ql/indexes/inflationindex.hpp>
#include <ql/time/schedule.hpp>

namespace QuantLib {


    class BTPItaliaCouponPricer;

    //! %Coupon paying the performance of the italian consumer price index for 
    //! blue and white-collar worker households (FOI) and a fixed coupon rate 
    //! based on a resetting and performance adjusted notional.
    //! 
    /*  Details can be found in the explanatory note on 
        www.dt.mef.gov.it/modules/documenti_en/debito_pubblico/titoli_di_stato/BTP_Italia.pdf
     
        The performance is relative to the index value on the refPeriodStart date 
        with observation lag. The other inflation value is taken from the refPeriodEnd date
        with observation lag.  
    */

    class BTPItaliaCoupon : public InflationCoupon {
      public:
        //! \name Constructors
        //@{
    
        BTPItaliaCoupon(
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
                  const Date& refPeriodStart = Date(),
                  const Date& refPeriodEnd = Date(),
                  const Date& exCouponDate = Date());
        //@}

        //! \name CashFlow interface
        //@{
        Real amount() const override;
        //@}

        //! \name Inspectors
        //@{
        //! fixed rate that will be inflated by the index ratio
        Real fixedRate() const;

        //! how do you observe the index?  as-is, flat, linear?
        CPI::InterpolationType observationInterpolation() const;

        //! index used
        ext::shared_ptr<ZeroInflationIndex> CPIIndex() const;
        //@}

        //! \name Calculations
        //@{
        Real accruedAmount(const Date&) const override;

        //! the index value observed (with a lag) at the end date
        Rate indexFixing() const override;

        //! the ratio between the index fixing at the passed date and the base BTPItalia
        /*! No adjustments are applied */
        Rate indexRatio(Date d) const;
        //@}

        //! \name Visitability
        //@{
        void accept(AcyclicVisitor&) override;
        //@}
      protected:

        InterestRate fixedRate_;
        CPI::InterpolationType observationInterpolation_;
        std::vector<Date> observationDates_;


        bool checkPricerImpl(const ext::shared_ptr<InflationCouponPricer>&) const override;
    };



    //! Helper class building a sequence of capped/floored BTP Italia coupons.
    /*! Also allowing for the inflated notional and possibly a bonus payment at the end.
    */
    class BTPItaliaLeg {
      public:
        BTPItaliaLeg(Schedule schedule,
               ext::shared_ptr<ZeroInflationIndex> index,
               const Period& observationLag);
        BTPItaliaLeg& withNotionals(Real notional);
        BTPItaliaLeg& withNotionals(const std::vector<Real>& notionals);
        BTPItaliaLeg& withFixedRates(Real fixedRate);
        BTPItaliaLeg& withFixedRates(const std::vector<Real>& fixedRates);
        BTPItaliaLeg& withSpreads(Spread spread);
        BTPItaliaLeg& withSpreads(const std::vector<Spread>& spreads);
        BTPItaliaLeg& withPaymentDayCounter(const DayCounter&);
        BTPItaliaLeg& withPaymentCalendar(const Calendar&);
        BTPItaliaLeg& withPaymentAdjustment(BusinessDayConvention);
        BTPItaliaLeg& withObservationInterpolation(CPI::InterpolationType);
        BTPItaliaLeg& withExCouponPeriod(const Period&,
                                         const Calendar&,
                                         BusinessDayConvention,
                                         bool endOfMonth = false);

        operator Leg() const;

      private:
        Schedule schedule_;

        ext::shared_ptr<ZeroInflationIndex> index_;
        Period observationLag_;
        CPI::InterpolationType observationInterpolation_; 

        std::vector<Real> notionals_;
        std::vector<Real> fixedRates_;

        DayCounter paymentDayCounter_;
        Calendar paymentCalendar_;
        BusinessDayConvention paymentAdjustment_; 
 
        Period exCouponPeriod_;
        Calendar exCouponCalendar_;
        BusinessDayConvention exCouponAdjustment_ = Following;
        bool exCouponEndOfMonth_ = false;
    };


    // inline definitions

    inline Real BTPItaliaCoupon::fixedRate() const {
        return fixedRate_;
    }


    inline Rate BTPItaliaCoupon::indexFixing() const {
        return CPI::laggedFixing(CPIIndex(), accrualEndDate(), observationLag(), observationInterpolation());
    }


    inline CPI::InterpolationType BTPItaliaCoupon::observationInterpolation() const {
        return observationInterpolation_;
    }

    inline ext::shared_ptr<ZeroInflationIndex> BTPItaliaCoupon::CPIIndex() const {
        return ext::dynamic_pointer_cast<ZeroInflationIndex>(index());
    }

}

#endif
