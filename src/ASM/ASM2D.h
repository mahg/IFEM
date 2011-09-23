// $Id$
//==============================================================================
//!
//! \file ASM2D.h
//!
//! \date Sep 20 2011
//!
//! \author Knut Morten Okstad / SINTEF
//!
//! \brief Abstract interface for 2D patches.
//!
//==============================================================================

#ifndef _ASM_2D_H
#define _ASM_2D_H

#include <vector>

/*!
  \brief Abstract interface for 2D spline patches.
  \details This class contains an interface to methods common for structured and
  unstructured 2D patches, such that these methods can be invoked without the
  need to type-cast the patch object to the actual class type.
*/

class ASM2D
{
protected:
  //! \brief The constructor is protected to allow objects of sub-classes only.
  ASM2D() {}

public:
  //! \brief Empty destructor.
  virtual ~ASM2D() {}

  //! \brief Refine the parametrization by inserting extra knots.
  //! \param[in] dir Parameter direction to refine
  //! \param[in] xi Relative positions of added knots in each existing knot span
  virtual bool refine(int dir, const std::vector<double>& xi) = 0;
  //! \brief Refine the parametrization by inserting extra knots uniformly.
  //! \param[in] dir Parameter direction to refine
  //! \param[in] nInsert Number of extra knots to insert in each knot-span
  virtual bool uniformRefine(int dir, int nInsert) = 0;
  //! \brief Raise the order of the spline object for this patch.
  //! \param[in] ru Number of times to raise the order in u-direction
  //! \param[in] rv Number of times to raise the order in v-direction
  virtual bool raiseOrder(int ru, int rv) = 0;

  //! \brief Constrains all DOFs on a given boundary edge.
  //! \param[in] dir Parameter direction defining the edge to constrain
  //! \param[in] dof Which DOFs to constrain at each node on the edge
  //! \param[in] code Inhomogeneous dirichlet condition code
  virtual void constrainEdge(int dir, int dof = 123, int code = 0) = 0;
  //! \brief Constrains a corner node identified by the two parameter indices.
  //! \param[in] I Parameter index in u-direction
  //! \param[in] J Parameter index in v-direction
  //! \param[in] dof Which DOFs to constrain at the node
  //! \param[in] code Inhomogeneous dirichlet condition code
  //!
  //! \details The sign of the two indices is used to define whether we want
  //! the node at the beginning or the end of that parameter direction.
  //! The magnitude of the indices are not used.
  virtual void constrainCorner(int I, int J, int dof = 123, int code = 0) = 0;
  //! \brief Constrains a node identified by two relative parameter values.
  //! \param[in] xi Parameter in u-direction
  //! \param[in] eta Parameter in v-direction
  //! \param[in] dof Which DOFs to constrain at the node
  //! \param[in] code Inhomogeneous dirichlet condition code
  //!
  //! \details The parameter values have to be in the domain [0.0,1.0], where
  //! 0.0 means the beginning of the domain and 1.0 means the end. For values
  //! in between, the actual index is taken as the integer value closest to
  //! \a r*n, where \a r denotes the given relative parameter value,
  //! and \a n is the number of nodes along that parameter direction.
  virtual void constrainNode(double xi, double eta,
			     int dof = 123, int code = 0) = 0;
};

#endif