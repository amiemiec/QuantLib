/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2001, 2002, 2003 Sadruddin Rejeb
 Copyright (C) 2004, 2007 StatPro Italia srl
 Author:   Andre Miemiec (2010)

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

/*! \file discretizedirregularswaption.hpp
    \brief Discretized Irregular Swaption class
*/

#ifndef quantlib_discretized_irregular_swaption_hpp
#define quantlib_discretized_irregular_swaption_hpp

#include <ql/experimental/swaptions/irregularswaption.hpp>
#include <ql/discretizedasset.hpp>

namespace QuantLib {

    class DiscretizedIrregularSwaption : public DiscretizedOption {
      public:
        DiscretizedIrregularSwaption(const IrregularSwaption::arguments&,
                                     const Date& referenceDate,
                                     const DayCounter& dayCounter);
        void reset(Size size);
      private:
        IrregularSwaption::arguments arguments_;
        Time lastPayment_;
    };

}


#endif

