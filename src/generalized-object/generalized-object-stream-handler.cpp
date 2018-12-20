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
  latestPacketFreshnessPeriod_(1000.0), nRequestedSequenceNumbers_(0),
  maxRequestedSequenceNumber_(0), nReportedSequenceNumbers_(0),
  maxReportedSequenceNumber_(-1)
{
  if (pipelineSize_ < 0)
    pipelineSize_ = 0;
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
  if (state == NamespaceState_INTEREST_TIMEOUT ||
      state == NamespaceState_INTEREST_NETWORK_NACK) {
    if (&changedNamespace == latestNamespace_) {
      // Timeout or network NACK, so try to fetch again.
      latestNamespace_->getFace_()->callLater
        (latestPacketFreshnessPeriod_, [=]{ latestNamespace_->objectNeeded(true); });
      return;
    }
    else if (pipelineSize_ > 0 &&
        changedNamespace.getName().size() == namespace_->getName().size() + 2 &&
        changedNamespace.getName()[-1].equals
          (GeneralizedObjectHandler::getNAME_COMPONENT_META()) &&
        changedNamespace.getName()[-2].isSequenceNumber() &&
        changedNamespace.getName()[-2].toSequenceNumber() ==
          maxRequestedSequenceNumber_) {
      // The highest pipelined request timed out, so request the _latest.
      // TODO: Should we do this for the lowest requested?
      latestNamespace_->objectNeeded(true);
      return;
    }
  }

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

    if (pipelineSize_ == 0) {
      // Fetch one generalized object.
      Namespace& sequenceMeta =
        targetNamespace[GeneralizedObjectHandler::getNAME_COMPONENT_META()];
      // Make sure we didn't already request it.
      if (sequenceMeta.getState() < NamespaceState_INTEREST_EXPRESSED) {
        ptr_lib::shared_ptr<GeneralizedObjectHandler> generalizedObjectHandler =
          ptr_lib::make_shared<GeneralizedObjectHandler>
            (bind(&GeneralizedObjectStreamHandler::Impl::onGeneralizedObject,
                  shared_from_this(), _1, _2, sequenceNumber));
        targetNamespace.setHandler(generalizedObjectHandler);
        sequenceMeta.objectNeeded();
      }
    }
    else {
      // Fetch by continuously filling the Interest pipeline.
      maxReportedSequenceNumber_ = sequenceNumber - 1;
      // Reset the pipeline in case we are resuming after a timeout.
      nRequestedSequenceNumbers_ = nReportedSequenceNumbers_;
      requestNewSequenceNumbers();
    }
  }

  if (pipelineSize_ == 0) {
    // Schedule to fetch the next _latest packet.
    Milliseconds freshnessPeriod =
      changedNamespace.getData()->getMetaInfo().getFreshnessPeriod();
    if (freshnessPeriod < 0)
      // No freshness period. We don't expect this.
      return;
    latestNamespace_->getFace_()->callLater
      (freshnessPeriod / 2, [=]{ latestNamespace_->objectNeeded(true); });
  }
}

void
GeneralizedObjectStreamHandler::Impl::onGeneralizedObject
  (const ndn::ptr_lib::shared_ptr<ContentMetaInfoObject>& contentMetaInfo,
   Namespace& objectNamespace, int sequenceNumber)
{
  // The Handler is finished, so detach it from the Namespace to save resources.
  objectNamespace.setHandler(ptr_lib::shared_ptr<Handler>());

  if (onSequencedGeneralizedObject_) {
    try {
      onSequencedGeneralizedObject_
        (sequenceNumber, contentMetaInfo, objectNamespace);
    } catch (const std::exception& ex) {
      _LOG_ERROR("Error in onSequencedGeneralizedObject: " << ex.what());
    } catch (...) {
      _LOG_ERROR("Error in onSequencedGeneralizedObject.");
    }
  }

  ++nReportedSequenceNumbers_;
  if (sequenceNumber > maxReportedSequenceNumber_)
    maxReportedSequenceNumber_ = sequenceNumber;

  if (pipelineSize_ > 0)
    // Continue to fetch by filling the pipeline.
    requestNewSequenceNumbers();
}

void
GeneralizedObjectStreamHandler::Impl::requestNewSequenceNumbers()
{
  ptr_lib::shared_ptr<vector<Name::Component>> childComponents =
    namespace_->getChildComponents();
  int nOutstandingSequenceNumbers = 
    nRequestedSequenceNumbers_ - nReportedSequenceNumbers_;

  // Now find unrequested sequence numbers and request.
  int sequenceNumber = maxReportedSequenceNumber_;
  while (nOutstandingSequenceNumbers < pipelineSize_) {
    ++sequenceNumber;
    Namespace& sequenceNamespace =
      (*namespace_)[Name::Component::fromSequenceNumber(sequenceNumber)];
    Namespace& sequenceMeta =
      sequenceNamespace[GeneralizedObjectHandler::getNAME_COMPONENT_META()];
    if (sequenceMeta.getData() ||
        sequenceMeta.getState() >= NamespaceState_INTEREST_EXPRESSED)
      // Already got the data packet or already requested.
      continue;

    ++nOutstandingSequenceNumbers;
    ++nRequestedSequenceNumbers_;

    // Debug: Do we have to attach a new handler for each sequence number?
    ptr_lib::shared_ptr<GeneralizedObjectHandler> generalizedObjectHandler =
      ptr_lib::make_shared<GeneralizedObjectHandler>
        (bind(&GeneralizedObjectStreamHandler::Impl::onGeneralizedObject,
              shared_from_this(), _1, _2, sequenceNumber));
    sequenceNamespace.setHandler(generalizedObjectHandler);
    if (sequenceNumber > maxRequestedSequenceNumber_)
      maxRequestedSequenceNumber_ = sequenceNumber;
    sequenceMeta.objectNeeded();
  }
}

GeneralizedObjectStreamHandler::Values* GeneralizedObjectStreamHandler::values_ = 0;

}
