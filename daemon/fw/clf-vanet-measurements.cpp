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

#include "clf-vanet-measurements.hpp"

namespace nfd {
    namespace fw {
        namespace clf {

            NFD_LOG_INIT(ClfMeasurements);

            const double CsStats::ALPHA = 0.125;

            CsStats::CsStats()
                    : m_noOfInterest(0)
                    , m_noOfData(0)
                    , m_cs(0)
                    , m_scs(0)
            {
            }

            CsStats::Cscore
            CsStats::computeScs(Cscore currentCscore, Cscore previousCscore)
            {
                return ALPHA * currentCscore + (1 - ALPHA) * previousCscore;
            }

            void
            CsStats::incrementInterestCount(ndn::Name prefix)
            {
                m_noOfInterest++;
                NFD_LOG_DEBUG("Increasing interest count for " << prefix << ". NoOfInterests: " << m_noOfInterest);
            }

            void
            CsStats::incrementDataCount(ndn::Name prefix)
            {
                m_noOfData++;
                NFD_LOG_DEBUG("Increasing data count for " << prefix << ". NoOfData: " << m_noOfData);
            }

            void
            CsStats::updateScore(ndn::Name prefix)
            {
                if (m_noOfData <= m_noOfInterest) {
                    if (m_noOfData == 0 && m_noOfInterest == 0) {
                        m_cs = 0;
                    }
                    else {
                        m_cs = (double) m_noOfData/ m_noOfInterest;
                    }
                }
                else { // will happen if a new entry is created from prefix ann.
                    m_noOfInterest = m_noOfData;
                    m_cs = 1.0;
                }

                // calculate new scs
                m_scs = computeScs(m_cs, m_scs);

                NFD_LOG_DEBUG("Calculating centrality score for " << prefix  << ": NoOfInterests = " << m_noOfInterest << ", NoOfData = " << m_noOfData << ". CS= " << m_cs << ", SCS= " << m_scs);

                // reset interest and data count
                m_noOfInterest = 0;
                m_noOfData = 0;
            }

            void
            CsStats::decayScore()
            {
                NFD_LOG_DEBUG("Decaying score from " << m_scs << " to " << m_scs/2 );
                m_scs = m_scs * VanetMeasurements::R;
            }

//            DestLocationInfo::DestLocationInfo()
//                    : m_destLocation(ndn::Location(0, 0))
//            {
//            }

//            void
//            DestLocationInfo::setLocation(ndn::Name namePrefix, ndn::Location destLocation) {
//                NFD_LOG_DEBUG("Setting dest location for prefix " << namePrefix << " to (" << destLocation.getLongitude() << ", "  << destLocation.getLatitude() << ")");
//
//                m_destLocation = destLocation;
//            }

            FaceInfo::FaceInfo()
            {
            }

            void
            FaceInfo::recordCs(const shared_ptr<pit::Entry>& pitEntry, const Face& inFace)
            {
            }

            void
            FaceInfo::updateScore(ndn::Name prefix)
            {
                m_csStats.updateScore(prefix);
            }

            void
            FaceInfo::incrementInterestCount(ndn::Name prefix)
            {
                m_csStats.incrementInterestCount(prefix);
            }

            void
            FaceInfo::incrementDataCount(ndn::Name prefix)
            {
                m_csStats.incrementDataCount(prefix);
            }

            void
            FaceInfo::decayScore()
            {
                m_csStats.decayScore();
                // schedule next round of decay here
//  m_scoreDecayEventId = scheduler::schedule(VanetMeasurements::SCORE_DECAY_TIME, [&] { decayScore(); });
            }

            void
            FaceInfo::rescheduleScoreDecay(FaceInfo& info, FaceId faceId, ndn::Name prefix)
            {
                NFD_LOG_DEBUG("Rescheduling score decay event for " << prefix.toUri());
                scheduler::cancel(info.getScoreDecayEventId());

                //auto id = scheduler::schedule(VanetMeasurements::SCORE_DECAY_TIME, [&] { decayScore(); });
                //info.setScoreDecayEventId(id);
            }

//            void
//            FaceInfo::updateLocation(ndn::Name namePrefix, ndn::Location dl)
//            {
//                m_destLocationInfo.setLocation(namePrefix, dl);
//            }

            NamespaceInfo::NamespaceInfo()
            {
            }

            FaceInfo*
            NamespaceInfo::getFaceInfo(const fib::Entry& fibEntry, FaceId faceId)
            {
                FaceInfoTable::iterator it = m_fit.find(faceId);

                if (it != m_fit.end()) {
                    return &it->second;
                }
                else {
                    return nullptr;
                }
            }

