/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil -*- */
/**
 * Copyright (C) 2017-2018 Regents of the University of California.
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

#ifndef CNL_CPP_SEGMENTED_CONTENT_HPP
#define CNL_CPP_SEGMENTED_CONTENT_HPP

#include "segment-stream.hpp"

namespace cnl_cpp {

/**
 * SegmentedContent assembles the contents of child segment packets into a
 * single block of memory.
 */
class SegmentedContent {
public:
  /**
   * Create a SegmentedContent object to use the given segmentStream to
   * assemble content.
   * @param segmentStream The SegmentStream where the Namespace is a node whose
   * children are the names of segment Data packets.
   */
  SegmentedContent(SegmentStream& segmentStream)
  : impl_(ndn::ptr_lib::make_shared<Impl>(segmentStream))
  {
    impl_->initialize();
  }

  /**
   * Create a SegmentedContent object to use a SegmentStream to assemble content.
   * @param nameSpace The Namespace node whose children are the names of segment
   * Data packets. This is used to create a SegmentStream which you can access
   * with getSegmentStream().
   */
  SegmentedContent(Namespace& nameSpace)
  : impl_(ndn::ptr_lib::make_shared<Impl>(nameSpace))
  {
    impl_->initialize();
  }

  /**
   * Get the SegmentStream given to the constructor.
   * @return The SegmentStream.
   */
  SegmentStream&
  getSegmentStream() { return impl_->getSegmentStream(); }

  /**
   * Get the Namespace object for this handler.
   * @return The Namespace object for this handler.
   */
  Namespace&
  getNamespace() { return impl_->getSegmentStream().getNamespace(); }

  /**
   * Start fetching segment Data packets. When done, the library will call the
   * callback given to getNamespace().onStateChanged .
   */
  void
  start() { impl_->getSegmentStream().start(); }

private:
  /**
   * SegmentedContent::Impl does the work of SegmentedContent. It is a separate
   * class so that SegmentedContent can create an instance in a shared_ptr to
   * use in callbacks.
   */
  class Impl : public ndn::ptr_lib::enable_shared_from_this<Impl> {
  public:
    /**
     * Create a new Impl, which should belong to a shared_ptr, then call
     * initialize().
     * @param segmentStream See the SegmentedContent constructor.
     */
    Impl(SegmentStream& segmentStream);

    /**
     * Create a new Impl, which should belong to a shared_ptr, then call
     * initialize().
     * @param nameSpace See the SegmentedContent constructor. This is used to
     * create and hold a SegmentStream object.
     */
    Impl(Namespace& nameSpace);

    /**
     * Complete the work of the constructor. This is needed because we can't
     * call shared_from_this() in the constructor.
     */
    void
    initialize();

    SegmentStream&
    getSegmentStream() { return segmentStream_; }

  private:
    void
    onSegment
      (SegmentStream& segmentStream, Namespace* segmentNamespace,
       uint64_t callbackId);

    ndn::ptr_lib::shared_ptr<SegmentStream> segmentStreamHolder_;
    SegmentStream& segmentStream_;
    bool finished_;
    std::vector<ndn::Blob> segments_;
    size_t totalSize_;
  };

  ndn::ptr_lib::shared_ptr<Impl> impl_;
};

}

#endif
