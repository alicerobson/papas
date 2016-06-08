//
//  Created by Alice Robson on 09/11/15.
//
//
#include <iostream> //TODOAJR remove
#include "Simulator.h"
#include "CMS.h"
#include "Id.h"
#include "Definitions.h"
#include "Cluster.h"
#include "pTrack.h"
#include "PFParticle.h"
#include "ParticlePData.h"
#include "Path.h"
#include "random.h"
#include "Log.h"

namespace papas {

//using papas::Layer;

Simulator::Simulator(const Detector& d) :
    m_detector(d),
    m_propStraight(),
    m_propHelix(d.field()->getMagnitude())
{
  //TODO think about sizing
  int isize=1000;
  m_ecalClusters.reserve(isize);
  m_hcalClusters.reserve(isize);
  m_smearedEcalClusters.reserve(isize);
  m_smearedHcalClusters.reserve(isize);
  m_particles.reserve(isize);
  m_tracks.reserve(isize);
  m_smearedTracks.reserve(isize);
  m_nodes.reserve(isize);
}

void  Simulator::simulatePhoton(PFParticle& ptc)
{
  PDebug::write("Simulating Photon");
  auto ecal_sp = m_detector.ecal(); //ECAL detector element
  
  //find where the photon meets the Ecal inner cylinder
  // make and smear the cluster
  propagate(ptc, ecal_sp->volumeCylinder().inner());
  Id::Type ecalId = addEcalCluster(ptc);
  addSmearedCluster(ecalId);
}

void Simulator::simulateHadron(PFParticle& ptc) {
  PDebug::write("Simulating Hadron");
  auto ecal_sp = m_detector.ecal();
  auto hcal_sp = m_detector.hcal();
  auto field_sp = m_detector.field();
  double fracEcal = 0.;  // TODO ask Colin

  // make a track if it is charged
  if (ptc.charge()!=0) {
    const Track& track = addTrack(ptc);
    addSmearedTrack(track);
  }
  // find where it meets the inner Ecal cyclinder
  propagate(ptc, ecal_sp->volumeCylinder().inner());
  double pathLength = ecal_sp->material().pathLength(ptc.isElectroMagnetic());

  if (pathLength < std::numeric_limits<double>::max()) {

    /// ecal path length can be infinite in case the ecal
    /// has lambda_I = 0 (fully transparent to hadrons)
    std::shared_ptr<Path> path = ptc.path();
    double timeEcalInner = path->timeAtZ(path->namedPoint(papas::Position::kEcalIn).Z());
    double deltaT = path->deltaT(pathLength);
    double timeDecay = timeEcalInner + deltaT;
    TVector3 pointDecay = path->pointAtTime(timeDecay);
    path->addPoint(papas::Position::kEcalDecay, pointDecay);
    if (ecal_sp->volumeCylinder().Contains(pointDecay)) {
      fracEcal = randomgen::RandUniform(0., 0.7).next();
      Id::Type ecalId = addEcalCluster(ptc, fracEcal);
      // For now, using the hcal resolution and acceptance for hadronic cluster
      // in the Ecal. That's not a bug!
      addSmearedCluster(ecalId, papas::Layer::kHcal, papas::Layer::kEcal );
    }
  }

  // now find where it reaches into HCAL
  propagate(ptc, hcal_sp->volumeCylinder().inner());
  Id::Type hcalId = addHcalCluster(ptc, 1 - fracEcal);
  addSmearedCluster(hcalId);
  
  
}

void Simulator::simulateNeutrino(PFParticle& ptc) {
  PDebug::write("Simulating Neutrino \n");
  (void)ptc;
}

void  Simulator::propagate(PFParticle& ptc, const SurfaceCylinder& cylinder) {
  bool isNeutral = fabs(ptc.charge())<0.5; //TODO ask colin why not zero
  if (isNeutral)
    m_propStraight.propagateOne(ptc, cylinder);
  else
    m_propHelix.propagateOne(ptc, cylinder);
}

const Cluster& Simulator::cluster(Id::Type clusterId) const {
  if (Id::isEcal(clusterId))
    return m_ecalClusters.at(clusterId);
  else if (Id::isHcal(clusterId))
    return m_hcalClusters.at(clusterId);
  //TODO or throw error
}

PFParticle& Simulator::addParticle(int pdgid, TLorentzVector tlv, TVector3 vertex)
{
  
  double field = m_detector.field()->getMagnitude();
  Id::Type uniqueid = Id::makeParticleId();
  m_particles.emplace(uniqueid, PFParticle{uniqueid, pdgid, tlv, vertex, field});
  addNode(uniqueid); //add node to history graph
                     //PDebug::write()<<  ;
                     //PDebug::write("Made Simulation Particle {}", m_particles[uniqueid].info());
  return m_particles[uniqueid];
}
  
