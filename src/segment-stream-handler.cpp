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
#include <cnl-cpp/segment-stream-handler.hpp>

using namespace std;
using namespace ndn;
using namespace ndn::func_lib;

INIT_LOGGER("cnl_cpp.SegmentStreamHandler");

namespace cnl_cpp {

void
SegmentStreamHandler::onNamespaceSet()
{ 
  // Store getNamespace() in impl_. We do this instead of keeping a pointer to
  // this outer Handler object since it might be destroyed.
  impl_->onNamespaceSet(&getNamespace());
}

SegmentStreamHandler::Impl::Impl(const OnSegment& onSegment)
: maxRetrievedSegmentNumber_(-1), didRequestFinalSegment_(false),
  finalSegmentNumber_(-1), interestPipelineSize_(8), initialInterestCount_(1),
  namespace_(0)
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
SegmentStreamHandler::Impl::onNamespaceSet(Namespace* nameSpace)
{
  namespace_ = nameSpace;

  namespace_->addOnObjectNeeded
    (bind(&SegmentStreamHandler::Impl::onObjectNeeded, shared_from_this(), _1, _2, _3));
  namespace_->addOnStateChanged
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
  if (!(changedNamespace.getName().size() >=
          namespace_->getName().size() + 1 &&
        state == NamespaceState_OBJECT_READY &&
        changedNamespace.getName()[
          namespace_->getName().size()].isSegment()))
    // Not a segment, ignore.
    return;

  MetaInfo& metaInfo = changedNamespace.getData()->getMetaInfo();
  if (metaInfo.getFinalBlockId().getValue().size() > 0 &&
      metaInfo.getFinalBlockId().isSegment())
    finalSegmentNumber_ = metaInfo.getFinalBlockId().toSegment();

  // Report as many segments as possible where the node already has content.
  while (true) {
    int nextSegmentNumber = maxRetrievedSegmentNumber_ + 1;
    Namespace& nextSegment =
      (*namespace_)[Name::Component::fromSegment(nextSegmentNumber)];
    if (!nextSegment.getObject())
      break;

    maxRetrievedSegmentNumber_ = nextSegmentNumber;
    fireOnSegment(&nextSegment);

    if (finalSegmentNumber_ >= 0 && nextSegmentNumber == finalSegmentNumber_) {
      // Finished.
      fireOnSegment(0);
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
  int segmentNumber = maxRetrievedSegmentNumber_;
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

}
