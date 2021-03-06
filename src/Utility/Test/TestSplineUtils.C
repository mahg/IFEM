//==============================================================================
//!
//! \file TestSplineUtils.C
//!
//! \date Oct 13 2014
//!
//! \author Arne Morten Kvarving / SINTEF
//!
//! \brief Tests for various utility functions on spline objects.
//!
//==============================================================================

#include "SplineUtils.h"
#include "GoTools/utils/Point.h"
#include "GoTools/geometry/SplineSurface.h"
#include "GoTools/geometry/Line.h"
#include "GoTools/geometry/Disc.h"
#include "GoTools/geometry/Plane.h"
#include "GoTools/trivariate/SphereVolume.h"
#include "GoTools/trivariate/SplineVolume.h"
#include "ExprFunctions.h"
#include <fstream>

#include "gtest/gtest.h"

static Matrix readMatrix(size_t r, size_t c, const std::string& file)
{
  Matrix result(r,c);
  std::ifstream f(file);
  for (size_t i=1;i<=r;++i)
    for (size_t j=1;j<=c;++j)
      f >> result(i,j);

  return result;
}

static Matrix3D readMatrices(size_t r, size_t c, size_t k, const std::string& file)
{
  Matrix3D result(r,c,k);
  std::ifstream f(file);
  for (size_t n=1;n<=k;++n)
    for (size_t i=1;i<=r;++i)
      for (size_t j=1;j<=c;++j)
        f >> result(i,j,n);

  return result;
}

#define CHECK_VECTORS_EQUAL(A,path) { \
    Matrix B = readMatrix(A.size(), 1, path); \
    for (size_t i=1;i<=A.size();++i) \
      EXPECT_NEAR(A(i), B(i,1), 1e-13); \
  }

#define CHECK_MATRICES_EQUAL(A,path) { \
    Matrix B = readMatrix(A.rows(), A.cols(), path); \
    for (size_t i=1;i<=A.rows();++i) \
      for (size_t j=1;j<=A.cols();++j) \
        EXPECT_NEAR(A(i,j), B(i,j), 1e-13); \
  }

#define CHECK_MATRICES3D_EQUAL(A,path) { \
    Matrix3D B = readMatrices(A.dim(1), A.dim(2), A.dim(3), path); \
    for (size_t i=1;i<=A.dim(1);++i) \
      for (size_t j=1;j<=A.dim(2);++j) \
        for (size_t k=1;k<=A.dim(3);++k) \
          EXPECT_NEAR(A(i,j,k), B(i,j,k), 1e-13); \
  }

TEST(TestSplineUtils, ToVec3)
{
  Go::Point X(1.0, 2.0, 3.0);
  Vec3 result1 = SplineUtils::toVec3(X,2);
  Vec3 result2 = SplineUtils::toVec3(X,3);
  Vec3 result3 = SplineUtils::toVec3(X);
  EXPECT_FLOAT_EQ(result1.x, 1.0);
  EXPECT_FLOAT_EQ(result1.y, 2.0);
  EXPECT_FLOAT_EQ(result2.x, 1.0);
  EXPECT_FLOAT_EQ(result2.y, 2.0);
  EXPECT_FLOAT_EQ(result2.z, 3.0);
  EXPECT_FLOAT_EQ(result3.x, 1.0);
  EXPECT_FLOAT_EQ(result3.y, 2.0);
  EXPECT_FLOAT_EQ(result3.z, 3.0);
}

TEST(TestSplineUtils, ToVec4)
{
  Go::Point X(1.0, 2.0, 3.0);
  Vec4 result = SplineUtils::toVec4(X,4.0);
  EXPECT_FLOAT_EQ(result.x, 1.0);
  EXPECT_FLOAT_EQ(result.y, 2.0);
  EXPECT_FLOAT_EQ(result.z, 3.0);
  EXPECT_FLOAT_EQ(result.t, 4.0);
}

