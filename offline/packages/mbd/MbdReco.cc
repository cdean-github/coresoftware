#include "MbdReco.h"
#include "MbdEvent.h"
#include "MbdGeomV1.h"
#include "MbdOutV2.h"
#include "MbdPmtContainerV1.h"
#include "MbdPmtSimContainerV1.h"

#include <globalvertex/MbdVertexMapv1.h>
#include <globalvertex/MbdVertexv2.h>

#include <ffarawobjects/CaloPacket.h>

#include <fun4all/Fun4AllReturnCodes.h>


#include <Event/Event.h>

#include <phool/PHCompositeNode.h>
#include <phool/PHIODataNode.h>
#include <phool/PHNode.h>
#include <phool/PHNodeIterator.h>
#include <phool/PHObject.h>
#include <phool/PHRandomSeed.h>
#include <phool/getClass.h>
#include <phool/phool.h>

#include <ffaobjects/EventHeader.h>
#include <ffarawobjects/CaloPacketContainer.h>
#include <ffarawobjects/Gl1Packet.h>

#include <TF1.h>

//____________________________________________________________________________..
MbdReco::MbdReco(const std::string &name)
  : SubsysReco(name)
{
}

//____________________________________________________________________________..
int MbdReco::Init(PHCompositeNode * /*topNode*/)
{
  m_gaussian = std::make_unique<TF1>("gaussian", "gaus", 0, 20);
  m_gaussian->FixParameter(2, m_tres);

  m_mbdevent = std::make_unique<MbdEvent>(_calpass,_always_process_charge);
  if ( Verbosity()>0 )
  {
    m_mbdevent->Verbosity( Verbosity() );
  }

  return Fun4AllReturnCodes::EVENT_OK;
}

//____________________________________________________________________________..
int MbdReco::InitRun(PHCompositeNode *topNode)
{
  if (createNodes(topNode) == Fun4AllReturnCodes::ABORTEVENT)
  {
    return Fun4AllReturnCodes::ABORTEVENT;
  }

  int ret = getNodes(topNode);

  m_mbdevent->SetSim(_simflag);
  m_mbdevent->InitRun();

  return ret;
}

