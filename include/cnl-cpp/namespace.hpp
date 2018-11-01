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

#ifndef CNL_CPP_NAMESPACE_HPP
#define CNL_CPP_NAMESPACE_HPP

#include <map>
#include <ndn-cpp/face.hpp>
#ifdef NDN_CPP_HAVE_BOOST_ASIO
#include <boost/atomic.hpp>
#endif

#include <ndn-cpp/security/v2/validation-error.hpp>
#include "blob-object.hpp"

namespace cnl_cpp {

/**
 * A NamespaceState specifies the state of a Namespace node.
 */
enum NamespaceState {
  NamespaceState_NAME_EXISTS =             0,
  NamespaceState_INTEREST_EXPRESSED =      1,
  NamespaceState_INTEREST_TIMEOUT =        2,
  NamespaceState_INTEREST_NETWORK_NACK =   3,
  NamespaceState_DATA_RECEIVED =           4,
  NamespaceState_DESERIALIZING =           5,
  NamespaceState_DECRYPTING =              6,
  NamespaceState_DECRYPTION_ERROR =        7,
  NamespaceState_PRODUCING_OBJECT =        8,
  NamespaceState_SERIALIZING =             9,
  NamespaceState_ENCRYPTING =             10,
  NamespaceState_ENCRYPTION_ERROR =       11,
  NamespaceState_SIGNING =                12,
  NamespaceState_SIGNING_ERROR =          13,
  NamespaceState_OBJECT_READY =           14,
  NamespaceState_OBJECT_READY_BUT_STALE = 15
};

/**
 * A NamespaceValidateState specifies the state of validating a Namespace node.
 */
enum NamespaceValidateState {
  NamespaceValidateState_WAITING_FOR_DATA = 0,
  NamespaceValidateState_VALIDATING =       1,
  NamespaceValidateState_VALIDATE_SUCCESS = 2,
  NamespaceValidateState_VALIDATE_FAILURE = 3
};

class PendingIncomingInterestTable;

/**
 * Namespace is the main class that represents the name tree and related
 * operations to manage it.
 */
class Namespace {
public:
  typedef ndn::func_lib::function<void
    (Namespace& nameSpace, Namespace& changedNamespace,
     NamespaceValidateState validateState,
     uint64_t callbackId)> OnValidateStateChanged;

  typedef ndn::func_lib::function<void
    (Namespace& nameSpace, Namespace& changedNamespace, NamespaceState state,
     uint64_t callbackId)> OnStateChanged;

  typedef ndn::func_lib::function<void
    (const ndn::ptr_lib::shared_ptr<Object>& object)> OnDeserialized;

  typedef ndn::func_lib::function<bool
    (Namespace& nameSpace, Namespace& neededNamespace,
     uint64_t callbackId)> OnObjectNeeded;

  class Impl;

  /**
   * Namespace::Handler is a base class for handler classes. See setHandler for
   * details.
   */
  class Handler {
  public:
    Handler()
    : namespace_(0)
    {}

    /**
     * Set the Namespace that this handler is attached to. (This is
     * automatically called when you call Namespace.setHandler.)
     * @param nameSpace The Handler's Namespace.
     * @throws runtime_error if this Handler is already attached to a different
     * Namespace.
     */
    void
    setNamespace(Namespace* nameSpace);

    /**
     * Get the Namespace that this Handler is attached to.
     * @return This Handler's Namespace, or null if this Handler is not attached
     * to a Namespace.
     */
    Namespace&
    getNamespace() { return *namespace_; }

    /**
     * An internal method to check if this Handler can deserialize the blob in
     * order to set the object for the objectNamespace. This should only be
     * called by the Namespace class. This base implementation just returns
     * false. The subclass can override.
     * @param objectNamespace The Namespace node which needs its object
     * deserialized.
     * @param blob The serialized bytes to deserialize.
     * @param onDeserialized If the Handler can deserialize, it should return
     * true and eventually call onDeserialized(object) with the deserialized
     * object.
     * @return True if this Handler can deserialize and will call
     * onDeserialized, otherwise false.
     */
    virtual bool
    canDeserialize
      (Namespace& objectNamespace, const ndn::Blob& blob,
       const OnDeserialized& onDeserialized);

