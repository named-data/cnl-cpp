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

#include <memory.h>
#include <cnl-cpp/segmented-content.hpp>

using namespace std;
using namespace ndn;
using namespace ndn::func_lib;

namespace cnl_cpp {

SegmentedContent::Impl::Impl(SegmentStream& segmentStream)
: segmentStream_(segmentStream), finished_(false), totalSize_(0)
{
}

SegmentedContent::Impl::Impl(Namespace& nameSpace)
: segmentStreamHolder_(new SegmentStream(nameSpace)),
  segmentStream_(*segmentStreamHolder_), finished_(false), totalSize_(0)
{
}

void
SegmentedContent::Impl::initialize()
{
  segmentStream_.addOnSegment
    (bind(&SegmentedContent::Impl::onSegment, shared_from_this(), _1, _2, _3));
}

void
SegmentedContent::Impl::onSegment
  (SegmentStream& segmentStream, Namespace* segmentNamespace,
   uint64_t callbackId)
{
  if (finished_)
    // We already finished and called onContent. (We don't expect this.)
    return;

  if (segmentNamespace) {
    segments_.push_back(segmentNamespace->getBlobContent());
    totalSize_ += segmentNamespace->getBlobContent().size();
  }
  else {
      // Finished. We don't need the callback anymore.
      segmentStream.removeCallback(callbackId);

      // Concatenate the segments.
      ptr_lib::shared_ptr<vector<uint8_t> > content
        (new std::vector<uint8_t>(totalSize_));
      size_t offset = 0;
      for (size_t i = 0; i < segments_.size(); ++i) {
        const Blob& segment = segments_[i];
        memcpy(&(*content)[offset], segment.buf(), segment.size());
        offset += segment.size();
        // Free the memory.
        segments_[i] = Blob();
      }

      // Free memory.
      segments_.clear();
      finished_ = true;

      // Debug: Fix this hack. How can we attach content to a namespace
      // node which has no associated Data packet? Who is authorized to do so?
      segmentStream_.getNamespace().debugOnContentTransformed
        (ptr_lib::make_shared<Data>(), 
         ptr_lib::make_shared<BlobContent>(Blob(content, false)));
  }
}

}
