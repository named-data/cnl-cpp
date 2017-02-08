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

#ifndef NDN_NAMESPACE_HPP
#define NDN_NAMESPACE_HPP

#include <map>
#include <ndn-cpp/face.hpp>
#ifdef NDN_CPP_HAVE_BOOST_ASIO
#include <boost/atomic.hpp>
#endif

namespace cnl_cpp {

/**
 * Namespace is the main class that represents the name tree and related
 * operations to manage it.
 */
class Namespace {
public:
  typedef ndn::func_lib::function<void
    (Namespace& nameSpace, Namespace& addedNamespace,
     uint64_t callbackId)> OnNameAdded;

  typedef ndn::func_lib::function<void
    (Namespace& nameSpace, Namespace& contentNamespace,
     uint64_t callbackId)> OnContentSet;

  typedef ndn::func_lib::function<void
    (const ndn::ptr_lib::shared_ptr<ndn::Data>& data,
     const ndn::Blob& content)> OnContentTransformed;

  typedef ndn::func_lib::function<void
    (const ndn::ptr_lib::shared_ptr<ndn::Data>& data,
     const OnContentTransformed& onContentTransformed)> TransformContent;

  /**
   * Create a Namespace object with the given name, and with no parent. This is
   * the root of the name tree. To create child nodes, use
   * myNamespace.getChild("foo") or myNamespace["foo"].
   * @param name The name of this root node in the namespace. This makes a copy
   * of the name.
   */
  Namespace(const ndn::Name& name)
  : impl_(new Impl(*this, name))
  {
  }

  /**
   * Get the name of this node in the name tree. This includes the name
   * components of parent nodes. To get the name component of just this node,
   * use getName()[-1].
   * @return The name of this namespace. NOTE: You must not change the name -
   * if you need to change it then make a copy.
   */
  const ndn::Name&
  getName() const { return impl_->getName(); }

  /**
   * Get the parent namespace.
   * @return The parent namespace, or null if this is the root of the tree.
   */
  Namespace*
  getParent() { return impl_->getParent(); }

  /**
   * Get the root namespace (which has no parent node).
   * @return The root namespace.
   */
  Namespace&
  getRoot() { return impl_->getRoot(); }

  /**
   * Check if this node in the namespace has the given child.
   * @param component The name component of the child.
   * @return True if this has a child with the name component.
   */
  bool
  hasChild(const ndn::Name::Component& component) const
  {
    return impl_->hasChild(component);
  }

  /**
   * Get a child, creating it if needed. This is equivalent to
   * namespace[component]. If a child is created, this calls callbacks as
   * described by addOnNameAdded.
   * @param component The name component of the immediate child.
   * @return The child Namespace object.
   */
  Namespace&
  getChild(const ndn::Name::Component& component)
  {
    return impl_->getChild(component);
  }

  /**
   * Get a child (or descendant), creating it if needed. This is equivalent to
   * namespace[descendantName]. If a child is created, this calls callbacks as
   * described by addOnNameAdded (but does not call the callbacks when creating
   * intermediate nodes).
   * @param descendantName Find or create the descendant node with the Name
   * (which must have this node's name as a prefix).
   * @return The child Namespace object. However, if name equals the name of
   * this Namespace, then just return this Namespace.
   * @throws runtime_error if the name of this Namespace node is not a prefix of
   * the given Name.
   */
  Namespace&
  getChild(const ndn::Name& descendantName)
  {
    return impl_->getChild(descendantName);
  }

  /**
   * Get a list of the name component of all child nodes.
   * @return A fresh sorted list of the name component of all child nodes. This
   * remains the same if child nodes are added or deleted.
   */
  ndn::ptr_lib::shared_ptr<std::vector<ndn::Name::Component>>
  getChildComponents() { return impl_->getChildComponents(); }

  /**
   * Attach the Data packet to this Namespace. This calls callbacks as described
   * by addOnContentSet. If a Data packet is already attached, do nothing.
   * @param data The Data packet object whose name must equal the name in this
   * Namespace node. To get the right Namespace, you can use
   * getChild(data.getName()). For efficiency, this does not copy the Data
   * packet object. If your application may change the object later, then you
   * must call setData with a copy of the object.
   * @throws runtime_error if the Data packet name does not equal the name of
   * this Namespace node.
   */
  void
  setData(const ndn::ptr_lib::shared_ptr<ndn::Data>& data)
  {
    impl_->setData(data);
  }