            void
            NamespaceInfo::rescheduleScoreDecay(FaceInfo& info, FaceId faceId, ndn::Name prefix)
            {
                NFD_LOG_DEBUG("Rescheduling score decay event for " << prefix.toUri());
                scheduler::cancel(info.getScoreDecayEventId());

                //auto id = scheduler::schedule(VanetMeasurements::SCORE_DECAY_TIME, [&] { info.decayScore(prefix); });
                //info.setScoreDecayEventId(id);
            }

            FaceInfo&
            NamespaceInfo::getOrCreateFaceInfo(const fib::Entry& fibEntry, FaceId faceId)
            {
                FaceInfoTable::iterator it = m_fit.find(faceId);

                FaceInfo* info = nullptr;

                if (it == m_fit.end()) {
                    NFD_LOG_DEBUG("Creating FaceInfo on face " << faceId);
                    const auto& pair = m_fit.emplace(faceId, FaceInfo());
                    info = &pair.first->second;

                    //extendFaceInfoLifetime(*info, faceId);
                }
                else {
                    info = &it->second;
                }

                return *info;

            }

            constexpr time::microseconds VanetMeasurements::SCORE_DECAY_TIME;
            constexpr double VanetMeasurements::R;
            constexpr time::microseconds VanetMeasurements::MEASUREMENTS_LIFETIME;

            VanetMeasurements::VanetMeasurements(MeasurementsAccessor& measurements)
                    : m_measurements(measurements)
            {
            }

            FaceInfo&
            VanetMeasurements::getOrCreateFaceInfo(const fib::Entry& fibEntry, ndn::Name namePrefix,
                                                   FaceId faceId)
            {
                NamespaceInfo& info = getOrCreateNamespaceInfo(fibEntry, namePrefix);
                return info.getOrCreateFaceInfo(fibEntry, faceId);
            }

            NamespaceInfo*
            VanetMeasurements::getNamespaceInfo(const Name& prefix) {
                measurements::Entry* me = m_measurements.findLongestPrefixMatch(prefix);
                if (me == nullptr) {
                    return nullptr;
                }

                NamespaceInfo* info = me->insertStrategyInfo<NamespaceInfo>().first;
                BOOST_ASSERT(info != nullptr);
                return info;
            }

            NamespaceInfo&
            VanetMeasurements::getOrCreateNamespaceInfo(const fib::Entry& fibEntry,
                                                        const ndn::Name namePrefix) {

                //NFD_LOG_DEBUG("FibEntry: " << fibEntry.getPrefix().toUri());
                measurements::Entry* me = m_measurements.get(fibEntry);

                // If the FIB entry is not under the strategy's namespace, find a part of the prefix
                // that falls under the strategy's namespace
                /*for (size_t prefixLen = fibEntry.getPrefix().size() + 1;
                     me == nullptr && prefixLen <= namePrefix.size(); ++prefixLen) {
                  me = m_measurements.get(namePrefix.getPrefix(prefixLen));
                  NFD_LOG_DEBUG("MeEntry: " << me->getName().toUri());
                }*/

                me = m_measurements.get(namePrefix);

                //NFD_LOG_DEBUG("Creating namespaceinfo for " << me->getName().toUri());

                // Either the FIB entry or the Interest's name must be under this strategy's namespace
                BOOST_ASSERT(me != nullptr);

                // Set or update entry lifetime
                extendLifetime(*me);

                NamespaceInfo* info = me->insertStrategyInfo<NamespaceInfo>().first;
                BOOST_ASSERT(info != nullptr);
                return *info;
            }

