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

#include <memory.h>
#include <stdexcept>
#include <ndn-cpp/util/logging.hpp>
#include <ndn-cpp-tools/usersync/content-meta-info.hpp>
#include <cnl-cpp/generalized-object/generalized-object-handler.hpp>

using namespace std;
using namespace ndn;
using namespace ndntools;
using namespace ndn::func_lib;

INIT_LOGGER("cnl_cpp.GeneralizedObjectHandler");

namespace cnl_cpp {

GeneralizedObjectHandler::Impl::Impl(const OnGeneralizedObject& onGeneralizedObject)
: // Instead of making this inherit from SegmentedObjectHandler, we create one
  // here and pass the method calls through.
  segmentedObjectHandler_(ptr_lib::make_shared<SegmentedObjectHandler>()),
  // We'll call onGeneralizedObject if we don't use the SegmentedObjectHandler.
  onGeneralizedObject_(onGeneralizedObject), namespace_(0)
{
}

void
GeneralizedObjectHandler::Impl::setObject
  (Namespace& nameSpace, const Blob& object, const string& contentType)
{
  bool hasSegments =
    (object.size() > segmentedObjectHandler_->getMaxSegmentPayloadLength());

  // Prepare the _meta packet.
  ContentMetaInfo contentMetaInfo;
  contentMetaInfo.setContentType(contentType);
  contentMetaInfo.setTimestamp(ndn_getNowMilliseconds());
  contentMetaInfo.setHasSegments(hasSegments);

  if (!hasSegments)
    // We don't need to segment. Put the object in the "other" field.
    contentMetaInfo.setOther(object);

  nameSpace[getNAME_COMPONENT_META()].serializeObject
    (ptr_lib::make_shared<BlobObject>(contentMetaInfo.wireEncode()));

  if (hasSegments)
    segmentedObjectHandler_->setObject(nameSpace, object, true);
  else
    // TODO: Do this in a canSerialize callback from Namespace.serializeObject?
    nameSpace.setObject_(ptr_lib::make_shared<BlobObject>(object));
}

void
GeneralizedObjectHandler::Impl::onNamespaceSet(Namespace* nameSpace)
{
  // Store getNamespace() in impl_. We do this instead of keeping a pointer to
  // this outer Handler object since it might be destroyed.
  namespace_ = nameSpace;

  namespace_->addOnObjectNeeded
    (bind(&GeneralizedObjectHandler::Impl::onObjectNeeded, shared_from_this(), _1, _2, _3));
  // We don't attach the SegmentedObjectHandler until we need it.
}

bool
GeneralizedObjectHandler::Impl::onObjectNeeded
  (Namespace& nameSpace, Namespace& neededNamespace, uint64_t callbackId)
{
  if (&neededNamespace != namespace_)
    // Don't respond for child namespaces (including when we call objectNeeded
    // on the _meta child below).
    return false;

  (*namespace_)[getNAME_COMPONENT_META()].objectNeeded();
  return true;
}

bool
GeneralizedObjectHandler::Impl::canDeserialize
  (Namespace& objectNamespace, const Blob& blob,
   const OnDeserialized& onDeserialized)
{
  if (!(objectNamespace.getName().size() == namespace_->getName().size() + 1 &&
        objectNamespace.getName()[-1] == getNAME_COMPONENT_META()))
    // Not the _meta packet. Ignore.
    return false;

  // Decode the ContentMetaInfo.
  ptr_lib::shared_ptr<ContentMetaInfoObject> contentMetaInfo =
    ptr_lib::make_shared<ContentMetaInfoObject>(ContentMetaInfo());
  // TODO: Report a deserializing error.
  contentMetaInfo->wireDecode(blob);

  // This will set the object for the _meta Namespace node.
  onDeserialized(contentMetaInfo);

  if (contentMetaInfo->getHasSegments()) {
    // Initiate fetching segments. This will call onGeneralizedObject.
    segmentedObjectHandler_->addOnSegmentedObject
      (bind(&GeneralizedObjectHandler::Impl::onSegmentedObject,
       shared_from_this(), _1, contentMetaInfo));
    segmentedObjectHandler_->setNamespace(namespace_);
    // Explicitly request segment 0 to avoid fetching _meta, etc.
    (*namespace_)[Name::Component::fromSegment(0)].objectNeeded();

    // Fetch the _manifest packet.
    // getAllData(dataList): Verification should be handled by SegmentedObjectHandler.
    // TODO: How does SegmentedObjectHandler consumer know we're using a _manifest?
    (*namespace_)[SegmentedObjectHandler::getNAME_COMPONENT_MANIFEST()].objectNeeded();
  }
  else
    // No segments, so the object is the ContentMetaInfo "other" Blob.
    // Deserialize and call the same callback as the segmentedObjectHandler.
    namespace_->deserialize_
      (contentMetaInfo->getOther(),
       bind(&GeneralizedObjectHandler::Impl::onSegmentedObject,
       shared_from_this(), _1, contentMetaInfo));

  return true;
}

void
GeneralizedObjectHandler::Impl::onSegmentedObject
  (Namespace& objectNamespace,
   const ptr_lib::shared_ptr<ContentMetaInfoObject>& contentMetaInfo)
{
  if (onGeneralizedObject_) {
    try {
      onGeneralizedObject_(contentMetaInfo, objectNamespace);
    } catch (const std::exception& ex) {
      _LOG_ERROR("Error in onGeneralizedObject: " << ex.what());
    } catch (...) {
      _LOG_ERROR("Error in onGeneralizedObject.");
    }
  }
}

GeneralizedObjectHandler::Values* GeneralizedObjectHandler::values_ = 0;

}
