// $Id$
//==============================================================================
//!
//! \file ASMu3Drecovery.C
//!
//! \date November 2016
//!
//! \author Kjetil A. Johannessen and Knut Morten Okstad / SINTEF
//!
//! \brief Recovery techniques for unstructured LR B-splines.
//!
//==============================================================================

#include "LRSpline/LRSplineVolume.h"
#include "LRSpline/Element.h"

#include "ASMu3D.h"
#include "IntegrandBase.h"
#include "CoordinateMapping.h"
#include "GaussQuadrature.h"
#include "SparseMatrix.h"
#include "DenseMatrix.h"
#include "SplineUtils.h"
#include "Profiler.h"
#include <array>


bool ASMu3D::getGrevilleParameters (RealArray& prm, int dir) const
{
  if (!lrspline || dir < 0 || dir > 2) return false;

  prm.clear();
  prm.reserve(lrspline->nBasisFunctions());
  for(LR::Basisfunction *b : lrspline->getAllBasisfunctions())
    prm.push_back(b->getGrevilleParameter()[dir]);

  return true;
}


/*!
  \brief Expands a tensor parametrization point to an unstructured one.
  \details Takes as input a tensor mesh, for instance
     in[0] = {0,1}
     in[1] = {2,3}
     in[2] = {7,9}
   and expands this to an unstructured representation, i.e.,
     out[0] = {0,1,0,1,0,1,0,1}
     out[1] = {2,2,3,3,2,2,3,3}
     out[2] = {7,7,7,7,9,9,9,9}
*/

static void expandTensorGrid (const RealArray* in, RealArray* out)
{
  out[0].resize(in[0].size()*in[1].size()*in[2].size());
  out[1].resize(in[0].size()*in[1].size()*in[2].size());
  out[2].resize(in[0].size()*in[1].size()*in[2].size());

  size_t i, j, k, ip = 0;
  for (k = 0; k < in[2].size(); k++)
    for (j = 0; j < in[1].size(); j++)
      for (i = 0; i < in[0].size(); i++, ip++) {
        out[0][ip] = in[0][i];
        out[1][ip] = in[1][j];
        out[2][ip] = in[2][k];
      }
}


LR::LRSplineVolume* ASMu3D::projectSolution (const IntegrandBase& integr) const
{
  PROFILE2("ASMu3D::projectSolution");

  // Compute parameter values of the result sampling points (Greville points)
  std::array<RealArray,3> gpar;
  for (int dir = 0; dir < 3; dir++)
    if (!this->getGrevilleParameters(gpar[dir],dir))
      return nullptr;

  // Evaluate the secondary solution at all sampling points
  Matrix sValues;
  if (!this->evalSolution(sValues,integr,gpar.data()))
    return nullptr;

  // Project the results onto the spline basis to find control point
  // values based on the result values evaluated at the Greville points.
  // Note that we here implicitly assume that the number of Greville points
  // equals the number of control points such that we don't have to resize
  // the result array. Think that is always the case, but beware if trying
  // other projection schemes later.

  return this->regularInterpolation(gpar[0],gpar[1],gpar[2],sValues);
}


LR::LRSpline* ASMu3D::evalSolution (const IntegrandBase& integrand) const
{
  return this->projectSolution(integrand);
}


