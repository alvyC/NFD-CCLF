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

#include "clf-vanet-strategy.hpp"
#include "algorithm.hpp"
#include "core/logger.hpp"

#include <ndn-cxx/lp/tags.hpp>

#include "ns3/simulator.h"
#include "ns3/nstime.h"
#include "ns3/event-id.h"

#include <random>

namespace nfd {
    namespace fw {
        namespace clf {

            NFD_REGISTER_STRATEGY(ClfStrategy);

            NFD_LOG_INIT(ClfStrategy);

            const double ClfStrategy::ALPHA = 0.5;

            ClfStrategy::ClfStrategy(Forwarder &forwarder, const Name &name)
                    : Strategy(forwarder), m_measurements(getMeasurements())
            //, m_scheduler(m_ioService)
            {
                ParsedInstanceName parsed = parseInstanceName(name);
                if (!parsed.parameters.empty()) {
                    BOOST_THROW_EXCEPTION(std::invalid_argument("ClfStrategy does not accept parameters"));
                }
                if (parsed.version && *parsed.version != getStrategyName()[-1].toVersion()) {
                    BOOST_THROW_EXCEPTION(std::invalid_argument(
                            "ClfStrategy does not support version " + to_string(*parsed.version)));
                }
                this->setInstanceName(makeInstanceName(name, getStrategyName()));
            }

            const Name &
            ClfStrategy::getStrategyName() {
                static Name strategyName("/localhost/nfd/strategy/clf/%FD%03");
                return strategyName;
            }

            void
            ClfStrategy::afterReceiveInterest(const Face &inFace, const Interest &interest,
                                              const shared_ptr <pit::Entry> &pitEntry) {
                //afterReceiveInterestBroadcast(inFace, interest, pitEntry);
                //afterReceiveInterestVndn(inFace, interest, pitEntry);
                //afterReceiveInterestNavigo(inFace, interest, pitEntry);
                afterReceiveInterestClf(inFace, interest, pitEntry);
            }

            void
            ClfStrategy::afterReceiveData(const shared_ptr <pit::Entry> &pitEntry,
                                          const Face &inFace, const Data &data) {
                afterReceiveDataClf(pitEntry, inFace, data);
                //afterReceiveDataNormal(pitEntry, inFace, data);
            }

            void
            ClfStrategy::onLoopedInterest(const shared_ptr <pit::Entry> &pitEntry, const Face &inFace,
                                          const Interest &interest) {
                NFD_LOG_DEBUG(
                        "onLoopedInterest inFace=" << inFace.getId() << " name=" << interest.getName() << " :Drop.");

                // if we already received this interest and put it in the pool
                if (m_scheduledInterstPool.find(interest.getName()) != m_scheduledInterstPool.end()) {
                    // cancel scheduled interest
                    ns3::Simulator::Cancel(m_scheduledInterstPool[interest.getName()]);
                    // delete interest from m_scheduledInterstPool
                    m_scheduledInterstPool.erase(m_scheduledInterstPool.find(interest.getName()));

                    // delete the corresponding PIT entry
                    this->rejectPendingInterest(pitEntry);

                    NFD_LOG_DEBUG(interest << ", cancel scheduled interest.");

                    // drop the current interest
                    return;
                }
            }

            void
            ClfStrategy::onPitExpiration(const shared_ptr <pit::Entry> &pitEntry) {
                const Interest &interest = pitEntry->getInterest();
                NFD_LOG_DEBUG(interest.getName().toUri());

                FaceId faceId = 257; // hardcoded for now
                const fib::Entry &fibEntry = this->lookupFib(*pitEntry);
                ndn::Name namePrefix = interest.getName().getPrefix(interest.getName().size() - 1);

                //m_measurements.updateScoreAndLocationLongestMatched(fibEntry, namePrefix, faceId);
            }

            void
            ClfStrategy::afterReceiveDataNormal(const shared_ptr <pit::Entry> &pitEntry,
                                                const Face &inFace, const Data &data) {
                NFD_LOG_DEBUG("afterReceiveData pitEntry=" << pitEntry->getName() <<
                                                           " inFace=" << inFace.getId() << " data=" << data.getName());

                // someone already responded with the data, cancel our scheduled interest
                if (m_scheduledInterstPool.find(data.getName()) != m_scheduledInterstPool.end()) {
                    // cancel scheduled interest
                    ns3::Simulator::Cancel(m_scheduledInterstPool[data.getName()]);
                    // delete interest from m_scheduledInterstPool
                    m_scheduledInterstPool.erase(m_scheduledInterstPool.find(data.getName()));

                    NFD_LOG_DEBUG(data.getName() << ", cancel scheduled interest.");
                }

                ndn::Location myLocation(0, 0);
                ndn::Location prevLocation(0, 0);
                // if there is destination location in prefix to location table
                //    destLocation =
                // else
                ndn::Location destLocation(0, 0);

                lp::LocationHeader lh;
                lh.setMyLocation(myLocation);
                lh.setPrevLocation(prevLocation);
                lh.setDestLocation(destLocation);

                data.setTag(make_shared<lp::LocationTag>(lp::LocationHeader(lh)));

                this->beforeSatisfyInterest(pitEntry, inFace, data);

                this->sendDataToAll(pitEntry, inFace, data);
            }

