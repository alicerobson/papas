/**
 * @file Clic.cc
 * @brief Implementation of the Clic detector
 */
#include "papas/detectors/ClicHcal.h"

#include "papas/datatypes/Cluster.h"
#include "papas/datatypes/Particle.h"
#include "papas/datatypes/Path.h"
#include "papas/utility/TRandom.h"

namespace papas {

ClicHCAL::ClicHCAL(double innerRadius, double innerZ, double outerRadius, double outerZ, double clusterSize,
                   std::vector<double> eresBarrel, double x0, double lambdaI, double eResponse, double etaAcceptance)
    : Calorimeter(Layer::kHcal,
                  VolumeCylinder(Layer::kHcal, outerRadius, outerZ, innerRadius, innerZ),
                  Material("Clic_HCAL", x0, lambdaI)),
      m_clusterSize(clusterSize),
      m_etaAcceptance(etaAcceptance),
      m_eresBarrel(eresBarrel),
      m_eResponse(eResponse) {}

/**
 Cluster_size as a function of the type of particle
 @param ptc particle
 @return size of resulting cluster
 */
double ClicHCAL::clusterSize(const Particle& ptc) const {
  (void)ptc;  // suppress warning messages for unused parameters;
  return m_clusterSize;
}

/* Decides whether a cluster will be seen by a detector
 @param cluster the cluster to be analysed
 @return true is cluster is detected, false it if is too small to be seen
 */
bool ClicHCAL::acceptance(const Cluster& cluster) const {
  auto energy = cluster.energy();
  auto eta = fabs(cluster.position().Eta());
  if (eta < m_etaAcceptance)  // TODO: check this value
    return (energy > 1.);
  else
    return false;
}

double ClicHCAL::energyResolution(double energy, double eta) const {
  // int part = kBarrel;
  double stoch = m_eresBarrel[0] / sqrt(energy);
  double noise = m_eresBarrel[1] / energy;
  double constant = m_eresBarrel[2];
  return sqrt(pow(stoch, 2) + pow(noise, 2) + pow(constant, 2));
}

double ClicHCAL::energyResponse(double energy, double eta) const { return m_eResponse; }

}  // end namespace papas