bool ASMu3D::assembleL2matrices (SparseMatrix& A, StdVector& B,
                                 const IntegrandBase& integrand,
                                 bool continuous) const
{
  const size_t nnod = this->getNoNodes();

  const int p1 = lrspline->order(0);
  const int p2 = lrspline->order(1);
  const int p3 = lrspline->order(2);

  // Get Gaussian quadrature points
  const int ng1 = continuous ? nGauss : p1 - 1;
  const int ng2 = continuous ? nGauss : p2 - 1;
  const int ng3 = continuous ? nGauss : p3 - 1;
  const double* xg = GaussQuadrature::getCoord(ng1);
  const double* yg = GaussQuadrature::getCoord(ng2);
  const double* zg = GaussQuadrature::getCoord(ng3);
  const double* wg = continuous ? GaussQuadrature::getWeight(nGauss) : nullptr;
  if (!xg || !yg || !zg) return false;
  if (continuous && !wg) return false;

  double dA = 0.0;
  Vector phi;
  Matrix dNdu, Xnod, Jac;
  Go::BasisDerivs spl1;
  Go::BasisPts    spl0;


  // === Assembly loop over all elements in the patch ==========================

  std::vector<LR::Element*>::iterator el = lrspline->elementBegin();
  for (int iel = 1; el != lrspline->elementEnd(); el++, iel++)
  {
    if (continuous)
    {
      // Set up control point (nodal) coordinates for current element
      if (!this->getElementCoordinates(Xnod,iel))
        return false;
      else if ((dA = 0.125*this->getParametricVolume(iel)) < 0.0)
        return false; // topology error (probably logic error)
    }

    // Compute parameter values of the Gauss points over this element
    std::array<RealArray,3> gpar, unstrGpar;
    this->getGaussPointParameters(gpar[0],0,ng1,iel,xg);
    this->getGaussPointParameters(gpar[1],1,ng2,iel,yg);
    this->getGaussPointParameters(gpar[2],2,ng3,iel,zg);

    // convert to unstructured mesh representation
    expandTensorGrid(gpar.data(),unstrGpar.data());

    // Evaluate the secondary solution at all integration points
    Matrix sField;
    if (!this->evalSolution(sField,integrand,unstrGpar.data()))
      return false;

    // set up basis function size (for extractBasis subroutine)
    phi.resize((**el).nBasisFunctions());

    // --- Integration loop over all Gauss points in each direction ----------
    int ip = 0;
    for (int k = 0; k < ng3; k++)
      for (int j = 0; j < ng2; j++)
        for (int i = 0; i < ng1; i++, ip++)
        {
          if (continuous)
          {
            lrspline->computeBasis(gpar[0][i],gpar[1][j],gpar[2][k],spl1,iel-1);
            SplineUtils::extractBasis(spl1,phi,dNdu);
          }
          else
          {
            lrspline->computeBasis(gpar[0][i],gpar[1][j],gpar[2][k],spl0,iel-1);
            phi = spl0.basisValues;
          }

          // Compute the Jacobian inverse and derivatives
          double dJw = 1.0;
          if (continuous)
          {
            dJw = dA*wg[i]*wg[j]*wg[k]*utl::Jacobian(Jac,dNdu,Xnod,dNdu,false);
            if (dJw == 0.0) continue; // skip singular points
          }

          // Integrate the linear system A*x=B
          for (size_t ii = 0; ii < phi.size(); ii++)
          {
            int inod = MNPC[iel-1][ii]+1;
            for (size_t jj = 0; jj < phi.size(); jj++)
            {
              int jnod = MNPC[iel-1][jj]+1;
              A(inod,jnod) += phi[ii]*phi[jj]*dJw;
            }
            for (size_t r = 1; r <= sField.rows(); r++)
              B(inod+(r-1)*nnod) += phi[ii]*sField(r,ip+1)*dJw;
          }
        }
  }

  return true;
}


/*!
  \brief Evaluates monomials in 3D up to the specified order.
*/

static void evalMonomials (int p1, int p2, int p3, double x, double y, double z, Vector& P)
{
  P.resize(p1*p2*p3);
  P.fill(1.0);

  int i, j, k, l, ip = 1;
  for (k = 0; k < p3; k++)
    for (j = 0; j < p2; j++)
      for (i = 0; i < p1; i++, ip++)
      {
        for (l = 0; l < i; l++) P(ip) *= x;
        for (l = 0; l < j; l++) P(ip) *= y;
        for (l = 0; l < k; l++) P(ip) *= z;
      }
}


