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

#ifndef NDN_GENERALIZED_OBJECT_STREAM_HANDLER_HPP
#define NDN_GENERALIZED_OBJECT_STREAM_HANDLER_HPP

#include "generalized-object-handler.hpp"

namespace cnl_cpp {

/**
 * GeneralizedObjectStreamHandler extends Namespace::Handler and attaches to a
 * Namespace node to fetch the _latest packet and use the name in it to start
 * fetching the stream of generalized object using a GeneralizedObjectHandler.
 */
class GeneralizedObjectStreamHandler : public Namespace::Handler {
public:
  typedef ndn::func_lib::function<void
    (int sequenceNumber,
     const ndn::ptr_lib::shared_ptr<ContentMetaInfoObject>& contentMetaInfo,
     const ndn::ptr_lib::shared_ptr<Object>& object)> OnSequencedGeneralizedObject;

  /**
   * Create a GeneralizedObjectStreamHandler with the optional
   * onSequencedGeneralizedObject callback.
   * @param pipelineSize (optional) The pipeline size (number of objects, not
   * interests). The pipelineSize times the expected period between objects
   * should be less than the maximum interest lifetime.
   * @param onSequencedGeneralizedObject (optional) When the ContentMetaInfo is
   * received for a new sequence number and the hasSegments is false, this calls
   * onSequencedGeneralizedObject(sequenceNumber, contentMetaInfo, object) where
   * sequenceNumber is the new sequence number, contentMetaInfo is the
   * ContentMetaInfo and object is the "other" info as a BlobObject or possibly
   * deserialized into another type. If the hasSegments flag is true, when the
   * segments are received and assembled into a single block of memory, this
   * calls onSequencedGeneralizedObject(sequenceNumber, contentMetaInfo, object)
   * where sequenceNumber is the new sequence number, contentMetaInfo is the
   * ContentMetaInfo and object is the object that was assembled from the
   * segment contents as a BlobObject or possibly deserialized to another type.
   * If you don't supply an onGeneralizedObject callback here, you can call
   * addOnStateChanged on the Namespace object to which this is attached and
   * listen for the OBJECT_READY state.
   */
  GeneralizedObjectStreamHandler
    (int pipelineSize = 8,
     const OnSequencedGeneralizedObject& onSequencedGeneralizedObject =
       OnSequencedGeneralizedObject())
  : impl_(ndn::ptr_lib::make_shared<Impl>
          (pipelineSize, onSequencedGeneralizedObject))
  {
  }

  /**
   * Get the getGeneralizedObjectHandlerwhich is used to segment an object. You
   * can use this to set parameters such as
   * getGeneralizedObjectHandler().getSegmentedObjectHandler().setMaxSegmentPayloadLength().
   * @return The GeneralizedObjectHandler.
   */
  GeneralizedObjectHandler&
  getGeneralizedObjectHandler() { return impl_->getGeneralizedObjectHandler(); }

  /**
   * Prepare the generalized object as a child of the given sequence number
   * Namespace node under the getNamespace() node, according to
   * GeneralizedObjectHandler.setObject. Also prepare to answer requests for the
   * _latest packet which refer to the given sequence number Name.
   * @param sequenceNumber The sequence number to publish. This updates the
   * value for getProducedSequenceNumber()
   * @param object The object to publish as a Generalized Object.
   * @param contentType The content type for the content _meta packet.
   */
  void
  setObject
    (int sequenceNumber, const ndn::Blob& object, const std::string& contentType)
  {
    impl_->setObject(sequenceNumber, object, contentType);
  }

  /**
   * Publish an object for the next sequence number by calling setObject where
   * the sequenceNumber is the current getProducedSequenceNumber() + 1.
   * @param object The object to publish as a Generalized Object.
   * @param contentType The content type for the content _meta packet.
   */
  void
  addObject(const ndn::Blob& object, const std::string& contentType)
  {
    setObject(getProducedSequenceNumber() + 1, object, contentType);
  }

  /**
   * Get the latest produced sequence number.
   * @return The latest produced sequence number, or -1 if none have been produced.
   */
  int
  getProducedSequenceNumber() { return impl_->getProducedSequenceNumber(); }