            void
            ClfStrategy::afterReceiveDataClf(const shared_ptr <pit::Entry> &pitEntry,
                                             const Face &inFace, const Data &data) {
                /* Cancellation of scehduled interest and updating of score should be done in Strategy::sendData() or Strategy::sendDataToAll() instead of here
                 * */

                NFD_LOG_DEBUG("afterReceiveData pitEntry=" << pitEntry->getName() <<
                                                           " inFace=" << inFace.getId() << " data=" << data.getName());

                const fib::Entry &fibEntry = this->lookupFib(*pitEntry);
                const fib::NextHopList &nexthops = fibEntry.getNextHops();
                const Interest &interest = pitEntry->getInterest();

                // get the name of interest without the last part, version number
                ndn::Name namePrefix = interest.getName();

                // someone already responded with the data, cancel our scheduled interest
                if (m_scheduledInterstPool.find(data.getName()) != m_scheduledInterstPool.end()) {
                    // cancel scheduled interest
                    ns3::Simulator::Cancel(m_scheduledInterstPool[data.getName()]);
                    // delete interest from m_scheduledInterstPool
                    m_scheduledInterstPool.erase(m_scheduledInterstPool.find(data.getName()));

                    NFD_LOG_DEBUG(data.getName() << ", cancel scheduled interest.");
                }


                // get prefix announcement tag
                auto paTag = data.getTag<lp::PrefixAnnouncementTag>();
                const ndn::PrefixAnnouncement &pa = *paTag->get().getPrefixAnn();
                NFD_LOG_DEBUG("Data has prefix announcement: " << pa.getAnnouncedName().toUri());

                // get location tag
                auto locationTag = data.getTag<lp::LocationTag>();

                if (locationTag == nullptr) {
                    NFD_LOG_DEBUG("Data packet does not contain Location tag.");
                    ndn::Location myLocation(0, 0);
                    ndn::Location prevLocation(0, 0);
                    // if there is destination location in prefix to location table
                    //    destLocation =
                    // else
                    ndn::Location destLocation(0, 0);

                    lp::LocationHeader lh;
                    lh.setMyLocation(myLocation);
                    lh.setPrevLocation(prevLocation);
                    lh.setDestLocation(destLocation);

                    data.setTag(make_shared<lp::LocationTag>(lp::LocationHeader(lh)));
                } else {
                    if (paTag != nullptr) {
                        NFD_LOG_DEBUG("Data packet contains Location tag and PA tag.");
                        ndn::Name annPrefix = pa.getAnnouncedName();
                        ndn::Location dl = locationTag->get().getDestLocation();
                        shared_ptr <PrefixLocationEntry> plEntry = make_shared<PrefixLocationEntry>(
                                PrefixLocationEntry(annPrefix, dl));
                        //m_prefixLocation.insert(annPrefix, plEntry);
                        //m_measurements.updateLocationInfo(fibEntry, namePrefix, annPrefix, dl, 257); // use hardcoded faceid
                    }
                }

                if (m_scheduledInterstPool.find(data.getName()) ==
                    m_scheduledInterstPool.end()) { // only update score for forwarded interest, don't update score for interest yet to be forwarded
                    m_measurements.incrementDataCount(fibEntry, pa.getAnnouncedName(), 257);
                    m_measurements.updateScoreAndLocation(fibEntry, namePrefix, pa.getAnnouncedName(),
                                                          locationTag->get().getDestLocation(), 257);
                }

                this->beforeSatisfyInterest(pitEntry, inFace, data);

                this->sendDataToAll(pitEntry, inFace, data);
            }

