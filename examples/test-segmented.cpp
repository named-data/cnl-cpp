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

/**
 * This tests updating a namespace based on segmented content.
 */

#include <cstdlib>
#include <iostream>
#include <unistd.h>
#include <cnl-cpp/segmented-content.hpp>

using namespace std;
using namespace cnl_cpp;
using namespace ndn;
using namespace ndn::func_lib;

static void
onContentSet
  (Namespace& nameSpace, Namespace& contentNamespace, uint64_t callbackId,
   bool* enabled);

int main(int argc, char** argv)
{
  try {
    Face face("memoria.ndn.ucla.edu");
    Namespace page
      ("/ndn/edu/ucla/remap/demo/ndn-js-test/named-data.net/project/ndn-ar2011.html/%FDT%F7n%9E");
    page.setFace(&face);

    bool enabled = true;
    page.addOnContentSet(bind(&onContentSet, _1, _2, _3, &enabled));
    SegmentedContent segmentedContent(page);
    segmentedContent.start();

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

/**
 * This is called to print the content after it is re-assembled from segments.
 * @param nameSpace The calling Namespace.
 * @param contentNamespace The Namespace where the content was set.
 * @param callbackId The callback ID returned by addOnContentSet.
 * @param enabled On success or error, set *enabled = false.
 */
static void
onContentSet
  (Namespace& nameSpace, Namespace& contentNamespace, uint64_t callbackId,
   bool* enabled)
{
  if (&contentNamespace == &nameSpace) {
    cout << "Got segmented content size " << contentNamespace.getBlobContent()->size() <<
      endl;
    *enabled = false;
  }
}
