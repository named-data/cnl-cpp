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

/**
 * This tests fetching a stream of generalized objects provided by
 * test-generalized-object-stream-producer (which must be running).
 */

#include <cstdlib>
#include <iostream>
#include <unistd.h>
#include <cnl-cpp/generalized-object/generalized-object-stream-handler.hpp>

using namespace std;
using namespace cnl_cpp;
using namespace ndn;

int main(int argc, char** argv)
{
  try {
    // The default Face will connect using a Unix socket, or to "localhost".
    Face face;

    Namespace stream("/ndn/eb/stream/run/28/annotations");
    stream.setFace(&face);

    bool enabled = true;
    // This is called to print the content of each streamed object when it arrives.
    auto onNewObject = [&]
      (int sequenceNumber,
       const ptr_lib::shared_ptr<ContentMetaInfoObject>& contentMetaInfo,
       const ndn::ptr_lib::shared_ptr<Object>& object) {
      cout << "Got generalized object, sequenceNumber " << sequenceNumber <<
        ", content-type " << contentMetaInfo->getContentType() << ": " <<
        ptr_lib::dynamic_pointer_cast<BlobObject>(object)->toRawStr() << endl;
      enabled = false;
    };
    stream.setHandler
      (ptr_lib::make_shared<GeneralizedObjectStreamHandler>
       (onNewObject)).objectNeeded();

    while (enabled) {
      face.processEvents();
      // We need to sleep for a few milliseconds so we don't use 100% of the CPU.
      usleep(10000);
    }
  } catch (std::exception& e) {
    cout << "exception: " << e.what() << endl;
  }
  return 0;
}