//____________________________________________________________________________..
int MbdReco::process_event(PHCompositeNode *topNode)
{
  getNodes(topNode);

  if ( (m_mbdevent==nullptr && m_mbdraw==nullptr && m_mbdpacket[0]==nullptr && m_mbdpacket[1]==nullptr) || m_mbdpmts==nullptr )
  {
    static int counter = 0;
    if ( counter<2 )
    {
      std::cout << PHWHERE << " ERROR, didn't find mbdevent, mbdraw, or mbdpmts" << std::endl;
      counter++;
    }
    return Fun4AllReturnCodes::ABORTEVENT;  // missing an essential object in BBC/MBD
  }

  if (m_mbdpacket[0] && m_mbdpacket[1] &&
      (m_mbdpacket[0]->getIdentifier() != 1001 || m_mbdpacket[1]->getIdentifier() != 1002))
  {
    static int counter = 0;
    if (counter < 100)
    {
      std::cout << PHWHERE << "packet 1001 and/or packet 1002 missing, bailing out" << std::endl;
      counter++;
    }
    return Fun4AllReturnCodes::EVENT_OK; // no mbd packets here
  }

  // Process raw waveforms from real data
  if ( m_mbdevent!=nullptr || m_mbdraw!=nullptr || m_mbdpacket[0]==nullptr || m_mbdpacket[1]==nullptr)
  {
    int status = Fun4AllReturnCodes::EVENT_OK;
    if ( m_evtheader!=nullptr )
    {
      m_mbdevent->set_EventNumber( m_evtheader->get_EvtSequence() );
    }
    if ( m_event!=nullptr )
    {
      status = m_mbdevent->SetRawData(m_event, m_mbdpmts);
    }
    else if ( m_mbdraw!=nullptr || m_mbdpacket[0]!=nullptr || m_mbdpacket[1]!=nullptr)
    {
      if (m_mbdraw)
      {
	m_mbdpacket[0] = m_mbdraw->getPacketbyId(1001);
	m_mbdpacket[1] = m_mbdraw->getPacketbyId(1002);
      }
      status = m_mbdevent->SetRawData(m_mbdpacket, m_mbdpmts,m_gl1raw);
    }

    if (status == Fun4AllReturnCodes::DISCARDEVENT )
    {
      static int counter = 0;
      if ( counter<3 )
      {
        std::cout << PHWHERE << " Warning, MBD discarding event " << std::endl;
        counter++;
      }
      return Fun4AllReturnCodes::DISCARDEVENT;
    }
    if (status == Fun4AllReturnCodes::ABORTEVENT )
    {
      static int counter = 0;
      if ( counter<3 )
      {
        std::cout << PHWHERE << " Warning, MBD aborting event " << std::endl;
        counter++;
      }
      return Fun4AllReturnCodes::ABORTEVENT;
    }
    if ( status == -1001 )
    {
      // calculating sampmax on this event
      return Fun4AllReturnCodes::DISCARDEVENT;
    }
    if (status < 0)
    {
      return Fun4AllReturnCodes::EVENT_OK;
    }
  }

  // Calibrate from UNCALDST or recalibrate from DST
  if ( _calpass==3 )
  {
    m_mbdevent->ProcessRawPackets( m_mbdpmts );
  }

  m_mbdevent->Calculate(m_mbdpmts, m_mbdout, topNode);

  // For multiple global vertex
  if (m_mbdevent->get_bbcn(0) > 0 && m_mbdevent->get_bbcn(1) > 0 && _calpass==0 )
  {
    auto *vertex = new MbdVertexv2();
    vertex->set_t(m_mbdevent->get_bbct0());
    vertex->set_z(m_mbdevent->get_bbcz());
    vertex->set_z_err(0.6);
    vertex->set_t_err(m_tres);
    vertex->set_beam_crossing(0);

    m_mbdvtxmap->insert(vertex);

    // copy to globalvertex
  }

  if (Verbosity() > 0)
  {
    std::cout << "mbd vertex z and t0 " << m_mbdevent->get_bbcz() << ", " << m_mbdevent->get_bbct0() << std::endl;
  }

  return Fun4AllReturnCodes::EVENT_OK;
}

//____________________________________________________________________________..
int MbdReco::End(PHCompositeNode * /*unused*/)
{
  m_mbdevent->End();

  return Fun4AllReturnCodes::EVENT_OK;
}

int MbdReco::createNodes(PHCompositeNode *topNode)
{
  PHNodeIterator iter(topNode);
  PHCompositeNode *dstNode = dynamic_cast<PHCompositeNode *>(iter.findFirst("PHCompositeNode", "DST"));
  if (!dstNode)
  {
    std::cout << PHWHERE << "DST Node missing doing nothing" << std::endl;
    return Fun4AllReturnCodes::ABORTRUN;
  }

  PHCompositeNode *runNode = dynamic_cast<PHCompositeNode *>(iter.findFirst("PHCompositeNode", "RUN"));
  if (!runNode)
  {
    std::cout << PHWHERE << "RUN Node missing doing nothing" << std::endl;
    return Fun4AllReturnCodes::ABORTRUN;
  }

  PHNodeIterator dstiter(dstNode);
  PHCompositeNode *bbcNode = dynamic_cast<PHCompositeNode *>(dstiter.findFirst("PHCompositeNode", "MBD"));
  if (!bbcNode)
  {
    bbcNode = new PHCompositeNode("MBD");
    dstNode->addNode(bbcNode);
  }

  PHNodeIterator runiter(runNode);
  PHCompositeNode *bbcRunNode = dynamic_cast<PHCompositeNode *>(runiter.findFirst("PHCompositeNode", "MBD"));
  if (!bbcRunNode)
  {
    bbcRunNode = new PHCompositeNode("MBD");
    runNode->addNode(bbcRunNode);
  }

  m_mbdout = findNode::getClass<MbdOut>(bbcNode, "MbdOut");
  if (!m_mbdout)
  {
    m_mbdout = new MbdOutV2();
    PHIODataNode<PHObject> *MbdOutNode = new PHIODataNode<PHObject>(m_mbdout, "MbdOut", "PHObject");
    bbcNode->addNode(MbdOutNode);
  }

  m_mbdpmts = findNode::getClass<MbdPmtSimContainerV1>(bbcNode, "MbdPmtContainer");
  if (!m_mbdpmts)
  {
    m_mbdpmts = new MbdPmtContainerV1();

    PHIODataNode<PHObject> *MbdPmtContainerNode = new PHIODataNode<PHObject>(m_mbdpmts, "MbdPmtContainer", "PHObject");
    bbcNode->addNode(MbdPmtContainerNode);
  }

  PHCompositeNode *globalNode = dynamic_cast<PHCompositeNode *>(dstiter.findFirst("PHCompositeNode", "GLOBAL"));
  if (!globalNode)
  {
    globalNode = new PHCompositeNode("GLOBAL");
    dstNode->addNode(globalNode);
  }

  m_mbdvtxmap = findNode::getClass<MbdVertexMap>(globalNode, "MbdVertexMap");
  if (!m_mbdvtxmap)
  {
    m_mbdvtxmap = new MbdVertexMapv1();
    PHIODataNode<PHObject> *VertexMapNode = new PHIODataNode<PHObject>(m_mbdvtxmap, "MbdVertexMap", "PHObject");
    globalNode->addNode(VertexMapNode);
  }

  m_mbdgeom = findNode::getClass<MbdGeom>(runNode, "MbdGeom");
  if (!m_mbdgeom)
  {
    m_mbdgeom = new MbdGeomV1();
    PHIODataNode<PHObject> *MbdGeomNode = new PHIODataNode<PHObject>(m_mbdgeom, "MbdGeom", "PHObject");
    bbcRunNode->addNode(MbdGeomNode);
  }

  return Fun4AllReturnCodes::EVENT_OK;
}

