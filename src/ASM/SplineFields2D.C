// $Id$
//==============================================================================
//!
//! \file SplineFields2D.C
//!
//! \date Mar 28 2011
//!
//! \author Runar Holdahl / SINTEF
//!
//! \brief Class for spline-based finite element vector fields in 2D.
//!
//==============================================================================

#include "SplineFields2D.h"
#include "ASMs2D.h"
#include "FiniteElement.h"
#include "CoordinateMapping.h"
#include "Utilities.h"
#include "Vec3.h"

#include "GoTools/geometry/SplineSurface.h"


SplineFields2D::SplineFields2D (const ASMs2D* patch,
                                const RealArray& v, char nbasis,
                                int nnf, const char* name)
  : Fields(name), basis(patch->getBasis(nbasis)), surf(patch->getSurface())
{
  const int n1 = basis->numCoefs_u();
  const int n2 = basis->numCoefs_v();
  nno = n1*n2;

  const int p1 = basis->order_u();
  const int p2 = basis->order_v();
  nelm = (n1-p1+1)*(n2-p2+1);

  size_t ofs = 0;
  for (char i = 1; i < nbasis; ++i)
    ofs += patch->getNoNodes(i)*patch->getNoFields(i);
  auto vit = v.begin()+ofs;

  // Ensure the values array has compatible length, pad with zeros if necessary
  if (nnf == 0)
    nnf = 2;

  nf = nnf;
  int nfc = patch->getNoFields(nbasis);
  values.resize(nno*nf);
  int ndof = nfc*nno;
  auto end = v.size() > ofs+ndof ? vit+ndof : v.end();
  if (nfc == nf)
    std::copy(vit,end,values.begin());
  else
    for (size_t i = 0; i < nno && vit != end; ++i, vit += nfc)
      for (size_t j = 0; j < nf; ++j)
        values[nf*i+j] = *(vit+j);
}


bool SplineFields2D::valueNode (size_t node, Vector& vals) const
{
  if (node < 1 || node > nno) return false;

  vals.resize(nf);
  vals.fill(values.ptr()+(node-1)*nf);
  return true;
}


bool SplineFields2D::valueFE (const FiniteElement& fe, Vector& vals) const
{
  if (!basis) return false;

  // Evaluate the basis functions at the given point
  Go::BasisPtsSf spline;
#pragma omp critical
  basis->computeBasis(fe.u,fe.v,spline);

  // Evaluate the solution field at the given point
  std::vector<int> ip;
  ASMs2D::scatterInd(basis->numCoefs_u(),basis->numCoefs_v(),
		     basis->order_u(),basis->order_v(),
		     spline.left_idx,ip);

  Matrix Vnod;
  utl::gather(ip,nf,values,Vnod);
  Vnod.multiply(spline.basisValues,vals); // vals = Vnod * basisValues

  return true;
}


bool SplineFields2D::valueCoor (const Vec4& x, Vector& vals) const
{
  FiniteElement fe;
  if (x.u) {
    fe.u = x.u[0];
    fe.v = x.u[1];
  }
  else {
    // use with caution
    Go::Point pt(3), clopt(3);
    pt[0] = x[0];
    pt[1] = x[1];
    pt[2] = x[2];
    double clo_u, clo_v, dist;
  #pragma omp critical
    surf->closestPoint(pt, clo_u, clo_v, clopt, dist, 1e-5);

    fe.u = clo_u;
    fe.v = clo_v;
  }

  return this->valueFE(fe, vals);
}