  protected:
    /**
     * This protected method is called after this Handler's Namespace field is
     * set by setNamespace(). A subclass can override to perform actions with
     * getNamespace() such as adding callbacks to the Namespace.
     */
    virtual void
    onNamespaceSet();

  private:
    Namespace* namespace_;
  };

  /**
   * Create a Namespace object with the given name, and with no parent. This is
   * the root of the name tree. This object must remain allocated as long as any
   * operations or handlers are using it. To create child nodes, use
   * myNamespace.getChild("foo") or myNamespace["foo"].
   * @param name The name of this root node in the namespace. This makes a copy
   * of the name.
   * @param keyChain (optional) The KeyChain for signing packets, if needed. The
   * KeyChain object must remain valid during the life of this Namespace object.
   * You can also call setKeyChain().
   */
  Namespace(const ndn::Name& name, ndn::KeyChain* keyChain = 0)
  : impl_(ndn::ptr_lib::make_shared<Impl>(*this, name, keyChain))
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
  Namespace*
  getRoot() { return impl_->getRoot(); }

  /**
   * Get the state of this Namespace node. When a Namespace node is first
   * created, its state is NamespaceState_NAME_EXISTS .
   * @return The state of this Namespace node.
   */
  NamespaceState
  getState() { return impl_->getState(); }

  /**
   * Get the NetworkNack for when the state is set to
   * NamespaceState_INTEREST_NETWORK_NACK .
   * @return The NetworkNack, or null if one wasn't received.
   */
  ndn::ptr_lib::shared_ptr<ndn::NetworkNack>
  getNetworkNack() { return impl_->getNetworkNack(); }

  /**
   * Get the validate state of this Namespace node. When a Namespace node is
   * first created, its validate state is
   * NamespaceValidateState_WAITING_FOR_DATA .
   * @return The validate state of this Namespace node.
   */
  NamespaceValidateState
  getValidateState() { return impl_->getValidateState(); }

