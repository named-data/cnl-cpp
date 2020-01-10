/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil -*- */
/**
 * Copyright (C) 2018-2020 Regents of the University of California.
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

#if NDN_CPP_HAVE_MEMORY_H
#include <memory.h>
#else
#include <string.h>
#endif
#include <ndn-cpp/util/logging.hpp>
#include <ndn-cpp/digest-sha256-signature.hpp>
#include <cnl-cpp/segment-stream-handler.hpp>

using namespace std;
using namespace ndn;
using namespace ndn::func_lib;

INIT_LOGGER("cnl_cpp.SegmentStreamHandler");

extern "C" {

const char*
cnl_cpp_getSegmentStreamHandlerManifestComponent() {  return "_manifest"; }

}

namespace cnl_cpp {

void
SegmentStreamHandler::onNamespaceSet()
{ 
  // Store getNamespace() in impl_. We do this instead of keeping a pointer to
  // this outer Handler object since it might be destroyed.
  impl_->onNamespaceSet(&getNamespace());
}

SegmentStreamHandler::Impl::Impl(const OnSegment& onSegment)
: maxReportedSegmentNumber_(-1), didRequestFinalSegment_(false),
  finalSegmentNumber_(-1), interestPipelineSize_(8), initialInterestCount_(1),
  onObjectNeededId_(0), onStateChangedId_(0), namespace_(0),
  maxSegmentPayloadLength_(8192)
{
  if (onSegment)
    addOnSegment(onSegment);
}

uint64_t
SegmentStreamHandler::Impl::addOnSegment(const OnSegment& onSegment)
{
  uint64_t callbackId = Namespace::getNextCallbackId();
  onSegmentCallbacks_[callbackId] = onSegment;
  return callbackId;
}

void
SegmentStreamHandler::Impl::removeCallback(uint64_t callbackId)
{
  onSegmentCallbacks_.erase(callbackId);
}

void
SegmentStreamHandler::Impl::setInterestPipelineSize(int interestPipelineSize)
{
  if (interestPipelineSize < 1)
    throw runtime_error("The interestPipelineSize must be at least 1");
  interestPipelineSize_ = interestPipelineSize;
}

void
SegmentStreamHandler::Impl::setInitialInterestCount(int initialInterestCount)
{
  if (initialInterestCount < 1)
    throw runtime_error("The initial Interest count must be at least 1");
  initialInterestCount_ = initialInterestCount;
}

void
SegmentStreamHandler::Impl::setMaxSegmentPayloadLength
  (size_t maxSegmentPayloadLength)
{
  if (maxSegmentPayloadLength < 1)
    throw runtime_error("The maximum segment payload length must be at least 1");
  maxSegmentPayloadLength_ = maxSegmentPayloadLength;
}

void
SegmentStreamHandler::Impl::setObject
  (Namespace& nameSpace, const ndn::Blob& object, bool useSignatureManifest)
{
  KeyChain* keyChain = nameSpace.getKeyChain_();
  if (!keyChain)
    throw runtime_error("SegmentStreamHandler.setObject: There is no KeyChain");

  // Get the final block ID.
  uint64_t finalSegment = 0;
  // Instead of a brute calculation, imitate the loop we will use below.
  uint64_t segment = 0;
  for (size_t offset = 0; offset < object.size();
       offset += maxSegmentPayloadLength_) {
    finalSegment = segment;
    ++segment;
  }
  Name::Component finalBlockId = Name().appendSegment(finalSegment)[0];

  ptr_lib::shared_ptr<vector<uint8_t> > manifestContent;
  DigestSha256Signature digestSignature;
  if (useSignatureManifest) {
    // Get ready to save the segment implicit digests.
    manifestContent.reset
      (new vector<uint8_t>((finalSegment + 1) * ndn_SHA256_DIGEST_SIZE));

    // Use a DigestSha256Signature with all zeros.
    ptr_lib::shared_ptr<vector<uint8_t> > zeros
      (new vector<uint8_t>(ndn_SHA256_DIGEST_SIZE));
    memset(&zeros->front(), 0, ndn_SHA256_DIGEST_SIZE);
    digestSignature.setSignature(Blob(zeros, false));
  }

  segment = 0;
  for (size_t offset = 0; offset < object.size();
       offset += maxSegmentPayloadLength_) {
    size_t payloadLength = maxSegmentPayloadLength_;
    if (offset + payloadLength > object.size())
      payloadLength = object.size() - offset;

    // Make the Data packet.
    Namespace& segmentNamespace = nameSpace[Name::Component::fromSegment(segment)];
    ptr_lib::shared_ptr<Data> data =
      ptr_lib::make_shared<Data>(segmentNamespace.getName());

    const MetaInfo* metaInfo = nameSpace.getNewDataMetaInfo_();
    if (metaInfo)
      // Start with a copy of the provided MetaInfo.
      data->setMetaInfo(*metaInfo);
    data->getMetaInfo().setFinalBlockId(finalBlockId);
    data->setContent(Blob(object.buf() + offset, payloadLength));

    if (useSignatureManifest) {
      data->setSignature(digestSignature);

      // Append the implicit digest to the manifestContent.
      const Blob& implicitDigest = (*data->getFullName())[-1].getValue();
      size_t digestOffset = segment * ndn_SHA256_DIGEST_SIZE;
      memcpy
        (&manifestContent->front() + digestOffset, implicitDigest.buf(),
         ndn_SHA256_DIGEST_SIZE);
    }
    else
      keyChain->sign(*data);

    segmentNamespace.setData(data);

    ++segment;
  }

  if (useSignatureManifest)
    // Create the _manifest data packet.
    nameSpace[getNAME_COMPONENT_MANIFEST()].serializeObject
      (ptr_lib::make_shared<BlobObject>(Blob(manifestContent, false)));

  // TODO: Do this in a canSerialize callback from Namespace.serializeObject?
  nameSpace.setObject_(ptr_lib::make_shared<BlobObject>(object));
}

bool
SegmentStreamHandler::Impl::verifyWithManifest(Namespace& nameSpace)
{
  Blob manifestContent = nameSpace[getNAME_COMPONENT_MANIFEST()].getBlobObject();
  size_t nSegments = manifestContent.size() / ndn_SHA256_DIGEST_SIZE;
  if (manifestContent.size() != nSegments * ndn_SHA256_DIGEST_SIZE)
    // The manifest size is not a multiple of the digest size as expected.
    return false;

  for (size_t segment = 0; segment < nSegments; ++segment) {
    Namespace& segmentNamespace = nameSpace[Name::Component::fromSegment(segment)];
    const Blob& segmentDigest = (*segmentNamespace.getData()->getFullName())[-1].getValue();
    if (segmentDigest.size() != ndn_SHA256_DIGEST_SIZE)
      // We don't expect this.
      return false;
    // To avoid copying, use memcmp directly instead of making a Blob.
    if (memcmp(segmentDigest.buf(),
               manifestContent.buf() + segment * ndn_SHA256_DIGEST_SIZE,
               ndn_SHA256_DIGEST_SIZE) != 0)
      return false;
  }

  return true;
}

void
SegmentStreamHandler::Impl::onNamespaceSet(Namespace* nameSpace)
{
  namespace_ = nameSpace;

  onObjectNeededId_ = namespace_->addOnObjectNeeded
    (bind(&SegmentStreamHandler::Impl::onObjectNeeded, shared_from_this(), _1, _2, _3));
  onStateChangedId_ = namespace_->addOnStateChanged
    (bind(&SegmentStreamHandler::Impl::onStateChanged, shared_from_this(), _1, _2, _3, _4));
}

bool
SegmentStreamHandler::Impl::onObjectNeeded
  (Namespace& nameSpace, Namespace& neededNamespace, uint64_t callbackId)
{
  if (&nameSpace != &neededNamespace)
    return false;

  requestNewSegments(initialInterestCount_);
  return true;
}

void
SegmentStreamHandler::Impl::onStateChanged
  (Namespace& nameSpace, Namespace& changedNamespace, NamespaceState state,
   uint64_t callbackId)
{
  if (!(state == NamespaceState_OBJECT_READY &&
        changedNamespace.getName().size() == namespace_->getName().size() + 1 &&
        changedNamespace.getName()[-1].isSegment()))
    // Not a segment, ignore.
    return;

  MetaInfo& metaInfo = changedNamespace.getData()->getMetaInfo();
  if (metaInfo.getFinalBlockId().getValue().size() > 0 &&
      metaInfo.getFinalBlockId().isSegment())
    finalSegmentNumber_ = metaInfo.getFinalBlockId().toSegment();

  // Report as many segments as possible where the node already has content.
  while (true) {
    int nextSegmentNumber = maxReportedSegmentNumber_ + 1;
    Namespace& nextSegment =
      (*namespace_)[Name::Component::fromSegment(nextSegmentNumber)];
    if (!nextSegment.getObject())
      break;

    maxReportedSegmentNumber_ = nextSegmentNumber;
    fireOnSegment(&nextSegment);

    if (dynamic_cast<const DigestSha256Signature *>
        (nextSegment.getData()->getSignature())) {
      // Assume we are using a signature _manifest.
      Namespace& manifestNamespace = (*namespace_)[getNAME_COMPONENT_MANIFEST()];
      if (manifestNamespace.getState() < NamespaceState_INTEREST_EXPRESSED)
        // We haven't requested the signature _manifest yet.
        manifestNamespace.objectNeeded();
    }

    if (finalSegmentNumber_ >= 0 && nextSegmentNumber == finalSegmentNumber_) {
      // Finished.
      fireOnSegment(0);

      // Free resources that won't be used anymore.
      onSegmentCallbacks_.clear();
      namespace_->removeCallback(onObjectNeededId_);
      namespace_->removeCallback(onStateChangedId_);

      return;
    }
  }

  requestNewSegments(interestPipelineSize_);
}

void
SegmentStreamHandler::Impl::requestNewSegments(int maxRequestedSegments)
{
  if (maxRequestedSegments < 1)
    maxRequestedSegments = 1;

  ptr_lib::shared_ptr<vector<Name::Component>> childComponents =
    namespace_->getChildComponents();
  // First, count how many are already requested and not received.
  int nRequestedSegments = 0;
  for (vector<Name::Component>::iterator component = childComponents->begin();
       component != childComponents->end(); ++component) {
    if (!component->isSegment())
      // The namespace contains a child other than a segment. Ignore.
      continue;

    Namespace& child = (*namespace_)[*component];
    if (!child.getData() &&
        child.getState() >= NamespaceState_INTEREST_EXPRESSED) {
      ++nRequestedSegments;
      if (nRequestedSegments >= maxRequestedSegments)
        // Already maxed out on requests.
        break;
    }
  }

  // Now find unrequested segment numbers and request.
  int segmentNumber = maxReportedSegmentNumber_;
  while (nRequestedSegments < maxRequestedSegments) {
    ++segmentNumber;
    if (finalSegmentNumber_ >= 0 && segmentNumber > finalSegmentNumber_)
      break;

    Namespace& segment = (*namespace_)[
      Name::Component::fromSegment(segmentNumber)];
    if (segment.getData() ||
        segment.getState() >= NamespaceState_INTEREST_EXPRESSED)
      // Already got the data packet or already requested.
      continue;

    ++nRequestedSegments;
    segment.objectNeeded();
  }
}

void
SegmentStreamHandler::Impl::fireOnSegment(Namespace* segmentNamespace)
{
  // Copy the keys before iterating since callbacks can change the list.
  vector<uint64_t> keys;
  keys.reserve(onSegmentCallbacks_.size());
  for (map<uint64_t, OnSegment>::iterator i = onSegmentCallbacks_.begin();
       i != onSegmentCallbacks_.end(); ++i)
    keys.push_back(i->first);

  for (size_t i = 0; i < keys.size(); ++i) {
    // A callback on a previous pass may have removed this callback, so check.
    map<uint64_t, OnSegment>::iterator entry = onSegmentCallbacks_.find(keys[i]);
    if (entry != onSegmentCallbacks_.end()) {
      try {
        entry->second(segmentNamespace);
      } catch (const std::exception& ex) {
        _LOG_ERROR("SegmentStreamHandler::fireOnSegment: Error in onSegment: " <<
                   ex.what());
      } catch (...) {
        _LOG_ERROR("SegmentStreamHandler::fireOnSegment: Error in onSegment.");
      }
    }
  }
}

SegmentStreamHandler::Values* SegmentStreamHandler::values_ = 0;

}
