// $Id$
//==============================================================================
//!
//! \file SIMdependency.C
//!
//! \date May 22 2012
//!
//! \author Knut Morten Okstad / SINTEF
//!
//! \brief Administration of simulators with dependencies to other simulators.
//!
//==============================================================================

#include "SIMdependency.h"
#include "IntegrandBase.h"
#include "ASMbase.h"
#include "Fields.h"
#include "Field.h"


void SIMdependency::registerDependency (SIMdependency* sim,
                                        const std::string& name, short int nvc,
                                        const PatchVec& patches, bool diffBasis)
{
  Dependency dep;
  dep.sim = sim;
  dep.name = name;
  dep.components = nvc;
  dep.patches = patches;
  dep.differentBasis = diffBasis;
  depFields.push_back(dep);
}


void SIMdependency::registerDependency (SIMdependency* sim,
                                        const std::string& name, short int nvc)
{
  Dependency dep;
  dep.sim = sim;
  dep.name = name;
  dep.components = nvc;
  depFields.push_back(dep);
}


const utl::vector<double>* SIMdependency::getField (const std::string& name)
{
  FieldMap::const_iterator it = myFields.find(name);
  return it == myFields.end() ? NULL : it->second;
}


void SIMdependency::registerField (const std::string& name,
                                   const utl::vector<double>& vec)
{
  myFields[name] = &vec;
}


bool SIMdependency::extractPatchDependencies (IntegrandBase* problem,
                                              const PatchVec& model,
                                              size_t pindx)
{
  ASMbase* patch;
  DepVector::const_iterator it;
  for (it = depFields.begin(); it != depFields.end(); ++it)
  {
    const Vector* gvec = it->sim->getField(it->name);
    Vector* lvec = problem->getNamedVector(it->name);
    if (lvec && gvec && !gvec->empty())
    {
      if (pindx < it->patches.size())
	patch = it->patches[pindx];
      else
	patch = model[pindx];
      int bflag = it->components < 0 ? it->components : 0; // HACK
      patch->extractNodeVec(*gvec,*lvec,abs(it->components),bflag);
      if (it->differentBasis) {
	if (it->components == 1)
	  problem->setNamedField(it->name,Field::create(patch,*lvec));
	else
	  problem->setNamedFields(it->name,Fields::create(patch,*lvec));
      }
#if SP_DEBUG > 2
      std::cout <<"SIMdependency:  Dependent field \""<< it->name
                <<"\" for patch "<< pindx+1 << *lvec;
#endif
    }
  }

  return true;
}