  /**
   * Get the ValidationError for when the state is set to
   * NamespaceValidateState_VALIDATE_FAILURE .
   * @return The ValidationError, or null if it hasn't been set due to a
   * VALIDATE_FAILURE.
   */
  const ndn::ptr_lib::shared_ptr<ndn::ValidationError>&
  getValidationError() { return impl_->getValidationError(); }

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
   * Check if there is a descendant node with the name (which must have this
   * node's name as a prefix).
   * @param descendantName The name of the descendant child.
   * @return True if this has a child with the name. This also returns True if
   * the given name equals the name of this node.
   */
  bool
  hasChild(const ndn::Name& descendantName) const
  {
    return impl_->hasChild(descendantName);
  }

  /**
   * Get a child, creating it if needed. This is equivalent to
   * namespace[component]. If a child is created, this calls callbacks as
   * described by addOnStateChanged.
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
   * described by addOnStateChanged (but does not call the callbacks when
   * creating intermediate nodes).
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
   * Attach the Data packet to this Namespace. This sets the state to
   * DATA_RECEIVED and calls callbacks as described by addOnStateChanged.
   * However, if a Data packet is already attached, do nothing.
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
   * getObject() may be different than the content in the attached Data packet
   * (for example if the content is decrypted). To get the deserialized content,
   * you should use getObject() instead of getData().getContent(). Also, the
   * Data packet name is the same as the name of this Namespace node, so you can
   * simply use getName() instead of getData().getName(). You should only use
   * getData() to get other information such as the MetaInfo.
   * @return The Data packet object, or null if not set.
   */
  const ndn::ptr_lib::shared_ptr<ndn::Data>&
  getData() { return impl_->getData(); }

  void
  setObject(const ndn::ptr_lib::shared_ptr<Object>& object)
  {
    impl_->setObject(object);
  }

  /**
   * Get the deserialized object attached to this Namespace object. Note that
   * getObject() may be different than the content in the attached Data packet
   * (for example if the content is decrypted). In the default behavior, the
   * object is a BlobObject holding the Blob content of the Data packet, but may
   * be a different type as determined by the attached handler.
   * @return The object which is a BlobObject or other type as determined by
   * the attached handler. You must use ndn::ptr_lib::dynamic_pointer_cast to
   * cast to the  correct type. If the type is BlobObject, you can use the
   * convenience method getBlobObject(). If the object is not set, return a
   * null shared_ptr.
   */
  const ndn::ptr_lib::shared_ptr<Object>&
  getObject() { return impl_->getObject(); }

  /**
   * Cast getObject() to a BlobObject and return a reference to the Blob. This
   * throws an exception if the object is null, or if it is not a BlobObject.
   * @return A reference to the Blob.
   */
  const ndn::Blob&
  getBlobObject()
  {
    return ndn::ptr_lib::dynamic_pointer_cast<BlobObject>
      (impl_->getObject())->getBlob();
  }

  /**
   * Add an onStateChanged callback. When the state changes in this namespace at
   * this node or any children, this calls onStateChanged as described below.
   * @param onStateChanged This calls
   * onStateChanged(namespace, changedNamespace, state, callbackId) where
   * namespace is this Namespace, changedNamespace is the Namespace (possibly a
   * child) whose state has changed, state is the new state, and callbackId is
   * the callback ID returned by this method. If you only care if the state has
   * changed for this Namespace (and not any of its children) then your callback
   * can check "if &changedNamespace == &namespace". (Note that the state given
   * to the callback may be different than changedNamespace.getState() if other
   * processing has changed the state before this callback is called.)
   * NOTE: The library will log any exceptions thrown by this callback, but for
   * better error handling the callback should catch and properly handle any
   * exceptions.
   * @return The callback ID which you can use in removeCallback().
   */
  uint64_t
  addOnStateChanged(const OnStateChanged& onStateChanged)
  {
    return impl_->addOnStateChanged(onStateChanged);
  }

  /**
   * Add an onValidateStateChanged callback. When the validate state changes in
   * this namespace at this node or any children, this calls
   * onValidateStateChanged as described below.
   * @param onValidateStateChanged This calls
   * onValidateStateChanged(namespace, changedNamespace, validateState, callbackId)
   * where namespace is this Namespace, changedNamespace is the Namespace
   * (possibly a child) whose validate state has changed, validateState is the
   * new validate state, and callbackId is the callback ID returned by this
   * method. If you only care if the validate state has changed for this
   * Namespace (and not any of its children) then your callback can check
   * "if &changedNamespace == &namespace". (Note that the validate state given
   * to the callback may be different than changedNamespace.getValidateState() 
   * if other processing has changed the validate state before this callback is
   * called.)
   * NOTE: The library will log any exceptions thrown by this callback, but for
   * better error handling the callback should catch and properly handle any
   * exceptions.
   * @return The callback ID which you can use in removeCallback().
   */
  uint64_t
  addOnValidateStateChanged(const OnValidateStateChanged& onValidateStateChanged)
  {
    return impl_->addOnValidateStateChanged(onValidateStateChanged);
  }

  /**
   * Add an onObjectNeeded callback. The objectNeeded() method calls all the
   * onObjectNeeded callback on that Namespace node and all the parents, as
   * described below.
   * @param onObjectNeeded This calls
   * onObjectNeeded(namespace, neededNamespace, callbackId) where namespace is
   * this Namespace, neededNamespace is the Namespace (possibly a child) whose
   * objectNeeded was called, and callbackId is the callback ID returned by this
   * method. If the owner of the callback (the application or a Handler) can
   * produce the object for the neededNamespace, then the callback should return
   * true and the owner should produce the object (either during the callback or
   * at a later time) and call neededNamespace.setObject(). If the owner cannot
   * produce the object then the callback should return false.
   * NOTE: The library will log any exceptions thrown by this callback, but for
   * better error handling the callback should catch and properly handle any
   * exceptions.
   * @return The callback ID which you can use in removeCallback().
   */
  uint64_t
  addOnObjectNeeded(const OnObjectNeeded& onObjectNeeded)
  {
    return impl_->addOnObjectNeeded(onObjectNeeded);
  }

  /**
   * Set the Face used when expressInterest is called on this or child nodes
   * (unless a child node has a different Face), and optionally register to
   * receive Interest packets under this prefix and answer with Data packets.
   * TODO: Replace this by a mechanism for requesting a Data object which is
   * more general than a Face network operation.
   * @param face The Face object. If this Namespace object already has a Face
   * object, it is replaced.
   * @param onRegisterFailed (optional) Call face.registerPrefix to register to
   * receive Interest packets under this prefix, and if register prefix fails
   * for any reason, this calls onRegisterFailed(prefix). However, if
   * onRegisterFailed is omitted, do not register to receive Interests.
   * NOTE: The library will log any exceptions thrown by this callback, but for
   * better error handling the callback should catch and properly handle any
   * exceptions.
   * param onRegisterSuccess: (optional) This calls
   * onRegisterSuccess(prefix, registeredPrefixId) when this receives a success
   * message from the forwarder. If
   * onRegisterSuccess is an empty OnRegisterSuccess(), this does not use it.
   * (The onRegisterSuccess parameter comes after onRegisterFailed because it
   * can be empty or omitted, unlike onRegisterFailed.)
   * NOTE: The library will log any exceptions thrown by this callback, but for
   * better error handling the callback should catch and properly handle any
   * exceptions.
   */
  void
  setFace
    (ndn::Face* face, 
     const ndn::OnRegisterFailed& onRegisterFailed = ndn::OnRegisterFailed(),
     const ndn::OnRegisterSuccess& onRegisterSuccess = ndn::OnRegisterSuccess())
  {
    impl_->setFace(face, onRegisterFailed, onRegisterSuccess);
  }

  /**
   * Set the KeyChain used to sign packets (if needed) at this or child nodes.
   * If a KeyChain already exists at this node, it is replaced.
   * @param keyChain The KeyChain, which must remain valid during the life of
   * this Namespace object.
   */
  void
  setKeyChain(ndn::KeyChain* keyChain)
  {
    impl_->setKeyChain(keyChain);
  }

  Namespace&
  setHandler(const ndn::ptr_lib::shared_ptr<Handler>& handler)
  {
    return impl_->setHandler(handler);
  }

  void
  objectNeeded() { impl_->objectNeeded(); }

  /**
   * Set the maximum lifetime for re-expressed interests to be used when this or
   * a child node calls expressInterest. You can call this on a child node to
   * set a different maximum lifetime. If you don't set this, the default is
   * 16000 milliseconds.
   * @param maxInterestLifetime The maximum lifetime in milliseconds.
   */
  void
  setMaxInterestLifetime(ndn::Milliseconds maxInterestLifetime)
  {
    impl_->setMaxInterestLifetime(maxInterestLifetime);
  }

  /**
   * Call expressInterest on this (or a parent's) Face where the interest name
   * is the name of this Namespace node. When the Data packet is received this
   * calls setData, so you should use a callback with addOnStateChanged. This
   * uses ExponentialReExpress to re-express a timed-out interest with longer
   * lifetimes, with a maximum determined by setMaxInterestLifetime(). If the
   * Interest times out, this sets the state to NamespaceState_INTEREST_TIMEOUT
   * and calls the OnStateChanged callbacks. If this receives a network Nack,
   * this stores the NetworkNack object which you can access with
   * getNetworkNack(), sets the state to NamespaceState_INTEREST_NETWORK_NACK,
   * and calls the OnStateChanged callbacks.
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
   * addOnStateChanged.
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

  friend class NacConsumerHandler; // Debug: temporary;

  /**
   * Namespace::Impl does the work of Namespace. It is a separate class so that
   * Namespace can create an instance in a shared_ptr to use in callbacks.
   */
  class Impl : public ndn::ptr_lib::enable_shared_from_this<Impl> {
  public:
    /**
     * Create a new Impl, which should belong to a shared_ptr.
     * @param outerNamespace The Namespace which is creating this inner Imp.
     * @param name See the Namespace constructor.
     */
    Impl
      (Namespace& outerNamespace, const ndn::Name& name, ndn::KeyChain* keyChain);

    const ndn::Name&
    getName() const { return name_; }

    Namespace*
    getParent() { return parent_; }

    Namespace*
    getRoot() { return root_; }

    NamespaceState
    getState() { return state_; }

    ndn::ptr_lib::shared_ptr<ndn::NetworkNack>
    getNetworkNack() { return networkNack_; }

    NamespaceValidateState
    getValidateState() { return validateState_; }

    const ndn::ptr_lib::shared_ptr<ndn::ValidationError>&
    getValidationError() { return validationError_; }

    bool
    hasChild(const ndn::Name::Component& component) const
    {
      return children_.find(component) != children_.end();
    }

    bool
    hasChild(const ndn::Name& descendantName);

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

    void
    setObject(const ndn::ptr_lib::shared_ptr<Object>& object);

    const ndn::ptr_lib::shared_ptr<Object>&
    getObject() { return object_; }

    uint64_t
    addOnStateChanged(const OnStateChanged& onStateChanged);

    uint64_t
    addOnValidateStateChanged
      (const OnValidateStateChanged& onValidateStateChanged);

    uint64_t
    addOnObjectNeeded(const OnObjectNeeded& onObjectNeeded);

    void
    setFace
      (ndn::Face* face, const ndn::OnRegisterFailed& onRegisterFailed,
       const ndn::OnRegisterSuccess& onRegisterSuccess);

    void
    setKeyChain(ndn::KeyChain* keyChain) { keyChain_ = keyChain; }

    /**
     * Get the KeyChain set by setKeyChain (or the NameSpace constructor) on
     * this or a parent Namespace node.
     * @return The KeyChain, or null if not set on this or any parent.
     */
    ndn::KeyChain*
    getKeyChain();

    Namespace&
    setHandler(const ndn::ptr_lib::shared_ptr<Handler>& handler);

    void
    objectNeeded();

    void
    setMaxInterestLifetime(ndn::Milliseconds maxInterestLifetime)
    {
      maxInterestLifetime_ = maxInterestLifetime;
    }

    void
    expressInterest(const ndn::Interest *interestTemplate);

    void
    removeCallback(uint64_t callbackId);

  private:
    friend class NacConsumerHandler; // Debug: temporary;

    /**
     * Get the Face set by setFace on this or a parent Namespace node.
     * @return The Face, or null if not set on this or any parent.
     */
    ndn::Face*
    getFace();

    /**
     * Get the maximum Interest lifetime that was set on this or a parent node.
     * @return The  maximum Interest lifetime, or the default if not set on this
     * or any parent.
     */
    ndn::Milliseconds
    getMaxInterestLifetime();

    /**
     * If canDeserialize on the Handler of this or a parent Namespace node
     * returns true, set the state to DESERIALIZING and wait for the Handler to
     * set the object. Otherwise, just all the default onDeserialized
     * immediately, which sets the state to OBJECT_READY.
     * @param blob The Blob to deserialize.
     */
    void
    deserialize(const ndn::Blob& blob);

    /**
     * Create the child with the given name component and add it to this
     * namespace. This private method should only be called if the child does not
     * already exist. The application should use getChild.
     * @param component The name component of the child.
     * @param fireCallbacks If true, call _setState to fire OnStateChanged
     * callbacks for this and all parent nodes (where the initial state is
     * NamespaceState_NAME_EXISTS). If false, don't call callbacks (for example
     * if creating intermediate nodes).
     * @return The child Namespace object.
     */
    Namespace&
    createChild(const ndn::Name::Component& component, bool fireCallbacks);

    /**
     * Set the state of this Namespace object and call the OnStateChanged
     * callbacks for this and all parents. This does not check if this Namespace
     * object already has the given state.
     * @param state The new state.
     */
    void
    setState(NamespaceState state);

    void
    fireOnStateChanged(Namespace& changedNamespace, NamespaceState state);

    /**
     * Set the validate state of this Namespace object and call the
     * OnValidateStateChanged callbacks for this and all parents. This does not
     * check if this Namespace object already has the given validate state.
     * @param validateState The new validate state.
     */
    void
    setValidateState(NamespaceValidateState validateState);

    void
    fireOnValidateStateChanged
      (Namespace& changedNamespace, NamespaceValidateState validateState);

    bool
    fireOnObjectNeeded(Namespace& neededNamespace);

    /**
     * Set object_ to the given value, set the state to OBJECT_READY, and fire
     * the OnStateChanged callbacks. This may be called from canDeserialize in a
     * handler.
     * @param object The deserialized object.
     */
    void
    onDeserialized(const ndn::ptr_lib::shared_ptr<Object>& object);

    /**
     * This is the default OnInterest callback which searches this node and
     * children nodes for a matching Data packet, longest prefix. This calls
     * face.putData(). If an existing Data packet is not found, add the
     * Interest to the PendingIncomingInterestTable so that a later call to
     * setData may satisfy it.
     */
    void
    onInterest
       (const ndn::ptr_lib::shared_ptr<const ndn::Name>& prefix,
        const ndn::ptr_lib::shared_ptr<const ndn::Interest>& interest,
        ndn::Face& face, uint64_t interestFilterId,
        const ndn::ptr_lib::shared_ptr<const ndn::InterestFilter>& filter);

    /**
     * This is a helper for onInterest to find the longest-prefix match under
     * the given Namespace.
     * @param nameSpace This searches this Namespace and its children.
     * @param interest This calls interest.matchesData().
     * @return The Namespace object for the matched name or null if not found.
     */
    static Namespace*
    findBestMatchName(Namespace& nameSpace, const ndn::Interest& interest);

    void
    onData(const ndn::ptr_lib::shared_ptr<const ndn::Interest>& interest,
           const ndn::ptr_lib::shared_ptr<ndn::Data>& data);

    void
    onTimeout(const ndn::ptr_lib::shared_ptr<const ndn::Interest>& interest);

    void
    onNetworkNack
      (const ndn::ptr_lib::shared_ptr<const ndn::Interest>& interest,
       const ndn::ptr_lib::shared_ptr<ndn::NetworkNack>& networkNack);

    Namespace& outerNamespace_;
    ndn::Name name_;
    // parent_ and root_ may be updated by createChild.
    Namespace* parent_;
    Namespace* root_;
    // The key is a Name::Component. The value is the child Namespace.
    std::map<ndn::Name::Component, ndn::ptr_lib::shared_ptr<Namespace>> children_;
    NamespaceState state_;
    ndn::ptr_lib::shared_ptr<ndn::NetworkNack> networkNack_;
    NamespaceValidateState validateState_;
    ndn::ptr_lib::shared_ptr<ndn::ValidationError> validationError_;
    ndn::ptr_lib::shared_ptr<ndn::Data> data_;
    ndn::ptr_lib::shared_ptr<Object> object_;
    ndn::Face* face_;
    ndn::KeyChain* keyChain_;
    ndn::ptr_lib::shared_ptr<Handler> handler_;
    // The key is the callback ID. The value is the OnStateChanged function.
    std::map<uint64_t, OnStateChanged> onStateChangedCallbacks_;
    // The key is the callback ID. The value is the OnValidateStateChanged function.
    std::map<uint64_t, OnValidateStateChanged> onValidateStateChangedCallbacks_;
    // The key is the callback ID. The value is the OnObjectNeeded function.
    std::map<uint64_t, OnObjectNeeded> onObjectNeededCallbacks_;
    // setFace will create this in the root Namespace node.
    ndn::ptr_lib::shared_ptr<PendingIncomingInterestTable>
      pendingIncomingInterestTable_;
    ndn::Interest defaultInterestTemplate_;
    ndn::Milliseconds maxInterestLifetime_; // -1 if not specified.
  };

private:
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
