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

#ifndef CNL_CPP_SEGMENTED_OBJECT_HANDLER_HPP
#define CNL_CPP_SEGMENTED_OBJECT_HANDLER_HPP

#include "segment-stream-handler.hpp"

namespace cnl_cpp {

/**
 * SegmentedObjectHandler extends SegmentStreamHandler and assembles the
 * contents of child segments into a single block of memory.
 */
class SegmentedObjectHandler : public SegmentStreamHandler {
public:
  typedef ndn::func_lib::function<void(ndn::Blob contentBlob)> OnSegmentedObject;

  /**
   * Create a SegmentedObjectHandler with the optional onSegmentedObject callback.
   * @param onSegmentedObject (optional) When the child segments are assembled
   * into a single block of memory, this calls onSegmentedObject(contentBlob)
   * where contentBlob is the Blob assembled from the contents. If you don't
   * supply an onSegmentedObject callback here, you can call addOnStateChanged
   * on the Namespace object to which this is attached and listen for the
   * OBJECT_READY state.
   */
  SegmentedObjectHandler
    (const OnSegmentedObject& onSegmentedObject = OnSegmentedObject())
  : impl_(ndn::ptr_lib::make_shared<Impl>(onSegmentedObject))
  {
    impl_->initialize(this);
  }

protected:
  virtual void
  onNamespaceSet()
  {
    // Call the base class method.
    SegmentStreamHandler::onNamespaceSet();

    impl_->onNamespaceSet(&getNamespace());
  }

private:
  /**
   * SegmentedObjectHandler::Impl does the work of SegmentedObjectHandler. It is
   * a separate class so that SegmentedObjectHandler can create an instance in a
   * shared_ptr to use in callbacks.
   */
  class Impl : public ndn::ptr_lib::enable_shared_from_this<Impl> {
  public:
    /**
     * Create a new Impl, which should belong to a shared_ptr, then call
     * initialize().
     * @param onSegmentedObject See the SegmentedObjectHandler constructor.
     */
    Impl(const OnSegmentedObject& onSegmentedObject);

    /**
     * Complete the work of the constructor. This is needed because we can't
     * call shared_from_this() in the constructor.
     * @param outerHandler A pointer to the outer handler which shouldn't be
     * saved since it might be destroyed later.
     */
    void
    initialize(SegmentedObjectHandler* outerHandler);

    void
    onNamespaceSet(Namespace* nameSpace)
    {
      // Store getNamespace() in impl_. We do this instead of keeping a pointer to
      // this outer Handler object since it might be destroyed.
      namespace_ = nameSpace;
    }

  private:
    void
    onSegment(Namespace* segmentNamespace);

    bool finished_;
    std::vector<ndn::Blob> segments_;
    size_t totalSize_;
    OnSegmentedObject onSegmentedObject_;
    Namespace* namespace_;
  };

  ndn::ptr_lib::shared_ptr<Impl> impl_;
};

}

#endif