            void
            ClfStrategy::afterReceiveInterestBroadcast(const Face &inFace, const Interest &interest,
                                                       const shared_ptr <pit::Entry> &pitEntry) {

                auto locationTag = interest.getTag<lp::LocationTag>();
                ndn::Location ml;
                ndn::Location pl;

                if (locationTag != nullptr) {
                    ml = locationTag->get().getMyLocation();
                    pl = locationTag->get().getPrevLocation();

                    NFD_LOG_DEBUG("MyLocation: " << ml.getLatitude() << ", " << ml.getLongitude());
                    NFD_LOG_DEBUG("PrevLocation: " << pl.getLatitude() << ", " << pl.getLongitude());
                }

                // get forwarding information
                const fib::Entry &fibEntry = this->lookupFib(*pitEntry);
                const fib::NextHopList &nexthops = fibEntry.getNextHops();

                // forward interest
                bool isProducer = false;
                for (const auto &nexthop: nexthops) {
                    Face &outFace = nexthop.getFace();
                    if (outFace.getLinkType() == ndn::nfd::LINK_TYPE_POINT_TO_POINT) {
                        isProducer = true;
                    }
                }

                for (const auto &nexthop: nexthops) {
                    Face &outFace = nexthop.getFace();
                    if (isProducer && outFace.getLinkType() == ndn::nfd::LINK_TYPE_POINT_TO_POINT) {
                        NFD_LOG_DEBUG(interest << " from=" << inFace.getId()
                                               << " pitEntry-to=" << outFace.getId() << ", outface link type: "
                                               << outFace.getLinkType());
                        this->sendInterest(pitEntry, outFace, interest);
                        break;
                    }

                    if (!isProducer && outFace.getLinkType() == ndn::nfd::LINK_TYPE_AD_HOC) {
                        NFD_LOG_DEBUG(interest << " from=" << inFace.getId()
                                               << " pitEntry-to=" << outFace.getId() << ", outface link type: "
                                               << outFace.getLinkType());
                        forwardInterest(interest, pitEntry, &outFace);
                        //this->sendInterest(pitEntry, outFace, interest);
                        break;
                    }
                }
            }

            void
            ClfStrategy::forwardInterest(const Interest &interest,
                                         const shared_ptr <pit::Entry> &pitEntry,
                    //TODO: FaceInfo* info,
                                         Face *outFace) {
                //std::cout << "Forwarding scheduled interest: " << interest;
                NFD_LOG_DEBUG("Forwarding scheduled interest: " << interest);

                const fib::Entry &fibEntry = this->lookupFib(*pitEntry);

                // get the name of interest without the last part, version number
                ndn::Name namePrefix = interest.getName().getPrefix(interest.getName().size() - 1);

                m_measurements.incrementInterestCount(fibEntry, namePrefix, outFace->getId());

                // send the interest
                this->sendInterest(pitEntry, *outFace, interest);

                // remove the interest from the pool, because it has been sent.
                m_scheduledInterstPool.erase(interest.getName());

                // TODO: update the score of this prefix
                //info->incrementInterest();
            }

