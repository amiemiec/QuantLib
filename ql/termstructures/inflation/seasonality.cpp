/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2008 Piero Del Boca
 Copyright (C) 2009 Chris Kenyon
 Copyright (C) 2015 Bernd Lewerenz
 Copyright (C) 2024 André Miemiec

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


#include <ql/termstructures/inflation/seasonality.hpp>
#include <ql/termstructures/inflationtermstructure.hpp>
#include <ql/errors.hpp>
#include <ql/time/daycounters/one.hpp>

namespace QuantLib {

    MultiplicativePriceSeasonality::MultiplicativePriceSeasonality(
        //const Date& baseDate,
        const Frequency frequency,
        const std::vector<Real>& seasonalityData)
    : frequency_(frequency), /* baseDate_(baseDate),*/ seasonalityData_(seasonalityData) 
    {
        QL_REQUIRE(seasonalityData.size() == frequency, "Size of seasonality vector does not match");
    }


    Rate MultiplicativePriceSeasonality::correctZeroRate(const Date &date,
                                                         const Rate rate,
                                                         const InflationTermStructure& iTS) const 
    {
        //QL_REQUIRE(baseDate() == iTS.baseDate(), "base dates of inflation term structure and seasonality should be alligned");

        std::pair<Date, Date> lim = inflationPeriod(date, iTS.frequency());

        return MultiplicativePriceSeasonality::seasonalityCorrectionImpl(
            rate, lim.first, iTS.dayCounter(), iTS.baseDate(), RateType::Zero);
    }


    Rate MultiplicativePriceSeasonality::correctYoYRate(const Date& date,
                                                        const Rate rate,
                                                        const InflationTermStructure& iTS) const 
    {
        //QL_REQUIRE(baseDate() == iTS.baseDate(),
        //           "base dates of inflation term structure and seasonality should be alligned");

        std::pair<Date, Date> lim = inflationPeriod(iTS.baseDate(), iTS.frequency());

        return seasonalityCorrectionImpl(rate, date, iTS.dayCounter(),lim.second, RateType::YoY);

    }


    Real MultiplicativePriceSeasonality::seasonalityFactor(const Date& to) const {

        /* simplified old code:
        Date from = baseDate();
        Size N = seasonalityFactors().size();
        //OneDayCounter dcc;

        signed sign; sign = (to > from) ? 1 : -1;

        Size periodsInFromTo;

        if (to != from) {
             
            if (frequency() == Daily) {
                QL_FAIL("a seasonality specification on a daily basis is not allowed");
                //periodsInFromTo = sign * dcc.dayCount(from,to); // approx  only 
            } else if (frequency() == Weekly) {
                QL_FAIL("a seasonality specification on a weekly basis is not allowed");
                //periodsInFromTo = sign * (int)(dcc.dayCount(from, to) / 7); // approx only 
            } else if (frequency() == Monthly) {
                std::pair<Date, Date> fmlim = inflationPeriod(from, frequency());
                std::pair<Date, Date> tolim = inflationPeriod(to, frequency());

                periodsInFromTo = 12 * (tolim.first.year() - fmlim.first.year()) +
                                  (tolim.first.month() - fmlim.first.month());               
                periodsInFromTo *= sign;
            } else if (frequency() == Annual) {
                QL_FAIL("a seasonality specification on a yearly basis is not allowed");
            } else {
                QL_FAIL("Unknown frequency provided: " << frequency());
            }
        } else {
            return seasonalityFactors()[0];
        }

        Size which = sign * (periodsInFromTo % N);
             which += (sign == -1) ? N : 0;

        return seasonalityFactors()[which];
        */

        if (frequency() == Daily) {
            QL_FAIL("a seasonality specification on a daily basis is not allowed");
            // periodsInFromTo = sign * dcc.dayCount(from,to); // approx  only
        } else if (frequency() == Weekly) {
            QL_FAIL("a seasonality specification on a weekly basis is not allowed");
            // periodsInFromTo = sign * (int)(dcc.dayCount(from, to) / 7); // approx only
        } else if (frequency() == Monthly) {

            std::pair<Date, Date> tolim = inflationPeriod(to, frequency());
            Real aux = 1.0;
            for (int i = 0; i < tolim.first.month(); i++)
                aux *= seasonalityFactors()[i];
            return aux;

        } else if (frequency() == Annual) {
            QL_FAIL("a seasonality specification on a yearly basis is not allowed");
        } else {
            QL_FAIL("Unknown frequency provided: " << frequency());
        }

    }


