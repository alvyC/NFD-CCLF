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

#ifndef NFD_DAEMON_FACE_ETHERNET_CHANNEL_HPP
#define NFD_DAEMON_FACE_ETHERNET_CHANNEL_HPP

#include "channel.hpp"
#include "pcap-helper.hpp"
#include "core/network-interface.hpp"

namespace nfd {
namespace face {

/**
 * \brief Class implementing Ethernet-based channel to create faces
 */
class EthernetChannel : public Channel
{
public:
  /**
   * \brief EthernetChannel-related error
   */
  class Error : public std::runtime_error
  {
  public:
    explicit
    Error(const std::string& what)
      : std::runtime_error(what)
    {
    }
  };

  /**
   * \brief Create an Ethernet channel on the given \p localEndpoint (network interface)
   *
   * To enable creation of faces upon incoming connections,
   * one needs to explicitly call EthernetChannel::listen method.
   */
  EthernetChannel(const NetworkInterfaceInfo& localEndpoint,
                  time::nanoseconds idleTimeout);

  bool
  isListening() const override
  {
    return m_isListening;
  }

  size_t
  size() const override
  {
    return m_channelFaces.size();
  }

  /**
   * \brief Create a unicast Ethernet face toward \p remoteEndpoint
   *
   * \param remoteEndpoint The remote Ethernet endpoint
   * \param persistency Persistency of the newly created face
   * \param onFaceCreated Callback to notify successful creation of the face
   * \param onConnectFailed Callback to notify errors
   */
  void
  connect(const ethernet::Address& remoteEndpoint,
          ndn::nfd::FacePersistency persistency,
          const FaceCreatedCallback& onFaceCreated,
          const FaceCreationFailedCallback& onConnectFailed);

  /**
   * \brief Start listening
   *
   * Enable listening on the local endpoint, waiting for incoming frames,
   * and creating a face when a frame is received from a new remote host.
   *
   * Faces created in this way will have on-demand persistency.
   *
   * \param onFaceCreated Callback to notify successful creation of a face
   * \param onFaceCreationFailed Callback to notify errors
   * \throw Error
   */
  void
  listen(const FaceCreatedCallback& onFaceCreated,
         const FaceCreationFailedCallback& onFaceCreationFailed);

private:
  void
  asyncRead(const FaceCreatedCallback& onFaceCreated,
            const FaceCreationFailedCallback& onReceiveFailed);

  void
  handleRead(const boost::system::error_code& error,
             const FaceCreatedCallback& onFaceCreated,
             const FaceCreationFailedCallback& onReceiveFailed);

  void
  processIncomingPacket(const uint8_t* packet, size_t length,
                        const ethernet::Address& sender,
                        const FaceCreatedCallback& onFaceCreated,
                        const FaceCreationFailedCallback& onReceiveFailed);

  std::pair<bool, shared_ptr<Face>>
  createFace(const ethernet::Address& remoteEndpoint,
             ndn::nfd::FacePersistency persistency);

  void
  updateFilter();

private:
  const NetworkInterfaceInfo m_localEndpoint;
  bool m_isListening;
  boost::asio::posix::stream_descriptor m_socket;
  PcapHelper m_pcap;
  std::map<ethernet::Address, shared_ptr<Face>> m_channelFaces;
  const time::nanoseconds m_idleFaceTimeout; ///< Timeout for automatic closure of idle on-demand faces

#ifdef _DEBUG
  /// number of frames dropped by the kernel, as reported by libpcap
  size_t m_nDropped;
#endif
};

} // namespace face
} // namespace nfd

#endif // NFD_DAEMON_FACE_ETHERNET_CHANNEL_HPP