int MbdReco::getNodes(PHCompositeNode *topNode)
{
  // Get the bbc prdf data to mpcRawContent
  m_event = findNode::getClass<Event>(topNode, "PRDF");
  // std::cout << "event addr " << (unsigned int)m_event << endl;

  // Get the raw data from event combined DST
  m_mbdraw = findNode::getClass<CaloPacketContainer>(topNode, "MBDPackets");

  m_mbdpacket[0] = findNode::getClass<CaloPacket>(topNode,1001);
  m_mbdpacket[1] = findNode::getClass<CaloPacket>(topNode,1002);
  if (!m_event && !m_mbdraw && !m_mbdpacket[0] && !m_mbdpacket[1])
  {
    // not PRDF and not event combined DST, so we assume this is a sim file
    _simflag = 1;

    static int counter = 0;
    if (counter < 1)
    {
      std::cout << PHWHERE << "Unable to get PRDF or Event Combined DST, assuming this is simulation" << std::endl;
      counter++;
    }
  }

  // Get the raw gl1 data from event combined DST
  m_gl1raw = findNode::getClass<Gl1Packet>(topNode,14001);
  if (!m_gl1raw)
  {
    m_gl1raw = findNode::getClass<Gl1Packet>(topNode, "GL1Packet");
  }
  /*
  if ( !m_gl1raw )
  {
    cout << PHWHERE << " Gl1Packet node not found on node tree" << endl;
  }
  */
  

  // MbdPmtContainer
  m_mbdpmts = findNode::getClass<MbdPmtContainer>(topNode, "MbdPmtContainer");
  if (!m_mbdpmts)
  {
    std::cout << PHWHERE << " MbdPmtContainer node not found on node tree" << std::endl;
    return Fun4AllReturnCodes::ABORTEVENT;
  }

  m_mbdvtxmap = findNode::getClass<MbdVertexMap>(topNode, "MbdVertexMap");
  if (!m_mbdvtxmap)
  {
    std::cout << PHWHERE << "MbdVertexMap node not found on node tree" << std::endl;
    return Fun4AllReturnCodes::ABORTEVENT;
  }

  m_evtheader = findNode::getClass<EventHeader>(topNode, "EventHeader");
  if (!m_evtheader )
  {
    static int ctr = 0;
    if ( ctr<4 )
    {
      std::cout << PHWHERE << " EventHeader node not found on node tree" << std::endl;
      ctr++;
    }
  }

  return Fun4AllReturnCodes::EVENT_OK;
}