TEST(TestSplineUtils, PointCurve)
{
  Go::Line line(Go::Point(0.0, 0.0, 0.0), Go::Point(1.0, 0.0, 0.0));
  Go::SplineCurve* crv = line.createSplineCurve();

  Vec3 result;
  SplineUtils::point(result, 0.3, crv);
  EXPECT_FLOAT_EQ(result.x, 0.3);
  EXPECT_FLOAT_EQ(result.y, 0.0);
  EXPECT_FLOAT_EQ(result.z, 0.0);
}

TEST(TestSplineUtils, PointSurface)
{
  Go::Plane plane(Go::Point(0.0, 0.0, 0.0), Go::Point(1.0, 0.0, 0.0));
  Go::SplineSurface* srf = plane.createSplineSurface();

  Vec3 result;
  SplineUtils::point(result, 0.3, 0.3, srf);
  EXPECT_FLOAT_EQ(result.x, 0.0);
  EXPECT_FLOAT_EQ(result.y, 0.3);
  EXPECT_FLOAT_EQ(result.z, 0.3);
}

TEST(TestSplineUtils, PointVolume)
{
  Go::SphereVolume sphere(1.0, Go::Point(0.0, 0.0, 0.0),
                          Go::Point(0.0, 0.0, 1.0), Go::Point(1.0, 0.0, 0.0));
  Go::SplineVolume* vol = sphere.geometryVolume();

  Vec3 result;
  SplineUtils::point(result, 0.3, 0.3, 0.3, vol);
  EXPECT_FLOAT_EQ(result.x, 0.2392104875250847);
  EXPECT_FLOAT_EQ(result.y, 0.1603249784385625);
  EXPECT_FLOAT_EQ(result.z, 0.08410852481577462);
}

TEST(TestSplineUtils, ExtractBasisSurface)
{
  Go::Plane plane(Go::Point(0.0, 0.0, 0.0), Go::Point(0.0, 0.0, 1.0),
                  Go::Point(sqrt(2.0), sqrt(2.0), 0.0));
  Go::SplineSurface* srf = plane.createSplineSurface();
  srf->setParameterDomain(0.0, 1.0, 0.0, 1.0);

  std::vector<Go::BasisDerivsSf> spline;
  std::vector<Go::BasisDerivsSf2> spline2;
  Matrix gpar(1, 1);
  gpar(1,1) = 0.3;
  srf->computeBasisGrid(gpar,gpar,spline);
  srf->raiseOrder(1,1);
  srf->computeBasisGrid(gpar,gpar,spline2);

  Vector N;
  Matrix dNdU;
  Matrix3D d2NdU2;
  SplineUtils::extractBasis(spline[0],N,dNdU);
  CHECK_VECTORS_EQUAL(N, "src/Utility/Test/refdata/ExtractBasis_srf_N.asc");
  CHECK_MATRICES_EQUAL(dNdU, "src/Utility/Test/refdata/ExtractBasis_srf_dNdU.asc");
  SplineUtils::extractBasis(spline2[0],N,dNdU,d2NdU2);
  CHECK_MATRICES3D_EQUAL(d2NdU2, "src/Utility/Test/refdata/ExtractBasis_srf_d2NdU2.asc");
}

