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

#include <memory.h>
#include <ndn-cpp/util/logging.hpp>
#include <ndn-cpp/digest-sha256-signature.hpp>
#include <cnl-cpp/segmented-object-handler.hpp>

using namespace std;
using namespace ndn;
using namespace ndn::func_lib;

INIT_LOGGER("cnl_cpp.SegmentedObjectHandler");

namespace cnl_cpp {

SegmentedObjectHandler::Impl::Impl(const OnSegmentedObject& onSegmentedObject)
: totalSize_(0), maxSegmentPayloadLength_(8192), namespace_(0)
{
  if (onSegmentedObject)
    addOnSegmentedObject(onSegmentedObject);
}

void
SegmentedObjectHandler::Impl::initialize(SegmentedObjectHandler* outerHandler)
{
  outerHandler->addOnSegment
    (bind(&SegmentedObjectHandler::Impl::onSegment, shared_from_this(), _1));
}

uint64_t
SegmentedObjectHandler::Impl::addOnSegmentedObject
  (const OnSegmentedObject& onSegmentedObject)
{
  uint64_t callbackId = Namespace::getNextCallbackId();
  onSegmentedObjectCallbacks_[callbackId] = onSegmentedObject;
  return callbackId;
}

void
SegmentedObjectHandler::Impl::removeCallback(uint64_t callbackId)
{
  onSegmentedObjectCallbacks_.erase(callbackId);
}

void
SegmentedObjectHandler::Impl::setMaxSegmentPayloadLength
  (size_t maxSegmentPayloadLength)
{
  if (maxSegmentPayloadLength < 1)
    throw runtime_error("The maximum segment payload length must be at least 1");
  maxSegmentPayloadLength_ = maxSegmentPayloadLength;
}

void
SegmentedObjectHandler::Impl::setObject
  (Namespace& nameSpace, const ndn::Blob& object, bool useSignatureManifest)
{
  KeyChain* keyChain = nameSpace.getKeyChain_();
  if (!keyChain)
    throw runtime_error("SegmentedObjectHandler.setObject: There is no KeyChain");

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
    // Get ready to save the segment payload digests.
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
    data->getMetaInfo().setFinalBlockId(finalBlockId);
    data->setContent(Blob(object.buf() + offset, payloadLength));

    if (useSignatureManifest) {
      data->setSignature(digestSignature);

      // Append the implicit to the manifestContent.
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

void
SegmentedObjectHandler::Impl::onSegment(Namespace* segmentNamespace)
{
  if (segmentNamespace) {
    segments_.push_back(segmentNamespace->getBlobObject());
    totalSize_ += segmentNamespace->getBlobObject().size();
  }
  else {
      // Concatenate the segments.
      ptr_lib::shared_ptr<vector<uint8_t> > content =
        ptr_lib::make_shared<vector<uint8_t> >(totalSize_);
      size_t offset = 0;
      for (size_t i = 0; i < segments_.size(); ++i) {
        const Blob& segment = segments_[i];
        memcpy(&(*content)[offset], segment.buf(), segment.size());
        offset += segment.size();
        // Free the memory.
        segments_[i] = Blob();
      }

      // Free resources that won't be used anymore.
      // The OnSegment callback was already removed by the SegmentStreamHandler.
      segments_.clear();

      // Deserialize and fire the onSegmentedObject callbacks when done.
      auto onObjectSet = [&](Namespace& objectNamespace) {
        fireOnSegmentedObject(*namespace_);
        // We only fire the callbacks once, so free the resources.
        onSegmentedObjectCallbacks_.clear();
      };
      namespace_->deserialize_(Blob(content, false), onObjectSet);
  }
}

void
SegmentedObjectHandler::Impl::fireOnSegmentedObject(Namespace& objectNamespace)
{
  // Copy the keys before iterating since callbacks can change the list.
  vector<uint64_t> keys;
  keys.reserve(onSegmentedObjectCallbacks_.size());
  for (map<uint64_t, OnSegmentedObject>::iterator i =
         onSegmentedObjectCallbacks_.begin();
       i != onSegmentedObjectCallbacks_.end(); ++i)
    keys.push_back(i->first);

  for (size_t i = 0; i < keys.size(); ++i) {
    // A callback on a previous pass may have removed this callback, so check.
    map<uint64_t, OnSegmentedObject>::iterator entry =
      onSegmentedObjectCallbacks_.find(keys[i]);
    if (entry != onSegmentedObjectCallbacks_.end()) {
      try {
        entry->second(objectNamespace);
      } catch (const std::exception& ex) {
        _LOG_ERROR("SegmentStreamHandler::fireOnSegment: Error in onSegmentedObject: " <<
                   ex.what());
      } catch (...) {
        _LOG_ERROR("SegmentStreamHandler::fireOnSegment: Error in onSegmentedObject.");
      }
    }
  }
}

SegmentedObjectHandler::Values* SegmentedObjectHandler::values_ = 0;


}
