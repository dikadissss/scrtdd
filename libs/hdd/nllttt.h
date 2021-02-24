/***************************************************************************
 *   Copyright (C) by ETHZ/SED                                             *
 *                                                                         *
 * This program is free software: you can redistribute it and/or modify    *
 * it under the terms of the GNU Affero General Public License as published*
 * by the Free Software Foundation, either version 3 of the License, or    *
 * (at your option) any later version.                                     *
 *                                                                         *
 * This program is distributed in the hope that it will be useful,         *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of          *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the           *
 * GNU Affero General Public License for more details.                     *
 *                                                                         *
 *                                                                         *
 *   Developed by Luca Scarabello <luca.scarabello@sed.ethz.ch>            *
 ***************************************************************************/

#ifndef __HDD_NLLTTT_H__
#define __HDD_NLLTTT_H__

#include "ttt.h"

#include <seiscomp3/core/baseobject.h>
#include <seiscomp3/seismology/ttt.h>

#include <fstream>
#include <unordered_map>
#include <unordered_set>

namespace Seiscomp {
namespace HDD {
namespace NLL {

// Transform should be an abstract class and each transform type should
// provide a specific implementation, but I do not want to add that extra
// complexity and slow down the perfomance (virtual methods). At the moment
// it's not worth it
struct Transform
{
  Transform(const std::vector<std::string> &tokens);
  ~Transform() {}

  void fromLatLon(double lat, double lon, double &xLoc, double &yLoc) const;
  void toLatLon(double xLoc, double yLoc, double &lat, double &lon) const;
  double fromLatLonAngle(double latlonAngle) const;
  double toLatLonAngle(double rectAngle) const;
  double distance(double xLoc1, double yLoc1, double xLoc2, double yLoc2) const;
  double distance(double xLoc1,
                  double yLoc1,
                  double zLoc1,
                  double xLoc2,
                  double yLoc2,
                  double zLoc2) const;

  struct Info
  {
    std::string type;
    double angle;
    double cosang;
    double sinang;
    double orig_lat;
    double orig_long;
    double rot;
    double sdc_xltkm;
    double sdc_xlnkm;
  };
  const Info info;
  static Info parse(const std::vector<std::string> &tokens);

  /*
   * Adopting NLL constants to improve compatibility
   */
  static constexpr double FLATTENING =
      1.0 / 298.26;                              // Earth flattening (WGS '72)
  static constexpr double ERAD = 6378.135;       // WGS-72
  static constexpr double c111 = 10000.0 / 90.0; // kilometers per degree
  static constexpr double MAP_TRANS_SDC_DRLT = 0.99330647;
};

class Grid : public Core::BaseObject
{
public:
  enum class Type
  {
    time,
    angle,
    velocity
  };

  static std::string filePath(const std::string &basePath,
                              const Catalog::Station &station,
                              const std::string &phaseType);
  Grid(Type gridType,
       const std::string &basePath,
       const Catalog::Station &station,
       const std::string &phaseType,
       bool swapBytes);
  virtual ~Grid() {}

  bool isLocationInside(double xloc, double yloc, double zloc) const;
  bool isIndexInside(unsigned long long ix,
                     unsigned long long iy,
                     unsigned long long iz) const;
  virtual bool is3D() const { return info.numx > 1; }

  struct Info
  {
    std::string hdrFilePath;
    std::string bufFilePath;
    Type gridType;
    bool swapBytes; // should disk values bytes be swapped?

    unsigned long long numx, numy, numz;
    double origx, origy, origz; // km
    double dx, dy, dz;          // km
    std::string type;
    bool useDouble; // grid values stored as double instead of float
    std::string label;
    double srcex, srcey, srcez;
    std::unique_ptr<Transform> transform;
  };
  const Info info;
  static Info
  parse(const std::string &baseFilePath, Type gridType, bool swapBytes);

protected:
  std::ifstream _bufReader;

  template <typename GRID_FLOAT_TYPE> struct Interpolate2D
  {
    typedef std::function<GRID_FLOAT_TYPE(double,
                                          double,
                                          GRID_FLOAT_TYPE,
                                          GRID_FLOAT_TYPE,
                                          GRID_FLOAT_TYPE,
                                          GRID_FLOAT_TYPE)>
        Type;
  };