  PFParticle& Simulator::addGunParticle(int pdgid, double thetamin,double thetamax, double ptmin, double ptmax, TVector3 vertex)
  {
    double theta = randomgen::RandUniform(thetamin, thetamax).next();
    double phi = randomgen::RandUniform(-M_PI, M_PI).next();
    double energy = randomgen::RandUniform(ptmin, ptmax).next();
  
    double mass = ParticlePData::particleMass(pdgid);
    double costheta = cos(M_PI/2-theta);
    double sintheta = sin(M_PI/2-theta);
    double cosphi = cos(phi);
    double sinphi = sin(phi);
    double momentum=energy/sintheta;
    energy = sqrt(pow(momentum, 2) + pow(mass, 2));

    TLorentzVector p4(momentum * sintheta * cosphi,
                      momentum * sintheta * sinphi,
                      momentum * costheta,
                      energy);
    return addParticle(pdgid, p4, vertex);
  }

  


PFParticle& Simulator::addParticle(int pdgid, double theta, double phi, double energy, TVector3 vertex)
{
  double mass = ParticlePData::particleMass(pdgid);
  double momentum = sqrt(pow(energy, 2) - pow(mass, 2));
  double costheta = cos(theta);
  double sintheta = sin(theta);
  double cosphi = cos(phi);
  double sinphi = sin(phi);
  TLorentzVector p4(momentum * sintheta * cosphi,
                    momentum * sintheta * sinphi,
                    momentum * costheta,
                    energy);
  return addParticle(pdgid, p4, vertex);
}



Id::Type Simulator::addEcalCluster(PFParticle& ptc, double fraction, double csize)
{
  Cluster cluster = makeCluster(ptc,  papas::Layer::kEcal, fraction, csize);
  m_ecalClusters.emplace(cluster.id(), std::move(cluster));
  return cluster.id();
}

Id::Type Simulator::addHcalCluster(PFParticle& ptc, double fraction, double csize)
{
  Cluster cluster = makeCluster(ptc,  papas::Layer::kHcal, fraction, csize);
  m_hcalClusters.emplace(cluster.id(), std::move(cluster));
  return cluster.id();
}


Cluster Simulator::makeCluster(PFParticle& ptc, papas::Layer layer, double fraction, double csize)
{
  double energy = ptc.p4().E() * fraction;
  papas::Position clayer = m_detector.calorimeter(layer)->volumeCylinder().innerLayer();
  TVector3 pos = ptc.pathPosition(clayer);
  if (csize == -1.) { //is value not provided
    csize = m_detector.calorimeter(layer)->clusterSize(ptc);
  }
  
  Cluster cluster{energy, pos, csize, Id::itemType(layer)} ;
  addNode(cluster.id(), ptc.id());
  PDebug::write("Made {}", cluster);
  return cluster;
}

Id::Type Simulator::addSmearedCluster(Id::Type parentClusterId, papas::Layer detlayer,  papas::Layer acceptlayer, bool accept)

{
  Cluster smeared=makeSmearedCluster(parentClusterId,  detlayer);
  PDebug::write("Made Smeared{}", smeared);
  
  //Determine if this smeared cluster will be detected and, if so, add it into the
  //smeared cluster collections (Ecal or Hcal)
  if (detlayer == papas::Layer::kNone)
    detlayer = Id::layer(parentClusterId);
  if (acceptlayer == papas::Layer::kNone)
    acceptlayer = detlayer;
  
  if (m_detector.calorimeter(acceptlayer)->acceptance(smeared)|| accept) {
    addNode(smeared.id(), parentClusterId);
    if(Id::layer(parentClusterId) ==papas::Layer::kEcal) {
      m_smearedEcalClusters.emplace(smeared.id(), std::move(smeared));
    }
    else
      m_smearedHcalClusters.emplace(smeared.id(), std::move(smeared));
    
    return smeared.id();
  }
  else {
    PDebug::write("Rejected Smeared{}", smeared);
    return parentClusterId;
  }
}


Cluster Simulator::makeSmearedCluster(Id::Type parentClusterId, papas::Layer detlayer)
{
  
  //detlayer will be used to choose which detector layer is used for energy resolution etc.
  // NB It is not always the same layer as the new smeared cluster
  if(detlayer==papas::Layer::kNone)
    detlayer = Id::layer(parentClusterId);
  std::shared_ptr<const Calorimeter> sp_calorimeter = m_detector.calorimeter(detlayer);
  
  //now do the smearing
  const Cluster& parent = cluster(parentClusterId); //find the cluster in the Clusters collections
  double energyresolution = sp_calorimeter->energyResolution(parent.energy(), parent.eta());
  double response = sp_calorimeter->energyResponse(parent.energy(), parent.eta());
  double energy = parent.energy() * randomgen::RandNormal(response, energyresolution).next();
 
  //if (Id::pretty(parentClusterId).compare(0,5, "h3498")==0)
  //  std::cout <<"e106";
  
  //create smeared cluster
  Cluster cluster = Cluster{ energy, parent.position(), parent.size(), Id::itemType(parentClusterId)};
  return cluster;
}

const Track& Simulator::addTrack(PFParticle& ptc)
{
  Track track = Track{ ptc.p3(), ptc.charge(), ptc.path()};
  m_tracks.emplace(track.id(), track);
  addNode(track.id(), ptc.id());
  PDebug::write("Made {}", track);
  return m_tracks.at(track.id()); }


Id::Type Simulator::addSmearedTrack( const Track& track, bool accept) {
  
  double ptResolution = m_detector.tracker()->ptResolution(track);
  double scale_factor = randomgen::RandNormal(1, ptResolution).next();
  
  Track smeared = Track{ track.p3() * scale_factor, track.charge(), track.path()};
  PDebug::write("Made Smeared{}", smeared);
  
  //decide whether the smearedTrack is detected
  if (m_detector.tracker()->acceptance(smeared) || accept ) {
    addNode(smeared.id(), track.id());
    m_smearedTracks.emplace(smeared.id(), std::move(smeared));
  }
  else {
      PDebug::write("Rejected Smeared{}", smeared);
  }
  return smeared.id();
}

void Simulator::addNode(Id::Type newid, const Id::Type parentid)
{
  //add the new node into the set of all nodes
  m_nodes[newid] = {newid};
  if (parentid) {
    PFNode& parent = m_nodes[parentid];
    PFNode& child = m_nodes[newid];
    parent.addChild(child);
  }
}

std::shared_ptr<const DetectorElement> Simulator::elem(papas::Layer layer) const {
  return m_detector.element(layer);
}

void Simulator::testing()
{
  DAG::BFSVisitor<PFNode> bfs;
  for (auto p : m_particles) {
    std::cout<< "Connected to "<<p.first<< std::endl;
    auto res =bfs.traverseUndirected(m_nodes[p.first]);
    for (auto r : res)
      std::cout<< "  "<<r->value()<<": "<<Id::itemType(r->value()) <<std::endl;
  }
}
/*
Ids Simulator::linkedEcalSmearedClusterIds(long nodeid) const {
  return getMatchingIds(nodeid,
                        Id::DataType::kCluster,
                        papas::Layer::kEcal,
                        papas::SubType::SMEARED,
                        papas::enumSource::SIMULATION);
}

Ids  Simulator::linkedRawTrackIds(long nodeid) const {
  return getMatchingIds(nodeid,
                        Id::DataType::kTrack,
                        papas::Layer::kNone,
                        papas::SubType::RAW,
                        papas::enumSource::SIMULATION);
}

Ids Simulator::linkedSmearedTrackIds(long nodeid) const {
  return getMatchingIds(nodeid,
                        Id::DataType::kTrack,
                        papas::Layer::kNone,
                        papas::SubType::SMEARED,
                        papas::enumSource::SIMULATION);
}

Ids Simulator::linkedParticleIds(long nodeid) const {
  return getMatchingIds(nodeid,
                        Id::DataType::kParticle,
                        papas::Layer::kNone,
                        papas::SubType::RAW,
                        papas::enumSource::SIMULATION);
}

Ids Simulator::parentParticleIds(long nodeid) const {
  return getMatchingParentIds(nodeid,
                              Id::DataType::kParticle,
                              papas::Layer::kNone,
                              papas::SubType::RAW,
                              papas::enumSource::SIMULATION);
}*/

Ids Simulator::linkedIds(Id::Type nodeid) const {
  DAG::BFSVisitor<PFNode> bfs;
  Ids foundids;
  foundids.reserve(1000); //TODO how
  auto res = bfs.traverseUndirected(m_nodes.at(nodeid));
  for (auto r : res)
  {
    foundids.push_back(r->value());
  }
  return foundids;
}

  /*
Ids Simulator::getMatchingIds(long nodeid, Id::DataType datatype, papas::Layer layer, papas::SubType type, papas::enumSource source) const
{
  DAG::BFSVisitor<PFNode> bfs;
  Ids foundids;
  //foundids.reserve(1000); //TODO set sizes sensible.... how
  auto res =bfs.traverseUndirected(m_nodes.at(nodeid));
  for (auto r : res)
  {
    long id=r->value();
    if(Id::isUniqueIdMatch(id, datatype, layer, type, source)) {
      foundids.push_back(id);
    }
  }
  return foundids;
}

Ids Simulator::getMatchingParentIds(long nodeid, Id::DataType datatype, papas::Layer layer, papas::SubType type,papas::enumSource source) const
{
  DAG::BFSVisitor<PFNode> bfs;
  Ids foundids;
  //foundids.reserve(1000); //TODO set sizes sensible.... how
  auto res =bfs.traverseParents(m_nodes.at(nodeid));
  for (auto r : res)
  {
    long id=r->value();
    if(Id::isUniqueIdMatch(id, datatype, layer, type, source)) {
      foundids.push_back(id);
    }
  }
  return foundids;
}*/


} // end namespace papas




