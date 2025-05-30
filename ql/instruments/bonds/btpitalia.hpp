/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2025 Andre Miemiec

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

/*! \file btpitalia.hpp
 \brief inflation bond with resetting principal adjustment and a fixed rate payed on adusted notional.
 */

#ifndef quantlib_btpitalia_hpp
#define quantlib_btpitalia_hpp


#include <ql/instruments/bond.hpp>
#include <ql/time/dategenerationrule.hpp>
#include <ql/time/daycounter.hpp>
#include <ql/interestrate.hpp>
#include <ql/cashflows/btpitaliacoupon.hpp>

namespace QuantLib {

    class Schedule;

    //! btp italia bond.
    //! 
    /*! \ingroup instruments

     */
    class BTPItalia : public Bond {
      public:
        BTPItalia(Natural settlementDays,
                Real faceAmount,
                const Period& observationLag,
                ext::shared_ptr<ZeroInflationIndex> cpiIndex,
                CPI::InterpolationType observationInterpolation,
                Schedule schedule,
                const std::vector<Rate>& coupons,
                const DayCounter& accrualDayCounter,
                BusinessDayConvention paymentConvention = ModifiedFollowing,
                const Date& issueDate = Date(),
                const Calendar& paymentCalendar = Calendar(),
                const Period& exCouponPeriod = Period(),
                const Calendar& exCouponCalendar = Calendar(),
                BusinessDayConvention exCouponConvention = Unadjusted,
                bool exCouponEndOfMonth = false);

        Frequency frequency() const { return frequency_; }
        const DayCounter& dayCounter() const { return dayCounter_; }
        Period observationLag() const { return observationLag_; }
        const ext::shared_ptr<ZeroInflationIndex>& cpiIndex() const { return cpiIndex_; }
        CPI::InterpolationType observationInterpolation() const { return observationInterpolation_; }

      protected:
        Frequency frequency_;
        DayCounter dayCounter_;
        Period observationLag_;
        ext::shared_ptr<ZeroInflationIndex> cpiIndex_;
        CPI::InterpolationType observationInterpolation_;
    };


}






#endif
