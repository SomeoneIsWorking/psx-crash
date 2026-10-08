#include "crash2_widescreen.h"

namespace crash2 {

namespace {

// No pass-through list: the image has no `cfc2` read of CR[24]/CR[25].
constexpr crash::ProjectionTitleFacts kMeasuredFacts{
    .serial = "crash2-wide",
    .centreXRegister = 24,
    .centreYRegister = 25,
    .screenDistanceRegister = 26,
    .projectionInit = kProjectionInit,
    .setGeomOffset = kSetGeomOffset,
    .setGeomScreen = kSetGeomScreen,
    .retailScreenDistance = kRetailScreenDistance,
    .retailCentreX = kRetailCentreX,
    .retailCentreY = kRetailCentreY,
    .passThroughCallSites = nullptr,
    .passThroughCallSiteCount = 0,
    .centreCallSites = kCentreCallSites,
    .centreCallSiteCount = kCentreCallSiteCount,
};

} // namespace

const crash::ProjectionTitleFacts &Crash2Widescreen::facts() {
  return kMeasuredFacts;
}

} // namespace crash2
