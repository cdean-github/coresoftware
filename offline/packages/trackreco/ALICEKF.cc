#include "ALICEKF.h"

#include "GPUTPCTrackLinearisation.h"
#include "GPUTPCTrackParam.h"

#include <trackbase/TrkrCluster.h>
#include <Geant4/G4SystemOfUnits.hh>
#include <trackbase_historic/ActsTransformations.h>
#include "TFile.h"
#include "TNtuple.h"

#include <TMatrixFfwd.h>
#include <TMatrixT.h>   
#include <TMatrixTUtils.h>
//#define _DEBUG_

#if defined(_DEBUG_)
#define LogDebug(exp) std::cout << "DEBUG: " << __FILE__ << ": " << __LINE__ << ": " << exp
#else
#define LogDebug(exp) (void)0
#endif

#define LogError(exp) if(Verbosity()>0) std::cout << "ERROR: " << __FILE__ << ": " << __LINE__ << ": " << exp
#define LogWarning(exp) if(Verbosity()>0) std::cout << "WARNING: " << __FILE__ << ": " << __LINE__ << ": " << exp

using keylist = std::vector<TrkrDefs::cluskey>;

// anonymous namespace for local functions
namespace
{
  // square
  template<class T> inline constexpr T square( const T& x ) { return x*x; }
}

bool ALICEKF::checknan(double val, const std::string &name, int num) const
{
  if(std::isnan(val))
  {
    if(Verbosity()>0) std::cout << "WARNING: " << name << " is NaN for seed " << num << ". Aborting this seed.\n";
  }
  return std::isnan(val);
}

double ALICEKF::get_Bz(double x, double y, double z) const
{
  if(_use_const_field) return 1.4;
  double p[4] = {x*cm,y*cm,z*cm,0.*cm};
  double bfield[3];
  _B->GetFieldValue(p,bfield);
  return bfield[2]/tesla;
}

double ALICEKF::getClusterError(TrkrCluster* c, Acts::Vector3 global, int i, int j) const
{
  if(_use_fixed_clus_error) 
  {
     if(i==j) return _fixed_clus_error.at(i)*_fixed_clus_error.at(i);
     else return 0.;
  }
  else 
    {
      TMatrixF localErr(3,3);
      localErr[0][0] = 0.;
      localErr[0][1] = 0.;
      localErr[0][2] = 0.;
      localErr[1][0] = 0.;
      localErr[1][1] = c->getActsLocalError(0,0);
      localErr[1][2] = c->getActsLocalError(0,1);
      localErr[2][0] = 0.;
      localErr[2][1] = c->getActsLocalError(1,0);
      localErr[2][2] = c->getActsLocalError(2,0);
      float clusphi = atan2(global(1), global(0));
      TMatrixF ROT(3,3);
      ROT[0][0] = cos(clusphi);
      ROT[0][1] = -sin(clusphi);
      ROT[0][2] = 0.0;
      ROT[1][0] = sin(clusphi);
      ROT[1][1] = cos(clusphi);
      ROT[1][2] = 0.0;
      ROT[2][0] = 0.0;
      ROT[2][1] = 0.0;
      ROT[2][2] = 1.0;
      TMatrixF ROT_T(3,3);
      ROT_T.Transpose(ROT);
  
      TMatrixF err(3,3);
      err = ROT * localErr * ROT_T;
      
      return err[i][j];
    }
}

