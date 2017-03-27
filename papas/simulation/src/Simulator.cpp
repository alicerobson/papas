#include "papas/datatypes/Cluster.h"
#include "papas/datatypes/Definitions.h"
#include "papas/datatypes/IdCoder.h"
#include "papas/datatypes/Particle.h"
#include "papas/datatypes/ParticlePData.h"
#include "papas/datatypes/Path.h"
#include "papas/datatypes/Track.h"
#include "papas/simulation/Simulator.h"
#include "papas/utility/PDebug.h"
#include "papas/utility/TRandom.h"
#include "papas/utility/Log.h"
#include "papas/datatypes/Helix.h"
class Detector;

namespace papas {

Simulator::Simulator(const Event& papasevent, const Detector& detector,
                     Clusters& ecalClusters, Clusters& hcalClusters, Clusters& smearedEcalClusters,
                     Clusters& smearedHcalClusters, Tracks& tracks, Tracks& smearedTracks, Particles& particles,
                     Nodes& history)
    : m_event(papasevent),
      m_detector(detector),
      m_ecalClusters(ecalClusters),
      m_hcalClusters(hcalClusters),
      m_smearedEcalClusters(smearedEcalClusters),
      m_smearedHcalClusters(smearedHcalClusters),
      m_tracks(tracks),
      m_smearedTracks(smearedTracks),
      m_particles(particles),
      m_history(history)
  