            void
            ClfStrategy::afterReceiveInterestVndn(const Face &inFace, const Interest &interest,
                                                  const shared_ptr <pit::Entry> &pitEntry) {
                // if interest hop count > 5 drop it
                auto hopCountTag = interest.getTag<lp::HopCountTag>();
                if (hopCountTag != nullptr) {
                    int hopCount = *hopCountTag;

                    if (hopCount > 5) {
                        NFD_LOG_DEBUG("Hopcount greater than 5, dropping interest.");
                        return;
                    }
                }

                // if we already received this interest and put it in the pool
                if (m_scheduledInterstPool.find(interest.getName()) != m_scheduledInterstPool.end()) {
                    // cancel scheduled interest
                    ns3::Simulator::Cancel(m_scheduledInterstPool[interest.getName()]);
                    // delete interest from m_scheduledInterstPool
                    m_scheduledInterstPool.erase(m_scheduledInterstPool.find(interest.getName()));

                    NFD_LOG_DEBUG(interest.getName() << ", cancel scheduled interest.");

                    return;
                }

                // get location information
                auto locationTag = interest.getTag<lp::LocationTag>();
                ndn::Location ml;
                ndn::Location pl;
                ndn::Location dl;

                if (locationTag != nullptr) {
                    ml = locationTag->get().getMyLocation();
                    pl = locationTag->get().getPrevLocation();
                    dl = locationTag->get().getDestLocation();
                    NFD_LOG_DEBUG("MyLocation: " << ml.getLatitude() << ", " << ml.getLongitude());
                    NFD_LOG_DEBUG("PrevLocation: " << pl.getLatitude() << ", " << pl.getLongitude());
                    NFD_LOG_DEBUG("DestLocation: " << dl.getLatitude() << ", " << dl.getLongitude());
                } else {
                    NFD_LOG_DEBUG(interest.getName() << ", Consumer node: LocationHeader not found in the interest");
                    // if it is the consumer node, only then it is not going to have location header
                    // need to tag the interest with destLocaton if available
                    ndn::Location myLocation(0, 0);
                    ndn::Location prevLocation(0, 0);
                    // if there is destination location in prefix to location table
                    //    destLocation =
                    // else
                    ndn::Location destLocation(0, 0);

                    lp::LocationHeader lh;
                    lh.setMyLocation(myLocation);
                    lh.setPrevLocation(prevLocation);
                    lh.setDestLocation(destLocation);

                    NFD_LOG_DEBUG("Adding Location Header to the interst: " << interest.getName());

                    interest.setTag(make_shared<lp::LocationTag>(lh));

                    /* setting the tag is not needed for vndn algorithm, but had to add, so that same transport service can be used by all the strategies.*/

                    afterReceiveInterestBroadcast(inFace, interest, pitEntry);
                    return;
                }

                double timer = 0;
                double distanceFromMetoPrev = 0;
                // calculate timer based on ml and pl
                if ((ml.getLongitude() != 0 or ml.getLatitude() != 0) and
                    (pl.getLatitude() != 0 or pl.getLongitude() != 0)) {
                    //distanceFromMetoPrev = calculateDistance(pl.getLatitude(), pl.getLongitude(),
                    //                                         ml.getLatitude(), ml.getLongitude());

                    distanceFromMetoPrev = calculateDistanceEuclid(pl.getLongitude(), pl.getLatitude(),
                                                                   ml.getLongitude(), ml.getLatitude());

                    // shorter the distance longer the wait
                    timer = 1 / distanceFromMetoPrev;

                    // make it bigger
                    timer = timer * 100 * 1000;
                    timer = timer + (double) (rand() % 10 + 1);
                } else {
                    // Don't have location info, just broadcast
                    NFD_LOG_DEBUG("Location info is not available. Broadcast interest: " << interest);
                    afterReceiveInterestBroadcast(inFace, interest, pitEntry);
                    return;
                }

                // get forwarding information
                const fib::Entry &fibEntry = this->lookupFib(*pitEntry);
                const fib::NextHopList &nexthops = fibEntry.getNextHops();

                // is this node producer?
                bool isProducer = false;
                for (const auto &nexthop: nexthops) {
                    Face &outFace = nexthop.getFace();
                    if (outFace.getLinkType() == ndn::nfd::LINK_TYPE_POINT_TO_POINT) {
                        isProducer = true;
                    }
                }

                NFD_LOG_DEBUG("DistanceFromMeToPrev = " << distanceFromMetoPrev << ", Timer = " << timer << "us.");

                // forward interest
                for (const auto &nexthop: nexthops) {
                    Face *outFace = &nexthop.getFace();
                    if (isProducer && outFace->getLinkType() == ndn::nfd::LINK_TYPE_POINT_TO_POINT) {
                        NFD_LOG_DEBUG(interest << ", Producer Node: from=" << inFace.getId()
                                               << " pitEntry-to=" << outFace->getId() << ", outface link type: "
                                               << outFace->getLinkType());

                        this->sendInterest(pitEntry, *outFace, interest);
                        break;
                    }

                    if (!isProducer && outFace->getLinkType() == ndn::nfd::LINK_TYPE_AD_HOC) {
                        NFD_LOG_DEBUG(interest << ", Intermediate Node: from=" << inFace.getId()
                                               << " pitEntry-to=" << outFace->getId() << ", outface link type: "
                                               << outFace->getLinkType() << ", scheduled after " << timer << "ms.");

                        //ndn::EventId eventId = scheduler::schedule(waitingTime,
                        //                          [this, &interest, pitEntry, &outFace] {forwardInterest(interest, pitEntry,
                        /*info,*/ //outFace);});
                        ns3::EventId eventId = ns3::Simulator::Schedule(ns3::MicroSeconds(timer),
                                                                        &ClfStrategy::forwardInterest, this, interest,
                                                                        pitEntry, outFace);
                        //this->sendInterest(pitEntry, outFace, interest);
                        m_scheduledInterstPool[interest.getName()] = eventId;

                        break;
                    }
                }
            }

