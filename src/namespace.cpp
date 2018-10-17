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

#include <ndn-cpp/util/exponential-re-express.hpp>
#include <ndn-cpp/util/logging.hpp>
#include <cnl-cpp/namespace.hpp>

using namespace std;
using namespace ndn;
using namespace ndn::func_lib;

INIT_LOGGER("cnl_cpp.Namespace");

namespace cnl_cpp {

bool
Namespace::Handler::canDeserialize
  (Namespace& objectNamespace, const Blob& blob,
   const OnDeserialized& onDeserialized)
{
  return false;
}

void
Namespace::Handler::onNamespaceSet()
{
}

uint64_t
Namespace::getNextCallbackId()
{
  // This is an atomic_uint64_t, so increment is thread-safe.
  return ++lastCallbackId_;
}

Namespace::Impl::Impl(Namespace& outerNamespace, const Name& name)
: outerNamespace_(outerNamespace), name_(name), parent_(0), 
  root_(&outerNamespace), state_(NamespaceState_NAME_EXISTS),
  validateState_(NamespaceValidateState_WAITING_FOR_DATA), face_(0),
  transformContent_(TransformContent()), maxInterestLifetime_(-1)
{
  defaultInterestTemplate_.setInterestLifetimeMilliseconds(4000.0);
}

Namespace&
Namespace::Impl::getChild(const Name& descendantName)
{
  if (!name_.isPrefixOf(descendantName))
    throw runtime_error
      ("The name of this node is not a prefix of the descendant name");

  // Find or create the child node whose name equals the descendantName.
  // We know descendantNamespace is a prefix, so we can just go by
  // component count instead of a full compare.
  Namespace* descendantNamespace = &outerNamespace_;
  while (descendantNamespace->impl_->name_.size() < descendantName.size()) {
    const Name::Component& nextComponent =
      descendantName[descendantNamespace->impl_->name_.size()];

    map<Name::Component, ptr_lib::shared_ptr<Namespace>>::iterator
      child = descendantNamespace->impl_->children_.find(nextComponent);
    if (child !=  descendantNamespace->impl_->children_.end())
      descendantNamespace = child->second.get();
    else {
      // Only fire the callbacks for the leaf node.
      bool isLeaf =
        (descendantNamespace->impl_->name_.size() == descendantName.size() - 1);
      descendantNamespace = &descendantNamespace->impl_->createChild
        (nextComponent, isLeaf);
    }
  }

  return *descendantNamespace;
}

ptr_lib::shared_ptr<vector<Name::Component>>
Namespace::Impl::getChildComponents()
{
  ptr_lib::shared_ptr<vector<Name::Component>> result =
    ptr_lib::make_shared<vector<Name::Component>>();
  for (map<Name::Component, ptr_lib::shared_ptr<Namespace>>::iterator i = children_.begin();
       i != children_.end(); ++i)
    result->push_back(i->first);

  return result;
}

void
Namespace::Impl::setData(const ptr_lib::shared_ptr<Data>& data)
{
  if (data_)
    // We already have an attached object.
    return;
  if (!data->getName().equals(name_))
    throw runtime_error
      ("The Data packet name does not equal the name of this Namespace node");

  data_ = data;
  setState(NamespaceState_DATA_RECEIVED);

  // TODO: This is presumably called by the application in the producer pipeline
  // (who may have already serialized and encrypted), but should we decrypt and
  // deserialize?
}

void
Namespace::Impl::setObject(const ndn::ptr_lib::shared_ptr<Object>& object)
{
  // Debug: How do we know if we need to serialize/encrypt/sign?
  object_ = object;
  setState(NamespaceState_OBJECT_READY);
}

uint64_t
Namespace::Impl::addOnStateChanged(const OnStateChanged& onStateChanged)
{
  uint64_t callbackId = getNextCallbackId();
  onStateChangedCallbacks_[callbackId] = onStateChanged;
  return callbackId;
}

uint64_t
Namespace::Impl::addOnValidateStateChanged
  (const OnValidateStateChanged& onValidateStateChanged)
{
  uint64_t callbackId = getNextCallbackId();
  onValidateStateChangedCallbacks_[callbackId] = onValidateStateChanged;
  return callbackId;
}

uint64_t
Namespace::Impl::addOnObjectNeeded(const OnObjectNeeded& onObjectNeeded)
{
  uint64_t callbackId = getNextCallbackId();
  onObjectNeededCallbacks_[callbackId] = onObjectNeeded;
  return callbackId;
}

uint64_t
Namespace::Impl::addOnContentSet(const OnContentSet& onContentSet)
{
  uint64_t callbackId = getNextCallbackId();
  onContentSetCallbacks_[callbackId] = onContentSet;
  return callbackId;
}

Namespace&
Namespace::Impl::setHandler(const ptr_lib::shared_ptr<Handler>& handler)
{
  if (handler_)
    // TODO: Should we try to chain handlers?
    throw runtime_error("This Namespace node already has a handler");

  if (handler->namespace_)
    // TODO: Should we allow attaching to multiple Namespace nodes?
    throw runtime_error("The handler is already attached to a Namespace node");

  handler->namespace_ = &outerNamespace_;
  handler->onNamespaceSet();
  handler_ = handler;
  return outerNamespace_;
}

void
Namespace::Impl::objectNeeded()
{
  // Debug: Check if we already have the object. (But maybe not the data_?)

  // Ask all OnObjectNeeded callbacks if they can produce.
  bool canProduce = false;
  Namespace* nameSpace = &outerNamespace_;
  while (nameSpace) {
    if (nameSpace->impl_->fireOnObjectNeeded(outerNamespace_))
      canProduce = true;
    nameSpace = nameSpace->impl_->parent_;
  }

  // Debug: Check if the object has been set (even if onObjectNeeded returned false.)

  if (canProduce) {
    // Assume that the application will produce the object.
    setState(NamespaceState_PRODUCING_OBJECT);
    return;
  }

  // Debug: Need an Interest template?
  expressInterest(0);
}

void
Namespace::Impl::expressInterest(const Interest *interestTemplate)
{
  Face* face = getFace();
  if (!face)
    throw runtime_error("A Face object has not been set for this or a parent");

  // TODO: What if the state is already INTEREST_EXPRESSED?
  setState(NamespaceState_INTEREST_EXPRESSED);

  if (!interestTemplate)
    interestTemplate = &defaultInterestTemplate_;
  face->expressInterest
    (name_, interestTemplate,
     bind(&Namespace::Impl::onData, shared_from_this(), _1, _2),
     ExponentialReExpress::makeOnTimeout
       (face, bind(&Namespace::Impl::onData, shared_from_this(), _1, _2),
        bind(&Namespace::Impl::onTimeout, shared_from_this(), _1),
        getMaxInterestLifetime()),
     bind(&Namespace::Impl::onNetworkNack, shared_from_this(), _1, _2));
}

void
Namespace::Impl::removeCallback(uint64_t callbackId)
{
  onStateChangedCallbacks_.erase(callbackId);
  onValidateStateChangedCallbacks_.erase(callbackId);
  onContentSetCallbacks_.erase(callbackId);
}

Face*
Namespace::Impl::getFace()
{
  Namespace* nameSpace = &outerNamespace_;
  while (nameSpace) {
    if (nameSpace->impl_->face_)
      return nameSpace->impl_->face_;
    nameSpace = nameSpace->impl_->parent_;
  }

  return 0;
}

Milliseconds
Namespace::Impl::getMaxInterestLifetime()
{
  Namespace* nameSpace = &outerNamespace_;
  while (nameSpace) {
    if (nameSpace->impl_->maxInterestLifetime_ >= 0)
      return nameSpace->impl_->maxInterestLifetime_;
    nameSpace = nameSpace->impl_->parent_;
  }

  // Return the default.
  return 16000.0;
}

void
Namespace::Impl::deserialize(const Blob& blob)
{
  Namespace* nameSpace = &outerNamespace_;
  while (nameSpace) {
    if (nameSpace->impl_->handler_) {
      if (nameSpace->impl_->handler_->canDeserialize
          (outerNamespace_, blob,
           bind(&Namespace::Impl::onDeserialized, shared_from_this(), _1))) {
        // Wait for the Handler to set the object.
        setState(NamespaceState_DESERIALIZING);
        return;
      }
    }

    nameSpace = nameSpace->impl_->parent_;
  }

  // Debug: Check if the object has been set (even if canDeserialize returned false.)

  // Call the default onDeserialized immediately.
  onDeserialized(ptr_lib::make_shared<BlobObject>(blob));
}

Namespace::TransformContent
Namespace::Impl::getTransformContent()
{
  Namespace* nameSpace = &outerNamespace_;
  while (nameSpace) {
    if (nameSpace->impl_->transformContent_)
      return nameSpace->impl_->transformContent_;
    nameSpace = nameSpace->impl_->parent_;
  }

  return TransformContent();
}

Namespace&
Namespace::Impl::createChild(const Name::Component& component, bool fireCallbacks)
{
  ptr_lib::shared_ptr<Namespace> child =
    ptr_lib::make_shared<Namespace>(Name(name_).append(component));
  child->impl_->parent_ = &outerNamespace_;
  child->impl_->root_ = root_;
  children_[component] = child;

  if (fireCallbacks)
    child->impl_->setState(NamespaceState_NAME_EXISTS);

  return *child;
}

void
Namespace::Impl::setState(NamespaceState state)
{
  state_ = state;

  // Fire callbacks.
  Namespace* nameSpace = &outerNamespace_;
  while (nameSpace) {
    nameSpace->impl_->fireOnStateChanged(outerNamespace_, state);
    nameSpace = nameSpace->impl_->parent_;
  }
}

void
Namespace::Impl::fireOnStateChanged
  (Namespace& changedNamespace, NamespaceState state)
{
  // Copy the keys before iterating since callbacks can change the list.
  vector<uint64_t> keys;
  keys.reserve(onStateChangedCallbacks_.size());
  for (map<uint64_t, OnStateChanged>::iterator i = onStateChangedCallbacks_.begin();
       i != onStateChangedCallbacks_.end(); ++i)
    keys.push_back(i->first);

  for (size_t i = 0; i < keys.size(); ++i) {
    // A callback on a previous pass may have removed this callback, so check.
    map<uint64_t, OnStateChanged>::iterator entry =
      onStateChangedCallbacks_.find(keys[i]);
    if (entry != onStateChangedCallbacks_.end()) {
      try {
        entry->second(outerNamespace_, changedNamespace, state, entry->first);
      } catch (const std::exception& ex) {
        _LOG_ERROR("Namespace::fireOnStateChanged: Error in onStateChanged: " <<
                   ex.what());
      } catch (...) {
        _LOG_ERROR("Namespace::fireOnStateChanged: Error in onStateChanged.");
      }
    }
  }
}

void
Namespace::Impl::setValidateState(NamespaceValidateState validateState)
{
  validateState_ = validateState;

  // Fire callbacks.
  Namespace* nameSpace = &outerNamespace_;
  while (nameSpace) {
    nameSpace->impl_->fireOnValidateStateChanged(outerNamespace_, validateState);
    nameSpace = nameSpace->impl_->parent_;
  }
}

void
Namespace::Impl::fireOnValidateStateChanged
  (Namespace& changedNamespace, NamespaceValidateState validateState)
{
  // Copy the keys before iterating since callbacks can change the list.
  vector<uint64_t> keys;
  keys.reserve(onValidateStateChangedCallbacks_.size());
  for (map<uint64_t, OnValidateStateChanged>::iterator i =
         onValidateStateChangedCallbacks_.begin();
       i != onValidateStateChangedCallbacks_.end(); ++i)
    keys.push_back(i->first);

  for (size_t i = 0; i < keys.size(); ++i) {
    // A callback on a previous pass may have removed this callback, so check.
    map<uint64_t, OnValidateStateChanged>::iterator entry =
      onValidateStateChangedCallbacks_.find(keys[i]);
    if (entry != onValidateStateChangedCallbacks_.end()) {
      try {
        entry->second
          (outerNamespace_, changedNamespace, validateState, entry->first);
      } catch (const std::exception& ex) {
        _LOG_ERROR
          ("Namespace::fireOnValidateStateChanged: Error in onValidateStateChanged: " <<
           ex.what());
      } catch (...) {
        _LOG_ERROR
          ("Namespace::fireOnValidateStateChanged: Error in onValidateStateChanged.");
      }
    }
  }
}

bool
Namespace::Impl::fireOnObjectNeeded(Namespace& neededNamespace)
{
  // Copy the keys before iterating since callbacks can change the list.
  vector<uint64_t> keys;
  keys.reserve(onObjectNeededCallbacks_.size());
  for (map<uint64_t, OnObjectNeeded>::iterator i = onObjectNeededCallbacks_.begin();
       i != onObjectNeededCallbacks_.end(); ++i)
    keys.push_back(i->first);

  bool canProduce = false;
  for (size_t i = 0; i < keys.size(); ++i) {
    // A callback on a previous pass may have removed this callback, so check.
    map<uint64_t, OnObjectNeeded>::iterator entry =
      onObjectNeededCallbacks_.find(keys[i]);
    if (entry != onObjectNeededCallbacks_.end()) {
      try {
        if (entry->second(outerNamespace_, neededNamespace, entry->first))
          canProduce = true;
      } catch (const std::exception& ex) {
        _LOG_ERROR("Namespace::fireOnObjectNeeded: Error in onObjectNeeded: " <<
                   ex.what());
      } catch (...) {
        _LOG_ERROR("Namespace::fireOnObjectNeeded: Error in onObjectNeeded.");
      }
    }
  }

  return canProduce;
}

void
Namespace::Impl::onDeserialized(const ptr_lib::shared_ptr<Object>& object)
{
  object_ = object;
  setState(NamespaceState_OBJECT_READY);
}

void
Namespace::Impl::onContentTransformed
  (const ptr_lib::shared_ptr<Data>& data,
   const ptr_lib::shared_ptr<Object>& object)
{
  data_ = data;
  object_ = object;

  setState(NamespaceState_OBJECT_READY);

  // Fire callbacks.
  Namespace* nameSpace = &outerNamespace_;
  while (nameSpace) {
    nameSpace->impl_->fireOnContentSet(outerNamespace_);
    nameSpace = nameSpace->impl_->parent_;
  }
}

void
Namespace::Impl::fireOnContentSet(Namespace& contentNamespace)
{
  // Copy the keys before iterating since callbacks can change the list.
  vector<uint64_t> keys;
  keys.reserve(onContentSetCallbacks_.size());
  for (map<uint64_t, OnContentSet>::iterator i = onContentSetCallbacks_.begin();
       i != onContentSetCallbacks_.end(); ++i)
    keys.push_back(i->first);

  for (size_t i = 0; i < keys.size(); ++i) {
    // A callback on a previous pass may have removed this callback, so check.
    map<uint64_t, OnContentSet>::iterator entry =
      onContentSetCallbacks_.find(keys[i]);
    if (entry != onContentSetCallbacks_.end()) {
      try {
        entry->second(outerNamespace_, contentNamespace, entry->first);
      } catch (const std::exception& ex) {
        _LOG_ERROR("Namespace::fireOnContentSet: Error in onContentSet: " <<
                   ex.what());
      } catch (...) {
        _LOG_ERROR("Namespace::fireOnContentSet: Error in onContentSet.");
      }
    }
  }
}

void
Namespace::Impl::onData
  (const ptr_lib::shared_ptr<const Interest>& interest,
   const ptr_lib::shared_ptr<Data>& data)
{
  Namespace& dataNamespace = getChild(data->getName());
  // setData will set the state to DATA_RECEIVED.
  dataNamespace.setData(data);

  // TODO: Start the validator.
  dataNamespace.impl_->setValidateState(NamespaceValidateState_VALIDATING);

  // TODO: Decrypt.

  dataNamespace.impl_->deserialize(data->getContent());
}

void
Namespace::Impl::onTimeout(const ptr_lib::shared_ptr<const Interest>& interest)
{
  // TODO: Need to detect a timeout on a child node.
  setState(NamespaceState_INTEREST_TIMEOUT);
}

void
Namespace::Impl::onNetworkNack
  (const ptr_lib::shared_ptr<const Interest>& interest,
   const ptr_lib::shared_ptr<NetworkNack>& networkNack)
{
  // TODO: Need to detect a network nack on a child node.
  networkNack_ = networkNack;
  setState(NamespaceState_INTEREST_NETWORK_NACK);
}

#ifdef NDN_CPP_HAVE_BOOST_ASIO
boost::atomic_uint64_t Namespace::lastCallbackId_;
#else
uint64_t Namespace::lastCallbackId_;
#endif

}
