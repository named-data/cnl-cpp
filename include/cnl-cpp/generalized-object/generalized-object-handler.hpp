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

#ifndef CNL_CPP_GENERALIZED_OBJECT_HANDLER_HPP
#define CNL_CPP_GENERALIZED_OBJECT_HANDLER_HPP

#include <cnl-cpp/generalized-object/content-meta-info-object.hpp>
#include "../segmented-object-handler.hpp"

namespace cnl_cpp {

/**
 * GeneralizedObjectHandler extends Namespace::Handler and attaches to a
 * Namespace node to fetch the _meta packet for a generalized object and, if
 * necessary, assemble the contents of segment packets into a single block of
 * memory.
 */
class GeneralizedObjectHandler : public Namespace::Handler {
public:
  typedef ndn::func_lib::function<void
    (const ndn::ptr_lib::shared_ptr<ContentMetaInfoObject>& contentMetaInfo,
     Namespace& objectNamespace)> OnGeneralizedObject;

  /**
   * Create a GeneralizedObjectHandler with the optional onGeneralizedObject
   * callback.
   * @param nameSpace (optional) Set the Namespace that this handler is attached
   * to. If omitted or null, you can call setNamespace() later.
   * @param onGeneralizedObject (optional) When the ContentMetaInfo is received
   * and the hasSegments is false, this calls 
   * onGeneralizedObject(contentMetaInfo, objectNamespace) where contentMetaInfo
   * is the ContentMetaInfo and objectNamespace.getObject() is the "other" info
   * as a BlobObject or possibly deserialized into another type. If the
   * hasSegments flag is true, when the segments are received and assembled into
   * a single block of memory, this calls
   * onGeneralizedObject(contentMetaInfo, objectNamespace) where contentMetaInfo
   * is the ContentMetaInfo and objectNamespace.getObject() is the object that
   * was assembled from the segment contents as a BlobObject or possibly
   * deserialized to another type. If you don't supply an onGeneralizedObject
   * callback here, you can call addOnStateChanged on the Namespace object to
   * which this is attached and listen for the OBJECT_READY state.
   */
  GeneralizedObjectHandler
    (Namespace* nameSpace = 0,
     const OnGeneralizedObject& onGeneralizedObject = OnGeneralizedObject())
  : impl_(ndn::ptr_lib::make_shared<Impl>(onGeneralizedObject))
  {
    if (nameSpace)
      setNamespace(nameSpace);
  }

  /**
   * Set the Namespace that this handler is attached to. (This is
   * automatically called when you call Namespace.setHandler.) This method
   * does not attach this Handler to the Namespace.
   * @param nameSpace The Handler's Namespace.
   * @return This GeneralizedObjectHandler so you can chain calls to update values.
   * @throws runtime_error if this Handler is already attached to a different
   * Namespace.
   */
  GeneralizedObjectHandler&
  setNamespace(Namespace* nameSpace)
  {
    // Call the base implementation and cast the return value.
    return static_cast<GeneralizedObjectHandler&>(Handler::setNamespace(nameSpace));
  }

  /**
   * Set the number of name components after the object Namespace for fetching
   * the generalized object, as described below.
   * @param nComponentsAfterObjectNamespace If nComponentsAfterObjectNamespace
   * is zero (the default), then require that the _meta and segment nodes are
   * directly under the given Namespace name for the object. If
   * nComponentsAfterObjectNamespace is greater than zero, allow exactly this
   * number of name components after the given Namespace name but before the
   * _meta and segment packets. In this case, the value of these name
   * components may not be known before the first packet is fetched.
   * @throws runtime_error if nComponentsAfterObjectNamespace is negative.
   */
  void
  setNComponentsAfterObjectNamespace(int nComponentsAfterObjectNamespace)
  {
    impl_->setNComponentsAfterObjectNamespace(nComponentsAfterObjectNamespace);
  }

  /**
   * Create a _meta packet with the given contentType and as a child of the
   * given Namespace. If the object is large enough to require segmenting, also
   * segment the object and create child segment packets plus a signature
   * _manifest packet of the given Namespace.
   * @param nameSpace The Namespace to append segment packets to. This
   * ignores the Namespace from setNamespace().
   * @param object The object to publish as a Generalized Object.
   * @param contentType The content type for the content _meta packet.
   */
  void
  setObject
    (Namespace& nameSpace, const ndn::Blob& object,
     const std::string& contentType)
  {
    impl_->setObject(nameSpace, object, contentType);
  }

  /**
   * Get the number of outstanding interests which this maintains while fetching
   * segments (if the ContentMetaInfo hasSegments is true).
   * @return The Interest pipeline size.
   */
  int
  getInterestPipelineSize() { return impl_->getInterestPipelineSize(); }

  /**
   * Set the number of outstanding interests which this maintains while fetching
   * segments (if the ContentMetaInfo hasSegments is true).
   * @param interestPipelineSize The Interest pipeline size.
   * @throws runtime_error if interestPipelineSize is less than 1.
   */
  void
  setInterestPipelineSize(int interestPipelineSize)
  {
    impl_->setInterestPipelineSize(interestPipelineSize);
  }

