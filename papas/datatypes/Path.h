//
//  Created by Alice Robson on 02/12/15.
//
//
#ifndef path_h
#define path_h

#include "TLorentzVector.h"
#include "TVector3.h"
#include "papas/datatypes/Definitions.h"
#include <map>
#include <memory>
#include <vector>

namespace papas {

/// @brief Path followed by a particle in 3D space.
///
/// Assumes constant speed magnitude both along the z axis and in the transverse plane.
/// Path base class is essentially a straightline but can be inherited from to make Helix etc
///
class Path {
public:
  typedef std::map<papas::Position, TVector3> Points; ///< Map of path points indexed by position
  typedef std::shared_ptr<Path> Ptr;  ///< shared pointer to allow for straightline or helix

  Path(const TLorentzVector& p4, const TVector3& origin, double field = 0.);
  Path();
  virtual ~Path() = default;

  void addPoint(papas::Position layer, const TVector3& vec) { m_points[layer] = vec; }  ///< add new point to path
  double timeAtZ(double z) const;  ///< Time when particle gets to point z on z axis
  double deltaT(double path_length) const; ///< Time needed to follow a given path length
  double vZ() const; ///< Speed magnitude in Z direction
  double vPerp() const;  ///< Speed magnitude in the transverse plane
  double speed() const { return m_speed; }  ///< Speed magnitude
  const TVector3& unitDirection() const { return m_unitDirection; }  ///< unit direction of velocity (3d)
  const TVector3& origin() const { return m_origin; }  ///< start vertex
  bool hasNamedPoint(papas::Position layer) const;   ///< Checks if there is a path point at this location
  const TVector3& namedPoint(papas::Position layer) const; ///<  Returns path point matching this location
  virtual TVector3 pointAtTime(double time) const; ///< Returns the 3D point on the path at a given time
  const Points& points() const { return m_points; }  ///< Returns all path points
  double field() const { return m_field; }  ///< Returns magnetic field for Helix (or 0 for straighline)

protected:
  TLorentzVector m_p4;  ///< 4-momentum
  TVector3 m_unitDirection; ///< unit direction of velocity (3d)
  double m_speed; ///< Speed magnitude
  TVector3 m_origin; ///< start vertex (3d)
  Points m_points; ///< Map of path points indexed by position
  double m_field;  ///< Magnetic field which is set to 0 for a straightline
private:
};

/// Alternative name for Path class
class StraightLine : public Path {
  using Path::Path;
};
}  // end namespace papas

#endif /* path_h */
