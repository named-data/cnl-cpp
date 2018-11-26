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
 * Namespace node to fetch the _latest packet and use the name in it to fetch
 * the generalized object using a GeneralizedObjectHandler.
 */
class GeneralizedObjectStreamHandler : public Namespace::Handler {
public:
  typedef ndn::func_lib::function<void
    (int sequenceNumber,
     const ndn::ptr_lib::shared_ptr<ContentMetaInfoObject>& contentMetaInfo,
     const ndn::ptr_lib::shared_ptr<Object>& object)> OnSequencedGeneralizedObject;

  GeneralizedObjectStreamHandler
    (const OnSequencedGeneralizedObject& onSequencedGeneralizedObject =
       OnSequencedGeneralizedObject())
  : impl_(ndn::ptr_lib::make_shared<Impl>(onSequencedGeneralizedObject))
  {
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
    Impl(const OnSequencedGeneralizedObject& onSequencedGeneralizedObject);

    void
    onNamespaceSet(Namespace* nameSpace);

  private:
    bool
    onObjectNeeded
      (Namespace& nameSpace, Namespace& neededNamespace,
       uint64_t callbackId);

    OnSequencedGeneralizedObject onSequencedGeneralizedObject_;
    Namespace* namespace_;
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
