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

uint64_t
Namespace::getNextCallbackId()
{
  // This is an atomic_uint64_t, so increment is thread-safe.
  return ++lastCallbackId_;
}

Namespace::Impl::Impl(Namespace& outerNamespace, const Name& name)
: outerNamespace_(outerNamespace), name_(name), parent_(0), face_(0),
  transformContent_(TransformContent()), maxInterestLifetime_(-1)
{
  defaultInterestTemplate_.setInterestLifetimeMilliseconds(4000.0);
}

Namespace&
Namespace::Impl::getRoot()
{
  Namespace* result = &outerNamespace_;
  while (result->impl_->parent_)
    result = result->impl_->parent_;
  return *result;
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
  ptr_lib::shared_ptr<vector<Name::Component>> result
    (new vector<Name::Component>);
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

  TransformContent transformContent = getTransformContent();
  // TODO: TransformContent should take an OnError.
  if (transformContent)
    transformContent
      (data, bind(&Namespace::Impl::onContentTransformed, shared_from_this(), _1, _2));
  else
    // Otherwise just invoke directly.
    onContentTransformed
      (data, ptr_lib::make_shared<BlobContent>(data->getContent()));
}

uint64_t
Namespace::Impl::addOnNameAdded(const OnNameAdded& onNameAdded)
{
  uint64_t callbackId = getNextCallbackId();
  onNameAddedCallbacks_[callbackId] = onNameAdded;
  return callbackId;
}

uint64_t
Namespace::Impl::addOnContentSet(const OnContentSet& onContentSet)
{
  uint64_t callbackId = getNextCallbackId();
  onContentSetCallbacks_[callbackId] = onContentSet;
  return callbackId;
}

void
Namespace::Impl::expressInterest(const Interest *interestTemplate)
{
  Face* face = getFace();
  if (!face)
    throw runtime_error("A Face object has not been set for this or a parent");

  if (!interestTemplate)
    interestTemplate = &defaultInterestTemplate_;
  face->expressInterest
    (name_, interestTemplate,
     bind(&Namespace::Impl::onData, shared_from_this(), _1, _2),
     ExponentialReExpress::makeOnTimeout
       (face, bind(&Namespace::Impl::onData, shared_from_this(), _1, _2),
        OnTimeout(), getMaxInterestLifetime()));
}

void
Namespace::Impl::removeCallback(uint64_t callbackId)
{
  onNameAddedCallbacks_.erase(callbackId);
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

ndn::Milliseconds
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
  ptr_lib::shared_ptr<Namespace> child
    (new Namespace(Name(name_).append(component)));
  child->impl_->parent_ = &outerNamespace_;
  children_[component] = child;

  if (fireCallbacks) {
    Namespace* nameSpace = &outerNamespace_;
    while (nameSpace) {
      nameSpace->impl_->fireOnNameAdded(*child);
      nameSpace = nameSpace->impl_->parent_;
    }
  }

  return *child;
}

void
Namespace::Impl::fireOnNameAdded(Namespace& addedNamespace)
{
  // Copy the keys before iterating since callbacks can change the list.
  vector<uint64_t> keys;
  keys.reserve(onNameAddedCallbacks_.size());
  for (map<uint64_t, OnNameAdded>::iterator i = onNameAddedCallbacks_.begin();
       i != onNameAddedCallbacks_.end(); ++i)
    keys.push_back(i->first);

  for (size_t i = 0; i < keys.size(); ++i) {
    // A callback on a previous pass may have removed this callback, so check.
    map<uint64_t, OnNameAdded>::iterator entry =
      onNameAddedCallbacks_.find(keys[i]);
    if (entry != onNameAddedCallbacks_.end()) {
      try {
        entry->second(outerNamespace_, addedNamespace, entry->first);
      } catch (const std::exception& ex) {
        _LOG_ERROR("Namespace::fireOnNameAdded: Error in onNameAdded: " <<
                   ex.what());
      } catch (...) {
        _LOG_ERROR("Namespace::fireOnNameAdded: Error in onNameAdded.");
      }
    }
  }
}

void
Namespace::Impl::onContentTransformed
  (const ptr_lib::shared_ptr<Data>& data, 
   const ndn::ptr_lib::shared_ptr<Content>& content)
{
  data_ = data;
  content_ = content;

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
  getChild(data->getName()).setData(data);
}

#ifdef NDN_CPP_HAVE_BOOST_ASIO
boost::atomic_uint64_t Namespace::lastCallbackId_;
#else
uint64_t Namespace::lastCallbackId_;
#endif

}
