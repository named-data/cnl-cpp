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

#include <ndn-cpp/util/logging.hpp>
#include <ndn-cpp/delegation-set.hpp>
#include <cnl-cpp/generalized-object/generalized-object-stream-handler.hpp>

using namespace std;
using namespace ndn;
using namespace ndntools;
using namespace ndn::func_lib;

INIT_LOGGER("cnl_cpp.GeneralizedObjectStreamHandler");

namespace cnl_cpp {

GeneralizedObjectStreamHandler::Impl::Impl
  (int pipelineSize,
   const OnSequencedGeneralizedObject& onSequencedGeneralizedObject)
: pipelineSize_(pipelineSize),
  onSequencedGeneralizedObject_(onSequencedGeneralizedObject), namespace_(0),
  latestNamespace_(0), producedSequenceNumber_(-1),
  latestPacketFreshnessPeriod_(1000.0), maxReportedSequenceNumber_(-1)
{
  if (pipelineSize_ < 1)
    pipelineSize_ = 1;
}

void
GeneralizedObjectStreamHandler::Impl::setObject
  (int sequenceNumber, const ndn::Blob& object, const std::string& contentType)
{
  if (!namespace_)
    throw runtime_error
      ("GeneralizedObjectStreamHandler.setObject: The Namespace is not set");

  producedSequenceNumber_ = sequenceNumber;
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
    // Assume this is called by a consumer. Fetch the _latest packet.
    latestNamespace_->objectNeeded(true);
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
    metaInfo.setFreshnessPeriod(latestPacketFreshnessPeriod_);
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
  if (!(state == NamespaceState_OBJECT_READY &&
        changedNamespace.getName().size() ==
          latestNamespace_->getName().size() + 1 &&
        latestNamespace_->getName().isPrefixOf(changedNamespace.getName()) &&
        changedNamespace.getName()[-1].isVersion()))
    // Not a versioned _latest, so ignore.
    return;

  // Decode the _latest packet to get the target to fetch.
  // TODO: Should this already have been done by deserialize()?)
  DelegationSet delegations;
  delegations.wireDecode
    (ptr_lib::dynamic_pointer_cast<BlobObject>(changedNamespace.getObject())->getBlob());
  if (delegations.size() <= 0)
    return;
  const Name& targetName = delegations.get(0).getName();
  if (!(namespace_->getName().isPrefixOf(targetName) &&
        targetName.size() == namespace_->getName().size() + 1 &&
        targetName[-1].isSequenceNumber()))
    // TODO: Report an error for invalid target name?
    return;
  Namespace& targetNamespace = (*namespace_)[targetName];

  // We may already have the target if this was triggered by the producer.
  if (!targetNamespace.getObject()) {
    int sequenceNumber = targetName[-1].toSequenceNumber();
    maxReportedSequenceNumber_ = sequenceNumber - 1;
    requestNewSequenceNumbers();
  }
}

void
GeneralizedObjectStreamHandler::Impl::onGeneralizedObject
  (const ndn::ptr_lib::shared_ptr<ContentMetaInfoObject>& contentMetaInfo,
   Namespace& objectNamespace, int sequenceNumber)
{
  if (onSequencedGeneralizedObject_) {
    try {
      onSequencedGeneralizedObject_
        (sequenceNumber, contentMetaInfo, objectNamespace.getObject());
    } catch (const std::exception& ex) {
      _LOG_ERROR("Error in onSequencedGeneralizedObject: " << ex.what());
    } catch (...) {
      _LOG_ERROR("Error in onSequencedGeneralizedObject.");
    }
  }

  if (sequenceNumber > maxReportedSequenceNumber_)
    maxReportedSequenceNumber_ = sequenceNumber;
  requestNewSequenceNumbers();
}

void
GeneralizedObjectStreamHandler::Impl::requestNewSequenceNumbers()
{
  ptr_lib::shared_ptr<vector<Name::Component>> childComponents =
    namespace_->getChildComponents();
  // First, count how many are already requested and not received.
  int nRequestedSequenceNumbers = 0;
  // Debug: Track the max requested (and don't search all children).
  for (vector<Name::Component>::iterator component = childComponents->begin();
       component != childComponents->end(); ++component) {
    if (!component->isSequenceNumber())
      // The namespace contains a child other than a sequence number. Ignore.
      continue;

    // TODO: Should the child sequence be set to INTEREST_EXPRESSED along with _meta?
    Namespace& metaChild =
      (*namespace_)[*component]
                   [GeneralizedObjectHandler::getNAME_COMPONENT_META()];
    if (!metaChild.getData() &&
        metaChild.getState() >= NamespaceState_INTEREST_EXPRESSED) {
      ++nRequestedSequenceNumbers;
      if (nRequestedSequenceNumbers >= pipelineSize_)
        // Already maxed out on requests.
        break;
    }
  }

  // Now find unrequested sequence numbers and request.
  int sequenceNumber = maxReportedSequenceNumber_;
  while (nRequestedSequenceNumbers < pipelineSize_) {
    ++sequenceNumber;
    Namespace& sequenceNamespace =
      (*namespace_)[Name::Component::fromSequenceNumber(sequenceNumber)];
    Namespace& sequenceMeta =
      sequenceNamespace[GeneralizedObjectHandler::getNAME_COMPONENT_META()];
    if (sequenceMeta.getData() ||
        sequenceMeta.getState() >= NamespaceState_INTEREST_EXPRESSED)
      // Already got the data packet or already requested.
      continue;

    ++nRequestedSequenceNumbers;
    // Debug: Do we have to attach a new handler for each sequence number?
    ptr_lib::shared_ptr<GeneralizedObjectHandler> generalizedObjectHandler =
      ptr_lib::make_shared<GeneralizedObjectHandler>
        (bind(&GeneralizedObjectStreamHandler::Impl::onGeneralizedObject,
              shared_from_this(), _1, _2, sequenceNumber));
    sequenceNamespace.setHandler(generalizedObjectHandler);
    sequenceMeta.objectNeeded();
  }
}

GeneralizedObjectStreamHandler::Values* GeneralizedObjectStreamHandler::values_ = 0;

}
