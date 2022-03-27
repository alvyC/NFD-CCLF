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

#ifndef NFD_DAEMON_FW_CLF_PREFIX_LOCATION_TREE_HPP
#define NFD_DAEMON_FW_CLF_PREFIX_LOCATION_TREE_HPP

#include "fw/strategy-info.hpp"
#include "table/measurements-accessor.hpp"

//#include <ndn-cxx/location.hpp>

#include <list>

namespace nfd {
    namespace fw {
        namespace clf {

            class PrefixLocationEntry : public std::enable_shared_from_this<PrefixLocationEntry> {
            public:
                PrefixLocationEntry(Name, ndn::Location);

                PrefixLocationEntry();

                void
                setName(const Name &prefix);

                const Name &
                getName() const;

                inline void
                setLocation(const ndn::Location &destLocation);

                inline const ndn::Location &
                getLocation() const;

                shared_ptr <PrefixLocationEntry>
                getParent() const;

                bool
                hasParent() const;

                void
                addChild(shared_ptr <PrefixLocationEntry> child);

                void
                removeChild(shared_ptr <PrefixLocationEntry> child);

                const std::list <shared_ptr<PrefixLocationEntry>> &
                getChildren() const;

                bool
                hasChildren() const;

                /*ndn::PrefixAnnouncement
                getPrefixAnnouncement(time::milliseconds minExpiration = 15_s,
                                      time::milliseconds maxExpiration = 1_h) const;
                */
            private:
                void
                setParent(shared_ptr <PrefixLocationEntry> parent);

            private:
                Name m_name;
                std::list <shared_ptr<PrefixLocationEntry>> m_children;
                shared_ptr <PrefixLocationEntry> m_parent;

                ndn::Location m_destLocation;
            };

            inline void
            PrefixLocationEntry::setName(const Name &prefix) {
                m_name = prefix;
            }

            inline const Name &
            PrefixLocationEntry::getName() const {
                return m_name;
            }

            inline void
            PrefixLocationEntry::setLocation(const ndn::Location &destLocation) {
                m_destLocation = destLocation;
            }

            inline const ndn::Location &
            PrefixLocationEntry::getLocation() const {
                return m_destLocation;
            }

            inline void
            PrefixLocationEntry::setParent(shared_ptr <PrefixLocationEntry> parent) {
                m_parent = parent;
            }

            inline shared_ptr <PrefixLocationEntry>
            PrefixLocationEntry::getParent() const {
                return m_parent;
            }

            inline const std::list <shared_ptr<PrefixLocationEntry>> &
            PrefixLocationEntry::getChildren() const {
                return m_children;
            }

            class PrefixLocationTree {
            public:
                typedef std::map <Name, shared_ptr<PrefixLocationEntry>> PrefixLocationTable;
                typedef PrefixLocationTable::const_iterator const_iterator;

                PrefixLocationTree();

                const_iterator
                find(const Name &prefix) const;

                shared_ptr <PrefixLocationEntry>
                findLongestPrefix(const Name &prefix) const;

                const_iterator
                begin() const;

                const_iterator
                end() const;

                size_t
                size() const;

                bool
                empty() const;

                shared_ptr <PrefixLocationEntry>
                findParent(const Name &prefix) const;

                std::list <shared_ptr<PrefixLocationEntry>>
                findDescendants(const Name &prefix) const;

                std::list <shared_ptr<PrefixLocationEntry>>
                findDescendantsForNonInsertedName(const Name &prefix) const;

                void
                insert(const Name &prefix, shared_ptr <PrefixLocationEntry> entry);

            private:
                PrefixLocationTable m_prefixLocationTable;

                size_t m_nItems;
            };

            inline PrefixLocationTree::const_iterator
            PrefixLocationTree::begin() const {
                return m_prefixLocationTable.begin();
            }

            inline PrefixLocationTree::const_iterator
            PrefixLocationTree::end() const {
                return m_prefixLocationTable.end();
            }

            inline size_t
            PrefixLocationTree::size() const {
                return m_nItems;
            }

            inline bool
            PrefixLocationTree::empty() const {
                return m_prefixLocationTable.empty();
            }

        }
    }
}

#endif