  template <typename GRID_FLOAT_TYPE> struct Interpolate3D
  {
    typedef std::function<GRID_FLOAT_TYPE(double,
                                          double,
                                          double,
                                          GRID_FLOAT_TYPE,
                                          GRID_FLOAT_TYPE,
                                          GRID_FLOAT_TYPE,
                                          GRID_FLOAT_TYPE,
                                          GRID_FLOAT_TYPE,
                                          GRID_FLOAT_TYPE,
                                          GRID_FLOAT_TYPE,
                                          GRID_FLOAT_TYPE)>
        Type;
  };

  template <typename GRID_FLOAT_TYPE>
  GRID_FLOAT_TYPE
  getValue(double lat,
           double lon,
           double depth,
           const typename Interpolate3D<GRID_FLOAT_TYPE>::Type &,
           const typename Interpolate2D<GRID_FLOAT_TYPE>::Type &);

  template <typename GRID_FLOAT_TYPE>
  GRID_FLOAT_TYPE
  getValue3D(double lat,
             double lon,
             double depth,
             const typename Interpolate3D<GRID_FLOAT_TYPE>::Type &);

  template <typename GRID_FLOAT_TYPE>
  GRID_FLOAT_TYPE
  getValue2D(double lat,
             double lon,
             double depth,
             const typename Interpolate2D<GRID_FLOAT_TYPE>::Type &);

  template <typename GRID_FLOAT_TYPE>
  void getValuesAt3DLocation(double xloc,
                             double yloc,
                             double zloc,
                             double &xdiff,
                             double &ydiff,
                             double &zdiff,
                             GRID_FLOAT_TYPE &vval000,
                             GRID_FLOAT_TYPE &vval001,
                             GRID_FLOAT_TYPE &vval010,
                             GRID_FLOAT_TYPE &vval011,
                             GRID_FLOAT_TYPE &vval100,
                             GRID_FLOAT_TYPE &vval101,
                             GRID_FLOAT_TYPE &vval110,
                             GRID_FLOAT_TYPE &vval111);

  template <typename GRID_FLOAT_TYPE>
  void getValuesAt2DLocation(double yloc,
                             double zloc,
                             double &ydiff,
                             double &zdiff,
                             GRID_FLOAT_TYPE &vval00,
                             GRID_FLOAT_TYPE &vval01,
                             GRID_FLOAT_TYPE &vval10,
                             GRID_FLOAT_TYPE &vval11);

  template <typename GRID_FLOAT_TYPE>
  GRID_FLOAT_TYPE getValueAtIndex(unsigned long long ix,
                                  unsigned long long iy,
                                  unsigned long long iz);
};

class TimeGrid : public Grid
{
public:
  TimeGrid(const std::string &basePath,
           const Catalog::Station &station,
           const std::string &phaseType,
           bool swapBytes);
  double getTime(double lat, double lon, double depth);

protected:
  template <class GRID_FLOAT_TYPE>
  static GRID_FLOAT_TYPE interpolateValues2D(double xdiff,
                                             double zdiff,
                                             GRID_FLOAT_TYPE vval00,
                                             GRID_FLOAT_TYPE vval01,
                                             GRID_FLOAT_TYPE vval10,
                                             GRID_FLOAT_TYPE vval11);
  template <class GRID_FLOAT_TYPE>
  static GRID_FLOAT_TYPE interpolateValues3D(double xdiff,
                                             double ydiff,
                                             double zdiff,
                                             GRID_FLOAT_TYPE vval000,
                                             GRID_FLOAT_TYPE vval001,
                                             GRID_FLOAT_TYPE vval010,
                                             GRID_FLOAT_TYPE vval011,
                                             GRID_FLOAT_TYPE vval100,
                                             GRID_FLOAT_TYPE vval101,
                                             GRID_FLOAT_TYPE vval110,
                                             GRID_FLOAT_TYPE vval111);
};

class AngleGrid : public Grid
{
public:
  AngleGrid(const std::string &basePath,
            const Catalog::Station &station,
            const std::string &phaseType,
            bool swapBytes);
  void
  getAngles(double lat, double lon, double depth, double &azim, double &dip);

