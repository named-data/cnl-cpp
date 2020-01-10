/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil -*- */
/**
 * Copyright (C) 2018-2020 Regents of the University of California.
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

#include <algorithm>
#include <ndn-cpp/util/logging.hpp>
#include "pending-incoming-interest-table.hpp"

INIT_LOGGER("ndn.PendingIncomingInterestTable");

using namespace std;
using namespace ndn;

namespace cnl_cpp {

PendingIncomingInterestTable::Entry::Entry
  (const ptr_lib::shared_ptr<const Interest>& interest, Face& face)
: interest_(interest), face_(face)
{
  // Set up timeoutTimeMilliseconds_.
  if (interest_->getInterestLifetimeMilliseconds() >= 0.0)
    timeoutTimeMilliseconds_ = ndn_getNowMilliseconds() +
      interest_->getInterestLifetimeMilliseconds();
  else
    // No timeout.
    timeoutTimeMilliseconds_ = -1.0;
}

void
PendingIncomingInterestTable::satisfyInterests(const Data& data)
{
  Milliseconds nowMilliseconds = ndn_getNowMilliseconds();

  // Go backwards through the list so we can erase entries.
  for (int i = (int)table_.size() - 1; i >= 0; --i) {
    Entry& pendingInterest = *table_[i];
    if (pendingInterest.isTimedOut(nowMilliseconds)) {
      table_.erase(table_.begin() + i);
      continue;
    }

    // TODO: Use matchesData to match selectors?
    if (pendingInterest.getInterest()->matchesName(data.getName())) {
      try {
        // Send to the same face from the original call to the OnInterest
        // callback. wireEncode returns the cached encoding if available.
        pendingInterest.getFace().send(data.wireEncode());
      } catch (const std::exception& ex) {
        _LOG_ERROR("PendingIncomingInterestTable: Error sending data: " << ex.what());
      } catch (...) {
        _LOG_ERROR("PendingIncomingInterestTable: Error sending data.");
      }

      table_.erase(table_.begin() + i);
    }
  }
}

}
