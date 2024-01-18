// Tell emacs that this is a C++ source
//  -*- C++ -*-.
#ifndef CALOTOWEREMBED_H
#define CALOTOWEREMBED_H

#include "caloreco/CaloTowerDefs.h"


#include <calobase/TowerInfoContainer.h>  // for TowerInfoContainer, TowerIn...

#include <calobase/RawTowerGeom.h>
#include <calobase/RawTowerGeomContainer.h>

#include <fun4all/SubsysReco.h>

#include <cassert>
#include <iostream>
#include <string>

#include "TFile.h"
#include "TTree.h"

//class CDBInterface;
//class CDBTTree;
class PHCompositeNode;
class TowerInfoContainerv1;
class TowerInfoContainerv2;
class RawTowerGeomContainer;

class caloTowerEmbed : public SubsysReco
{
 public:
  caloTowerEmbed(const std::string &name = "caloTowerEmbed");

  ~caloTowerEmbed() override;

  int InitRun(PHCompositeNode *topNode) override;
  int process_event(PHCompositeNode *topNode) override;
  int End(PHCompositeNode *topNode) override;
  void CreateNodeTree(PHCompositeNode *topNode);

  void set_detector_type(CaloTowerDefs::DetectorSystem dettype)
  {
    m_dettype = dettype;
    return;
  }
  

  void set_inputNodePrefix(const std::string &name)
  {
    m_inputNodePrefix = name;
    return;
  }
  
  void set_useRetower(bool a)
  {
    m_useRetower = a;
    return;
  };

 private:


  TowerInfoContainer *_data_towers = nullptr;
  TowerInfoContainer *_sim_towers = nullptr;

  RawTowerGeomContainer *tower_geom = nullptr;

  bool m_useRetower = false;

  CaloTowerDefs::DetectorSystem m_dettype{CaloTowerDefs::DETECTOR_INVALID};

  std::string m_detector;
  std::string m_inputNodePrefix{"TOWERINFO_CALIB_"};

  int m_runNumber;
  int m_eventNumber;

};

#endif  // CALOTOWEREMBED_H
