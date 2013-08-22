// $Id$
//==============================================================================
//!
//! \file NewmarkNLSIM.C
//!
//! \date Jul 4 2013
//!
//! \author Knut Morten Okstad / SINTEF
//!
//! \brief Newmark solution driver for isogeometric dynamic FEM simulators.
//!
//==============================================================================

#include "NewmarkNLSIM.h"
#include "SystemMatrix.h"
#include "SIMbase.h"
#include "TimeStep.h"
#include "tinyxml.h"


NewmarkNLSIM::NewmarkNLSIM (SIMbase& sim) : NewmarkSIM(sim), Finert(NULL)
{
  // Default Newmark parameters (alpha = -0.1)
  beta = 0.3025;
  gamma = 0.6;

  predictor = 'd'; // default predictor (constant displacement)
}


bool NewmarkNLSIM::parse (const TiXmlElement* elem)
{
  bool ok = this->NewmarkSIM::parse(elem);

  if (!strcasecmp(elem->Value(),"newmarksolver"))
  {
    const char* attr = elem->Attribute("alpha");
    double alpha = attr ? atof(attr) : -0.1;
    beta = 0.25*(1.0-alpha)*(1.0-alpha);
    gamma = 0.5 - alpha;
  }

  return ok;
}


void NewmarkNLSIM::init (size_t nSol)
{
  model.setIntegrationPrm(0,alpha1);
  model.setIntegrationPrm(1,alpha2);
  model.setIntegrationPrm(2,0.5-gamma);

  size_t nDOFs = model.getNoDOFs();
  size_t nSols = model.getNoSolutions();
  if (nSols > nSol) nSol = nSols;
  solution.resize(nSol);

  for (Vectors::iterator it = solution.begin(); it != solution.end(); ++it)
    it->resize(nDOFs,true);

  incDis.resize(nDOFs,true);
  predVel.resize(nDOFs,true);
  predAcc.resize(nDOFs,true);
}


bool NewmarkNLSIM::initEqSystem (bool withRF)
{
  return model.initSystem(opt.solver,1,2,withRF);
}


bool NewmarkNLSIM::advanceStep (TimeStep& param, bool updateTime)
{
  // Update displacement solutions between time steps
  for (int n = solution.size()-3; n > 0; n--)
    std::copy(solution[n-1].begin(),solution[n-1].end(),solution[n].begin());

  return updateTime ? param.increment() : true;
}


void NewmarkNLSIM::finalizeRHSvector ()
{
  if (Finert)
    model.addToRHSvector(0,*Finert,gamma-0.5);
}


bool NewmarkNLSIM::predictStep (TimeStep& param)
{
  if (solution.size() < 3) return false;

  size_t iA = solution.size() - 1;
  size_t iV = solution.size() - 2;

  // Predicted new velocity
  predVel = solution[iV];
  predVel *= gamma/beta - 1.0;
  predVel.add(solution[iA],(0.5*gamma/beta-1.0)*param.time.dt);

  // Predicted new acceleration
  predAcc = solution[iA];
  predAcc *= 0.5/beta - 1.0;
  predAcc.add(solution[iV],1.0/(beta*param.time.dt));

#if SP_DEBUG > 1
  std::cout <<"\nPredicted velocity:"<< predVel;
  std::cout <<"Predicted acceleration:"<< predAcc;
#endif

  solution[iV] = predVel;
  solution[iA] = predAcc;

  incDis.fill(0.0);
  predVel *= -1.0;
  predAcc *= -1.0;

  return true;
}


bool NewmarkNLSIM::correctStep (TimeStep& param, bool converged)
{
  if (solution.size() < 3) return false;

  size_t iD = 0;
  size_t iA = solution.size() - 1;
  size_t iV = solution.size() - 2;

  // Update current displacement, velocity and acceleration solutions
  incDis.add(linsol,1.0);
  solution[iD].add(linsol,1.0);
  solution[iV] = predVel;
  solution[iV].add(incDis,gamma/(beta*param.time.dt));
  solution[iA] = predAcc;
  solution[iA].add(incDis,1.0/(beta*param.time.dt*param.time.dt));

  if (converged)
  {
    // Save the actual inertia vector (minus the residual) from converged step
    delete Finert;
    Finert = model.getRHSvector(1,true);
  }

#if SP_DEBUG > 1
  std::cout <<"\nCorrected displacement:"<< solution[iD]
            <<"Corrected velocity:"<< solution[iV]
            <<"Corrected acceleration:"<< solution[iA];
  if (converged && Finert)
    std::cout <<"Actual inertia force:"<< *Finert;
#elif defined(SP_DEBUG)
  if (converged && solution[iD].size() < 100)
    std::cout <<"\nConverged displacement:"<< solution[iD]
              <<"Converged velocity:"<< solution[iV]
              <<"Converged acceleration:"<< solution[iA];
#endif

  return model.updateConfiguration(solution[iD]);
}