            double
            VanetMeasurements::getCs(const fib::Entry& fibEntry, FaceId faceId, ndn::Name namePrefix)
            {
                // also need to update the score of the prefix's parents
                measurements::Entry* currentMe = m_measurements.get(namePrefix);
                NamespaceInfo* namespaceInfo = currentMe->getStrategyInfo<NamespaceInfo>();
                for (size_t prefixLen = namePrefix.size();
                     namespaceInfo == nullptr && prefixLen > 0; --prefixLen) {
                    //NFD_LOG_DEBUG(namePrefix.getPrefix(prefixLen));
                    currentMe = m_measurements.get(namePrefix.getPrefix(prefixLen));
                    namespaceInfo = currentMe->getStrategyInfo<NamespaceInfo>();
                }

                if (namespaceInfo != nullptr) {
                    FaceInfo* faceInfo = namespaceInfo->get(faceId);
                    NFD_LOG_DEBUG("Longest matching measurement entry for prefix " << namePrefix.toUri() << " is " <<currentMe->getName().toUri() << ". Score is " << faceInfo->getScs());
                    return faceInfo->getScs();
                }
                else {
                    NFD_LOG_DEBUG("Measurement entry for prefix " << namePrefix.toUri() << " not found. Score is 0.");
                    return 0;
                }
            }

//            ndn::Location
//            VanetMeasurements::getDestLocationInfo(const fib::Entry& fibEntry,
//                                                   ndn::Name namePrefix, FaceId faceId)
//            {
//                // also need to update the score of the prefix's parents
//                measurements::Entry* currentMe = m_measurements.get(namePrefix);
//                NamespaceInfo* namespaceInfo = currentMe->getStrategyInfo<NamespaceInfo>();
//                for (size_t prefixLen = namePrefix.size();
//                     namespaceInfo == nullptr && prefixLen > 0; --prefixLen) {
//                    //NFD_LOG_DEBUG(namePrefix.getPrefix(prefixLen));
//                    currentMe = m_measurements.get(namePrefix.getPrefix(prefixLen));
//                    namespaceInfo = currentMe->getStrategyInfo<NamespaceInfo>();
//                }
//
//                if (namespaceInfo != nullptr) {
//                    FaceInfo* faceInfo = namespaceInfo->get(faceId);
//                    NFD_LOG_DEBUG("Longest matching measurement entry for prefix " << namePrefix.toUri() << " is " << currentMe->getName().toUri() << ". Location is: " << faceInfo->getDestLocation().getLongitude() << ", " << faceInfo->getDestLocation().getLatitude());
//                    return faceInfo->getDestLocation();
//                }
//                else {
//                    NFD_LOG_DEBUG("Measurement entry for prefix " << namePrefix.toUri() << " not found. Location is (0, 0).");
//
//                    return ndn::Location(0, 0);
//                }
//            }

// called when data comes back with a prefix announcement
            void
            VanetMeasurements::updateScoreAndLocation(const fib::Entry& fibEntry, ndn::Name namePrefix,
                                                      ndn::Name annPrefix, FaceId faceId)
            {
                FaceInfo& faceInfo = getOrCreateFaceInfo(fibEntry, annPrefix, faceId);

                // also need to update the score of the prefix's parents
                measurements::Entry* currentMe = m_measurements.get(annPrefix);

                BOOST_ASSERT(currentMe != nullptr);

                while(currentMe != nullptr) {
                    //NFD_LOG_DEBUG("Updating score for: " << currentMe->getName());

                    // get or create face info, which in turn will get or create name space info
                    FaceInfo& faceInfo = getOrCreateFaceInfo(fibEntry,currentMe->getName(), faceId);

                    // update score for this entry
                    //faceInfo.updateScore(currentMe->getName());
                    //faceInfo.rescheduleScoreDecay(faceInfo, faceId, currentMe->getName());

//                    if (currentMe->getName() == annPrefix) {
//                        // update location info
//                        NFD_LOG_DEBUG("Updating location info for: " << currentMe->getName());
//                        faceInfo.updateLocation(annPrefix, destLocation);
//                    }

                    // Set or update entry lifetime
                    extendLifetime(*currentMe);

                    // go to next parent
                    currentMe = m_measurements.getParent(*currentMe);
                }
            }

// called when PIT expires; update score for longest matched entry (can be deleted; not used anymore))
            void
            VanetMeasurements::updateScoreAndLocationLongestMatched(const fib::Entry& fibEntry, ndn::Name namePrefix,
                                                                    FaceId faceId)
            {
                measurements::Entry* currentMe = m_measurements.get(namePrefix);
                NamespaceInfo* namespaceInfo = currentMe->getStrategyInfo<NamespaceInfo>();
                for (size_t prefixLen = namePrefix.size();
                     namespaceInfo == nullptr && prefixLen > 0; --prefixLen) {
                    //NFD_LOG_DEBUG(namePrefix.getPrefix(prefixLen));

                    currentMe = m_measurements.get(namePrefix.getPrefix(prefixLen));
                    namespaceInfo = currentMe->getStrategyInfo<NamespaceInfo>();
                }

                if (namespaceInfo != nullptr) {
                    NFD_LOG_DEBUG("Longest matching measurement entry for prefix " << namePrefix.toUri() << " is " << currentMe->getName().toUri());

                    // also need to update the score of the longest-matched prefix's parents
                    while(currentMe != nullptr) {
                        NFD_LOG_DEBUG("Updating score for: " << currentMe->getName());

                        namespaceInfo = currentMe->getStrategyInfo<NamespaceInfo>();
                        FaceInfo* faceInfo = namespaceInfo->get(faceId);
                        faceInfo->updateScore(currentMe->getName());

                        // Set or update entry lifetime
                        extendLifetime(*currentMe);

                        currentMe = m_measurements.getParent(*currentMe);
                    }
                }
                else {
                    NFD_LOG_DEBUG("Measurement entry for prefix " << namePrefix.toUri() << " not found.");
                }
            }

// called when forwarding interest; increment interest count for longest matched entry
            void
            VanetMeasurements::incrementInterestCount(const fib::Entry& fibEntry, ndn::Name namePrefix,
                                                      FaceId faceId)
            {

                measurements::Entry* currentMe = m_measurements.get(namePrefix);
                NamespaceInfo* namespaceInfo = currentMe->getStrategyInfo<NamespaceInfo>();
                for (size_t prefixLen = namePrefix.size();
                     namespaceInfo == nullptr && prefixLen > 0; --prefixLen) {
                    //NFD_LOG_DEBUG(namePrefix.getPrefix(prefixLen));

                    currentMe = m_measurements.get(namePrefix.getPrefix(prefixLen));
                    namespaceInfo = currentMe->getStrategyInfo<NamespaceInfo>();
                }

                if (namespaceInfo != nullptr) {
                    NFD_LOG_DEBUG("Longest matching measurement entry for prefix " << namePrefix.toUri() << " is " << currentMe->getName().toUri());

                    // also need to update the score of the prefix's parents
                    while(currentMe != nullptr) {
                        //NFD_LOG_DEBUG("Incrementing Interest count for: " << currentMe->getName());

                        namespaceInfo = currentMe->getStrategyInfo<NamespaceInfo>();
                        FaceInfo* faceInfo = namespaceInfo->get(faceId);
                        faceInfo->incrementInterestCount(currentMe->getName());

                        // insert the name to the list
                        nameList.insert(currentMe->getName());

                        // Set or update entry lifetime
                        extendLifetime(*currentMe);

                        currentMe = m_measurements.getParent(*currentMe);
                    }
                }
                else {
                    NFD_LOG_DEBUG("Measurement entry for prefix " << namePrefix.toUri() << " not found. First interest. Create root prefix's measurement entry.");
                    // create root entry and increment its interest count (hardcoded for now)
                    FaceInfo& faceInfo = getOrCreateFaceInfo(fibEntry,ndn::Name("/test"), faceId);
                    faceInfo.incrementInterestCount(ndn::Name("/test"));
                    //scheduler::schedule(VanetMeasurements::SCORE_DECAY_TIME, [&] { updateScoreBfs(); });
                    ns3::Simulator::Schedule(ns3::Seconds(6), &VanetMeasurements::updateScoreBfs, this);
                }
            }

// called when forwarding data; increment data count for announced prefix
            void
            VanetMeasurements::incrementDataCount(const fib::Entry& fibEntry, ndn::Name annPrefix,
                                                  FaceId faceId)
            {
                FaceInfo& faceInfo = getOrCreateFaceInfo(fibEntry, annPrefix, faceId);

                // also need to update the score of the prefix's parents
                measurements::Entry* currentMe = m_measurements.get(annPrefix);

                BOOST_ASSERT(currentMe != nullptr);

                while(currentMe != nullptr) {
                    //NFD_LOG_DEBUG("Incrementing Data count for: " << currentMe->getName());

                    // get or create face info, which in turn will get or create name space info
                    FaceInfo& faceInfo = getOrCreateFaceInfo(fibEntry,currentMe->getName(), faceId);
                    //NamespaceInfo* namespaceInfo = currentMe->insertStrategyInfo<NamespaceInfo>().first;

                    // update score for this entry
                    faceInfo.incrementDataCount(currentMe->getName());

                    // insert the name to the list
                    nameList.insert(currentMe->getName());

                    // Set or update entry lifetime
                    extendLifetime(*currentMe);

                    // go to next parent
                    currentMe = m_measurements.getParent(*currentMe);
                }
            }

            void
            VanetMeasurements::updateScoreBfs()
            {
                NFD_LOG_DEBUG("");
                ndn::Name root("/test");
                fib::Entry fibEntry(ndn::Name("/"));
                FaceId faceId = 257;
                FaceInfo& faceInfo = getOrCreateFaceInfo(fibEntry, root, faceId);

                measurements::Entry* rootMe = m_measurements.get(root);
                std::vector<measurements::Entry*> allNodesMe = m_measurements.getAllNodesMe(*rootMe);

                for (auto node : allNodesMe) {
                    if (nameList.find(node->getName()) != nameList.end()) {
                        NFD_LOG_DEBUG("Updating Node: " << node->getName());
                        FaceInfo& faceInfo = getOrCreateFaceInfo(fibEntry,node->getName(), faceId);
                        faceInfo.updateScore(node->getName());
                    }
                }

                ns3::Simulator::Schedule(ns3::Seconds(6), &VanetMeasurements::updateScoreBfs, this);
            }

            void
            VanetMeasurements::extendLifetime(measurements::Entry& me)
            {
                m_measurements.extendLifetime(me, MEASUREMENTS_LIFETIME);
            }

        } // vanet
    } // fw
} // nfd