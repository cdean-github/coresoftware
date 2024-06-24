#ifndef MICROMEGAS_MICROMEGASRAWDATACALIBRATION_H
#define MICROMEGAS_MICROMEGASRAWDATACALIBRATION_H

/*!
 * \file MicromegasRawDataCalibration.h
 * \author Hugo Pereira Da Costa <hugo.pereira-da-costa@cea.fr>
 */

#include "MicromegasMapping.h"

#include <fun4all/SubsysReco.h>

#include <map>
#include <memory>
#include <string>

class PHCompositeNode;
class TFile;
class TH1;
class TH2;
class TProfile;

/// micromegas raw data decoder
class MicromegasRawDataCalibration : public SubsysReco
{
  public:

  /// constructor
  MicromegasRawDataCalibration( const std::string &name = "MicromegasRawDataCalibration" );

  /// global initialization
  int Init(PHCompositeNode*) override;

  /// run initialization
  int InitRun(PHCompositeNode*) override;

  /// event processing
  int process_event(PHCompositeNode*) override;

  /// end of processing
  int End(PHCompositeNode*) override;

  /// set min sample for noise estimation
  void set_sample_min( int value ) { m_sample_min = value; }

  /// set min sample for noise estimation
  void set_sample_max( int value ) { m_sample_max = value; }

  /// set to true to store evaluation histograms and ntuples
  void set_calibration_file( const std::string& value ) { m_calibration_filename = value; }

  private:

  //! mapping
  MicromegasMapping m_mapping;

  /// min sample for noise estimation
  int m_sample_min = 0;

  /// max sample for noise estimation
  int m_sample_max = 100;

  /// calibration output file
  std::string m_calibration_filename = "TPOT_Pedestal_000.root";

  /// map fee id to Profile histogram
  using profile_map_t = std::map<int, TProfile*>;
  profile_map_t m_profile_map;

};

#endif
