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

#include <sstream>
#include <ndn-cpp/util/exponential-re-express.hpp>
#include <ndn-cpp/util/logging.hpp>
#include "impl/pending-incoming-interest-table.hpp"
#include <cnl-cpp/namespace.hpp>

using namespace std;
using namespace ndn;
using namespace ndn::func_lib;

INIT_LOGGER("cnl_cpp.Namespace");

namespace cnl_cpp {

Namespace::Handler&
Namespace::Handler::setNamespace(Namespace* nameSpace)
{
  if (namespace_ && namespace_ != nameSpace)
    throw runtime_error("This Handler is already attached to a different Namespace object");

  namespace_ = nameSpace;
  onNamespaceSet();

  return *this;
}

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

Namespace::Impl::Impl
  (Namespace& outerNamespace, const Name& name, KeyChain* keyChain)
: outerNamespace_(outerNamespace), name_(name), keyChain_(keyChain), parent_(0),
  root_(&outerNamespace), state_(NamespaceState_NAME_EXISTS),
  validateState_(NamespaceValidateState_WAITING_FOR_DATA), 
  freshnessExpiryTimeMilliseconds_(-1.0), face_(0), decryptor_(0),
  maxInterestLifetime_(-1)
{
}

bool
Namespace::Impl::hasChild(const Name& descendantName)
{
  if (!name_.isPrefixOf(descendantName))
    throw runtime_error
      ("The name of this node is not a prefix of the descendant name");

  if (descendantName.size() == name_.size())
    // A trivial case where it is already the name of this node.
    return true;

  // Find the child node whose name equals the descendantName. We know
  // descendantNamespace is a prefix, so we can just go by component count
  // instead of a full compare.
  Namespace* descendantNamespace = &outerNamespace_;
  while (true) {
    const Name::Component& nextComponent =
      descendantName[descendantNamespace->getName().size()];
    if (!descendantNamespace->hasChild(nextComponent))
      return false;

    if (descendantNamespace->getName().size() + 1 == descendantName.size())
      // nextComponent is the final component.
      return true;
    descendantNamespace = descendantNamespace->impl_->children_[nextComponent].get();
  }
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
Namespace::Impl::serializeObject(const ptr_lib::shared_ptr<Object>& object)
{
  // TODO: What if this node already has a _data and/or _object?

  // TODO: Call handler canSerialize and set state SERIALIZING.
  // (Does this happen in a different place from onObjectNeeded?)
  // (If the handler can't serialize and this node has children, should abort?)

  BlobObject* blobObject = dynamic_cast<BlobObject*>(object.get());
  if (!blobObject)
      throw runtime_error
        ("serializeObject: For the default serialize, the object must be a Blob");

  KeyChain* keyChain = getKeyChain();
  if (!keyChain)
    throw runtime_error
      ("serializeObject: There is no KeyChain, so can't serialize " +
       name_.toUri());

  // TODO: Encrypt and set state ENCRYPTING.

  // Prepare the Data packet.
  ptr_lib::shared_ptr<Data> data = ptr_lib::make_shared<Data>(name_);
  data->setContent(blobObject->getBlob());
  const MetaInfo* metaInfo = getNewDataMetaInfo();
  if (metaInfo)
    data->setMetaInfo(*metaInfo);

  setState(NamespaceState_SIGNING);
  try {
    keyChain->sign(*data);
  } catch (const std::exception& ex) {
    signingError_ = string("Error signing the serialized Data: ") + ex.what();
    setState(NamespaceState_SIGNING_ERROR);
    return;
  }

  // This calls satisfyInterests.
  setData(data);

  object_ = object;
  setState(NamespaceState_OBJECT_READY);
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

  if (root_->impl_->pendingIncomingInterestTable_)
    // Quickly send the Data packet to satisfy interest, before calling callbacks.
    root_->impl_->pendingIncomingInterestTable_->satisfyInterests(*data);

  if (data->getMetaInfo().getFreshnessPeriod() >= 0.0)
    freshnessExpiryTimeMilliseconds_ =
      ndn_getNowMilliseconds() + data->getMetaInfo().getFreshnessPeriod();
  else
    // Does not expire.
    freshnessExpiryTimeMilliseconds_ = -1.0;
  data_ = data;

  // TODO: This is presumably called by the application in the producer pipeline
  // (who may have already serialized and encrypted), but should we decrypt and
  // deserialize?
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

void
Namespace::Impl::setFace
  (Face* face, const OnRegisterFailed& onRegisterFailed,
   const OnRegisterSuccess& onRegisterSuccess)
{
  face_ = face;

  if (onRegisterFailed) {
    if (!root_->impl_->pendingIncomingInterestTable_)
      // All onInterest callbacks share this in the root node. When we add a new
      // Data packet to a Namespace node, we will also check if it satisfies a
      // pending Interest.
      root_->impl_->pendingIncomingInterestTable_ =
        ptr_lib::make_shared<PendingIncomingInterestTable>();

    face->registerPrefix
      (name_,
       bind(&Namespace::Impl::onInterest, shared_from_this(), _1, _2, _3, _4, _5),
       onRegisterFailed, onRegisterSuccess);
  }
}

Namespace&
Namespace::Impl::setHandler(const ptr_lib::shared_ptr<Handler>& handler)
{
  if (handler_)
    // TODO: Should we try to chain handlers?
    throw runtime_error("This Namespace node already has a handler");

  handler->setNamespace(&outerNamespace_);
  handler_ = handler;
  return outerNamespace_;
}

void
Namespace::Impl::objectNeeded()
{
  // Debug: Allow mustBeFresh == false?
  // Debug: Check if already have a child object?
  if (object_)
    // We already have the object.
    // Debug: (But maybe we don't have the _data and we need it?)
    return;

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
  Face* face = getFace_();
  if (!face)
    throw runtime_error("A Face object has not been set for this or a parent");

  // TODO: What if the state is already INTEREST_EXPRESSED?
  setState(NamespaceState_INTEREST_EXPRESSED);

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
}

Face*
Namespace::Impl::getFace_()
{
  Namespace* nameSpace = &outerNamespace_;
  while (nameSpace) {
    if (nameSpace->impl_->face_)
      return nameSpace->impl_->face_;
    nameSpace = nameSpace->impl_->parent_;
  }

  return 0;
}

KeyChain*
Namespace::Impl::getKeyChain()
{
  Namespace* nameSpace = &outerNamespace_;
  while (nameSpace) {
    if (nameSpace->impl_->keyChain_)
      return nameSpace->impl_->keyChain_;
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

const MetaInfo*
Namespace::Impl::getNewDataMetaInfo()
{
  Namespace* nameSpace = &outerNamespace_;
  while (nameSpace) {
    if (nameSpace->impl_->newDataMetaInfo_)
      return nameSpace->impl_->newDataMetaInfo_.get();
    nameSpace = nameSpace->impl_->parent_;
  }

  return 0;
}

DecryptorV2*
Namespace::Impl::getDecryptor()
{
  Namespace* nameSpace = &outerNamespace_;
  while (nameSpace) {
    if (nameSpace->impl_->decryptor_)
      return nameSpace->impl_->decryptor_;
    nameSpace = nameSpace->impl_->parent_;
  }

  return 0;
}

void
Namespace::Impl::deserialize_
  (const Blob& blob, const Handler::OnDeserialized& onObjectSet)
{
  Namespace* nameSpace = &outerNamespace_;
  while (nameSpace) {
    if (nameSpace->impl_->handler_) {
      if (nameSpace->impl_->handler_->canDeserialize
          (outerNamespace_, blob,
           bind(&Namespace::Impl::defaultOnDeserialized, shared_from_this(), 
                _1, onObjectSet))) {
        // Wait for the Handler to set the object.
        setState(NamespaceState_DESERIALIZING);
        return;
      }
    }

    nameSpace = nameSpace->impl_->parent_;
  }

  // Debug: Check if the object has been set (even if canDeserialize returned false.)

  // Just call defaultOnDeserialized immediately.
  defaultOnDeserialized(ptr_lib::make_shared<BlobObject>(blob), onObjectSet);
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
Namespace::Impl::defaultOnDeserialized
  (const ptr_lib::shared_ptr<Object>& object,
   const Handler::OnDeserialized& onObjectSet)
{
  object_ = object;
  setState(NamespaceState_OBJECT_READY);

  if (onObjectSet)
    onObjectSet(object);
}

void
Namespace::Impl::onInterest
   (const ptr_lib::shared_ptr<const Name>& prefix,
    const ptr_lib::shared_ptr<const Interest>& interest, Face& face,
    uint64_t interestFilterId,
    const ptr_lib::shared_ptr<const InterestFilter>& filter)
{
  Name interestName = interest->getName();
  if (interestName.size() >= 1 && interestName[-1].isImplicitSha256Digest())
    // Strip the implicit digest.
    interestName = interestName.getPrefix(-1);

  if (!name_.isPrefixOf(interestName))
    // No match.
    return;

  // Check if the Namespace node exists and has a matching Data packet.
  if (hasChild(interestName)) {
    Namespace* bestMatch = findBestMatchName
      (getChild(interestName), *interest, ndn_getNowMilliseconds());
    if (bestMatch) {
      // findBestMatchName makes sure there is a data_ packet.
      face.putData(*bestMatch->impl_->data_);
      return;
    }
  }

  // No Data packet found, so save the pending Interest.
  root_->impl_->pendingIncomingInterestTable_->add(interest, face);
  // Signal that a Data packet is needed.
  getChild(interestName).objectNeeded();
}

Namespace*
Namespace::Impl::findBestMatchName
  (Namespace& nameSpace, const Interest& interest,
   MillisecondsSince1970 nowMilliseconds)
{
  Namespace *bestMatch = 0;

  // Search the children backwards which will result in a "less than" name
  // among names of the same length.
  for (map<Name::Component, ptr_lib::shared_ptr<Namespace>>::reverse_iterator
        i = nameSpace.impl_->children_.rbegin();
       i != nameSpace.impl_->children_.rend(); ++i) {
    Namespace& child = *i->second;
    Namespace* childBestMatch = findBestMatchName
      (child, interest, nowMilliseconds);

    if (childBestMatch &&
        (!bestMatch ||
         childBestMatch->getName().size() >= bestMatch->getName().size()))
      bestMatch = childBestMatch;
  }

  if (bestMatch)
    // We have a child match, and it is longer than this name, so return it.
    return bestMatch;

  if (interest.getMustBeFresh() &&
      nameSpace.impl_->freshnessExpiryTimeMilliseconds_ >= 0 &&
      nowMilliseconds >= nameSpace.impl_->freshnessExpiryTimeMilliseconds_)
    // The Data packet is no longer fresh.
    // Debug: When to set the state to OBJECT_READY_BUT_STALE?
    return 0;

  if (nameSpace.impl_->data_ && interest.matchesData(*nameSpace.impl_->data_))
    return &nameSpace;

  return 0;
}

void
Namespace::Impl::onData
  (const ptr_lib::shared_ptr<const Interest>& interest,
   const ptr_lib::shared_ptr<Data>& data)
{
  Namespace& dataNamespace = getChild(data->getName());
  // setData will set the state to DATA_RECEIVED.
  dataNamespace.setData(data);
  setState(NamespaceState_DATA_RECEIVED);

  // TODO: Start the validator.
  dataNamespace.impl_->setValidateState(NamespaceValidateState_VALIDATING);

  DecryptorV2* decryptor = dataNamespace.impl_->getDecryptor();
  if (!decryptor) {
    dataNamespace.impl_->deserialize_(data->getContent());
    return;
  }

  // Decrypt, then deserialize.
  dataNamespace.impl_->setState(NamespaceState_DECRYPTING);
  ptr_lib::shared_ptr<EncryptedContent> encryptedContent =
   ptr_lib::make_shared<EncryptedContent>();
  try {
    encryptedContent->wireDecodeV2(data->getContent());
  } catch (const std::exception& ex) {
    dataNamespace.impl_->decryptionError_ =
      string("Error decoding the EncryptedContent: ") + ex.what();
    dataNamespace.impl_->setState(NamespaceState_DECRYPTION_ERROR);
    return;
  }

  decryptor->decrypt
    (encryptedContent,
     bind(&Namespace::Impl::deserialize_, shared_from_this(), _1,
          Handler::OnDeserialized()),
     bind(&Namespace::Impl::onDecryptionError, shared_from_this(), _1, _2));
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

void
Namespace::Impl::onDecryptionError
  (EncryptError::ErrorCode errorCode, const string& message)
{
  ostringstream decryptionErrorText;
  decryptionErrorText << "Decryptor error " << errorCode << ": " + message;
  decryptionError_ = decryptionErrorText.str();
  setState(NamespaceState_DECRYPTION_ERROR);
}

#ifdef NDN_CPP_HAVE_BOOST_ASIO
boost::atomic_uint64_t Namespace::lastCallbackId_;
#else
uint64_t Namespace::lastCallbackId_;
#endif

}
