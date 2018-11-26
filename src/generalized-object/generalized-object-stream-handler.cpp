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

#include <cnl-cpp/generalized-object/generalized-object-stream-handler.hpp>

using namespace std;
using namespace ndn;
using namespace ndntools;
using namespace ndn::func_lib;

namespace cnl_cpp {

GeneralizedObjectStreamHandler::Impl::Impl
  (const OnSequencedGeneralizedObject& onSequencedGeneralizedObject)
: onSequencedGeneralizedObject_(onSequencedGeneralizedObject), namespace_(0)
{
}

void
GeneralizedObjectStreamHandler::Impl::onNamespaceSet(Namespace* nameSpace)
{
  // Store getNamespace() in impl_. We do this instead of keeping a pointer to
  // this outer Handler object since it might be destroyed.
  namespace_ = nameSpace;

  namespace_->addOnObjectNeeded
    (bind(&GeneralizedObjectStreamHandler::Impl::onObjectNeeded,
          shared_from_this(), _1, _2, _3));
}

bool
GeneralizedObjectStreamHandler::Impl::onObjectNeeded
  (Namespace& nameSpace, Namespace& neededNamespace,
   uint64_t callbackId)
{
  if (&neededNamespace != namespace_)
    // Don't respond for child namespaces (including when we call objectNeeded
    // on the _meta child below).
    return false;

  return false; // debug
}

GeneralizedObjectStreamHandler::Values* GeneralizedObjectStreamHandler::values_ = 0;

}
