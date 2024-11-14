/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2001, 2002, 2003 Sadruddin Rejeb
 Copyright (C) 2005, 2007 StatPro Italia srl
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

/*! \file treeswaptionengine.hpp
    \brief Numerical lattice engine for swaptions
*/

#ifndef quantlib_tree_irregular_swaption_engine_hpp
#define quantlib_tree_irregular_swaption_engine_hpp

#include <ql/experimental/swaptions/irregularswaption.hpp>
#include <ql/pricingengines/latticeshortratemodelengine.hpp>

namespace QuantLib {

    //! Numerical lattice engine for swaptions
    /*! \ingroup swaptionengines

        \warning This engine is not guaranteed to work if the
                 underlying swap has a start date in the past, i.e.,
                 before today's date. When using this engine, prune
                 the initial part of the swap so that it starts at
                 \f$ t \geq 0 \f$.

        \test calculations are checked against cached results
    */
    class TreeIrregularSwaptionEngine
    : public LatticeShortRateModelEngine<IrregularSwaption::arguments,
                                         IrregularSwaption::results> {
      public:
        /*! \name Constructors
            \note the term structure is only needed when the short-rate
                  model cannot provide one itself.
        */
        //@{
        TreeIrregularSwaptionEngine(const boost::shared_ptr<ShortRateModel>&,
                                    Size timeSteps,
                                    const Handle<YieldTermStructure>& termStructure =
                                                 Handle<YieldTermStructure>());
        TreeIrregularSwaptionEngine(const boost::shared_ptr<ShortRateModel>&,
                                    const TimeGrid& timeGrid,
                                    const Handle<YieldTermStructure>& termStructure =
                                                 Handle<YieldTermStructure>());
        //@}
        void calculate() const;
      private:
        Handle<YieldTermStructure> termStructure_;
    };




}






#endif