  /**
   * Get the Data packet attached to this Namespace object. Note that
   * getContent() may be different than the content in the attached Data packet
   * (for example if the content is decrypted). To get the content, you should
   * use getContent() instead of getData().getContent(). Also, the Data packet
   * name is the same as the name of this Namespace node, so you can simply use
   * getName() instead of getData().getName(). You should only use getData() to
   * get other information such as the MetaInfo.
   * @return The Data packet object, or null if not set.
   */
  const ndn::ptr_lib::shared_ptr<ndn::Data>&
  getData() { return impl_->getData(); }

  /**
   * Get the content attached to this Namespace object. Note that getContent() 
   * may be different than the content in the attached Data packet (for example 
   * if the content is decrypted).
   * @return The content Blob, or an isNull Blob if not set.
   */
  const ndn::Blob&
  getContent() { return impl_->getContent(); }

  /**
   * Add an OnNameAdded callback. When a new name is added to this namespace at
   * this node or any children, this calls onNameAdded as described below.
   * @param onNameAdded This calls
   * onNameAdded(nameSpace, addedNamespace, callbackId)
   * where nameSpace is this Namespace, addedNamespace is the Namespace of the
   * added name, and callbackId is the callback ID returned by this method.
   * NOTE: The library will log any exceptions thrown by this callback, but for
   * better error handling the callback should catch and properly handle any
   * exceptions.
   * @return The callback ID which you can use in removeCallback().
   */
  uint64_t
  addOnNameAdded(const OnNameAdded& onNameAdded)
  {
    return impl_->addOnNameAdded(onNameAdded);
  }

  /**
   * Add an OnContentSet callback. When the content has been set for this
   * Namespace node or any children, this calls onContentSet as described below.
   * @param onContentSet This calls
   * onContentSet(nameSpace, contentNamespace, callbackId)
   * where nameSpace is this Namespace, contentNamespace is the Namespace where
   * the content was set, and callbackId is the callback ID returned by this
   * method. If you only care if the content has been set for this Namespace
   * (and not any of its children) then your callback can check
   * "if contentNamespace == nameSpace". To get the content or data packet, use
   * contentNamespace.getContent() or contentNamespace.getData().
   * NOTE: The library will log any exceptions thrown by this callback, but for
   * better error handling the callback should catch and properly handle any
   * exceptions.
   * @return The callback ID which you can use in removeCallback().
   */
  uint64_t
  addOnContentSet(const OnContentSet& onContentSet)
  {
    return impl_->addOnContentSet(onContentSet);
  }

  /**
   * Set the Face used when expressInterest is called on this or child nodes
   * (unless a child node has a different Face).
   * TODO: Replace this by a mechanism for requesting a Data object which is
   * more general than a Face network operation.
   * @param face The Face object. If this Namespace object already has a Face
   * object, it is replaced.
   */
  void
  setFace(ndn::Face* face) { impl_->setFace(face); }

  /**
   * Call expressInterest on this (or a parent's) Face where the interest name
   * is the name of this Namespace node. When the Data packet is received this
   * calls setData, so you should use a callback with addOnContentSet. This uses
   * ExponentialReExpress to re-express a timed-out interest with longer
   * lifetimes.
   * TODO: How to alert the application on a final interest timeout?
   * TODO: Replace this by a mechanism for requesting a Data object which is
   * more general than a Face network operation.
   * @param interestTemplate (optional) The interest template for
   * expressInterest. If omitted, just use a default interest lifetime.
   * @throws runtime_error if a Face object has not been set for this or a
   * parent Namespace node.
   */
  void
  expressInterest(const ndn::Interest *interestTemplate = 0)
  {
    impl_->expressInterest(interestTemplate);
  }

  /**
   * Remove the callback with the given callbackId. This does not search for the
   * callbackId in child nodes. If the callbackId isn't found, do nothing.
   * @param callbackId The callback ID returned, for example, from
   * addOnNameAdded.
   */
  void
  removeCallback(uint64_t callbackId) { impl_->removeCallback(callbackId); }

  /**
   * Get the next unique callback ID. This uses an atomic_uint64_t to be thread 
   * safe. This is an internal method only meant to be called by library
   * classes; the application should not call it.
   * @return The next callback ID.
   */
  static uint64_t
  getNextCallbackId();

  Namespace&
  operator [] (const ndn::Name::Component& component)
  {
    return getChild(component);
  }

