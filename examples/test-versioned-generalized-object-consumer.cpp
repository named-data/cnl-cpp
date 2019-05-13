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
 * This tests fetching of versioned generalized objects on demand as provided by
 * test-versioned-generalized-object-producer (which must be running).
 */

#include <cstdlib>
#include <iostream>
#include <unistd.h>
#include <cnl-cpp/generalized-object/generalized-object-handler.hpp>

using namespace std;
using namespace cnl_cpp;
using namespace ndn;

int main(int argc, char** argv)
{
  try {
    // Silence the warning from Interest wire encode.
    Interest::setDefaultCanBePrefix(true);

    // The default Face will connect using a Unix socket, or to "localhost".
    Face face;

    Namespace prefix("/ndn/test/status");
    prefix.setFace(&face);

    bool enabled = true;
    // This is called to print the content after it is re-assembled from segments.
    auto onObject = [&]
      (const ptr_lib::shared_ptr<ContentMetaInfoObject>& contentMetaInfo,
       Namespace& objectNamespace) {
      cout << "Got generalized object " << objectNamespace.getName() <<
        ", content-type " << contentMetaInfo->getContentType() << ": " <<
        objectNamespace.getBlobObject().toRawStr() << endl;
      enabled = false;
    };
    auto handler = ptr_lib::make_shared<GeneralizedObjectHandler>(&prefix, onObject);
    // Allow one component after the prefix for the <version>.
    handler->setNComponentsAfterObjectNamespace(1);
    // In objectNeeded, set mustBeFresh == true so we avoid expired cached data.
    prefix.objectNeeded(true);

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
