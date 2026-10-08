#include "crash3_widescreen.h"

namespace crash3 {

namespace {

// Pass-through list needed: only this title `cfc2`s CR[24]/CR[25].
constexpr crash::ProjectionTitleFacts kMeasuredFacts{
    .serial = "crash3-wide",
    .centreXRegister = 24,
    .centreYRegister = 25,
    .screenDistanceRegister = 26,
    .projectionInit = kProjectionInit,
    .setGeomOffset = kSetGeomOffset,
    .setGeomScreen = kSetGeomScreen,
    .retailScreenDistance = kRetailScreenDistance,
    .retailCentreX = kRetailCentreX,
    .retailCentreY = kRetailCentreY,
    .passThroughCallSites = kPassThroughCallSites,
    .passThroughCallSiteCount = kPassThroughCallSiteCount,
    .centreCallSites = kCentreCallSites,
    .centreCallSiteCount = kCentreCallSiteCount,
};

} // namespace

const crash::ProjectionTitleFacts &Crash3Widescreen::facts() {
  return kMeasuredFacts;
}

} // namespace crash3
