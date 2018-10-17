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

#ifndef CNL_CPP_NAC_CONSUMER_HANDLE_HPP
#define CNL_CPP_NAC_CONSUMER_HANDLE_HPP

#include "namespace.hpp"
#include <ndn-cpp/encrypt/consumer.hpp>

namespace cnl_cpp {

/**
 * A NacConsumerHandler uses a Name-based Access Control Consumer to
 * automatically decrypt Data packets which are attached to a Namespace node.
 */
class NacConsumerHandler : public Namespace::Handler {
public:
  /**
   * Create a NacConsumerHandler object to attach to the given Namespace. This
   * holds an internal NAC Consumer with the given values. This uses the Face
   * which must already be set for the Namespace (or one of its parents).
   * @param nameSpace The Namespace node whose content is transformed by
   * decrypting it.
   * @param keyChain The keyChain used to verify data packets. This is only a
   * pointer to a KeyChain object which must remain valid for the life of this
   * Consumer.
   * @param groupName The reading group name that the consumer belongs to.
   * This makes a copy of the Name.
   * @param consumerName The identity of the consumer. This makes a copy of the
   * Name.
   * @param database The ConsumerDb database for storing decryption keys.
   */
  NacConsumerHandler
    (Namespace& nameSpace, ndn::KeyChain* keyChain, const ndn::Name& groupName,
     const ndn::Name& consumerName,
     const ndn::ptr_lib::shared_ptr<ndn::ConsumerDb>& database)
  : impl_(ndn::ptr_lib::make_shared<Impl>(*this))
  {
    impl_->initialize(nameSpace, keyChain, groupName, consumerName, database);
  }

  /**
   * Add a new decryption key with keyName and keyBlob to the database given
   * to the constructor.
   * @param keyName The key name.
   * @param keyBlob The encoded key.
   * @throws ConsumerDb::Error if a key with the same keyName already exists in
   * the database, or other database error.
   * @throws runtime_error if the consumer name is not a prefix of the key name.
   */
  void
  addDecryptionKey(const ndn::Name& keyName, const ndn::Blob& keyBlob)
  {
    impl_->addDecryptionKey(keyName, keyBlob);
  }

  virtual bool
  canDeserialize
    (Namespace& objectNamespace, const ndn::Blob& blob,
     const Namespace::OnDeserialized& onDeserialized)
  {
    return impl_->canDeserialize(objectNamespace, blob, onDeserialized);
  }

private:
  /**
   * NacConsumerHandler::Impl does the work of NacConsumerHandler. It is a
   * separate class so that NacConsumerHandler can create an instance in a
   * shared_ptr to use in callbacks.
   */
  class Impl : public ndn::ptr_lib::enable_shared_from_this<Impl> {
  public:
    /**
     * Create a new Impl, which should belong to a shared_ptr, then call
     * initialize().
     * @param outerNacConsumerHandler The NacConsumerHandler which is creating
     * this inner Imp.
     * @param nameSpace See the NacConsumerHandler constructor.
     */
    Impl(NacConsumerHandler& outerNacConsumerHandler);

    /**
     * Complete the work of the constructor. This is needed because we can't
     * call shared_from_this() in the constructor.
     */
    void
    initialize
      (Namespace& nameSpace, ndn::KeyChain* keyChain, const ndn::Name& groupName,
       const ndn::Name& consumerName,
       const ndn::ptr_lib::shared_ptr<ndn::ConsumerDb>& database);

    void
    addDecryptionKey(const ndn::Name& keyName, const ndn::Blob& keyBlob)
    {
      consumer_->addDecryptionKey(keyName, keyBlob);
    }

    bool
    canDeserialize
      (Namespace& objectNamespace, const ndn::Blob& blob,
       const Namespace::OnDeserialized& onDeserialized);

  private:
    void
    onStateChanged
      (Namespace& nameSpace, Namespace& changedNamespace, NamespaceState state,
       uint64_t callbackId);

    NacConsumerHandler& outerNacConsumerHandler_;
    ndn::ptr_lib::shared_ptr<ndn::Consumer> consumer_;
  };

  ndn::ptr_lib::shared_ptr<Impl> impl_;
};

}

#endif
