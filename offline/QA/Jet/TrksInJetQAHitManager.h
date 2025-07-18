/// ===========================================================================
/*! \file   TrksInJetQAHitManager.h
 *  \author Derek Anderson
 *  \date   03.25.2024
 *
 *  A submodule for the TrksInJetQA module
 *  to generate QA plots for track hits
 */
/// ===========================================================================

#ifndef TRKSINJETQAHITMANAGER_H
#define TRKSINJETQAHITMANAGER_H

// submodule definitions
#include "TrksInJetQABaseManager.h"

// tracking includes
#include <trackbase/InttDefs.h>
#include <trackbase/MvtxDefs.h>
#include <trackbase/TpcDefs.h>
#include <trackbase/TrkrDefs.h>
#include <trackbase/TrkrHit.h>

// root includes
#include <TH1.h>
#include <TH2.h>

// c++ utilities
#include <limits>
#include <utility>
#include <vector>

// ============================================================================
//! Tracker hit histogram manager for TrksInJetQA module
// ============================================================================
/*! This histogram manager defines what to histogram
 *  from tracker hits.
 */
class TrksInJetQAHitManager : public TrksInJetQABaseManager
{
 public:
  ///! enumerate hit subsystem
  enum Type
  {
    All,
    Mvtx,
    Intt,
    Tpc
  };

  ///! enumerates 1D histograms
  enum H1D
  {
    Ene,
    ADC,
    Layer,
    PhiBin,
    ZBin
  };

  ///! enumerates 2D histograms
  enum H2D
  {
    EneVsLayer,
    EneVsADC,
    PhiVsZBin
  };

  // --------------------------------------------------------------------------
  //! Hit histogram content
  // --------------------------------------------------------------------------
  /*! A small struct to consolidate what variables
   *  to histogram for hits.
   */ 
  struct HitQAContent
  {
    double ene = std::numeric_limits<double>::max();
    uint64_t adc = std::numeric_limits<uint64_t>::max();
    uint16_t layer = std::numeric_limits<uint16_t>::max();
    uint16_t phiBin = std::numeric_limits<uint16_t>::max();
    uint16_t zBin = std::numeric_limits<uint16_t>::max();
  };

  // ctor/dtor
  using TrksInJetQABaseManager::TrksInJetQABaseManager;
  ~TrksInJetQAHitManager(){};

  // public methods
  void GetInfo(TrkrHit* hit, TrkrDefs::hitsetkey& setKey, TrkrDefs::hitkey& hitKey);

 private:
  // private methods
  void FillHistograms(const int type, HitQAContent& content);

  // inherited private methods
  void DefineHistograms() override;

};  // end TrksInJetQAHitManager

#endif

// end ========================================================================