  struct TakeOffAngles
  {
    unsigned short quality : 4;  // 0 to 10
    unsigned short dip : 12;     // 0 (down) to 1800 (up) in tenths deg
    unsigned short azimuth : 16; // 0 to 3600 in tenths deg
  };

  static const unsigned QUALITY_CUTOFF = 5;

protected:
  template <class GRID_FLOAT_TYPE>
  static GRID_FLOAT_TYPE interpolateValues2D(double xdiff,
                                             double zdiff,
                                             GRID_FLOAT_TYPE vval00,
                                             GRID_FLOAT_TYPE vval01,
                                             GRID_FLOAT_TYPE vval10,
                                             GRID_FLOAT_TYPE vval11);
  template <class GRID_FLOAT_TYPE>
  static GRID_FLOAT_TYPE interpolateValues3D(double xdiff,
                                             double ydiff,
                                             double zdiff,
                                             GRID_FLOAT_TYPE vval000,
                                             GRID_FLOAT_TYPE vval001,
                                             GRID_FLOAT_TYPE vval010,
                                             GRID_FLOAT_TYPE vval011,
                                             GRID_FLOAT_TYPE vval100,
                                             GRID_FLOAT_TYPE vval101,
                                             GRID_FLOAT_TYPE vval110,
                                             GRID_FLOAT_TYPE vval111);
};

class VelGrid : public Grid
{
public:
  VelGrid(const std::string &basePath,
          const Catalog::Station &station,
          const std::string &phaseType,
          bool swapBytes);
  virtual bool is3D() const { return info.numx > 2; }

  double getVel(double lat, double lon, double depth);

protected:
  template <class GRID_FLOAT_TYPE>
  static GRID_FLOAT_TYPE interpolateValues2D(double xdiff,
                                             double zdiff,
                                             GRID_FLOAT_TYPE vval00,
                                             GRID_FLOAT_TYPE vval01,
                                             GRID_FLOAT_TYPE vval10,
                                             GRID_FLOAT_TYPE vval11);
  template <class GRID_FLOAT_TYPE>
  static GRID_FLOAT_TYPE interpolateValues3D(double xdiff,
                                             double ydiff,
                                             double zdiff,
                                             GRID_FLOAT_TYPE vval000,
                                             GRID_FLOAT_TYPE vval001,
                                             GRID_FLOAT_TYPE vval010,
                                             GRID_FLOAT_TYPE vval011,
                                             GRID_FLOAT_TYPE vval100,
                                             GRID_FLOAT_TYPE vval101,
                                             GRID_FLOAT_TYPE vval110,
                                             GRID_FLOAT_TYPE vval111);
  std::function<double(double)> convertUnits; // velocity -> km/sec
};

DEFINE_SMARTPOINTER(Grid);
DEFINE_SMARTPOINTER(TimeGrid);
DEFINE_SMARTPOINTER(AngleGrid);
DEFINE_SMARTPOINTER(VelGrid);

class NllTravelTimeTable : public TravelTimeTable
{
public:
  NllTravelTimeTable(const std::string &type, const std::string &model);
  virtual ~NllTravelTimeTable() {}

  virtual void compute(double eventLat,
                       double eventLon,
                       double eventDepth,
                       const Catalog::Station &station,
                       const std::string &phaseType,
                       double &travelTime);

  virtual void compute(double eventLat,
                       double eventLon,
                       double eventDepth,
                       const Catalog::Station &station,
                       const std::string &phaseType,
                       double &travelTime,
                       double &takeOfAngleAzim,
                       double &takeOfAngleDip,
                       double &velocityAtSrc);

private:
  std::string _velGridPath;
  std::string _timeGridPath;
  std::string _angleGridPath;
  bool _swapBytes;
  std::unordered_map<std::string, VelGridPtr> _velGrids;
  std::unordered_map<std::string, TimeGridPtr> _timeGrids;
  std::unordered_map<std::string, AngleGridPtr> _angleGrids;
  std::unordered_set<std::string> _unloadableGrids;
};
} // namespace NLL
} // namespace HDD
} // namespace Seiscomp

#endif
