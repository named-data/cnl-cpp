/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil -*- */
/**
 * Copyright (C) 2017 Regents of the University of California.
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

#include <cnl-cpp/segment-stream.hpp>
#include <ndn-cpp/util/logging.hpp>

using namespace std;
using namespace ndn;
using namespace ndn::func_lib;

INIT_LOGGER("cnl_cpp.SegmentStream");

namespace cnl_cpp {

SegmentStream::Impl::Impl(SegmentStream& outerSegmentStream, Namespace& nameSpace)
: outerSegmentStream_(outerSegmentStream), namespace_(nameSpace),
  maxRetrievedSegmentNumber_(-1), didRequestFinalSegment_(false),
  finalSegmentNumber_(-1), interestPipelineSize_(8)
{
}

void
SegmentStream::Impl::initialize()
{
  namespace_.addOnContentSet
    (bind(&SegmentStream::Impl::onContentSet, shared_from_this(), _1, _2, _3));
}

uint64_t
SegmentStream::Impl::addOnSegment(const OnSegment& onSegment)
{
  uint64_t callbackId = Namespace::getNextCallbackId();
  onSegmentCallbacks_[callbackId] = onSegment;
  return callbackId;
}

void
SegmentStream::Impl::removeCallback(uint64_t callbackId)
{
  onSegmentCallbacks_.erase(callbackId);
}

void
SegmentStream::Impl::setInterestPipelineSize(int interestPipelineSize)
{
  if (interestPipelineSize < 1)
    throw runtime_error("The interestPipelineSize must be at least 1");
  interestPipelineSize_ = interestPipelineSize;
}

Namespace&
SegmentStream::Impl::debugGetRightmostLeaf(Namespace& nameSpace)
{
  Namespace* result = &nameSpace;
  while (true) {
    ptr_lib::shared_ptr<vector<Name::Component>> childComponents =
      result->getChildComponents();
    if (childComponents->size() == 0)
      return *result;

    result = &result->getChild(childComponents->at(childComponents->size() - 1));
  }
}

void
SegmentStream::Impl::onContentSet
  (Namespace& nameSpace, Namespace& contentNamespace, uint64_t callbackId)
{
  if (!(contentNamespace.getName().size() >= namespace_.getName().size() + 1 &&
        contentNamespace.getName()[namespace_.getName().size()].isSegment()))
    // Not a segment, ignore.
    // Debug: If this is the first call, we still need to request segments.
    return;

  // TODO: Use the Namespace mechanism to validate the Data packet.

  MetaInfo& metaInfo = contentNamespace.getData()->getMetaInfo();
  if (metaInfo.getFinalBlockId().getValue().size() > 0 &&
      metaInfo.getFinalBlockId().isSegment())
    finalSegmentNumber_ = metaInfo.getFinalBlockId().toSegment();

  // Report as many segments as possible where the node already has content.
  while (true) {
    int nextSegmentNumber = maxRetrievedSegmentNumber_ + 1;
    Namespace& nextSegment = debugGetRightmostLeaf
      (namespace_[Name::Component::fromSegment(nextSegmentNumber)]);
    if (nextSegment.getContent().isNull())
      break;

    maxRetrievedSegmentNumber_ = nextSegmentNumber;
    fireOnSegment(&nextSegment);

    if (finalSegmentNumber_ >= 0 && nextSegmentNumber == finalSegmentNumber_) {
      // Finished.
      fireOnSegment(0);
      return;
    }
  }

  if (finalSegmentNumber_ < 0 && !didRequestFinalSegment_) {
    didRequestFinalSegment_ = true;
    // Try to determine the final segment now.
    Interest interestTemplate;
    interestTemplate.setInterestLifetimeMilliseconds(4000.0);
    interestTemplate.setChildSelector(1);
    namespace_.expressInterest(&interestTemplate);
  }

  requestNewSegments();
}

void
SegmentStream::Impl::requestNewSegments()
{
  ptr_lib::shared_ptr<vector<Name::Component>> childComponents =
    namespace_.getChildComponents();
  // First, count how many are already requested and not received.
  int nRequestedSegments = 0;
  for (vector<Name::Component>::iterator component = childComponents->begin();
       component != childComponents->end(); ++component) {
    if (!component->isSegment())
      // The namespace contains a child other than a segment. Ignore.
      continue;

    Namespace& child = namespace_[*component];
    // Debug: Check the leaf for content, but use the immediate child
    // for _debugSegmentStreamDidExpressInterest.
    if (debugGetRightmostLeaf(child).getContent().isNull() &&
        child.debugSegmentStreamDidExpressInterest_) {
      ++nRequestedSegments;
      if (nRequestedSegments >= interestPipelineSize_)
        // Already maxed out on requests.
        break;
    }
  }

  // Now find unrequested segment numbers and request.
  int segmentNumber = maxRetrievedSegmentNumber_;
  while (nRequestedSegments < interestPipelineSize_) {
    ++segmentNumber;
    if (finalSegmentNumber_ >= 0 && segmentNumber > finalSegmentNumber_)
      break;

    Namespace& segment = namespace_[Name::Component::fromSegment(segmentNumber)];
    if (!debugGetRightmostLeaf(segment).getContent().isNull() ||
        segment.debugSegmentStreamDidExpressInterest_)
      // Already got the data packet or already requested.
      continue;

    ++nRequestedSegments;
    segment.debugSegmentStreamDidExpressInterest_ = true;
    segment.expressInterest();
  }
}

void
SegmentStream::Impl::fireOnSegment(Namespace* segmentNamespace)
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
        entry->second(outerSegmentStream_, segmentNamespace, entry->first);
      } catch (const std::exception& ex) {
        _LOG_ERROR("SegmentStream::fireOnSegment: Error in onSegment: " <<
                   ex.what());
      } catch (...) {
        _LOG_ERROR("SegmentStream::fireOnSegment: Error in onSegment.");
      }
    }
  }
}

}
