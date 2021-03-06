// $Id$
//==============================================================================
//!
//! \file SplineFields3Dmx.h
//!
//! \date Oct 3 2016
//!
//! \author Arne Morten Kvarving / SINTEF
//!
//! \brief Class for mixed spline-based finite element vector fields in 3D.
//!
//==============================================================================

#ifndef _SPLINE_FIELDS_3D_MX_H
#define _SPLINE_FIELDS_3D_MX_H

#include "Fields.h"
#include <set>

class ASMs3Dmx;

namespace Go {
  class SplineVolume;
}


/*!
  \brief Class for mixed spline-based finite element vector fields in 3D.

  \details This class implements the methods required to evaluate a 3D
  mixed spline vector field at a given point in parametrical or physical coordinates.
*/

class SplineFields3Dmx : public Fields
{
public:
  //! \brief The constructor sets the number of space dimensions and fields.
  //! \param[in] patch The spline patch on which the field is to be defined
  //! \param[in] v Array of control point field values
  //! \param[in] basis Bases to use from patch
  //! \param[in] name Name of spline field
  SplineFields3Dmx(const ASMs3Dmx* patch, const RealArray& v,
                  char basis = 12, const char* name = nullptr);
  //! \brief Empty destructor.
  virtual ~SplineFields3Dmx() {}

  // Methods to evaluate the field
  //==============================

  //! \brief Computes the value in a given node/control point.
  //! \param[in] node Node number
  //! \param[out] vals Node values
  bool valueNode(size_t node, Vector& vals) const;

  //! \brief Computes the value at a given local coordinate.
  //! \param[in] fe Finite element definition
  //! \param[out] vals Values in local point in given element
  bool valueFE(const FiniteElement& fe, Vector& vals) const;

  //! \brief Computes the gradient for a given local coordinate.
  //! \param[in] fe Finite element
  //! \param[out] grad Gradient of solution in a given local coordinate
  bool gradFE(const FiniteElement& fe, Matrix& grad) const;

protected:
  const ASMs3Dmx* svol; //!< Patch description
  std::set<int> bases; //!< Bases to use
};

#endif
