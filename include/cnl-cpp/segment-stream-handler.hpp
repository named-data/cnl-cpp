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

#ifndef CNL_CPP_SEGMENT_STREAM_HANDLER_HPP
#define CNL_CPP_SEGMENT_STREAM_HANDLER_HPP

#include "namespace.hpp"

namespace cnl_cpp {

/**
 * SegmentStreamHandler extends Namespace::Handler and attaches to a Namespace
 * node to fetch and return child segments in order.
 */
class SegmentStreamHandler : public Namespace::Handler {
public:
  typedef ndn::func_lib::function<void
    (SegmentStreamHandler& handler, Namespace* segmentNamespace,
     uint64_t callbackId)> OnSegment;

  /**
   * Create a SegmentStreamHandler with the optional onSegment callback.
   * @param onSegment (optional) If supplied, this calls addOnSegment(onSegment).
   * You may also call addOnSegment directly.
   */
  SegmentStreamHandler(const OnSegment& onSegment = OnSegment())
  : impl_(ndn::ptr_lib::make_shared<Impl>(*this, onSegment))
  {
  }

  /**
   * Add an onSegment callback. When a new segment is available, this calls
   * onSegment as described below. Segments are supplied in order.
   * @param onSegment This calls
   * onSegment(handler, segmentNamespace, callbackId) where handler is this
   * SegmentStreamHandler, segmentNamespace is the Namespace where you can use
   * segmentNamespace.getObject(), and callbackId is the callback ID returned by
   * this method. You must check if segmentNamespace is null because after
   * supplying the final segment, this calls
   * onSegment(handler, null, callbackId) to signal the "end of stream".
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

  // TODO: get/setInitialInterestCount

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

protected:
  virtual void
  onNamespaceSet();

private:
  /**
   * SegmentStreamHandler::Impl does the work of SegmentStreamHandler. It is a
   * separate class so that SegmentStreamHandler can create an instance in a
   * shared_ptr to use in callbacks.
   */
  class Impl : public ndn::ptr_lib::enable_shared_from_this<Impl> {
  public:
    /**
     * Create a new Impl, which should belong to a shared_ptr.
     * @param outerHandler The SegmentStreamHandler which is creating this inner
     * Impl.
     * @param onSegment See the SegmentStreamHandler constructor.
     */
    Impl
      (SegmentStreamHandler& outerHandler,
       const OnSegment& onSegment);

    uint64_t
    addOnSegment(const OnSegment& onSegment);

    void
    removeCallback(uint64_t callbackId);

    int
    getInterestPipelineSize() { return interestPipelineSize_; }

    void
    setInterestPipelineSize(int interestPipelineSize);

    void
    start(int interestCount) { requestNewSegments(interestCount); }

    void
    onNamespaceSet();

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
    onStateChanged
      (Namespace& nameSpace, Namespace& changedNamespace, NamespaceState state,
       uint64_t callbackId);

    void
    requestNewSegments(int maxRequestedSegments);

    void
    fireOnSegment(Namespace* segmentNamespace);

    SegmentStreamHandler& outerHandler_;
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