/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil -*- */
/**
 * Copyright (C) 2018 Regents of the University of California.
 * @author: Jeff Thompson <jefft0@remap.ucla.edu>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version, with the additional exemption that
 * compiling, linking, and/or using OpenSSL is allowed.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 * A copy of the GNU Lesser General Public License is in the file COPYING.
 */

#include <ndn-cpp/delegation-set.hpp>
#include <cnl-cpp/generalized-object/generalized-object-stream-handler.hpp>

using namespace std;
using namespace ndn;
using namespace ndntools;
using namespace ndn::func_lib;

namespace cnl_cpp {

GeneralizedObjectStreamHandler::Impl::Impl
  (int pipelineSize,
   const OnSequencedGeneralizedObject& onSequencedGeneralizedObject)
: pipelineSize_(pipelineSize),
  onSequencedGeneralizedObject_(onSequencedGeneralizedObject), namespace_(0),
  latestNamespace_(0), producedSequenceNumber_(-1)
{
}

void
GeneralizedObjectStreamHandler::Impl::addObject
  (const ndn::Blob& object, const std::string& contentType)
{
  if (!namespace_)
    throw runtime_error
      ("GeneralizedObjectStreamHandler.addObject: The Namespace is not set");

  ++producedSequenceNumber_;
  Namespace& sequenceNamespace =
    (*namespace_)[Name::Component::fromSequenceNumber(producedSequenceNumber_)];
  generalizedObjectHandler_.setObject(sequenceNamespace, object, contentType);
}

void
GeneralizedObjectStreamHandler::Impl::onNamespaceSet(Namespace* nameSpace)
{
  // Store getNamespace() in impl_. We do this instead of keeping a pointer to
  // this outer Handler object since it might be destroyed.
  namespace_ = nameSpace;
  latestNamespace_ = &(*namespace_)[getNAME_COMPONENT_LATEST()];

  namespace_->addOnObjectNeeded
    (bind(&GeneralizedObjectStreamHandler::Impl::onObjectNeeded,
          shared_from_this(), _1, _2, _3));
  namespace_->addOnStateChanged
    (bind(&GeneralizedObjectStreamHandler::Impl::onStateChanged,
          shared_from_this(), _1, _2, _3, _4));
}

bool
GeneralizedObjectStreamHandler::Impl::onObjectNeeded
  (Namespace& nameSpace, Namespace& neededNamespace, uint64_t callbackId)
{
  if (&neededNamespace == namespace_) {
    // Assume this is called by a consumer. Fetch the first _latest packet.
    latestNeeded();
    return true;
  }

  if (&neededNamespace == latestNamespace_ && producedSequenceNumber_ >= 0) {
    // Produce the _latest Data packet.
    Name sequenceName = Name(namespace_->getName()).append
      (Name::Component::fromSequenceNumber(producedSequenceNumber_));
    DelegationSet delegations;
    delegations.add(1, sequenceName);

    Namespace& versionedLatest =
      (*latestNamespace_)[Name::Component::fromVersion((uint64_t)ndn_getNowMilliseconds())];
    MetaInfo metaInfo;
    // Debug: Get the correct freshness period.
    metaInfo.setFreshnessPeriod(1000);
    versionedLatest.setNewDataMetaInfo(metaInfo);
    // Make the Data packet and reply to outstanding Interests.
    versionedLatest.serializeObject(ptr_lib::make_shared<BlobObject>
      (delegations.wireEncode()));

    return true;
  }

  return false;
}

void
GeneralizedObjectStreamHandler::Impl::onStateChanged
  (Namespace& nameSpace, Namespace& changedNamespace, NamespaceState state,
   uint64_t callbackId)
{
  if (!(changedNamespace.getName().size() ==
          latestNamespace_->getName().size() + 1 &&
        state == NamespaceState_OBJECT_READY &&
        changedNamespace.getName()[-1].isVersion()))
    // Not a versioned _latest, so ignore.
    return;

  // Decode the _latest packet to get the target to fetch.
  DelegationSet delegations;
  delegations.wireDecode
    (ptr_lib::dynamic_pointer_cast<BlobObject>(changedNamespace.getObject())->getBlob());
  if (delegations.size() <= 0)
    return;
  const Name& targetName = delegations.get(0).getName();
  if (!(namespace_->getName().isPrefixOf(targetName)) &&
          targetName.size() == namespace_->getName().size() + 1)
    // TODO: Report an error that the target name is outside of the handler's prefix.
    return;
  Namespace& targetNamespace = (*namespace_)[targetName];

  if (!targetNamespace.getObject()) {
    // Fetch the target.
    int targetSequenceNumber = (int)targetName[-1].toSequenceNumber();
    // Debug: Do we have to attach a new handler for each sequence number?
    ptr_lib::shared_ptr<GeneralizedObjectHandler> generalizedObjectHandler =
      ptr_lib::make_shared<GeneralizedObjectHandler>
        (bind(&GeneralizedObjectStreamHandler::Impl::onGeneralizedObject,
              shared_from_this(), _1, _2, targetSequenceNumber));
    targetNamespace.setHandler(generalizedObjectHandler).objectNeeded();
  }
  
  // Schedule to fetch the next _latest packet.
  int freshnessPeriod =
    changedNamespace.getData()->getMetaInfo().getFreshnessPeriod();
  if (freshnessPeriod < 0)
    // No freshness period. We don't expect this.
    return;
  latestNamespace_->getFace_()->callLater
    (freshnessPeriod / 2,
     bind(&GeneralizedObjectStreamHandler::Impl::latestNeeded, shared_from_this()));
}

void
GeneralizedObjectStreamHandler::Impl::onGeneralizedObject
  (const ndn::ptr_lib::shared_ptr<ContentMetaInfoObject>& contentMetaInfo,
   const ndn::ptr_lib::shared_ptr<Object>& object,
   int sequenceNumber)
{
  if (onSequencedGeneralizedObject_) {
    // TODO: Catch and log errors.
    onSequencedGeneralizedObject_(sequenceNumber, contentMetaInfo, object);
  }
}

GeneralizedObjectStreamHandler::Values* GeneralizedObjectStreamHandler::values_ = 0;

}
