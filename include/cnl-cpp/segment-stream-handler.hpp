/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil -*- */
/**
 * Copyright (C) 2018-2019 Regents of the University of California.
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
  typedef ndn::func_lib::function<void(Namespace* segmentNamespace)> OnSegment;

  /**
   * Create a SegmentStreamHandler with the optional onSegment callback.
   * @param nameSpace (optional) Set the Namespace that this handler is attached
   * to. If omitted or null, you can call setNamespace() later.
   * @param onSegment (optional) If supplied, this calls addOnSegment(onSegment).
   * You may also call addOnSegment directly.
   */
  SegmentStreamHandler
    (Namespace* nameSpace = 0, const OnSegment& onSegment = OnSegment())
  : impl_(ndn::ptr_lib::make_shared<Impl>(onSegment))
  {
    if (nameSpace)
      setNamespace(nameSpace);
  }

  /**
   * Set the Namespace that this handler is attached to.
   * @param nameSpace The Handler's Namespace.
   * @return This SegmentStreamHandler so you can chain calls to update values.
   * @throws runtime_error if this Handler is already attached to a different
   * Namespace.
   */
  SegmentStreamHandler&
  setNamespace(Namespace* nameSpace)
  {
    // Call the base implementation and cast the return value.
    return static_cast<SegmentStreamHandler&>(Handler::setNamespace(nameSpace));
  }

  /**
   * Add an onSegment callback. When a new segment is available, this calls
   * onSegment as described below. Segments are supplied in order.
   * @param onSegment This calls
   * onSegment(segmentNamespace) where segmentNamespace is the Namespace where
   * you can use segmentNamespace.getObject(). You must check if
   * segmentNamespace is null because after supplying the final segment, this
   * calls onSegment(null) to signal the "end of stream".
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

  /**
   * Get the initial Interest count (as described in setInitialInterestCount).
   * @return The initial Interest count.
   */
  int
  getInitialInterestCount() { return impl_->getInitialInterestCount(); }

  /**
   * Set the number of initial Interests to send for segments. By default this
   * just sends an Interest for the first segment and waits for the response
   * before fetching more segments, but if you know the number of segments you
   * can reduce latency by initially requesting more segments. (However, you
   * should not use a number larger than the Interest pipeline size.)
   * @param initialInterestCount The initial Interest count.
   * @throws runtime_error if initialInterestCount is less than 1.
   */
  void
  setInitialInterestCount(int initialInterestCount)
  {
    impl_->setInitialInterestCount(initialInterestCount);
  }

  /**
   * Get the maximum length of the payload of one segment, used to split a
   * larger payload into segments.
   * @return The maximum payload length.
   */
  size_t
  getMaxSegmentPayloadLength() { return impl_->getMaxSegmentPayloadLength(); }

  /**
   * Set the maximum length of the payload of one segment, used to split a
   * larger payload into segments.
   * @param maxSegmentPayloadLength The maximum payload length.
   */
  void
  setMaxSegmentPayloadLength(size_t maxSegmentPayloadLength)
  {
    impl_->setMaxSegmentPayloadLength(maxSegmentPayloadLength);
  }

  /**
   * Segment the object and create child segment packets of the given Namespace.
   * @param nameSpace The Namespace to append segment packets to. This
   * ignores the Namespace from setNamespace().
   * @param object The object to segment.
   * @param useSignatureManifest (optional) If true, only use a
   * DigestSha256Signature on the segment packets and create a signed
   * _manifest packet as a child of the given Namespace. If omitted or false,
   * sign each segment packet individually.
   */
  void
  setObject
    (Namespace& nameSpace, const ndn::Blob& object,
     bool useSignatureManifest = false)
  {
    impl_->setObject(nameSpace, object, useSignatureManifest);
  }

  /**
   * Get the list of implicit digests from the _manifest packet and use it to
   * verify the segment implicit digests.
   * @param nameSpace The Namespace with child _manifest and segments.
   * @return True if the segment digests verify, false if not.
   */
  static bool
  verifyWithManifest(Namespace& nameSpace)
  {
    return Impl::verifyWithManifest(nameSpace);
  }

  static const ndn::Name::Component&
  getNAME_COMPONENT_MANIFEST() { return getValues().NAME_COMPONENT_MANIFEST; }

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
     * @param onSegment See the SegmentStreamHandler constructor.
     */
    Impl(const OnSegment& onSegment);

    uint64_t
    addOnSegment(const OnSegment& onSegment);

    void
    removeCallback(uint64_t callbackId);

    int
    getInterestPipelineSize() { return interestPipelineSize_; }

    void
    setInterestPipelineSize(int interestPipelineSize);

    int
    getInitialInterestCount() { return initialInterestCount_; }

    void
    setInitialInterestCount(int initialInterestCount);

    size_t
    getMaxSegmentPayloadLength() { return maxSegmentPayloadLength_; }

    void
    setMaxSegmentPayloadLength(size_t maxSegmentPayloadLength);

    void
    setObject
      (Namespace& nameSpace, const ndn::Blob& object, bool useSignatureManifest);

    static bool
    verifyWithManifest(Namespace& nameSpace);

    void
    onNamespaceSet(Namespace* nameSpace);

  private:
    /**
     * Start fetching segment Data packets and adding them as children of
     * getNamespace(), calling any onSegment callbacks in order as the
     * segments are received. Even though the segments supplied to onSegment are
     * in order, note that children of the Namespace node are not necessarily
     * added in order.
     */
    bool
    onObjectNeeded
      (Namespace& nameSpace, Namespace& neededNamespace, uint64_t callbackId);

    void
    onStateChanged
      (Namespace& nameSpace, Namespace& changedNamespace, NamespaceState state,
       uint64_t callbackId);

    void
    requestNewSegments(int maxRequestedSegments);

    void
    fireOnSegment(Namespace* segmentNamespace);

    int maxReportedSegmentNumber_;
    bool didRequestFinalSegment_;
    int finalSegmentNumber_;
    int interestPipelineSize_;
    int initialInterestCount_;
    // The key is the callback ID. The value is the OnSegment function.
    std::map<uint64_t, OnSegment> onSegmentCallbacks_;
    uint64_t onObjectNeededId_;
    uint64_t onStateChangedId_;
    Namespace* namespace_;
    size_t maxSegmentPayloadLength_;
  };

  /**
   * Values holds values used by the static member values_.
   */
  class Values {
  public:
    Values()
    : NAME_COMPONENT_MANIFEST("_manifest")
    {}

    ndn::Name::Component NAME_COMPONENT_MANIFEST;
  };

  /**
   * Get the static Values object, creating it if needed. We do this explicitly
   * because some C++ environments don't handle static constructors well.
   * @return The static Values object.
   */
  static Values&
  getValues()
  {
    if (!values_)
      values_ = new Values();

    return *values_;
  }

  ndn::ptr_lib::shared_ptr<Impl> impl_;
  static Values* values_;
};

}

#endif