            void
            ClfStrategy::afterReceiveInterestNavigo(const Face &inFace, const Interest &interest,
                                                    const shared_ptr <pit::Entry> &pitEntry) {
                // if we already received this interest and put it in the pool
                if (m_scheduledInterstPool.find(interest.getName()) != m_scheduledInterstPool.end()) {
                    // cancel scheduled interest
                    ns3::Simulator::Cancel(m_scheduledInterstPool[interest.getName()]);
                    // delete interest from m_scheduledInterstPool
                    m_scheduledInterstPool.erase(m_scheduledInterstPool.find(interest.getName()));

                    NFD_LOG_DEBUG(interest << ", cancel scheduled interest.");

                    // drop the current interest
                    return;
                }

                // detect duplicate Nonce in PIT entry
                int dnw = findDuplicateNonce(*pitEntry, interest.getNonce(), inFace);
                bool hasDuplicateNonceInPit = dnw != DUPLICATE_NONCE_NONE;
                NFD_LOG_DEBUG(dnw << ", " << hasDuplicateNonceInPit);
                if (inFace.getLinkType() == ndn::nfd::LINK_TYPE_POINT_TO_POINT) {
                    // for p2p face: duplicate Nonce from same incoming face is not loop
                    hasDuplicateNonceInPit = hasDuplicateNonceInPit && !(dnw & fw::DUPLICATE_NONCE_IN_SAME);
                }

                // handle unscheduled looped interest
                if (hasDuplicateNonceInPit) {
                    // drop the interest if it is looping
                    NFD_LOG_DEBUG("Looped interest: " << interest);
                    //return; // TODO: It is dropping all the packet. Need to fix it.
                }

                // get location information
                auto locationTag = interest.getTag<lp::LocationTag>();
                ndn::Location ml;
                ndn::Location pl;
                ndn::Location dl;

                if (locationTag != nullptr) {
                    ml = locationTag->get().getMyLocation();
                    pl = locationTag->get().getPrevLocation();
                    dl = locationTag->get().getDestLocation();

                    NFD_LOG_DEBUG("MyLocation: " << ml.getLatitude() << "," << ml.getLongitude() << "; PrevLocation: "
                                                 << pl.getLatitude() << "," << pl.getLongitude() << "; DestLocation: "
                                                 << dl.getLatitude() << "," << dl.getLongitude());
                } else {
                    NFD_LOG_DEBUG("Consumer node: LocationHeader not found in the interest");
                    // if it is the consumer node, only then it is not going to have location header
                    // need to tag the interest with destLocaton if available
                    ndn::Location myLocation(0, 0);
                    ndn::Location prevLocation(0, 0);
                    // if there is destination location in prefix to location table
                    //    destLocation =
                    // else
                    ndn::Location destLocation(350, 0); // hardcode for testing

                    lp::LocationHeader lh;
                    lh.setMyLocation(myLocation);
                    lh.setPrevLocation(prevLocation);
                    lh.setDestLocation(destLocation);

                    NFD_LOG_DEBUG("Adding Location Header to the interst: " << interest.getName());

                    interest.setTag(make_shared<lp::LocationTag>(lh));
                    afterReceiveInterestBroadcast(inFace, interest, pitEntry);
                    return;
                }

                // calculate timer based on location, NAVIGO algorithm
                double timer = 0;
                double locationScore = 0;
                double distanceFromMeToDest = 0.0;
                double distanceFromPrevToDest = 0.0;
                if ((dl.getLongitude() != 0 or dl.getLatitude() != 0) and
                    (ml.getLongitude() != 0 or ml.getLatitude() != 0) and
                    (pl.getLatitude() != 0 or pl.getLongitude() != 0)) {
                    distanceFromMeToDest = calculateDistanceEuclid(dl.getLatitude(), dl.getLongitude(),
                                                                   ml.getLatitude(), ml.getLongitude());
                    distanceFromPrevToDest = calculateDistanceEuclid(pl.getLatitude(), pl.getLongitude(),
                                                                     dl.getLatitude(), dl.getLongitude());
                    if (distanceFromMeToDest < distanceFromPrevToDest) {
                        NFD_LOG_DEBUG("DistanceFromMeToDest = " << distanceFromMeToDest
                                                                << " is smaller than DistanceFromPrevToDest = "
                                                                << distanceFromPrevToDest << ". Calculating timer.");

                        // calculate timer based on ml, pl and dl
                        timer = distanceFromMeToDest;
                    } else {
                        NFD_LOG_DEBUG("DistanceFromMeToDest = " << distanceFromMeToDest
                                                                << " is greater than DistanceFromPrevToDest = "
                                                                << distanceFromPrevToDest
                                                                << ". Stop processing interest.");
                        return;
                    }
                } else {
                    if (true) { /* TODO: if prefix location table has destination location*/
                        //Location destLocation = getLocation(interest.getName());
                        //ndn::Location dl(250, 0); // hardcode for testing, need to do the above
                        distanceFromMeToDest = calculateDistanceEuclid(dl.getLatitude(), dl.getLongitude(),
                                                                       ml.getLatitude(), ml.getLongitude());
                        distanceFromPrevToDest = calculateDistanceEuclid(pl.getLatitude(), pl.getLongitude(),
                                                                         dl.getLatitude(), dl.getLongitude());

                        if (distanceFromMeToDest < distanceFromPrevToDest) {
                            // calculate timer based on ml, pl and dl
                            timer = distanceFromMeToDest;
                        } else {
                            NFD_LOG_DEBUG("DistanceFromMeToDest = " << distanceFromMeToDest
                                                                    << " is greater than DistanceFromPrevToDest = "
                                                                    << distanceFromPrevToDest
                                                                    << ". Stop processing interest.");
                            return;
                        }
                    } else {
                        // Don't have location info, just broadcast
                        NFD_LOG_DEBUG("Location info is not available. Broadcast interest: " << interest);
                        afterReceiveInterestBroadcast(inFace, interest, pitEntry);
                        return;
                    }
                }

                // to make timer smaller
                timer /= 10;

                NFD_LOG_DEBUG("DistanceFromMeToDest = " << distanceFromMeToDest << ", DistanceFromPrevToDest = "
                                                        << distanceFromPrevToDest << ", Timer = " << timer);

                // get forwarding information
                const fib::Entry &fibEntry = this->lookupFib(*pitEntry);
                const fib::NextHopList &nexthops = fibEntry.getNextHops();

                // is this node producer?
                bool isProducer = false;
                for (const auto &nexthop: nexthops) {
                    Face &outFace = nexthop.getFace();
                    if (outFace.getLinkType() == ndn::nfd::LINK_TYPE_POINT_TO_POINT) {
                        isProducer = true;
                    }
                }

                // forward interest
                for (const auto &nexthop: nexthops) {
                    Face *outFace = &nexthop.getFace();
                    if (isProducer && outFace->getLinkType() == ndn::nfd::LINK_TYPE_POINT_TO_POINT) {
                        NFD_LOG_DEBUG(interest << "Produces Node: from=" << inFace.getId()
                                               << " pitEntry-to=" << outFace->getId() << ", outface link type: "
                                               << outFace->getLinkType());

                        this->sendInterest(pitEntry, *outFace, interest);
                        break;
                    }

                    if (!isProducer && outFace->getLinkType() == ndn::nfd::LINK_TYPE_AD_HOC) {
                        NFD_LOG_DEBUG(interest << "Intermediate Node: from=" << inFace.getId()
                                               << " pitEntry-to=" << outFace->getId() << ", outface link type: "
                                               << outFace->getLinkType() << ", scheduled after " << timer << " ms.");

                        //ndn::EventId eventId = scheduler::schedule(waitingTime,
                        //                          [this, &interest, pitEntry, &outFace] {forwardInterest(interest, pitEntry,
                        /*info,*/ //outFace);});
                        ns3::EventId eventId = ns3::Simulator::Schedule(ns3::MilliSeconds(timer),
                                                                        &ClfStrategy::forwardInterest, this, interest,
                                                                        pitEntry, outFace);
                        m_scheduledInterstPool[interest.getName()] = eventId;
                        //this->sendInterest(pitEntry, outFace, interest);

                        break;
                    }
                }
            }