std::vector<TrackSeed> ALICEKF::ALICEKalmanFilter(const std::vector<keylist>& trackSeedKeyLists,bool use_nhits_limit, const PositionMap& globalPositions) const
{
//  TFile* f = new TFile("/sphenix/u/mjpeters/macros_hybrid/detectors/sPHENIX/pull.root", "RECREATE");
//  TNtuple* ntp = new TNtuple("pull","pull","cx:cy:cz:xerr:yerr:zerr:tx:ty:tz:layer:xsize:ysize:phisize:phierr:zsize");
  std::vector<TrackSeed> seeds_vector;
  int nseeds = 0;
 
  if(Verbosity()>0) std::cout << "min clusters per track: " << _min_clusters_per_track << "\n";
  for( auto trackKeyChain:trackSeedKeyLists )
  {
    if(trackKeyChain.size()<2) continue;
    if(use_nhits_limit && trackKeyChain.size() < _min_clusters_per_track) continue;
    if(TrkrDefs::getLayer(trackKeyChain.front())<TrkrDefs::getLayer(trackKeyChain.back())) std::reverse(trackKeyChain.begin(),trackKeyChain.end());
    // get starting cluster from key
    // Transform sPHENIX coordinates into ALICE-compatible coordinates
    const auto& globalpos = globalPositions.at(trackKeyChain.at(0));
    double x0 = globalpos(0);
    double y0 = globalpos(1);
    double z0 = globalpos(2);;
    LogDebug("Initial (x,y,z): (" << x0 << "," << y0 << "," << z0 << ")" << std::endl);
    // ALICE x coordinate = distance from beampipe
    double alice_x0 = sqrt(x0*x0+y0*y0);
    double alice_y0 = 0;
    double alice_z0 = z0;
    // Initialize track and linearisation
    GPUTPCTrackParam trackSeed;
    trackSeed.InitParam();
    trackSeed.SetX(alice_x0);
    trackSeed.SetY(alice_y0);
    trackSeed.SetZ(alice_z0);
    double x = x0;
    double y = y0;
    #if defined(_DEBUG_)
    double z = z0;
    double alice_x = sqrt(x0*x0+y0*y0);
    #endif
    double trackCartesian_x = 0.;
    double trackCartesian_y = 0.;
    double trackCartesian_z = 0.;
    // Pre-set momentum-based parameters to improve numerical stability
    const auto& secondpos = globalPositions.at(trackKeyChain.at(1));

    const double second_x = secondpos(0);
    const double second_y = secondpos(1);
    const double second_z = secondpos(2);
    const double first_phi = atan2(y0,x0);
    const double second_alice_x = second_x*std::cos(first_phi)+second_y*std::sin(first_phi);
    const double delta_alice_x = second_alice_x - alice_x0;
    //double second_alice_y = (second_x/cos(first_phi)-second_y/sin(first_phi))/(sin(first_phi)/cos(first_phi)+cos(first_phi)/sin(first_phi));
    const double second_alice_y = -second_x*std::sin(first_phi)+second_y*std::cos(first_phi);
    const double init_SinPhi = second_alice_y / std::sqrt(square(delta_alice_x) + square(second_alice_y));
    const double delta_z = second_z - z0;
    const double init_DzDs = -delta_z / std::sqrt(square(delta_alice_x) + square(second_alice_y));
    trackSeed.SetSinPhi(init_SinPhi);
    LogDebug("Set initial SinPhi to " << init_SinPhi << std::endl);
    trackSeed.SetDzDs(init_DzDs);
    LogDebug("Set initial DzDs to " << init_DzDs << std::endl);
    
    // get initial pt estimate
    std::vector<std::pair<double,double>> pts;
    std::transform( trackKeyChain.begin(), trackKeyChain.end(), std::back_inserter( pts ), [&globalPositions]( const TrkrDefs::cluskey& key )
    {
      const auto& clpos = globalPositions.at(key);
      return std::make_pair(clpos(0),clpos(1));
    });
    
    double R = 0;
    double x_center = 0;
    double y_center = 0;
    CircleFitByTaubin(pts,R,x_center,y_center);
    if(Verbosity()>1) std::cout << "circle fit parameters: R=" << R << ", X0=" << x_center << ", Y0=" << y_center << std::endl;
    
    // check circle fit success
    /* failed fit will result in infinite momentum for the track, which in turn will break the kalman filter */
    if( std::isnan(R) ) continue;   
    
    double init_QPt = 1./(0.3*R/100.*get_Bz(x0,y0,z0));
    // determine charge
    double phi_first = atan2(y0,x0);
    if(Verbosity()>1) std::cout << "phi_first: " << phi_first << std::endl;
    double phi_second = atan2(second_y,second_x);
    if(Verbosity()>1) std::cout << "phi_second: " << phi_second << std::endl;
    double dphi = phi_second - phi_first;
    if(Verbosity()>1) std::cout << "dphi: " << dphi << std::endl;
    if(dphi>M_PI) dphi = 2*M_PI - dphi;
    if(dphi<-M_PI) dphi = 2*M_PI + dphi;
    if(Verbosity()>1) std::cout << "corrected dphi: " << dphi << std::endl;
    if(dphi<0) init_QPt = -1*init_QPt;
    LogDebug("initial QPt: " << init_QPt << std::endl);
    trackSeed.SetQPt(init_QPt);

  
    GPUTPCTrackLinearisation trackLine(trackSeed);

    LogDebug(std::endl << std::endl << "------------------------" << std::endl << "seed size: " << trackKeyChain.size() << std::endl << std::endl << std::endl);
    int cluster_ctr = 1;
//    bool aborted = false;
    // starting at second cluster, perform track propagation
    std::vector<double> cx;
    std::vector<double> cy;
    std::vector<double> cz;
    std::vector<double> tx;
    std::vector<double> ty;
    std::vector<double> tz;
    std::vector<double> xerr;
    std::vector<double> yerr;
    std::vector<double> zerr;
    std::vector<double> layer;
    std::vector<double> xsize;
    std::vector<double> ysize;
    std::vector<double> phisize;
    std::vector<double> phierr;
    std::vector<double> zsize;
    for(auto clusterkey = std::next(trackKeyChain.begin()); clusterkey != trackKeyChain.end(); ++clusterkey)
    {
      LogDebug("-------------------------------------------------------------" << std::endl);
      LogDebug("cluster " << cluster_ctr << " -> " << cluster_ctr + 1 << std::endl);
      LogDebug("this cluster (x,y,z) = (" << x << "," << y << "," << z << ")" << std::endl);
      LogDebug("layer " << (int)TrkrDefs::getLayer(*clusterkey) << std::endl);
      // get cluster from key
      TrkrCluster* nextCluster = _cluster_map->findCluster(*clusterkey);
      const auto& nextpos = globalPositions.at(*clusterkey);
     
      // find ALICE x-coordinate
      double nextCluster_x = nextpos(0);
      double nextCluster_xerr = sqrt(getClusterError(nextCluster,nextpos,0,0));
      double nextCluster_y = nextpos(1);
      double nextCluster_yerr = sqrt(getClusterError(nextCluster,nextpos,1,1));
      double nextCluster_z = nextpos(2);
      double nextCluster_zerr = sqrt(getClusterError(nextCluster,nextpos,2,2));
      // rotate track coordinates to match orientation of next cluster
      double newPhi = atan2(nextCluster_y,nextCluster_x);
      LogDebug("new phi = " << newPhi << std::endl);
      double oldPhi = atan2(y,x);
      LogDebug("old phi = " << oldPhi << std::endl);
      double alpha = newPhi - oldPhi;
      LogDebug("alpha = " << alpha << std::endl);
      if(!trackSeed.Rotate(alpha,trackLine,_max_sin_phi))
      {
        LogWarning("Rotate failed! Aborting for this seed...\n");
//        aborted = true;
        break;
      }
      double nextAlice_x = nextCluster_x*cos(newPhi)+nextCluster_y*sin(newPhi);
      LogDebug("track coordinates (ALICE) after rotation: (" << trackSeed.GetX() << "," << trackSeed.GetY() << "," << trackSeed.GetZ() << ")" << std::endl);
      LogDebug("Transporting from " << alice_x << " to " << nextAlice_x << "...");
      GPUTPCTrackParam::GPUTPCTrackFitParam fp;
      trackSeed.CalculateFitParameters(fp);
  //    for(int i=1;i<=10;i++)
  //    {
        double track_x = trackSeed.GetX()*cos(newPhi)-trackSeed.GetY()*sin(newPhi);
        double track_y = trackSeed.GetX()*sin(newPhi)+trackSeed.GetY()*cos(newPhi);
        double track_z = trackSeed.GetZ();
        if(!trackSeed.TransportToX(nextAlice_x,_Bzconst*get_Bz(track_x,track_y,track_z),_max_sin_phi)) // remember: trackLine was here
        {
          LogWarning("Transport failed! Aborting for this seed...\n");
//          aborted = true;
          break;
        }
  //    }
      // convert ALICE coordinates to sPHENIX cartesian coordinates, for debugging

      double predicted_alice_x = trackSeed.GetX();
      LogDebug("new track ALICE x = " << trackSeed.GetX() << std::endl);
      double predicted_alice_y = trackSeed.GetY();
      LogDebug("new track ALICE y = " << trackSeed.GetY() << std::endl);
      double predicted_z = trackSeed.GetZ();
      LogDebug("new track z = " << trackSeed.GetZ() << std::endl);
      double cos_phi = x/sqrt(x*x+y*y);
      LogDebug("cos_phi = " << cos_phi << std::endl);
      double sin_phi = y/sqrt(x*x+y*y);
      LogDebug("sin phi = " << sin_phi << std::endl);
      trackCartesian_x = predicted_alice_x*cos_phi-predicted_alice_y*sin_phi;
      trackCartesian_y = predicted_alice_x*sin_phi+predicted_alice_y*cos_phi;
      trackCartesian_z = predicted_z;
      LogDebug("Track transported to (x,y,z) = (" << trackCartesian_x << "," << trackCartesian_y << "," << trackCartesian_z << ")" << std::endl);
      LogDebug("Track position ALICE Y error: " << sqrt(trackSeed.GetCov(0)) << std::endl);
      LogDebug("Track position x error: " << sqrt(trackSeed.GetCov(0))*sin_phi << std::endl);
      LogDebug("Track position y error: " << sqrt(trackSeed.GetCov(0))*cos_phi << std::endl);
      LogDebug("Track position z error: " << sqrt(trackSeed.GetCov(5)) << std::endl);
      LogDebug("Next cluster is at (x,y,z) = (" << nextCluster_x << "," << nextCluster_y << "," << nextCluster_z << ")" << std::endl);
      LogDebug("Cluster errors: (" << nextCluster_xerr << ", " << nextCluster_yerr << ", " << nextCluster_zerr << ")" << std::endl);
      LogDebug("track coordinates (ALICE) after rotation: (" << trackSeed.GetX() << "," << trackSeed.GetY() << "," << trackSeed.GetZ() << ")" << std::endl);
      //double nextCluster_alice_y = (nextCluster_x/cos(newPhi) - nextCluster_y/sin(newPhi))/(tan(newPhi)+1./tan(newPhi));
      //double nextCluster_alice_y = 0.;
      double nextCluster_alice_y = -nextCluster_x*sin(newPhi)+nextCluster_y*cos(newPhi);
      LogDebug("next cluster ALICE y = " << nextCluster_alice_y << std::endl);
      double y2_error = getClusterError(nextCluster,nextpos,0,0)*sin(newPhi)*sin(newPhi)+2*getClusterError(nextCluster,nextpos,0,1)*cos(newPhi)*sin(newPhi)+getClusterError(nextCluster,nextpos,1,1)*cos(newPhi)*cos(newPhi);
      double z2_error = getClusterError(nextCluster,nextpos,2,2);
      LogDebug("track ALICE SinPhi = " << trackSeed.GetSinPhi() << std::endl);
      LogDebug("track DzDs = " << trackSeed.GetDzDs() << std::endl);
      LogDebug("chi2 = " << trackSeed.GetChi2() << std::endl);
      LogDebug("NDF = " << trackSeed.GetNDF() << std::endl);
      LogDebug("chi2 / NDF = " << trackSeed.GetChi2()/trackSeed.GetNDF() << std::endl);
  
      // Apply Kalman filter
      if(!trackSeed.Filter(nextCluster_alice_y,nextCluster_z,y2_error,z2_error,_max_sin_phi))
      {
	LogError("Kalman filter failed for seed " << nseeds << "! Aborting for this seed..." << std::endl);
//        aborted = true;
        break;
      }
      #if defined(_DEBUG_)
      double track_pt = 1./trackSeed.GetQPt();
      double track_pY = track_pt*trackSeed.GetSinPhi();
      double track_pX = sqrt(track_pt*track_pt-track_pY*track_pY);
      double track_px = track_pX*cos(newPhi)-track_pY*sin(newPhi);
      double track_py = track_pX*sin(newPhi)+track_pY*cos(newPhi);
      double track_pz = -track_pt*trackSeed.GetDzDs();
      double track_pterr = sqrt(trackSeed.GetErr2QPt())/(trackSeed.GetQPt()*trackSeed.GetQPt());
      #endif
      LogDebug("track pt = " << track_pt << " +- " << track_pterr << std::endl);
      LogDebug("track ALICE p = (" << track_pX << ", " << track_pY << ", " << track_pz << ")" << std::endl);
      LogDebug("track p = (" << track_px << ", " << track_py << ", " << track_pz << ")" << std::endl);
      x = nextCluster_x;
      y = nextCluster_y;
      #if defined(_DEBUG_)
      z = nextCluster_z;
      alice_x = nextAlice_x;
      #endif
      ++cluster_ctr;
  
      //if(cluster_ctr>10)
      {
    
	float nextclusrad = std::sqrt(nextCluster_x*nextCluster_x +
				      nextCluster_y*nextCluster_y);
	float nextclusphierr = nextCluster->getRPhiError() / nextclusrad;

	cx.push_back(nextCluster_x);
        cy.push_back(nextCluster_y);
        cz.push_back(nextCluster_z);
        tx.push_back(trackCartesian_x);
        ty.push_back(trackCartesian_y);
        tz.push_back(trackCartesian_z);
        xerr.push_back(nextCluster_xerr);
        yerr.push_back(nextCluster_yerr);
        zerr.push_back(nextCluster_zerr);
        layer.push_back(TrkrDefs::getLayer(*clusterkey));
        phierr.push_back(nextclusphierr);     
      }
  }
//    if(aborted) continue;
    if(Verbosity()>0) std::cout << "finished track\n";
/*
    // transport to beamline
//    float old_phi = atan2(y,x);
    float trackX = trackSeed.GetX();
    for(int i=99;i>=0;i--)
    {
      if(!trackSeed.TransportToX(i/100.*trackX,trackLine,_Bz,_max_sin_phi))
      {
        LogWarning("Transport failed! Aborting for this seed...\n");
        aborted = true;
        break;
      }
//      float new_phi = atan2(trackSeed.GetX()*sin(old_phi)+trackSeed.GetY()*cos(old_phi),trackSeed.GetX()*cos(old_phi)-trackSeed.GetY()*sin(old_phi));
//      if(!trackSeed.Rotate(new_phi-old_phi,trackLine,_max_sin_phi))
//      {
//        LogWarning("Rotate failed! Aborting for this seed...\n");
//        aborted = true;
//        break;
//      }
//      old_phi = new_phi;
    }
    if(aborted) continue;
    std::cout << "transported to beamline\n";
    // find nearest vertex
    double beamline_X = trackSeed.GetX();
    double beamline_Y = trackSeed.GetY();
*/
    double track_phi = atan2(y,x);
/*
    double beamline_x = beamline_X*cos(track_phi)-beamline_Y*sin(track_phi);
    double beamline_y = beamline_X*sin(track_phi)+beamline_Y*cos(track_phi);
    double beamline_z = trackSeed.GetZ();
    double min_dist = 1e9;
    int best_vtx = -1;
    for(int i=0;i<_vertex_x.size();++i)
    {
      double delta_x = beamline_x-_vertex_x[i];
      double delta_y = beamline_y-_vertex_y[i];
      double delta_z = beamline_z-_vertex_z[i];
      double dist = sqrt(delta_x*delta_x+delta_y*delta_y+delta_z*delta_z);
      if(dist<min_dist)
      {
        min_dist = dist;
        best_vtx = i;
      }
    }
    std::cout << "best vtx:\n";
    std::cout << "("<<_vertex_x[best_vtx]<<","<<_vertex_y[best_vtx]<<","<<_vertex_z[best_vtx]<<")\n";
    // Fit to vertex point
    double vertex_phi = atan2(_vertex_y[best_vtx],_vertex_x[best_vtx]);
    std::cout << "vertex_phi: " << vertex_phi << "\n";
    std::cout << "track_phi: " << track_phi << "\n";
//    double alpha = vertex_phi - track_phi;
    // Here's where we need to be careful about the vertex position.
    // Most clusters are at roughly the same spatial phi, with only a little rotation required between them.
    // This is no longer guaranteed for the vertex - its phi could be anywhere,
    // including on the opposite side of the origin.
    // If it ends up on the opposite side, then we need to transport to *negative* radius in order to get close to it.
    // We will simplify this condition to abs(alpha)>pi/2, which assumes that (innermost TPC cluster R) >> (vertex R).

    bool crosses_origin = false;
    if(alpha<-M_PI/4)
    {
      while(alpha<-M_PI/4) alpha += M_PI/2;
      crosses_origin = true;
    }
    if(alpha>M_PI/4)
    {
      while(alpha>M_PI/4) alpha -= M_PI/2;
      crosses_origin = true;
    }
    if(crosses_origin) std::cout << "bad\n";
    std::cout << "alpha: " << alpha << "\n";

    if(!trackSeed.Rotate(alpha,trackLine,_max_sin_phi))
    {
      LogWarning("Rotate failed! Aborting for this seed...\n");
      aborted = true;
      continue;
    }
    LogDebug("ALICE coordinates after rotation: (" << trackSeed.GetX() << ", " << trackSeed.GetY() << ", " << trackSeed.GetZ() << ")\n");
    std::cout << "rotated to vertex\n";

    double vertex_X = sqrt(_vertex_x[best_vtx]*_vertex_x[best_vtx]+_vertex_y[best_vtx]*_vertex_y[best_vtx]);
    if(crosses_origin) vertex_X = -vertex_X;
    if(!trackSeed.TransportToX(vertex_X,trackLine,_Bz,_max_sin_phi))
    {
      LogWarning("Transport failed! Aborting for this seed...\n");
      aborted = true;
      continue;
    }
    LogDebug("Track transported to (x,y,z) = (" << trackSeed.GetX()*cos(vertex_phi)-trackSeed.GetY()*sin(vertex_phi) << "," << trackSeed.GetX()*sin(vertex_phi)+trackSeed.GetY()*cos(vertex_phi) << "," << trackSeed.GetZ() << ")" << std::endl);
    LogDebug("Next cluster is at (x,y,z) = (" << _vertex_x[best_vtx] << "," << _vertex_y[best_vtx] << "," << _vertex_z[best_vtx] << ")" << std::endl);

    double vertex_Y = -_vertex_x[best_vtx]*sin(vertex_phi)+_vertex_y[best_vtx]*cos(vertex_phi);
    std::cout << "vertex Y: " << vertex_Y << "\n";
    std::cout << "transported to vertex\n";
    double vertex_Yerr = -_vertex_xerr[best_vtx]*sin(vertex_phi)+_vertex_yerr[best_vtx]*cos(vertex_phi);
    std::cout << "vertex Y err: " << vertex_Yerr << "\n";

    if(!trackSeed.Filter(vertex_Y,_vertex_z[best_vtx],vertex_Yerr*vertex_Yerr,_vertex_zerr[best_vtx]*_vertex_zerr[best_vtx],_max_sin_phi))
    {
      std::cout << "filter failed\n";
      if (Verbosity() >= 1)
        LogError("Kalman filter failed for seed " << nseeds << "! Aborting for this seed..." << std::endl);
      aborted = true;
      continue;
    }
*/
    double track_pt = fabs(1./trackSeed.GetQPt());
    #if defined(_DEBUG_)
    double track_pY = track_pt*trackSeed.GetSinPhi();
    double track_pX = sqrt(track_pt*track_pt-track_pY*track_pY);
    double track_px = track_pX*cos(track_phi)-track_pY*sin(track_phi);
    double track_py = track_pX*sin(track_phi)+track_pY*cos(track_phi);
    double track_pz = track_pt*trackSeed.GetDzDs();
    #endif
    double track_pterr = sqrt(trackSeed.GetErr2QPt())/(trackSeed.GetQPt()*trackSeed.GetQPt());
    // If Kalman filter doesn't do its job (happens often with short seeds), use the circle-fit estimate as the central value
    if(trackKeyChain.size()<10) track_pt = fabs(1./init_QPt);
    LogDebug("track pt = " << track_pt << " +- " << track_pterr << std::endl);
    LogDebug("track ALICE p = (" << track_pX << ", " << track_pY << ", " << track_pz << ")" << std::endl);
    LogDebug("track p = (" << track_px << ", " << track_py << ", " << track_pz << ")" << std::endl);

/*    
    if(cluster_ctr!=1 && !trackSeed.CheckNumericalQuality())
    {
      std::cout << "ERROR: Track seed failed numerical quality check before conversion to sPHENIX coordinates! Skipping this one.\n";
      aborted = true;
      continue;
    } 
*/    
    //    pt:z:dz:phi:dphi:c:dc
    // Fill NT with track parameters
    // double StartEta = -log(tan(atan(z0/sqrt(x0*x0+y0*y0))));
//    if(aborted) continue;
//    double track_pt = fabs( 1./(trackSeed.GetQPt()));
    if(checknan(track_pt,"pT",nseeds)) continue;
//    double track_pterr = sqrt(trackSeed.GetErr2QPt())/(trackSeed.GetQPt()*trackSeed.GetQPt());
    if(checknan(track_pterr,"pT err",nseeds)) continue;
    LogDebug("Track pterr = " << track_pterr << std::endl);
    double track_x = trackSeed.GetX()*cos(track_phi)-trackSeed.GetY()*sin(track_phi);
    double track_y = trackSeed.GetX()*sin(track_phi)+trackSeed.GetY()*cos(track_phi);
    double track_z = trackSeed.GetZ();
    if(checknan(track_z,"z",nseeds)) continue;
    double track_zerr = sqrt(trackSeed.GetErr2Z());
    if(checknan(track_zerr,"zerr",nseeds)) continue;
    auto lcluster = _cluster_map->findCluster(trackKeyChain.back());
    const auto& lclusterglob = globalPositions.at(trackKeyChain.back());
    const float lclusterrad = sqrt(lclusterglob(0)*lclusterglob(0) + lclusterglob(1)*lclusterglob(1));
    double last_cluster_phierr = lcluster->getRPhiError() / lclusterrad;
    // phi error assuming error in track radial coordinate is zero
    double track_phierr = sqrt(pow(last_cluster_phierr,2)+(pow(trackSeed.GetX(),2)*trackSeed.GetErr2Y()) / 
      pow(pow(trackSeed.GetX(),2)+pow(trackSeed.GetY(),2),2));
    if(checknan(track_phierr,"phierr",nseeds)) continue;
    LogDebug("Track phi = " << atan2(track_py,track_px) << std::endl);
    LogDebug("Track phierr = " << track_phierr << std::endl);
    double track_curvature = trackSeed.GetKappa(_Bzconst*get_Bz(track_x,track_y,track_z));
    if(checknan(track_curvature,"curvature",nseeds)) continue;
    double track_curverr = sqrt(trackSeed.GetErr2QPt())*_Bzconst*get_Bz(track_x,track_y,track_z);
    if(checknan(track_curverr,"curvature error",nseeds)) continue;
    TrackSeed_v1 track;

    for (unsigned int j = 0; j < trackKeyChain.size(); ++j)
    {
      track.insert_cluster_key(trackKeyChain.at(j));
    }
 
    double s = sin(track_phi);
    double c = cos(track_phi);
    double p = trackSeed.GetSinPhi();
    // TrkrCluster *cl = _cluster_map->findCluster(trackKeyChain.at(0));
    track.set_X0(trackSeed.GetX()*c-trackSeed.GetY()*s);//_vertex_x[best_vtx]);  //track.set_x(cl->getX());
    track.set_Y0(trackSeed.GetX()*s+trackSeed.GetY()*c);//_vertex_y[best_vtx]);  //track.set_y(cl->getY());
    track.set_Z0(trackSeed.GetZ());//_vertex_z[best_vtx]);  //track.set_z(cl->getZ());
    if(Verbosity()>0) std::cout << "x " << track.get_x() << "\n";
    if(Verbosity()>0) std::cout << "y " << track.get_y() << "\n";
    if(Verbosity()>0) std::cout << "z " << track.get_z() << "\n";
    if(checknan(p,"ALICE sinPhi",nseeds)) continue;
    double d = trackSeed.GetDzDs();
    if(checknan(d,"ALICE dz/ds",nseeds)) continue;
    double pY = track_pt*p;
    double pX = sqrt(track_pt*track_pt-pY*pY);
    
    double qpt = trackSeed.GetQPt();
    /// transform to q/R
    qpt *= 100/(0.3*get_Bz(track.get_X0(), track.get_Y0(), track.get_Z0()));
    track.set_qOverR(qpt);
    float tpx = pX*c-pY*s;
    float tpy = pX*s+pY*c;
    float tpz = track_pt * trackSeed.GetDzDs();
    float eta = atanh(tpz/sqrt(tpx*tpx+tpy*tpy+tpz*tpz));
    float theta = 2*atan(exp(-1*eta));
    track.set_slope(1./tan(theta));
    
    const double* cov = trackSeed.GetCov();
    bool cov_nan = false;
    for(int i=0;i<15;i++)
    {
      if(checknan(cov[i],"covariance element "+std::to_string(i),nseeds)) cov_nan = true;
    }
    if(cov_nan) continue;
 
    seeds_vector.push_back(track);
    ++nseeds;
  }
//  f->cd();
//  ntp->Write();
//  f->Close();
  if(Verbosity()>0) std::cout << "number of seeds: " << nseeds << "\n";
  return seeds_vector;
}

