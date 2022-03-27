/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 * Copyright (c) 2014-2017,  Regents of the University of California,
 *                           Arizona Board of Regents,
 *                           Colorado State University,
 *                           University Pierre & Marie Curie, Sorbonne University,
 *                           Washington University in St. Louis,
 *                           Beijing Institute of Technology,
 *                           The University of Memphis.
 *
 * This file is part of NFD (Named Data Networking Forwarding Daemon).
 * See AUTHORS.md for complete list of NFD authors and contributors.
 *
 * NFD is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.
 *
 * NFD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * NFD, e.g., in COPYING.md file.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef NFD_DAEMON_FW_CLF_STRATEGY_HPP
#define NFD_DAEMON_FW_CLF_STRATEGY_HPP

#include "clf-vanet-measurements.hpp"
#include "strategy.hpp"
#include "clf-prefix-location-tree.hpp"

#include <ndn-cxx/lp/location-header.hpp>
#include <ndn-cxx/util/scheduler.hpp>

#include <cmath>
#include <math.h>

#include <boost/asio.hpp>

namespace nfd {
    namespace fw {
        namespace clf {

/** \brief a forwarding strategy that forwards Interest to all FIB nexthops
 */
            class ClfStrategy : public Strategy {
            public:
                explicit
                ClfStrategy(Forwarder &forwarder, const Name &name = getStrategyName());

                static const Name &
                getStrategyName();

                void
                afterReceiveInterest(const Face &inFace, const Interest &interest,
                                     const shared_ptr <pit::Entry> &pitEntry) override;

                void
                afterReceiveNack(const Face &inFace, const lp::Nack &nack,
                                 const shared_ptr <pit::Entry> &pitEntry) override;

                void
                afterReceiveData(const shared_ptr <pit::Entry> &pitEntry,
                                 const Face &inFace, const Data &data) override;

                void
                onLoopedInterest(const shared_ptr <pit::Entry> &pitEntry, const Face &outFace,
                                 const Interest &interest) override;

                void
                onPitExpiration(const shared_ptr <pit::Entry> &pitEntry) override;

            private:
                void
                afterReceiveInterestBroadcast(const Face &inFace, const Interest &interest,
                                              const shared_ptr <pit::Entry> &pitEntry);

                void
                afterReceiveInterestVndn(const Face &inFace, const Interest &interest,
                                         const shared_ptr <pit::Entry> &pitEntry);

                void
                afterReceiveInterestNavigo(const Face &inFace, const Interest &interest,
                                           const shared_ptr <pit::Entry> &pitEntry);

                void
                afterReceiveInterestClf(const Face &inFace, const Interest &interest,
                                        const shared_ptr <pit::Entry> &pitEntry);

                void
                afterReceiveDataNormal(const shared_ptr <pit::Entry> &pitEntry,
                                       const Face &inFace, const Data &data);

                void
                afterReceiveDataClf(const shared_ptr <pit::Entry> &pitEntry,
                                    const Face &inFace, const Data &data);


                // This function converts decimal degrees to radians
                double deg2rad(double deg) {
                    return (deg * M_PI / 180);
                }

                //  This function converts radians to decimal degrees
                double rad2deg(double rad) {
                    return (rad * 180 / M_PI);
                }

                double
                calculateDistanceEuclid(double x1, double y1, double x2, double y2);

                double
                calculateDistance(double lat1d, double lon1d,
                                  double lat2d, double lon2d);


                double
                getLocationScore(ndn::Location pl, ndn::Location dl,
                                 ndn::Location ml) {
                    double distanceFromMeToDest = calculateDistanceEuclid(dl.getLatitude(), dl.getLongitude(),
                                                                          ml.getLatitude(), ml.getLongitude());
                    double distanceFromPrevToDest = calculateDistanceEuclid(pl.getLatitude(), pl.getLongitude(),
                                                                            dl.getLatitude(), dl.getLongitude());

                    return (1 - (distanceFromMeToDest / std::max(distanceFromMeToDest, distanceFromPrevToDest)));
                }

                double
                getLocationScoreVndn(ndn::Location pl, ndn::Location ml) {
                    double distanceFromMe = calculateDistance(pl.getLatitude(), pl.getLongitude(),
                                                              ml.getLatitude(), ml.getLongitude());

                    std::cout << "distanceFromMeToPrev: " << distanceFromMe << std::endl;

                    //return distanceFromMe/std::max(distanceFromMe, distanceFromPrev);
                    return 0.0;
                }

                double
                getCentralityScore(const shared_ptr <pit::Entry> &pitEntry, FaceId faceId);

                ndn::Location
                getDestLocation(const shared_ptr <pit::Entry> &pitEntry, FaceId faceId);

                double
                calculateTimer(double lat1d, double lon1d,
                               double lat2d, double lon2d);

                void
                forwardInterest(const Interest &interest,
                                const shared_ptr <pit::Entry> &pitEntry,
                        //TODO: FaceInfo* info,
                                Face *outFace);

            private:
                VanetMeasurements m_measurements;

                const double earthRadiusKm = 6371;
                //ndn::DummyIoService m_ioService;
                //ndn::Scheduler m_scheduler;
                std::unordered_map <ndn::Name, ns3::EventId> m_scheduledInterstPool;

                PrefixLocationTree m_prefixLocation;

                static const double ALPHA;
            };

        } // namespace clf
    } // namespace fw
} // namespace nfd

#endif // NFD_DAEMON_FW_CLF_STRATEGY_HPP