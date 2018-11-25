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
     const ndn::ptr_lib::shared_ptr<Object>& object)> OnGeneralizedObject;

  /**
   * Create a GeneralizedObjectHandler with the optional onGeneralizedObject
   * callback.
   * @param onGeneralizedObject (optional) When the ContentMetaInfo is received
   * and the hasSegments is false, this calls 
   * onGeneralizedObject(contentMetaInfo, object) where contentMetaInfo is the
   * ContentMetaInfo and object is the "other" info as a BlobObject or possibly
   * deserialized into another type. If the hasSegments flag is true, when the
   * segments are received and assembled into a single block of memory, this
   * calls onGeneralizedObject(contentMetaInfo, object) where contentMetaInfo is
   * the ContentMetaInfo and object is the object that was assembled from the
   * segment contents as a BlobObject or possibly deserialized to another type.
   * If you don't supply an onGeneralizedObject callback here, you can call
   * addOnStateChanged on the Namespace object to which this is attached and
   * listen for the OBJECT_READY state.
   */
  GeneralizedObjectHandler
    (const OnGeneralizedObject& onGeneralizedObject = OnGeneralizedObject())
  : impl_(ndn::ptr_lib::make_shared<Impl>(onGeneralizedObject))
  {
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
   * Add an onSegment callback. When a new segment is available, this calls
   * onSegment as described below. Segments are supplied in order.
   * This is only used if the ContentMetaInfo hasSegments flag is true.
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
  addOnSegment(const SegmentedObjectHandler::OnSegment& onSegment)
  {
    return impl_->addOnSegment(onSegment);
  }

  /**
   * Remove the callback with the given callbackId. This does not search for the
   * callbackId in child nodes. If the callbackId isn't found, do nothing.
   * This is only used if the ContentMetaInfo hasSegments flag is true.
   * @param callbackId The callback ID returned, for example, from
   * addOnSegment.
   */
  void
  removeCallback(uint64_t callbackId) { impl_->removeCallback(callbackId); }

  /**
   * Get the number of outstanding interests which this maintains while fetching
   * segments.
   * This is only used if the ContentMetaInfo hasSegments flag is true.
   * @return The Interest pipeline size.
   */
  int
  getInterestPipelineSize() { return impl_->getInterestPipelineSize(); }

  /**
   * Set the number of outstanding interests which this maintains while fetching
   * segments.
   * This is only used if the ContentMetaInfo hasSegments flag is true.
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
   * This is only used if the ContentMetaInfo hasSegments flag is true.
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
   * This is only used if the ContentMetaInfo hasSegments flag is true.
   * @param initialInterestCount The initial Interest count.
   * @throws runtime_error if initialInterestCount is less than 1.
   */
  void
  setInitialInterestCount(int initialInterestCount)
  {
    impl_->setInitialInterestCount(initialInterestCount);
  }

  static const ndn::Name::Component&
  getNAME_COMPONENT_META() { return getValues().NAME_COMPONENT_META; }

  static const ndn::Name::Component&
  getNAME_COMPONENT_MANIFEST() { return getValues().NAME_COMPONENT_MANIFEST; }

  /**
   * This is called by Namespace when a packet is received. If this is the
   * _meta packet, then decode it.
   */
  virtual bool
  canDeserialize
    (Namespace& objectNamespace, const ndn::Blob& blob,
     const OnDeserialized& onDeserialized)
  {
    return impl_->canDeserialize(objectNamespace, blob, onDeserialized);
  }

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
    onNamespaceSet(Namespace* nameSpace);

    uint64_t
    addOnSegment(const SegmentStreamHandler::OnSegment& onSegment)
    {
      // Pass through to the segmentedObjectHandler_.
      return segmentedObjectHandler_->addOnSegment(onSegment);
    }

    void
    removeCallback(uint64_t callbackId)
    {
      // Pass through to the segmentedObjectHandler_.
      segmentedObjectHandler_->removeCallback(callbackId);
    }

    int
    getInterestPipelineSize()
    {
      // Pass through to the segmentedObjectHandler_.
      return segmentedObjectHandler_->getInterestPipelineSize();
    }

    void
    setInterestPipelineSize(int interestPipelineSize)
    {
      // Pass through to the segmentedObjectHandler_.
      segmentedObjectHandler_->setInterestPipelineSize(interestPipelineSize);
    }

    int
    getInitialInterestCount()
    {
      // Pass through to the segmentedObjectHandler_.
      return segmentedObjectHandler_->getInitialInterestCount();
    }

    void
    setInitialInterestCount(int initialInterestCount)
    {
      // Pass through to the segmentedObjectHandler_.
      segmentedObjectHandler_->setInitialInterestCount(initialInterestCount);
    }

    void
    setNamespace(Namespace* nameSpace) { namespace_ = nameSpace; }

    bool
    canDeserialize
      (Namespace& objectNamespace, const ndn::Blob& blob,
       const OnDeserialized& onDeserialized);

  private:
    bool
    onObjectNeeded
      (Namespace& nameSpace, Namespace& neededNamespace,
       uint64_t callbackId);

    /**
     * This is called when the SegmentedObjectHandler finishes.
     * @param object The deserialized object from the SegmentedObjectHandler.
     * @param contentMetaInfo The ContentMetaInfoObject from canDeserialize.
     */
    void onSegmentedObject
      (const ndn::ptr_lib::shared_ptr<Object>& object,
       const ndn::ptr_lib::shared_ptr<ContentMetaInfoObject>& contentMetaInfo);

    ndn::ptr_lib::shared_ptr<SegmentedObjectHandler> segmentedObjectHandler_;
    OnGeneralizedObject onGeneralizedObject_;
    Namespace* namespace_;
  };

  /**
   * Values holds values used by the static member values_.
   */
  class Values {
  public:
    Values()
    : NAME_COMPONENT_META("_meta"),
      NAME_COMPONENT_MANIFEST("_manifest")
    {}

    ndn::Name::Component NAME_COMPONENT_META;
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
