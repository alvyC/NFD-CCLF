#include "clf-prefix-location-tree.hpp"

#include "core/logger.hpp"

namespace nfd {
    namespace fw {
        namespace clf {

            NFD_LOG_INIT(PrefixLocationTree);

            PrefixLocationEntry::PrefixLocationEntry(Name prefix, ndn::Location dl)
                    : m_name(prefix), m_destLocation(dl) {
            }

            void
            PrefixLocationEntry::addChild(shared_ptr <PrefixLocationEntry> child) {
                BOOST_ASSERT(!child->getParent());
                child->setParent(this->shared_from_this());
                m_children.push_back(std::move(child));
            }

            void
            PrefixLocationEntry::removeChild(shared_ptr <PrefixLocationEntry> child) {
                BOOST_ASSERT(child->getParent().get() == this);
                child->setParent(nullptr);
                m_children.remove(child);
            }

            PrefixLocationTree::PrefixLocationTree()
                    : m_nItems(0) {
            }

            PrefixLocationTree::const_iterator
            PrefixLocationTree::find(const Name &prefix) const {
                NFD_LOG_DEBUG("Lookup " << prefix << " in PrefixLocationTable");
                return m_prefixLocationTable.find(prefix);
            }

            shared_ptr <PrefixLocationEntry>
            PrefixLocationTree::findLongestPrefix(const Name &prefix) const {
                PrefixLocationTree::const_iterator entryIt = find(prefix);

                return entryIt->second;
            }

            void
            PrefixLocationTree::insert(const Name &prefix, shared_ptr <PrefixLocationEntry> entry) {
                auto prefixLocationIt = m_prefixLocationTable.find(prefix);

                NFD_LOG_DEBUG("Insert " << prefix << " to PrefixLocationTable");

                m_prefixLocationTable[prefix] = entry;
                // Name prefix exist, not sure what needs to be done. Right now just updating it with the new DestLocation
                if (prefixLocationIt != m_prefixLocationTable.end()) {
                    /*shared_ptr<PrefixLocationEntry> entry(prefixLocationIt->second);
                    m_prefixLocationTable[prefix] = entry;*/
                } else { // just increase the count for now, if it is new
                    m_nItems++;
                    // New name prefix
                    /*auto entry = make_shared<PrefixLocationTreeEntry>();

                    m_prefixLocationTable[prefix] = entry;
                    m_nItems++;

                    entry->setName(prefix);
                    auto routeIt = entry->insertRoute(route).first;

                    // Find prefix's parent
                    shared_ptr<PrefixLocationTreeEntry> parent = findParent(prefix);

                    // Add self to parent's children
                    if (parent != nullptr) {
                      parent->addChild(entry);
                    }

                    PrefixLocationTreeEntryList children = findDescendants(prefix);

                    for (const auto& child : children) {
                      if (child->getParent() == parent) {
                        // Remove child from parent and inherit parent's child
                        if (parent != nullptr) {
                          parent->removeChild(child);
                        }

                        entry->addChild(child);
                      }
                    }

                    // Register with face lookup table
                    m_faceMap[route.faceId].push_back(entry);

                    // do something after inserting an entry
                    afterInsertEntry(prefix);
                    afterAddRoute(PrefixLocationTreeRouteRef{entry, routeIt});
                */  }
            }

/*
void
PrefixLocationTree::erase(const Name& prefix, const Route& route)
{
  auto prefixLocationIt = m_prefixLocationTable.find(prefix);

  // Name prefix exists
  if (prefixLocationIt != m_prefixLocationTable.end()) {
    shared_ptr<PrefixLocationTreeEntry> entry = prefixLocationIt->second;
    auto routeIt = entry->findRoute(route);

    if (routeIt != entry->end()) {
      beforeRemoveRoute(PrefixLocationTreeRouteRef{entry, routeIt});

      auto faceId = route.faceId;
      entry->eraseRoute(routeIt);
      m_nItems--;

      // If this PrefixLocationTreeEntry no longer has this faceId, unregister from face lookup table
      if (!entry->hasFaceId(faceId)) {
        m_faceMap[faceId].remove(entry);
      }

      // If a PrefixLocationTreeEntry's route list is empty, remove it from the tree
      if (entry->getRoutes().empty()) {
        eraseEntry(prefixLocationIt);
      }
    }
  }
}
*/
        }
    }
}