            void
            ClfStrategy::afterReceiveInterestClf(const Face &inFace, const Interest &interest,
                                                 const shared_ptr <pit::Entry> &pitEntry) {
                NFD_LOG_DEBUG(interest.getName().toUri());

                // if we already received this interest and put it in the pool (don't think this codeblock is necessary since we are handling looped interest, same interest should always go to looped interest path))
                if (m_scheduledInterstPool.find(interest.getName()) != m_scheduledInterstPool.end()) {
                    // cancel scheduled interest
                    ns3::Simulator::Cancel(m_scheduledInterstPool[interest.getName()]);
                    // delete interest from m_scheduledInterstPool
                    m_scheduledInterstPool.erase(m_scheduledInterstPool.find(interest.getName()));

                    // delete the corresponding PIT entry
                    this->rejectPendingInterest(pitEntry);

                    NFD_LOG_DEBUG(interest << ", cancel scheduled interest. Drop current interest.");

                    // drop the current interest
                    return;
                }

                // get forwarding information
                const fib::Entry &fibEntry = this->lookupFib(*pitEntry);
                const fib::NextHopList &nexthops = fibEntry.getNextHops();

                // is this node producer?
                bool isProducer = false;
                double centralityScore = 0;
                ndn::Location dlFromTable(0, 0);
                for (const auto &nexthop: nexthops) {
                    Face &outFace = nexthop.getFace();
                    if (outFace.getLinkType() == ndn::nfd::LINK_TYPE_POINT_TO_POINT) {
                        isProducer = true;
                    }

                    // the producer node will not have ad hoc face in nexhop list
                    if (outFace.getLinkType() == ndn::nfd::LINK_TYPE_AD_HOC) {
                        // get centrality score for the interest name and outface
                        centralityScore = getCentralityScore(pitEntry, outFace.getId());
                        dlFromTable = getDestLocation(pitEntry, outFace.getId());
                    }
                }

                // get location information
                auto locationTag = interest.getTag<lp::LocationTag>();
                ndn::Location ml;
                ndn::Location pl;
                ndn::Location dl;
                if (locationTag != nullptr) {
                    ml = locationTag->get().getMyLocation();
                    pl = locationTag->get().getPrevLocation();
                    dl = locationTag->get().getDestLocation();

                    NFD_LOG_DEBUG("MyLocation: " << ml.getLatitude() << "," << ml.getLongitude() << "; PrevLocation: "
                                                 << pl.getLatitude() << "," << pl.getLongitude() << "; DestLocation: "
                                                 << dl.getLatitude() << "," << dl.getLongitude());
                } else {  // if it is the consumer node, only then it is not going to have location header
                    ndn::Location myLocation(0, 0);
                    ndn::Location prevLocation(0, 0);
                    ndn::Location destLocation;
                    if ((dlFromTable.getLongitude() != 0) or (dlFromTable.getLatitude() != 0)) {
                        destLocation = dlFromTable;
                        NFD_LOG_DEBUG("Consumer node: LocationHeader not found in the interest. But DestLocation for "
                                              << interest.getName().toUri() << " is found in prefix location tree: "
                                              << destLocation.getLongitude() << ", " << destLocation.getLatitude());
                    } else {
                        NFD_LOG_DEBUG(
                                "Consumer node: LocationHeader not found in the interest. And also not found in prefix location tree.");
                        destLocation.setLongitude(0);
                        destLocation.setLatitude(0);
                    }

                    lp::LocationHeader lh;
                    lh.setMyLocation(myLocation);
                    lh.setPrevLocation(prevLocation);
                    lh.setDestLocation(destLocation);

                    NFD_LOG_DEBUG("Consumer node. Added Location Header to the interst: " << interest.getName()
                                                                                          << ". Now broadcasting.");

                    interest.setTag(make_shared<lp::LocationTag>(lh));
                    afterReceiveInterestBroadcast(inFace, interest, pitEntry);
                    return;
                }

                // calculate weight based on location and centrlaity score according to CLF algorithm
                double weight = 0;
                double locationScore = 0;
                double distanceFromMeToDest = 0.0;
                double distanceFromPrevToDest = 0.0;
                if ((dl.getLongitude() != 0 or dl.getLatitude() != 0) and
                    (ml.getLongitude() != 0 or ml.getLatitude() != 0) and
                    (pl.getLatitude() != 0 or pl.getLongitude() != 0)) // if location info is available in the interest
                {
                    locationScore = getLocationScore(pl, dl, ml);

                    weight = ALPHA * (1 - locationScore) + (1 - ALPHA) * centralityScore;

                    distanceFromMeToDest = calculateDistanceEuclid(dl.getLatitude(), dl.getLongitude(),
                                                                   ml.getLatitude(), ml.getLongitude());
                    distanceFromPrevToDest = calculateDistanceEuclid(pl.getLatitude(), pl.getLongitude(),
                                                                     dl.getLatitude(), dl.getLongitude());

                    NFD_LOG_DEBUG(
                            "DestLocation information available in Interest. Calculating weight using both location and centrality score. ");
                } else {
                    if ((dlFromTable.getLongitude() != 0) or
                        (dlFromTable.getLatitude() != 0)) { // if location info is available in the meausrement table
                        ndn::Location dl = dlFromTable;
                        locationScore = getLocationScore(pl, dl, ml);

                        NFD_LOG_DEBUG(
                                "DestLocation information is not available in Interest, but available in prefix-location table: ("
                                        << dlFromTable.getLatitude() << "," << dlFromTable.getLongitude()
                                        << "). Calculating score using both location and centrality score.");

                        weight = ALPHA * (1 - locationScore) + (1 - ALPHA) * centralityScore;
                    } else { // just use centality to calculate the wegiht
                        NFD_LOG_DEBUG(
                                "DestLocation information is neither available in Interest nor in prefix-location table. Calculating weight only with centrality score.");
                        weight = (1 - ALPHA) * centralityScore;
                    }
                }

                double timer = 0;
                int finalTimer = 0;
                if (weight <=
                    0) { // it means both location score and centrality score is zero. This node just joined the network. So to bootstrap the scores it should broadcast
                    timer = 0;
                } else {
                    // higher the weight, lower the timer (waiting time))
                    timer = 1 / weight;

                    // add a random number between 1-10 incase the timer conflicts with another node
                    //timer = timer + (double)(rand() % 10 + 1);

                    // select a random number between (timer * 0.5) to (timer * 1.5) incase the timer conflicts with another node
                    /*std::random_device rd; // obtain a random number from hardware
                    std::mt19937 eng(rd()); // seed the generator
                    double lower_bound = timer * 0.5;
                    double upper_bound = timer * 1.5;
                    std::uniform_int_distribution<> distr(lower_bound, upper_bound);
                    double randomTimer = distr(eng);*/

                    std::random_device rd; // obtain a random number from hardware
                    std::mt19937 eng(rd()); // seed the generator
                    double lower_bound = timer * 0.5;
                    double upper_bound = timer * 1.5;
                    std::uniform_real_distribution<double> unif(lower_bound, upper_bound);
                    //std::default_random_engine re;
                    double randomTimer = unif(eng);

                    // set final timer to randomTimer and convert it to us.
                    finalTimer = round(randomTimer * 1000);

                    NFD_LOG_DEBUG("Calculating timer. Timer = " << timer << " ms, lower_bound = " << lower_bound
                                                                << ", upper_bound = " << upper_bound
                                                                << ", randomTimer = " << randomTimer
                                                                << ", finalTimer = " << finalTimer << " us.");
                }

                NFD_LOG_DEBUG("Calculated values: DistanceFromMeToDest = " << distanceFromMeToDest
                                                                           << "; DistanceFromPrevToDest = "
                                                                           << distanceFromPrevToDest
                                                                           << ". LocationScore = " << locationScore
                                                                           << "; CentralityScore = " << centralityScore
                                                                           << "; Weight = " << weight << "; Timer = "
                                                                           << timer << " ms; Final Timer = "
                                                                           << finalTimer << " us.");

                //timer = finalTimer;// * 10; // multiplying avoids collision

                // forward interest
                for (const auto &nexthop: nexthops) {
                    Face *outFace = &nexthop.getFace();
                    if (isProducer && outFace->getLinkType() == ndn::nfd::LINK_TYPE_POINT_TO_POINT) {
                        NFD_LOG_DEBUG(interest << "Producer Node: from=" << inFace.getId()
                                               << " pitEntry-to=" << outFace->getId() << ", outface link type: "
                                               << outFace->getLinkType());

                        // get the name of interest without the last part, version number
                        ndn::Name namePrefix = interest.getName().getPrefix(interest.getName().size() - 1);

                        // increment interest count for incoming face becuase that is the ad-hoc face
                        m_measurements.incrementInterestCount(fibEntry, namePrefix, inFace.getId());

                        this->sendInterest(pitEntry, *outFace, interest);
                        break;
                    }

                    if (!isProducer && outFace->getLinkType() == ndn::nfd::LINK_TYPE_AD_HOC) {
                        NFD_LOG_DEBUG("Intermediate Node: Interest= " << interest.getName().toUri() << " received from="
                                                                      << inFace.getId()
                                                                      << " pitEntry-to=" << outFace->getId()
                                                                      << ", outface link type: "
                                                                      << outFace->getLinkType() << ", scheduled after "
                                                                      << finalTimer << " us.");

                        //ndn::EventId eventId = scheduler::schedule(waitingTime,
                        //                          [this, &interest, pitEntry, &outFace] {forwardInterest(interest, pitEntry,
                        /*info,*/ //outFace);});
                        ns3::EventId eid = ns3::Simulator::Schedule(ns3::MicroSeconds(finalTimer),
                                                                    &ClfStrategy::forwardInterest, this, interest,
                                                                    pitEntry, outFace);

                        //this->sendInterest(pitEntry, outFace, interest);
                        m_scheduledInterstPool[interest.getName()] = eid;
                        break;
                    }
                }
            }

