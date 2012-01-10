// $Id$
//==============================================================================
//!
//! \file ASMs1D.h
//!
//! \date Apr 20 2010
//!
//! \author Einar Christensen / SINTEF
//!
//! \brief Driver for assembly of structured 1D spline FE models.
//!
//==============================================================================

#ifndef _ASM_S1D_H
#define _ASM_S1D_H

#include "ASMstruct.h"

namespace Go {
  class SplineCurve;
}


/*!
  \brief Driver for assembly of structured 1D spline FE models.
  \details This class contains methods common for structured 1D spline patches.
*/

class ASMs1D : public ASMstruct
{
public:
  //! \brief Default constructor.
  ASMs1D(unsigned char n_s = 1, unsigned char n_f = 1);
  //! \brief Copy constructor.
  ASMs1D(const ASMs1D& patch, unsigned char n_f = 0);
  //! \brief Empty destructor.
  virtual ~ASMs1D() {}


  // Methods for model generation
  // ============================

  //! \brief Creates an instance by reading the given input stream.
  virtual bool read(std::istream&);
  //! \brief Writes the geometry of the SplineCurve object to given stream.
  virtual bool write(std::ostream&, int = 0) const;

  //! \brief Generates the finite element topology data for the patch.
  //! \details The data generated are the element-to-node connectivity array,
  //! and the global node and element numbers.
  virtual bool generateFEMTopology();

  //! \brief Clears the contents of the patch, making it empty.
  //! \param[in] retainGeometry If \e true, the spline geometry is not cleared.
  //! This is used to reinitialize the patch after it has been refined.
  virtual void clear(bool retainGeometry = false);

  //! \brief Returns the global coordinates for the given node.
  //! \param[in] inod 1-based node index local to current patch
  virtual Vec3 getCoord(size_t inod) const;

  //! \brief Returns a matrix with nodal coordinates for an element.
  //! \param[in] iel Element index
  //! \param[out] X 3\f$\times\f$n-matrix, where \a n is the number of nodes
  //! in one element
  virtual bool getElementCoordinates(Matrix& X, int iel) const;

  //! \brief Returns a matrix with all nodal coordinates within the patch.
  //! \param[out] X 3\f$\times\f$n-matrix, where \a n is the number of nodes
  //! in the patch
  virtual void getNodalCoordinates(Matrix& X) const;

  //! \brief Updates the nodal coordinates for this patch.
  //! \param[in] displ Incremental displacements to update the coordinates with
  virtual bool updateCoords(const Vector& displ);

  //! \brief Refines the parametrization by inserting extra knots.
  //! \param[in] xi Relative positions of added knots in each existing knot span
  bool refine(const RealArray& xi);
  //! \brief Refines the parametrization by inserting extra knots uniformly.
  //! \param[in] nInsert Number of extra knots to insert in each knot-span
  bool uniformRefine(int nInsert);
  //! \brief Raises the order of the SplineCurve object for this patch.
  //! \param[in] ru Number of times to raise the order
  bool raiseOrder(int ru);


  // Various methods for preprocessing of boundary conditions and patch topology
  // ===========================================================================

  //! \brief Constrains a node identified by a relative parameter value.
  //! \param[in] xi Parameter value along the curve
  //! \param[in] dof Which DOFs to constrain at the node
  //! \param[in] code Inhomogeneous dirichlet condition code
  //!
  //! \details The parameter value has to be in the domain [0.0,1.0], where
  //! 0.0 means the beginning of the domain and 1.0 means the end. For values
  //! in between, the actual index is taken as the integer value closest to
  //! \a r*n, where \a r denotes the given relative parameter value,
  //! and \a n is the number of nodes along that parameter direction.
  void constrainNode(double xi, int dof = 123, int code = 0);

  //! \brief Connects matching nodes on two adjacent vertices.
  //! \param[in] vertex Local vertex index of this patch, in range [1,2]
  //! \param neighbor The neighbor patch
  //! \param[in] nvertex Local vertex index of neighbor patch, in range [1,2]
  virtual bool connectPatch(int vertex, ASMs1D& neighbor, int nvertex);

  //! \brief Makes the two end vertices of the curve periodic.
  //! \param[in] basis Which basis to connect (mixed methods), 0 means both
  //! \param[in] master 1-based index of the first master node in this basis
  virtual void closeEnds(int basis = 0, int master = 1);


  // Methods for integration of finite element quantities.
  // These are the main computational methods of the ASM class hierarchy.
  // ====================================================================

  //! \brief Evaluates an integral over the interior patch domain.
  //! \param integrand Object with problem-specific data and methods
  //! \param glbInt The integrated quantity
  //! \param[in] time Parameters for nonlinear/time-dependent simulations
  //! \param locInt Vector of element-wise contributions to \a glbInt
  virtual bool integrate(Integrand& integrand,
			 GlobalIntegral& glbInt, const TimeDomain& time,
			 const LintegralVec& locInt = LintegralVec());

  //! \brief Evaluates a boundary integral over a patch end.
  //! \param integrand Object with problem-specific data and methods
  //! \param[in] lIndex Local index of the end point
  //! \param glbInt The integrated quantity
  //! \param[in] time Parameters for nonlinear/time-dependent simulations
  //! \param locInt Vector of element-wise contributions to \a glbInt
  virtual bool integrate(Integrand& integrand, int lIndex,
			 GlobalIntegral& glbInt, const TimeDomain& time,
			 const LintegralVec& locInt = LintegralVec());


  // Post-processing methods
  // =======================