bool SplineFields2D::gradFE (const FiniteElement& fe, Matrix& grad) const
{
  if (!basis) return false;
  if (!surf)  return false;

  // Evaluate the basis functions at the given point
  Go::BasisDerivsSf spline;
#pragma omp critical
  surf->computeBasis(fe.u,fe.v,spline);

  const int uorder = surf->order_u();
  const int vorder = surf->order_v();
  const size_t nen = uorder*vorder;

  Matrix dNdu(nen,2), dNdX;
  for (size_t n = 1; n <= nen; n++)
  {
    dNdu(n,1) = spline.basisDerivs_u[n-1];
    dNdu(n,2) = spline.basisDerivs_v[n-1];
  }

  std::vector<int> ip;
  ASMs2D::scatterInd(surf->numCoefs_u(),surf->numCoefs_v(),
		     uorder,vorder,spline.left_idx,ip);

  // Evaluate the Jacobian inverse
  Matrix Xnod, Jac;
  Vector Xctrl(&(*surf->coefs_begin()),surf->coefs_end()-surf->coefs_begin());
  utl::gather(ip,surf->dimension(),Xctrl,Xnod);
  utl::Jacobian(Jac,dNdX,Xnod,dNdu);

  // Evaluate the gradient of the solution field at the given point
  if (basis != surf)
  {
    // Mixed formulation, the solution uses a different basis than the geometry
#pragma omp critical
    basis->computeBasis(fe.u,fe.v,spline);

    const size_t nbf = basis->order_u()*basis->order_v();
    dNdu.resize(nbf,2);
    for (size_t n = 1; n <= nbf; n++)
    {
      dNdu(n,1) = spline.basisDerivs_u[n-1];
      dNdu(n,2) = spline.basisDerivs_v[n-1];
    }
    dNdX.multiply(dNdu,Jac); // dNdX = dNdu * Jac

    ip.clear();
    ASMs2D::scatterInd(basis->numCoefs_u(),basis->numCoefs_v(),
		       basis->order_u(),basis->order_v(),
		       spline.left_idx,ip);
  }

  utl::gather(ip,nf,values,Xnod);
  grad.multiply(Xnod,dNdX); // grad = Xnod * dNdX

  return true;
}


bool SplineFields2D::hessianFE (const FiniteElement& fe, Matrix3D& H) const
{
  if (!basis) return false;
  if (!surf)  return false;

  // Order of basis
  const int uorder = surf->order_u();
  const int vorder = surf->order_v();
  const size_t nen = uorder*vorder;

  // Evaluate the basis functions at the given point
  Go::BasisDerivsSf  spline;
  Go::BasisDerivsSf2 spline2;
  Matrix3D d2Ndu2;
  Matrix dNdu(nen,2), dNdX;
  IntVec ip;
  if (surf == basis) {
#pragma omp critical
    surf->computeBasis(fe.u,fe.v,spline2);
    d2Ndu2.resize(nen,2,2);
    for (size_t n = 1; n <= nen; n++) {
      dNdu(n,1) = spline2.basisDerivs_u[n-1];
      dNdu(n,2) = spline2.basisDerivs_v[n-1];
      d2Ndu2(n,1,1) = spline2.basisDerivs_uu[n-1];
      d2Ndu2(n,1,2) = d2Ndu2(n,2,1) = spline2.basisDerivs_uv[n-1];
      d2Ndu2(n,2,2) = spline2.basisDerivs_vv[n-1];
    }

    ASMs2D::scatterInd(surf->numCoefs_u(),surf->numCoefs_v(),
		       uorder,vorder,spline2.left_idx,ip);
  }
  else {
#pragma omp critical
    surf->computeBasis(fe.u,fe.v,spline);
    for (size_t n = 1; n <= nen; n++) {
      dNdu(n,1) = spline.basisDerivs_u[n-1];
      dNdu(n,2) = spline.basisDerivs_v[n-1];
    }

    ASMs2D::scatterInd(surf->numCoefs_u(),surf->numCoefs_v(),
		       uorder,vorder,spline.left_idx,ip);
  }

  // Evaluate the Jacobian inverse
  Matrix Xnod, Jac;
  Vector Xctrl(&(*surf->coefs_begin()),surf->coefs_end()-surf->coefs_begin());
  utl::gather(ip,surf->dimension(),Xctrl,Xnod);
  utl::Jacobian(Jac,dNdX,Xnod,dNdu);

  // Evaluate the gradient of the solution field at the given point
  if (basis != surf)
  {
    // Mixed formulation, the solution uses a different basis than the geometry
#pragma omp critical
    basis->computeBasis(fe.u,fe.v,spline2);

    const size_t nbf = basis->order_u()*basis->order_v();
    dNdu.resize(nbf,2);
    d2Ndu2.resize(nbf,2,2);
    for (size_t n = 1; n <= nbf; n++) {
      dNdu(n,1) = spline2.basisDerivs_u[n-1];
      dNdu(n,2) = spline2.basisDerivs_v[n-1];
      d2Ndu2(n,1,1) = spline2.basisDerivs_uu[n-1];
      d2Ndu2(n,1,2) = d2Ndu2(n,2,1) = spline2.basisDerivs_uv[n-1];
      d2Ndu2(n,2,2) = spline2.basisDerivs_vv[n-1];
    }

    ip.clear();
    ASMs2D::scatterInd(basis->numCoefs_u(),basis->numCoefs_v(),
		       basis->order_u(),basis->order_v(),
		       spline2.left_idx,ip);
  }

  Matrix Vnod;
  utl::gather(ip,nf,values,Vnod);
  return H.multiply(Vnod,d2Ndu2);
}
