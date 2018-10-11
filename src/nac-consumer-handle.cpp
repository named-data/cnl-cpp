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

#include <cnl-cpp/nac-consumer-handle.hpp>
#include <ndn-cpp/util/logging.hpp>

using namespace std;
using namespace ndn;
using namespace ndn::func_lib;

INIT_LOGGER("cnl_cpp.NacConsumerHandler");

namespace cnl_cpp {

NacConsumerHandler::Impl::Impl
  (NacConsumerHandler& outerNacConsumerHandler)
  : outerNacConsumerHandler_(outerNacConsumerHandler)
{
}

void
NacConsumerHandler::Impl::initialize
  (Namespace& nameSpace, KeyChain* keyChain, const Name& groupName,
   const Name& consumerName, const ptr_lib::shared_ptr<ConsumerDb>& database)
{
  // TODO: What is the right way to get access to the Face?
  Face* face = nameSpace.impl_->getFace();
  consumer_.reset(new Consumer(face, keyChain, groupName, consumerName, database));

  // TODO: Use a way to set the callback which is better than setting the member.
  nameSpace.impl_->transformContent_ = bind
    (&NacConsumerHandler::Impl::transformContent, shared_from_this(), _1, _2);
}

void
NacConsumerHandler::Impl::transformContent
  (const ptr_lib::shared_ptr<Data>& data,
   const Namespace::OnContentTransformed& onContentTransformed)
{
  // TODO: Use Namespace mechanisms to verify the Data packet.

  // Prepare the callbacks. We make a shared_ptr object since it needs to
  // exist after we call expressInterest and return.
  class Callbacks : public ptr_lib::enable_shared_from_this<Callbacks> {
  public:
    Callbacks
      (NacConsumerHandler::Impl* parent,
       const ptr_lib::shared_ptr<Data>& data,
       const Namespace::OnContentTransformed& onContentTransformed)
    : parent_(parent), data_(data), onContentTransformed_(onContentTransformed)
    {}

    void
    onPlainText(const Blob& plainText)
    {
      try {
        onContentTransformed_(data_, ptr_lib::make_shared<BlobObject>(plainText));
      } catch (const std::exception& ex) {
        _LOG_ERROR("Error in onContentTransformed: " << ex.what());
      } catch (...) {
        _LOG_ERROR("Error in onContentTransformed.");
      }
    }

    void
    onError(EncryptError::ErrorCode errorCode, const string& message)
    {
      _LOG_ERROR("consume error " << errorCode << " " << message);
    }

    NacConsumerHandler::Impl* parent_;
    ptr_lib::shared_ptr<Data> data_;
    Namespace::OnContentTransformed onContentTransformed_;
  };

  ptr_lib::shared_ptr<Callbacks> callbacks(new Callbacks
    (this, data, onContentTransformed));
  consumer_->decryptContent
    (*data, bind(&Callbacks::onPlainText, callbacks, _1),
     bind(&Callbacks::onError, callbacks, _1, _2));
}

}