  //! \brief Evaluates the geometry at a specified point.
  //! \param[in] xi Dimensionless parameter in range [0.0,1.0] of the point
  //! \param[out] param The parameter of the point in knot-span domain
  //! \param[out] X The Cartesian coordinates of the point
  //! \return Local node number within the patch that matches the point, if any
  //! \return 0 if no node (control point) matches this point
  virtual int evalPoint(const double* xi, double* param, Vec3& X) const;

  //! \brief Creates a line element model of this patch for visualization.
  //! \param[out] grid The generated line grid
  //! \param[in] npe Number of visualization nodes over each knot span
  //! \note The number of element nodes must be set in \a grid on input.
  virtual bool tesselate(ElementBlock& grid, const int* npe) const;

  //! \brief Evaluates the primary solution field at all visualization points.
  //! \param[out] sField Solution field
  //! \param[in] locSol Solution vector in DOF-order
  //! \param[in] npe Number of visualization nodes over each knot span
  virtual bool evalSolution(Matrix& sField, const Vector& locSol,
			    const int* npe) const;

  //! \brief Evaluates the primary solution field at the given points.
  //! \param[out] sField Solution field
  //! \param[in] locSol Solution vector local to current patch
  //! \param[in] gpar Parameter values of the result sampling points
  virtual bool evalSolution(Matrix& sField, const Vector& locSol,
			    const RealArray* gpar, bool = true) const;

  //! \brief Evaluates the secondary solution field at all visualization points.
  //! \param[out] sField Solution field
  //! \param[in] integrand Object with problem-specific data and methods
  //! \param[in] npe Number of visualization nodes over each knot span
  //! \param[in] project Flag indicating if the projected solution is wanted
  //!
  //! \details The secondary solution is derived from the primary solution,
  //! which is assumed to be stored within the \a integrand for current patch.
  //! If \a npe is NULL, the solution is evaluated at the Greville points and
  //! then projected onto the spline basis to obtain the control point values,
  //! which then are returned through \a sField.
  //! If \a npe is not NULL and \a project is \e true, the solution is also
  //! projected onto the spline basis, and then evaluated at the \a npe points.
  virtual bool evalSolution(Matrix& sField, const Integrand& integrand,
			    const int* npe = 0, bool project = false) const;

  //! \brief Projects the secondary solution field onto the primary basis.
  //! \param[in] integrand Object with problem-specific data and methods
  Go::SplineCurve* projectSolution(const Integrand& integrand) const;
  //! \brief Projects the secondary solution field onto the primary basis.
  //! \param[in] integrand Object with problem-specific data and methods
  virtual Go::GeomObject* evalSolution(const Integrand& integrand) const;

  //! \brief Evaluates the secondary solution field at the given points.
  //! \param[out] sField Solution field
  //! \param[in] integrand Object with problem-specific data and methods
  //! \param[in] gpar Parameter values of the result sampling points
  //!
  //! \details The secondary solution is derived from the primary solution,
  //! which is assumed to be stored within the \a integrand for current patch.
  virtual bool evalSolution(Matrix& sField, const Integrand& integrand,
			    const RealArray* gpar, bool = true) const;

  //! \brief Calculates parameter values for visualization nodal points.
  //! \param[out] prm Parameter values for all points
  //! \param[in] nSegSpan Number of visualization segments over each knot-span
  virtual bool getGridParameters(RealArray& prm, int nSegSpan) const;

protected:

  // Internal utility methods
  // ========================

  //! \brief Connects matching nodes on two adjacent vertices.
  //! \param[in] vertex Local vertex index of this patch, in range [1,2]
  //! \param neighbor The neighbor patch
  //! \param[in] nvertex Local vertex index of neighbor patch, in range [1,2]
  //! \param[in] basis Which basis to connect the nodes for (mixed methods)
  //! \param[in] slave 0-based index of the first slave node in this basis
  //! \param[in] master 0-based index of the first master node in this basis
  bool connectBasis(int vertex, ASMs1D& neighbor, int nvertex,
		    int basis = 1, int slave = 0, int master = 0);

  //! \brief Extracts parameter values of the Gauss points.
  //! \param[out] uGP Parameter values for all points
  //! \param[in] nGauss Number of Gauss points along a knot-span
  //! \param[in] xi Dimensionless Gauss point coordinates [-1,1]
  void getGaussPointParameters(Matrix& uGP, int nGauss, const double* xi) const;

  //! \brief Calculates parameter values for the Greville points.
  //! \param[out] prm Parameter values for all points
  bool getGrevilleParameters(RealArray& prm) const;

  //! \brief Returns the length in the parameter space for an element.
  //! \param[in] iel Element index
  double getParametricLength(int iel) const;

  //! \brief Returns the number of nodal points in the patch.
  //! \param[in] basis Which basis to return size parameters for (mixed methods)
  virtual int getSize(int basis = 0) const;

  //! \brief Establishes matrices with basis functions and 1st derivatives.
  //! \param[in] u Parameter value of current integration point
  //! \param[out] N Basis function values
  //! \param[out] dNdu First derivatives of basis functions
  void extractBasis(double u, Vector& N, Matrix& dNdu) const;
  //! \brief Establishes matrices with basis functions, 1st and 2nd derivatives.
  //! \param[in] u Parameter value of current integration point
  //! \param[out] N Basis function values
  //! \param[out] dNdu First derivatives of basis functions
  //! \param[out] d2Ndu2 Second derivatives of basis functions
  void extractBasis(double u, Vector& N, Matrix& dNdu, Matrix3D& d2Ndu2) const;

  //! \brief Returns the parametric length on the \a i'th knot-span
  double getKnotSpan(int i) const;

public:
  //! \brief Auxilliary function for computation of basis function indices.
  static void scatterInd(int p1, int start, IntVec& index);

protected:
  Go::SplineCurve* curv; //!< Pointer to the actual spline curve object
};

#endif
