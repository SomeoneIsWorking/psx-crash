#include "crash1_widescreen.h"

namespace crash1 {

namespace {

// No pass-through list: the image has no `cfc2` read of CR[24]/CR[25].
constexpr crash::ProjectionTitleFacts kMeasuredFacts{
    .serial = "crash1-wide",
    .centreXRegister = 24,
    .centreYRegister = 25,
    .screenDistanceRegister = 26,
    .projectionInit = kProjectionInit,
    .setGeomOffset = kSetGeomOffset,
    .setGeomScreen = kSetGeomScreen,
    .retailScreenDistance = kRetailScreenDistance,
    .retailCentreX = kRetailCentreX,
    .retailCentreY = kRetailCentreY,
    .centreReach = kCentreReach,
    .passThroughCallSites = nullptr,
    .passThroughCallSiteCount = 0,
    .centreCallSites = kCentreCallSites,
    .centreCallSiteCount = sizeof(kCentreCallSites) / sizeof(kCentreCallSites[0]),
};

} // namespace

const crash::ProjectionTitleFacts &Crash1Widescreen::facts() {
  return kMeasuredFacts;
}

} // namespace crash1