TEST(TestSplineUtils, ExtractBasisVolume)
{
  Go::SphereVolume sphere(1.0, Go::Point(0.0, 0.0, 0.0),
                          Go::Point(0.0, 0.0, 1.0), Go::Point(1.0, 0.0, 0.0));
  Go::SplineVolume* vol = sphere.geometryVolume();
  vol->setParameterDomain(0.0, 1.0, 0.0, 1.0, 0.0, 1.0);

  std::vector<Go::BasisDerivs> spline;
  std::vector<Go::BasisDerivs2> spline2;
  Matrix gpar(1, 1);
  gpar(1,1) = 0.3;
  vol->computeBasisGrid(gpar,gpar,gpar,spline);
  vol->raiseOrder(1,1,1);
  vol->computeBasisGrid(gpar,gpar,gpar,spline2);

  Vector N;
  Matrix dNdU;
  Matrix3D d2NdU2;
  SplineUtils::extractBasis(spline[0],N,dNdU);
  CHECK_VECTORS_EQUAL(N, "src/Utility/Test/refdata/ExtractBasis_vol_N.asc");
  CHECK_MATRICES_EQUAL(dNdU, "src/Utility/Test/refdata/ExtractBasis_vol_dNdU.asc");
  SplineUtils::extractBasis(spline2[0],N,dNdU,d2NdU2);
  CHECK_MATRICES3D_EQUAL(d2NdU2, "src/Utility/Test/refdata/ExtractBasis_vol_d2NdU2.asc");
}

TEST(TestSplineUtils, ProjectCurve)
{
  Go::Line line(Go::Point(0.0, 0.0, 0.0), Go::Point(1.0, 0.0, 0.0));
  Go::SplineCurve* crv = line.createSplineCurve();
  crv->setParameterInterval(0.0, 2*M_PI);

  EvalFunction func("sin(x)*t");
  VecFuncExpr func2("sin(x)*t|cos(x)*t");
  Go::SplineCurve* prjCrv  = SplineUtils::project(crv, func , 1, 0.1);
  Go::SplineCurve* prjCrv2 = SplineUtils::project(crv, func2, 2, 0.1);

  Vec3 result1, result2, result3, result4;
  SplineUtils::point(result1, 0.5, prjCrv);
  SplineUtils::point(result2, 0.8, prjCrv);
  SplineUtils::point(result3, 0.5, prjCrv2);
  SplineUtils::point(result4, 0.8, prjCrv2);

  EXPECT_FLOAT_EQ(result1.x, -0.0783364);
  EXPECT_FLOAT_EQ(result2.x, -0.0694399);
  EXPECT_FLOAT_EQ(result3.x, -0.0783364);
  EXPECT_FLOAT_EQ(result3.y, -0.0363385);
  EXPECT_FLOAT_EQ(result4.x, -0.0694399);
  EXPECT_FLOAT_EQ(result4.y, -0.0363385);
}

TEST(TestSplineUtils, ProjectSurface)
{
  Go::Disc disc(Go::Point(0.0, 0.0, 0.0), 1.0,
                Go::Point(1.0/sqrt(2.0), 1.0/sqrt(2.0), 0.0),
                Go::Point(0.0, 0.0, 1.0));
  Go::SplineSurface* srf = disc.createSplineSurface();
  srf->setParameterDomain(0.0, 1.0, 0.0, 1.0);

  EvalFunction func("sin(x)*sin(y)*t");
  VecFuncExpr func2("sin(x)*sin(y)*t|cos(x)*cos(y)*t");
  Go::SplineSurface* prjSrf  = SplineUtils::project(srf, func , 1, 0.1);
  Go::SplineSurface* prjSrf2 = SplineUtils::project(srf, func2, 2, 0.1);

  Vec3 result1, result2, result3, result4;
  SplineUtils::point(result1, 0.5, 0.5, prjSrf);
  SplineUtils::point(result2, 0.8, 0.8, prjSrf);
  SplineUtils::point(result3, 0.5, 0.5, prjSrf2);
  SplineUtils::point(result4, 0.8, 0.8, prjSrf2);
  EXPECT_FLOAT_EQ(result1.x,  0.02110140763086564);
  EXPECT_FLOAT_EQ(result2.x, -0.02189938149140131);
  EXPECT_FLOAT_EQ(result3.x,  0.02110140763086564);
  EXPECT_FLOAT_EQ(result3.y,  0.07889859236913437);
  EXPECT_FLOAT_EQ(result4.x, -0.02189938149140131);
  EXPECT_FLOAT_EQ(result4.y,  0.06514225417205573);
}