            void
            ClfStrategy::afterReceiveNack(const Face &inFace, const lp::Nack &nack,
                                          const shared_ptr <pit::Entry> &pitEntry) {
                //this->processNack(inFace, nack, pitEntry);
            }

            double
            ClfStrategy::getCentralityScore(const shared_ptr <pit::Entry> &pitEntry, FaceId faceId) {
                NFD_LOG_DEBUG("Getting centrality score for face " << faceId << ", name prefix: "
                                                                   << pitEntry->getInterest().getName().toUri());

                const Interest &interest = pitEntry->getInterest();
                const fib::Entry &fibEntry = this->lookupFib(*pitEntry);

                // get the name of interest without the last part, version number
                ndn::Name namePrefix = interest.getName().getPrefix(interest.getName().size() - 1);

                return m_measurements.getCs(fibEntry, faceId, namePrefix);
            }

            ndn::Location
            ClfStrategy::getDestLocation(const shared_ptr <pit::Entry> &pitEntry, FaceId faceId) {
                const Interest &interest = pitEntry->getInterest();
                const fib::Entry &fibEntry = this->lookupFib(*pitEntry);

                // get the name of interest without the last part, version number
                ndn::Name namePrefix = interest.getName().getPrefix(interest.getName().size() - 1);

                return m_measurements.getDestLocationInfo(fibEntry, namePrefix, faceId);
            }

            double
            ClfStrategy::calculateDistanceEuclid(double x1, double y1, double x2, double y2) {
                double x = x1 - x2; //calculating number to square in next step
                double y = y1 - y2;
                double dist;

                dist = pow(x, 2) + pow(y, 2);       //calculating Euclidean distance
                dist = sqrt(dist);

                return dist;
            }

/*
 * https://stackoverflow.com/questions/10198985/calculating-the-distance-between-2-latitudes-and-longitudes-that-are-saved-in-a
 */

            double
            ClfStrategy::calculateDistance(double lat1d, double lon1d,
                                           double lat2d, double lon2d) {
                double lat1r, lon1r, lat2r, lon2r, u, v;

                lat1r = deg2rad(lat1d);
                lon1r = deg2rad(lon1d);
                lat2r = deg2rad(lat2d);
                lon2r = deg2rad(lon2d);

                u = sin((lat2r - lat1r) / 2);
                v = sin((lon2r - lon1r) / 2);

                return 2.0 * earthRadiusKm * asin(sqrt(u * u + cos(lat1r) * cos(lat2r) * v * v));
            }

        } // namespace clf
    } // namespace fw
} // namespace nfd