LR::LRSplineVolume* ASMu3D::scRecovery (const IntegrandBase& integrand) const
{
  PROFILE2("ASMu3D::scRecovery");

  const int m = integrand.derivativeOrder();
  const int p1 = lrspline->order(0);
  const int p2 = lrspline->order(1);
  const int p3 = lrspline->order(2);

  // Get Gaussian quadrature point coordinates
  const int ng1 = p1 - m;
  const int ng2 = p2 - m;
  const int ng3 = p3 - m;
  const double* xg = GaussQuadrature::getCoord(ng1);
  const double* yg = GaussQuadrature::getCoord(ng2);
  const double* zg = GaussQuadrature::getCoord(ng3);
  if (!xg || !yg || !zg) return nullptr;

  // Compute parameter values of the Greville points
  std::array<RealArray,3> gpar;
  if (!this->getGrevilleParameters(gpar[0],0)) return nullptr;
  if (!this->getGrevilleParameters(gpar[1],1)) return nullptr;
  if (!this->getGrevilleParameters(gpar[2],2)) return nullptr;

  const int n1 = p1 - m + 1; // Patch size in first parameter direction
  const int n2 = p2 - m + 1; // Patch size in second parameter direction
  const int n3 = p3 - m + 1; // Patch size in second parameter direction

  const size_t nCmp = integrand.getNoFields(); // Number of result components
  const size_t nPol = n1*n2*n3; // Number of terms in polynomial expansion

  Matrix sValues(nCmp,gpar[0].size());
  Vector P(nPol);
  Go::Point X, G;

  // Loop over all Greville points (one for each basis function)
  size_t ip = 0;
  std::vector<LR::Element*>::const_iterator elStart, elEnd, el;
  std::vector<LR::Element*> supportElements;
  for (LR::Basisfunction *b : lrspline->getAllBasisfunctions())
  {
#if SP_DEBUG > 2
    std::cout <<"Basis: "<< *b <<"\n  ng1 ="<< ng1 <<"\n  ng2 ="<< ng2 <<"\n  ng3 ="<< ng3
              <<"\n  nPol="<< nPol << std::endl;
#endif

    // Special case for basis functions with too many zero knot spans by using
    // the extended support
    // if (nel*ng1*ng2*ng3 < nPol)
    if (true)
    {
      // I am unsure as to the consequence of going back to previous if-statement
      // here so we keep if (true) for now. This was introduced mainly when considering
      // functions that live on the boundary and have support on few elements;
      // corner functions have support on one element. Using i.e. 2x2 points
      // for every element is not enough to fit 1,x,x^2,x^3,y,xy,...x^3y^3 when
      // we only have one element. The solution is getExtendedSupport, which is the
      // union of support from all functions that overlap *b.
      supportElements = b->getExtendedSupport();
      elStart = supportElements.begin();
      elEnd   = supportElements.end();
#if SP_DEBUG > 2
      std::cout <<"Extended basis:";
      for (el = elStart; el != elEnd; el++)
        std::cout <<"\n  " << **el;
      std::cout << std::endl;
#endif
    }
    else
    {
      elStart = b->supportedElementBegin();
      elEnd   = b->supportedElementEnd();
    }

    // Physical coordinates of current Greville point
    lrspline->point(G,gpar[0][ip],gpar[1][ip],gpar[2][ip]);

    // Set up the local projection matrices
    DenseMatrix A(nPol,nPol);
    Matrix B(nPol,nCmp);

    // Loop over all non-zero knot-spans in the support of
    // the basis function associated with current Greville point
    for (el = elStart; el != elEnd; el++)
    {
      int iel = (**el).getId()+1;

      // evaluate all gauss points for this element
      std::array<RealArray,3> gaussPt, unstrGauss;
      this->getGaussPointParameters(gaussPt[0],0,ng1,iel,xg);
      this->getGaussPointParameters(gaussPt[1],1,ng2,iel,yg);
      this->getGaussPointParameters(gaussPt[2],2,ng3,iel,zg);

#if SP_DEBUG > 2
      std::cout << "Element " << **el << std::endl;
#endif

      // convert to unstructured mesh representation
      expandTensorGrid(gaussPt.data(),unstrGauss.data());

      // Evaluate the secondary solution at all Gauss points
      Matrix sField;
      if (!this->evalSolution(sField,integrand,unstrGauss.data()))
        return nullptr;

      // Loop over the Gauss points in current knot-span
      int i, j, k, ig = 1;
      for (k = 0; k < ng3; k++)
        for (j = 0; j < ng2; j++)
          for (i = 0; i < ng1; i++, ig++)
          {
            // Evaluate the polynomial expansion at current Gauss point
            lrspline->point(X,gaussPt[0][i],gaussPt[1][j],gaussPt[2][k]);
            evalMonomials(n1,n2,n3,X[0]-G[0],X[1]-G[1],X[2]-G[2],P);
  #if SP_DEBUG > 2
            std::cout <<"Greville     point: "<< G
                      <<"\nGauss        point: "<< gaussPt[0][i] <<", "<< gaussPt[0][j] << ", " << gaussPt[0][k]
                      <<"\nMapped gauss point: "<< X
                      <<"\nP-matrix:"<< P <<"--------------------\n"<< std::endl;
  #endif

            for (size_t ii = 1; ii <= nPol; ii++)
            {
              // Accumulate the projection matrix, A += P^t * P
              for (size_t jj = 1; jj <= nPol; jj++)
                A(ii,jj) += P(ii)*P(jj);

              // Accumulate the right-hand-side matrix, B += P^t * sigma
              for (size_t jj = 1; jj <= nCmp; jj++)
                B(ii,jj) += P(ii)*sField(jj,ig);
            }
          }
    }

#if SP_DEBUG > 2
    std::cout <<"---- Matrix A -----"<< A
              <<"-------------------"<< std::endl;
    std::cout <<"---- Vector B -----"<< B
              <<"-------------------"<< std::endl;
#endif

    // Solve the local equation system
    if (!A.solve(B)) return nullptr;

    // Evaluate the projected field at current Greville point (first row of B)
    ip++;
    for (size_t l = 1; l <= nCmp; l++)
      sValues(l,ip) = B(1,l);

#if SP_DEBUG > 1
    std::cout <<"Greville point "<< ip <<" (u,v,w) = "
              << gpar[0][ip-1] <<" "<< gpar[1][ip-1] <<" "<< gpar[2][ip-1] <<" :";
    for (size_t k = 1; k <= nCmp; k++)
      std::cout <<" "<< sValues(k,ip);
    std::cout << std::endl;
#endif
  }

  // Project the Greville point results onto the spline basis
  // to find the control point values

  return this->regularInterpolation(gpar[0],gpar[1],gpar[2],sValues);
}


