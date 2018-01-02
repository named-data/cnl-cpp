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

#ifndef NDN_BLOB_CONTENT_HPP
#define NDN_BLOB_CONTENT_HPP

#include <ndn-cpp/util/blob.hpp>
#include "content.hpp"

namespace cnl_cpp {

/**
 * A BlobContent extends Content to hold an ndn::Blob and provides convenience
 * wrapper access methods the same as Blob.
 */
class BlobContent : public Content {
public:
  /**
   * Create a new BlobContent to hold the given blob. Objects of this type are
   * created internally by the library, so your application normally does not
   * call this constructor.
   * @param blob
   */
  BlobContent(const ndn::Blob& blob)
  : blob_(blob) {}

  /**
   * Get the Blob given to the constructor.
   * @return The Blob.
   */
  const ndn::Blob&
  getBlob() const { return blob_; }

  /**
   * Return the length of the immutable byte array.
   */
  size_t
  size() const { return blob_.size(); }

  /**
   * Return a const pointer to the first byte of the immutable byte array, or 0
   * if the pointer is null.
   */
  const uint8_t*
  buf() const { return blob_.buf(); }

  /**
   * Check if the array pointer is null.
   * @return true if the buffer pointer is null, otherwise false.
   */
  bool
  isNull() const { return blob_.isNull(); }

  /**
   * Write the hex representation of the bytes in array to the result. If the
   * pointer is null, write nothing.
   * @param result The output stream to write to.
   */
  void
  toHex(std::ostringstream& result) const { blob_.toHex(result); }

  /**
   * Return the hex representation of the bytes in array.
   * @return The hex bytes as a string, or an empty string if the pointer is
   * null.
   */
  std::string
  toHex() const { return blob_.toHex(); }

  /**
   * Return the bytes of the byte array as a raw str of the same length. This
   * does not do any character encoding such as UTF-8.
   * @return The buffer as a string, or "" if isNull().
   */
  std::string
  toRawStr() const { return blob_.toRawStr(); }

  /**
   * Check if the value of this Blob equals the other blob.
   * @param other The other Blob to check.
   * @return True if this isNull and other isNull or if the bytes of this
   * blob equal the bytes of the other.
   */
  bool
  equals(const ndn::Blob& other) const { return blob_.equals(other); }

private:
  ndn::Blob blob_;
};

}

#endif