void ALICEKF::CircleFitByTaubin (const std::vector<std::pair<double,double>>& points, double &R, double &X0, double &Y0) const
/*  
      Circle fit to a given set of data points (in 2D)
      This is an algebraic fit, due to Taubin, based on the journal article
      G. Taubin, "Estimation Of Planar Curves, Surfaces And Nonplanar
                  Space Curves Defined By Implicit Equations, With 
                  Applications To Edge And Range Image Segmentation",
                  IEEE Trans. PAMI, Vol. 13, pages 1115-1138, (1991)
*/
{
  // Compute x- and y- sample means   
  double meanX = 0;
  double meanY = 0;
  double weight = 0;
  for( const auto& point:points )
  {
    meanX += point.first;
    meanY += point.second;
    weight++;
  }
  meanX /= weight;
  meanY /= weight;

  //     computing moments 
  double Mxy = 0;
  double Mxx = 0;
  double Myy = 0;
  double Mxz = 0;
  double Myz = 0;
  double Mzz = 0;
  for( const auto& point:points )
  {
    double Xi = point.first - meanX;   //  centered x-coordinates
    double Yi = point.second - meanY;   //  centered y-coordinates
    double Zi = Xi*Xi + Yi*Yi;
    
    Mxy += Xi*Yi;
    Mxx += Xi*Xi;
    Myy += Yi*Yi;
    Mxz += Xi*Zi;
    Myz += Yi*Zi;
    Mzz += Zi*Zi;
  }
  Mxx /= weight;
  Myy /= weight;
  Mxy /= weight;
  Mxz /= weight;
  Myz /= weight;
  Mzz /= weight;
  
  //  computing coefficients of the characteristic polynomial
  const double Mz = Mxx + Myy;
  const double Cov_xy = Mxx*Myy - Mxy*Mxy;
  const double Var_z = Mzz - Mz*Mz;
  const double A3 = 4*Mz;
  const double A2 = -3*Mz*Mz - Mzz;
  const double A1 = Var_z*Mz + 4*Cov_xy*Mz - Mxz*Mxz - Myz*Myz;
  const double A0 = Mxz*(Mxz*Myy - Myz*Mxy) + Myz*(Myz*Mxx - Mxz*Mxy) - Var_z*Cov_xy;
  const double A22 = A2 + A2;
  const double A33 = A3 + A3 + A3;
  
  //    finding the root of the characteristic polynomial
  //    using Newton's method starting at x=0  
  //    (it is guaranteed to converge to the right root)
  double x = 0;
  double y = A0;  
  static constexpr int IterMAX=99;
  for (int iter=0; iter<IterMAX; ++iter)  // usually, 4-6 iterations are enough
  {
    double Dy = A1 + x*(A22 + A33*x);
    double xnew = x - y/Dy;
    if ((xnew == x)||(!std::isfinite(xnew))) break;
    double ynew = A0 + xnew*(A1 + xnew*(A2 + xnew*A3));
    if (fabs(ynew)>=fabs(y))  break;
    x = xnew;  y = ynew;
  }
  
  //  computing parameters of the fitting circle
  const double DET = x*x - x*Mz + Cov_xy;
    
  const double Xcenter = (Mxz*(Myy - x) - Myz*Mxy)/DET/2;
  const double Ycenter = (Myz*(Mxx - x) - Mxz*Mxy)/DET/2;
  
  //  assembling the output
  
  X0 = Xcenter + meanX;
  Y0 = Ycenter + meanY;
  R = sqrt(Xcenter*Xcenter + Ycenter*Ycenter + Mz);
}

