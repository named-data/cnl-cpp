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

#ifndef NDN_PENDING_INCOMING_INTEREST_TABLE_HPP
#define NDN_PENDING_INCOMING_INTEREST_TABLE_HPP

#include <ndn-cpp/face.hpp>

namespace cnl_cpp {

/**
 * PendingImcomingInterestTable is an internal class to hold a list of
 * Interests which OnInterest received but could not satisfy.
 */
class PendingIncomingInterestTable {
public:
  /**
   * Entry holds the Interest and other fields for an entry in the pending
   * interest table.
   */
  class Entry {
  public:
    /**
     * Create a PendingIncomingInterestTable::Entry and set the timeoutTime_
     * based on the current time and the Interest lifetime.
     *
     * @param interest The Interest. This does not make a copy.
     * @param face The face from the OnInterest callback. If the Interest is
     * satisfied later by a new data packet, we will send the Data packet to the
     * face.
     */
    Entry
      (const ndn::ptr_lib::shared_ptr<const ndn::Interest>& interest,
       ndn::Face& face);

    /**
     * Get the interest given to the constructor.
     * @return The interest.
     */
    const ndn::ptr_lib::shared_ptr<const ndn::Interest>&
    getInterest() { return interest_; }

    /**
     * Get the face given to the constructor.
     * @return The face.
     */
    ndn::Face&
    getFace() { return face_; }

    /**
     * Check if this Interest is timed out.
     * @param nowMilliseconds The current time in milliseconds since 1/1/1970 UTC.
     * @return True if this Interest is timed out, otherwise false.
     */
    bool
    isTimedOut(ndn::MillisecondsSince1970 nowMilliseconds)
    {
      return timeoutTimeMilliseconds_ >= 0.0 &&
             nowMilliseconds >= timeoutTimeMilliseconds_;
    }

  private:
    ndn::ptr_lib::shared_ptr<const ndn::Interest> interest_;
    ndn::Face& face_;
    ndn::MillisecondsSince1970 timeoutTimeMilliseconds_;
  };

  /**
   * Store an interest from an OnInterest callback in the internal pending
   * interest table. Use satisfyInterests(data) to check if the Data packet
   * satisfies any pending interest.
   * @param interest The Interest for which we don't have a Data packet yet.
   * You should not modify the interest after calling this.
   * @param face The Face from the OnInterest callback with the connection which
   * received the Interest and to which satisfyInterests will send the Data
   * packet.
   */
  void
  add
    (const ndn::ptr_lib::shared_ptr<const ndn::Interest>& interest,
     ndn::Face& face)
  {
    table_.push_back(ndn::ptr_lib::make_shared<Entry>(interest, face));
  }

  /**
   * Remove timed-out Interests, then for each pending Interest that the Data
   * packet matches, send the Data packet through the face and remove the
   * pending Interest.
   * @param data The Data packet to send if it satisfies an Interest.
   */
  void
  satisfyInterests(const ndn::Data& data);

private:
  std::vector<ndn::ptr_lib::shared_ptr<Entry> > table_;
};

}

#endif
