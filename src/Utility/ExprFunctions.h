// $Id$
//==============================================================================
//!
//! \file ExprFunctions.h
//!
//! \date Dec 1 2011
//!
//! \author Arne Morten Kvarving / SINTEF
//!
//! \brief Expression function implementations.
//!
//==============================================================================

#ifndef _EXPR_FUNCTIONS_H
#define _EXPR_FUNCTIONS_H

#include "Function.h"
#include "TensorFunction.h"
#include <string>
#include <vector>
#include <array>
#ifdef USE_OPENMP
#include <omp.h>
#endif

namespace ExprEval {
  class Expression;
  class FunctionList;
  class ValueList;
}


/*!
  \brief A scalar-valued function, general expression.
*/

class EvalFunc : public ScalarFunc
{
  std::vector<ExprEval::Expression*> expr; //!< Roots of the expression tree
  std::vector<ExprEval::FunctionList*>  f; //!< Lists of functions
  std::vector<ExprEval::ValueList*>     v; //!< Lists of variables and constants

  std::vector<Real*> arg; //!< Function argument values

public:
  //! \brief The constructor parses the expression string.
  EvalFunc(const char* function, const char* x = "x" );
  //! \brief The destructor frees the dynamically allocated objects.
  virtual ~EvalFunc();

  static int numError; //!< Error counter - set by the exception handler

protected:
  //! \brief Non-implemented copy constructor to disallow copying.
  EvalFunc(const EvalFunc&);
  //! \brief Non-implemented assigment operator to disallow copying.
  EvalFunc& operator=(const EvalFunc&);
  //! \brief Evaluates the function expression.
  virtual Real evaluate(const Real& x) const;

  //! \brief Cleanup allocated data.
  void cleanup();
};


/*!
  \brief A scalar-valued spatial function, general function expression.
*/

class EvalFunction : public RealFunc
{
  std::vector<ExprEval::Expression*> expr; //!< Roots of the expression tree
  std::vector<ExprEval::FunctionList*>  f; //!< Lists of functions
  std::vector<ExprEval::ValueList*>     v; //!< Lists of variables and constants

  //! \brief A struct representing a spatial function argument.
  struct Arg
  {
    Real* x; //!< X-coordinate
    Real* y; //!< Y-coordinate
    Real* z; //!< Z-coordinate
    Real* t; //!< Time
  };

  std::vector<Arg> arg; //!< Function argument values

  std::array<EvalFunction*,3> gradient;  //!< First derivative expressions
  std::array<EvalFunction*,6> dgradient; //!< Second derivative expressions

  bool IAmConstant; //!< Indicates whether the time coordinate is given or not

public:
  //! \brief The constructor parses the expression string.
  EvalFunction(const char* function);
  //! \brief The destructor frees the dynamically allocated objects.
  virtual ~EvalFunction();

  //! \brief Adds an expression function for a first or second derivative.
  void addDerivative(const std::string& function, const std::string& variables,
                     int d1, int d2 = 0);

  //! \brief Returns whether the function is time-independent or not.
  virtual bool isConstant() const { return IAmConstant; }

  //! \brief Returns first-derivative of the function.
  virtual Real deriv(const Vec3& X, int dir) const;
  //! \brief Returns second-derivative of the function.
  virtual Real dderiv(const Vec3& X, int dir1, int dir2) const;

protected:
  //! \brief Evaluates the function expression.
  virtual Real evaluate(const Vec3& X) const;
  //! \brief Non-implemented copy constructor to disallow copying.
  EvalFunction(const EvalFunction&);
  //! \brief Non-implemented assignment operator to disallow copying.
  EvalFunction& operator=(const EvalFunction&);

  //! \brief Cleanup allocated data.
  void cleanup();
};


/*!
  \brief A base class for multi-component expression functions.
*/

class EvalFunctions
{
protected:
  //! \brief The constructor parses the expression string for each component.
  EvalFunctions(const std::string& functions, const std::string& variables);
  //! \brief The destructor frees the dynamically allocated function components.
  virtual ~EvalFunctions();

public:
  //! \brief Adds an expression function for a first or second derivative.
  void addDerivative(const std::string& functions,
                     const std::string& variables, int d1, int d2 = 0);

protected:
  std::vector<EvalFunction*> p; //!< Array of component expressions
};


/*!
  \brief A general spatial expression function of any return type.
  \details The function is implemented as an array of EvalFunction objects.
*/

template <class ParentFunc, class Ret>
class EvalMultiFunction : public ParentFunc, public EvalFunctions
{
  size_t nsd; //!< Number of spatial dimensions

public:
  //! \brief The constructor parses the expression string for each component.
  EvalMultiFunction<ParentFunc,Ret>(const std::string& functions,
                                    const std::string& variables = "")
    : EvalFunctions(functions,variables), nsd(0) { this->setNoDims(); }

  //! \brief Empty destructor.
  virtual ~EvalMultiFunction<ParentFunc,Ret>() {}

  //! \brief Returns whether the function is time-independent or not.
  virtual bool isConstant() const
  {
    for (EvalFunction* func : p)
      if (!func->isConstant())
        return false;
    return true;
  }

  //! \brief Returns first-derivative of the function.
  virtual Ret deriv(const Vec3& X, int dir) const;
  //! \brief Returns second-derivative of the function.
  virtual Ret dderiv(const Vec3& X, int dir1, int dir2) const;

protected:
  //! \brief Sets the number of spatial dimensions (default implementation).
  void setNoDims() { ParentFunc::ncmp = nsd = p.size(); }

  //! \brief Evaluates the function expressions.
  virtual Ret evaluate(const Vec3& X) const;
};

//! Vector-valued function expression
typedef EvalMultiFunction<VecFunc,Vec3>           VecFuncExpr;
//! Tensor-valued function expression
typedef EvalMultiFunction<TensorFunc,Tensor>      TensorFuncExpr;
//! Symmetric tensor-valued function expression
typedef EvalMultiFunction<STensorFunc,SymmTensor> STensorFuncExpr;

//! \brief Specialization for vector functions.
template<> Vec3 VecFuncExpr::evaluate(const Vec3& X) const;

//! \brief Specialization for tensor functions.
template<> void TensorFuncExpr::setNoDims();
//! \brief Specialization for tensor functions.
template<> Tensor TensorFuncExpr::evaluate(const Vec3& X) const;
//! \brief Specialization for tensor functions.
template<> Tensor TensorFuncExpr::deriv(const Vec3& X, int dir) const;
//! \brief Specialization for tensor functions.
template<> Tensor TensorFuncExpr::dderiv(const Vec3& X, int d1, int d2) const;

//! \brief Specialization for symmetric tensor functions.
template<> void STensorFuncExpr::setNoDims();
//! \brief Specialization for symmetric tensor functions.
template<> SymmTensor STensorFuncExpr::evaluate(const Vec3& X) const;
//! \brief Specialization for symmetric tensor functions.
template<> SymmTensor STensorFuncExpr::deriv(const Vec3& X, int dir) const;
//! \brief Specialization for symmetric tensor functions.
template<> SymmTensor STensorFuncExpr::dderiv(const Vec3& X,
                                              int d1, int d2) const;

#endif
