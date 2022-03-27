/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2014-2018,  Regents of the University of California,
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

#ifndef NFD_DAEMON_FW_VANET_MEASUREMENTS_HPP
#define NFD_DAEMON_FW_VANET_MEASUREMENTS_HPP

#include "fw/strategy-info.hpp"
#include "table/measurements-accessor.hpp"

//#include <ndn-cxx/location.hpp>

namespace nfd {
    namespace fw {
        namespace clf {

            class CsStats {
            public:
                typedef double Cscore;

                CsStats();

                Cscore
                getCs() const {
                    return m_cs;
                }

                Cscore
                getScs() const {
                    return m_scs;
                }

                void
                incrementInterestCount(ndn::Name prefix);

                void
                incrementDataCount(ndn::Name prefix);

                void
                updateScore(ndn::Name);

                void
                decayScore();

//                void
//                updateLocation(ndn::Name namePrefix, ndn::Location dl);

            private:
                static Cscore
                computeScs(Cscore previousCscore, Cscore currentCscore);

                int m_noOfInterest;
                int m_noOfData;
                Cscore m_cs;
                Cscore m_scs;

                static const double ALPHA;
            };

//            class DestLocationInfo {
//            public:
//                DestLocationInfo();
//
////                ndn::Location
////                getDestLocation() const {
////                    return m_destLocation;
////                }
//
////                void
////                setLocation(ndn::Name, ndn::Location);
//
//            private:
////                ndn::Location m_destLocation;
//            };

            class FaceInfo {
            public:
                class Error : public std::runtime_error {
                public:
                    explicit
                    Error(const std::string &what)
                            : std::runtime_error(what) {
                    }
                };

                FaceInfo();

                void
                recordCs(const shared_ptr <pit::Entry> &pitEntry, const Face &inFace);

                void
                updateScore(ndn::Name);

                void
                incrementInterestCount(ndn::Name prefix);

                void
                incrementDataCount(ndn::Name prefix);

                void
                decayScore();

//

                void
                rescheduleScoreDecay(FaceInfo &info, FaceId faceId, ndn::Name prefix);

                CsStats::Cscore
                getCs() const {
                    return m_csStats.getCs();
                }

                CsStats::Cscore
                getScs() const {
                    return m_csStats.getScs();
                }

//                ndn::Location
//                getDestLocation() const {
//                    return m_destLocationInfo.getDestLocation();
//                }

                void
                setScoreDecayEventId(const scheduler::EventId &eId) {
                    m_scoreDecayEventId = eId;
                }

                const scheduler::EventId &
                getScoreDecayEventId() {
                    return m_scoreDecayEventId;
                }

            private:
                CsStats m_csStats;
//                DestLocationInfo m_destLocationInfo;

                scheduler::EventId m_scoreDecayEventId;
            };

            typedef std::unordered_map <FaceId, FaceInfo> FaceInfoTable;

            class NamespaceInfo : public StrategyInfo {
            public:
                NamespaceInfo();

                static constexpr int
                getTypeId() {
                    return 1080;
                }

                FaceInfo &
                getOrCreateFaceInfo(const fib::Entry &fibEntry, FaceId faceId);

                FaceInfo *
                getFaceInfo(const fib::Entry &fibEntry, FaceId faceId);

                void
                rescheduleScoreDecay(FaceInfo &, FaceId, ndn::Name);

                FaceInfo *
                get(FaceId faceId) {
                    if (m_fit.find(faceId) != m_fit.end()) {
                        return &m_fit.at(faceId);
                    } else {
                        return nullptr;
                    }
                }

                FaceInfoTable::iterator
                find(FaceId faceId) {
                    return m_fit.find(faceId);
                }

                FaceInfoTable::iterator
                end() {
                    return m_fit.end();
                }

                const FaceInfoTable::iterator
                insert(FaceId faceId) {
                    return m_fit.emplace(faceId, FaceInfo()).first;
                }

                void
                setDestination(double lat, double longi) {
                    m_destination = std::make_pair(lat, longi);
                }

                std::pair<double, double>
                getDestination() const {
                    return m_destination;
                }

            private:
                FaceInfoTable m_fit;
                std::pair<double, double> m_destination; // lat, long. todo: make it a list of destination
            };

            class VanetMeasurements : noncopyable {
            public:
                explicit
                VanetMeasurements(MeasurementsAccessor &measurements);

                FaceInfo *
                getFaceInfo(const fib::Entry &fibEntry, FaceId faceId);

                FaceInfo &
                getOrCreateFaceInfo(const fib::Entry &fibEntry, ndn::Name name, FaceId faceId);

                NamespaceInfo *
                getNamespaceInfo(const Name &prefix);

                NamespaceInfo &
                getOrCreateNamespaceInfo(const fib::Entry &fibEntry, const ndn::Name name);

//                void
//                updateScoreAndLocation(const fib::Entry &fibEntry, ndn::Name namePrefix, ndn::Name annPrefix,
//                                       ndn::Location destLocation, FaceId faceId);
                void
                updateScoreAndLocation(const fib::Entry &fibEntry, ndn::Name namePrefix, ndn::Name annPrefix,
                                       FaceId faceId);

                void
                updateScoreAndLocationLongestMatched(const fib::Entry &fibEntry, ndn::Name namePrefix,
                                                     FaceId faceId);

                void
                incrementInterestCount(const fib::Entry &fibEntry, ndn::Name namePrefix,
                                       FaceId faceId);

                void
                incrementDataCount(const fib::Entry &fibEntry, ndn::Name namePrefix,
                                   FaceId faceId);

                double
                getCs(const fib::Entry &fibEntry, FaceId faceId, ndn::Name namePrefix);

//                ndn::Location
//                getDestLocationInfo(const fib::Entry &fibEntry, ndn::Name namePrefix, FaceId faceId);

            private:
                void
                extendLifetime(measurements::Entry &me);

                void
                updateScoreBfs();

            public:
                static constexpr time::microseconds
                SCORE_DECAY_TIME = 10_s;
                static constexpr double R = 0.9;
                static constexpr time::microseconds
                MEASUREMENTS_LIFETIME = 3000_s;

            private:
                MeasurementsAccessor &m_measurements;
                std::set <ndn::Name> nameList;
            };

        } // namespace vanet
    } // namespace fw
} // namespace nfd

#endif // NFD_DAEMON_FW_VANET_MEASUREMENTS_HPP