void  ALICEKF::line_fit(const std::vector<std::pair<double,double>>& points, double &a, double &b) const
{
  // copied from: https://www.bragitoff.com
  // we want to fit z vs radius
  
    double xsum=0,x2sum=0,ysum=0,xysum=0;                //variables for sums/sigma of xi,yi,xi^2,xiyi etc
    for( const auto& point:points )
    {
      double r = point.first;
      double z = point.second;

      xsum=xsum+r;                        //calculate sigma(xi)
      ysum=ysum+z;                        //calculate sigma(yi)
      x2sum=x2sum+square(r);                //calculate sigma(x^2i)
      xysum=xysum+r*z;                    //calculate sigma(xi*yi)
    }
   a=(points.size()*xysum-xsum*ysum)/(points.size()*x2sum-xsum*xsum);            //calculate slope
   b=(x2sum*ysum-xsum*xysum)/(x2sum*points.size()-xsum*xsum);            //calculate intercept

   if(Verbosity() > 10)
     {
       for (unsigned int i=0;i<points.size(); ++i)
	 {
	   double r = points[i].first;
	   double z_fit = a * r + b;                    //to calculate z(fitted) at given r points
	   std::cout << " r " << r << " z " << points[i].second << " z_fit " << z_fit << std::endl; 
	 } 
     }

    return;
}   

std::vector<double> ALICEKF::GetCircleClusterResiduals(const std::vector<std::pair<double,double>>& points, double R, double X0, double Y0) const
{
  std::vector<double> residues;
  std::transform( points.begin(), points.end(), std::back_inserter( residues ), [R,X0,Y0]( const std::pair<double,double>& point )
  {
    double x = point.first;
    double y = point.second;

    // The shortest distance of a point from a circle is along the radial; line from the circle center to the point
    return std::sqrt( square(x-X0) + square(y-Y0) )  -  R;  
  } );
  return residues;  
}

std::vector<double> ALICEKF::GetLineClusterResiduals(const std::vector<std::pair<double,double>>& points, double A, double B) const
{
  std::vector<double> residues;
  // calculate cluster residuals from the fitted circle
  std::transform( points.begin(), points.end(), std::back_inserter( residues ), [A,B]( const std::pair<double,double>& point )
  {
    double r = point.first;
    double z = point.second;
    
    // The shortest distance of a point from a circle is along the radial; line from the circle center to the point
    
    double a = -A;
    double b = 1.0;
    double c = -B;
    return std::abs(a*r+b*z+c)/sqrt(square(a)+square(b));
  });
  return residues;  
}
