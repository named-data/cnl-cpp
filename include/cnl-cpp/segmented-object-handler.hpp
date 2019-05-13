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
  typedef ndn::func_lib::function<void(Namespace& objectNamespace)> OnSegmentedObject;

  /**
   * Create a SegmentedObjectHandler with the optional onSegmentedObject callback.
   * @param nameSpace (optional) Set the Namespace that this handler is attached
   * to. If omitted or null, you can call setNamespace() later.
   * @param onSegmentedObject (optional) If supplied, this calls
   * addOnSegmentedObject(onSegmentedObject). You may also call
   * addOnSegmentedObject directly.
   */
  SegmentedObjectHandler
    (Namespace* nameSpace = 0,
     const OnSegmentedObject& onSegmentedObject = OnSegmentedObject())
  : impl_(ndn::ptr_lib::make_shared<Impl>(onSegmentedObject))
  {
    impl_->initialize(this);

    if (nameSpace)
      setNamespace(nameSpace);
  }

  /**
   * Set the Namespace that this handler is attached to.
   * @param nameSpace The Handler's Namespace.
   * @return This SegmentedObjectHandler so you can chain calls to update values.
   * @throws runtime_error if this Handler is already attached to a different
   * Namespace.
   */
  SegmentedObjectHandler&
  setNamespace(Namespace* nameSpace)
  {
    // Call the base implementation and cast the return value.
    return static_cast<SegmentedObjectHandler&>
      (SegmentStreamHandler::setNamespace(nameSpace));
  }

  /**
   * Add an OnSegmentedObject callback. When the child segments are assembled
   * into a single block of memory, this calls onSegmentedObject as described
   * below.
   * @param onSegmentedObject This calls onSegmentedObject(objectNamespace) 
   * where objectNamespace.getObject() is the object that was assembled from the
   * segment contents and deserialized.
   * NOTE: The library will log any exceptions thrown by this callback, but for
   * better error handling the callback should catch and properly handle any
   * exceptions.
   * @return The callback ID which you can use in removeCallback().
   */
  uint64_t
  addOnSegmentedObject(const OnSegmentedObject& onSegmentedObject)
  {
    return impl_->addOnSegmentedObject(onSegmentedObject);
  }

  /**
   * Remove the callback with the given callbackId. This does not search for the
   * callbackId in child nodes. If the callbackId isn't found, do nothing.
   * @param callbackId The callback ID returned, for example, from
   * addOnSegmentedObject.
   */
  void
  removeCallback(uint64_t callbackId) { impl_->removeCallback(callbackId); }

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

    uint64_t
    addOnSegmentedObject(const OnSegmentedObject& onSegmentedObject);

    void
    removeCallback(uint64_t callbackId);

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

    void
    fireOnSegmentedObject(Namespace& objectNamespace);

    std::vector<ndn::Blob> segments_;
    size_t totalSize_;
    // The key is the callback ID. The value is the OnSegmentedObject function.
    std::map<uint64_t, OnSegmentedObject> onSegmentedObjectCallbacks_;
    Namespace* namespace_;
  };

  ndn::ptr_lib::shared_ptr<Impl> impl_;
};

}

#endif
