// $Id$
//==============================================================================
//!
//! \file SIMSolver.h
//!
//! \date Oct 12 2012
//!
//! \author Arne Morten Kvarving / SINTEF
//!
//! \brief SIM solver class template.
//!
//==============================================================================

#ifndef _SIM_SOLVER_H_
#define _SIM_SOLVER_H_

#include "IFEM.h"
#include "SIMadmin.h"
#include "TimeStep.h"
#include "HDF5Writer.h"
#include "XMLWriter.h"
#include "tinyxml.h"


/*!
  \brief Struct for configuring a given simulator.
  \details Your SIM needs to specialize this for its type.
*/

template<class T>
struct SolverConfigurator
{
  //! \brief Configures a simulator.
  //! \param sim The simulator to configure
  //! \param props The setup properties for the simulator
  //! \param infile The input file to parse
  int setup(T& sim, const typename T::SetupProps& props, char* infile);
};


//! \brief Configuration template
template<class T>
int ConfigureSIM(T& t, char* infile,
                 const typename T::SetupProps& p = typename T::SetupProps())
{
  SolverConfigurator<T> setup;
  return setup.setup(t, p, infile);
}


/*!
  \brief Template class for simulator drivers.
  \details This template can be instanciated over any type implementing the
  ISolver interface. It provides a time stepping loop with data output.
*/

template<class T1> class SIMSolver : public SIMadmin
{
public:
  //! \brief The constructor initializes the reference to the actual solver.
  SIMSolver(T1& s1) : SIMadmin("Time integration driver"), S1(s1)
  {
    saveDivergedSol = false;
    exporter = nullptr;
  }

  //! \brief The destructor deletes the results data exporter object.
  virtual ~SIMSolver() { delete exporter; }

  //! \brief Returns a const reference to the time stepping information.
  const TimeStep& getTimePrm() const { return tp; }

  //! \brief Advances the time step one step forward.
  bool advanceStep() { return tp.increment() && S1.advanceStep(tp); }

  //! \brief Solves the problem up to the final time.
  virtual int solveProblem(char* infile, const char* heading = nullptr,
                           bool saveInit = true)
  {
    // Save FE model to VTF and HDF5 for visualization
    // Optionally save the initial configuration also
    int geoBlk = 0, nBlock = 0;
    if (!this->saveState(geoBlk,nBlock,true,infile,saveInit))
      return 2;

    this->printHeading(heading);

    // Solve for each time step up to final time
    for (int iStep = 1; this->advanceStep(); iStep++)
      if (!S1.solveStep(tp))
        return saveDivergedSol && !S1.saveStep(tp,nBlock) ? 4 : 3;
      else if (!this->saveState(geoBlk,nBlock))
        return 4;
      else
        IFEM::pollControllerFifo();

    return 0;
  }

  //! \brief Serialize internal state for restarting purposes.
  //! \param data Container for serialized data
  bool serialize(DataExporter::SerializeData& data)
  {
    return tp.serialize(data) && S1.serialize(data);
  }

  //! \brief Set internal state from a serialized state.
  //! \param[in] data Container for serialized data
  bool deSerialize(const DataExporter::SerializeData& data)
  {
    return tp.deSerialize(data) && S1.deSerialize(data);
  }

protected:
  //! \brief Parses a data section from an input stream.
  virtual bool parse(char* keyw, std::istream& is) { return tp.parse(keyw,is); }

  //! \brief Parses a data section from an XML element.
  virtual bool parse(const TiXmlElement* elem)
  {
    if (strcasecmp(elem->Value(),"postprocessing"))
      return tp.parse(elem);

    const TiXmlElement* child = elem->FirstChildElement();
    for (; child; child = child->NextSiblingElement())
      if (!strncasecmp(child->Value(),"savediverg",10))
        saveDivergedSol = true;

    return true;
  }

  //! \brief Writes an application-specific heading, if provided
  void printHeading(const char* heading)
  {
    if (heading)
    {
      std::string myHeading(heading);
      size_t n = myHeading.find_last_of('\n');
      if (n+1 < myHeading.size()) n = myHeading.size()-n;
      IFEM::cout <<"\n\n"<< myHeading <<"\n";
      for (size_t i = 0; i < n && i < myHeading.size(); i++) IFEM::cout <<'=';
      IFEM::cout << std::endl;
    }
  }

  //! \brief Saves geometry and results to VTF and HDF5 for current time step.
  bool saveState(int& geoBlk, int& nBlock, bool newMesh = false,
                 char* infile = nullptr, bool saveRes = true)
  {
    if (newMesh && !S1.saveModel(infile,geoBlk,nBlock))
      return false;

    if (saveRes && !S1.saveStep(tp,nBlock))
      return false;

    if (saveRes && exporter) {
      DataExporter::SerializeData data;
      if (exporter->dumpForRestart(&tp) && this->serialize(data))
        return exporter->dumpTimeLevel(&tp,newMesh,&data);
      else // no restart dump, or serialization failure
        return exporter->dumpTimeLevel(&tp,newMesh);
    }

    return true;
  }

public:
  //! \brief Handles application restarts by reading a serialized solver state.
  //! \param[in] restartFile File to read restart state from
  //! \param[in] restartStep Index of the time step to read restart state for
  //! \return One-based time step index of the restart state read.
  //! If zero, no restart specified. If negative, read failure.
  int restart(const std::string& restartFile, int restartStep)
  {
    if (restartFile.empty()) return 0;

    DataExporter::SerializeData data;
    HDF5Writer hdf(restartFile,adm,true);
    if ((restartStep = hdf.readRestartData(data,restartStep)) >= 0)
    {
      IFEM::cout <<"\n === Restarting from a serialized state ==="
                 <<"\n     file = "<< restartFile
                 <<"\n     step = "<< restartStep << std::endl;
      if (this->deSerialize(data))
        return restartStep+1;
      else
        restartStep = -2;
    }

    std::cerr <<" *** SIMSolver: Failed to read restart data."<< std::endl;
    return restartStep;
  }

  //! \brief Handles application data output.
  //! \param[in] hdf5file The file to save to
  //! \param[in] saveInterval The stride in the output file
  //! \param[in] restartInterval The stride in the restart file
  void handleDataOutput(const std::string& hdf5file,
                        int saveInterval = 1, int restartInterval = 0)
  {
    if (IFEM::getOptions().discretization < ASM::Spline && !hdf5file.empty())
      IFEM::cout <<"\n  ** HDF5 output is available for spline discretization"
                 <<" only. Deactivating...\n"<< std::endl;
    else
    {
      exporter = new DataExporter(true,saveInterval,restartInterval);
      exporter->registerWriter(new XMLWriter(hdf5file,adm));
      exporter->registerWriter(new HDF5Writer(hdf5file,adm));
      S1.registerFields(*exporter);
      IFEM::registerCallback(*exporter);
    }
  }

private:
  bool saveDivergedSol; //!< If \e true, save also the diverged solution to VTF

protected:
  TimeStep tp; //!< Time stepping information
  T1&      S1; //!< The actual solver

  DataExporter* exporter; //!< Administrator for result output to HDF5 file
};

#endif
