/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil -*- */
/**
 * Copyright (C) 2018-2019 Regents of the University of California.
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

#ifndef NDN_CONTENT_META_INFO_OBJECT_HPP
#define NDN_CONTENT_META_INFO_OBJECT_HPP

#include <ndn-cpp-tools/usersync/content-meta-info.hpp>
#include "../object.hpp"

namespace cnl_cpp {

/**
 * A ContentMetaInfoObject extends Object to hold an ndntools::ContentMetaInfo
 * and provides convenience wrapper access methods the same as ContentMetaInfo.
 */
class ContentMetaInfoObject : public Object {
public:
  /**
   * Create a new ContentMetaInfoObject to hold the given contentMetaInfo. 
   * Objects of this type are created internally by the library, so your
   * application normally does not call this constructor.
   * @param contentMetaInfo The ContentMetaInfo object, which is copied.
   */
  ContentMetaInfoObject(const ndntools::ContentMetaInfo& contentMetaInfo)
  : contentMetaInfo_(contentMetaInfo) {}

  /**
   * Get the ContentMetaInfo given to the constructor.
   * @return The ContentMetaInfo.
   */
  const ndntools::ContentMetaInfo&
  getContentMetaInfo() const { return contentMetaInfo_; }

  ndntools::ContentMetaInfo&
  getContentMetaInfo() { return contentMetaInfo_; }

  /**
   * Get the content type.
   * @return The content type. If not specified, return an empty string.
   */
  const std::string&
  getContentType() const { return contentMetaInfo_.getContentType(); }

  /**
   * Get the time stamp.
   * @return The time stamp as milliseconds since Jan 1, 1970 UTC. If not
   * specified, return -1.
   */
  ndn::MillisecondsSince1970
  getTimestamp() const { return contentMetaInfo_.getTimestamp(); }

  /**
   * Get the hasSegments flag.
   * @return The hasSegments flag.
   */
  bool
  getHasSegments() const { return contentMetaInfo_.getHasSegments(); }

  /**
   * Get the Blob containing the optional other info.
   * @return The other info. If not specified, return an isNull Blob.
   */
  const ndn::Blob&
  getOther() const { return contentMetaInfo_.getOther(); }

  /**
   * Set the content type.
   * @param contentType The content type.
   * @return The ContentMetaInfo so that you can chain calls to update values.
   */
  ndntools::ContentMetaInfo&
  setContentType(const std::string& contentType)
  {
    return contentMetaInfo_.setContentType(contentType);
  }

  /**
   * Set the time stamp.
   * @param timestamp The time stamp.
   * @return The ContentMetaInfo so that you can chain calls to update values.
   */
  ndntools::ContentMetaInfo&
  setTimestamp(ndn::MillisecondsSince1970 timestamp)
  {
    return contentMetaInfo_.setTimestamp(timestamp);
  }

  /**
   * Set the hasSegments flag.
   * @param hasSegments The hasSegments flag.
   * @return The ContentMetaInfo so that you can chain calls to update values.
   */
  ndntools::ContentMetaInfo&
  setHasSegments(bool hasSegments)
  {
    return contentMetaInfo_.setHasSegments(hasSegments);
  }

  /**
   * Set the Blob containing the optional other info.
   * @param other The other info, or a default null Blob() if not specified.
   * @return The ContentMetaInfo so that you can chain calls to update values.
   */
  ndntools::ContentMetaInfo&
  setOther(const ndn::Blob& other)
  {
    return contentMetaInfo_.setOther(other);
  }

  /**
   * Set all the fields to their default unspecified values.
   */
  void
  clear() { contentMetaInfo_.clear(); }

  /**
   * Encode this ContentMetaInfo.
   * @return The encoded byte array.
   */
  ndn::Blob
  wireEncode() const { return contentMetaInfo_.wireEncode(); }

  /**
   * Decode the input and update this ContentMetaInfo.
   * @param input The input byte array to be decoded.
   * @param inputLength The length of input.
   */
  void
  wireDecode(const uint8_t *input, size_t inputLength)
  {
    contentMetaInfo_.wireDecode(input, inputLength);
  }

  /**
   * Decode the input and update this Schedule.
   * @param input The input byte array to be decoded.
   */
  void
  wireDecode(const std::vector<uint8_t>& input)
  {
    contentMetaInfo_.wireDecode(input);
  }

  /**
   * Decode the input and update this Schedule.
   * @param input The input byte array to be decoded as an immutable Blob.
   */
  void
  wireDecode(const ndn::Blob& input)
  {
    contentMetaInfo_.wireDecode(input);
  }

private:
  ndntools::ContentMetaInfo contentMetaInfo_;
};

}

#endif