  {
    m_propHelix  = std::make_shared<HelixPropagator>();
    m_propStraight  = std::make_shared<StraightLinePropagator>();
    Ids ids;
    for (auto& p : particles)
      ids.push_back(p.first);
#if WITHSORT
    ids.sort(std::greater<Identifier>());
#endif
    for (auto& id :ids) {
    simulateParticle(particles[id]);
  }
}

void Simulator::simulateParticle(Particle& ptc) {
  int pdgid = ptc.pdgId();
  if (ptc.charge() && ptc.pt() < 0.2 && abs(pdgid) >= 100) {
    // to avoid numerical problems in propagation
    return;
  }
  PDebug::write("Simulating {}", ptc);
  //Particle& storedParticle = makeAndStoreParticle(pdgid, ptc.charge(), ptc.p4(), ptc.startVertex());
  if (pdgid == 22) {
    simulatePhoton(ptc);
  } else if (abs(pdgid) == 11) {
    simulateElectron(ptc);
  } else if (abs(pdgid) == 13) {
    simulateMuon(ptc);
  } else if ((abs(pdgid) == 12) | (abs(pdgid) == 14) | (abs(pdgid) == 16)) {
    simulateNeutrino(ptc);
  } else if (abs(pdgid) >= 100) {
    simulateHadron(ptc);
  }
}

void Simulator::simulatePhoton(Particle& ptc) {
  PDebug::write("Simulating Photon");
  // find where the photon meets the Ecal inner cylinder
  // make and smear the cluster
  propagator(ptc.charge())->propagateOne(ptc, m_detector.ecal()->volumeCylinder().inner(), m_detector.field()->getMagnitude());
  auto cluster = makeAndStoreEcalCluster(ptc, 1, -1, 't');
  auto smeared = smearCluster(cluster, papas::Layer::kEcal);
  if (acceptSmearedCluster(smeared)) {
    storeSmearedEcalCluster(std::move(smeared), cluster.id());
  }
}

void Simulator::simulateHadron(Particle& ptc) {
  PDebug::write("Simulating Hadron");
  auto ecal_sp = m_detector.ecal();
  auto hcal_sp = m_detector.hcal();
  auto field_sp = m_detector.field();
  double fracEcal = 0.;  // TODO ask Colin

  propagator(ptc.charge())
  ->propagateOne(ptc, ecal_sp->volumeCylinder().inner(), m_detector.field()->getMagnitude());
  
  // make a track if it is charged
  if (ptc.charge() != 0) {
    auto track = makeAndStoreTrack(ptc);
    auto resolution = m_detector.tracker()->ptResolution(track);
    auto smeared = smearTrack(track, resolution);
    if (acceptSmearedTrack(smeared)) {
      storeSmearedTrack(std::move(smeared), track.id());
    }
  }
  // find where it meets the inner Ecal cyclinder
  if (ptc.path()->hasNamedPoint(papas::Position::kEcalIn)) {
    double pathLength = ecal_sp->material().pathLength(ptc.isElectroMagnetic());
    if (pathLength < std::numeric_limits<double>::max()) {
      /// ecal path length can be infinite in case the ecal
      /// has lambda_I = 0 (fully transparent to hadrons)
      auto path = std::static_pointer_cast<Helix>(ptc.path());
      double timeEcalInner = path->timeAtZ(path->namedPoint(papas::Position::kEcalIn).Z());
      double deltaT = path->deltaT(pathLength);
      double timeDecay = timeEcalInner + deltaT;
      TVector3 pointDecay = path->pointAtTime(timeDecay);
      path->addPoint(papas::Position::kEcalDecay, pointDecay);
      if (ecal_sp->volumeCylinder().contains(pointDecay)) {
        fracEcal = rootrandom::Random::uniform(0., 0.7);
        auto cluster = makeAndStoreEcalCluster(ptc, fracEcal, -1, 't');
        // For now, using the hcal resolution and acceptance for hadronic cluster
        // in the Ecal. That's not a bug!
        auto smeared = smearCluster(cluster, papas::Layer::kHcal);
        if (acceptSmearedCluster(smeared, papas::Layer::kEcal)) {
          storeSmearedEcalCluster(std::move(smeared), cluster.id());
        }
      }
    }
  }  // now find where it reaches into HCAL
  propagator(ptc.charge())->propagateOne(ptc, hcal_sp->volumeCylinder().inner(), m_detector.field()->getMagnitude());
  auto hcalCluster = makeAndStoreHcalCluster(ptc, 1 - fracEcal, -1, 't');
  auto hcalSmeared = smearCluster(hcalCluster, papas::Layer::kHcal);
  if (acceptSmearedCluster(hcalSmeared)) {
    storeSmearedHcalCluster(std::move(hcalSmeared), hcalCluster.id());
  }
}

void Simulator::simulateNeutrino(Particle& ptc) {
  PDebug::write("Simulating Neutrino \n");
  propagator(ptc.charge())->propagate(ptc, m_detector);
}

void Simulator::simulateElectron(Particle& ptc) {
  /*Simulate an electron corresponding to gen particle ptc.

   Uses the methods detector.electronEnergyResolution
   and detector.electronAcceptance to smear the electron track.
   Later on, the particle flow algorithm will use the tracks
   coming from an electron to reconstruct electrons.

   This method does not simulate an electron energy deposit in the ECAL.*/
  PDebug::write("Simulating Electron");
  propagator(ptc.charge())->propagateOne(ptc, m_detector.ecal()->volumeCylinder().inner(),m_detector.field()->getMagnitude());
  auto track = makeAndStoreTrack(ptc);
  auto eres = m_detector.electronEnergyResolution(ptc);
  auto smeared = smearTrack(track, eres);  // smear it
  if (acceptElectronSmearedTrack(smeared)) {
    storeSmearedTrack(std::move(smeared), track.id());
  }
  
}

void Simulator::simulateMuon(Particle& ptc) {
  /*Simulate a muon corresponding to gen particle ptc

  Uses the methods detector.muon_energy_resolution
  and detector.muon_acceptance to smear the muon track.
  Later on, the particle flow algorithm will use the tracks
  coming from a muon to reconstruct muons.

  This method does not simulate energy deposits in the calorimeters
  */
  PDebug::write("Simulating Muon");
  propagator(ptc.charge())->propagate(ptc, m_detector);
  auto ptres = m_detector.muonPtResolution(ptc);
  auto track = makeAndStoreTrack(ptc);
  auto smeared = smearTrack(track, ptres);
  if (acceptMuonSmearedTrack(smeared)) {
    storeSmearedTrack(std::move(smeared), track.id());
  }
}

std::shared_ptr<Propagator> Simulator::propagator(double charge) {
  if (fabs(charge) < 0.5)
    return m_propStraight;
  else
    return m_propHelix;
}
  
const Cluster& Simulator::cluster(Identifier clusterId) const {
  if (IdCoder::isEcal(clusterId))
    return m_ecalClusters.at(clusterId);
  else if (IdCoder::isHcal(clusterId))
    return m_hcalClusters.at(clusterId);
  throw std::out_of_range("Cluster not found");
}