LR::LRSplineVolume* ASMu3D::regularInterpolation (const RealArray& upar,
                                                  const RealArray& vpar,
                                                  const RealArray& wpar,
                                                  const Matrix& points) const
{
  if (lrspline->rational())
  {
    std::cerr <<" *** ASMu3D::regularInterpolation:"
              <<" Rational LR B-splines not supported yet."<< std::endl;
    return nullptr;
  }

  // sanity check on input parameters
  const size_t nBasis = lrspline->nBasisFunctions();
  if (upar.size() != nBasis || vpar.size() != nBasis || wpar.size() != nBasis ||
      points.cols() != nBasis)
  {
    std::cerr <<" *** ASMu3D::regularInterpolation:"
              <<" Mismatching input array sizes.\n"
              <<"     size(upar)="<< upar.size() <<" size(vpar)="<< vpar.size()
              <<" size(wpar)="<< wpar.size()
              <<" size(points)="<< points.cols() <<" nBasis="<< nBasis
              << std::endl;
    return nullptr;
  }

  SparseMatrix A(SparseMatrix::SUPERLU);
  A.resize(nBasis, nBasis);
  Matrix B2(points,true); // transpose to get one vector per field
  StdVector B(B2);
  Go::BasisPts splineValues;

  // Evaluate all basis functions at all points, stored in the A-matrix
  // (same row = same evaluation point)
  for (size_t i = 0; i < nBasis; i++)
  {
    size_t k = 0;
    int iel = lrspline->getElementContaining(upar[i], vpar[i], wpar[i]);
    lrspline->computeBasis(upar[i],vpar[i],wpar[i],splineValues, iel);
    for (LR::Basisfunction* function : lrspline->getElement(iel)->support())
      A(i+1,function->getId()+1) = splineValues.basisValues[k++];
  }

  // Solve for all solution components - one right-hand-side for each
  if (!A.solve(B)) return nullptr;

  // Copy all basis functions and mesh
  LR::LRSplineVolume* ans = lrspline->copy();
  ans->rebuildDimension(points.rows());

  // Back to interleaved data
  RealArray interleave;
  interleave.reserve(B.dim());
  for (size_t i = 0; i < nBasis; i++)
    for (size_t j = 0; j < points.rows(); j++)
      interleave.push_back(B(1+j*points.cols()+i));

  ans->setControlPoints(interleave);

  return ans;
}