    Rate MultiplicativePriceSeasonality::seasonalityCorrectionImpl( Rate rate,
                                                                    const Date& date,
                                                                    const DayCounter& dc,
                                                                    const Date& baseDate,
                                                                    const RateType type) const {
        // need _two_ corrections in order to get: seasonality = factor[atDate-seasonalityBase] / factor[reference-seasonalityBase]
        // i.e. for ZERO inflation rates you have the true fixing at the curve base so this factor must be normalized to one
        //      for YoY inflation rates your reference point is the year before

        Real dueDateFactor = seasonalityFactor(date);

        //Getting seasonality correction for either ZC or YoY
        Rate f = 0.0;
        if (type == Zero) {
            //ZC
            Rate refDateFactor = seasonalityFactor(baseDate);    
            std::pair<Date,Date> lim = inflationPeriod(date,frequency());
            Time timeFromBase = dc.yearFraction(baseDate, lim.first);
 
            Real seasonalityAt = dueDateFactor / refDateFactor;
 
            f = std::pow(seasonalityAt, 1/timeFromBase);
        }
        else { 
            //YoY
            Rate refDateFactor = seasonalityFactor(date - Period(1,Years));
            f = dueDateFactor / refDateFactor;
        }

        return (1+rate)*f - 1;    // at time T (timeFromBase) this corresponds to (1+r)^T*S(T)/S(B)
    }



    KerkhofSeasonality::KerkhofSeasonality(const Date& baseDate,
                                           const std::vector<Real>& seasonalityData)
    : MultiplicativePriceSeasonality(/*seasonalityBaseDate,*/ Monthly, seasonalityData), baseDate_(baseDate) {
        QL_REQUIRE(seasonalityData.size() == 12, "12 monthly seasonal factors needed for Kerkhof seasonality.");
    }


    Rate KerkhofSeasonality::correctZeroRate(const Date& date,
                                              const Rate rate,
                                              const InflationTermStructure& iTS) const {
        QL_REQUIRE(baseDate() == iTS.baseDate(),
                   "base dates of inflation term structure and seasonality should be alligned");
        std::pair<Date, Date> lim = inflationPeriod(date, iTS.frequency());

        return KerkhofSeasonality::seasonalityCorrectionImpl(rate, lim.first,
                                                              iTS.dayCounter(), iTS.baseDate(), RateType::Zero);
    }


    Rate KerkhofSeasonality::correctYoYRate(const Date& date,
                                            Rate rate,
                                            const InflationTermStructure& iTS) const {
        QL_FAIL("Kerkhof does not implement seasonality correction for YoY rates");
    }

    Real KerkhofSeasonality::seasonalityFactor(const Date& to) const {

        //AMI: tbd

        Date from = baseDate();

        if (from.month() == to.month())
            return seasonalityFactors()[0];

        Real factor = 1.0;

        if (from.month() > to.month()) {
            for (Size i = from.month(); i < to.month(); i++) {
                factor *= seasonalityFactors()[i-1];
            }        
        } else {
            for (Size i = to.month(); i < from.month(); i++) {
                factor /= seasonalityFactors()[i-1];
            }        
        }

        return factor;
    }


    
    Rate KerkhofSeasonality::seasonalityCorrectionImpl( Rate rate,
                                                        const Date& date,
                                                        const DayCounter& dc,
                                                        const Date& baseDate,
                                                        const RateType type) const 
    {
        //AMI: tbd
        Real dueDateFactor = seasonalityFactor(date);

        // Getting seasonality correction
        Rate f = 0.0;
        if (type == Zero) {
            //ZC
            std::pair<Date,Date> lim = inflationPeriod(baseDate, Monthly);
            Time timeFromBase = dc.yearFraction(lim.first, date);
            f = std::pow(dueDateFactor, 1/timeFromBase);                  //AMI: ??
        }
        else {
            //YoY
            QL_FAIL("Seasonal Kerkhof model is not defined on YoY rates");
        }
        return (rate + 1) * f - 1;                                    //AMI: ??
    }
    
}