  Namespace&
  operator [] (const ndn::Name& descendantName)
  {
    return getChild(descendantName);
  }

private:
  /**
   * Namespace::Impl does the work of Namespace. It is a separate class so that
   * Namespace can create an instance in a shared_ptr to use in callbacks.
   */
  class Impl : public ndn::ptr_lib::enable_shared_from_this<Impl> {
  public:
    /**
     * Create a new Impl, which should belong to a shared_ptr.
     * @param outerNamespace The Namespace which is creating this inner Imp.
     * @name See the Namespace constructor.
     */
    Impl(Namespace& outerNamespace, const ndn::Name& name)
    : outerNamespace_(outerNamespace), name_(name), parent_(0), face_(0),
      transformContent_(TransformContent())
    {
      defaultInterestTemplate_.setInterestLifetimeMilliseconds(4000.0);
    }

    const ndn::Name&
    getName() const { return name_; }

    Namespace*
    getParent() { return parent_; }

    Namespace&
    getRoot();

    bool
    hasChild(const ndn::Name::Component& component) const
    {
      return children_.find(component) != children_.end();
    }

    Namespace&
    getChild(const ndn::Name::Component& component)
    {
      std::map<ndn::Name::Component, ndn::ptr_lib::shared_ptr<Namespace>>::iterator
        child = children_.find(component);
      if (child != children_.end())
        return *child->second;
      else
        return createChild(component, true);
    }

    Namespace&
    getChild(const ndn::Name& descendantName);

    ndn::ptr_lib::shared_ptr<std::vector<ndn::Name::Component>>
    getChildComponents();

    void
    setData(const ndn::ptr_lib::shared_ptr<ndn::Data>& data);

    const ndn::ptr_lib::shared_ptr<ndn::Data>&
    getData() { return data_; }

    const ndn::Blob&
    getContent() { return content_; }

    uint64_t
    addOnNameAdded(const OnNameAdded& onNameAdded);

    uint64_t
    addOnContentSet(const OnContentSet& onContentSet);

    void
    setFace(ndn::Face* face) { face_ = face; }

    void
    expressInterest(const ndn::Interest *interestTemplate);

    void
    removeCallback(uint64_t callbackId);

  private:
    /**
     * Get the Face set by setFace on this or a parent Namespace node.
     * @return The Face, or null if not set on this or any parent.
     */
    ndn::Face*
    getFace();

    /**
     * Get the TransformContent callback on this or a parent Namespace node.
     * @return The TransformContent callback, or a default TransformContent() if
     * not set on this or any parent.
     */
    TransformContent
    getTransformContent();

    /**
     * Create the child with the given name component and add it to this
     * namespace. This private method should only be called if the child does not
     * already exist. The application should use getChild.
     * @param component The name component of the child.
     * @param fireCallbacks If true, call fireOnNameAdded for this and all parent
     * nodes. If false, don't call callbacks (for example if creating intermediate
     * nodes).
     * @return The child Namespace object.
     */
    Namespace&
    createChild(const ndn::Name::Component& component, bool fireCallbacks);

    void
    fireOnNameAdded(Namespace& addedNamespace);

    /**
     * Set data_ and content_ to the given values and fire the OnContentSet
     * callbacks. This may be called from a transformContent_ handler invoked by
     * setData.
     * @param data The Data packet object given to setData.
     * @param content The content which may have been processed from the Data
     * packet, e.g. by decrypting.
     */
    void
    onContentTransformed
      (const ndn::ptr_lib::shared_ptr<ndn::Data>& data, const ndn::Blob& content);

    void
    fireOnContentSet(Namespace& contentNamespace);

    void
    onData(const ndn::ptr_lib::shared_ptr<const ndn::Interest>& interest,
           const ndn::ptr_lib::shared_ptr<ndn::Data>& data);

    Namespace& outerNamespace_;
    ndn::Name name_;
    Namespace* parent_;
    // The key is a Name::Component. The value is the child Namespace.
    std::map<ndn::Name::Component, ndn::ptr_lib::shared_ptr<Namespace>> children_;
    ndn::ptr_lib::shared_ptr<ndn::Data> data_;
    ndn::Blob content_;
    ndn::Face* face_;
    // The key is the callback ID. The value is the OnNameAdded function.
    std::map<uint64_t, OnNameAdded> onNameAddedCallbacks_;
    // The key is the callback ID. The value is the OnContentSet function.
    std::map<uint64_t, OnContentSet> onContentSetCallbacks_;
    TransformContent transformContent_;
    ndn::Interest defaultInterestTemplate_;
  };

  ndn::ptr_lib::shared_ptr<Impl> impl_;
#ifdef NDN_CPP_HAVE_BOOST_ASIO
  // Multi-threading is enabled, so different threads my access this shared
  // static variable. Use atomic_uint64_t to be thread safe.
  static boost::atomic_uint64_t lastCallbackId_;
#else
  // Not using Boost asio multi-threading, so we can use a normal uint64_t.
  static uint64_t lastCallbackId_;
#endif
};

}

#endif