  //TODO particlesissue
Particle& Simulator::makeAndStoreParticle(int pdgid, double charge, const TLorentzVector& tlv,
                                              const TVector3& vertex) {
  double field = m_detector.field()->getMagnitude();
  Particle simParticle = Particle(pdgid, charge, tlv, m_particles.size(), 's', vertex);
  auto id = simParticle.id();
  
  m_particles.emplace(id, std::move(simParticle));
  addNode(id, 0);  // add node to history graph
  return m_particles[id];
}

Particle& Simulator::makeAndStoreParticle(int pdgid, double charge, double theta, double phi, double energy,
                                              const TVector3& vertex) {
  double mass = ParticlePData::particleMass(pdgid);
  double momentum = sqrt(pow(energy, 2) - pow(mass, 2));
  double costheta = cos(theta);
  double sintheta = sin(theta);
  double cosphi = cos(phi);
  double sinphi = sin(phi);
  TLorentzVector p4(momentum * sintheta * cosphi, momentum * sintheta * sinphi, momentum * costheta, energy);
  return makeAndStoreParticle(pdgid, charge, p4, vertex);
}

Particle& Simulator::addGunParticle(int pdgid, double charge, double thetamin, double thetamax, double ptmin,
                                      double ptmax, const TVector3& vertex) {
  double theta = rootrandom::Random::uniform(thetamin, thetamax);
  double phi = rootrandom::Random::uniform(-M_PI, M_PI);
  double energy = rootrandom::Random::uniform(ptmin, ptmax);
  double mass = ParticlePData::particleMass(pdgid);
  double costheta = cos(M_PI / 2 - theta);
  double sintheta = sin(M_PI / 2 - theta);
  double cosphi = cos(phi);
  double sinphi = sin(phi);
  double momentum = energy / sintheta;
  energy = sqrt(pow(momentum, 2) + pow(mass, 2));

  TLorentzVector p4(momentum * sintheta * cosphi, momentum * sintheta * sinphi, momentum * costheta, energy);
  return makeAndStoreParticle(pdgid, charge, p4, vertex);
}

Cluster Simulator::makeAndStoreEcalCluster(const Particle& ptc, double fraction, double csize, char subtype) {
  double energy = ptc.p4().E() * fraction;
  if (ptc.path()->hasNamedPoint(papas::Position::kEcalIn)) {
    TVector3 pos = ptc.path()->namedPoint(papas::Position::kEcalIn);

    if (csize == -1.) {  // ie value not provided
      csize = m_detector.calorimeter(papas::Layer::kEcal)->clusterSize(ptc);
    }
    Cluster cluster(energy, pos, csize, m_ecalClusters.size(), IdCoder::kEcalCluster, subtype);
    Identifier id = cluster.id();
    addNode(id, ptc.id());
    PDebug::write("Made {}", cluster);
    m_ecalClusters.emplace(id, std::move(cluster));
    return m_ecalClusters[id];
  } else {
    Log::warn("SimulationError : cannot make cluster for particle:{} with vertex rho {}, z {}. Cannot be extrapolated "
              "to EcalIn cylinder \n",
              ptc, ptc.startVertex().Perp(), ptc.startVertex().Z());
    std::string message = "Particle not extrapolated to the detector, so cannot make a cluster there. No worries for "
                          "now, problem will be solved :-)";
    throw message;
  }
}

Cluster Simulator::makeAndStoreHcalCluster(const Particle& ptc, double fraction, double csize, char subtype) {
  double energy = ptc.p4().E() * fraction;
  if (ptc.path()->hasNamedPoint(papas::Position::kHcalIn)) {
    TVector3 pos = ptc.path()->namedPoint(papas::Position::kHcalIn);
    if (csize == -1.) {  // ie value not provided
      csize = m_detector.calorimeter(papas::Layer::kHcal)->clusterSize(ptc);
    }
    Cluster cluster(energy, pos, csize, m_hcalClusters.size(), IdCoder::kHcalCluster, subtype);
    Identifier id = cluster.id();
    addNode(id, ptc.id());
    PDebug::write("Made {}", cluster);
    m_hcalClusters.emplace(id, std::move(cluster));
    return m_hcalClusters[id];
  } else {
    Log::warn("SimulationError : cannot make cluster for particle:{} with vertex rho {}, z {}. Cannot be extrapolated "
              "to HcalIn cylinder \n",
              ptc, ptc.startVertex().Perp(), ptc.startVertex().Z());
    std::string message = "Particle not extrapolated to the detector, so cannot make a cluster there. No worries for "
                          "now, problem will be solved :-)";
    throw message;
  }
}

Cluster Simulator::smearCluster(const Cluster& parent, papas::Layer detectorLayer) {
  // detectorLayer will be used to choose which detector layer is used for energy resolution etc.
  // NB It is not always the same layer as the new smeared cluster
  // The smeared cluster will have the same layer as the parent cluster
  if (detectorLayer == papas::Layer::kNone)
    detectorLayer = IdCoder::layer(parent.id());  // default to same layer as cluster
  std::shared_ptr<const Calorimeter> sp_calorimeter = m_detector.calorimeter(detectorLayer);
  double energyresolution = sp_calorimeter->energyResolution(parent.energy(), parent.eta());
  double response = sp_calorimeter->energyResponse(parent.energy(), parent.eta());
  double energy = parent.energy() * rootrandom::Random::gauss(response, energyresolution);
  unsigned int counter;
  if (IdCoder::layer(parent.id()) == Layer::kEcal)
    counter = m_smearedEcalClusters.size();
  else
    counter = m_smearedHcalClusters.size();
  // energy = fmax(0., energy);  // energy always positive
  Cluster cluster(energy, parent.position(), parent.size(), counter, IdCoder::type(parent.id()), 's');
  PDebug::write("Made Smeared{}", cluster);
  return cluster;
}

bool Simulator::acceptSmearedCluster(const Cluster& smearedCluster, papas::Layer acceptLayer, bool accept) const {
  // Determine if this smeared cluster will be detected
  if (acceptLayer == papas::Layer::kNone) acceptLayer = IdCoder::layer(smearedCluster.id());
  if (m_detector.calorimeter(acceptLayer)->acceptance(smearedCluster) || accept) {
    return true;
  } else {
    PDebug::write("Rejected Smeared{}", smearedCluster);
    return false;
  }
}

const Cluster& Simulator::storeSmearedEcalCluster(Cluster&& smearedCluster, Identifier parentId) {
  auto id = smearedCluster.id();
  addNode(id, parentId);
  m_smearedEcalClusters.emplace(id, std::move(smearedCluster));
  return m_smearedEcalClusters[id];
}

const Cluster& Simulator::storeSmearedHcalCluster(Cluster&& smearedCluster, Identifier parentId) {
  auto id = smearedCluster.id();
  addNode(id, parentId);
  m_smearedHcalClusters.emplace(id, std::move(smearedCluster));
  return m_smearedHcalClusters[id];
}


const Track& Simulator::makeAndStoreTrack(const Particle& ptc) {
  Track track(ptc.p3(), ptc.charge(), ptc.path(), m_tracks.size(), 't');
  Identifier id = track.id();
  PDebug::write("Made {}", track);
  m_tracks.emplace(id, std::move(track));
  addNode(id, ptc.id());
  return m_tracks.at(id);
}

void Simulator::storeSmearedTrack(Track&& track, Identifier parentid) {
  Identifier id = track.id();
  m_smearedTracks.emplace(id, std::move(track));
  addNode(id, parentid);
}

Track Simulator::smearTrack(const Track& track, double resolution) const {
  double scale_factor = rootrandom::Random::gauss(1, resolution);
  Track smeared(track.p3() * scale_factor, track.charge(), track.path(), m_smearedTracks.size(), 's');
  PDebug::write("Made Smeared{}", smeared);
  return smeared;
}

bool Simulator::acceptSmearedTrack(const Track& smearedTrack, bool accept) const {
  // decide whether the smearedTrack is detected
  if (m_detector.tracker()->acceptance(smearedTrack) || accept) {
    return true;
  } else {
    PDebug::write("Rejected Smeared{}", smearedTrack);
    return false;
  }
}

bool Simulator::acceptElectronSmearedTrack(const Track& smearedTrack, bool accept) const {
  // decide whether the electron smearedTrack is detected
  if (m_detector.electronAcceptance(smearedTrack) || accept) {
    return true;
  } else {
    PDebug::write("Rejected Smeared{}", smearedTrack);
    return false;
  }
}

bool Simulator::acceptMuonSmearedTrack(const Track& smearedTrack, bool accept) const {
  // decide whether the electron smearedTrack is detected
  if (m_detector.muonAcceptance(smearedTrack) || accept) {
    return true;
  } else {
    PDebug::write("Rejected Smeared{}", smearedTrack);
    return false;
  }
}

void Simulator::addNode(Identifier newid, const Identifier parentid) {
  // add the new node into the set of all nodes
  PFNode node{newid};
  m_history.emplace(newid, std::move(node));
  if (parentid) {
    PFNode& parent = m_history[parentid];
    PFNode& child = m_history[newid];
    parent.addChild(child);
  }
}

void Simulator::clear() {
  m_ecalClusters.clear();
  m_hcalClusters.clear();
  m_smearedEcalClusters.clear();
  m_smearedHcalClusters.clear();
  m_tracks.clear();
  m_smearedTracks.clear();
  m_particles.clear();
}

std::shared_ptr<const DetectorElement> Simulator::elem(papas::Layer layer) const { return m_detector.element(layer); }

}  // end namespace papas