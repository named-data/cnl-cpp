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

/**
 * This registers with the local NFD to produce a stream of generalized object
 * test data for test-generalized-object-stream consumer (which must be run
 * separately).
 */

#include <cstdlib>
#include <iostream>
#include <unistd.h>
#include <ndn-cpp/security/key-chain.hpp>
#include <ndn-cpp/delegation-set.hpp>
#include <cnl-cpp/generalized-object/generalized-object-stream-handler.hpp>

using namespace std;
using namespace ndn;
using namespace ndn::func_lib;
using namespace ndntools;
using namespace cnl_cpp;

static string
toString(int x)
{
  char buf[20];
  sprintf(buf, "%d", x);
  return string(buf);
}

int main(int argc, char** argv)
{
  try {
    // The default Face will connect using a Unix socket, or to "localhost".
    Face face;

    // Use the system default key chain and certificate name to sign.
    KeyChain keyChain;
    face.setCommandSigningInfo(keyChain, keyChain.getDefaultCertificateName());

    Milliseconds publishIntervalMs = 1000.0;
    Namespace stream("/ndn/eb/stream/run/28/annotations", &keyChain);
    GeneralizedObjectStreamHandler handler(&stream);

    cout << "Register prefix " << stream.getName().toUri() << endl;
    // Set the face and register to receive Interests.
    stream.setFace
      (&face, [](const ptr_lib::shared_ptr<const Name>& prefix) {
        cout << "Register failed for prefix " << prefix->toUri() << endl;
      });

    // Loop, producing a new object every publishIntervalMs milliseconds (and
    // also calling processEvents()).
    MillisecondsSince1970 previousPublishMs = 0;
    while (true) {
      MillisecondsSince1970 now = ndn_getNowMilliseconds();
      if (now >= previousPublishMs + publishIntervalMs) {
        cout << "Preparing data for sequence " <<
          (handler.getProducedSequenceNumber() + 1) << endl;
        handler.addObject
          (Blob::fromRawStr("Payload " + toString(handler.getProducedSequenceNumber() + 1)),
           "application/json");

        previousPublishMs = now;
      }

      face.processEvents();
      // We need to sleep for a few milliseconds so we don't use 100% of the CPU.
      usleep(10000);
    }
  } catch (std::exception& e) {
    cout << "exception: " << e.what() << endl;
  }
  return 0;
}