bool ASMu3D::faceL2projection (const DirichletFace& face,
                               const FunctionBase& values,
                               Real2DMat& result,
                               double time) const
{
  size_t n = face.MLGN.size();
  size_t m = values.dim();
  SparseMatrix A(SparseMatrix::SUPERLU);
  StdVector B(n*m);
  A.resize(n,n);

  // Get Gaussian quadrature points and weights
  const double* xg = GaussQuadrature::getCoord(nGauss);
  const double* wg = GaussQuadrature::getWeight(nGauss);
  if (!xg || !wg) return false;

  // find the normal direction for the face
  int faceDir;
  switch (face.edg)
  {
    case LR::WEST:   faceDir = -1; break;
    case LR::EAST:   faceDir =  1; break;
    case LR::SOUTH:  faceDir = -2; break;
    case LR::NORTH:  faceDir =  2; break;
    case LR::BOTTOM: faceDir = -3; break;
    case LR::TOP:    faceDir =  3; break;
    default:         return false;
  }
  const int t1 = 1 + abs(faceDir)%3; // first tangent direction
  const int t2 = 1 + t1%3;           // second tangent direction

  Vector N;
  Matrix dNdu, dNdX, Xnod, Jac;
  Vec4   X;

  // === Assembly loop over all elements on the patch face =====================
  for (size_t ie = 0; ie < face.MLGE.size(); ie++) // for all face elements
  {
    int iel = 1 + face.MLGE[ie];

    std::array<Vector,3> gpar;
    for (int d = 0; d < 3; d++)
      if (-1-d == faceDir)
      {
        gpar[d].resize(1);
        gpar[d].fill(lrspline->startparam(d));
      }
      else if (1+d == faceDir)
      {
        gpar[d].resize(1);
        gpar[d].fill(lrspline->endparam(d));
      }
      else
        this->getGaussPointParameters(gpar[d],d,nGauss,iel,xg);

    // Get element face area in the parameter space
    double dA = 0.25*this->getParametricArea(iel,abs(faceDir));
    if (dA < 0.0) return false; // topology error (probably logic error)

    // Set up control point coordinates for current element
    if (!this->getElementCoordinates(Xnod,iel)) return false;

    double u = gpar[0].front();
    double v = gpar[1].front();
    double w = gpar[2].front();

    // --- Integration loop over all Gauss points over the face -------------
    for (int j = 0; j < nGauss; j++)
      for (int i = 0; i < nGauss; i++)
      {
        // Parameter values of current integration point
        int k1, k2, k3;
        switch (abs(faceDir))
        {
          case 1: k2 = i; k3 = j; k1 = 0; break;
          case 2: k1 = i; k3 = j; k2 = 0; break;
          case 3: k1 = i; k2 = j; k3 = 0; break;
          default: k1 = k2 = k3 = 0;
        }
        if (gpar[0].size() > 1) u = gpar[0](k1+1);
        if (gpar[1].size() > 1) v = gpar[1](k2+1);
        if (gpar[2].size() > 1) w = gpar[2](k3+1);

        // Evaluate basis function derivatives at integration points
        this->evaluateBasis(iel-1, u, v, w, N, dNdu, myGeoBasis);

        // Compute basis function derivatives
        double dJxW = dA*wg[i]*wg[j]*utl::Jacobian(Jac,X,dNdX,Xnod,dNdu,t1,t2);
        if (dJxW == 0.0) continue; // skip singular points

        // Cartesian coordinates of current integration point
        X = Xnod * N;
        X.t = time;

        // For mixed basis, we need to compute functions separate from geometry
        if (face.lr != lrspline.get())
        {
          // different lrspline instances enumerate elements differently
          Go::BasisDerivs spline;
          face.lr->computeBasis(u, v, w, spline,
                                face.lr->getElementContaining(u,v,w));
          SplineUtils::extractBasis(spline,N,dNdu);
        }

        // Assemble into matrix A and vector B
        size_t il, jl;
        int    ig, jg;
        for (il = 0; il < face.MNPC[ie].size(); il++) // local i-index
          if ((ig = 1+face.MNPC[ie][il]) > 0)         // global i-index
          {
            for (jl = 0; jl < face.MNPC[ie].size(); jl++) // local j-index
              if ((jg = 1+face.MNPC[ie][jl]) > 0)         // global j-index
                A(ig,jg) += N[il]*N[jl]*dJxW;

            RealArray val = values.getValue(X);
            for (size_t k = 0; k < m; k++)
              B(ig+k*n) += N[il]*val[k]*dJxW;
          } // end basis-function loop
      } // end gauss-point loop
  } // end element loop

#if SP_DEBUG > 2
  std::cout <<"---- Matrix A -----\n"<< A
            <<"-------------------"<< std::endl;
  std::cout <<"---- Vector B -----\n"<< B
            <<"-------------------"<< std::endl;
  std::cout <<"---- Face-nodes (g2l-mapping) -----";
  for (size_t i = 0; i < face.MLGN.size(); i++)
    std::cout <<"\n     "<< i <<": "<< face.MLGN[i];
  std::cout <<"\n-------------------";
  std::cout <<"\n---- Element-nodes (-1 is interior element nodes) -----";
  for (const IntVec& d : face.MNPC) {
    std::cout <<"\n    ";
    for (int c : d)
      std::cout <<" "<< c;
  }
  std::cout <<"\n-------------------"<< std::endl;
#endif

  // Solve the face-global equation system
  if (!A.solve(B)) return false;

#if SP_DEBUG > 2
  std::cout <<"---- SOLUTION -----\n"<< B
            <<"-------------------"<< std::endl;
#endif

  // Store the control-point values of the projected field
  result.resize(m,RealArray(n));
  RealArray::const_iterator it = B.begin();
  for (size_t i = 0; i < m; i++, it += n)
    std::copy(it, it+n, result[i].begin());

  return true;
}
