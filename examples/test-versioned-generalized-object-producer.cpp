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

/**
 * This registers with the local NFD to produce versioned generalized object
 * test data on demand from test-versioned-generalized-object-consumer (which
 * must be run separately).
 */

#include <cstdlib>
#include <ctime>
#include <iostream>
#include <unistd.h>
#include <ndn-cpp/security/key-chain.hpp>
#include <cnl-cpp/generalized-object/generalized-object-handler.hpp>

using namespace std;
using namespace ndn;
using namespace ndntools;
using namespace cnl_cpp;

int main(int argc, char** argv)
{
  try {
    // The default Face will connect using a Unix socket, or to "localhost".
    Face face;

    // Use the system default key chain and certificate name to sign.
    KeyChain keyChain;
    face.setCommandSigningInfo(keyChain, keyChain.getDefaultCertificateName());

    Namespace prefix("/ndn/test/status", &keyChain);

    cout << "Register prefix " << prefix.getName().toUri() << endl;
    // Set the face and register to receive Interests.
    prefix.setFace
      (&face, [](const ptr_lib::shared_ptr<const Name>& prefix) {
        cout << "Register failed for prefix " << prefix->toUri() << endl;
      });

    GeneralizedObjectHandler handler;
    // Each generalized object will have a 1000 millisecond freshness period.
    MetaInfo metaInfo;
    metaInfo.setFreshnessPeriod(1000);

    // This is called when the library receives an Interest which is not
    // satisfied by Data already in the Namespace tree.
    auto onObjectNeeded = [&]
      (Namespace& nameSpace, Namespace& neededNamespace, uint64_t callbackId) {
      if (&neededNamespace != &prefix)
        // This is not the expected Namespace.
        return false;

      // Make a version from the current time.
      Namespace& versionNamespace = prefix
                [Name::Component::fromVersion((uint64_t)ndn_getNowMilliseconds())];
      // The metaInfo has the freshness period.
      versionNamespace.setNewDataMetaInfo(metaInfo);
      cout << "Producing the generalized object for " << versionNamespace.getName() << endl;
      time_t t = time(0);
      handler.setObject
        (versionNamespace, 
         Blob::fromRawStr(string("Status as of ") + asctime(localtime(&t))),
         "text/html");
      return true;
    };

    prefix.addOnObjectNeeded(onObjectNeeded);

    while (true) {
      face.processEvents();
      // We need to sleep for a few milliseconds so we don't use 100% of the CPU.
      usleep(10000);
    }
  } catch (std::exception& e) {
    cout << "exception: " << e.what() << endl;
  }
  return 0;
}