  /**
   * Get the initial Interest count (if the ContentMetaInfo hasSegments is true),
   * as described in setInitialInterestCount.
   * @return The initial Interest count.
   */
  int
  getInitialInterestCount() { return impl_->getInitialInterestCount(); }

  /**
   * Set the number of initial Interests to send for segments (if the
   * ContentMetaInfo hasSegments is true). By default this
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
   * larger payload into segments (if the ContentMetaInfo hasSegments is true).
   * @return The maximum payload length.
   */
  size_t
  getMaxSegmentPayloadLength() { return impl_->getMaxSegmentPayloadLength(); }

  /**
   * Set the maximum length of the payload of one segment, used to split a
   * larger payload into segments (if the ContentMetaInfo hasSegments is true).
   * @param maxSegmentPayloadLength The maximum payload length.
   */
  void
  setMaxSegmentPayloadLength(size_t maxSegmentPayloadLength)
  {
    impl_->setMaxSegmentPayloadLength(maxSegmentPayloadLength);
  }

  static const ndn::Name::Component&
  getNAME_COMPONENT_META() { return getValues().NAME_COMPONENT_META; }

protected:
  virtual void
  onNamespaceSet() { impl_->onNamespaceSet(&getNamespace()); }

private:
  /**
   * GeneralizedObjectHandler::Impl does the work of GeneralizedObjectHandler.
   * It is a separate class so that GeneralizedObjectHandler can create an
   * instance in a shared_ptr to use in callbacks.
   */
  class Impl : public ndn::ptr_lib::enable_shared_from_this<Impl> {
  public:
    /**
     * Create a new Impl, which should belong to a shared_ptr.
     * @param onSegmentedObject See the GeneralizedObjectHandler constructor.
     */
    Impl(const OnGeneralizedObject& onGeneralizedObject);

    void
    setNComponentsAfterObjectNamespace(int nComponentsAfterObjectNamespace);

    void
    onNamespaceSet(Namespace* nameSpace);

    void
    setObject
      (Namespace& nameSpace, const ndn::Blob& object,
       const std::string& contentType);

    int
    getInterestPipelineSize()
    {
      // Pass through to the SegmentedObjectHandler.
      return segmentedObjectHandler_->getInterestPipelineSize();
    }

    void
    setInterestPipelineSize(int interestPipelineSize)
    {
      // Pass through to the SegmentedObjectHandler.
      segmentedObjectHandler_->setInterestPipelineSize(interestPipelineSize);
    }

    int
    getInitialInterestCount()
    {
      // Pass through to the SegmentedObjectHandler.
      return segmentedObjectHandler_->getInitialInterestCount();
    }

    void
    setInitialInterestCount(int initialInterestCount)
    {
      // Pass through to the SegmentedObjectHandler.
      segmentedObjectHandler_->setInitialInterestCount(initialInterestCount);
    }

    size_t
    getMaxSegmentPayloadLength()
    {
      // Pass through to the SegmentedObjectHandler.
      return segmentedObjectHandler_->getMaxSegmentPayloadLength();
    }

    void
    setMaxSegmentPayloadLength(size_t maxSegmentPayloadLength)
    {
      // Pass through to the SegmentedObjectHandler.
      segmentedObjectHandler_->setMaxSegmentPayloadLength(maxSegmentPayloadLength);
    }

  private:
    bool
    onObjectNeeded
      (Namespace& nameSpace, Namespace& neededNamespace, uint64_t callbackId);

    /**
     * This is called when the SegmentedObjectHandler finishes.
     * @param objectNamespace The Namespace with the deserialized object from
     * the SegmentedObjectHandler.
     * @param contentMetaInfo The ContentMetaInfoObject from canDeserialize.
     */
    void onSegmentedObject
      (Namespace& objectNamespace,
       const ndn::ptr_lib::shared_ptr<ContentMetaInfoObject>& contentMetaInfo);

    /**
     * This is called by Namespace when a packet is received. If this is the
     * _meta packet, then decode it.
     */
    bool
    onDeserializeNeeded
      (Namespace& blobNamespace, const ndn::Blob& blob,
       const Namespace::Handler::OnDeserialized& onDeserialized,
       uint64_t callbackId);

    ndn::ptr_lib::shared_ptr<SegmentedObjectHandler> segmentedObjectHandler_;
    OnGeneralizedObject onGeneralizedObject_;
    Namespace* namespace_;
    int nComponentsAfterObjectNamespace_;
    uint64_t onObjectNeededId_;
    uint64_t onDeserializeNeededId_;
  };

  /**
   * Values holds values used by the static member values_.
   */
  class Values {
  public:
    Values()
    : NAME_COMPONENT_META("_meta")
    {}

    ndn::Name::Component NAME_COMPONENT_META;
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