  /**
   * Get the freshness period to use for the produced _latest data packet.
   * @return The freshness period in milliseconds.
   */
  ndn::Milliseconds
  getLatestPacketFreshnessPeriod() { return impl_->getLatestPacketFreshnessPeriod(); }

  /**
   * Set the freshness period to use for the produced _latest data packet.
   * @param latestPacketFreshnessPeriod The freshness period in milliseconds.
   */
  void
  setLatestPacketFreshnessPeriod(ndn::Milliseconds latestPacketFreshnessPeriod)
  {
    impl_->setLatestPacketFreshnessPeriod(latestPacketFreshnessPeriod);
  }

  static const ndn::Name::Component&
  getNAME_COMPONENT_LATEST() { return getValues().NAME_COMPONENT_LATEST; }

protected:
  virtual void
  onNamespaceSet() { impl_->onNamespaceSet(&getNamespace()); }

private:
  /**
   * GeneralizedObjectStreamHandler::Impl does the work of
   * GeneralizedObjectStreamHandler. It is a separate class so that
   * GeneralizedObjectStreamHandler can create an instance in a shared_ptr to
   * use in callbacks.
   */
  class Impl : public ndn::ptr_lib::enable_shared_from_this<Impl> {
  public:
    /**
     * Create a new Impl, which should belong to a shared_ptr.
     * @param onSegmentedObject See the GeneralizedObjectHandler constructor.
     */
    Impl(int pipelineSize,
         const OnSequencedGeneralizedObject& onSequencedGeneralizedObject);

    GeneralizedObjectHandler&
    getGeneralizedObjectHandler() { return generalizedObjectHandler_; }

    void
    setObject
      (int sequenceNumber, const ndn::Blob& object,
       const std::string& contentType);

    int
    getProducedSequenceNumber() { return producedSequenceNumber_; }

    ndn::Milliseconds
    getLatestPacketFreshnessPeriod() { return latestPacketFreshnessPeriod_; }

    void
    setLatestPacketFreshnessPeriod(ndn::Milliseconds latestPacketFreshnessPeriod)
    {
      latestPacketFreshnessPeriod_ = latestPacketFreshnessPeriod;
    }

    void
    onNamespaceSet(Namespace* nameSpace);

  private:
    /**
     * This is called for object needed at the Handler's namespace. If
     * neededNamespace is the Handler's Namespace (called by the appliction),
     * then fetch the _latest packet. If neededNamespace is for the _latest 
     * packet (from an incoming Interest), produce the _latest packet for the
     * current sequence number.
     */
    bool
    onObjectNeeded
      (Namespace& nameSpace, Namespace& neededNamespace, uint64_t callbackId);

    /**
     * This is called when a packet arrives. Parse the _latest packet and start
     * fetching the stream of GeneralizedObject by sequence number.
     */
    void
    onStateChanged
      (Namespace& nameSpace, Namespace& changedNamespace, NamespaceState state,
       uint64_t callbackId);

    /**
     * This is called when the GeneralizedObject arrives. Call the
     * OnSequencedGeneralizedObject callback, where we pass through the
     * sequenceNumber.
     */
    void
    onGeneralizedObject
      (const ndn::ptr_lib::shared_ptr<ContentMetaInfoObject>& contentMetaInfo,
       const ndn::ptr_lib::shared_ptr<Object>& object,
       int sequenceNumber);

    /**
     * Request new child sequence numbers, up to the pipelineSize_.
     */
    void
    requestNewSequenceNumbers();

    OnSequencedGeneralizedObject onSequencedGeneralizedObject_;
    Namespace* namespace_;
    Namespace* latestNamespace_;
    int producedSequenceNumber_;
    int pipelineSize_;
    ndn::Milliseconds latestPacketFreshnessPeriod_;
    GeneralizedObjectHandler generalizedObjectHandler_;
    int maxReportedSequenceNumber_;
  };

  /**
   * Values holds values used by the static member values_.
   */
  class Values {
  public:
    Values()
    : NAME_COMPONENT_LATEST("_latest")
    {}

    ndn::Name::Component NAME_COMPONENT_LATEST;
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
