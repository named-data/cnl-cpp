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

#ifndef CNL_CPP_SEGMENT_STREAM_HPP
#define CNL_CPP_SEGMENT_STREAM_HPP

#include "namespace.hpp"

namespace cnl_cpp {

/**
 * SegmentStream attaches to a Namespace node to fetch and return child segment
 * packets in order.
 */
class SegmentStream {
public:
  typedef ndn::func_lib::function<void
    (SegmentStream& segmentStream, Namespace* segmentNamespace,
     uint64_t callbackId)> OnSegment;

  /**
   * Create a SegmentStream object to attach to the given namespace. You can add
   * callbacks and set options, then you should call start().
   * @param nameSpace The Namespace node whose children are the names of segment
   * Data packets.
   */
  SegmentStream(Namespace& nameSpace)
  : impl_(new Impl(*this, nameSpace))
  {
    impl_->initialize();
  }

  /**
   * Add an onSegment callback. When a new segment is available, this calls
   * onSegment as described below. Segments are supplied in order.
   * @param onSegment This calls
   * onSegment(segmentStream, segmentNamespace, callbackId)
   * where segmentStream is this SegmentStream, segmentNamespace is the
   * Namespace where you can use segmentNamespace.getContent(), and callbackId
   * is the callback ID returned by this method. You must check if
   * segmentNamespace is null because after supplying the final segment, this
   * calls onSegment(stream, 0, callbackId) to signal the "end of stream".
   * NOTE: The library will log any exceptions thrown by this callback, but for
   * better error handling the callback should catch and properly handle any
   * exceptions.
   * @return The callback ID which you can use in removeCallback().
   */
  uint64_t
  addOnSegment(const OnSegment& onSegment)
  {
    return impl_->addOnSegment(onSegment);
  }

  /**
   * Remove the callback with the given callbackId. This does not search for the
   * callbackId in child nodes. If the callbackId isn't found, do nothing.
   * @param callbackId The callback ID returned, for example, from
   * addOnSegment.
   */
  void
  removeCallback(uint64_t callbackId) { impl_->removeCallback(callbackId); }

  /**
   * Get the Namespace object given to the constructor.
   * @return The Namespace object given to the constructor.
   */
  Namespace&
  getNamespace() { return impl_->getNamespace(); }

  /**
   * Get the number of outstanding interests which this maintains while fetching
   * segments.
   * @return The Interest pipeline size.
   */
  int
  getInterestPipelineSize() { return impl_->getInterestPipelineSize(); }

  /**
   * Set the number of outstanding interests which this maintains while fetching
   * segments.
   * @param interestPipelineSize The Interest pipeline size.
   * @throws runtime_error if interestPipelineSize is less than 1.
   */
  void
  setInterestPipelineSize(int interestPipelineSize)
  {
    impl_->setInterestPipelineSize(interestPipelineSize);
  }

  /**
   * Start fetching segment Data packets and adding them as children of
   * getNamespace(), calling any onSegment callbacks in order as the segments
   * are received. Even though the segments supplied to onSegment are in order,
   * note that children of the Namespace node are not necessarily added in order.
   * @param interestCount (optional) The number of initial Interests to send for
   * segments. By default this just sends an Interest for the first segment and
   * waits for the response before fetching more segments, but if you know the
   * number of segments you can reduce latency by initially requesting more
   * segments. (However, you should not use a number larger than the Interest
   * pipeline size.) If omitted, use 1.
   */
  void
  start(int interestCount = 1) { impl_->start(interestCount); }

private:
  /**
   * SegmentStream::Impl does the work of SegmentStream. It is a separate class
   * so that SegmentStream can create an instance in a shared_ptr to use in
   * callbacks.
   */
  class Impl : public ndn::ptr_lib::enable_shared_from_this<Impl> {
  public:
    /**
     * Create a new Impl, which should belong to a shared_ptr, then call
     * initialize().
     * @param outerSegmentStream The SegmentStream which is creating this inner Imp.
     * @param nameSpace See the SegmentStream constructor.
     */
    Impl(SegmentStream& outerSegmentStream, Namespace& nameSpace);

    /**
     * Complete the work of the constructor. This is needed because we can't
     * call shared_from_this() in the constructor.
     */
    void
    initialize();

    uint64_t
    addOnSegment(const OnSegment& onSegment);

    void
    removeCallback(uint64_t callbackId);

    Namespace&
    getNamespace() { return namespace_; }

    int
    getInterestPipelineSize() { return interestPipelineSize_; }

    void
    setInterestPipelineSize(int interestPipelineSize);

    void
    start(int interestCount) { requestNewSegments(interestCount); }

  private:
    /**
     * Get the rightmost leaf of the given namespace. Use this temporarily to
     * handle encrypted data packets where the name has the key name appended.
     * @param nameSpace The Namespace with the leaf node.
     * @return The leaf Namespace node.
     */
    static Namespace&
    debugGetRightmostLeaf(Namespace& nameSpace);

    void
    onContentSet
      (Namespace& nameSpace, Namespace& contentNamespace, uint64_t callbackId);

    void
    requestNewSegments(int maxRequestedSegments);

    void
    fireOnSegment(Namespace* segmentNamespace);

    SegmentStream& outerSegmentStream_;
    Namespace& namespace_;
    int maxRetrievedSegmentNumber_;
    bool didRequestFinalSegment_;
    int finalSegmentNumber_;
    int interestPipelineSize_;
    // The key is the callback ID. The value is the OnSegment function.
    std::map<uint64_t, OnSegment> onSegmentCallbacks_;
  };

  ndn::ptr_lib::shared_ptr<Impl> impl_;
};

}

#endif
